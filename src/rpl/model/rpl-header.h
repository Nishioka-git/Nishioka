#ifndef RPL_HEADER_H
#define RPL_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv6-address.h"

namespace ns3
{

/**
 * @ingroup rpl
 * @brief ICMPv6 Type for RPL control messages (\RFC{6550} Section 6)
 *
 * "RPL Control messages are identified by an ICMPv6 Type of 155"
 */
static constexpr uint8_t RPL_ICMPV6_TYPE = 155;

/**
 * @ingroup rpl
 * @brief ICMPv6 Code values for RPL control messages (\RFC{6550} Section 6)
 *
 * "RPL Control messages are identified by an ICMPv6 Type of 155 and Codes
 *  as follows:
 *    0x00: DODAG Information Solicitation (Section 6.2)
 *    0x01: DODAG Information Object (Section 6.3)
 *    0x02: Destination Advertisement Object (Section 6.4)
 *    0x03: Destination Advertisement Object Acknowledgment (Section 6.5)"
 */
enum class RplIcmpv6Code : uint8_t
{
    DIS = 0x00,     //!< DODAG Information Solicitation (Sec. 6.2)
    DIO = 0x01,     //!< DODAG Information Object (Sec. 6.3)
    DAO = 0x02,     //!< Destination Advertisement Object (Sec. 6.4)
    DAO_ACK = 0x03, //!< Destination Advertisement Object Acknowledgement (Sec. 6.5)
};

/**
 * @ingroup rpl
 * @brief DIS Options (\RFC{6550} Section 6.2.3)
 *
 * "This specification allows for the DIS message to carry the following
 *  options:
 *     0x00 Pad1
 *     0x01 PadN
 *     0x07 Solicited Information"
 *
 * Option TLV bodies are added when control-plane handling is implemented.
 */
enum class DisOptions : uint8_t
{
    PAD1 = 0x00,                  //!< Pad1
    PADN = 0x01,                  //!< PadN
    SOLICITED_INFORMATION = 0x07, //!< Solicited Information
};

/**
 * @ingroup rpl
 * @brief DIO Options (\RFC{6550} Section 6.3.3)
 *
 * "This specification allows for the DIO message to carry the following
 *  options:
 *     0x00 Pad1
 *     0x01 PadN
 *     0x02 DAG Metric Container
 *     0x03 Routing Information
 *     0x04 DODAG Configuration
 *     0x08 Prefix Information"
 *
 * Option TLV bodies are added when control-plane handling is implemented.
 */
enum class DioOptions : uint8_t
{
    PAD1 = 0x00,                 //!< Pad1
    PADN = 0x01,                 //!< PadN
    DAG_METRIC_CONTAINER = 0x02, //!< DAG Metric Container
    ROUTE_INFORMATION = 0x03,    //!< Routing Information
    DODAG_CONFIGURATION = 0x04,  //!< DODAG Configuration
    PREFIX_INFORMATION = 0x08,   //!< Prefix Information
};

/**
 * @ingroup rpl
 * @brief DAO Options (\RFC{6550} Section 6.4.3)
 *
 * "This specification allows for the DAO message to carry the following
 *  options:
 *     0x00 Pad1
 *     0x01 PadN
 *     0x05 RPL Target
 *     0x06 Transit Information
 *     0x09 RPL Target Descriptor"
 *
 * Option TLV bodies are added when control-plane handling is implemented.
 */
enum class DaoOptions : uint8_t
{
    PAD1 = 0x00,                  //!< Pad1
    PADN = 0x01,                  //!< PadN
    RPL_TARGET = 0x05,            //!< RPL Target
    TRANSIT_INFORMATION = 0x06,   //!< Transit Information
    RPL_TARGET_DESCRIPTOR = 0x09, //!< RPL Target Descriptor
};

/**
 * @ingroup rpl
 * @brief Mode of Operation (MOP) (\RFC{6550} Section 6.3.1, Figure 15)
 *
 * "Mode of Operation (MOP): The Mode of Operation (MOP) field identifies
 *  the mode of operation of the RPL Instance as administratively
 *  provisioned at and distributed by the DODAG root."
 *
 * "A value of 0 indicates that destination advertisement messages are
 *  disabled and the DODAG maintains only Upward routes."
 */
enum class ModeOfOperation : uint8_t
{
    NO_DOWNWARD_ROUTES = 0,       //!< No Downward routes maintained by RPL
    NON_STORING = 1,              //!< Non-Storing Mode of Operation
    STORING_NO_MULTICAST = 2,     //!< Storing Mode of Operation with no multicast support
    STORING_WITH_MULTICAST = 3,   //!< Storing Mode of Operation with multicast support
};

/**
 * @ingroup rpl
 * @brief DAO-ACK Status (\RFC{6550} Section 6.5.1)
 *
 * "Status: Indicates the completion.  Status 0 is defined as unqualified
 *  acceptance in this specification.  The remaining status values
 *  are reserved as rejection codes."
 *
 * Guidelines from Sec. 6.5.1:
 *   0:       Unqualified acceptance
 *   1-127:   Not an outright rejection; alternate parent suggested
 *   127-255: Rejection; unwilling to act as a parent
 *
 * \RFC{6550} Section 6.5.3:
 * "This specification does not define any options to be carried by the
 *  DAO-ACK message."
 */
enum class DaoAckStatus : uint8_t
{
    UNQUALIFIED_ACCEPTANCE = 0, //!< Status 0: unqualified acceptance
};

/**
 * @ingroup rpl
 * @brief DIS Base Object (\RFC{6550} Section 6.2 / 6.2.1, Figure 13)
 *
 * From \RFC{6550} Section 6.2. DODAG Information Solicitation (DIS):
 * "The DODAG Information Solicitation (DIS) message may be used to
 *  solicit a DODAG Information Object from a RPL node.  Its use is
 *  analogous to that of a Router Solicitation as specified in IPv6
 *  Neighbor Discovery; a node may use DIS to probe its neighborhood for
 *  nearby DODAGs.  Section 8.3 describes how nodes respond to a DIS."
 *
 * Flags / Reserved (\RFC{6550} Sec. 6.2.1) MUST be zero on send and
 * ignored on receive; they are not stored as members.
 *
 * \verbatim
  0                   1                   2
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     Flags     |   Reserved    |   Option(s)...
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class DisBaseObjectHeader : public Header
{
  public:
    DisBaseObjectHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;
};

std::ostream& operator<<(std::ostream& os, const DisBaseObjectHeader& h);

/**
 * @ingroup rpl
 * @brief DIO Base Object (\RFC{6550} Section 6.3 / 6.3.1, Figure 14)
 *
 * From \RFC{6550} Section 6.3. DODAG Information Object (DIO):
 * "The DODAG Information Object carries information that allows a node
 *  to discover a RPL Instance, learn its configuration parameters,
 *  select a DODAG parent set, and maintain the DODAG."
 *
 * Flags / Reserved (\RFC{6550} Sec. 6.3.1) MUST be zero on send and
 * ignored on receive; they are not stored as members.
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
class DioBaseObjectHeader : public Header
{
  public:
    DioBaseObjectHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /** \RFC{6550} Sec. 6.3.1: RPLInstanceID */
    void SetRplInstanceId(uint8_t rplInstanceId);
    uint8_t GetRplInstanceId() const;

