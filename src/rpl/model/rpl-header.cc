#include "rpl-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RplHeader");

/*
 * RplDisHeader
 */

NS_OBJECT_ENSURE_REGISTERED(RplDisHeader);

RplDisHeader::RplDisHeader()
    : m_flags(0),
      m_reserved(0)
{
}

TypeId
RplDisHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplDisHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<RplDisHeader>();
    return tid;
}

TypeId
RplDisHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplDisHeader::GetSerializedSize() const
{
    return 2;
}

void
RplDisHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_flags);
    i.WriteU8(m_reserved);
}

uint32_t
RplDisHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_flags = i.ReadU8();
    m_reserved = i.ReadU8();
    return GetSerializedSize();
}

void
RplDisHeader::Print(std::ostream& os) const
{
    os << "DIS flags=0x" << std::hex << int(m_flags) << std::dec;
}

void
RplDisHeader::SetFlags(uint8_t flags)
{
    m_flags = flags;
}

uint8_t
RplDisHeader::GetFlags() const
{
    return m_flags;
}

std::ostream&
operator<<(std::ostream& os, const RplDisHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * RplDioHeader
 */

NS_OBJECT_ENSURE_REGISTERED(RplDioHeader);

RplDioHeader::RplDioHeader()
    : m_instanceId(0),
      m_version(0),
      m_rank(0xffff),
      m_gMopPrf(0),
      m_dtsn(0),
      m_flags(0),
      m_reserved(0),
      m_dodagId(Ipv6Address::GetZero())
{
}

TypeId
RplDioHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplDioHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
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
    return 24;
}

void
RplDioHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_instanceId);
    i.WriteU8(m_version);
    i.WriteHtonU16(m_rank);
    i.WriteU8(m_gMopPrf);
    i.WriteU8(m_dtsn);
    i.WriteU8(m_flags);
    i.WriteU8(m_reserved);

    uint8_t buf[16];
    m_dodagId.Serialize(buf);
    i.Write(buf, 16);
}

uint32_t
RplDioHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_instanceId = i.ReadU8();
    m_version = i.ReadU8();
    m_rank = i.ReadNtohU16();
    m_gMopPrf = i.ReadU8();
    m_dtsn = i.ReadU8();
    m_flags = i.ReadU8();
    m_reserved = i.ReadU8();

    uint8_t buf[16];
    i.Read(buf, 16);
    m_dodagId.Set(buf);

    return GetSerializedSize();
}

void
RplDioHeader::Print(std::ostream& os) const
{
    os << "DIO instance=" << int(m_instanceId) << " ver=" << int(m_version)
       << " rank=" << m_rank << " G=" << GetGrounded() << " MOP=" << int(GetMop())
       << " Prf=" << int(GetPreference()) << " DTSN=" << int(m_dtsn) << " DODAGID=" << m_dodagId;
}

void
RplDioHeader::SetInstanceId(uint8_t instanceId)
{
    m_instanceId = instanceId;
}

uint8_t
RplDioHeader::GetInstanceId() const
{
    return m_instanceId;
}

void
RplDioHeader::SetVersion(uint8_t version)
{
    m_version = version;
}

uint8_t
RplDioHeader::GetVersion() const
{
    return m_version;
}

void
RplDioHeader::SetRank(uint16_t rank)
{
    m_rank = rank;
}

uint16_t
RplDioHeader::GetRank() const
{
    return m_rank;
}

void
RplDioHeader::SetGrounded(bool grounded)
{
    if (grounded)
    {
        m_gMopPrf |= 0x80;
    }
    else
    {
        m_gMopPrf &= ~0x80;
    }
}

bool
RplDioHeader::GetGrounded() const
{
    return (m_gMopPrf & 0x80) != 0;
}

void
RplDioHeader::SetMop(uint8_t mop)
{
    m_gMopPrf = (m_gMopPrf & 0x87) | ((mop & 0x07) << 3);
}

uint8_t
RplDioHeader::GetMop() const
{
    return (m_gMopPrf >> 3) & 0x07;
}

void
RplDioHeader::SetPreference(uint8_t preference)
{
    m_gMopPrf = (m_gMopPrf & 0xf8) | (preference & 0x07);
}

uint8_t
RplDioHeader::GetPreference() const
{
    return m_gMopPrf & 0x07;
}

void
RplDioHeader::SetDtsn(uint8_t dtsn)
{
    m_dtsn = dtsn;
}

uint8_t
RplDioHeader::GetDtsn() const
{
    return m_dtsn;
}

void
RplDioHeader::SetFlags(uint8_t flags)
{
    m_flags = flags;
}

uint8_t
RplDioHeader::GetFlags() const
{
    return m_flags;
}

void
RplDioHeader::SetDodagId(Ipv6Address dodagId)
{
    m_dodagId = dodagId;
}

Ipv6Address
RplDioHeader::GetDodagId() const
{
    return m_dodagId;
}

std::ostream&
operator<<(std::ostream& os, const RplDioHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * RplDaoHeader
 */

NS_OBJECT_ENSURE_REGISTERED(RplDaoHeader);

RplDaoHeader::RplDaoHeader()
    : m_instanceId(0),
      m_flags(0),
      m_reserved(0),
      m_daoSequence(0),
      m_dodagId(Ipv6Address::GetZero())
{
}

TypeId
RplDaoHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplDaoHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
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
    return GetDodagIdPresent() ? 20 : 4;
}

