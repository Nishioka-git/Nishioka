/*
 * Copyright (c) 2014 Universita' di Firenze, Italy
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

#ifndef RPL_HEADER_H
#define RPL_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv6-address.h"

namespace ns3
{

/**
 * @ingroup rpl
 * @brief ICMPv6 Code values for RPL control messages (\RFC{6550} Section 6)
 *
 * RPL messages are carried in ICMPv6 with Type = 155.
 */
enum RplCode : uint8_t
{
    RPL_DIS = 0x00,     //!< DODAG Information Solicitation
    RPL_DIO = 0x01,     //!< DODAG Information Object
    RPL_DAO = 0x02,     //!< Destination Advertisement Object
    RPL_DAO_ACK = 0x03, //!< Destination Advertisement Object Acknowledgement
    RPL_CC = 0x04,      //!< Consistency Check
};

/**
 * @ingroup rpl
 * @brief RPL Control Message Options (\RFC{6550} Section 6.7)
 */
enum RplOptionType : uint8_t
{
    RPL_OPT_PAD1 = 0x00,
    RPL_OPT_PADN = 0x01,
    RPL_OPT_DAG_METRIC_CONTAINER = 0x02,
    RPL_OPT_ROUTE_INFORMATION = 0x03,
    RPL_OPT_DODAG_CONFIGURATION = 0x04,
    RPL_OPT_RPL_TARGET = 0x05,
    RPL_OPT_TRANSIT_INFORMATION = 0x06,
    RPL_OPT_SOLICITED_INFORMATION = 0x07,
    RPL_OPT_PREFIX_INFORMATION = 0x08,
    RPL_OPT_RPL_TARGET_DESC = 0x09,
};

/**
 * @ingroup rpl
 * @brief DIS (DODAG Information Solicitation) base header
 *
 * \verbatim
  0                   1                   2
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     Flags     |   Reserved    |   Option(s)...
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class RplDisHeader : public Header
{
  public:
    RplDisHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetFlags(uint8_t flags);
    uint8_t GetFlags() const;

  private:
    uint8_t m_flags;    //!< Flags (unused in RFC 6550, set to 0)
    uint8_t m_reserved; //!< Reserved
};

std::ostream& operator<<(std::ostream& os, const RplDisHeader& h);

/**
 * @ingroup rpl
 * @brief DIO (DODAG Information Object) base header
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | RPLInstanceID |Version Number |             Rank              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |G|0| MOP | Prf |     DTSN      |     Flags     |   Reserved    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                            DODAGID                            |
 |                                                               |
 |                                                               |
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Option(s)...
 +-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class RplDioHeader : public Header
{
  public:
    RplDioHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetInstanceId(uint8_t instanceId);
    uint8_t GetInstanceId() const;

    void SetVersion(uint8_t version);
    uint8_t GetVersion() const;

    void SetRank(uint16_t rank);
    uint16_t GetRank() const;

    void SetGrounded(bool grounded);
    bool GetGrounded() const;

    void SetMop(uint8_t mop);
    uint8_t GetMop() const;

    void SetPreference(uint8_t preference);
    uint8_t GetPreference() const;

    void SetDtsn(uint8_t dtsn);
    uint8_t GetDtsn() const;

    void SetFlags(uint8_t flags);
    uint8_t GetFlags() const;

    void SetDodagId(Ipv6Address dodagId);
    Ipv6Address GetDodagId() const;

  private:
    uint8_t m_instanceId; //!< RPLInstanceID
    uint8_t m_version;    //!< Version Number
    uint16_t m_rank;      //!< Rank
    uint8_t m_gMopPrf;    //!< G | 0 | MOP | Prf
    uint8_t m_dtsn;       //!< Destination Advertisement Trigger Sequence Number
    uint8_t m_flags;      //!< Flags
    uint8_t m_reserved;   //!< Reserved
    Ipv6Address m_dodagId; //!< DODAGID
};

std::ostream& operator<<(std::ostream& os, const RplDioHeader& h);

/**
 * @ingroup rpl
 * @brief DAO (Destination Advertisement Object) base header
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | RPLInstanceID |K|D|   Flags   |   Reserved    | DAOSequence   |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               |
 +                                                               +
 |                            DODAGID*                           |
 +                                                               +
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Option(s)...
 +-+-+-+-+-+-+-+-+
 * \endverbatim
 *
 * DODAGID is present when the D flag is set.
 */
