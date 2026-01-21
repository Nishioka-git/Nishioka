/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-helper.h"

#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/nishioka-stack.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/lr-wpan-mac-base.h"
#include "ns3/lr-wpan-fields.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NishiokaHelper");

NishiokaHelper::NishiokaHelper()
    : m_seqNumCounter(0)
{
    NS_LOG_FUNCTION(this);
    m_stackFactory.SetTypeId("ns3::nishioka::NishiokaStack");
}

Ptr<Packet>
NishiokaHelper::CreatePacket(const std::string& data,
                             Mac16Address srcAddr,
                             Mac16Address dstAddr,
                             uint16_t panId,
                             uint8_t battery,
                             uint8_t lqi,
                             uint8_t hops,
                             uint8_t seqNum)
{
    NS_LOG_FUNCTION(this << data << srcAddr << dstAddr << (int)panId
                         << (int)battery << (int)lqi << (int)hops << (int)seqNum);

    // Create payload packet
    Ptr<Packet> payload = Create<Packet>((const uint8_t*)data.c_str(), data.size());

    // Create and configure NishiokaHeader
    nishioka::NishiokaHeader header;
    header.SetFrameType(nishioka::NishiokaHeader::CUSTOM_DATA);
    
    // Use provided seqNum or auto-increment
    if (seqNum == 0 && m_seqNumCounter > 0) {
        header.SetSeqNum(m_seqNumCounter++);
    } else if (seqNum > 0) {
        header.SetSeqNum(seqNum);
        m_seqNumCounter = seqNum + 1;
    } else {
        header.SetSeqNum(m_seqNumCounter++);
    }
    
    header.SetSrcAddrFields(panId, srcAddr);
    header.SetDstAddrFields(panId, dstAddr);
    header.SetBattery(battery);
    
    // Combine LQI and hops into evaluation field
    // Format: (LQI << 8) | hops
    uint16_t evaluation = (static_cast<uint16_t>(lqi) << 8) | hops;
    header.SetEvaluation(evaluation);

    // Add header to packet
    payload->AddHeader(header);

    NS_LOG_DEBUG("Created packet with NishiokaHeader: "
                 << "Src=" << srcAddr << " Dst=" << dstAddr
                 << " Battery=" << (int)battery << "% LQI=" << (int)lqi
                 << " Hops=" << (int)hops << " SeqNum=" << (int)header.GetSeqNum());

    return payload;
}

Ptr<Packet>
NishiokaHelper::CreatePacket(const std::string& data,
                             Mac64Address srcAddr,
                             Mac64Address dstAddr,
                             uint16_t panId,
                             uint8_t battery,
                             uint8_t lqi,
                             uint8_t hops,
                             uint8_t seqNum)
{
    NS_LOG_FUNCTION(this << data << srcAddr << dstAddr << (int)panId
                         << (int)battery << (int)lqi << (int)hops << (int)seqNum);

    // Create payload packet
    Ptr<Packet> payload = Create<Packet>((const uint8_t*)data.c_str(), data.size());

    // Create and configure NishiokaHeader
    nishioka::NishiokaHeader header;
    header.SetFrameType(nishioka::NishiokaHeader::CUSTOM_DATA);
    
    // Use provided seqNum or auto-increment
    if (seqNum == 0 && m_seqNumCounter > 0) {
        header.SetSeqNum(m_seqNumCounter++);
    } else if (seqNum > 0) {
        header.SetSeqNum(seqNum);
        m_seqNumCounter = seqNum + 1;
    } else {
        header.SetSeqNum(m_seqNumCounter++);
    }
    
    header.SetSrcAddrFields(panId, srcAddr);
    header.SetDstAddrFields(panId, dstAddr);
    header.SetBattery(battery);
    
    // Combine LQI and hops into evaluation field
    uint16_t evaluation = (static_cast<uint16_t>(lqi) << 8) | hops;
    header.SetEvaluation(evaluation);

    // Add header to packet
    payload->AddHeader(header);

    NS_LOG_DEBUG("Created packet with NishiokaHeader (64-bit addresses): "
                 << "Src=" << srcAddr << " Dst=" << dstAddr
                 << " Battery=" << (int)battery << "% LQI=" << (int)lqi
                 << " Hops=" << (int)hops);

    return payload;
}

