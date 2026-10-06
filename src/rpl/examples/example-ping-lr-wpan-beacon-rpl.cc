#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/rpl-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;
using namespace ns3::lrwpan;

namespace
{

Ptr<Rpl>
GetRpl(Ptr<Node> node)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    Ptr<Rpl> rpl = DynamicCast<Rpl>(ipv6->GetRoutingProtocol());
    if (rpl)
    {
        return rpl;
    }
    Ptr<Ipv6ListRouting> list = DynamicCast<Ipv6ListRouting>(ipv6->GetRoutingProtocol());
    if (!list)
    {
        return nullptr;
    }
    for (uint32_t i = 0; i < list->GetNRoutingProtocols(); ++i)
    {
        int16_t priority = 0;
        rpl = DynamicCast<Rpl>(list->GetRoutingProtocol(i, priority));
        if (rpl)
        {
            return rpl;
        }
    }
    return nullptr;
}

// 終了時の親選択結果（joined / Rank / parent）を表示する。
void
PrintNodeState(Ptr<Node> node, const std::string& label)
{
    Ptr<Rpl> rpl = GetRpl(node);
    std::cout << label << " joined=" << rpl->IsJoined() << " rank=" << rpl->GetRank()
              << " parent=" << rpl->GetPreferredParent() << "\n";
}

} // namespace

int
main(int argc, char** argv)
{
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnableAll(LogLevel(LOG_PREFIX_TIME | LOG_PREFIX_FUNC | LOG_PREFIX_NODE));
        LogComponentEnable("Rpl", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(3); // root -- 中継 -- 端 の 3 ノード

    // 直線配置。N2 が root の DIO を直接受信しない距離にする。
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));   // Node0 root
    positions->Add(Vector(80.0, 0.0, 0.0));  // Node1
    positions->Add(Vector(160.0, 0.0, 0.0)); // Node2
    mobility.SetPositionAllocator(positions);
    mobility.Install(nodes);

    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    // 到達距離 100 m → 隣接のみ通信可（160 m 先の N0–N2 は非接続）。
    lrWpanHelper.AddPropagationLossModel("ns3::RangePropagationLossModel",
                                         "MaxRange",
                                         DoubleValue(100.0));
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    lrWpanHelper.CreateAssociatedPan(lrwpanDevices, 5);

    Ptr<LrWpanNetDevice> dev0 = lrwpanDevices.Get(0)->GetObject<LrWpanNetDevice>();
    MlmeStartRequestParams params;
    params.m_panCoor = true;
    params.m_PanId = 5;
    params.m_bcnOrd = 14;
    params.m_sfrmOrd = 13;
    params.m_logCh = 11;
    Simulator::ScheduleWithContext(dev0->GetNode()->GetId(),
                                   Seconds(0),
                                   &LrWpanMac::MlmeStartRequest,
                                   dev0->GetMac(),
                                   params);

    RplHelper rplHelper;
    // 周期 DIO はシミュレーション時間内に出さない → JOIN は DIS 応答の DIO に依存。
    rplHelper.Set("DioInterval", TimeValue(Seconds(100.0)));
    rplHelper.Set("DisInterval", TimeValue(Seconds(0.5)));
    rplHelper.Set("DisMaxAttempts", UintegerValue(8));
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(rplHelper);
    internetv6.Install(nodes);

    Ptr<Rpl> rpl0 = GetRpl(nodes.Get(0));
    Ptr<Rpl> rpl1 = GetRpl(nodes.Get(1));
    Ptr<Rpl> rpl2 = GetRpl(nodes.Get(2));
    NS_ABORT_MSG_UNLESS(rpl0 && rpl1 && rpl2, "RPL routing protocol was not installed");
    rpl0->SetRoot(true);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces = ipv6.Assign(devices);

    std::cout << "Topology: Node0(root) --80m-- Node1 --80m-- Node2 (MaxRange=100m)\n"
              << "Root=" << deviceInterfaces.GetAddress(0, 1)
              << "  N1=" << deviceInterfaces.GetAddress(1, 1)
              << "  N2=" << deviceInterfaces.GetAddress(2, 1) << "\n"
              << "Mode: DIS-required JOIN (periodic DioInterval=100s).\n"
              << "Expect: unjoined DIS -> joined DIO reply; N1 parent=N0, N2 parent=N1.\n\n";

    Simulator::Stop(Seconds(8));
    Simulator::Run();

    std::cout << "\n=== Final RPL state ===\n";
    PrintNodeState(nodes.Get(0), "Node0");
    PrintNodeState(nodes.Get(1), "Node1");
    PrintNodeState(nodes.Get(2), "Node2");

    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>(&std::cout);
    std::cout << "\n";
    rpl0->PrintRoutingTable(routingStream);
    rpl1->PrintRoutingTable(routingStream);
    rpl2->PrintRoutingTable(routingStream);

    // 期待: N0 Rank1、N1→N0 Rank2、N2→N1 Rank3（DIS→DIO で形成）。
    const bool pass =
        rpl0->IsJoined() && rpl0->IsRoot() && rpl0->GetRank() == 1 &&
        rpl1->IsJoined() && rpl1->GetRank() == 2 && !rpl1->GetPreferredParent().IsAny() &&
        rpl2->IsJoined() && rpl2->GetRank() == 3 && !rpl2->GetPreferredParent().IsAny() &&
        rpl2->GetPreferredParent() != rpl1->GetPreferredParent();

    std::cout << "\n";
    if (pass)
    {
        std::cout << "PASS: DIS-solicited multi-hop join "
                     "(N1->root Rank2, N2->N1 Rank3).\n";
    }
    else
    {
        std::cout << "UNEXPECTED: parent/rank chain not formed as expected.\n";
    }

    Simulator::Destroy();
    return pass ? 0 : 1;
}
