/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 *
 * Example demonstrating the usage of NishiokaHelper
 */

#include "ns3/core-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/nishioka-helper.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/constant-position-mobility-model.h"

#include <iostream>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::nishioka;

// Global device pointer for callback
static Ptr<LrWpanNetDevice> g_rxDevice = nullptr;

// Receiver callback: extract and print NishiokaHeader information
static void
McpsIndication(const McpsDataIndicationParams params, Ptr<Packet> p)
{
    NishiokaHelper helper;
    NishiokaHeader header;
    std::string data;
    
    if (helper.ExtractHeader(p, header, data)) {
        uint8_t battery, lqi, hops;
        helper.ExtractRoutingInfo(header, battery, lqi, hops);
        
        uint32_t nodeId = 0;
        if (g_rxDevice) {
            nodeId = g_rxDevice->GetNode()->GetId();
        }
        
        std::cout << "[RX] Node " << nodeId
                  << " received packet:\n"
                  << "  Source: " << header.GetShortSrcAddr() << "\n"
                  << "  Destination: " << header.GetShortDstAddr() << "\n"
                  << "  Battery: " << (int)battery << "%\n"
                  << "  LQI: " << (int)lqi << "\n"
                  << "  Hops: " << (int)hops << "\n"
                  << "  Payload: \"" << data << "\"\n";
    } else {
        std::cout << "[RX] Failed to extract NishiokaHeader\n";
    }
}

// Schedule one transmission using NishiokaHelper
static void
SendOnce(Ptr<LrWpanNetDevice> dev, Mac16Address dst)
{
    NishiokaHelper helper;
    
    // Create packet with routing information using helper
    Ptr<Packet> packet = helper.CreatePacket(
        "Hello from NishiokaHelper",  // Data
        Mac16Address("00:01"),        // Source address
        dst,                           // Destination address
        0xCAFE,                       // PAN ID
        75,                            // Battery level (75%)
        200,                           // LQI
        1                              // Hops
    );
    
    McpsDataRequestParams params;
    params.m_dstPanId = 0xCAFE;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_srcAddrMode = SHORT_ADDR;
    params.m_dstAddr = dst;
    params.m_msduHandle = 0;
    params.m_txOptions = TX_OPTION_NONE;
    
    std::cout << "[TX] Sending packet using NishiokaHelper\n";
    dev->GetMac()->McpsDataRequest(params, packet);
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
    for (uint32_t i = 0; i < devs.GetN(); i++) {
        Ptr<LrWpanNetDevice> dev = devs.Get(i)->GetObject<LrWpanNetDevice>();
        dev->SetChannel(channel);
    }
    
    Ptr<LrWpanNetDevice> txDev = devs.Get(0)->GetObject<LrWpanNetDevice>();
    Ptr<LrWpanNetDevice> rxDev = devs.Get(1)->GetObject<LrWpanNetDevice>();
    
    txDev->GetMac()->SetShortAddress(Mac16Address("00:01"));
    rxDev->GetMac()->SetShortAddress(Mac16Address("00:02"));
    
    // Set global device pointer for callback
    g_rxDevice = rxDev;
    
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

