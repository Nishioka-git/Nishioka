/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Simple example: build a tiny lr-wpan link, attach NishiokaHeader
 * (src/dst + battery + evaluation), send once via McpsDataRequest,
 * and extract it at the receiver.
 */

#include "ns3/core-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/nishioka-header.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/constant-position-mobility-model.h"

#include <iostream>

using namespace ns3;
using namespace ns3::nishioka;
using namespace ns3::lrwpan;

// Receiver callback: remove NishiokaHeader and print fields
static void
McpsIndication(const McpsDataIndicationParams params, Ptr<Packet> p)
{
    NishiokaHeader hdr;
    // Set address modes before RemoveHeader (short addresses here)
    hdr.SetDstAddrMode(NishiokaHeader::SHORTADDR);
    hdr.SetSrcAddrMode(NishiokaHeader::SHORTADDR);

    p->RemoveHeader(hdr);

    std::cout << "[RX] Dst: " << hdr.GetShortDstAddr()
              << " Src: " << hdr.GetShortSrcAddr()
              << " Battery: " << hdr.GetBattery()
              << " Evaluation: " << hdr.GetEvaluation()
              << " Payload size: " << p->GetSize() << " bytes\n";
}

// Schedule one transmission with NishiokaHeader attached
static void
SendOnce(Ptr<LrWpanNetDevice> dev, Mac16Address dst)
{
    Ptr<Packet> payload = Create<Packet>((const uint8_t*)"hello", 5);

    NishiokaHeader hdr;
    hdr.SetFrameType(NishiokaHeader::CUSTOM_DATA);
    hdr.SetSeqNum(1);
    hdr.SetDstAddrFields(0xCAFE, dst);
    hdr.SetSrcAddrFields(0xCAFE, Mac16Address("00:01"));
    hdr.SetBattery(75);
    hdr.SetEvaluation(300);

    payload->AddHeader(hdr);

    McpsDataRequestParams params;
    params.m_dstPanId = 0xCAFE;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_srcAddrMode = SHORT_ADDR;
    params.m_dstAddr = Mac16Address(dst);
    params.m_msduHandle = 0;   // sequence for MAC retries (not used here)
    params.m_txOptions = TX_OPTION_NONE; // unacknowledged single send

    dev->GetMac()->McpsDataRequest(params, payload);
}

int
main()
{
    Packet::EnablePrinting();

    NodeContainer nodes;
    nodes.Create(2);

    // Channel and devices
    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    Ptr<ConstantSpeedPropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
    channel->SetPropagationDelayModel(delay);
    channel->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());

    LrWpanHelper helper;
    NetDeviceContainer devs = helper.Install(nodes);
    
    // Set channel for all devices
    for (uint32_t i = 0; i < devs.GetN(); i++)
    {
        Ptr<LrWpanNetDevice> dev = devs.Get(i)->GetObject<LrWpanNetDevice>();
        dev->SetChannel(channel);
    }

    Ptr<LrWpanNetDevice> txDev = devs.Get(0)->GetObject<LrWpanNetDevice>();
    Ptr<LrWpanNetDevice> rxDev = devs.Get(1)->GetObject<LrWpanNetDevice>();

    txDev->GetMac()->SetShortAddress(Mac16Address("00:01"));
    rxDev->GetMac()->SetShortAddress(Mac16Address("00:02"));

    // RX callback
    rxDev->GetMac()->SetMcpsDataIndicationCallback(MakeCallback(&McpsIndication));

    // Schedule one send at t=0.5s
    Simulator::ScheduleWithContext(txDev->GetNode()->GetId(),
                                   Seconds(0.5),
                                   &SendOnce,
                                   txDev,
                                   Mac16Address("00:02"));

    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}