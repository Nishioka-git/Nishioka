/*
 * Static multi-hop (multihop2-like flow) using LrWpanHelper + NishiokaStack.
 *
 * Node layout per device:
 *   LrWpanNetDevice (PHY+MAC via LrWpanHelper)
 *   NishiokaStack (NWK via NishiokaHelper)
 *
 * Flow:
 * 1) Static routes in NishiokaNwk
 * 2) Sequential association (deterministic addresses)
 * 3) Unicast relay by NWK routing table lookup (final destination in NishiokaHeader)
 */

#include "ns3/core-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nishioka-header.h"
#include "ns3/nishioka-helper.h"
#include "ns3/nishioka-stack.h"
#include "ns3/nishioka-stack-container.h"
#include "ns3/nishioka-nwk.h"
#include "ns3/packet.h"
#include "ns3/propagation-module.h"
#include "ns3/simulator.h"
#include "ns3/spectrum-module.h"

#include <iostream>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::nishioka;

static const uint16_t kPanId = 0;
static const uint8_t kChannel = 0xD;
static const Mac16Address kAddrCoord("00:01");
static const Mac16Address kAddrDev01("00:02");
static const Mac16Address kAddrDev02("00:03");
static const Mac16Address kAddrDev03("00:04");
static const Mac16Address kAddrDev04("00:05");

static Ptr<NishiokaStack> g_coordStack;
static Ptr<NishiokaStack> g_dev01Stack;
static Ptr<NishiokaStack> g_dev02Stack;
static Ptr<NishiokaStack> g_dev03Stack;
static Ptr<NishiokaStack> g_dev04Stack;

static uint16_t g_nextAddrCoord = 0x0002;
static uint16_t g_nextAddrDev01 = 0x0003;
static uint16_t g_nextAddrDev02 = 0x0004;

static Mac16Address g_dev01Assigned = Mac16Address("00:02");
static Mac16Address g_dev02Assigned = Mac16Address("00:03");
static Mac16Address g_dev03Assigned = Mac16Address("00:04");
static Mac16Address g_dev04Assigned = Mac16Address("00:05");

static uint32_t g_txCount = 0;
static uint32_t g_rxCount = 0;
static uint32_t g_relayCount = 0;
static uint16_t g_routeLogSeq = 0;

static Ptr<LrWpanNetDevice>
GetLrWpanDevice(Ptr<NishiokaStack> stack)
{
    if (!stack || !stack->GetNetDevice())
    {
        return nullptr;
    }
    return stack->GetNetDevice()->GetObject<LrWpanNetDevice>();
}

static Mac16Address
GetStackShortAddress(Ptr<NishiokaStack> stack)
{
    Ptr<LrWpanNetDevice> dev = GetLrWpanDevice(stack);
    if (!dev || !dev->GetMac())
    {
        return Mac16Address();
    }
    return dev->GetMac()->GetShortAddress();
}

static void
LogRouteInstall(Ptr<NishiokaStack> stack,
                Mac16Address dst,
                Mac16Address nextHop,
                uint8_t energy,
                uint8_t lqi,
                uint8_t hops)
{
    Ptr<NishiokaNwk> nwk = stack->GetNwk();
    if (!nwk)
    {
        return;
    }
    nwk->SetRoute(dst, nextHop);
    std::cout << "[ROUTING TABLE UPDATE] EntryID: " << g_routeLogSeq++ << " | Node="
              << GetStackShortAddress(stack) << " | Dst: " << dst << " | NextHop: " << nextHop
              << " | Energy: " << static_cast<int>(energy) << " | LQI: " << static_cast<int>(lqi)
              << " | Hops: " << static_cast<int>(hops) << " (NishiokaNwk)" << std::endl;
}

