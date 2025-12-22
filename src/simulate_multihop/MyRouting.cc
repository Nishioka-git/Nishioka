#include "MyRouting.h"
#include "RoutingInfo.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include <iostream>
#include <vector>
#include <cstring>

// External variables
extern uint16_t g_rreqIdCounter;
extern uint16_t g_totalDevicesInPAN;

// Battery level getter (will be implemented in main)
extern uint8_t GetBatteryLevel(Ptr<UartLrWpanNetDevice> device);

double CalculateRouteScore(uint8_t lqi, uint8_t energy, uint8_t hops) {
    const double LQI_WEIGHT = 0.5;
    const double ENERGY_WEIGHT = 0.3;
    const double HOPS_WEIGHT = 0.2;

    double lqiScore = lqi;
    double energyScore = energy * 2.55;
    double maxHops = g_totalDevicesInPAN > 0 ? g_totalDevicesInPAN : 1;
    double normalizedHops = hops < maxHops ? (maxHops - hops) / maxHops : 0.0;
    double hopsScore = normalizedHops * 255.0;

    double totalScore = (lqiScore * LQI_WEIGHT) +
                        (energyScore * ENERGY_WEIGHT) +
                        (hopsScore * HOPS_WEIGHT);

    std::cout << " [SCORE DETAIL] LQI:" << lqi << "(" << (lqiScore * LQI_WEIGHT) << ") "
              << "Energy:" << (int)energy << "%(" << (energyScore * ENERGY_WEIGHT) << ") "
              << "Hops:" << (int)hops << "(" << (hopsScore * HOPS_WEIGHT) << ") "
              << "Total:" << totalScore << std::endl;

    return totalScore;
}

bool ShouldUpdateRoute(const RoutingEntry& currentRoute, const RoutingEntry& newRoute) {
    double currentScore = CalculateRouteScore(currentRoute.lqi, currentRoute.energy, currentRoute.hops);
    double newScore = CalculateRouteScore(newRoute.lqi, newRoute.energy, newRoute.hops);

    std::cout << " [ROUTE EVALUATION] Current score: " << currentScore
              << " | New score: " << newScore << std::endl;

    return newScore > (currentScore * 1.1);
}

void LearnRoute(Ptr<UartLrWpanNetDevice> device,
                Mac16Address sourceDst,
                Mac16Address nextHop,
                uint8_t energy,
                uint8_t lqi,
                uint8_t hops) {
    std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
    auto it = myRoutingTable.find(sourceDst);

    if (it == myRoutingTable.end()) {
        std::cout << " [ROUTE LEARNING] New route discovered to " << sourceDst << std::endl;
        UpdateRoutingEntry(myRoutingTable, sourceDst, nextHop, energy, lqi, hops);
    } else {
        RoutingEntry currentRoute = it->second;
        RoutingEntry newRoute;
        newRoute.dst = sourceDst;
        newRoute.nextHop = nextHop;
        newRoute.energy = energy;
        newRoute.lqi = lqi;
        newRoute.hops = hops;

        if (ShouldUpdateRoute(currentRoute, newRoute)) {
            std::cout << " [ROUTE LEARNING] Better route found! Updating route to " << sourceDst << std::endl;
            UpdateRoutingEntry(myRoutingTable, sourceDst, nextHop, energy, lqi, hops);
        } else {
            std::cout << " [ROUTE LEARNING] Existing route is better. No update." << std::endl;
        }
    }
}

Ptr<Packet> CreateRREQPacket(const RREQPacket& rreq) {
    std::vector<uint8_t> packet;
    packet.resize(13);
    size_t offset = 0;

    packet[offset++] = rreq.packetType;
    packet[offset++] = (rreq.rreqId >> 8) & 0xFF;
    packet[offset++] = rreq.rreqId & 0xFF;

    uint8_t originatorBuf[2];
    rreq.originator.CopyTo(originatorBuf);
    packet[offset++] = originatorBuf[0];
    packet[offset++] = originatorBuf[1];

    uint8_t dstBuf[2];
    rreq.dst.CopyTo(dstBuf);
    packet[offset++] = dstBuf[0];
    packet[offset++] = dstBuf[1];

    packet[offset++] = rreq.hopCount;
    packet[offset++] = rreq.energy;
    packet[offset++] = rreq.lqi;

    return Create<Packet>(packet.data(), packet.size());
}