bool
NishiokaHelper::ExtractHeader(Ptr<Packet> packet, nishioka::NishiokaHeader& header, std::string& data)
{
    NS_LOG_FUNCTION(this << packet);

    // Create a copy to avoid modifying the original
    Ptr<Packet> pktCopy = packet->Copy();

    // Set address modes before removing header
    header.SetDstAddrMode(nishioka::NishiokaHeader::SHORTADDR);
    header.SetSrcAddrMode(nishioka::NishiokaHeader::SHORTADDR);

    // Check if packet is large enough
    if (pktCopy->GetSize() < header.GetSerializedSize()) {
        NS_LOG_WARN("Packet too small to contain NishiokaHeader");
        return false;
    }

    // Remove header
    pktCopy->RemoveHeader(header);

    // Extract payload data
    std::vector<uint8_t> buffer;
    buffer.resize(pktCopy->GetSize());
    pktCopy->CopyData(buffer.data(), pktCopy->GetSize());
    data = std::string(buffer.begin(), buffer.end());

    NS_LOG_DEBUG("Extracted NishiokaHeader: "
                 << "Src=" << header.GetShortSrcAddr()
                 << " Dst=" << header.GetShortDstAddr()
                 << " Battery=" << (int)header.GetBattery() << "%"
                 << " Payload size=" << data.size() << " bytes");

    return true;
}

void
NishiokaHelper::ExtractRoutingInfo(const nishioka::NishiokaHeader& header,
                                    uint8_t& battery,
                                    uint8_t& lqi,
                                    uint8_t& hops)
{
    NS_LOG_FUNCTION(this);

    battery = static_cast<uint8_t>(header.GetBattery());
    
    // Extract LQI and hops from evaluation field
    uint16_t evaluation = header.GetEvaluation();
    lqi = static_cast<uint8_t>((evaluation >> 8) & 0xFF);
    hops = static_cast<uint8_t>(evaluation & 0xFF);

    NS_LOG_DEBUG("Extracted routing info: "
                 << "Battery=" << (int)battery << "%"
                 << " LQI=" << (int)lqi
                 << " Hops=" << (int)hops);
}

Ptr<Packet>
NishiokaHelper::UpdateRoutingInfo(Ptr<Packet> packet,
                                    Mac16Address newSrcAddr,
                                    uint8_t newBattery,
                                    uint8_t newLqi,
                                    uint8_t newHops)
{
    NS_LOG_FUNCTION(this << newSrcAddr << (int)newBattery << (int)newLqi << (int)newHops);

    // Extract existing header and data
    nishioka::NishiokaHeader header;
    std::string data;
    
    if (!ExtractHeader(packet, header, data)) {
        NS_LOG_ERROR("Failed to extract header from packet");
        return nullptr;
    }

    // Get destination address from original header
    Mac16Address dstAddr = header.GetShortDstAddr();
    uint16_t panId = header.GetDstPanId();

    // Create new packet with updated information
    return CreatePacket(data, newSrcAddr, dstAddr, panId, newBattery, newLqi, newHops);
}

uint8_t
NishiokaHelper::GetNextSeqNum()
{
    NS_LOG_FUNCTION(this);
    return m_seqNumCounter;
}

void
NishiokaHelper::ResetSeqNum()
{
    NS_LOG_FUNCTION(this);
    m_seqNumCounter = 0;
}

nishioka::NishiokaStackContainer
NishiokaHelper::Install(NetDeviceContainer netDevices, const std::vector<Vector>& positions)
{
    NS_LOG_FUNCTION(this);

    nishioka::NishiokaStackContainer stackContainer;
    NodeContainer nodes;

    // Create nodes for each device
    for (uint32_t i = 0; i < netDevices.GetN(); ++i)
    {
        Ptr<Node> node = CreateObject<Node>();
        nodes.Add(node);
    }

    // Install stacks
    return Install(netDevices, positions, nodes);
}