static void
SendDataCoordinatorToDestination(const Mac16Address& finalDst, const std::string& sendMsg)
{
    Mac16Address coordinatorAddr = GetStackShortAddress(g_coordStack);
    Mac16Address nextHop;
    if (!g_coordStack->GetNwk()->GetNextHop(finalDst, nextHop))
    {
        std::cout << Simulator::Now().As(Time::S) << " [ERROR] No route to " << finalDst
                  << " in coordinator NWK table!" << std::endl;
        return;
    }

    std::cout << Simulator::Now().As(Time::S) << " [SEND DATA] Coordinator (" << coordinatorAddr
              << ") -> " << nextHop << " (NextHop from NWK table) | finalDst=" << finalDst
              << std::endl;

    NishiokaHelper helper;
    Ptr<Packet> packet = helper.CreatePacket(sendMsg,
                                             coordinatorAddr,
                                             finalDst,
                                             kPanId,
                                             100,
                                             255,
                                             0,
                                             1);

    McpsDataRequestParams dataParams;
    dataParams.m_dstPanId = kPanId;
    dataParams.m_dstAddrMode = SHORT_ADDR;
    dataParams.m_dstAddr = nextHop;
    dataParams.m_msduHandle = 3;
    dataParams.m_txOptions = TX_OPTION_NONE;
    dataParams.m_srcAddrMode = SHORT_ADDR;

    g_txCount++;
    std::cout << "  Total sent packets: " << g_txCount << std::endl;

    Ptr<LrWpanNetDevice> coordDev = GetLrWpanDevice(g_coordStack);
    if (!coordDev || !coordDev->GetMac())
    {
        return;
    }
    coordDev->GetMac()->McpsDataRequest(dataParams, packet);
}

static void
DataConfirm(McpsDataConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [CONFIRM] status="
              << static_cast<uint32_t>(params.m_status)
              << " | handle=" << static_cast<uint32_t>(params.m_msduHandle) << std::endl;
}

static void
DataIndication(Ptr<NishiokaStack> stack, McpsDataIndicationParams params, Ptr<Packet> p)
{
    g_rxCount++;
    Mac16Address myAddr = params.m_dstAddr;

    NishiokaHelper helper;
    NishiokaHeader hdr;
    std::string payload;
    if (!helper.ExtractHeader(p, hdr, payload))
    {
        std::cout << Simulator::Now().As(Time::S) << " [RX-ERROR] parse failed at " << myAddr
                  << std::endl;
        return;
    }

    uint8_t battery = 0;
    uint8_t lqi = 0;
    uint8_t hops = 0;
    helper.ExtractRoutingInfo(hdr, battery, lqi, hops);
    Mac16Address finalDst = hdr.GetShortDstAddr();

    std::cout << Simulator::Now().As(Time::S) << " [RX] node=" << myAddr << " from="
              << params.m_srcAddr << " | finalDst=" << finalDst << " | hops="
              << static_cast<uint32_t>(hops) << std::endl;
    std::cout << "  [NISHIOKA-HDR RX] src=" << hdr.GetShortSrcAddr() << " dst=" << finalDst
              << " pan=0x" << std::hex << hdr.GetDstPanId() << std::dec
              << " seq=" << static_cast<uint32_t>(hdr.GetSeqNum())
              << " battery=" << static_cast<uint32_t>(battery)
              << " lqi=" << static_cast<uint32_t>(lqi)
              << " hops=" << static_cast<uint32_t>(hops) << std::endl;

    if (myAddr == finalDst)
    {
        std::cout << "  [DELIVER] destination matched at " << myAddr << std::endl;
        return;
    }

    if (!stack)
    {
        std::cout << "  [IGNORE] unknown receiver (no stack)" << std::endl;
        return;
    }

    Mac16Address nextHop;
    if (!stack->GetNwk()->GetNextHop(finalDst, nextHop))
    {
        std::cout << "  [IGNORE] not destination and no route in NishiokaNwk" << std::endl;
        return;
    }

    if (nextHop == params.m_srcAddr)
    {
        std::cout << "  [IGNORE] loop guard (nextHop == sender)" << std::endl;
        return;
    }

    uint8_t relayHops = static_cast<uint8_t>(hops + 1);
    uint8_t relayLqi = params.m_mpduLinkQuality;

    Ptr<Packet> forwarded = helper.CreatePacket(payload,
                                                myAddr,
                                                finalDst,
                                                kPanId,
                                                100,
                                                relayLqi,
                                                relayHops,
                                                hdr.GetSeqNum());

    McpsDataRequestParams req;
    req.m_dstPanId = kPanId;
    req.m_srcAddrMode = SHORT_ADDR;
    req.m_dstAddrMode = SHORT_ADDR;
    req.m_dstAddr = nextHop;
    req.m_msduHandle = 2;
    req.m_txOptions = TX_OPTION_NONE;

    g_txCount++;
    g_relayCount++;
    std::cout << "  [RELAY] " << myAddr << " -> " << nextHop
              << " | hops=" << static_cast<uint32_t>(relayHops) << std::endl;
    std::cout << "  [NISHIOKA-HDR RELAY] src=" << myAddr << " dst=" << finalDst
              << " battery=100 lqi=" << static_cast<uint32_t>(relayLqi)
              << " hops=" << static_cast<uint32_t>(relayHops) << std::endl;

    Ptr<LrWpanNetDevice> dev = GetLrWpanDevice(stack);
    if (!dev || !dev->GetMac())
    {
        return;
    }
    dev->GetMac()->McpsDataRequest(req, forwarded);
}