bool ParseRREQPacket(Ptr<Packet> p, RREQPacket& rreq) {
    std::vector<uint8_t> buffer;
    buffer.resize(p->GetSize());
    p->CopyData(buffer.data(), p->GetSize());

    if (buffer.size() < 10) return false;

    size_t offset = 0;
    rreq.packetType = buffer[offset++];
    if (rreq.packetType != PACKET_TYPE_RREQ) return false;

    rreq.rreqId = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
    offset += 2;

    uint8_t originatorBuf[2] = {buffer[offset], buffer[offset + 1]};
    rreq.originator.CopyFrom(originatorBuf);
    offset += 2;

    uint8_t dstBuf[2] = {buffer[offset], buffer[offset + 1]};
    rreq.dst.CopyFrom(dstBuf);
    offset += 2;

    rreq.hopCount = buffer[offset++];
    rreq.energy = buffer[offset++];
    rreq.lqi = buffer[offset++];

    return true;
}

Ptr<Packet> CreateRREPPacket(const RREPPacket& rrep) {
    std::vector<uint8_t> packet;
    packet.resize(13);
    size_t offset = 0;

    packet[offset++] = rrep.packetType;
    packet[offset++] = (rrep.rreqId >> 8) & 0xFF;
    packet[offset++] = rrep.rreqId & 0xFF;

    uint8_t originatorBuf[2];
    rrep.originator.CopyTo(originatorBuf);
    packet[offset++] = originatorBuf[0];
    packet[offset++] = originatorBuf[1];

    uint8_t dstBuf[2];
    rrep.dst.CopyTo(dstBuf);
    packet[offset++] = dstBuf[0];
    packet[offset++] = dstBuf[1];

    packet[offset++] = rrep.hopCount;
    packet[offset++] = rrep.energy;
    packet[offset++] = rrep.lqi;

    return Create<Packet>(packet.data(), packet.size());
}

bool ParseRREPPacket(Ptr<Packet> p, RREPPacket& rrep) {
    std::vector<uint8_t> buffer;
    buffer.resize(p->GetSize());
    p->CopyData(buffer.data(), p->GetSize());

    if (buffer.size() < 10) return false;

    size_t offset = 0;
    rrep.packetType = buffer[offset++];
    if (rrep.packetType != PACKET_TYPE_RREP) return false;

    rrep.rreqId = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
    offset += 2;

    uint8_t originatorBuf[2] = {buffer[offset], buffer[offset + 1]};
    rrep.originator.CopyFrom(originatorBuf);
    offset += 2;

    uint8_t dstBuf[2] = {buffer[offset], buffer[offset + 1]};
    rrep.dst.CopyFrom(dstBuf);
    offset += 2;

    rrep.hopCount = buffer[offset++];
    rrep.energy = buffer[offset++];
    rrep.lqi = buffer[offset++];

    return true;
}

Ptr<Packet> CreatePacketWithRoutingInfo(const RoutingEntry& entry, const std::string& data) {
    std::vector<uint8_t> packet;
    packet.resize(8 + data.size());
    size_t offset = 0;

    packet[offset++] = PACKET_TYPE_DATA;
    packet[offset++] = (entry.entryId >> 8) & 0xFF;
    packet[offset++] = entry.entryId & 0xFF;

    uint8_t dstAddrBuf[2];
    entry.dst.CopyTo(dstAddrBuf);
    packet[offset++] = dstAddrBuf[0];
    packet[offset++] = dstAddrBuf[1];

    packet[offset++] = entry.energy;
    packet[offset++] = entry.lqi;
    packet[offset++] = entry.hops;

    memcpy(packet.data() + offset, data.c_str(), data.size());
    return Create<Packet>(packet.data(), packet.size());
}

bool ExtractRoutingInfo(Ptr<Packet> p, RoutingEntry& entry, std::string& data) {
    std::vector<uint8_t> buffer;
    buffer.resize(p->GetSize());
    p->CopyData(buffer.data(), p->GetSize());

    if (buffer.size() < 8) return false;

    size_t offset = 0;
    uint8_t packetType = buffer[offset++];
    if (packetType != PACKET_TYPE_DATA) return false;

    entry.entryId = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
    offset += 2;

    uint8_t dstAddrBuf[2] = {buffer[offset], buffer[offset + 1]};
    entry.dst.CopyFrom(dstAddrBuf);
    offset += 2;

    entry.energy = buffer[offset++];
    entry.lqi = buffer[offset++];
    entry.hops = buffer[offset++];

    data = std::string(buffer.begin() + offset, buffer.end());
    return true;
}

