/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#ifndef NISHIOKA_RPL_TABLES_H
#define NISHIOKA_RPL_TABLES_H

#include "nishioka-rpl-fields.h"

#include "ns3/mac16-address.h"

#include <map>
#include <optional>

namespace ns3
{
namespace nishioka
{

/**
 * Downward route installed by DAO processing (target -> next hop).
 * Complements NishiokaNwk::m_routingTable used for data forwarding.
 */
struct RplRouteEntry
{
    Mac16Address target;
    Mac16Address nextHop;
    uint8_t pathCost{0};
    bool valid{false};
};

/**
 * In-memory RPL routing state for one node.
 *
 * - Parent table: single preferred parent (scaffold).
 * - Downward table: entries learned from DAO messages.
 */
class RplRoutingTable
{
  public:
    void SetParent(const RplParentInfo& parent);
    std::optional<RplParentInfo> GetParent() const;

    bool AddDownwardRoute(const RplRouteEntry& entry);
    bool LookupDownwardRoute(Mac16Address target, Mac16Address& nextHop) const;
    void Clear();

    uint32_t GetDownwardRouteCount() const;

  private:
    RplParentInfo m_parent;
    std::map<Mac16Address, Mac16Address> m_downwardRoutes;
};

} // namespace nishioka
} // namespace ns3

#endif /* NISHIOKA_RPL_TABLES_H */
