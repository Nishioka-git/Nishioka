/*
* Copyright (c) 2024 Tokushima University, Japan.
*
* SPDX-License-Identifier: GPL-2.0-only
*
* Author:
* Nishioka,Yugo
*/

#include "RoutingInfo.h"
#include "MyRouting.h"
#include "Association.h"
#include "BatteryManager.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include "ns3/mobility-model.h"
#include "ns3/constant-position-mobility-model.h"

#include <iostream>
#include <map>
#include <cstring>
#include <vector>

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

// Global variables
uint32_t g_txCount = 0;
uint32_t g_rxCount = 0;
uint16_t g_nextShortAddr = 0x0002;
uint16_t g_nextShortAddrDev01 = 0x0003;
Mac16Address g_dev01ShortAddr = Mac16Address("00:02");

Ptr<UartLrWpanNetDevice> g_coordinatorDevice;
Ptr<UartLrWpanNetDevice> g_uartNetDevice1;
Ptr<UartLrWpanNetDevice> g_uartNetDevice2;

Mac16Address g_coordinatorAddr = Mac16Address("00:01");
Mac16Address g_dev01Addr = Mac16Address("FF:FE");
Mac16Address g_dev02Addr = Mac16Address("FF:FD");

uint8_t g_batteryLevelCoordinator = 100;
uint8_t g_batteryLevelDev01 = 100;
uint8_t g_batteryLevelDev02 = 100;

uint16_t g_rreqIdCounter = 0;
uint16_t g_totalDevicesInPAN = 3;
uint16_t g_associatedDeviceCount = 0;

// Data confirm callback
static void
DataConfirm(Ptr<UartLrWpanNetDevice> device, McpsDataConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [SEND CONFIRM] Node " << device->GetNode()->GetId()
              << ", Data confirm | Status :" << static_cast<uint32_t>(params.m_status)
              << " | Msdu handle " << static_cast<uint32_t>(params.m_msduHandle)
              << " | Total sent packets: " << g_txCount << "\n";
}

// Data indication callback
static void
DataIndication(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p)
{
    g_rxCount++;

    PacketType pktType = GetPacketType(p);

    if (pktType == PACKET_TYPE_RREQ) {
        RREQPacket rreq;
        if (ParseRREQPacket(p, rreq)) {
            HandleRREQ(device, params, rreq);
        }
        return;
    }

    if (pktType == PACKET_TYPE_RREP) {
        RREPPacket rrep;
        if (ParseRREPPacket(p, rrep)) {
            HandleRREP(device, params, rrep);
        }
        return;
    }

    // Data packet
    RoutingEntry receivedEntry;
    std::string data;
    if (ExtractRoutingInfo(p, receivedEntry, data)) {
        std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
                  << " <- from Node " << params.m_srcAddr << "\n"
                  << " Routing Info - EntryID: " << receivedEntry.entryId
                  << " | Dst: " << receivedEntry.dst
                  << " | Energy: " << (int)receivedEntry.energy << "%"
                  << " | LQI: " << (int)receivedEntry.lqi
                  << " | Hops: " << (int)receivedEntry.hops << "\n"
                  << " Data: " << data
                  << " | Total received packets: " << g_rxCount << "\n";
    } else {
        std::vector<uint8_t> buffer;
        buffer.resize(p->GetSize());
        p->CopyData(buffer.data(), p->GetSize());
        data = std::string(buffer.begin(), buffer.end());
        std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
                  << " <- from Node " << params.m_srcAddr
                  << " | Data: " << data
                  << " | Total received packets: " << g_rxCount << "\n";
    }
}

