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

/*
 * RplDodagConfigOption
 */

NS_OBJECT_ENSURE_REGISTERED(RplDodagConfigOption);

RplDodagConfigOption::RplDodagConfigOption()
    : m_flagsPcs(0),
      m_dioIntervalDoublings(20),
      m_dioIntervalMin(3),
      m_dioRedundancyConstant(10),
      m_maxRankIncrease(0),
      m_minHopRankIncrease(0x100),
      m_ocp(0),
      m_reserved(0),
      m_defaultLifetime(0xff),
      m_lifetimeUnit(0xffff)
{
}

TypeId
RplDodagConfigOption::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplDodagConfigOption")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<RplDodagConfigOption>();
    return tid;
}

TypeId
RplDodagConfigOption::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplDodagConfigOption::GetSerializedSize() const
{
    return 16; // Type + Length + 14 octets
}

void
RplDodagConfigOption::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(RPL_OPT_DODAG_CONFIGURATION);
    i.WriteU8(14);
    i.WriteU8(m_flagsPcs);
    i.WriteU8(m_dioIntervalDoublings);
    i.WriteU8(m_dioIntervalMin);
    i.WriteU8(m_dioRedundancyConstant);
    i.WriteHtonU16(m_maxRankIncrease);
    i.WriteHtonU16(m_minHopRankIncrease);
    i.WriteHtonU16(m_ocp);
    i.WriteU8(m_reserved);
    i.WriteU8(m_defaultLifetime);
    i.WriteHtonU16(m_lifetimeUnit);
}

uint32_t
RplDodagConfigOption::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8();
    uint8_t length = i.ReadU8();
    if (type != RPL_OPT_DODAG_CONFIGURATION || length != 14)
    {
        return 0;
    }
    m_flagsPcs = i.ReadU8();
    m_dioIntervalDoublings = i.ReadU8();
    m_dioIntervalMin = i.ReadU8();
    m_dioRedundancyConstant = i.ReadU8();
    m_maxRankIncrease = i.ReadNtohU16();
    m_minHopRankIncrease = i.ReadNtohU16();
    m_ocp = i.ReadNtohU16();
    m_reserved = i.ReadU8();
    m_defaultLifetime = i.ReadU8();
    m_lifetimeUnit = i.ReadNtohU16();
    return GetSerializedSize();
}

void
RplDodagConfigOption::Print(std::ostream& os) const
{
    os << "DODAG-Config A=" << GetAuthenticationEnabled() << " PCS=" << int(GetPathControlSize())
       << " IntDbl=" << int(m_dioIntervalDoublings) << " IntMin=" << int(m_dioIntervalMin)
       << " Redun=" << int(m_dioRedundancyConstant) << " OCP=" << m_ocp;
}

void
RplDodagConfigOption::SetAuthenticationEnabled(bool enabled)
{
    if (enabled)
    {
        m_flagsPcs |= 0x08;
    }
    else
    {
        m_flagsPcs &= ~0x08;
    }
}

bool
RplDodagConfigOption::GetAuthenticationEnabled() const
{
    return (m_flagsPcs & 0x08) != 0;
}

void
RplDodagConfigOption::SetPathControlSize(uint8_t pcs)
{
    m_flagsPcs = (m_flagsPcs & 0xf8) | (pcs & 0x07);
}

uint8_t
RplDodagConfigOption::GetPathControlSize() const
{
    return m_flagsPcs & 0x07;
}

void
RplDodagConfigOption::SetDioIntervalDoublings(uint8_t value)
{
    m_dioIntervalDoublings = value;
}

uint8_t
RplDodagConfigOption::GetDioIntervalDoublings() const
{
    return m_dioIntervalDoublings;
}

void
RplDodagConfigOption::SetDioIntervalMin(uint8_t value)
{
    m_dioIntervalMin = value;
}

uint8_t
RplDodagConfigOption::GetDioIntervalMin() const
{
    return m_dioIntervalMin;
}