void
RplDaoHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_instanceId);
    i.WriteU8(m_flags);
    i.WriteU8(m_reserved);
    i.WriteU8(m_daoSequence);

    if (GetDodagIdPresent())
    {
        uint8_t buf[16];
        m_dodagId.Serialize(buf);
        i.Write(buf, 16);
    }
}

uint32_t
RplDaoHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_instanceId = i.ReadU8();
    m_flags = i.ReadU8();
    m_reserved = i.ReadU8();
    m_daoSequence = i.ReadU8();

    if (GetDodagIdPresent())
    {
        uint8_t buf[16];
        i.Read(buf, 16);
        m_dodagId.Set(buf);
    }

    return GetSerializedSize();
}

void
RplDaoHeader::Print(std::ostream& os) const
{
    os << "DAO instance=" << int(m_instanceId) << " K=" << GetAckRequired()
       << " D=" << GetDodagIdPresent() << " seq=" << int(m_daoSequence);
    if (GetDodagIdPresent())
    {
        os << " DODAGID=" << m_dodagId;
    }
}

void
RplDaoHeader::SetInstanceId(uint8_t instanceId)
{
    m_instanceId = instanceId;
}

uint8_t
RplDaoHeader::GetInstanceId() const
{
    return m_instanceId;
}

void
RplDaoHeader::SetAckRequired(bool k)
{
    if (k)
    {
        m_flags |= 0x80;
    }
    else
    {
        m_flags &= ~0x80;
    }
}

bool
RplDaoHeader::GetAckRequired() const
{
    return (m_flags & 0x80) != 0;
}

void
RplDaoHeader::SetDodagIdPresent(bool d)
{
    if (d)
    {
        m_flags |= 0x40;
    }
    else
    {
        m_flags &= ~0x40;
    }
}

bool
RplDaoHeader::GetDodagIdPresent() const
{
    return (m_flags & 0x40) != 0;
}

void
RplDaoHeader::SetFlags(uint8_t flags)
{
    // Preserve K and D bits
    m_flags = (m_flags & 0xc0) | (flags & 0x3f);
}

uint8_t
RplDaoHeader::GetFlags() const
{
    return m_flags & 0x3f;
}

void
RplDaoHeader::SetDaoSequence(uint8_t seq)
{
    m_daoSequence = seq;
}

uint8_t
RplDaoHeader::GetDaoSequence() const
{
    return m_daoSequence;
}

void
RplDaoHeader::SetDodagId(Ipv6Address dodagId)
{
    m_dodagId = dodagId;
    SetDodagIdPresent(true);
}

Ipv6Address
RplDaoHeader::GetDodagId() const
{
    return m_dodagId;
}

std::ostream&
operator<<(std::ostream& os, const RplDaoHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * RplDaoAckHeader
 */

NS_OBJECT_ENSURE_REGISTERED(RplDaoAckHeader);

RplDaoAckHeader::RplDaoAckHeader()
    : m_instanceId(0),
      m_flags(0),
      m_daoSequence(0),
      m_status(0),
      m_dodagId(Ipv6Address::GetZero())
{
}

TypeId
RplDaoAckHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplDaoAckHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
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
    return GetDodagIdPresent() ? 20 : 4;
}

void
RplDaoAckHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_instanceId);
    i.WriteU8(m_flags);
    i.WriteU8(m_daoSequence);
    i.WriteU8(m_status);

    if (GetDodagIdPresent())
    {
        uint8_t buf[16];
        m_dodagId.Serialize(buf);
        i.Write(buf, 16);
    }
}

uint32_t
RplDaoAckHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_instanceId = i.ReadU8();
    m_flags = i.ReadU8();
    m_daoSequence = i.ReadU8();
    m_status = i.ReadU8();

    if (GetDodagIdPresent())
    {
        uint8_t buf[16];
        i.Read(buf, 16);
        m_dodagId.Set(buf);
    }

    return GetSerializedSize();
}

void
RplDaoAckHeader::Print(std::ostream& os) const
{
    os << "DAO-ACK instance=" << int(m_instanceId) << " D=" << GetDodagIdPresent()
       << " seq=" << int(m_daoSequence) << " status=" << int(m_status);
    if (GetDodagIdPresent())
    {
        os << " DODAGID=" << m_dodagId;
    }
}

void
RplDaoAckHeader::SetInstanceId(uint8_t instanceId)
{
    m_instanceId = instanceId;
}

uint8_t
RplDaoAckHeader::GetInstanceId() const
{
    return m_instanceId;
}

void
RplDaoAckHeader::SetDodagIdPresent(bool d)
{
    if (d)
    {
        m_flags |= 0x80;
    }
    else
    {
        m_flags &= ~0x80;
    }
}

bool
RplDaoAckHeader::GetDodagIdPresent() const
{
    return (m_flags & 0x80) != 0;
}

void
RplDaoAckHeader::SetDaoSequence(uint8_t seq)
{
    m_daoSequence = seq;
}

uint8_t
RplDaoAckHeader::GetDaoSequence() const
{
    return m_daoSequence;
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

void
RplDaoAckHeader::SetDodagId(Ipv6Address dodagId)
{
    m_dodagId = dodagId;
    SetDodagIdPresent(true);
}

Ipv6Address
RplDaoAckHeader::GetDodagId() const
{
    return m_dodagId;
}

std::ostream&
operator<<(std::ostream& os, const RplDaoAckHeader& h)
{
    h.Print(os);
    return os;
}

} // namespace ns3