static void
InstallStaticRoutesToDev04()
{
    LogRouteInstall(g_coordStack, kAddrDev04, kAddrDev01, 100, 255, 0);
    LogRouteInstall(g_dev01Stack, kAddrDev04, kAddrDev02, 100, 255, 0);
    LogRouteInstall(g_dev02Stack, kAddrDev04, kAddrDev04, 100, 255, 0);
}

static void
AssociateDev01ToCoordinator()
{
    g_dev01Assigned = Mac16Address::ConvertFrom(Mac16Address(g_nextAddrCoord++));
    GetLrWpanDevice(g_dev01Stack)->GetMac()->SetShortAddress(g_dev01Assigned);
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-IND] parent=" << kAddrCoord
              << " assigned=" << g_dev01Assigned << std::endl;
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-CONFIRM] dev01=" << g_dev01Assigned
              << std::endl;
}

static void
AssociateDev02ToDev01()
{
    g_dev02Assigned = Mac16Address::ConvertFrom(Mac16Address(g_nextAddrDev01++));
    GetLrWpanDevice(g_dev02Stack)->GetMac()->SetShortAddress(g_dev02Assigned);
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-IND] parent=" << g_dev01Assigned
              << " assigned=" << g_dev02Assigned << std::endl;
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-CONFIRM] dev02=" << g_dev02Assigned
              << std::endl;
}

static void
AssociateDev03ToDev02()
{
    g_dev03Assigned = Mac16Address::ConvertFrom(Mac16Address(g_nextAddrDev02++));
    GetLrWpanDevice(g_dev03Stack)->GetMac()->SetShortAddress(g_dev03Assigned);
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-IND] parent=" << g_dev02Assigned
              << " assigned=" << g_dev03Assigned << std::endl;
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-CONFIRM] dev03=" << g_dev03Assigned
              << std::endl;
}

static void
AssociateDev04ToDev02()
{
    g_dev04Assigned = Mac16Address::ConvertFrom(Mac16Address(g_nextAddrDev02++));
    GetLrWpanDevice(g_dev04Stack)->GetMac()->SetShortAddress(g_dev04Assigned);
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-IND] parent=" << g_dev02Assigned
              << " assigned=" << g_dev04Assigned << std::endl;
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC-CONFIRM] dev04=" << g_dev04Assigned
              << std::endl;

    Simulator::Schedule(Seconds(0.2), [=]() {
        Mac16Address targetAddr = kAddrDev04;
        std::cout << "\n========== Static route ready ==========\n";
        std::cout << "Coordinator will send toward " << targetAddr
                  << " (lookup NishiokaNwk on coordinator stack)\n";
        std::cout << "=========================================\n\n";

        Simulator::Schedule(Seconds(0.5), [=]() {
            SendDataCoordinatorToDestination(targetAddr, "Static-unicast-check");
        });
    });
}