void
RplDodagConfigOption::SetDioRedundancyConstant(uint8_t value)
{
    m_dioRedundancyConstant = value;
}

uint8_t
RplDodagConfigOption::GetDioRedundancyConstant() const
{
    return m_dioRedundancyConstant;
}

void
RplDodagConfigOption::SetMaxRankIncrease(uint16_t value)
{
    m_maxRankIncrease = value;
}

uint16_t
RplDodagConfigOption::GetMaxRankIncrease() const
{
    return m_maxRankIncrease;
}

void
RplDodagConfigOption::SetMinHopRankIncrease(uint16_t value)
{
    m_minHopRankIncrease = value;
}

uint16_t
RplDodagConfigOption::GetMinHopRankIncrease() const
{
    return m_minHopRankIncrease;
}

void
RplDodagConfigOption::SetOcp(uint16_t ocp)
{
    m_ocp = ocp;
}

uint16_t
RplDodagConfigOption::GetOcp() const
{
    return m_ocp;
}

void
RplDodagConfigOption::SetDefaultLifetime(uint8_t lifetime)
{
    m_defaultLifetime = lifetime;
}

uint8_t
RplDodagConfigOption::GetDefaultLifetime() const
{
    return m_defaultLifetime;
}

void
RplDodagConfigOption::SetLifetimeUnit(uint16_t unit)
{
    m_lifetimeUnit = unit;
}

uint16_t
RplDodagConfigOption::GetLifetimeUnit() const
{
    return m_lifetimeUnit;
}

std::ostream&
operator<<(std::ostream& os, const RplDodagConfigOption& h)
{
    h.Print(os);
    return os;
}

/*
 * RplPrefixInfoOption
 */

NS_OBJECT_ENSURE_REGISTERED(RplPrefixInfoOption);

RplPrefixInfoOption::RplPrefixInfoOption()
    : m_prefixLength(0),
      m_flags(0),
      m_validLifetime(0xffffffff),
      m_preferredLifetime(0xffffffff),
      m_prefix(Ipv6Address::GetZero())
{
}

TypeId
RplPrefixInfoOption::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplPrefixInfoOption")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<RplPrefixInfoOption>();
    return tid;
}

TypeId
RplPrefixInfoOption::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplPrefixInfoOption::GetSerializedSize() const
{
    return 32; // Type + Length + 30 octets
}

void
RplPrefixInfoOption::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(RPL_OPT_PREFIX_INFORMATION);
    i.WriteU8(30);
    i.WriteU8(m_prefixLength);
    i.WriteU8(m_flags);
    i.WriteHtonU32(m_validLifetime);
    i.WriteHtonU32(m_preferredLifetime);
    i.WriteHtonU32(0); // Reserved2

    uint8_t buf[16];
    m_prefix.Serialize(buf);
    i.Write(buf, 16);
}

uint32_t
RplPrefixInfoOption::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8();
    uint8_t length = i.ReadU8();
    if (type != RPL_OPT_PREFIX_INFORMATION || length != 30)
    {
        return 0;
    }
    m_prefixLength = i.ReadU8();
    m_flags = i.ReadU8();
    m_validLifetime = i.ReadNtohU32();
    m_preferredLifetime = i.ReadNtohU32();
    i.ReadNtohU32(); // Reserved2

    uint8_t buf[16];
    i.Read(buf, 16);
    m_prefix.Set(buf);

    return GetSerializedSize();
}

void
RplPrefixInfoOption::Print(std::ostream& os) const
{
    os << "Prefix-Info " << m_prefix << "/" << int(m_prefixLength) << " L=" << GetOnLink()
       << " A=" << GetAutonomous() << " R=" << GetRouterAddress();
}

void
RplPrefixInfoOption::SetPrefixLength(uint8_t prefixLength)
{
    m_prefixLength = prefixLength;
}

