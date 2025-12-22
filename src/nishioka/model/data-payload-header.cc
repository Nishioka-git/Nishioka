
/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Simple Data Payload Header implementation
 */

#include "data-payload-header.h"

namespace ns3
{
namespace nishioka
{

NS_OBJECT_ENSURE_REGISTERED(DataPayloadHeader);

DataPayloadHeader::DataPayloadHeader()
    : m_id(0),
      m_energy(0)
{
}

TypeId
DataPayloadHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::DataPayloadHeader")
                            .SetParent<Header>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<DataPayloadHeader>();
    return tid;
}

TypeId
DataPayloadHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DataPayloadHeader::GetSerializedSize() const
{
    // ID (4 bytes) + Energy (4 bytes) = 8 bytes total
    return 8;
}

void
DataPayloadHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU32(m_id);
    start.WriteHtonU32(m_energy);
}

uint32_t
DataPayloadHeader::Deserialize(Buffer::Iterator start)
{
    m_id = start.ReadNtohU32();
    m_energy = start.ReadNtohU32();
    return 8;
}

void
DataPayloadHeader::Print(std::ostream& os) const
{
    os << "Id=" << m_id << " Energy=" << m_energy;
}

void
DataPayloadHeader::SetId(uint32_t id)
{
    m_id = id;
}

uint32_t
DataPayloadHeader::GetId() const
{
    return m_id;
}

void
DataPayloadHeader::SetEnergy(uint32_t energy)
{
    m_energy = energy;
}

uint32_t
DataPayloadHeader::GetEnergy() const
{
    return m_energy;
}

} // namespace nishioka
} // namespace ns3

