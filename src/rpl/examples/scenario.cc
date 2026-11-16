/*
 * RPL over 6LoWPAN / IEEE 802.15.4.
 *
 * Topology (line):  Node0 (root) --80m-- Node1 --80m-- Node2
 * Radio range is 100 m, so Node0 and Node2 (160 m) cannot hear each other.
 *
 * MAC PAN ID and short addresses are set up front via MLME-SET.request.
 * 6LoWPAN builds each node's IPv6 IID from that short address, so IPv6 is
 * determined by the MAC.  There is no IEEE 802.15.4 scan / associate procedure.
 *
 * Multi-hop reachability comes from the radio range; RPL builds the routing
 * topology.  Downward routes: Storing mode (MOP 2).  After joining, Node2 and
 * Node1 send unicast DAO so that Node0 learns Node2 via Node1.
 *
 * After RPL converges, each endpoint (Node0 / Node2) has one UDP socket
 * used for both send and receive.  Probes: Node2->Node0 and Node0->Node2.
 */

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/rpl-module.h"
#include "ns3/sixlowpan-module.h"

#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::lrwpan;

namespace
{

constexpr uint16_t kPanId = 5;
constexpr uint16_t kDataPort = 9;
constexpr uint32_t kPayloadBytes = 16;

// 端点ノードの UDP 受信成否（経路の上り/下り用に Socket を分けているわけではない）
bool g_node0Rx = false;
bool g_node2Rx = false;

Ptr<Rpl>
GetRpl(Ptr<Node> node)
{
    return Ipv6RoutingHelper::GetRouting<Rpl>(node->GetObject<Ipv6>()->GetRoutingProtocol());
}

void
PrintMacAndIpv6(const NetDeviceContainer& lrwpanDevices, const Ipv6InterfaceContainer& ipv6Ifaces)
{
    std::cout << "node  panId  short  extended                 IPv6\n";
    for (uint32_t i = 0; i < lrwpanDevices.GetN(); ++i)
    {
        Ptr<LrWpanMac> mac = lrwpanDevices.Get(i)->GetObject<LrWpanNetDevice>()->GetMac();
        std::cout << i << "     " << mac->GetPanId() << "      " << mac->GetShortAddress() << "  "
                  << mac->GetExtendedAddress() << "  " << ipv6Ifaces.GetAddress(i, 1) << "\n";
    }
}

void
PrintNodeState(Ptr<Node> node, const std::string& label)
{
    Ptr<Rpl> rpl = GetRpl(node);
    std::cout << label << " root=" << (rpl->IsRoot() ? "true" : "false")
              << " joined=" << rpl->IsJoined() << " rank=" << rpl->GetRank()
              << " parent=" << rpl->GetPreferredParent() << "\n";
}

void
RecvOnNode(Ptr<Socket> socket, uint32_t nodeId, bool* received)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        Inet6SocketAddress src = Inet6SocketAddress::ConvertFrom(from);
        std::cout << Simulator::Now().As(Time::S) << " [Data Rx] Node" << nodeId
                  << " from=" << src.GetIpv6() << " size=" << packet->GetSize() << "\n";
        *received = true;
    }
}

// SetRecvCallback は Callback<void, Ptr<Socket>> のみ。束縛はラッパで行う。
void
RecvOnNode0(Ptr<Socket> socket)
{
    RecvOnNode(socket, 0, &g_node0Rx);
}

void
RecvOnNode2(Ptr<Socket> socket)
{
    RecvOnNode(socket, 2, &g_node2Rx);
}

void
SendUdp(Ptr<Socket> socket, Ipv6Address dst, const std::string& fromTo)
{
    Ptr<Packet> packet = Create<Packet>(kPayloadBytes);
    std::cout << Simulator::Now().As(Time::S) << " [Data Tx] " << fromTo << " dst=" << dst
              << " size=" << kPayloadBytes << "\n";
    socket->SendTo(packet, 0, Inet6SocketAddress(dst, kDataPort));
}

} // namespace

