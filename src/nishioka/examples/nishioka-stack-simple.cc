/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 *
 * Simple example demonstrating NishiokaStack packet transmission
 * This example creates two nodes, installs NishiokaStack on them,
 * and sends a packet from node 0 to node 1.
 */

#include "ns3/core-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/nishioka-header.h"
#include "ns3/nishioka-helper.h"
#include "ns3/nishioka-stack.h"
#include "ns3/nishioka-stack-container.h"
#include "ns3/nishioka-nwk.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <iostream>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::nishioka;

// Global variables for statistics
uint32_t g_txCount = 0;
uint32_t g_rxCount = 0;
Ptr<NishiokaStack> g_stack0 = nullptr;  // For delayed packet transmission
Mac16Address g_dstAddr;

// Forward declaration
static void SendPacket(Ptr<NishiokaStack> stack, Mac16Address dstAddr);

/**
 * Callback function for PAN coordinator start confirmation
 */
static void
StartConfirm(MlmeStartConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [PAN START CONFIRM] Status: " 
              << static_cast<int>(params.m_status) << std::endl;
    if (params.m_status == MacStatus::SUCCESS)
    {
        std::cout << "  PAN coordinator started successfully\n" << std::endl;
        // Schedule packet transmission after PAN is ready
        if (g_stack0)
        {
            Simulator::Schedule(Seconds(0.5), &SendPacket, g_stack0, g_dstAddr);
        }
    }
    else
    {
        std::cout << "  PAN coordinator start failed!\n" << std::endl;
    }
}

/**
 * Callback function for packet transmission confirmation
 */
static void
McpsConfirm(const McpsDataConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [TX CONFIRM] Status: " 
              << static_cast<int>(params.m_status);
    if (params.m_status == MacStatus::SUCCESS)
    {
        std::cout << " (SUCCESS)" << std::endl;
    }
    else if (params.m_status == MacStatus::NO_ACK)
    {
        std::cout << " (NO_ACK - ACK not received)" << std::endl;
    }
    else
    {
        std::cout << " (ERROR)" << std::endl;
    }
}

/**
 * Callback function for packet reception (MAC layer callback)
 */
static void
McpsIndication(const McpsDataIndicationParams params, Ptr<Packet> p)
{
    std::cout << Simulator::Now().As(Time::S) << " [RX] Node received packet:\n";
    std::cout << "  Source: " << params.m_srcAddr << "\n";
    std::cout << "  Destination: " << params.m_dstAddr << "\n";

    // Extract NishiokaHeader
    NishiokaHelper helper;
    NishiokaHeader header;
    std::string data;

    if (helper.ExtractHeader(p, header, data))
    {
        uint8_t battery, lqi, hops;
        helper.ExtractRoutingInfo(header, battery, lqi, hops);

        std::cout << "  Battery: " << (int)battery << "%\n";
        std::cout << "  LQI: " << (int)lqi << "\n";
        std::cout << "  Hops: " << (int)hops << "\n";
        std::cout << "  Payload: \"" << data << "\"\n";
        g_rxCount++;
    }
    else
    {
        std::cout << "  Failed to extract NishiokaHeader\n";
    }
    std::cout << std::endl;
}

/**
 * Function to send a packet using NishiokaStack (directly through MAC layer)
 */
