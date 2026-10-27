#include "rpl-header.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RplHeader");

/*
 * DisBaseObjectHeader — \RFC{6550} Section 6.2 / 6.2.1 (Figure 13)
 */

NS_OBJECT_ENSURE_REGISTERED(DisBaseObjectHeader);

DisBaseObjectHeader::DisBaseObjectHeader()
{
}

TypeId
DisBaseObjectHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DisBaseObjectHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<DisBaseObjectHeader>();
    return tid;
}

TypeId
DisBaseObjectHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DisBaseObjectHeader::GetSerializedSize() const
{
    return 2;
}

void
DisBaseObjectHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(0); // Flags
    i.WriteU8(0); // Reserved
}

uint32_t
DisBaseObjectHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    i.ReadU8();
    i.ReadU8();
    return GetSerializedSize();
}

void
DisBaseObjectHeader::Print(std::ostream& os) const
{
    os << "DIS Base Object";
}

std::ostream&
operator<<(std::ostream& os, const DisBaseObjectHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * DioBaseObjectHeader — \RFC{6550} Section 6.3 / 6.3.1 (Figure 14)
 */

NS_OBJECT_ENSURE_REGISTERED(DioBaseObjectHeader);

DioBaseObjectHeader::DioBaseObjectHeader()
    : m_rplInstanceId(0),
      m_versionNumber(0),
      m_rank(0xffff),
      m_gMopPrf(0),
      m_dtsn(0),
      m_dodagId(Ipv6Address::GetZero())
{
}

TypeId
DioBaseObjectHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DioBaseObjectHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<DioBaseObjectHeader>();
    return tid;
}

TypeId
DioBaseObjectHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DioBaseObjectHeader::GetSerializedSize() const
{
    return 24;
}

void
DioBaseObjectHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_rplInstanceId);
    i.WriteU8(m_versionNumber);
    i.WriteHtonU16(m_rank);
    i.WriteU8(m_gMopPrf & 0xbf); // clear reserved bit next to G
    i.WriteU8(m_dtsn);
    i.WriteU8(0); // Flags
    i.WriteU8(0); // Reserved

    uint8_t buf[16];
    m_dodagId.Serialize(buf);
    i.Write(buf, 16);
}

uint32_t
DioBaseObjectHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_rplInstanceId = i.ReadU8();
    m_versionNumber = i.ReadU8();
    m_rank = i.ReadNtohU16();
    m_gMopPrf = i.ReadU8() & 0xbf;
    m_dtsn = i.ReadU8();
    i.ReadU8(); // Flags
    i.ReadU8(); // Reserved

    uint8_t buf[16];
    i.Read(buf, 16);
    m_dodagId.Set(buf);

    return GetSerializedSize();
}

void
DioBaseObjectHeader::Print(std::ostream& os) const
{
    os << "DIO Base Object"
       << " RPLInstanceID=" << int(m_rplInstanceId)
       << " VersionNumber=" << int(m_versionNumber) << " Rank=" << m_rank
       << " G=" << IsGrounded()
       << " MOP=" << int(static_cast<uint8_t>(GetModeOfOperation()))
       << " Prf=" << int(GetDodagPreference()) << " DTSN=" << int(m_dtsn)
       << " DODAGID=" << m_dodagId;
}

void
DioBaseObjectHeader::SetRplInstanceId(uint8_t rplInstanceId)
{
    m_rplInstanceId = rplInstanceId;
}

uint8_t
DioBaseObjectHeader::GetRplInstanceId() const
{
    return m_rplInstanceId;
}

void
DioBaseObjectHeader::SetVersionNumber(uint8_t versionNumber)
{
    m_versionNumber = versionNumber;
}

uint8_t
DioBaseObjectHeader::GetVersionNumber() const
{
    return m_versionNumber;
}

void
DioBaseObjectHeader::SetRank(uint16_t rank)
{
    m_rank = rank;
}

uint16_t
DioBaseObjectHeader::GetRank() const
{
    return m_rank;
}

void
DioBaseObjectHeader::SetGrounded(bool grounded)
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
DioBaseObjectHeader::IsGrounded() const
{
    return (m_gMopPrf & 0x80) != 0;
}

void
DioBaseObjectHeader::SetModeOfOperation(ModeOfOperation mop)
{
    m_gMopPrf = (m_gMopPrf & 0x87) | ((static_cast<uint8_t>(mop) & 0x07) << 3);
}

