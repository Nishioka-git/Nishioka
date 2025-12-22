#include "RoutingInfo.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include <iostream>

// External device pointers
extern Ptr<UartLrWpanNetDevice> g_coordinatorDevice;
extern Ptr<UartLrWpanNetDevice> g_uartNetDevice1;
extern Ptr<UartLrWpanNetDevice> g_uartNetDevice2;

// Global routing tables
std::map<Mac16Address, RoutingEntry> g_routingTableCoordinator;
std::map<Mac16Address, RoutingEntry> g_routingTableDev01;
std::map<Mac16Address, RoutingEntry> g_routingTableDev02;

// Processed RREQ tracking
std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsCoordinator;
std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsDev01;
std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsDev02;

static uint16_t g_routingSeqNum = 0;

std::map<Mac16Address, RoutingEntry>& GetRoutingTable(Ptr<UartLrWpanNetDevice> device) {
    if (device == g_coordinatorDevice) {
        return g_routingTableCoordinator;
    } else if (device == g_uartNetDevice1) {
        return g_routingTableDev01;
    } else if (device == g_uartNetDevice2) {
        return g_routingTableDev02;
    }
    return g_routingTableCoordinator;
}

std::map<std::pair<Mac16Address, uint16_t>, bool>& GetProcessedRREQs(Ptr<UartLrWpanNetDevice> device) {
    if (device == g_coordinatorDevice) {
        return g_processedRREQsCoordinator;
    } else if (device == g_uartNetDevice1) {
        return g_processedRREQsDev01;
    } else if (device == g_uartNetDevice2) {
        return g_processedRREQsDev02;
    }
    return g_processedRREQsCoordinator;
}

void UpdateRoutingEntry(std::map<Mac16Address, RoutingEntry>& routingTable,
                        Mac16Address dst, Mac16Address nextHop,
                        uint8_t energy, uint8_t lqi, uint8_t hops) {
    RoutingEntry entry;
    entry.entryId = g_routingSeqNum++;
    entry.dst = dst;
    entry.nextHop = nextHop;
    entry.energy = energy;
    entry.lqi = lqi;
    entry.hops = hops;
    routingTable[dst] = entry;

    std::cout << "[ROUTING TABLE UPDATE] "
              << "EntryID: " << entry.entryId
              << " | Dst: " << dst
              << " | NextHop: " << nextHop
              << " | Energy: " << (int)energy
              << " | LQI: " << (int)lqi
              << " | Hops: " << (int)hops << std::endl;
}

void PrintRoutingTable(const std::map<Mac16Address, RoutingEntry>& routingTable) {
    std::cout << "\n========== ROUTING TABLE ==========\n";
    for (const auto& pair : routingTable) {
        const RoutingEntry& entry = pair.second;
        std::cout << "EntryID: " << entry.entryId
                  << " | Dst: " << entry.dst
                  << " | NextHop: " << entry.nextHop
                  << " | Energy: " << (int)entry.energy << "%"
                  << " | LQI: " << (int)entry.lqi
                  << " | Hops: " << (int)entry.hops << std::endl;
    }
    std::cout << "===================================\n\n";
}