static void
SendPacket(Ptr<NishiokaStack> stack, Mac16Address dstAddr)
{
    std::cout << Simulator::Now().As(Time::S) << " [TX] Sending packet to " << dstAddr << "\n";

    // Create packet with NishiokaHeader using NishiokaHelper
    NishiokaHelper helper;
    Ptr<Packet> packet = helper.CreatePacket(
        "Hello from NishiokaStack!",  // Data
        Mac16Address("00:01"),        // Source address
        dstAddr,                       // Destination address
        0xCAFE,                       // PAN ID
        85,                           // Battery level (85%)
        200,                          // LQI
        0,                            // Hops
        1                             // Sequence number
    );

    // Get MAC layer and send packet directly
    Ptr<LrWpanMacBase> mac = stack->GetMac();
    if (mac)
    {
        McpsDataRequestParams params;
        params.m_dstPanId = 0xCAFE;
        params.m_srcAddrMode = SHORT_ADDR;  // 送信元アドレスモードを明示的に設定
        params.m_dstAddrMode = SHORT_ADDR;
        params.m_dstAddr = dstAddr;
        params.m_msduHandle = 0;
        // Try without ACK first to see if packet is received
        // If it works, we can enable ACK later for reliability
        params.m_txOptions = TX_OPTION_NONE;  // No ACK required (simpler for testing)

        mac->McpsDataRequest(params, packet);
        g_txCount++;
        std::cout << "  Packet sent successfully\n" << std::endl;
    }
    else
    {
        std::cout << "  Error: MAC layer not available\n" << std::endl;
    }
}