PacketType GetPacketType(Ptr<Packet> p) {
    std::vector<uint8_t> buffer;
    buffer.resize(p->GetSize());
    p->CopyData(buffer.data(), p->GetSize());

    if (buffer.size() < 1) return PACKET_TYPE_DATA;
    return static_cast<PacketType>(buffer[0]);
}

// Helper to get device address (declared in main file)
extern Mac16Address g_coordinatorAddr;
extern Mac16Address g_dev01Addr;
extern Mac16Address g_dev02Addr;

Mac16Address GetDeviceAddress(Ptr<UartLrWpanNetDevice> device) {
    extern Ptr<UartLrWpanNetDevice> g_coordinatorDevice;
    extern Ptr<UartLrWpanNetDevice> g_uartNetDevice1;
    extern Ptr<UartLrWpanNetDevice> g_uartNetDevice2;

    if (device == g_coordinatorDevice) {
        return g_coordinatorAddr;
    } else if (device == g_uartNetDevice1) {
        return g_dev01Addr;
    } else if (device == g_uartNetDevice2) {
        return g_dev02Addr;
    }
    return g_coordinatorAddr;
}

void SendRREQ(Ptr<UartLrWpanNetDevice> device, Mac16Address dst) {
    Mac16Address myAddress = GetDeviceAddress(device);

    RREQPacket rreq;
    rreq.rreqId = g_rreqIdCounter++;
    rreq.originator = myAddress;
    rreq.dst = dst;
    rreq.hopCount = 0;
    rreq.energy = GetBatteryLevel(device);
    rreq.lqi = 255;

    std::map<std::pair<Mac16Address, uint16_t>, bool>& processedRREQs = GetProcessedRREQs(device);
    processedRREQs[std::make_pair(rreq.originator, rreq.rreqId)] = true;

    Ptr<Packet> packet = CreateRREQPacket(rreq);

    McpsDataRequestParams params;
    params.m_dstPanId = 0xCAFE;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_dstAddr = Mac16Address("ff:ff");
    params.m_msduHandle = 1;
    params.m_txOptions = 0;
    params.m_srcAddrMode = SHORT_ADDR;

    extern uint32_t g_txCount;
    g_txCount++;
    std::cout << Simulator::Now().As(Time::S)
              << " [RREQ SEND] Node " << device->GetNode()->GetId()
              << " (Addr: " << myAddress << ") -> BROADCAST\n"
              << "  RREQ ID: " << rreq.rreqId
              << " | Originator: " << rreq.originator
              << " | Dst: " << rreq.dst
              << " | Energy: " << (int)rreq.energy << "%"
              << " | Total sent packets: " << g_txCount << std::endl;

    device->GetMac()->McpsDataRequest(params, packet);
}

