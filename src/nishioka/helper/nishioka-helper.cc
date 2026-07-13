/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-helper.h"

#include "ns3/buffer.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/nishioka-stack.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/lr-wpan-mac-base.h"
#include "ns3/lr-wpan-fields.h"

#include <algorithm>
#include <utility>
#include <vector>

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

    const uint32_t totalSize = packet->GetSize();
    if (totalSize < 6)
    {
        NS_LOG_WARN("Packet too small for NishiokaHeader base fields");
        return false;
    }

    // Fast path: CreatePacket() always uses short destination and source addresses.
    {
        nishioka::NishiokaHeader probe;
        probe.SetDstAddrMode(nishioka::NishiokaHeader::SHORTADDR);
        probe.SetSrcAddrMode(nishioka::NishiokaHeader::SHORTADDR);
        const uint32_t H = probe.GetSerializedSize();

        if (totalSize >= H)
        {
            Ptr<Packet> copy = packet->Copy();
            const uint32_t before = copy->GetSize();
            nishioka::NishiokaHeader parsed;
            parsed.SetDstAddrMode(nishioka::NishiokaHeader::SHORTADDR);
            parsed.SetSrcAddrMode(nishioka::NishiokaHeader::SHORTADDR);
            copy->RemoveHeader(parsed);
            const uint32_t after = copy->GetSize();

            if (before - after == H)
            {
                header = parsed;
                if (after > 0)
                {
                    std::vector<uint8_t> payload(after);
                    copy->CopyData(payload.data(), after);
                    data.assign(payload.begin(), payload.end());
                }
                else
                {
                    data.clear();
                }

                NS_LOG_DEBUG("Extracted NishiokaHeader (short-short fast path): "
                             << "Src=" << header.GetShortSrcAddr()
                             << " Dst=" << header.GetShortDstAddr()
                             << " Payload size=" << data.size());
                return true;
            }
        }
    }

    std::vector<uint8_t> buf(totalSize);
    packet->CopyData(buf.data(), totalSize);

    using AM = nishioka::NishiokaHeader::AddrModeType;
    const AM dstModes[] = {AM::NOADDR, AM::SHORTADDR, AM::EXTADDR};
    const AM srcModes[] = {AM::NOADDR, AM::SHORTADDR, AM::EXTADDR};

    std::vector<std::pair<uint8_t, uint8_t>> candidates;
    for (AM dm : dstModes)
    {
        for (AM sm : srcModes)
        {
            nishioka::NishiokaHeader probe;
            probe.SetDstAddrMode(static_cast<uint8_t>(dm));
            probe.SetSrcAddrMode(static_cast<uint8_t>(sm));
            candidates.emplace_back(static_cast<uint8_t>(dm), static_cast<uint8_t>(sm));
        }
    }

    std::sort(candidates.begin(),
              candidates.end(),
              [](const std::pair<uint8_t, uint8_t>& a, const std::pair<uint8_t, uint8_t>& b) {
                  nishioka::NishiokaHeader pa;
                  pa.SetDstAddrMode(a.first);
                  pa.SetSrcAddrMode(a.second);
                  nishioka::NishiokaHeader pb;
                  pb.SetDstAddrMode(b.first);
                  pb.SetSrcAddrMode(b.second);
                  return pa.GetSerializedSize() > pb.GetSerializedSize();
              });

    bool found = false;
    uint32_t bestH = 0;
    nishioka::NishiokaHeader bestHeader;
    std::string bestData;

    for (const auto& c : candidates)
    {
        nishioka::NishiokaHeader probe;
        probe.SetDstAddrMode(c.first);
        probe.SetSrcAddrMode(c.second);
        const uint32_t H = probe.GetSerializedSize();
        if (H > totalSize)
        {
            continue;
        }

        // Reject NOADDR layouts when trailing bytes remain (false positive on 6-byte prefix).
        if (totalSize > H &&
            (c.first == static_cast<uint8_t>(AM::NOADDR) ||
             c.second == static_cast<uint8_t>(AM::NOADDR)))
        {
            continue;
        }

        Buffer wire;
        wire.AddAtStart(H);
        Buffer::Iterator wit = wire.Begin();
        for (uint32_t i = 0; i < H; ++i)
        {
            wit.WriteU8(buf[i]);
        }

        nishioka::NishiokaHeader parsed;
        parsed.SetDstAddrMode(c.first);
        parsed.SetSrcAddrMode(c.second);
        Buffer::Iterator rit = wire.Begin();
        const uint32_t n = parsed.Deserialize(rit);
        if (n != H)
        {
            continue;
        }

        Buffer roundTrip;
        roundTrip.AddAtStart(H);
        Buffer::Iterator sit = roundTrip.Begin();
        parsed.Serialize(sit);

        Buffer::Iterator chk = roundTrip.Begin();
        bool match = true;
        for (uint32_t i = 0; i < H; ++i)
        {
            if (chk.ReadU8() != buf[i])
            {
                match = false;
                break;
            }
        }
        if (!match)
        {
            continue;
        }

        if (H > bestH)
        {
            bestH = H;
            bestHeader = parsed;
            bestData.assign(reinterpret_cast<const char*>(buf.data() + H), totalSize - H);
            found = true;
        }
    }

    if (!found)
    {
        NS_LOG_WARN("Could not deserialize NishiokaHeader (no matching address layout)");
        return false;
    }

    header = bestHeader;
    data = bestData;

    NS_LOG_DEBUG("Extracted NishiokaHeader (probe fallback H=" << bestH << "): "
                 << "SrcMode=" << (int)header.GetSrcAddrMode()
                 << " DstMode=" << (int)header.GetDstAddrMode()
                 << " Src=" << header.GetShortSrcAddr() << " Dst=" << header.GetShortDstAddr()
                 << " Payload size=" << data.size());

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

    nishioka::NishiokaHeader header;
    std::string data;

    if (!ExtractHeader(packet, header, data))
    {
        NS_LOG_ERROR("Failed to extract header from packet");
        return nullptr;
    }

    header.SetSrcAddrFields(header.GetSrcPanId(), newSrcAddr);
    header.SetBattery(static_cast<uint16_t>(newBattery));
    const uint16_t evaluation =
        (static_cast<uint16_t>(newLqi) << 8) | static_cast<uint16_t>(newHops);
    header.SetEvaluation(evaluation);

    Ptr<Packet> rebuilt = Create<Packet>((const uint8_t*)data.data(), data.size());
    rebuilt->AddHeader(header);
    return rebuilt;
}