int
main(int argc, char* argv[])
{
    // Enable logging
    LogComponentEnable("NishiokaStack", LOG_LEVEL_INFO);
    LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO);

    std::cout << "=== NishiokaStack Simple Example ===\n" << std::endl;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    std::cout << "Created " << nodes.GetN() << " nodes\n" << std::endl;

    // Create mobility model (nodes need position)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX",
                                   DoubleValue(0.0),
                                   "MinY",
                                   DoubleValue(0.0),
                                   "DeltaX",
                                   DoubleValue(10.0),
                                   "DeltaY",
                                   DoubleValue(10.0),
                                   "GridWidth",
                                   UintegerValue(2),
                                   "LayoutType",
                                   StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    std::cout << "Installed mobility models\n" << std::endl;

    // Create channel
    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    Ptr<LogDistancePropagationLossModel> propModel = CreateObject<LogDistancePropagationLossModel>();
    propModel->SetPathLossExponent(2.0);
    channel->AddPropagationLossModel(propModel);
    Ptr<ConstantSpeedPropagationDelayModel> delayModel =
        CreateObject<ConstantSpeedPropagationDelayModel>();
    channel->SetPropagationDelayModel(delayModel);

    std::cout << "Created channel\n" << std::endl;

    // Create and install LrWpanNetDevice
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devices = lrWpanHelper.Install(nodes);

    // Set channel for devices
    for (uint32_t i = 0; i < devices.GetN(); i++)
    {
        Ptr<LrWpanNetDevice> dev = devices.Get(i)->GetObject<LrWpanNetDevice>();
        dev->SetChannel(channel);
    }

    // Get devices
    Ptr<LrWpanNetDevice> dev0 = devices.Get(0)->GetObject<LrWpanNetDevice>();
    Ptr<LrWpanNetDevice> dev1 = devices.Get(1)->GetObject<LrWpanNetDevice>();

    std::cout << "Installed LrWpanNetDevices\n" << std::endl;

    // Install NishiokaStack on each LrWpanNetDevice (same node as the device)
    NishiokaHelper helper;
    NishiokaStackContainer stacks = helper.Install(devices);

    Ptr<NishiokaStack> stack0 = stacks.Get(0);
    Ptr<NishiokaStack> stack1 = stacks.Get(1);

    std::cout << "Installed NishiokaStack on both nodes\n" << std::endl;

    // Configure MAC layer settings using helper

    // Set MAC addresses and PAN ID using helper
    std::vector<Mac16Address> addresses;
    addresses.push_back(Mac16Address("00:01"));  // Node 0
    addresses.push_back(Mac16Address("00:02"));    // Node 1
    helper.ConfigureMac(stacks, 0xD, 0xCAFE, addresses);

    // For LrWpanNetDevice, also set addresses directly to ensure they are set
    dev0->GetMac()->SetShortAddress(Mac16Address("00:01"));
    dev1->GetMac()->SetShortAddress(Mac16Address("00:02"));
    dev0->GetMac()->SetPanId(0xCAFE);
    dev1->GetMac()->SetPanId(0xCAFE);

    std::cout << "Configured MAC layer settings\n";
    std::cout << "  Node 0: Address " << dev0->GetMac()->GetShortAddress() << "\n";
    std::cout << "  Node 1: Address " << dev1->GetMac()->GetShortAddress() << "\n" << std::endl;

    // Set up PAN association for LrWpan communication
    // Node 0 acts as coordinator, Node 1 as end device
    // Manual association: Set Node 1 to be associated with Node 0 (coordinator)
    dev1->GetMac()->SetAssociatedCoor(Mac16Address("00:01"));
    std::cout << "Set up PAN association: Node 1 associated with Node 0 (coordinator)\n" << std::endl;

    // Set up PAN start confirmation callback
    dev0->GetMac()->SetMlmeStartConfirmCallback(MakeCallback(&StartConfirm));

    // Start PAN coordinator (Node 0) - this enables beacon transmission
    // For non-beacon mode, we can use a simple start request
    MlmeStartRequestParams startParams;
    startParams.m_panCoor = true;
    startParams.m_PanId = 0xCAFE;
    startParams.m_logCh = 0xD;
    startParams.m_logChPage = 0;
    startParams.m_bcnOrd = 15;  // Non-beacon mode (15 = no beacons)
    startParams.m_sfrmOrd = 15; // Non-beacon mode
    startParams.m_battLifeExt = false;
    startParams.m_coorRealgn = false;

    // Store stack and destination for delayed transmission
    g_stack0 = stack0;
    g_dstAddr = Mac16Address("00:02");

    // Schedule PAN start - packet transmission will be scheduled after PAN is ready
    Simulator::Schedule(Seconds(0.1),
                       &LrWpanMac::MlmeStartRequest,
                       dev0->GetMac(),
                       startParams);
    std::cout << "Scheduled PAN coordinator start at 0.1 seconds\n" << std::endl;

    // Set up routing in NWK layer (example: if node 0 wants to send to node 1 via node 2)
    // For this simple example, we set direct routes
    Ptr<NishiokaNwk> nwk0 = stack0->GetNwk();
    Ptr<NishiokaNwk> nwk1 = stack1->GetNwk();

    // Set direct route from node 0 to node 1
    nwk0->SetRoute(Mac16Address("00:02"), Mac16Address("00:02"));
    std::cout << "Set route in NWK layer: Node 0 -> Node 1 (direct)" << std::endl;

    // Example: If there was a node 2 (00:03), you could set:
    // nwk0->SetRoute(Mac16Address("00:03"), Mac16Address("00:02")); // Route to 00:03 via 00:02
    // nwk0->SetRoute(Mac16Address("00:01"), Mac16Address("00:01")); // Route to self

    std::cout << "Routing table size: " << nwk0->GetRouteCount() << " routes\n" << std::endl;

    // Set up callbacks on both nodes
    Ptr<LrWpanMacBase> mac0 = stack0->GetMac();
    Ptr<LrWpanMacBase> mac1 = stack1->GetMac();
    
    // Set transmission confirmation callback on sender
    mac0->SetMcpsDataConfirmCallback(MakeCallback(&McpsConfirm));
    
    // Set data indication callback on receiver
    mac1->SetMcpsDataIndicationCallback(MakeCallback(&McpsIndication));
    
    // Also set on sender in case of bidirectional communication
    mac0->SetMcpsDataIndicationCallback(MakeCallback(&McpsIndication));

    std::cout << "Set up data indication and confirmation callbacks\n" << std::endl;

    // Note: Packet transmission will be scheduled after PAN coordinator start is confirmed
    // (see StartConfirm callback)
    std::cout << "Packet transmission will be scheduled after PAN coordinator is ready\n" << std::endl;

    // Run simulation
    std::cout << "Starting simulation...\n" << std::endl;
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    // Print statistics
    std::cout << "\n=== Simulation Results ===" << std::endl;
    std::cout << "Packets sent: " << g_txCount << std::endl;
    std::cout << "Packets received: " << g_rxCount << std::endl;
    std::cout << "Success rate: " << (g_rxCount * 100.0 / g_txCount) << "%" << std::endl;

    return 0;
}

