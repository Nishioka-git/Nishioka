/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 *
 * Scaffold example: RPL-like DODAG formation + data delivery over NishiokaStack.
 *
 * Topology (line):
 *   [Coordinator/ROOT] -- [dev01] -- [dev02]
 *
 * Control plane: DIO (root) -> parent selection -> DAO -> downward routes in NishiokaNwk.
 * Data plane:    NishiokaHeader::CUSTOM_DATA unicast relay via NishiokaNwk routing table.
 */

#include "ns3/core-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nishioka-header.h"
#include "ns3/nishioka-helper.h"
#include "ns3/nishioka-rpl-helper.h"
#include "ns3/nishioka-stack.h"
#include "ns3/nishioka-stack-container.h"
#include "ns3/nishioka-rpl-nwk.h"
#include "ns3/nishioka-rpl-routing.h"
#include "ns3/packet.h"
#include "ns3/propagation-module.h"
#include "ns3/simulator.h"
#include "ns3/spectrum-module.h"

#include <iostream>
#include <string>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::nishioka;

static const uint16_t kPanId = 0xCAFE;
static const uint8_t kChannel = 0xD;
static const Mac16Address kAddrRoot("00:01");
static const Mac16Address kAddrDev01("00:02");
static const Mac16Address kAddrDev02("00:03");

static Ptr<NishiokaStack> g_rootStack;
static Ptr<NishiokaStack> g_dev01Stack;
static Ptr<NishiokaStack> g_dev02Stack;
static NishiokaRplHelper g_rplHelper;

static uint32_t g_dataTx = 0;
static uint32_t g_dataRx = 0;
static uint32_t g_relay = 0;

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
GetShortAddr(Ptr<NishiokaStack> stack)
{
    if (!stack || !stack->GetMac())
    {
        return Mac16Address();
    }
    return stack->GetMac()->GetShortAddress();
}

static void
DataConfirm(McpsDataConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [DATA-CONFIRM] status="
              << static_cast<uint32_t>(params.m_status) << std::endl;
}

static void
RelayOrDeliver(Ptr<NishiokaStack> stack, McpsDataIndicationParams params, Ptr<Packet> p)
{
    Mac16Address myAddr = GetShortAddr(stack);
    NishiokaHelper& helper = g_rplHelper.GetNishiokaHelper();

    NishiokaHeader hdr;
    std::string payload;
    if (!helper.ExtractHeader(p, hdr, payload))
    {
        std::cout << Simulator::Now().As(Time::S) << " [DATA-RX-ERR] parse failed at " << myAddr
                  << std::endl;
        return;
    }

    if (hdr.GetFrameType() != NishiokaHeader::CUSTOM_DATA)
    {
        return;
    }

    Mac16Address finalDst = hdr.GetShortDstAddr();
    uint8_t battery = 0;
    uint8_t lqi = 0;
    uint8_t hops = 0;
    helper.ExtractRoutingInfo(hdr, battery, lqi, hops);

    std::cout << Simulator::Now().As(Time::S) << " [DATA-RX] node=" << myAddr << " from="
              << params.m_srcAddr << " finalDst=" << finalDst << " hops="
              << static_cast<uint32_t>(hops) << std::endl;

    if (myAddr == finalDst)
    {
        g_dataRx++;
        std::cout << "  [DELIVER] payload=\"" << payload << "\"" << std::endl;
        return;
    }

    Mac16Address nextHop;
    if (!stack->GetNwk()->GetNextHop(finalDst, nextHop))
    {
        std::cout << "  [DROP] no downward route for " << finalDst << std::endl;
        return;
    }

    if (nextHop == params.m_srcAddr)
    {
        std::cout << "  [DROP] loop guard" << std::endl;
        return;
    }

    Ptr<Packet> forwarded = helper.CreatePacket(payload,
                                                myAddr,
                                                finalDst,
                                                kPanId,
                                                100,
                                                params.m_mpduLinkQuality,
                                                static_cast<uint8_t>(hops + 1),
                                                hdr.GetSeqNum());

    McpsDataRequestParams req;
    req.m_dstPanId = kPanId;
    req.m_srcAddrMode = SHORT_ADDR;
    req.m_dstAddrMode = SHORT_ADDR;
    req.m_dstAddr = nextHop;
    req.m_msduHandle = 2;
    req.m_txOptions = TX_OPTION_NONE;

    g_dataTx++;
    g_relay++;
    std::cout << "  [RELAY] " << myAddr << " -> " << nextHop << std::endl;
    stack->GetMac()->McpsDataRequest(req, forwarded);
}