nishioka::NishiokaStackContainer
NishiokaHelper::Install(NetDeviceContainer netDevices,
                        const std::vector<Vector>& positions,
                        NodeContainer nodes)
{
    NS_LOG_FUNCTION(this);

    nishioka::NishiokaStackContainer stackContainer;

    NS_ASSERT_MSG(netDevices.GetN() == nodes.GetN(),
                  "Number of devices must match number of nodes");

    if (!positions.empty())
    {
        NS_ASSERT_MSG(positions.size() == netDevices.GetN(),
                      "Number of positions must match number of devices");
    }

    for (uint32_t i = 0; i < netDevices.GetN(); ++i)
    {
        Ptr<NetDevice> device = netDevices.Get(i);
        Ptr<Node> node = nodes.Get(i);

        NS_ASSERT_MSG(device, "NetDevice not found at index " << i);
        NS_ASSERT_MSG(node, "Node not found at index " << i);

        NS_LOG_LOGIC("Installing NishiokaStack on node " << node->GetId());

        // Add device to node if not already added
        if (device->GetNode() != node)
        {
            node->AddDevice(device);
        }

        // Set up mobility model if position is provided
        if (!positions.empty() && i < positions.size())
        {
            Ptr<ConstantPositionMobilityModel> mobility =
                CreateObject<ConstantPositionMobilityModel>();
            mobility->SetPosition(positions[i]);
            node->AggregateObject(mobility);
            NS_LOG_LOGIC("Set mobility model for node " << node->GetId()
                                                         << " at position " << positions[i]);
        }

        // Create and install NishiokaStack
        Ptr<nishioka::NishiokaStack> stack = m_stackFactory.Create<nishioka::NishiokaStack>();
        stackContainer.Add(stack);
        node->AggregateObject(stack);
        stack->SetNetDevice(device);
        // Note: Initialize() is called automatically by ns-3 when the simulation starts
    }

    return stackContainer;
}

void
NishiokaHelper::SetStackAttribute(std::string n1, const AttributeValue& v1)
{
    NS_LOG_FUNCTION(this << n1);
    m_stackFactory.Set(n1, v1);
}

void
NishiokaHelper::ConfigureMac(nishioka::NishiokaStackContainer& stacks,
                               uint8_t channel,
                               uint16_t panId,
                               const std::vector<Mac16Address>& addresses)
{
    NS_LOG_FUNCTION(this << (int)channel << (int)panId);

    using namespace ns3::lrwpan;

    // Set channel for all stacks
    Ptr<MacPibAttributes> pibAttrChannel = Create<MacPibAttributes>();
    pibAttrChannel->pCurrentChannel = channel;

    // Set PAN ID for all stacks
    Ptr<MacPibAttributes> pibAttrPan = Create<MacPibAttributes>();
    pibAttrPan->macPanId = panId;

    for (uint32_t i = 0; i < stacks.GetN(); ++i)
    {
        Ptr<nishioka::NishiokaStack> stack = stacks.Get(i);
        if (!stack)
        {
            NS_LOG_WARN("Stack at index " << i << " is null");
            continue;
        }

        // Initialize stack if not already initialized (required to access MAC layer)
        if (!stack->GetMac())
        {
            NS_LOG_LOGIC("Initializing stack at index " << i << " to access MAC layer");
            stack->Initialize();
        }

        Ptr<LrWpanMacBase> mac = stack->GetMac();
        if (!mac)
        {
            NS_LOG_WARN("MAC layer not available for stack at index " << i);
            continue;
        }

        // Set channel
        mac->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttrChannel);
        NS_LOG_LOGIC("Set channel " << (int)channel << " for stack " << i);

        // Set PAN ID
        mac->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, pibAttrPan);
        NS_LOG_LOGIC("Set PAN ID 0x" << std::hex << panId << std::dec << " for stack " << i);

        // Set address if provided
        if (!addresses.empty() && i < addresses.size())
        {
            Ptr<MacPibAttributes> pibAttrAddr = Create<MacPibAttributes>();
            pibAttrAddr->macShortAddress = addresses[i];
            mac->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttrAddr);
            NS_LOG_LOGIC("Set address " << addresses[i] << " for stack " << i);
        }
    }
}

} // namespace ns3


