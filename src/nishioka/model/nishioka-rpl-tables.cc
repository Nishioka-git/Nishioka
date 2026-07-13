/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-rpl-tables.h"

namespace ns3
{
namespace nishioka
{

void
RplRoutingTable::SetParent(const RplParentInfo& parent)
{
    m_parent = parent;
}

std::optional<RplParentInfo>
RplRoutingTable::GetParent() const
{
    if (!m_parent.valid)
    {
        return std::nullopt;
    }
    return m_parent;
}

bool
RplRoutingTable::AddDownwardRoute(const RplRouteEntry& entry)
{
    if (!entry.valid)
    {
        return false;
    }
    m_downwardRoutes[entry.target] = entry.nextHop;
    return true;
}

bool
RplRoutingTable::LookupDownwardRoute(Mac16Address target, Mac16Address& nextHop) const
{
    auto it = m_downwardRoutes.find(target);
    if (it == m_downwardRoutes.end())
    {
        return false;
    }
    nextHop = it->second;
    return true;
}

void
RplRoutingTable::Clear()
{
    m_parent = RplParentInfo{};
    m_downwardRoutes.clear();
}

uint32_t
RplRoutingTable::GetDownwardRouteCount() const
{
    return static_cast<uint32_t>(m_downwardRoutes.size());
}

} // namespace nishioka
} // namespace ns3