static void
RegisterDataCallbacks(const NishiokaStackContainer& stacks)
{
    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        Ptr<NishiokaStack> stack = stacks.Get(i);
        stack->GetNwk()->SetMcpsDataIndicationCallback(
            MakeBoundCallback(&RelayOrDeliver, stack));
        stack->GetNwk()->SetMcpsDataConfirmCallback(MakeCallback(&DataConfirm));
    }
}

static void
LogRplState(Ptr<NishiokaStack> stack, const std::string& label)
{
    Ptr<NishiokaRplRouting> rpl = NishiokaRplHelper::GetRplRouting(stack);
    if (!rpl)
    {
        return;
    }
    std::cout << "  [" << label << "] addr=" << GetShortAddr(stack)
              << " role=" << static_cast<uint32_t>(rpl->GetRole())
              << " rank=" << rpl->GetRank()
              << " nwkRoutes=" << stack->GetNwk()->GetRouteCount()
              << " downward=" << rpl->GetRoutingTable().GetDownwardRouteCount() << std::endl;
}

static void
PrintRplSummary()
{
    std::cout << "\n=== RPL Scaffold Summary ===\n";
    LogRplState(g_rootStack, "ROOT");
    LogRplState(g_dev01Stack, "DEV01");
    LogRplState(g_dev02Stack, "DEV02");
    std::cout << "Data TX (incl. relay): " << g_dataTx << "\n";
    std::cout << "Data delivered       : " << g_dataRx << "\n";
    std::cout << "Relay count          : " << g_relay << "\n";
}

static void
SendTestData()
{
    std::cout << "\n========== Send application data (root -> dev02) ==========\n";
    g_dataTx++;
    g_rplHelper.SendData(g_rootStack, "RPL-scaffold-test-data", kAddrDev02, 100, 255, 0);
}

int
main(int argc, char* argv[])
{
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<SingleModelSpectrumChannel> ch = CreateObject<SingleModelSpectrumChannel>();
    Ptr<RangePropagationLossModel> loss = CreateObject<RangePropagationLossModel>();
    loss->SetAttribute("MaxRange", DoubleValue(30.0));
    ch->AddPropagationLossModel(loss);
    ch->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetChannel(ch);
    NetDeviceContainer devices = lrWpanHelper.Install(nodes);

    const std::vector<Vector> positions = {Vector(0, 0, 0), Vector(10, 0, 0), Vector(20, 0, 0)};
    NishiokaStackContainer stacks = g_rplHelper.Install(devices, positions);

    g_rootStack = stacks.Get(0);
    g_dev01Stack = stacks.Get(1);
    g_dev02Stack = stacks.Get(2);

    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        Ptr<LrWpanNetDevice> dev = GetLrWpanDevice(stacks.Get(i));
        if (dev)
        {
            dev->SetChannel(ch);
        }
    }

    std::vector<Mac16Address> addresses = {kAddrRoot, kAddrDev01, kAddrDev02};
    g_rplHelper.ConfigureMac(stacks, kChannel, kPanId, addresses);

    g_rplHelper.ConfigureDodagRoot(g_rootStack, kAddrRoot, 0);

    RegisterDataCallbacks(stacks);

    Simulator::Schedule(Seconds(0.5), [&]() {
        std::cout << "\n========== Start DODAG (DIO from root) ==========\n";
        g_rplHelper.StartDodag(g_rootStack);
    });

    Simulator::Schedule(Seconds(3.0), &SendTestData);
    Simulator::Schedule(Seconds(4.5), &PrintRplSummary);

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
