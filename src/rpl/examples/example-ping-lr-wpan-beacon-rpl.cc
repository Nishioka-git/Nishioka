#include "ns3/core-module.h"
#include "ns3/internet-apps-module.h"
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

uint32_t g_routeOutputOk = 0;
uint32_t g_routeOutputFail = 0;

std::string
SockErrToString(Socket::SocketErrno err)
{
    switch (err)
    {
    case Socket::ERROR_NOTERROR:
        return "ERROR_NOTERROR";
    case Socket::ERROR_NOROUTETOHOST:
        return "ERROR_NOROUTETOHOST";
    default:
        return "errno=" + std::to_string(static_cast<int>(err));
    }
}

void
RouteOutputProbe(std::string context,
                 Ptr<const Packet> /*packet*/,
                 Ipv6Address dst,
                 bool success,
                 Socket::SocketErrno sockerr)
{
    if (success)
    {
        ++g_routeOutputOk;
        std::cout << Simulator::Now().As(Time::S) << " [RPL RouteOutput OK] " << context
                  << " dst=" << dst << "\n";
    }
    else
    {
        ++g_routeOutputFail;
        std::cout << Simulator::Now().As(Time::S) << " [RPL RouteOutput FAIL] " << context
                  << " dst=" << dst << " " << SockErrToString(sockerr) << "\n";
    }
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

void
PrintRplTables(NodeContainer nodes)
{
    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Rpl> rpl = GetRpl(nodes.Get(i));
        if (rpl)
        {
            rpl->PrintRoutingTable(stream);
        }
    }
}

void
DataSentMacConfirm(Ptr<LrWpanNetDevice> device, McpsDataConfirmParams params)
{
    if (params.m_status == MacStatus::SUCCESS)
    {
        std::cout << Simulator::Now().As(Time::S) << " | Node " << device->GetNode()->GetId()
                  << " | Transmission successfully sent\n";
    }
}

} // namespace

int
main(int argc, char** argv)
{
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.Parse(argc, argv);

    LogComponentEnable("Rpl", LOG_LEVEL_WARN);
    if (verbose)
    {
        LogComponentEnableAll(LogLevel(LOG_PREFIX_TIME | LOG_PREFIX_FUNC | LOG_PREFIX_NODE));
        LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO);
        LogComponentEnable("Ping", LOG_LEVEL_INFO);
        LogComponentEnable("Rpl", LOG_LEVEL_LOGIC);
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

    Ptr<LrWpanNetDevice> dev1 = lrwpanDevices.Get(0)->GetObject<LrWpanNetDevice>();
    Ptr<LrWpanNetDevice> dev2 = lrwpanDevices.Get(1)->GetObject<LrWpanNetDevice>();
    dev1->GetMac()->SetMcpsDataConfirmCallback(MakeBoundCallback(&DataSentMacConfirm, dev1));
    dev2->GetMac()->SetMcpsDataConfirmCallback(MakeBoundCallback(&DataSentMacConfirm, dev2));

    lrWpanHelper.CreateAssociatedPan(lrwpanDevices, 5);

    MlmeStartRequestParams params;
    params.m_panCoor = true;
    params.m_PanId = 5;
    params.m_bcnOrd = 14;
    params.m_sfrmOrd = 13;
    params.m_logCh = 11;
    Simulator::ScheduleWithContext(dev1->GetNode()->GetId(),
                                   Seconds(0),
                                   &LrWpanMac::MlmeStartRequest,
                                   dev1->GetMac(),
                                   params);

    RplHelper rpl;
    InternetStackHelper internetv6;
    internetv6.SetRoutingHelper(rpl);
    internetv6.Install(nodes);

    Ptr<Rpl> rpl0 = GetRpl(nodes.Get(0));
    Ptr<Rpl> rpl1 = GetRpl(nodes.Get(1));
    NS_ABORT_MSG_UNLESS(rpl0 && rpl1, "RPL routing protocol was not installed");
    rpl0->SetRoot(true);

    rpl0->TraceConnect("RouteOutputProbe", "Node0", MakeCallback(&RouteOutputProbe));
    rpl1->TraceConnect("RouteOutputProbe", "Node1", MakeCallback(&RouteOutputProbe));

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces = ipv6.Assign(devices);

    Ipv6Address peerAddress = deviceInterfaces.GetAddress(1, 1);
    // Deliberately unreachable destination (not covered by 2001:2::/64)
    Ipv6Address pingTarget("2001:db8:bad::1");

    std::cout << "Ping unreachable " << pingTarget << " (peer is " << peerAddress << ")\n"
              << "Expected: [RPL RouteOutput FAIL] ERROR_NOROUTETOHOST\n\n";
    PrintRplTables(nodes);

    PingHelper ping(pingTarget);
    ping.SetAttribute("Count", UintegerValue(1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.)));
    ping.SetAttribute("Size", UintegerValue(16));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(2));
    apps.Stop(Seconds(7));

    AsciiTraceHelper ascii;
    lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("Ping-6LoW-lr-wpan-beacon-rpl.tr"));
    lrWpanHelper.EnablePcapAll(std::string("Ping-6LoW-lr-wpan-beacon-rpl"), true);

    Simulator::Stop(Seconds(7));
    Simulator::Run();

    std::cout << "\n=== RPL probe summary ===\n"
              << "RouteOutput OK=" << g_routeOutputOk << " FAIL=" << g_routeOutputFail << "\n";
    if (g_routeOutputFail > 0)
    {
        std::cout << "PASS: RPL RouteOutput reported NOROUTETOHOST (RPL is consulted).\n";
    }
    else
    {
        std::cout << "UNEXPECTED: no RouteOutput FAIL. Is RPL actually used?\n";
    }

    Simulator::Destroy();
    return 0;
}
