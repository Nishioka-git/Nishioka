/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_HELPER_H
#define NISHIOKA_HELPER_H

#include "ns3/nishioka-header.h"
#include "ns3/nishioka-stack-container.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/packet.h"
#include "ns3/mac16-address.h"
#include "ns3/mac64-address.h"
#include "ns3/vector.h"

namespace ns3
{

/**
 * @ingroup nishioka
 *
 * @brief Helper for NishiokaHeader packets and NishiokaStack installation.
 *
 * Node layout (mirrors ZigbeeHelper + LrWpanHelper):
 * - LrWpanHelper installs LrWpanNetDevice (PHY+MAC) on each Node.
 * - NishiokaHelper::Install attaches NishiokaStack on the same Node and binds
 *   it to that NetDevice so MAC and NishiokaNwk communicate.
 *
 * This helper also provides:
 * - Creating packets with NishiokaHeader
 * - Extracting NishiokaHeader from packets
 * - Setting routing information (battery, LQI, hops) in headers
 * - ConfigureMac() for common MAC PIB settings on installed stacks
 */
class NishiokaHelper
{
  public:
    /**
     * Construct a NishiokaHelper
     */
    NishiokaHelper();

    /**
     * Create a packet with NishiokaHeader containing routing information
     *
     * @param data The payload data as a string
     * @param srcAddr Source MAC address (16-bit)
     * @param dstAddr Destination MAC address (16-bit)
     * @param panId PAN ID (default: 0xCAFE)
     * @param battery Battery level (0-100%)
     * @param lqi Link Quality Indicator (0-255)
     * @param hops Number of hops (default: 0)
     * @param seqNum Sequence number (optional, auto-incremented if not provided)
     * @return Packet with NishiokaHeader attached
     */
    Ptr<Packet> CreatePacket(const std::string& data,
                             Mac16Address srcAddr,
                             Mac16Address dstAddr,
                             uint16_t panId = 0xCAFE,
                             uint8_t battery = 100,
                             uint8_t lqi = 255,
                             uint8_t hops = 0,
                             uint8_t seqNum = 0);

    /**
     * Create a packet with NishiokaHeader using extended addresses
     *
     * @param data The payload data as a string
     * @param srcAddr Source MAC address (64-bit)
     * @param dstAddr Destination MAC address (64-bit)
     * @param panId PAN ID (default: 0xCAFE)
     * @param battery Battery level (0-100%)
     * @param lqi Link Quality Indicator (0-255)
     * @param hops Number of hops (default: 0)
     * @param seqNum Sequence number (optional, auto-incremented if not provided)
     * @return Packet with NishiokaHeader attached
     */
    Ptr<Packet> CreatePacket(const std::string& data,
                             Mac64Address srcAddr,
                             Mac64Address dstAddr,
                             uint16_t panId = 0xCAFE,
                             uint8_t battery = 100,
                             uint8_t lqi = 255,
                             uint8_t hops = 0,
                             uint8_t seqNum = 0);

    /**
     * Extract NishiokaHeader from a packet
     *
     * Address modes are inferred by deserializing candidate (dst, src) mode pairs and
     * accepting the first pair whose round-trip serialization matches the wire bytes.
     *
     * @param packet The packet containing NishiokaHeader
     * @param header Reference to store the extracted header
     * @param data Reference to store the extracted payload data
     * @return true if extraction was successful, false otherwise
     */
    bool ExtractHeader(Ptr<Packet> packet, nishioka::NishiokaHeader& header, std::string& data);

    /**
     * Extract routing information from NishiokaHeader
     *
     * @param header The NishiokaHeader to extract information from
     * @param battery Reference to store battery level
     * @param lqi Reference to store LQI
     * @param hops Reference to store hop count
     */
    void ExtractRoutingInfo(const nishioka::NishiokaHeader& header,
                            uint8_t& battery,
                            uint8_t& lqi,
                            uint8_t& hops);