ModeOfOperation
DioBaseObjectHeader::GetModeOfOperation() const
{
    return static_cast<ModeOfOperation>((m_gMopPrf >> 3) & 0x07);
}

void
DioBaseObjectHeader::SetDodagPreference(uint8_t preference)
{
    m_gMopPrf = (m_gMopPrf & 0xf8) | (preference & 0x07);
}

uint8_t
DioBaseObjectHeader::GetDodagPreference() const
{
    return m_gMopPrf & 0x07;
}

void
DioBaseObjectHeader::SetDtsn(uint8_t dtsn)
{
    m_dtsn = dtsn;
}

uint8_t
DioBaseObjectHeader::GetDtsn() const
{
    return m_dtsn;
}

void
DioBaseObjectHeader::SetDodagId(Ipv6Address dodagId)
{
    m_dodagId = dodagId;
}

Ipv6Address
DioBaseObjectHeader::GetDodagId() const
{
    return m_dodagId;
}

std::ostream&
operator<<(std::ostream& os, const DioBaseObjectHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * DaoBaseObjectHeader — \RFC{6550} Section 6.4 / 6.4.1 (Figure 16)
 */

NS_OBJECT_ENSURE_REGISTERED(DaoBaseObjectHeader);

DaoBaseObjectHeader::DaoBaseObjectHeader()
    : m_rplInstanceId(0),
      m_k(false),
      m_d(false),
      m_daoSequence(0),
      m_dodagId(Ipv6Address::GetZero())
{
}

TypeId
DaoBaseObjectHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DaoBaseObjectHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<DaoBaseObjectHeader>();
    return tid;
}

TypeId
DaoBaseObjectHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DaoBaseObjectHeader::GetSerializedSize() const
{
    return m_d ? 20 : 4;
}

void
DaoBaseObjectHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    uint8_t kdFlags = 0;
    if (m_k)
    {
        kdFlags |= 0x80;
    }
    if (m_d)
    {
        kdFlags |= 0x40;
    }

    i.WriteU8(m_rplInstanceId);
    i.WriteU8(kdFlags);
    i.WriteU8(0); // Reserved
    i.WriteU8(m_daoSequence);

    if (m_d)
    {
        uint8_t buf[16];
        m_dodagId.Serialize(buf);
        i.Write(buf, 16);
    }
}

uint32_t
DaoBaseObjectHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_rplInstanceId = i.ReadU8();
    uint8_t kdFlags = i.ReadU8();
    m_k = (kdFlags & 0x80) != 0;
    m_d = (kdFlags & 0x40) != 0;
    i.ReadU8(); // Reserved
    m_daoSequence = i.ReadU8();

    if (m_d)
    {
        uint8_t buf[16];
        i.Read(buf, 16);
        m_dodagId.Set(buf);
    }

    return GetSerializedSize();
}

void
DaoBaseObjectHeader::Print(std::ostream& os) const
{
    os << "DAO Base Object"
       << " RPLInstanceID=" << int(m_rplInstanceId) << " K=" << m_k << " D=" << m_d
       << " DAOSequence=" << int(m_daoSequence);
    if (m_d)
    {
        os << " DODAGID=" << m_dodagId;
    }
}

void
DaoBaseObjectHeader::SetRplInstanceId(uint8_t rplInstanceId)
{
    m_rplInstanceId = rplInstanceId;
}

uint8_t
DaoBaseObjectHeader::GetRplInstanceId() const
{
    return m_rplInstanceId;
}

void
DaoBaseObjectHeader::SetK(bool k)
{
    m_k = k;
}

bool
DaoBaseObjectHeader::GetK() const
{
    return m_k;
}

void
DaoBaseObjectHeader::SetD(bool d)
{
    m_d = d;
}

bool
DaoBaseObjectHeader::GetD() const
{
    return m_d;
}

void
DaoBaseObjectHeader::SetDaoSequence(uint8_t daoSequence)
{
    m_daoSequence = daoSequence;
}

uint8_t
DaoBaseObjectHeader::GetDaoSequence() const
{
    return m_daoSequence;
}

void
DaoBaseObjectHeader::SetDodagId(Ipv6Address dodagId)
{
    m_dodagId = dodagId;
    m_d = true;
}

Ipv6Address
DaoBaseObjectHeader::GetDodagId() const
{
    return m_dodagId;
}

