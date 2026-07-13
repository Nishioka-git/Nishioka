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
    uint8_t m_instanceId;  //!< RPLInstanceID
    uint8_t m_version;     //!< Version Number
    uint16_t m_rank;       //!< Rank
    uint8_t m_gMopPrf;     //!< G | 0 | MOP | Prf
    uint8_t m_dtsn;        //!< Destination Advertisement Trigger Sequence Number
    uint8_t m_flags;       //!< Flags
    uint8_t m_reserved;    //!< Reserved
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

} // namespace ns3

#endif /* RPL_HEADER_H */
