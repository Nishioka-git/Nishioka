/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Custom MAC Header implementation
 */

#include "nishioka-header.h"

#include "ns3/address-utils.h"

namespace ns3
{
namespace nishioka
{

NS_OBJECT_ENSURE_REGISTERED(NishiokaHeader);

NishiokaHeader::NishiokaHeader()
    : m_frameType(CUSTOM_DATA),
      m_seqNum(0),
      m_dstAddrMode(NOADDR),
      m_srcAddrMode(NOADDR),
      m_dstPanId(0),
      m_shortDstAddr(),
      m_extDstAddr(),
      m_srcPanId(0),
      m_shortSrcAddr(),
      m_extSrcAddr(),
      m_battery(0),
      m_evaluation(0)
{
}

TypeId
NishiokaHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::NishiokaHeader")
                            .SetParent<Header>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<NishiokaHeader>();
    return tid;
}

TypeId
NishiokaHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
NishiokaHeader::GetSerializedSize() const
{
    uint32_t size = 2; // Frame type (1 byte) + Sequence number (1 byte)

    // Battery (2 bytes) + Evaluation (2 bytes)
    size += 4;

    // Add destination address size
    switch (m_dstAddrMode)
    {
    case NOADDR:
        break;
    case SHORTADDR:
        size += 4; // PAN ID (2 bytes) + Short address (2 bytes)
        break;
    case EXTADDR:
        size += 10; // PAN ID (2 bytes) + Extended address (8 bytes)
        break;
    }

    // Add source address size
    switch (m_srcAddrMode)
    {
    case NOADDR:
        break;
    case SHORTADDR:
        size += 4; // PAN ID (2 bytes) + Short address (2 bytes)
        break;
    case EXTADDR:
        size += 10; // PAN ID (2 bytes) + Extended address (8 bytes)
        break;
    }

    return size;
}

void
NishiokaHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;

    // Serialize frame type and sequence number
    i.WriteU8(m_frameType);
    i.WriteU8(m_seqNum);

    // Serialize battery and evaluation
    i.WriteHtolsbU16(m_battery);
    i.WriteHtolsbU16(m_evaluation);

    // Serialize destination address
    switch (m_dstAddrMode)
    {
    case NOADDR:
        break;
    case SHORTADDR:
        i.WriteHtolsbU16(m_dstPanId);
        WriteTo(i, m_shortDstAddr);
        break;
    case EXTADDR:
        i.WriteHtolsbU16(m_dstPanId);
        WriteTo(i, m_extDstAddr);
        break;
    }

    // Serialize source address
    switch (m_srcAddrMode)
    {
    case NOADDR:
        break;
    case SHORTADDR:
        i.WriteHtolsbU16(m_srcPanId);
        WriteTo(i, m_shortSrcAddr);
        break;
    case EXTADDR:
        i.WriteHtolsbU16(m_srcPanId);
        WriteTo(i, m_extSrcAddr);
        break;
    }
}

uint32_t
NishiokaHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;

    // Deserialize frame type and sequence number
    m_frameType = static_cast<FrameType>(i.ReadU8());
    m_seqNum = i.ReadU8();

    // Deserialize battery and evaluation
    m_battery = i.ReadLsbtohU16();
    m_evaluation = i.ReadLsbtohU16();

    // Deserialize destination address
    switch (m_dstAddrMode)
    {
    case NOADDR:
        break;
    case SHORTADDR:
        m_dstPanId = i.ReadLsbtohU16();
        ReadFrom(i, m_shortDstAddr);
        break;
    case EXTADDR:
        m_dstPanId = i.ReadLsbtohU16();
        ReadFrom(i, m_extDstAddr);
        break;
    }

    // Deserialize source address
    switch (m_srcAddrMode)
    {
    case NOADDR:
        break;
    case SHORTADDR:
        m_srcPanId = i.ReadLsbtohU16();
        ReadFrom(i, m_shortSrcAddr);
        break;
    case EXTADDR:
        m_srcPanId = i.ReadLsbtohU16();
        ReadFrom(i, m_extSrcAddr);
        break;
    }

    return i.GetDistanceFrom(start);
}