Ptr<Packet>
NishiokaHelper::UpdateRoutingInfo(Ptr<Packet> packet,
                                    Mac64Address newSrcAddr,
                                    uint8_t newBattery,
                                    uint8_t newLqi,
                                    uint8_t newHops)
{
    NS_LOG_FUNCTION(this << newSrcAddr << (int)newBattery << (int)newLqi << (int)newHops);

    nishioka::NishiokaHeader header;
    std::string data;

    if (!ExtractHeader(packet, header, data))
    {
        NS_LOG_ERROR("Failed to extract header from packet");
        return nullptr;
    }

    header.SetSrcAddrFields(header.GetSrcPanId(), newSrcAddr);
    header.SetBattery(static_cast<uint16_t>(newBattery));
    const uint16_t evaluation =
        (static_cast<uint16_t>(newLqi) << 8) | static_cast<uint16_t>(newHops);
    header.SetEvaluation(evaluation);

    Ptr<Packet> rebuilt = Create<Packet>((const uint8_t*)data.data(), data.size());
    rebuilt->AddHeader(header);
    return rebuilt;
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
NishiokaHelper::Install(NetDeviceContainer netDevices)
{
    NS_LOG_FUNCTION(this);

    nishioka::NishiokaStackContainer stackContainer;

    for (uint32_t i = 0; i < netDevices.GetN(); ++i)
    {
        Ptr<NetDevice> device = netDevices.Get(i);

        NS_ASSERT_MSG(device, "NetDevice not found at index " << i);

        Ptr<Node> node = device->GetNode();
        NS_ABORT_MSG_UNLESS(node,
                            "NetDevice at index "
                                << i
                                << " is not attached to a Node. Install the NetDevice first "
                                   "(e.g. LrWpanHelper::Install or Node::AddDevice).");

        // MAC may not be aggregated until NetDevice::Initialize() (see LrWpanNetDevice).
        // LrWpanMacBase availability is verified in NishiokaStack::DoInitialize().
        NS_LOG_LOGIC("Installing NishiokaStack on node " << node->GetId());

        Ptr<nishioka::NishiokaStack> stack = m_stackFactory.Create<nishioka::NishiokaStack>();
        stackContainer.Add(stack);
        node->AggregateObject(stack);
        stack->SetNetDevice(device);
    }

    return stackContainer;
}

nishioka::NishiokaStackContainer
NishiokaHelper::Install(NetDeviceContainer netDevices, const std::vector<Vector>& positions)
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(positions.size() == netDevices.GetN(),
                  "Number of positions must match number of NetDevices");

    nishioka::NishiokaStackContainer stackContainer = Install(netDevices);

    for (uint32_t i = 0; i < netDevices.GetN(); ++i)
    {
        Ptr<Node> node = netDevices.Get(i)->GetNode();
        NS_ASSERT_MSG(node, "NetDevice at index " << i << " has no Node");

        Ptr<ConstantPositionMobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
        mobility->SetPosition(positions[i]);
        node->AggregateObject(mobility);
        NS_LOG_LOGIC("Set mobility model for node " << node->GetId() << " at position "
                                                    << positions[i]);
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


