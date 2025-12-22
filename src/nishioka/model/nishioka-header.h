/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Custom MAC Header implementation based on lr-wpan-mac-header
 */

#ifndef NISHIOKA_HEADER_H
#define NISHIOKA_HEADER_H

#include "ns3/header.h"
#include "ns3/mac16-address.h"
#include "ns3/mac64-address.h"

namespace ns3
{
namespace nishioka
{

/**
 * @ingroup nishioka
 * Custom MAC Header implementation
 * This header is similar to LrWpanMacHeader but simplified for demonstration
 */
class NishiokaHeader : public Header
{
  public:
    /**
     * The possible MAC frame types
     */
    enum FrameType
    {
        CUSTOM_DATA = 0,        //!< Data frame
        CUSTOM_BEACON = 1,      //!< Beacon frame
        CUSTOM_ACK = 2,         //!< Acknowledgment frame
        CUSTOM_COMMAND = 3,     //!< Command frame
        CUSTOM_RESERVED = 0xff  //!< Reserved
    };

    /**
     * The addressing mode types
     */
    enum AddrModeType
    {
        NOADDR = 0,
        SHORTADDR = 2,
        EXTADDR = 3
    };

    NishiokaHeader();

    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * Set the frame type
     * @param frameType the frame type
     */
    void SetFrameType(FrameType frameType);

    /**
     * Get the frame type
     * @return the frame type
     */
    FrameType GetFrameType() const;

    /**
     * Set the sequence number
     * @param seqNum sequence number
     */
    void SetSeqNum(uint8_t seqNum);

    /**
     * Get the sequence number
     * @return the sequence number
     */
    uint8_t GetSeqNum() const;

    /**
     * Set source address fields
     * @param panId source PAN ID
     * @param addr source address (16 bit)
     */
    void SetSrcAddrFields(uint16_t panId, Mac16Address addr);

    /**
     * Set source address fields
     * @param panId source PAN ID
     * @param addr source address (64 bit)
     */
    void SetSrcAddrFields(uint16_t panId, Mac64Address addr);

    /**
     * Set destination address fields
     * @param panId destination PAN ID
     * @param addr destination address (16 bit)
     */
    void SetDstAddrFields(uint16_t panId, Mac16Address addr);

    /**
     * Set destination address fields
     * @param panId destination PAN ID
     * @param addr destination address (64 bit)
     */
    void SetDstAddrFields(uint16_t panId, Mac64Address addr);

    /**
     * Get the Destination PAN ID
     * @return the Destination PAN ID
     */
    uint16_t GetDstPanId() const;

    /**
     * Get the Destination Short address
     * @return the Destination Short address
     */
    Mac16Address GetShortDstAddr() const;

    /**
     * Get the Destination Extended address
     * @return the Destination Extended address
     */
    Mac64Address GetExtDstAddr() const;

    /**
     * Get the Source PAN ID
     * @return the Source PAN ID
     */
    uint16_t GetSrcPanId() const;

    /**
     * Get the Source Short address
     * @return the Source Short address
     */
    Mac16Address GetShortSrcAddr() const;

    /**
     * Get the Source Extended address
     * @return the Source Extended address
     */
    Mac64Address GetExtSrcAddr() const;

    /**
     * Set battery level (e.g., remaining capacity)
     * @param battery battery level value
     */
    void SetBattery(uint16_t battery);

    /**
     * Get battery level
     * @return battery level value
     */
    uint16_t GetBattery() const;

    /**
     * Set evaluation metric value
     * @param evaluation metric value
     */
    void SetEvaluation(uint16_t evaluation);

    /**
     * Get evaluation metric value
     * @return evaluation metric value
     */
    uint16_t GetEvaluation() const;

    /**
     * Set the destination address mode
     * @param addrMode Destination address mode
     */
    void SetDstAddrMode(uint8_t addrMode);

    /**
     * Set the source address mode
     * @param addrMode Source address mode
     */
    void SetSrcAddrMode(uint8_t addrMode);

    /**
     * Get the Destination address mode
     * @return the Destination address mode
     */
    uint8_t GetDstAddrMode() const;

    /**
     * Get the Source address mode
     * @return the Source address mode
     */
    uint8_t GetSrcAddrMode() const;

  private:
    uint8_t m_frameType;              //!< Frame type (0-3)
    uint8_t m_seqNum;                 //!< Sequence number
    uint8_t m_dstAddrMode;            //!< Destination address mode
    uint8_t m_srcAddrMode;            //!< Source address mode
    uint16_t m_dstPanId;              //!< Destination PAN ID
    Mac16Address m_shortDstAddr;       //!< Destination short address
    Mac64Address m_extDstAddr;         //!< Destination extended address
    uint16_t m_srcPanId;              //!< Source PAN ID
    Mac16Address m_shortSrcAddr;       //!< Source short address
    Mac64Address m_extSrcAddr;        //!< Source extended address
    uint16_t m_battery;               //!< Battery level
    uint16_t m_evaluation;            //!< Evaluation metric
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_HEADER_H */