/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author:
 *  Nishioka, Yugo
 */

#include "nishioka-nwk.h"

#include "ns3/log.h"
#include "ns3/lr-wpan-mac-base.h"
#include "ns3/mac16-address.h"

using namespace ns3::lrwpan;

namespace ns3
{
namespace nishioka
{

NS_LOG_COMPONENT_DEFINE("NishiokaNwk");
NS_OBJECT_ENSURE_REGISTERED(NishiokaNwk);

TypeId
NishiokaNwk::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nishioka::NishiokaNwk")
                            .SetParent<Object>()
                            .SetGroupName("Nishioka")
                            .AddConstructor<NishiokaNwk>();
    return tid;
}

NishiokaNwk::NishiokaNwk()
{
    NS_LOG_FUNCTION(this);
}

NishiokaNwk::~NishiokaNwk()
{
    NS_LOG_FUNCTION(this);
}

void
NishiokaNwk::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_mac = nullptr;
    Object::DoDispose();
}

void
NishiokaNwk::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO("NishiokaNwk initialized");
    Object::DoInitialize();
}

void
NishiokaNwk::SetMac(Ptr<LrWpanMacBase> mac)
{
    NS_LOG_FUNCTION(this << mac);
    m_mac = mac;
}

Ptr<LrWpanMacBase>
NishiokaNwk::GetMac() const
{
    NS_LOG_FUNCTION(this);
    return m_mac;
}

bool
NishiokaNwk::SetRoute(Mac16Address dst, Mac16Address nextHop)
{
    NS_LOG_FUNCTION(this << dst << nextHop);

    // Check if route already exists
    auto it = m_routingTable.find(dst);
    if (it != m_routingTable.end())
    {
        // Update existing route
        it->second = nextHop;
        NS_LOG_INFO("Updated route: dst=" << dst << " nextHop=" << nextHop);
    }
    else
    {
        // Add new route
        m_routingTable[dst] = nextHop;
        NS_LOG_INFO("Added route: dst=" << dst << " nextHop=" << nextHop);
    }

    return true;
}

bool
NishiokaNwk::GetNextHop(Mac16Address dst, Mac16Address& nextHop) const
{
    NS_LOG_FUNCTION(this << dst);

    auto it = m_routingTable.find(dst);
    if (it != m_routingTable.end())
    {
        nextHop = it->second;
        NS_LOG_DEBUG("Found route: dst=" << dst << " nextHop=" << nextHop);
        return true;
    }

    NS_LOG_DEBUG("Route not found for dst=" << dst);
    return false;
}

bool
NishiokaNwk::RemoveRoute(Mac16Address dst)
{
    NS_LOG_FUNCTION(this << dst);

    auto it = m_routingTable.find(dst);
    if (it != m_routingTable.end())
    {
        m_routingTable.erase(it);
        NS_LOG_INFO("Removed route: dst=" << dst);
        return true;
    }

    NS_LOG_DEBUG("Route not found for removal: dst=" << dst);
    return false;
}

bool
NishiokaNwk::HasRoute(Mac16Address dst) const
{
    NS_LOG_FUNCTION(this << dst);
    return m_routingTable.find(dst) != m_routingTable.end();
}

uint32_t
NishiokaNwk::GetRouteCount() const
{
    NS_LOG_FUNCTION(this);
    return m_routingTable.size();
}

} // namespace nishioka
} // namespace ns3