// Relay and indicate callback
static void
RelayAndIndicate(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p)
{
    g_rxCount++;

    PacketType pktType = GetPacketType(p);

    if (pktType == PACKET_TYPE_RREQ) {
        RREQPacket rreq;
        if (ParseRREQPacket(p, rreq)) {
            HandleRREQ(device, params, rreq);
        }
        return;
    }

    if (pktType == PACKET_TYPE_RREP) {
        RREPPacket rrep;
        if (ParseRREPPacket(p, rrep)) {
            HandleRREP(device, params, rrep);
        }
        return;
    }

    RoutingEntry receivedEntry;
    std::string data;
    if (!ExtractRoutingInfo(p, receivedEntry, data)) {
        std::cout << "[ERROR] Failed to extract routing info from packet\n";
        return;
    }

    Mac16Address myAddress = GetDeviceAddress(device);

    std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
              << " (Addr: " << myAddress << ") <- from Node " << params.m_srcAddr << "\n"
              << " Routing Info - EntryID: " << receivedEntry.entryId
              << " | Dst: " << receivedEntry.dst
              << " | Energy: " << (int)receivedEntry.energy << "%"
              << " | LQI: " << (int)receivedEntry.lqi
              << " | Hops: " << (int)receivedEntry.hops << "\n"
              << " Data: " << data
              << " | Total received packets: " << g_rxCount << "\n";

    if (params.m_srcAddr != myAddress) {
        std::cout << " [INFO] Learning reverse route to sender " << params.m_srcAddr << std::endl;
        LearnRoute(device,
                   params.m_srcAddr,
                   params.m_srcAddr,
                   receivedEntry.energy,
                   params.m_mpduLinkQuality,
                   1);
    }

    if (receivedEntry.dst == myAddress) {
        std::cout << " [INFO] I am the final destination. Packet delivered.\n";
        return;
    }

    std::cout << " [INFO] Not for me (dst=" << receivedEntry.dst << "). Looking for route...\n";
    std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
    auto it = myRoutingTable.find(receivedEntry.dst);
    if (it != myRoutingTable.end()) {
        RoutingEntry& routeEntry = it->second;
        if (routeEntry.nextHop == params.m_srcAddr) {
            std::cout << " [ROUTING ERROR] NextHop(" << routeEntry.nextHop
                      << ") is same as sender(" << params.m_srcAddr
                      << "). Preventing reverse routing!\n";
            return;
        }

        receivedEntry.hops++;
        receivedEntry.lqi = params.m_mpduLinkQuality;
        receivedEntry.energy = GetBatteryLevel(device);

        if (routeEntry.dst == routeEntry.nextHop) {
            std::cout << " [ROUTING] Dst matches NextHop - Final hop to destination!\n";
        } else {
            std::cout << " [ROUTING] Intermediate hop - NextHop: " << routeEntry.nextHop << "\n";
        }

        std::cout << " [ROUTING] Route found - NextHop: " << routeEntry.nextHop
                  << " | Updated Hops: " << (int)receivedEntry.hops
                  << " | Updated LQI: " << (int)receivedEntry.lqi
                  << " | Battery: " << (int)receivedEntry.energy << "%" << std::endl;

        Ptr<Packet> forwardPacket = CreatePacketWithRoutingInfo(receivedEntry, data);

        McpsDataRequestParams relayParams;
        relayParams.m_dstPanId = 0xCAFE;
        relayParams.m_dstAddrMode = SHORT_ADDR;
        relayParams.m_dstAddr = routeEntry.nextHop;
        relayParams.m_msduHandle = 2;
        relayParams.m_txOptions = 0;
        relayParams.m_srcAddrMode = SHORT_ADDR;

        g_txCount++;
        std::cout << Simulator::Now().As(Time::S)
                  << " [RELAY/SEND] Node " << device->GetNode()->GetId()
                  << " -> Node " << relayParams.m_dstAddr
                  << " | Data: " << data
                  << " | Total sent packets: " << g_txCount << std::endl;
        device->GetMac()->McpsDataRequest(relayParams, forwardPacket);
    } else {
        std::cout << " [ROUTING ERROR] No route found for destination: "
                  << receivedEntry.dst << std::endl;
    }
}

