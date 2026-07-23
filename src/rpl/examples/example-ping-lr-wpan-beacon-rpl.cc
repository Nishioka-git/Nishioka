#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/rpl-header.h"
#include "ns3/rpl-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;
using namespace ns3::lrwpan;

namespace
{

uint32_t g_dioTx = 0;
uint32_t g_dioRx = 0;

void
DioTxProbe(Ptr<const Packet> /*packet*/,
           Ipv6Address src,
           Ipv6Address dst,
           DioBaseObjectHeader dio)
{
    ++g_dioTx;
    std::cout << Simulator::Now().As(Time::S) << " [RPL DIO Tx] src=" << src << " dst=" << dst
              << " Rank=" << dio.GetRank() << " DODAGID=" << dio.GetDodagId() << "\n";
}

void
DioRxProbe(Ptr<const Packet> /*packet*/,
           Ipv6Address src,
           Ipv6Address dst,
           DioBaseObjectHeader dio)
{
    ++g_dioRx;
    std::cout << Simulator::Now().As(Time::S) << " [RPL DIO Rx] src=" << src << " dst=" << dst
              << " Rank=" << dio.GetRank() << " DODAGID=" << dio.GetDodagId() << "\n";
}

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
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(20),
                                  "DeltaY",
                                  DoubleValue(20),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.Install(nodes);

    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    lrWpanHelper.AddPropagationLossModel("ns3::LogDistancePropagationLossModel");
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
    rplHelper.Set("DioInterval", TimeValue(Seconds(1.0)));
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(rplHelper);
    internetv6.Install(nodes);

    Ptr<Rpl> rpl0 = GetRpl(nodes.Get(0));
    Ptr<Rpl> rpl1 = GetRpl(nodes.Get(1));
    NS_ABORT_MSG_UNLESS(rpl0 && rpl1, "RPL routing protocol was not installed");
    rpl0->SetRoot(true);

    rpl0->TraceConnectWithoutContext("DioTx", MakeCallback(&DioTxProbe));
    rpl0->TraceConnectWithoutContext("DioRx", MakeCallback(&DioRxProbe));
    rpl1->TraceConnectWithoutContext("DioTx", MakeCallback(&DioTxProbe));
    rpl1->TraceConnectWithoutContext("DioRx", MakeCallback(&DioRxProbe));

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces = ipv6.Assign(devices);

    std::cout << "Root=Node0 " << deviceInterfaces.GetAddress(0, 1)
              << "  Child=Node1 " << deviceInterfaces.GetAddress(1, 1) << "\n"
              << "Expect: Node0 periodically sends DIO to ff02::1a; Node1 receives them.\n\n";

    AsciiTraceHelper ascii;
    lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("Ping-6LoW-lr-wpan-beacon-rpl.tr"));
    lrWpanHelper.EnablePcapAll(std::string("Ping-6LoW-lr-wpan-beacon-rpl"), true);

    Simulator::Stop(Seconds(5));
    Simulator::Run();

    std::cout << "\n=== RPL DIO summary ===\n"
              << "DIO Tx=" << g_dioTx << " Rx=" << g_dioRx << "\n";
    if (g_dioTx > 0 && g_dioRx > 0)
    {
        std::cout << "PASS: root transmitted DIO and another node received it.\n";
    }
    else
    {
        std::cout << "UNEXPECTED: DIO Tx/Rx not observed (Tx=" << g_dioTx << " Rx=" << g_dioRx
                  << ").\n";
    }

    Simulator::Destroy();
    return (g_dioTx > 0 && g_dioRx > 0) ? 0 : 1;
}
