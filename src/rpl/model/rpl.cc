/*
 * Copyright (c) 2014 Universita' di Firenze, Italy
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

#include "rpl.h"

#include "ns3/boolean.h"
#include "ns3/ipv6-route.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/trace-source-accessor.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Rpl");

NS_OBJECT_ENSURE_REGISTERED(Rpl);

Rpl::Rpl()
    : m_ipv6(nullptr),
      m_isRoot(false),
      m_initialized(false)
{
}

Rpl::~Rpl()
{
}

TypeId
Rpl::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Rpl")
            .SetParent<Ipv6RoutingProtocol>()
            .SetGroupName("Rpl")
            .AddConstructor<Rpl>()
            .AddAttribute("IsRoot",
                          "True if this node acts as the DODAG root.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&Rpl::m_isRoot),
                          MakeBooleanChecker())
            .AddTraceSource("RouteOutputProbe",
                            "Fired when RouteOutput is invoked (success or NOROUTETOHOST).",
                            MakeTraceSourceAccessor(&Rpl::m_routeOutputTrace),
                            "ns3::Rpl::RouteProbeTracedCallback")
            .AddTraceSource("RouteInputProbe",
                            "Fired when RouteInput is invoked for unicast forwarding.",
                            MakeTraceSourceAccessor(&Rpl::m_routeInputTrace),
                            "ns3::Rpl::RouteProbeTracedCallback");
    return tid;
}

void
Rpl::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_ipv6 = nullptr;
    m_routes.clear();
    Ipv6RoutingProtocol::DoDispose();
}

void
Rpl::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    m_initialized = true;
    Ipv6RoutingProtocol::DoInitialize();
}

void
Rpl::SetIpv6(Ptr<Ipv6> ipv6)
{
    NS_LOG_FUNCTION(this << ipv6);
    m_ipv6 = ipv6;
}

Ptr<Ipv6Route>
Rpl::RouteOutput(Ptr<Packet> p,
                 const Ipv6Header& header,
                 Ptr<NetDevice> oif,
                 Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << header.GetDestination() << oif);

    Ptr<Ipv6Route> rtentry = Lookup(header.GetDestination(), true, oif);
    if (rtentry)
    {
        sockerr = Socket::ERROR_NOTERROR;
        m_routeOutputTrace(p, header.GetDestination(), true, sockerr);
    }
    else
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
        m_routeOutputTrace(p, header.GetDestination(), false, sockerr);
        NS_LOG_WARN("RouteOutput FAIL dst=" << header.GetDestination() << " NOROUTETOHOST");
    }
    return rtentry;
}

bool
Rpl::RouteInput(Ptr<const Packet> p,
                const Ipv6Header& header,
                Ptr<const NetDevice> idev,
                const UnicastForwardCallback& ucb,
                const MulticastForwardCallback& mcb,
                const LocalDeliverCallback& lcb,
                const ErrorCallback& ecb)
{
    NS_LOG_FUNCTION(this << p << header << header.GetSource() << header.GetDestination() << idev);

    NS_ASSERT(m_ipv6);
    NS_ASSERT(m_ipv6->GetInterfaceForDevice(idev) >= 0);
    uint32_t iif = m_ipv6->GetInterfaceForDevice(idev);

    if (header.GetDestination().IsMulticast())
    {
        return false;
    }

    if (header.GetDestination().IsLinkLocal() || header.GetSource().IsLinkLocal())
    {
        m_routeInputTrace(p, header.GetDestination(), false, Socket::ERROR_NOROUTETOHOST);
        if (!ecb.IsNull())
        {
            ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        }
        return false;
    }

    if (!m_ipv6->IsForwarding(iif))
    {
        m_routeInputTrace(p, header.GetDestination(), false, Socket::ERROR_NOROUTETOHOST);
        if (!ecb.IsNull())
        {
            ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        }
        return true;
    }

    Ptr<Ipv6Route> rtentry = Lookup(header.GetDestination(), false, nullptr);
    if (rtentry)
    {
        m_routeInputTrace(p, header.GetDestination(), true, Socket::ERROR_NOTERROR);
        ucb(idev, rtentry, p, header);
        return true;
    }

    m_routeInputTrace(p, header.GetDestination(), false, Socket::ERROR_NOROUTETOHOST);
    NS_LOG_WARN("RouteInput FAIL dst=" << header.GetDestination() << " NOROUTETOHOST");
    return false;
}

void
Rpl::NotifyInterfaceUp(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    if (!m_initialized)
    {
        return;
    }

    if (m_interfaceExclusions.find(interface) == m_interfaceExclusions.end())
    {
        m_ipv6->SetForwarding(interface, true);
    }
}

void
Rpl::NotifyInterfaceDown(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);
    InvalidateRoutesOnInterface(interface);
}

void
Rpl::NotifyAddAddress(uint32_t interface, Ipv6InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);

    if (!m_ipv6->IsUp(interface))
    {
        return;
    }
    if (m_interfaceExclusions.find(interface) != m_interfaceExclusions.end())
    {
        return;
    }

    if (address.GetScope() == Ipv6InterfaceAddress::GLOBAL)
    {
        Ipv6Address networkAddress = address.GetAddress().CombinePrefix(address.GetPrefix());
        AddNetworkRouteTo(networkAddress, address.GetPrefix(), Ipv6Address::GetZero(), interface);
    }
}

void
Rpl::NotifyRemoveAddress(uint32_t interface, Ipv6InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);

    if (!m_ipv6->IsUp(interface))
    {
        return;
    }

    if (address.GetScope() == Ipv6InterfaceAddress::GLOBAL)
    {
        Ipv6Address networkAddress = address.GetAddress().CombinePrefix(address.GetPrefix());
        RemoveNetworkRoute(networkAddress, address.GetPrefix());
    }
}

void
Rpl::NotifyAddRoute(Ipv6Address dst,
                    Ipv6Prefix mask,
                    Ipv6Address nextHop,
                    uint32_t interface,
                    Ipv6Address prefixToUse)
{
    NS_LOG_FUNCTION(this << dst << mask << nextHop << interface);

    if (nextHop == Ipv6Address::GetZero())
    {
        AddNetworkRouteTo(dst, mask, nextHop, interface);
    }
    else if (dst != Ipv6Address::GetZero())
    {
        AddNetworkRouteTo(dst, mask, nextHop, interface);
    }
    else
    {
        AddDefaultRouteTo(nextHop, interface);
    }
}

void
Rpl::NotifyRemoveRoute(Ipv6Address dst,
                       Ipv6Prefix mask,
                       Ipv6Address nextHop,
                       uint32_t interface,
                       Ipv6Address prefixToUse)
{
    NS_LOG_FUNCTION(this << dst << mask << nextHop << interface);
    RemoveNetworkRoute(dst, mask);
}

void
Rpl::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    NS_LOG_FUNCTION(this);

    std::ostream* os = stream->GetStream();
    *os << "Rpl routing table (Node " << GetObject<Node>()->GetId() << ")\n";
    *os << "  IsRoot: " << m_isRoot << "\n";
    for (const auto& route : m_routes)
    {
        *os << "  " << route.dest << "/" << int(route.prefix.GetPrefixLength()) << " via "
            << route.nextHop << " if " << route.interface << "\n";
    }
}

void
Rpl::SetInterfaceExclusions(std::set<uint32_t> exceptions)
{
    NS_LOG_FUNCTION(this);
    m_interfaceExclusions = exceptions;
}

void
Rpl::AddDefaultRouteTo(Ipv6Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(this << nextHop << interface);
    AddNetworkRouteTo(Ipv6Address("::"), Ipv6Prefix::GetZero(), nextHop, interface);
}

void
Rpl::SetRoot(bool isRoot)
{
    NS_LOG_FUNCTION(this << isRoot);
    m_isRoot = isRoot;
}

Ptr<Ipv6Route>
Rpl::Lookup(Ipv6Address dest, bool setSource, Ptr<NetDevice> oif)
{
    NS_LOG_FUNCTION(this << dest << setSource << oif);

    Ptr<Ipv6Route> rtentry = nullptr;
    uint16_t longestMask = 0;

    if (dest.IsLinkLocalMulticast())
    {
        NS_ASSERT_MSG(oif,
                      "Try to send on link-local multicast address, and no interface index is given!");
        rtentry = Create<Ipv6Route>();
        if (setSource)
        {
            rtentry->SetSource(
                m_ipv6->SourceAddressSelection(m_ipv6->GetInterfaceForDevice(oif), dest));
        }
        rtentry->SetDestination(dest);
        rtentry->SetGateway(Ipv6Address::GetZero());
        rtentry->SetOutputDevice(oif);
        return rtentry;
    }

    for (const auto& entry : m_routes)
    {
        if (entry.prefix.IsMatch(dest, entry.dest))
        {
            if (oif && oif != m_ipv6->GetNetDevice(entry.interface))
            {
                continue;
            }

            uint16_t maskLen = entry.prefix.GetPrefixLength();
            if (maskLen < longestMask)
            {
                continue;
            }
            longestMask = maskLen;

            rtentry = Create<Ipv6Route>();
            if (setSource)
            {
                rtentry->SetSource(
                    m_ipv6->SourceAddressSelection(entry.interface, entry.dest));
            }
            rtentry->SetDestination(entry.dest);
            rtentry->SetGateway(entry.nextHop);
            rtentry->SetOutputDevice(m_ipv6->GetNetDevice(entry.interface));
        }
    }

    return rtentry;
}

void
Rpl::AddNetworkRouteTo(Ipv6Address network,
                       Ipv6Prefix networkPrefix,
                       Ipv6Address nextHop,
                       uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkPrefix << nextHop << interface);

    RplRouteEntry entry;
    entry.dest = network;
    entry.prefix = networkPrefix;
    entry.nextHop = nextHop;
    entry.interface = interface;
    m_routes.push_back(entry);
}

void
Rpl::RemoveNetworkRoute(Ipv6Address network, Ipv6Prefix networkPrefix)
{
    NS_LOG_FUNCTION(this << network << networkPrefix);

    for (auto it = m_routes.begin(); it != m_routes.end();)
    {
        if (it->dest == network && it->prefix == networkPrefix)
        {
            it = m_routes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void
Rpl::InvalidateRoutesOnInterface(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    for (auto it = m_routes.begin(); it != m_routes.end();)
    {
        if (it->interface == interface)
        {
            it = m_routes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace ns3