std::ostream&
operator<<(std::ostream& os, const DaoBaseObjectHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * DaoAckBaseObjectHeader — \RFC{6550} Section 6.5 / 6.5.1 (Figure 17)
 */

NS_OBJECT_ENSURE_REGISTERED(DaoAckBaseObjectHeader);

DaoAckBaseObjectHeader::DaoAckBaseObjectHeader()
    : m_rplInstanceId(0),
      m_d(false),
      m_daoSequence(0),
      m_status(static_cast<uint8_t>(DaoAckStatus::UNQUALIFIED_ACCEPTANCE)),
      m_dodagId(Ipv6Address::GetZero())
{
}

TypeId
DaoAckBaseObjectHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DaoAckBaseObjectHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<DaoAckBaseObjectHeader>();
    return tid;
}

TypeId
DaoAckBaseObjectHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DaoAckBaseObjectHeader::GetSerializedSize() const
{
    return m_d ? 20 : 4;
}

void
DaoAckBaseObjectHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_rplInstanceId);
    i.WriteU8(m_d ? 0x80 : 0x00);
    i.WriteU8(m_daoSequence);
    i.WriteU8(m_status);

    if (m_d)
    {
        uint8_t buf[16];
        m_dodagId.Serialize(buf);
        i.Write(buf, 16);
    }
}

uint32_t
DaoAckBaseObjectHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_rplInstanceId = i.ReadU8();
    m_d = (i.ReadU8() & 0x80) != 0;
    m_daoSequence = i.ReadU8();
    m_status = i.ReadU8();

    if (m_d)
    {
        uint8_t buf[16];
        i.Read(buf, 16);
        m_dodagId.Set(buf);
    }

    return GetSerializedSize();
}

void
DaoAckBaseObjectHeader::Print(std::ostream& os) const
{
    os << "DAO-ACK Base Object"
       << " RPLInstanceID=" << int(m_rplInstanceId) << " D=" << m_d
       << " DAOSequence=" << int(m_daoSequence) << " Status=" << int(m_status);
    if (m_d)
    {
        os << " DODAGID=" << m_dodagId;
    }
}

void
DaoAckBaseObjectHeader::SetRplInstanceId(uint8_t rplInstanceId)
{
    m_rplInstanceId = rplInstanceId;
}

uint8_t
DaoAckBaseObjectHeader::GetRplInstanceId() const
{
    return m_rplInstanceId;
}

void
DaoAckBaseObjectHeader::SetD(bool d)
{
    m_d = d;
}

bool
DaoAckBaseObjectHeader::GetD() const
{
    return m_d;
}

void
DaoAckBaseObjectHeader::SetDaoSequence(uint8_t daoSequence)
{
    m_daoSequence = daoSequence;
}

uint8_t
DaoAckBaseObjectHeader::GetDaoSequence() const
{
    return m_daoSequence;
}

void
DaoAckBaseObjectHeader::SetStatus(DaoAckStatus status)
{
    m_status = static_cast<uint8_t>(status);
}

void
DaoAckBaseObjectHeader::SetStatus(uint8_t status)
{
    m_status = status;
}

uint8_t
DaoAckBaseObjectHeader::GetStatus() const
{
    return m_status;
}

void
DaoAckBaseObjectHeader::SetDodagId(Ipv6Address dodagId)
{
    m_dodagId = dodagId;
    m_d = true;
}

Ipv6Address
DaoAckBaseObjectHeader::GetDodagId() const
{
    return m_dodagId;
}

std::ostream&
operator<<(std::ostream& os, const DaoAckBaseObjectHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * RplTargetOptionHeader — \RFC{6550} Section 6.7.7 (Figure 28)
 */

NS_OBJECT_ENSURE_REGISTERED(RplTargetOptionHeader);

RplTargetOptionHeader::RplTargetOptionHeader()
    : m_prefixLength(128),
      m_target(Ipv6Address::GetZero())
{
}

TypeId
RplTargetOptionHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RplTargetOptionHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<RplTargetOptionHeader>();
    return tid;
}

TypeId
RplTargetOptionHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RplTargetOptionHeader::GetSerializedSize() const
{
    // Type(1) + Length(1) + Flags(1) + PrefixLength(1) + prefix bytes
    return 4 + (m_prefixLength + 7) / 8;
}

