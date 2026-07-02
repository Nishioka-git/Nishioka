#include "ns3/core-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/rpl-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;
using namespace ns3::lrwpan;

static void
DataSentMacConfirm(Ptr<LrWpanNetDevice> device, McpsDataConfirmParams params)
{
    if (params.m_status == MacStatus::SUCCESS)
    {
        std::cout << Simulator::Now().As(Time::S) << " | Node " << device->GetNode()->GetId()
                  << " | Transmission successfully sent\n";
    }
}

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
        LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO);
        LogComponentEnable("LrWpanCsmaCa", LOG_LEVEL_INFO);
        LogComponentEnable("LrWpanHelper", LOG_LEVEL_ALL);
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
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
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

    Ptr<Rpl> rootRpl = DynamicCast<Rpl>(nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    if (rootRpl)
    {
        rootRpl->SetRoot(true);
    }

    SixLowPanHelper sixlowpan;
    NetDeviceContainer devices = sixlowpan.Install(lrwpanDevices);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces = ipv6.Assign(devices);

    uint32_t packetSize = 16;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1);
    PingHelper ping(deviceInterfaces.GetAddress(1, 1));

    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ApplicationContainer apps = ping.Install(nodes.Get(0));

    apps.Start(Seconds(2));
    apps.Stop(Seconds(7));

    AsciiTraceHelper ascii;
    lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("Ping-6LoW-lr-wpan-beacon-rpl.tr"));
    lrWpanHelper.EnablePcapAll(std::string("Ping-6LoW-lr-wpan-beacon-rpl"), true);

    Simulator::Stop(Seconds(7));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