    /**
     * \RFC{6550} Sec. 6.3.1:
     * "Version Number: 8-bit unsigned integer set by the DODAG root to the
     *  DODAGVersionNumber."
     */
    void SetVersionNumber(uint8_t versionNumber);
    uint8_t GetVersionNumber() const;

    /**
     * \RFC{6550} Sec. 6.3.1:
     * "Rank: 16-bit unsigned integer indicating the DODAG Rank of the node
     *  sending the DIO message."
     */
    void SetRank(uint16_t rank);
    uint16_t GetRank() const;

    /**
     * \RFC{6550} Sec. 6.3.1:
     * "Grounded (G): The Grounded 'G' flag indicates whether the DODAG
     *  advertised can satisfy the application-defined goal.  If the
     *  flag is set, the DODAG is grounded.  If the flag is cleared,
     *  the DODAG is floating."
     */
    void SetGrounded(bool grounded);
    bool IsGrounded() const;

    /**
     * \RFC{6550} Sec. 6.3.1 / Figure 15: Mode of Operation (MOP)
     */
    void SetModeOfOperation(ModeOfOperation mop);
    ModeOfOperation GetModeOfOperation() const;

    /**
     * \RFC{6550} Sec. 6.3.1:
     * "DODAGPreference (Prf): A 3-bit unsigned integer that defines how
     *  preferable the root of this DODAG is compared to other DODAG
     *  roots within the instance.  DAGPreference ranges from 0x00
     *  (least preferred) to 0x07 (most preferred)."
     */
    void SetDodagPreference(uint8_t preference);
    uint8_t GetDodagPreference() const;

    /**
     * \RFC{6550} Sec. 6.3.1:
     * "Destination Advertisement Trigger Sequence Number (DTSN): 8-bit
     *  unsigned integer set by the node issuing the DIO message."
     */
    void SetDtsn(uint8_t dtsn);
    uint8_t GetDtsn() const;

    /**
     * \RFC{6550} Sec. 6.3.1:
     * "DODAGID: 128-bit IPv6 address set by a DODAG root that uniquely
     *  identifies a DODAG.  The DODAGID MUST be a routable IPv6
     *  address belonging to the DODAG root."
     */
    void SetDodagId(Ipv6Address dodagId);
    Ipv6Address GetDodagId() const;

  private:
    uint8_t m_rplInstanceId; //!< RPLInstanceID
    uint8_t m_versionNumber; //!< Version Number
    uint16_t m_rank;         //!< Rank
    uint8_t m_gMopPrf;       //!< G | 0 | MOP | Prf
    uint8_t m_dtsn;          //!< DTSN
    Ipv6Address m_dodagId;   //!< DODAGID
};