uint8_t
RplPrefixInfoOption::GetPrefixLength() const
{
    return m_prefixLength;
}

void
RplPrefixInfoOption::SetOnLink(bool onLink)
{
    if (onLink)
    {
        m_flags |= 0x80;
    }
    else
    {
        m_flags &= ~0x80;
    }
}

bool
RplPrefixInfoOption::GetOnLink() const
{
    return (m_flags & 0x80) != 0;
}

void
RplPrefixInfoOption::SetAutonomous(bool autonomous)
{
    if (autonomous)
    {
        m_flags |= 0x40;
    }
    else
    {
        m_flags &= ~0x40;
    }
}

bool
RplPrefixInfoOption::GetAutonomous() const
{
    return (m_flags & 0x40) != 0;
}

void
RplPrefixInfoOption::SetRouterAddress(bool routerAddress)
{
    if (routerAddress)
    {
        m_flags |= 0x20;
    }
    else
    {
        m_flags &= ~0x20;
    }
}

bool
RplPrefixInfoOption::GetRouterAddress() const
{
    return (m_flags & 0x20) != 0;
}

void
RplPrefixInfoOption::SetValidLifetime(uint32_t lifetime)
{
    m_validLifetime = lifetime;
}

uint32_t
RplPrefixInfoOption::GetValidLifetime() const
{
    return m_validLifetime;
}

void
RplPrefixInfoOption::SetPreferredLifetime(uint32_t lifetime)
{
    m_preferredLifetime = lifetime;
}

uint32_t
RplPrefixInfoOption::GetPreferredLifetime() const
{
    return m_preferredLifetime;
}

void
RplPrefixInfoOption::SetPrefix(Ipv6Address prefix)
{
    m_prefix = prefix;
}

Ipv6Address
RplPrefixInfoOption::GetPrefix() const
{
    return m_prefix;
}

std::ostream&
operator<<(std::ostream& os, const RplPrefixInfoOption& h)
{
    h.Print(os);
    return os;
}

/*
 * RplTargetOption
 */

NS_OBJECT_ENSURE_REGISTERED(RplTargetOption);

RplTargetOption::RplTargetOption()
    : m_flags(0),
      m_prefixLength(128),
      m_target(Ipv6Address::GetZero())
{
}

TypeId
RplTargetOption::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplTargetOption")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<RplTargetOption>();
    return tid;
}

TypeId
RplTargetOption::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplTargetOption::GetSerializedSize() const
{
    // Type + Length + Flags + Prefix Length + 16-byte Target
    return 20;
}

void
RplTargetOption::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(RPL_OPT_RPL_TARGET);
    i.WriteU8(18); // Flags + Prefix Length + Target
    i.WriteU8(m_flags);
    i.WriteU8(m_prefixLength);

    uint8_t buf[16];
    m_target.Serialize(buf);
    i.Write(buf, 16);
}

uint32_t
RplTargetOption::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8();
    uint8_t length = i.ReadU8();
    if (type != RPL_OPT_RPL_TARGET || length < 2)
    {
        return 0;
    }
    m_flags = i.ReadU8();
    m_prefixLength = i.ReadU8();

    uint8_t buf[16] = {};
    uint8_t targetBytes = length - 2;
    if (targetBytes > 16)
    {
        return 0;
    }
    i.Read(buf, targetBytes);
    m_target.Set(buf);

    return 2 + length;
}

void
RplTargetOption::Print(std::ostream& os) const
{
    os << "Target " << m_target << "/" << int(m_prefixLength);
}

void
RplTargetOption::SetPrefixLength(uint8_t prefixLength)
{
    m_prefixLength = prefixLength;
}

uint8_t
RplTargetOption::GetPrefixLength() const
{
    return m_prefixLength;
}

void
RplTargetOption::SetTarget(Ipv6Address target)
{
    m_target = target;
}

Ipv6Address
RplTargetOption::GetTarget() const
{
    return m_target;
}

