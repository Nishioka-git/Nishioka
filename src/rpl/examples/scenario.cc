#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-module.h"
#include "ns3/rpl-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/spectrum-module.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace ns3;
using namespace ns3::lrwpan;

namespace
{

constexpr uint32_t kNodeCount = 50;
constexpr double kAreaMeters = 200.0;
constexpr double kRadioRangeMeters = 60.0;
constexpr double kSimSeconds = 35.0;

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

Ipv6Address
GetLinkLocal(Ptr<Node> node)
{
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < ipv6->GetNAddresses(i); ++j)
        {
            Ipv6Address address = ipv6->GetAddress(i, j).GetAddress();
            if (address.IsLinkLocal())
            {
                return address;
            }
        }
    }
    return Ipv6Address::GetZero();
}

int32_t
FindParentNodeId(NodeContainer nodes, Ipv6Address parent)
{
    if (parent.IsAny())
    {
        return -1;
    }
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        if (GetLinkLocal(nodes.Get(i)) == parent)
        {
            return static_cast<int32_t>(nodes.Get(i)->GetId());
        }
    }
    return -1;
}

void
InsertDodagLinks(const std::string& path, const std::vector<std::pair<uint32_t, uint32_t>>& edges)
{
    std::ifstream in(path);
    if (!in)
    {
        std::cerr << "Cannot open NetAnim file " << path << "\n";
        return;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();
    std::string xml = buffer.str();
    const std::string marker = "<nu ";
    const std::size_t pos = xml.find(marker);
    if (pos == std::string::npos)
    {
        std::cerr << "NetAnim file has no update element to attach DODAG links\n";
        return;
    }
    std::string links;
    for (const auto& edge : edges)
    {
        links += "<link fromId=\"" + std::to_string(edge.first) + "\" toId=\"" +
                 std::to_string(edge.second) + "\" ld=\"DODAG parent\" />\n";
    }
    xml.insert(pos, links);
    std::ofstream out(path, std::ios::trunc);
    out << xml;
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
    nodes.Create(kNodeCount);

    // 根を中央、残りを正方形内に一様ランダム配置する。
    // 一辺 200 m、到達距離 60 m。孤立は起こり得るので、終了時に受信ゼロを列挙する。
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    const double center = kAreaMeters / 2.0;
    positions->Add(Vector(center, center, 0.0));

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
    x->SetStream(1);
    y->SetStream(2);
    x->SetAttribute("Min", DoubleValue(0.0));
    x->SetAttribute("Max", DoubleValue(kAreaMeters));
    y->SetAttribute("Min", DoubleValue(0.0));
    y->SetAttribute("Max", DoubleValue(kAreaMeters));
    for (uint32_t i = 1; i < kNodeCount; ++i)
    {
        positions->Add(Vector(x->GetValue(), y->GetValue(), 0.0));
    }
    mobility.SetPositionAllocator(positions);
    mobility.Install(nodes);

    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    lrWpanHelper.AddPropagationLossModel("ns3::RangePropagationLossModel",
                                         "MaxRange",
                                         DoubleValue(kRadioRangeMeters));
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
    // RFC 6550 既定。Imin = 2^3 ms = 8 ms、Imax = 8 ms × 2^20、冗長定数 k = 10。
    rplHelper.Set("DioIntervalMin", UintegerValue(3));
    rplHelper.Set("DioIntervalDoublings", UintegerValue(20));
    rplHelper.Set("DioRedundancyConstant", UintegerValue(10));
    rplHelper.Set("DisInterval", TimeValue(Seconds(1.0)));
    rplHelper.Set("DisMaxAttempts", UintegerValue(30));
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(rplHelper);
    internetv6.Install(nodes);

    Ptr<Rpl> root = GetRpl(nodes.Get(0));
    NS_ABORT_MSG_UNLESS(root, "RPL routing protocol was not installed");
    root->SetRoot(true);

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    ipv6.Assign(devices);

    AnimationInterface* animOwner = new AnimationInterface("rpl-50-anim.xml");
    AnimationInterface& anim = *animOwner;
    anim.EnablePacketMetadata(true);
    anim.SetMobilityPollInterval(Seconds(1.0));
    const uint32_t rankCounter = anim.AddNodeCounter("Rank", AnimationInterface::UINT32_COUNTER);
    const uint32_t hopCounter = anim.AddNodeCounter("HopCount", AnimationInterface::UINT32_COUNTER);
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        anim.UpdateNodeColor(nodes.Get(i), 160, 160, 160);
        anim.UpdateNodeSize(nodes.Get(i)->GetId(), 5.0, 5.0);
    }
    anim.UpdateNodeColor(nodes.Get(0), 255, 0, 0);

    std::cout << "Topology: Node0 at center (" << center << "," << center << "), others uniform in "
              << kAreaMeters << " m square. MaxRange=" << kRadioRangeMeters << " m. n="
              << kNodeCount << "\n"
              << "Mode: Trickle DIO (Imin=8ms, doublings=20, k=10) and DIS while unjoined. sim="
              << kSimSeconds << "s.\n"
              << "NetAnim: rpl-50-anim.xml\n\n";

    Simulator::Stop(Seconds(kSimSeconds));
    Simulator::Run();

    std::cout << "\n=== Final RPL state ===\n";
    std::cout << "id\tx\ty\tjoined\trank\thops\tparent\tdisTx\tdisRx\tdioTx\tdioRx\n";

    uint32_t joinedCount = 0;
    uint32_t silentCount = 0;
    std::vector<std::pair<uint32_t, uint32_t>> dodagEdges;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Rpl> rpl = GetRpl(node);
        NS_ABORT_MSG_UNLESS(rpl, "RPL missing on node " << i);
        Vector pos = node->GetObject<MobilityModel>()->GetPosition();
        const int32_t parentId =
            rpl->IsRoot() ? -1 : FindParentNodeId(nodes, rpl->GetPreferredParent());
        const bool heard = rpl->GetDisRxCount() > 0 || rpl->GetDioRxCount() > 0;
        if (rpl->IsJoined())
        {
            ++joinedCount;
        }
        if (!heard)
        {
            ++silentCount;
        }
        if (parentId >= 0)
        {
            dodagEdges.emplace_back(node->GetId(), static_cast<uint32_t>(parentId));
        }

        std::cout << node->GetId() << "\t" << pos.x << "\t" << pos.y << "\t" << rpl->IsJoined()
                  << "\t" << rpl->GetRank() << "\t" << (rpl->IsJoined() ? rpl->GetPathMetric() : 0)
                  << "\t" << parentId << "\t" << rpl->GetDisTxCount() << "\t" << rpl->GetDisRxCount()
                  << "\t" << rpl->GetDioTxCount() << "\t" << rpl->GetDioRxCount() << "\n";

        std::string descr = "N" + std::to_string(node->GetId());
        descr += rpl->IsJoined() ? " joined" : " unjoined";
        descr += " rank=" + std::to_string(rpl->GetRank());
        descr += " hops=" + std::to_string(rpl->IsJoined() ? rpl->GetPathMetric() : 0);
        descr += " parent=" + std::to_string(parentId);
        descr += " disTx=" + std::to_string(rpl->GetDisTxCount());
        descr += " disRx=" + std::to_string(rpl->GetDisRxCount());
        descr += " dioTx=" + std::to_string(rpl->GetDioTxCount());
        descr += " dioRx=" + std::to_string(rpl->GetDioRxCount());
        anim.UpdateNodeDescription(node, descr);
        anim.UpdateNodeSize(node->GetId(), 5.0, 5.0);
        if (rpl->IsRoot())
        {
            anim.UpdateNodeColor(node, 255, 0, 0);
        }
        else if (rpl->IsJoined())
        {
            anim.UpdateNodeColor(node, 0, 160, 0);
        }
        else
        {
            anim.UpdateNodeColor(node, 160, 160, 160);
        }
        anim.UpdateNodeCounter(rankCounter, node->GetId(), rpl->IsJoined() ? rpl->GetRank() : 0);
        anim.UpdateNodeCounter(hopCounter,
                               node->GetId(),
                               rpl->IsJoined() ? rpl->GetPathMetric() : 0);
    }

    std::cout << "\nJoined " << joinedCount << "/" << kNodeCount
              << ". Nodes with neither DIS nor DIO received: " << silentCount << ".\n";
    std::cout << "NetAnim colors: root=red, joined=green, unjoined=gray.\n";
    std::cout << "Node description includes parent id and DIS/DIO counters, including unjoined "
                 "nodes.\n";
    std::cout << "DODAG edges in NetAnim: " << dodagEdges.size() << " (child to parent).\n";

    delete animOwner;
    InsertDodagLinks("rpl-50-anim.xml", dodagEdges);

    Simulator::Destroy();
    return 0;
}