void
RplTargetOptionHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    const uint8_t prefixBytes = (m_prefixLength + 7) / 8;

    i.WriteU8(static_cast<uint8_t>(DaoOptions::RPL_TARGET));
    i.WriteU8(2 + prefixBytes); // Option Length
    i.WriteU8(0);               // Flags
    i.WriteU8(m_prefixLength);

    uint8_t buf[16];
    m_target.Serialize(buf);
    i.Write(buf, prefixBytes);
}

uint32_t
RplTargetOptionHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    i.ReadU8(); // Type
    i.ReadU8(); // Option Length
    i.ReadU8(); // Flags
    m_prefixLength = i.ReadU8();
    if (m_prefixLength > 128)
    {
        m_prefixLength = 128;
    }

    uint8_t buf[16] = {0};
    i.Read(buf, (m_prefixLength + 7) / 8);
    m_target.Set(buf);
    return GetSerializedSize();
}

void
RplTargetOptionHeader::Print(std::ostream& os) const
{
    os << "RPL Target " << m_target << "/" << int(m_prefixLength);
}

void
RplTargetOptionHeader::SetTarget(Ipv6Address target, uint8_t prefixLength)
{
    m_prefixLength = prefixLength > 128 ? 128 : prefixLength;
    m_target = target.CombinePrefix(Ipv6Prefix(m_prefixLength));
}

Ipv6Address
RplTargetOptionHeader::GetTarget() const
{
    return m_target;
}

uint8_t
RplTargetOptionHeader::GetPrefixLength() const
{
    return m_prefixLength;
}

std::ostream&
operator<<(std::ostream& os, const RplTargetOptionHeader& h)
{
    h.Print(os);
    return os;
}

/*
 * TransitInformationOptionHeader — \RFC{6550} Section 6.7.8 (Figure 29)
 */

NS_OBJECT_ENSURE_REGISTERED(TransitInformationOptionHeader);

TransitInformationOptionHeader::TransitInformationOptionHeader()
    : m_external(false),
      m_pathControl(0),
      m_pathSequence(0),
      m_pathLifetime(0xFF)
{
}

TypeId
TransitInformationOptionHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TransitInformationOptionHeader")
                            .SetParent<Header>()
                            .SetGroupName("Rpl")
                            .AddConstructor<TransitInformationOptionHeader>();
    return tid;
}

TypeId
TransitInformationOptionHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
TransitInformationOptionHeader::GetSerializedSize() const
{
    return 6;
}

void
TransitInformationOptionHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(static_cast<uint8_t>(DaoOptions::TRANSIT_INFORMATION));
    i.WriteU8(4); // Option Length (no Parent Address)
    i.WriteU8(m_external ? 0x80 : 0x00);
    i.WriteU8(m_pathControl);
    i.WriteU8(m_pathSequence);
    i.WriteU8(m_pathLifetime);
}

uint32_t
TransitInformationOptionHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    i.ReadU8(); // Type
    i.ReadU8(); // Option Length
    m_external = (i.ReadU8() & 0x80) != 0;
    m_pathControl = i.ReadU8();
    m_pathSequence = i.ReadU8();
    m_pathLifetime = i.ReadU8();
    return GetSerializedSize();
}

void
TransitInformationOptionHeader::Print(std::ostream& os) const
{
    os << "Transit Information E=" << m_external << " PathControl=" << int(m_pathControl)
       << " PathSequence=" << int(m_pathSequence) << " PathLifetime=" << int(m_pathLifetime);
}

void
TransitInformationOptionHeader::SetExternal(bool external)
{
    m_external = external;
}

bool
TransitInformationOptionHeader::IsExternal() const
{
    return m_external;
}

void
TransitInformationOptionHeader::SetPathControl(uint8_t pathControl)
{
    m_pathControl = pathControl;
}

uint8_t
TransitInformationOptionHeader::GetPathControl() const
{
    return m_pathControl;
}

void
TransitInformationOptionHeader::SetPathSequence(uint8_t pathSequence)
{
    m_pathSequence = pathSequence;
}

uint8_t
TransitInformationOptionHeader::GetPathSequence() const
{
    return m_pathSequence;
}

void
TransitInformationOptionHeader::SetPathLifetime(uint8_t pathLifetime)
{
    m_pathLifetime = pathLifetime;
}

uint8_t
TransitInformationOptionHeader::GetPathLifetime() const
{
    return m_pathLifetime;
}

std::ostream&
operator<<(std::ostream& os, const TransitInformationOptionHeader& h)
{
    h.Print(os);
    return os;
}

} // namespace ns3