std::ostream&
operator<<(std::ostream& os, const RplTargetOption& h)
{
    h.Print(os);
    return os;
}

/*
 * RplTransitInfoOption
 */

NS_OBJECT_ENSURE_REGISTERED(RplTransitInfoOption);

RplTransitInfoOption::RplTransitInfoOption()
    : m_flags(0),
      m_pathControl(0),
      m_pathSequence(0),
      m_pathLifetime(0),
      m_hasParent(false),
      m_parentAddress(Ipv6Address::GetZero())
{
}

TypeId
RplTransitInfoOption::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplTransitInfoOption")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<RplTransitInfoOption>();
    return tid;
}

TypeId
RplTransitInfoOption::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplTransitInfoOption::GetSerializedSize() const
{
    return m_hasParent ? 20 : 6;
}

void
RplTransitInfoOption::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(RPL_OPT_TRANSIT_INFORMATION);
    i.WriteU8(m_hasParent ? 18 : 4);
    i.WriteU8(m_flags);
    i.WriteU8(m_pathControl);
    i.WriteU8(m_pathSequence);
    i.WriteU8(m_pathLifetime);

    if (m_hasParent)
    {
        uint8_t buf[16];
        m_parentAddress.Serialize(buf);
        i.Write(buf, 16);
    }
}

uint32_t
RplTransitInfoOption::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8();
    uint8_t length = i.ReadU8();
    if (type != RPL_OPT_TRANSIT_INFORMATION || (length != 4 && length != 18))
    {
        return 0;
    }
    m_flags = i.ReadU8();
    m_pathControl = i.ReadU8();
    m_pathSequence = i.ReadU8();
    m_pathLifetime = i.ReadU8();
    m_hasParent = (length == 18);
    if (m_hasParent)
    {
        uint8_t buf[16];
        i.Read(buf, 16);
        m_parentAddress.Set(buf);
    }
    return GetSerializedSize();
}

void
RplTransitInfoOption::Print(std::ostream& os) const
{
    os << "Transit E=" << GetExternal() << " PC=" << int(m_pathControl)
       << " PS=" << int(m_pathSequence) << " PL=" << int(m_pathLifetime);
    if (m_hasParent)
    {
        os << " parent=" << m_parentAddress;
    }
}

void
RplTransitInfoOption::SetExternal(bool external)
{
    if (external)
    {
        m_flags |= 0x80;
    }
    else
    {
        m_flags &= ~0x80;
    }
}

bool
RplTransitInfoOption::GetExternal() const
{
    return (m_flags & 0x80) != 0;
}

void
RplTransitInfoOption::SetPathControl(uint8_t pathControl)
{
    m_pathControl = pathControl;
}

uint8_t
RplTransitInfoOption::GetPathControl() const
{
    return m_pathControl;
}

void
RplTransitInfoOption::SetPathSequence(uint8_t pathSequence)
{
    m_pathSequence = pathSequence;
}

uint8_t
RplTransitInfoOption::GetPathSequence() const
{
    return m_pathSequence;
}

void
RplTransitInfoOption::SetPathLifetime(uint8_t pathLifetime)
{
    m_pathLifetime = pathLifetime;
}

uint8_t
RplTransitInfoOption::GetPathLifetime() const
{
    return m_pathLifetime;
}

void
RplTransitInfoOption::SetParentAddress(Ipv6Address parent)
{
    m_parentAddress = parent;
    m_hasParent = true;
}

Ipv6Address
RplTransitInfoOption::GetParentAddress() const
{
    return m_parentAddress;
}

bool
RplTransitInfoOption::HasParentAddress() const
{
    return m_hasParent;
}

void
RplTransitInfoOption::ClearParentAddress()
{
    m_hasParent = false;
    m_parentAddress = Ipv6Address::GetZero();
}

std::ostream&
operator<<(std::ostream& os, const RplTransitInfoOption& h)
{
    h.Print(os);
    return os;
}

} // namespace ns3