int
main(int argc, char* argv[])
{
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    std::cout << "\n========== Network Configuration ==========\n";
    std::cout << "Total devices in PAN: " << g_totalDevicesInPAN << std::endl;
    std::cout << " - Coordinator: 1\n";
    std::cout << " - End Devices: " << (g_totalDevicesInPAN - 1) << std::endl;
    std::cout << "==========================================\n\n";

    // Coordinator
    Ptr<Node> node = CreateObject<Node>();
    g_coordinatorDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
    node->AddDevice(g_coordinatorDevice);
    Ptr<ConstantPositionMobilityModel> mobility0 = CreateObject<ConstantPositionMobilityModel>();
    mobility0->SetPosition(Vector(0, 0, 0));
    node->AggregateObject(mobility0);

    // End Device 1 (dev01)
    Ptr<Node> node2 = CreateObject<Node>();
    g_uartNetDevice1 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB1");
    node2->AddDevice(g_uartNetDevice1);
    Ptr<ConstantPositionMobilityModel> mobility1 = CreateObject<ConstantPositionMobilityModel>();
    mobility1->SetPosition(Vector(0, 90, 0));
    node2->AggregateObject(mobility1);

    // End Device 2 (dev02)
    Ptr<Node> node3 = CreateObject<Node>();
    g_uartNetDevice2 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB2");
    node3->AddDevice(g_uartNetDevice2);
    Ptr<ConstantPositionMobilityModel> mobility2 = CreateObject<ConstantPositionMobilityModel>();
    mobility2->SetPosition(Vector(0, 180, 0));
    node3->AggregateObject(mobility2);

    // Callback setup
    g_coordinatorDevice->GetMac()->SetMcpsDataConfirmCallback(
        MakeBoundCallback(&DataConfirm, g_coordinatorDevice));
    g_coordinatorDevice->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&DataIndication, g_coordinatorDevice));
    g_coordinatorDevice->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&AssociateIndication, g_coordinatorDevice));

    g_uartNetDevice1->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&RelayAndIndicate, g_uartNetDevice1));
    g_uartNetDevice1->GetMac()->SetMlmeAssociateConfirmCallback(
        MakeBoundCallback(&AssociateConfirm, g_uartNetDevice1));
    g_uartNetDevice1->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&AssociateIndicationDev01, g_uartNetDevice1));

    g_uartNetDevice2->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&DataIndication, g_uartNetDevice2));
    g_uartNetDevice2->GetMac()->SetMlmeAssociateConfirmCallback(
        MakeBoundCallback(&AssociateConfirm, g_uartNetDevice2));

    // Channel and address setup
    Ptr<MacPibAttributes> pibAttr0 = Create<MacPibAttributes>();
    pibAttr0->pCurrentChannel = 0xD;
    g_coordinatorDevice->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttr0);
    g_uartNetDevice1->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttr0);
    g_uartNetDevice2->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttr0);

    Ptr<MacPibAttributes> pibAttr1 = Create<MacPibAttributes>();
    pibAttr1->macShortAddress = Mac16Address("00:01");
    g_coordinatorDevice->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttr1);

    Ptr<MacPibAttributes> pibAttr2 = Create<MacPibAttributes>();
    pibAttr2->macShortAddress = Mac16Address("FF:FE");
    g_uartNetDevice1->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttr2);

    Ptr<MacPibAttributes> pibAttr3 = Create<MacPibAttributes>();
    pibAttr3->macShortAddress = Mac16Address("FF:FD");
    g_uartNetDevice2->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttr3);

    Ptr<MacPibAttributes> pibAttrPan1 = Create<MacPibAttributes>();
    pibAttrPan1->macPanId = 0xCAFE;
    g_uartNetDevice1->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, pibAttrPan1);

    Ptr<MacPibAttributes> pibAttrPan2 = Create<MacPibAttributes>();
    pibAttrPan2->macPanId = 0xCAFE;
    g_uartNetDevice2->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, pibAttrPan2);

    // Start coordinator
    MlmeStartRequestParams startParams;
    startParams.m_PanId = 0xCAFE;
    startParams.m_logCh = 0xD;
    startParams.m_logChPage = 0;
    startParams.m_bcnOrd = 15;
    startParams.m_sfrmOrd = 15;
    startParams.m_panCoor = true;
    startParams.m_battLifeExt = false;
    startParams.m_coorRealgn = false;
    g_coordinatorDevice->GetMac()->MlmeStartRequest(startParams);

    std::cout << "\n========== Initial Battery Level Reading ==========\n";
    UpdateBatteryLevels();

    std::cout << "\n========== AODV-style Route Discovery Enabled ==========\n";
    std::cout << "Routing tables will be populated dynamically using RREQ/RREP\n";
    std::cout << "========================================================\n\n";

    // Schedule dev01 association
    Simulator::Schedule(Seconds(0.5), [=]() {
        MlmeAssociateRequestParams associateParams;
        associateParams.m_chNum = 0xD;
        associateParams.m_chPage = 0;
        associateParams.m_coordAddrMode = SHORT_ADDR;
        associateParams.m_coordPanId = 0xCAFE;
        associateParams.m_capabilityInfo = 0x80;
        associateParams.m_coordShortAddr = Mac16Address("00:01");
        g_uartNetDevice1->GetMac()->MlmeAssociateRequest(associateParams);
    });

    // Schedule route discovery after dev02 association
    Simulator::Schedule(Seconds(1.7), [=]() {
        Mac16Address targetAddr = Mac16Address("00:03");
        std::cout << "Coordinator initiating route discovery to dev02\n";
        SendRREQ(g_coordinatorDevice, targetAddr);

        Simulator::Schedule(Seconds(1.0), [=]() {
            auto it = g_routingTableCoordinator.find(Mac16Address("00:03"));
            if (it != g_routingTableCoordinator.end()) {
                RoutingEntry& entry = it->second;
                entry.energy = GetBatteryLevel(g_coordinatorDevice);
                std::string sendMsg = "Hello from Coordinator to dev02";
                Ptr<Packet> packet = CreatePacketWithRoutingInfo(entry, sendMsg);

                McpsDataRequestParams dataParams;
                dataParams.m_dstPanId = 0xCAFE;
                dataParams.m_dstAddrMode = SHORT_ADDR;
                dataParams.m_dstAddr = entry.nextHop;
                dataParams.m_msduHandle = 3;
                dataParams.m_txOptions = 0;
                dataParams.m_srcAddrMode = SHORT_ADDR;

                g_txCount++;
                std::cout << Simulator::Now().As(Time::S)
                          << " [SEND DATA] Coordinator (Node 0)"
                          << " -> Node " << dataParams.m_dstAddr << " (NextHop from routing table)\n"
                          << " Routing Info - EntryID: " << entry.entryId
                          << " | Dst: " << entry.dst
                          << " | Energy: " << (int)entry.energy << "%"
                          << " | LQI: " << (int)entry.lqi
                          << " | Hops: " << (int)entry.hops << "\n"
                          << " Data: " << sendMsg
                          << " | Total sent packets: " << g_txCount << std::endl;
                g_coordinatorDevice->GetMac()->McpsDataRequest(dataParams, packet);
            } else {
                std::cout << "  [ERROR] No route to dev02 found after RREQ!" << std::endl;
            }
        });
    });

    // Print routing tables before end
    Simulator::Schedule(Seconds(4.9), []() {
        std::cout << "\n========== Final Routing Tables (After RREQ/RREP) ==========\n";
        std::cout << "Coordinator:\n";
        PrintRoutingTable(g_routingTableCoordinator);
        std::cout << "Dev01:\n";
        PrintRoutingTable(g_routingTableDev01);
        std::cout << "Dev02:\n";
        PrintRoutingTable(g_routingTableDev02);
    });

    Simulator::Stop(Seconds(5));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