class RplDaoHeader : public Header
{
  public:
    RplDaoHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetInstanceId(uint8_t instanceId);
    uint8_t GetInstanceId() const;

    void SetAckRequired(bool k);
    bool GetAckRequired() const;

    void SetDodagIdPresent(bool d);
    bool GetDodagIdPresent() const;

    void SetFlags(uint8_t flags);
    uint8_t GetFlags() const;

    void SetDaoSequence(uint8_t seq);
    uint8_t GetDaoSequence() const;

    void SetDodagId(Ipv6Address dodagId);
    Ipv6Address GetDodagId() const;

  private:
    uint8_t m_instanceId;  //!< RPLInstanceID
    uint8_t m_flags;       //!< K | D | Flags
    uint8_t m_reserved;    //!< Reserved
    uint8_t m_daoSequence; //!< DAOSequence
    Ipv6Address m_dodagId; //!< DODAGID (when D is set)
};

std::ostream& operator<<(std::ostream& os, const RplDaoHeader& h);

/**
 * @ingroup rpl
 * @brief DAO-ACK base header
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | RPLInstanceID |D|  Reserved   |  DAOSequence  |    Status     |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               |
 +                                                               +
 |                            DODAGID*                           |
 +                                                               +
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Option(s)...
 +-+-+-+-+-+-+-+-+
 * \endverbatim
 *
 * DODAGID is present when the D flag is set.
 */
class RplDaoAckHeader : public Header
{
  public:
    RplDaoAckHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetInstanceId(uint8_t instanceId);
    uint8_t GetInstanceId() const;

    void SetDodagIdPresent(bool d);
    bool GetDodagIdPresent() const;

    void SetDaoSequence(uint8_t seq);
    uint8_t GetDaoSequence() const;

    void SetStatus(uint8_t status);
    uint8_t GetStatus() const;

    void SetDodagId(Ipv6Address dodagId);
    Ipv6Address GetDodagId() const;

  private:
    uint8_t m_instanceId;  //!< RPLInstanceID
    uint8_t m_flags;       //!< D | Reserved
    uint8_t m_daoSequence; //!< DAOSequence
    uint8_t m_status;      //!< Status
    Ipv6Address m_dodagId; //!< DODAGID (when D is set)
};

std::ostream& operator<<(std::ostream& os, const RplDaoAckHeader& h);