void
NishiokaHeader::Print(std::ostream& os) const
{
    os << "Frame Type = " << static_cast<uint32_t>(m_frameType)
       << ", Sequence Number = " << static_cast<uint32_t>(m_seqNum)
       << ", Battery = " << m_battery
       << ", Evaluation = " << m_evaluation;

    if (m_dstAddrMode != NOADDR)
    {
        os << ", Dst PAN ID = " << m_dstPanId;
        if (m_dstAddrMode == SHORTADDR)
        {
            os << ", Dst Short Addr = " << m_shortDstAddr;
        }
        else if (m_dstAddrMode == EXTADDR)
        {
            os << ", Dst Ext Addr = " << m_extDstAddr;
        }
    }

    if (m_srcAddrMode != NOADDR)
    {
        os << ", Src PAN ID = " << m_srcPanId;
        if (m_srcAddrMode == SHORTADDR)
        {
            os << ", Src Short Addr = " << m_shortSrcAddr;
        }
        else if (m_srcAddrMode == EXTADDR)
        {
            os << ", Src Ext Addr = " << m_extSrcAddr;
        }
    }
}

void
NishiokaHeader::SetFrameType(FrameType frameType)
{
    m_frameType = frameType;
}

NishiokaHeader::FrameType
NishiokaHeader::GetFrameType() const
{
    return static_cast<FrameType>(m_frameType);
}

void
NishiokaHeader::SetSeqNum(uint8_t seqNum)
{
    m_seqNum = seqNum;
}

uint8_t
NishiokaHeader::GetSeqNum() const
{
    return m_seqNum;
}

void
NishiokaHeader::SetSrcAddrFields(uint16_t panId, Mac16Address addr)
{
    m_srcPanId = panId;
    m_shortSrcAddr = addr;
    m_srcAddrMode = SHORTADDR;
}

void
NishiokaHeader::SetSrcAddrFields(uint16_t panId, Mac64Address addr)
{
    m_srcPanId = panId;
    m_extSrcAddr = addr;
    m_srcAddrMode = EXTADDR;
}

void
NishiokaHeader::SetDstAddrFields(uint16_t panId, Mac16Address addr)
{
    m_dstPanId = panId;
    m_shortDstAddr = addr;
    m_dstAddrMode = SHORTADDR;
}

void
NishiokaHeader::SetDstAddrFields(uint16_t panId, Mac64Address addr)
{
    m_dstPanId = panId;
    m_extDstAddr = addr;
    m_dstAddrMode = EXTADDR;
}

uint16_t
NishiokaHeader::GetDstPanId() const
{
    return m_dstPanId;
}

Mac16Address
NishiokaHeader::GetShortDstAddr() const
{
    return m_shortDstAddr;
}

Mac64Address
NishiokaHeader::GetExtDstAddr() const
{
    return m_extDstAddr;
}

uint16_t
NishiokaHeader::GetSrcPanId() const
{
    return m_srcPanId;
}

Mac16Address
NishiokaHeader::GetShortSrcAddr() const
{
    return m_shortSrcAddr;
}

Mac64Address
NishiokaHeader::GetExtSrcAddr() const
{
    return m_extSrcAddr;
}

void
NishiokaHeader::SetBattery(uint16_t battery)
{
    m_battery = battery;
}

uint16_t
NishiokaHeader::GetBattery() const
{
    return m_battery;
}

void
NishiokaHeader::SetEvaluation(uint16_t evaluation)
{
    m_evaluation = evaluation;
}

uint16_t
NishiokaHeader::GetEvaluation() const
{
    return m_evaluation;
}

void
NishiokaHeader::SetDstAddrMode(uint8_t addrMode)
{
    m_dstAddrMode = addrMode;
}

void
NishiokaHeader::SetSrcAddrMode(uint8_t addrMode)
{
    m_srcAddrMode = addrMode;
}

uint8_t
NishiokaHeader::GetDstAddrMode() const
{
    return m_dstAddrMode;
}

uint8_t
NishiokaHeader::GetSrcAddrMode() const
{
    return m_srcAddrMode;
}

} // namespace nishioka
} // namespace ns3