    /**
     * Update routing information in an existing packet
     * Creates a new packet with updated header information. Preserves destination
     * addressing, frame type, and sequence number from the original header.
     *
     * @param packet Original packet
     * @param newSrcAddr New source address (short)
     * @param newBattery New battery level
     * @param newLqi New LQI
     * @param newHops New hop count
     * @return New packet with updated header
     */
    Ptr<Packet> UpdateRoutingInfo(Ptr<Packet> packet,
                                   Mac16Address newSrcAddr,
                                   uint8_t newBattery,
                                   uint8_t newLqi,
                                   uint8_t newHops);

    /**
     * Same as UpdateRoutingInfo(Mac16Address, ...) but sets a 64-bit source address.
     *
     * @param packet Original packet
     * @param newSrcAddr New source address (extended)
     * @param newBattery New battery level
     * @param newLqi New LQI
     * @param newHops New hop count
     * @return New packet with updated header
     */
    Ptr<Packet> UpdateRoutingInfo(Ptr<Packet> packet,
                                   Mac64Address newSrcAddr,
                                   uint8_t newBattery,
                                   uint8_t newLqi,
                                   uint8_t newHops);

    /**
     * Peek at the internal sequence counter without advancing it.
     * The next CreatePacket call that relies on automatic numbering uses this value;
     * see ResetSeqNum() to clear the counter.
     *
     * @return Current sequence counter (does not increment)
     */
    uint8_t GetNextSeqNum();

    /**
     * Reset sequence number counter
     */
    void ResetSeqNum();

    /**
     * Install NishiokaStack on top of existing LrWpan NetDevices.
     *
     * Each NetDevice must already be attached to a Node (for example by
     * LrWpanHelper::Install or UartLrWpanHelper::Install). The helper creates
     * one NishiokaStack per device, aggregates it on the same Node, and connects
     * the stack to the device's MAC (IEEE 802.15.4) for Nishioka NWK processing.
     *
     * Typical usage:
     * @code
     *   NodeContainer nodes;
     *   nodes.Create (n);
     *   LrWpanHelper lrWpanHelper;
     *   NetDeviceContainer devices = lrWpanHelper.Install (nodes);
     *   NishiokaHelper nishiokaHelper;
     *   NishiokaStackContainer stacks = nishiokaHelper.Install (devices);
     * @endcode
     *
     * @param netDevices Container of NetDevices (LrWpanNetDevice or UartLrWpanNetDevice)
     * @return Container with the newly created NishiokaStacks
     */
    nishioka::NishiokaStackContainer Install(NetDeviceContainer netDevices);

    /**
     * Install NishiokaStack and attach a ConstantPositionMobilityModel to each node.
     *
     * Same as Install(NetDeviceContainer) but also sets node positions. PHY mobility
     * for simulated LrWpanNetDevice should still be configured separately (e.g. via
     * LrWpanHelper or dev->GetPhy()->SetMobility).
     *
     * @param netDevices Container of NetDevices already attached to nodes
     * @param positions Position for each device/node (size must equal device count)
     * @return Container with the newly created NishiokaStacks
     */
    nishioka::NishiokaStackContainer Install(NetDeviceContainer netDevices,
                                              const std::vector<Vector>& positions);

    /**
     * Set an attribute on each NishiokaStack created by Install.
     *
     * @param n1 The name of the attribute to set.
     * @param v1 The value of the attribute to set.
     */
    void SetStackAttribute(std::string n1, const AttributeValue& v1);

    /**
     * Configure MAC layer settings for all stacks in the container.
     * This method sets common MAC parameters like channel, PAN ID, and addresses.
     *
     * @param stacks Container of NishiokaStacks to configure
     * @param channel Channel number (default: 0xD)
     * @param panId PAN ID (default: 0xCAFE)
     * @param addresses Vector of MAC addresses for each stack (optional, empty for default)
     */
    void ConfigureMac(nishioka::NishiokaStackContainer& stacks,
                      uint8_t channel = 0xD,
                      uint16_t panId = 0xCAFE,
                      const std::vector<Mac16Address>& addresses = std::vector<Mac16Address>());

  private:
    uint8_t m_seqNumCounter; //!< Sequence number counter
    ObjectFactory m_stackFactory; //!< NishiokaStack object factory
};

} // namespace ns3

#endif /* NISHIOKA_HELPER_H */