/**
 * @ingroup rpl
 * @brief DODAG Configuration Option (\RFC{6550} Section 6.7.6)
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Type = 4    |Opt Length = 14| Flags |A| PCS | DIOIntDoubl.  |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  DIOIntMin.   |   DIORedun.   |        MaxRankIncrease        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |      MinHopRankIncrease       |              OCP              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Reserved    | Def. Lifetime |      Lifetime Unit            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class RplDodagConfigOption : public Header
{
  public:
    RplDodagConfigOption();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetAuthenticationEnabled(bool enabled);
    bool GetAuthenticationEnabled() const;

    void SetPathControlSize(uint8_t pcs);
    uint8_t GetPathControlSize() const;

    void SetDioIntervalDoublings(uint8_t value);
    uint8_t GetDioIntervalDoublings() const;

    void SetDioIntervalMin(uint8_t value);
    uint8_t GetDioIntervalMin() const;

    void SetDioRedundancyConstant(uint8_t value);
    uint8_t GetDioRedundancyConstant() const;

    void SetMaxRankIncrease(uint16_t value);
    uint16_t GetMaxRankIncrease() const;

    void SetMinHopRankIncrease(uint16_t value);
    uint16_t GetMinHopRankIncrease() const;

    void SetOcp(uint16_t ocp);
    uint16_t GetOcp() const;

    void SetDefaultLifetime(uint8_t lifetime);
    uint8_t GetDefaultLifetime() const;

    void SetLifetimeUnit(uint16_t unit);
    uint16_t GetLifetimeUnit() const;

  private:
    uint8_t m_flagsPcs;              //!< Flags | A | PCS
    uint8_t m_dioIntervalDoublings;  //!< DIOIntervalDoublings
    uint8_t m_dioIntervalMin;        //!< DIOIntervalMin
    uint8_t m_dioRedundancyConstant; //!< DIORedundancyConstant
    uint16_t m_maxRankIncrease;      //!< MaxRankIncrease
    uint16_t m_minHopRankIncrease;   //!< MinHopRankIncrease
    uint16_t m_ocp;                  //!< Objective Code Point
    uint8_t m_reserved;              //!< Reserved
    uint8_t m_defaultLifetime;     //!< Default Lifetime
    uint16_t m_lifetimeUnit;         //!< Lifetime Unit
};

std::ostream& operator<<(std::ostream& os, const RplDodagConfigOption& h);

/**
 * @ingroup rpl
 * @brief Prefix Information Option (\RFC{6550} Section 6.7.10)
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Type = 8    |Opt Length = 30| Prefix Length |L|A|R|Reserved1|
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                         Valid Lifetime                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                       Preferred Lifetime                      |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Reserved2                           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                            Prefix                             |
 |                                                               |
 |                                                               |
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class RplPrefixInfoOption : public Header
{
  public:
    RplPrefixInfoOption();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetPrefixLength(uint8_t prefixLength);
    uint8_t GetPrefixLength() const;

    void SetOnLink(bool onLink);
    bool GetOnLink() const;

    void SetAutonomous(bool autonomous);
    bool GetAutonomous() const;

    void SetRouterAddress(bool routerAddress);
    bool GetRouterAddress() const;

    void SetValidLifetime(uint32_t lifetime);
    uint32_t GetValidLifetime() const;

    void SetPreferredLifetime(uint32_t lifetime);
    uint32_t GetPreferredLifetime() const;

    void SetPrefix(Ipv6Address prefix);
    Ipv6Address GetPrefix() const;

  private:
    uint8_t m_prefixLength;       //!< Prefix Length
    uint8_t m_flags;              //!< L | A | R | Reserved1
    uint32_t m_validLifetime;     //!< Valid Lifetime
    uint32_t m_preferredLifetime; //!< Preferred Lifetime
    Ipv6Address m_prefix;         //!< Prefix
};

std::ostream& operator<<(std::ostream& os, const RplPrefixInfoOption& h);

/**
 * @ingroup rpl
 * @brief RPL Target Option (\RFC{6550} Section 6.7.7)
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Type = 5    | Option Length |     Flags     | Prefix Length |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               |
 +                                                               +
 |                            Target                             |
 +                                                               +
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class RplTargetOption : public Header
{
  public:
    RplTargetOption();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetPrefixLength(uint8_t prefixLength);
    uint8_t GetPrefixLength() const;

    void SetTarget(Ipv6Address target);
    Ipv6Address GetTarget() const;

  private:
    uint8_t m_flags;        //!< Flags
    uint8_t m_prefixLength; //!< Prefix Length
    Ipv6Address m_target;   //!< Target prefix / address
};

std::ostream& operator<<(std::ostream& os, const RplTargetOption& h);

/**
 * @ingroup rpl
 * @brief Transit Information Option (\RFC{6550} Section 6.7.8)
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Type = 6    | Option Length |E|   Flags     | Path Control  |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | Path Sequence | Path Lifetime |      Parent Address*          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               |
 +                                                               +
 |                     Parent Address* (cont.)                   |
 +                                                               +
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 *
 * Parent Address is present when Option Length is greater than 4.
 */
class RplTransitInfoOption : public Header
{
  public:
    RplTransitInfoOption();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetExternal(bool external);
    bool GetExternal() const;

    void SetPathControl(uint8_t pathControl);
    uint8_t GetPathControl() const;

    void SetPathSequence(uint8_t pathSequence);
    uint8_t GetPathSequence() const;

    void SetPathLifetime(uint8_t pathLifetime);
    uint8_t GetPathLifetime() const;

    void SetParentAddress(Ipv6Address parent);
    Ipv6Address GetParentAddress() const;
    bool HasParentAddress() const;
    void ClearParentAddress();

  private:
    uint8_t m_flags;           //!< E | Flags
    uint8_t m_pathControl;     //!< Path Control
    uint8_t m_pathSequence;    //!< Path Sequence
    uint8_t m_pathLifetime;    //!< Path Lifetime
    bool m_hasParent;          //!< Whether Parent Address is included
    Ipv6Address m_parentAddress; //!< Parent Address
};

std::ostream& operator<<(std::ostream& os, const RplTransitInfoOption& h);

} // namespace ns3

#endif /* RPL_HEADER_H */