void HandleRREQ(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, const RREQPacket& rreq) {
    Mac16Address myAddress = GetDeviceAddress(device);

    std::cout << Simulator::Now().As(Time::S)
              << " [RREQ RECEIVE] Node " << device->GetNode()->GetId()
              << " (Addr: " << myAddress << ") <- from " << params.m_srcAddr << "\n"
              << "  RREQ ID: " << rreq.rreqId
              << " | Originator: " << rreq.originator
              << " | Dst: " << rreq.dst
              << " | HopCount: " << (int)rreq.hopCount << std::endl;

    if (rreq.originator == myAddress) {
        std::cout << "  [INFO] Ignoring my own RREQ" << std::endl;
        return;
    }

    std::map<std::pair<Mac16Address, uint16_t>, bool>& processedRREQs = GetProcessedRREQs(device);
    auto rreqKey = std::make_pair(rreq.originator, rreq.rreqId);
    if (processedRREQs.find(rreqKey) != processedRREQs.end()) {
        std::cout << "  [INFO] Already processed this RREQ" << std::endl;
        return;
    }
    processedRREQs[rreqKey] = true;

    LearnRoute(device, rreq.originator, params.m_srcAddr,
               rreq.energy, params.m_mpduLinkQuality, rreq.hopCount + 1);

    if (rreq.dst == myAddress) {
        std::cout << "  [INFO] I am the destination! Sending RREP" << std::endl;

        RREPPacket rrep;
        rrep.rreqId = rreq.rreqId;
        rrep.originator = rreq.originator;
        rrep.dst = myAddress;
        rrep.hopCount = 0;
        rrep.energy = GetBatteryLevel(device);
        rrep.lqi = params.m_mpduLinkQuality;

        Ptr<Packet> packet = CreateRREPPacket(rrep);

        McpsDataRequestParams replyParams;
        replyParams.m_dstPanId = 0xCAFE;
        replyParams.m_dstAddrMode = SHORT_ADDR;
        replyParams.m_dstAddr = params.m_srcAddr;
        replyParams.m_msduHandle = 2;
        replyParams.m_txOptions = 0;
        replyParams.m_srcAddrMode = SHORT_ADDR;

        extern uint32_t g_txCount;
        g_txCount++;
        std::cout << Simulator::Now().As(Time::S)
                  << " [RREP SEND] Node " << device->GetNode()->GetId()
                  << " -> Node " << replyParams.m_dstAddr
                  << " | RREP ID: " << rrep.rreqId
                  << " | Total sent packets: " << g_txCount << std::endl;

        device->GetMac()->McpsDataRequest(replyParams, packet);
    } else {
        std::cout << "  [INFO] Forwarding RREQ" << std::endl;

        RREQPacket forwardRreq = rreq;
        forwardRreq.hopCount++;
        forwardRreq.energy = GetBatteryLevel(device);
        forwardRreq.lqi = params.m_mpduLinkQuality;

        Ptr<Packet> packet = CreateRREQPacket(forwardRreq);

        McpsDataRequestParams forwardParams;
        forwardParams.m_dstPanId = 0xCAFE;
        forwardParams.m_dstAddrMode = SHORT_ADDR;
        forwardParams.m_dstAddr = Mac16Address("ff:ff");
        forwardParams.m_msduHandle = 1;
        forwardParams.m_txOptions = 0;
        forwardParams.m_srcAddrMode = SHORT_ADDR;

        extern uint32_t g_txCount;
        g_txCount++;
        device->GetMac()->McpsDataRequest(forwardParams, packet);
    }
}

void HandleRREP(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, const RREPPacket& rrep) {
    Mac16Address myAddress = GetDeviceAddress(device);

    std::cout << Simulator::Now().As(Time::S)
              << " [RREP RECEIVE] Node " << device->GetNode()->GetId()
              << " (Addr: " << myAddress << ") <- from " << params.m_srcAddr << "\n"
              << "  RREP ID: " << rrep.rreqId
              << " | Originator: " << rrep.originator
              << " | Dst: " << rrep.dst
              << " | HopCount: " << (int)rrep.hopCount << std::endl;

    LearnRoute(device, rrep.dst, params.m_srcAddr,
               rrep.energy, params.m_mpduLinkQuality, rrep.hopCount + 1);

    if (rrep.originator == myAddress) {
        std::cout << "  [INFO] Route established to " << rrep.dst << "!" << std::endl;
        return;
    }

    std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
    auto it = myRoutingTable.find(rrep.originator);
    if (it != myRoutingTable.end()) {
        std::cout << "  [INFO] Forwarding RREP to originator" << std::endl;

        RREPPacket forwardRrep = rrep;
        forwardRrep.hopCount++;
        forwardRrep.energy = GetBatteryLevel(device);
        forwardRrep.lqi = params.m_mpduLinkQuality;

        Ptr<Packet> packet = CreateRREPPacket(forwardRrep);

        McpsDataRequestParams forwardParams;
        forwardParams.m_dstPanId = 0xCAFE;
        forwardParams.m_dstAddrMode = SHORT_ADDR;
        forwardParams.m_dstAddr = it->second.nextHop;
        forwardParams.m_msduHandle = 2;
        forwardParams.m_txOptions = 0;
        forwardParams.m_srcAddrMode = SHORT_ADDR;

        extern uint32_t g_txCount;
        g_txCount++;
        device->GetMac()->McpsDataRequest(forwardParams, packet);
    } else {
        std::cout << "  [ERROR] No route back to originator " << rrep.originator << std::endl;
    }
}
