/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-rpl-payload-header.h"

#include "ns3/log.h"

namespace ns3
{
namespace nishioka
{

NS_LOG_COMPONENT_DEFINE("NishiokaRplPayloadHeader");

// --- RplControlHeader ---

RplControlHeader::RplControlHeader() = default;

TypeId
RplControlHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::RplControlHeader")
                            .SetParent<Header>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<RplControlHeader>();
    return tid;
}

TypeId
RplControlHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplControlHeader::GetSerializedSize() const
{
    return 1;
}

void
RplControlHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(m_controlType);
}

uint32_t
RplControlHeader::Deserialize(Buffer::Iterator start)
{
    m_controlType = start.ReadU8();
    return GetSerializedSize();
}

void
RplControlHeader::Print(std::ostream& os) const
{
    os << "RplControl type=" << static_cast<uint32_t>(m_controlType);
}

void
RplControlHeader::SetControlType(RplControlType type)
{
    m_controlType = static_cast<uint8_t>(type);
}

RplControlType
RplControlHeader::GetControlType() const
{
    return static_cast<RplControlType>(m_controlType);
}

// --- RplDioHeader ---

RplDioHeader::RplDioHeader() = default;

TypeId
RplDioHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::RplDioHeader")
                            .SetParent<Header>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<RplDioHeader>();
    return tid;
}

TypeId
RplDioHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplDioHeader::GetSerializedSize() const
{
    return 1 + 2 + 1 + 1 + 2 + 2;
}

void
RplDioHeader::Serialize(Buffer::Iterator start) const
{
    uint8_t buffer[2];
    start.WriteU8(static_cast<uint8_t>(RplControlType::DIO));
    m_dodag.dodagId.CopyTo(buffer);
    start.WriteU8(buffer[0]);
    start.WriteU8(buffer[1]);
    start.WriteU8(m_dodag.instanceId);
    start.WriteU8(m_dodag.version);
    start.WriteU16(m_dodag.rank);
    start.WriteU16(m_senderRank);
}

uint32_t
RplDioHeader::Deserialize(Buffer::Iterator start)
{
    start.ReadU8(); // control type
    uint8_t buffer[2];
    buffer[0] = start.ReadU8();
    buffer[1] = start.ReadU8();
    m_dodag.dodagId.CopyFrom(buffer);
    m_dodag.instanceId = start.ReadU8();
    m_dodag.version = start.ReadU8();
    m_dodag.rank = start.ReadU16();
    m_senderRank = start.ReadU16();
    return GetSerializedSize();
}

void
RplDioHeader::Print(std::ostream& os) const
{
    os << "DIO dodag=" << m_dodag.dodagId << " rank=" << m_senderRank;
}

void
RplDioHeader::SetDodagInfo(const RplDodagInfo& info)
{
    m_dodag = info;
}

RplDodagInfo
RplDioHeader::GetDodagInfo() const
{
    return m_dodag;
}

void
RplDioHeader::SetSenderRank(uint16_t rank)
{
    m_senderRank = rank;
}

uint16_t
RplDioHeader::GetSenderRank() const
{
    return m_senderRank;
}

// --- RplDaoHeader ---

RplDaoHeader::RplDaoHeader() = default;

TypeId
RplDaoHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::RplDaoHeader")
                            .SetParent<Header>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<RplDaoHeader>();
    return tid;
}

TypeId
RplDaoHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplDaoHeader::GetSerializedSize() const
{
    return 1 + 2 + 1;
}

void
RplDaoHeader::Serialize(Buffer::Iterator start) const
{
    uint8_t buffer[2];
    start.WriteU8(static_cast<uint8_t>(RplControlType::DAO));
    m_target.CopyTo(buffer);
    start.WriteU8(buffer[0]);
    start.WriteU8(buffer[1]);
    start.WriteU8(m_pathSequence);
}

uint32_t
RplDaoHeader::Deserialize(Buffer::Iterator start)
{
    start.ReadU8();
    uint8_t buffer[2];
    buffer[0] = start.ReadU8();
    buffer[1] = start.ReadU8();
    m_target.CopyFrom(buffer);
    m_pathSequence = start.ReadU8();
    return GetSerializedSize();
}

void
RplDaoHeader::Print(std::ostream& os) const
{
    os << "DAO target=" << m_target << " pathSeq=" << static_cast<uint32_t>(m_pathSequence);
}

void
RplDaoHeader::SetTarget(Mac16Address target)
{
    m_target = target;
}

Mac16Address
RplDaoHeader::GetTarget() const
{
    return m_target;
}

void
RplDaoHeader::SetPathSequence(uint8_t seq)
{
    m_pathSequence = seq;
}

uint8_t
RplDaoHeader::GetPathSequence() const
{
    return m_pathSequence;
}

// --- RplDaoAckHeader ---

RplDaoAckHeader::RplDaoAckHeader() = default;

TypeId
RplDaoAckHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::RplDaoAckHeader")
                            .SetParent<Header>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<RplDaoAckHeader>();
    return tid;
}

TypeId
RplDaoAckHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplDaoAckHeader::GetSerializedSize() const
{
    return 1 + 2 + 1;
}

void
RplDaoAckHeader::Serialize(Buffer::Iterator start) const
{
    uint8_t buffer[2];
    start.WriteU8(static_cast<uint8_t>(RplControlType::DAO_ACK));
    m_target.CopyTo(buffer);
    start.WriteU8(buffer[0]);
    start.WriteU8(buffer[1]);
    start.WriteU8(m_status);
}

uint32_t
RplDaoAckHeader::Deserialize(Buffer::Iterator start)
{
    start.ReadU8();
    uint8_t buffer[2];
    buffer[0] = start.ReadU8();
    buffer[1] = start.ReadU8();
    m_target.CopyFrom(buffer);
    m_status = start.ReadU8();
    return GetSerializedSize();
}

void
RplDaoAckHeader::Print(std::ostream& os) const
{
    os << "DAO-ACK target=" << m_target << " status=" << static_cast<uint32_t>(m_status);
}

void
RplDaoAckHeader::SetTarget(Mac16Address target)
{
    m_target = target;
}

Mac16Address
RplDaoAckHeader::GetTarget() const
{
    return m_target;
}

void
RplDaoAckHeader::SetStatus(uint8_t status)
{
    m_status = status;
}

uint8_t
RplDaoAckHeader::GetStatus() const
{
    return m_status;
}

} // namespace nishioka
} // namespace ns3
