/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_RPL_PAYLOAD_HEADER_H
#define NISHIOKA_RPL_PAYLOAD_HEADER_H

#include "nishioka-rpl-fields.h"

#include "ns3/header.h"
#include "ns3/mac16-address.h"

namespace ns3
{
namespace nishioka
{

/**
 * Common prefix for all RPL control payloads inside NishiokaHeader::CUSTOM_COMMAND.
 */
class RplControlHeader : public Header
{
  public:
    RplControlHeader();
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    void SetControlType(RplControlType type);
    RplControlType GetControlType() const;

  private:
    uint8_t m_controlType{static_cast<uint8_t>(RplControlType::DIO)};
};

/**
 * DIO payload (DODAG Information Object) — simplified RFC 6550 subset.
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

    void SetDodagInfo(const RplDodagInfo& info);
    RplDodagInfo GetDodagInfo() const;

    void SetSenderRank(uint16_t rank);
    uint16_t GetSenderRank() const;

  private:
    RplDodagInfo m_dodag;
    uint16_t m_senderRank{RPL_ROOT_RANK};
};

/**
 * DAO payload (Destination Advertisement Object) — target prefix advertisement.
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

    void SetTarget(Mac16Address target);
    Mac16Address GetTarget() const;

    void SetPathSequence(uint8_t seq);
    uint8_t GetPathSequence() const;

  private:
    Mac16Address m_target;
    uint8_t m_pathSequence{0};
};

/**
 * DAO-ACK payload.
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

    void SetTarget(Mac16Address target);
    Mac16Address GetTarget() const;

    void SetStatus(uint8_t status);
    uint8_t GetStatus() const;

  private:
    Mac16Address m_target;
    uint8_t m_status{0};
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_RPL_PAYLOAD_HEADER_H */