std::ostream& operator<<(std::ostream& os, const DioBaseObjectHeader& h);

/**
 * @ingroup rpl
 * @brief DAO Base Object (\RFC{6550} Section 6.4 / 6.4.1, Figure 16)
 *
 * From \RFC{6550} Section 6.4. Destination Advertisement Object (DAO):
 * "The Destination Advertisement Object (DAO) is used to propagate
 *  destination information Upward along the DODAG.  In Storing mode, the
 *  DAO message is unicast by the child to the selected parent(s).  In
 *  Non-Storing mode, the DAO message is unicast to the DODAG root.  The
 *  DAO message may optionally, upon explicit request or error, be
 *  acknowledged by its destination with a Destination Advertisement
 *  Acknowledgement (DAO-ACK) message back to the sender of the DAO."
 *
 * Unused Flags bits and Reserved MUST be zero on send and ignored on
 * receive; they are not stored as members.
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | RPLInstanceID |K|D|   Flags   |   Reserved    | DAOSequence   |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                            DODAGID*                           |
 |                                                               |
 |                                                               |
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Option(s)...
 +-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class DaoBaseObjectHeader : public Header
{
  public:
    DaoBaseObjectHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /** \RFC{6550} Sec. 6.4.1: RPLInstanceID */
    void SetRplInstanceId(uint8_t rplInstanceId);
    uint8_t GetRplInstanceId() const;

    /**
     * \RFC{6550} Sec. 6.4.1:
     * "K: The 'K' flag indicates that the recipient is expected to send a
     *  DAO-ACK back.  (See Section 9.3.)"
     */
    void SetK(bool k);
    bool GetK() const;

    /**
     * \RFC{6550} Sec. 6.4.1:
     * "D: The 'D' flag indicates that the DODAGID field is present.  This
     *  flag MUST be set when a local RPLInstanceID is used."
     */
    void SetD(bool d);
    bool GetD() const;

    /**
     * \RFC{6550} Sec. 6.4.1:
     * "DAOSequence: Incremented at each unique DAO message from a node and
     *  echoed in the DAO-ACK message."
     */
    void SetDaoSequence(uint8_t daoSequence);
    uint8_t GetDaoSequence() const;

    /**
     * \RFC{6550} Sec. 6.4.1: optional DODAGID (present when D is set).
     * Setting DODAGID also sets the D flag.
     */
    void SetDodagId(Ipv6Address dodagId);
    Ipv6Address GetDodagId() const;

  private:
    uint8_t m_rplInstanceId; //!< RPLInstanceID
    bool m_k;                //!< K flag
    bool m_d;                //!< D flag
    uint8_t m_daoSequence;   //!< DAOSequence
    Ipv6Address m_dodagId;   //!< DODAGID*
};

std::ostream& operator<<(std::ostream& os, const DaoBaseObjectHeader& h);

/**
 * @ingroup rpl
 * @brief DAO-ACK Base Object (\RFC{6550} Section 6.5 / 6.5.1, Figure 17)
 *
 * From \RFC{6550} Section 6.5:
 * "The DAO-ACK message is sent as a unicast packet by a DAO recipient (a
 *  DAO parent or DODAG root) in response to a unicast DAO message."
 *
 * Reserved bits MUST be zero on send and ignored on receive.
 *
 * \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | RPLInstanceID |D|  Reserved   |  DAOSequence  |    Status     |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                            DODAGID*                           |
 |                                                               |
 |                                                               |
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Option(s)...
 +-+-+-+-+-+-+-+-+
 * \endverbatim
 */
class DaoAckBaseObjectHeader : public Header
{
  public:
    DaoAckBaseObjectHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /** \RFC{6550} Sec. 6.5.1: RPLInstanceID */
    void SetRplInstanceId(uint8_t rplInstanceId);
    uint8_t GetRplInstanceId() const;

    /**
     * \RFC{6550} Sec. 6.5.1:
     * "D: The 'D' flag indicates that the DODAGID field is present."
     */
    void SetD(bool d);
    bool GetD() const;

    /** \RFC{6550} Sec. 6.5.1: DAOSequence (echoed from DAO) */
    void SetDaoSequence(uint8_t daoSequence);
    uint8_t GetDaoSequence() const;

    /** \RFC{6550} Sec. 6.5.1: Status */
    void SetStatus(DaoAckStatus status);
    void SetStatus(uint8_t status);
    uint8_t GetStatus() const;

    /** \RFC{6550} Sec. 6.5.1: optional DODAGID (present when D is set) */
    void SetDodagId(Ipv6Address dodagId);
    Ipv6Address GetDodagId() const;

  private:
    uint8_t m_rplInstanceId; //!< RPLInstanceID
    bool m_d;                //!< D flag
    uint8_t m_daoSequence;   //!< DAOSequence
    uint8_t m_status;        //!< Status
    Ipv6Address m_dodagId;   //!< DODAGID*
};

std::ostream& operator<<(std::ostream& os, const DaoAckBaseObjectHeader& h);

} // namespace ns3

#endif /* RPL_HEADER_H */
