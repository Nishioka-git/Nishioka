#ifndef ROUTING_INFO_H
#define ROUTING_INFO_H

#include "ns3/lr-wpan-fields.h"
#include "ns3/packet.h"
#include <map>
#include <string>
#include <utility>

namespace ns3 {
namespace lrwpan {
class UartLrWpanNetDevice;
}
}

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

// Packet types
enum PacketType : uint8_t {
    PACKET_TYPE_DATA = 0x01,
    PACKET_TYPE_RREQ = 0x02,
    PACKET_TYPE_RREP = 0x03
};

// Routing entry structure
struct RoutingEntry {
    uint16_t entryId;
    Mac16Address dst;
    Mac16Address nextHop;
    uint8_t energy;
    uint8_t lqi;
    uint8_t hops;

    RoutingEntry()
        : entryId(0), dst(Mac16Address()), nextHop(Mac16Address()),
          energy(100), lqi(255), hops(0) {}

    RoutingEntry(uint16_t id, Mac16Address d, Mac16Address nh,
                 uint8_t e, uint8_t l, uint8_t h)
        : entryId(id), dst(d), nextHop(nh), energy(e), lqi(l), hops(h) {}
};

// RREQ packet structure
struct RREQPacket {
    uint8_t packetType;
    uint16_t rreqId;
    Mac16Address originator;
    Mac16Address dst;
    uint8_t hopCount;
    uint8_t energy;
    uint8_t lqi;

    RREQPacket()
        : packetType(PACKET_TYPE_RREQ), rreqId(0),
          originator(Mac16Address()), dst(Mac16Address()),
          hopCount(0), energy(100), lqi(255) {}
};

// RREP packet structure
struct RREPPacket {
    uint8_t packetType;
    uint16_t rreqId;
    Mac16Address originator;
    Mac16Address dst;
    uint8_t hopCount;
    uint8_t energy;
    uint8_t lqi;

    RREPPacket()
        : packetType(PACKET_TYPE_RREP), rreqId(0),
          originator(Mac16Address()), dst(Mac16Address()),
          hopCount(0), energy(100), lqi(255) {}
};

// Routing table management
extern std::map<Mac16Address, RoutingEntry> g_routingTableCoordinator;
extern std::map<Mac16Address, RoutingEntry> g_routingTableDev01;
extern std::map<Mac16Address, RoutingEntry> g_routingTableDev02;
extern std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsCoordinator;
extern std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsDev01;
extern std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsDev02;

// Routing information functions
std::map<Mac16Address, RoutingEntry>& GetRoutingTable(Ptr<UartLrWpanNetDevice> device);
std::map<std::pair<Mac16Address, uint16_t>, bool>& GetProcessedRREQs(Ptr<UartLrWpanNetDevice> device);
void UpdateRoutingEntry(std::map<Mac16Address, RoutingEntry>& routingTable,
                        Mac16Address dst, Mac16Address nextHop,
                        uint8_t energy, uint8_t lqi, uint8_t hops);
void PrintRoutingTable(const std::map<Mac16Address, RoutingEntry>& routingTable);

#endif // ROUTING_INFO_H
