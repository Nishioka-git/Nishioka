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
 * After RPL converges, a small UDP probe checks both directions:
 *   upward   Node2 -> Node0  (default route via Node1)
 *   downward Node0 -> Node2  (DAO DOWNWARD via Node1)
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

bool g_upwardRx = false;
bool g_downwardRx = false;

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
RecvUpward(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        Inet6SocketAddress src = Inet6SocketAddress::ConvertFrom(from);
        std::cout << Simulator::Now().As(Time::S) << " [Data Rx] upward Node0 from=" << src.GetIpv6()
                  << " size=" << packet->GetSize() << "\n";
        g_upwardRx = true;
    }
}

void
RecvDownward(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        Inet6SocketAddress src = Inet6SocketAddress::ConvertFrom(from);
        std::cout << Simulator::Now().As(Time::S)
                  << " [Data Rx] downward Node2 from=" << src.GetIpv6()
                  << " size=" << packet->GetSize() << "\n";
        g_downwardRx = true;
    }
}

void
SendUdp(Ptr<Socket> socket, Ipv6Address dst, const std::string& direction)
{
    Ptr<Packet> packet = Create<Packet>(kPayloadBytes);
    std::cout << Simulator::Now().As(Time::S) << " [Data Tx] " << direction << " dst=" << dst
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

    // MLME-SET で PAN ID と短縮アドレス 00:01, 00:02, 00:03 を決める。
    // 6LoWPAN / IPv6 の IID は MAC の GetAddress()（短縮アドレス由来）から決まる。
    for (uint32_t i = 0; i < lrwpanDevices.GetN(); ++i)
    {
        Ptr<LrWpanMac> mac = lrwpanDevices.Get(i)->GetObject<LrWpanNetDevice>()->GetMac();

        Ptr<MacPibAttributes> panAttr = Create<MacPibAttributes>();
        panAttr->macPanId = kPanId;
        mac->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, panAttr);

        static const char* const shortAddrs[] = {"00:01", "00:02", "00:03"};
        Ptr<MacPibAttributes> shortAttr = Create<MacPibAttributes>();
        shortAttr->macShortAddress = Mac16Address(shortAddrs[i]);
        mac->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, shortAttr);
    }

    RplHelper rplHelper;
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(rplHelper);
    internetv6.Install(nodes);

    Ptr<Rpl> rpl0 = GetRpl(nodes.Get(0));
    Ptr<Rpl> rpl1 = GetRpl(nodes.Get(1));
    Ptr<Rpl> rpl2 = GetRpl(nodes.Get(2));
    NS_ABORT_MSG_UNLESS(rpl0 && rpl1 && rpl2, "RPL routing protocol was not installed");
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

    TypeId udpTid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sock0 = Socket::CreateSocket(nodes.Get(0), udpTid);
    Ptr<Socket> sock2 = Socket::CreateSocket(nodes.Get(2), udpTid);
    sock0->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), kDataPort));
    sock2->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), kDataPort));
    sock0->SetRecvCallback(MakeCallback(&RecvUpward));
    sock2->SetRecvCallback(MakeCallback(&RecvDownward));

    // DAO 完了後に上り・下りを 1 パケットずつ送る（N0-N2 は無線的に非隣接）。
    const Time dataTime = Seconds(lateJoin + 6.0);
    Simulator::Schedule(dataTime, &SendUdp, sock2, n0Addr, std::string("upward Node2->Node0"));
    Simulator::Schedule(dataTime + MilliSeconds(100),
                        &SendUdp,
                        sock0,
                        n2Addr,
                        std::string("downward Node0->Node2"));

    std::cout << "Topology: Node0(root) --80m-- Node1 --80m-- Node2 (MaxRange=100m)\n"
              << "MAC PAN ID / short addresses set via MLME-SET; IPv6 IID follows the MAC.\n"
              << "RPL : Trickle DIO (Imin=8ms, doublings=20, k=10); MOP=2 (Storing) DAO.\n"
              << "Data: UDP upward N2->N0 and downward N0->N2 after RPL converges.\n"
              << "Expect: N1 parent=N0 Rank2, N2 parent=N1 Rank3; Node0 DOWNWARD to N2 via N1.\n\n";
    PrintMacAndIpv6(lrwpanDevices, deviceInterfaces);
    std::cout << "\n";

    const Time simTime = Seconds(10 + lateJoin);
    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
    Simulator::Schedule(simTime, &Rpl::PrintRoutingTable, rpl0, stream, Time::S);
    Simulator::Schedule(simTime, &Rpl::PrintRoutingTable, rpl1, stream, Time::S);
    Simulator::Schedule(simTime, &Rpl::PrintRoutingTable, rpl2, stream, Time::S);

    Simulator::Stop(simTime + MicroSeconds(1));
    Simulator::Run();

    std::cout << "\n=== Final RPL state ===\n";
    PrintNodeState(nodes.Get(0), "Node0");
    PrintNodeState(nodes.Get(1), "Node1");
    PrintNodeState(nodes.Get(2), "Node2");

    std::cout << "\n=== DAO downward routes ===\n"
              << "  Node0 has DOWNWARD to N1: " << rpl0->HasDownwardRoute(n1Addr) << "\n"
              << "  Node0 has DOWNWARD to N2: " << rpl0->HasDownwardRoute(n2Addr) << "\n"
              << "  Node1 has DOWNWARD to N2: " << rpl1->HasDownwardRoute(n2Addr) << "\n";

    std::cout << "\n=== Data plane (UDP) ===\n"
              << "  upward   Node2->Node0: " << (g_upwardRx ? "OK" : "NG") << "\n"
              << "  downward Node0->Node2: " << (g_downwardRx ? "OK" : "NG") << "\n";

    const bool pass =
        rpl0->HasDownwardRoute(n1Addr) && rpl0->HasDownwardRoute(n2Addr) &&
        rpl1->HasDownwardRoute(n2Addr) && rpl0->IsJoined() && rpl0->IsRoot() &&
        rpl0->GetRank() == 1 && rpl1->IsJoined() && rpl1->GetRank() == 2 &&
        !rpl1->GetPreferredParent().IsAny() && rpl2->IsJoined() && rpl2->GetRank() == 3 &&
        !rpl2->GetPreferredParent().IsAny() &&
        rpl2->GetPreferredParent() != rpl1->GetPreferredParent() && g_upwardRx && g_downwardRx;

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
