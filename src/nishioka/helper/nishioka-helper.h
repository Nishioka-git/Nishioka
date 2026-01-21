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
 * @brief Helper class for creating and managing NishiokaHeader in packets
 * and installing NishiokaStack on nodes
 *
 * This helper class provides convenient methods for:
 * - Creating packets with NishiokaHeader
 * - Extracting NishiokaHeader from packets
 * - Setting routing information (battery, LQI, hops) in headers
 * - Managing packet creation for multi-hop routing scenarios
 * - Installing NishiokaStack on nodes with automatic setup
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
     * Creates a new packet with updated header information
     *
     * @param packet Original packet
     * @param newSrcAddr New source address
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
     * Get the next sequence number (auto-incremented)
     *
     * @return Next sequence number
     */
    uint8_t GetNextSeqNum();

    /**
     * Reset sequence number counter
     */
    void ResetSeqNum();

    /**
     * Install NishiokaStack on top of existing NetDevices.
     *
     * This function creates nodes, installs NetDevices, sets up mobility models,
     * and installs NishiokaStack for each device in the container.
     *
     * @param netDevices Container of NetDevices (e.g., UartLrWpanNetDevice)
     * @param positions Vector of positions for each device (must match device count)
     * @return Container with the newly created NishiokaStacks
     */
    nishioka::NishiokaStackContainer Install(NetDeviceContainer netDevices,
                                              const std::vector<Vector>& positions);

    /**
     * Install NishiokaStack on top of existing NetDevices with nodes.
     *
     * This function installs NishiokaStack on devices that are already
     * attached to nodes. Mobility models are set up if positions are provided.
     *
     * @param netDevices Container of NetDevices already attached to nodes
     * @param positions Vector of positions for each device (optional, can be empty)
     * @return Container with the newly created NishiokaStacks
     */
    nishioka::NishiokaStackContainer Install(NetDeviceContainer netDevices,
                                              const std::vector<Vector>& positions,
                                              NodeContainer nodes);

    /**
     * Set an attribute on each NishiokaStack created by Install.
     *
     * @param n1 The name of the attribute to set.
     * @param v1 The value of the attribute to set.
     */
    void SetStackAttribute(std::string n1, const AttributeValue& v1);

  private:
    uint8_t m_seqNumCounter; //!< Sequence number counter
    ObjectFactory m_stackFactory; //!< NishiokaStack object factory
};

} // namespace ns3

#endif /* NISHIOKA_HELPER_H */


