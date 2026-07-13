/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_RPL_FIELDS_H
#define NISHIOKA_RPL_FIELDS_H

#include "ns3/mac16-address.h"
#include "ns3/mac64-address.h"

#include <cstdint>

namespace ns3
{
namespace nishioka
{

/** RPL rank of the DODAG root (RFC 6550). */
static constexpr uint16_t RPL_ROOT_RANK = 256;

/** Rank value meaning "no parent selected". */
static constexpr uint16_t RPL_INFINITE_RANK = 0xFFFF;

/** Default DIO trickle interval (scaffold; tune when implementing RFC 6206). */
static constexpr double RPL_DEFAULT_DIO_INTERVAL_SEC = 1.0;

/**
 * RPL control message types carried in NishiokaHeader::CUSTOM_COMMAND payloads.
 * Maps conceptually to RFC 6550 ICMPv6 RPL control messages.
 */
enum class RplControlType : uint8_t
{
    DIO = 1,     //!< DODAG Information Object
    DAO = 2,     //!< Destination Advertisement Object
    DAO_ACK = 3, //!< DAO acknowledgment
    DIS = 4      //!< DODAG Information Solicitation (optional extension)
};

/**
 * Node role inside a DODAG (simplified RPL MOP-agnostic view).
 */
enum class RplNodeRole : uint8_t
{
    UNASSIGNED = 0,
    ROOT = 1,
    ROUTER = 2,
    LEAF = 3
};

/**
 * DODAG metadata shared by DIO/DAO processing.
 */
struct RplDodagInfo
{
    Mac16Address dodagId;   //!< DODAG ID (typically root short address)
    uint8_t instanceId{0};  //!< RPLInstanceID
    uint8_t version{0};     //!< DODAGVersionNumber
    uint16_t rank{RPL_INFINITE_RANK};
};

/**
 * Selected parent toward the DODAG root.
 */
struct RplParentInfo
{
    Mac16Address parentAddr;
    uint16_t parentRank{RPL_INFINITE_RANK};
    uint8_t parentLqi{0};
    bool valid{false};
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_RPL_FIELDS_H */
