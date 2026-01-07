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

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NishiokaHelper");

NishiokaHelper::NishiokaHelper()
    : m_seqNumCounter(0)
{
    NS_LOG_FUNCTION(this);
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

} // namespace ns3

