#ifndef MY_ROUTING_H
#define MY_ROUTING_H

#include "RoutingInfo.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include "ns3/lr-wpan-fields.h"

namespace ns3 {
namespace lrwpan {
class UartLrWpanNetDevice;
}
}

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

// Route calculation and decision functions
double CalculateRouteScore(uint8_t lqi, uint8_t energy, uint8_t hops);
bool ShouldUpdateRoute(const RoutingEntry& currentRoute, const RoutingEntry& newRoute);
void LearnRoute(Ptr<UartLrWpanNetDevice> device,
                Mac16Address sourceDst,
                Mac16Address nextHop,
                uint8_t energy,
                uint8_t lqi,
                uint8_t hops);

// Packet creation and parsing
Ptr<Packet> CreateRREQPacket(const RREQPacket& rreq);
bool ParseRREQPacket(Ptr<Packet> p, RREQPacket& rreq);
Ptr<Packet> CreateRREPPacket(const RREPPacket& rrep);
bool ParseRREPPacket(Ptr<Packet> p, RREPPacket& rrep);
Ptr<Packet> CreatePacketWithRoutingInfo(const RoutingEntry& entry, const std::string& data);
bool ExtractRoutingInfo(Ptr<Packet> p, RoutingEntry& entry, std::string& data);

// Route discovery
void SendRREQ(Ptr<UartLrWpanNetDevice> device, Mac16Address dst);
void HandleRREQ(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, const RREQPacket& rreq);
void HandleRREP(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, const RREPPacket& rrep);

// Helper functions
PacketType GetPacketType(Ptr<Packet> p);
Mac16Address GetDeviceAddress(Ptr<UartLrWpanNetDevice> device);

#endif // MY_ROUTING_H