int
main(int argc, char** argv)
{
    bool verbose = false;
    double lateJoin = 3.0; // Node1 は 3 秒後に圏内へ（0 なら最初から圏内）

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.AddValue("lateJoin",
                 "Seconds until Node1 enters radio range (default 3; 0 = present from the start).",
                 lateJoin);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnableAll(LogLevel(LOG_PREFIX_TIME | LOG_PREFIX_FUNC | LOG_PREFIX_NODE));
        LogComponentEnable("Rpl", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(3);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    const Vector node1Home(80.0, 0.0, 0.0);
    const Vector node1Away(80.0, 500.0, 0.0);
    positions->Add(Vector(0.0, 0.0, 0.0));
    positions->Add(lateJoin > 0.0 ? node1Away : node1Home);
    positions->Add(Vector(160.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positions);
    mobility.Install(nodes);

    if (lateJoin > 0.0)
    {
        Ptr<MobilityModel> mm1 = nodes.Get(1)->GetObject<MobilityModel>();
        Simulator::Schedule(Seconds(lateJoin), [mm1, node1Home]() {
            mm1->SetPosition(node1Home);
            std::cout << Simulator::Now().As(Time::S)
                      << " [Scenario] Node1 moved into radio range\n";
        });
    }

    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    lrWpanHelper.AddPropagationLossModel("ns3::RangePropagationLossModel",
                                         "MaxRange",
                                         DoubleValue(100.0));
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // MLME-SET で PAN ID と短縮アドレス Mac16Address(1), (2), ... を決める。
    // 6LoWPAN / IPv6 の IID は MAC の GetAddress()（短縮アドレス由来）から決まる。
    for (uint32_t i = 0; i < lrwpanDevices.GetN(); ++i)
    {
        Ptr<LrWpanMac> mac = lrwpanDevices.Get(i)->GetObject<LrWpanNetDevice>()->GetMac();

        Ptr<MacPibAttributes> panAttr = Create<MacPibAttributes>();
        panAttr->macPanId = kPanId;
        mac->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, panAttr);

        Ptr<MacPibAttributes> shortAttr = Create<MacPibAttributes>();
        shortAttr->macShortAddress = Mac16Address(static_cast<uint16_t>(i + 1));
        mac->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, shortAttr);
    }

    RplHelper rplHelper;
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(rplHelper);
    internetv6.Install(nodes);

    std::vector<Ptr<Rpl>> rpls;
    rpls.reserve(nodes.GetN());
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Rpl> rpl = GetRpl(nodes.Get(i));
        NS_ABORT_MSG_UNLESS(rpl, "RPL routing protocol was not installed on node " << i);
        rpls.push_back(rpl);
    }
    Ptr<Rpl> rpl0 = rpls[0];
    Ptr<Rpl> rpl1 = rpls[1];
    Ptr<Rpl> rpl2 = rpls[2];
    rpl0->SetRoot(true);
    rpl0->SetAttribute("ModeOfOperation",
                       EnumValue<ModeOfOperation>(ModeOfOperation::STORING_NO_MULTICAST));

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    // 2001:2::/64 は route-over の複数ホップであり同一リンクではない。
    // on-link を付けると Node2 宛が直接到達扱いになり、RPL を使わなくなる。
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces = ipv6.Assign(devices,
                                                          std::vector<bool>(nodes.GetN(), true),
                                                          std::vector<bool>(nodes.GetN(), false));

    const Ipv6Address n0Addr = deviceInterfaces.GetAddress(0, 1);
    const Ipv6Address n1Addr = deviceInterfaces.GetAddress(1, 1);
    const Ipv6Address n2Addr = deviceInterfaces.GetAddress(2, 1);

    // 端点ノードごとに UDP Socket を 1 つずつ持つ（送受信兼用。上り/下りで分けない）。
    TypeId udpTid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sockNode0 = Socket::CreateSocket(nodes.Get(0), udpTid);
    Ptr<Socket> sockNode2 = Socket::CreateSocket(nodes.Get(2), udpTid);
    sockNode0->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), kDataPort));
    sockNode2->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), kDataPort));
    sockNode0->SetRecvCallback(MakeCallback(&RecvOnNode0));
    sockNode2->SetRecvCallback(MakeCallback(&RecvOnNode2));

    // DAO 完了後に端点同士で 1 パケットずつ送る（N0-N2 は無線的に非隣接）。
    const Time dataTime = Seconds(lateJoin + 6.0);
    Simulator::Schedule(dataTime, &SendUdp, sockNode2, n0Addr, std::string("Node2->Node0"));
    Simulator::Schedule(dataTime + MilliSeconds(100),
                        &SendUdp,
                        sockNode0,
                        n2Addr,
                        std::string("Node0->Node2"));

    std::cout << "Topology: Node0(root) --80m-- Node1 --80m-- Node2 (MaxRange=100m)\n"
              << "MAC PAN ID / short addresses set via MLME-SET; IPv6 IID follows the MAC.\n"
              << "RPL : Trickle DIO (Imin=8ms, doublings=20, k=10); MOP=2 (Storing) DAO.\n"
              << "Data: one UDP socket per endpoint; Node2->Node0 and Node0->Node2 after join.\n"
              << "Expect: N1 parent=N0 Rank2, N2 parent=N1 Rank3; Node0 DOWNWARD to N2 via N1.\n\n";
    PrintMacAndIpv6(lrwpanDevices, deviceInterfaces);
    std::cout << "\n";

    const Time simTime = Seconds(10 + lateJoin);
    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
    for (uint32_t i = 0; i < rpls.size(); ++i)
    {
        Simulator::Schedule(simTime, &Rpl::PrintRoutingTable, rpls[i], stream, Time::S);
    }

    Simulator::Stop(simTime + MicroSeconds(1));
    Simulator::Run();

    std::cout << "\n=== Final RPL state ===\n";
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        PrintNodeState(nodes.Get(i), "Node" + std::to_string(i));
    }

    std::cout << "\n=== DAO downward routes ===\n"
              << "  Node0 has DOWNWARD to N1: " << rpl0->HasDownwardRoute(n1Addr) << "\n"
              << "  Node0 has DOWNWARD to N2: " << rpl0->HasDownwardRoute(n2Addr) << "\n"
              << "  Node1 has DOWNWARD to N2: " << rpl1->HasDownwardRoute(n2Addr) << "\n";

    std::cout << "\n=== Data plane (UDP) ===\n"
              << "  Node0 received (from Node2): " << (g_node0Rx ? "OK" : "NG") << "\n"
              << "  Node2 received (from Node0): " << (g_node2Rx ? "OK" : "NG") << "\n";

    const bool pass =
        rpl0->HasDownwardRoute(n1Addr) && rpl0->HasDownwardRoute(n2Addr) &&
        rpl1->HasDownwardRoute(n2Addr) && rpl0->IsJoined() && rpl0->IsRoot() &&
        rpl0->GetRank() == 1 && rpl1->IsJoined() && rpl1->GetRank() == 2 &&
        !rpl1->GetPreferredParent().IsAny() && rpl2->IsJoined() && rpl2->GetRank() == 3 &&
        !rpl2->GetPreferredParent().IsAny() &&
        rpl2->GetPreferredParent() != rpl1->GetPreferredParent() && g_node0Rx && g_node2Rx;

    std::cout << "\n";
    if (pass)
    {
        std::cout << "PASS: multi-hop RPL join + DAO downward routes + UDP both directions.\n";
    }
    else
    {
        std::cout << "UNEXPECTED: parent/rank, DAO routes, or UDP data plane not as expected.\n";
    }

    Simulator::Destroy();
    return pass ? 0 : 1;
}