static void
SetupMacAddressesAndPan(const NishiokaStackContainer& stacks,
                        const std::vector<Mac16Address>& addresses)
{
    static const Mac64Address extAddrs[] = {
        Mac64Address("00:00:00:00:00:00:00:01"),
        Mac64Address("00:00:00:00:00:00:00:02"),
        Mac64Address("00:00:00:00:00:00:00:03"),
        Mac64Address("00:00:00:00:00:00:00:04"),
        Mac64Address("00:00:00:00:00:00:00:05"),
    };

    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        Ptr<LrWpanNetDevice> dev = GetLrWpanDevice(stacks.Get(i));
        if (!dev || !dev->GetMac() || i >= addresses.size())
        {
            continue;
        }
        dev->GetMac()->SetExtendedAddress(extAddrs[i]);
        dev->GetMac()->SetShortAddress(addresses[i]);
        dev->GetMac()->SetPanId(kPanId);
        dev->GetMac()->SetRxOnWhenIdle(true);
    }
}

static void
RegisterMacCallbacks(NishiokaStackContainer stacks)
{
    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        Ptr<NishiokaStack> stack = stacks.Get(i);
        Ptr<LrWpanNetDevice> dev = GetLrWpanDevice(stack);
        if (!dev || !dev->GetMac())
        {
            continue;
        }
        dev->GetMac()->SetMcpsDataConfirmCallback(MakeCallback(&DataConfirm));
        dev->GetMac()->SetMcpsDataIndicationCallback(
            MakeBoundCallback(&DataIndication, stack));
    }
}

static void
SetupPhyMobility(const NishiokaStackContainer& stacks, Ptr<SpectrumChannel> ch)
{
    const std::vector<Vector> positions = {
        Vector(0, 0, 0),
        Vector(10, 0, 0),
        Vector(20, 0, 0),
        Vector(20, 10, 0),
        Vector(30, 0, 0),
    };

    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        Ptr<LrWpanNetDevice> dev = GetLrWpanDevice(stacks.Get(i));
        if (!dev)
        {
            continue;
        }
        dev->SetChannel(ch);
        Ptr<ConstantPositionMobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
        mobility->SetPosition(positions[i]);
        dev->GetPhy()->SetMobility(mobility);
    }
}

int
main(int argc, char* argv[])
{
    // 5-node topology: coordinator, dev01, dev02, dev03 (reachable but not routed), dev04 (final dst)
    NodeContainer nodes;
    nodes.Create(5);

    Ptr<SingleModelSpectrumChannel> ch = CreateObject<SingleModelSpectrumChannel>();
    Ptr<RangePropagationLossModel> loss = CreateObject<RangePropagationLossModel>();
    loss->SetAttribute("MaxRange", DoubleValue(120.0));
    ch->AddPropagationLossModel(loss);
    ch->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetChannel(ch);
    NetDeviceContainer devices = lrWpanHelper.Install(nodes);

    NishiokaHelper nishiokaHelper;
    NishiokaStackContainer stacks = nishiokaHelper.Install(devices);

    g_coordStack = stacks.Get(0);
    g_dev01Stack = stacks.Get(1);
    g_dev02Stack = stacks.Get(2);
    g_dev03Stack = stacks.Get(3);
    g_dev04Stack = stacks.Get(4);

    SetupPhyMobility(stacks, ch);

    std::vector<Mac16Address> addresses = {kAddrCoord, kAddrDev01, kAddrDev02, kAddrDev03, kAddrDev04};
    SetupMacAddressesAndPan(stacks, addresses);

    Simulator::Schedule(Seconds(0.001), &RegisterMacCallbacks, stacks);

    InstallStaticRoutesToDev04();

    Simulator::Schedule(Seconds(0.2), &AssociateDev01ToCoordinator);
    Simulator::Schedule(Seconds(0.6), &AssociateDev02ToDev01);
    Simulator::Schedule(Seconds(1.0), &AssociateDev03ToDev02);
    Simulator::Schedule(Seconds(1.4), &AssociateDev04ToDev02);

    Simulator::Stop(Seconds(5.0));
    const uint32_t coordRouteCount = g_coordStack->GetNwk()->GetRouteCount();
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\n=== Static Multi-hop Result ===\n";
    std::cout << "TX count   : " << g_txCount << "\n";
    std::cout << "RX count   : " << g_rxCount << "\n";
    std::cout << "Relay count: " << g_relayCount << "\n";
    std::cout << "Coordinator NWK routes: " << coordRouteCount << "\n";
    std::cout << "Broadcast TX used: NO (unicast dst only)\n";
    std::cout << "Non-destination without route: IGNORED\n";
    return 0;
}
