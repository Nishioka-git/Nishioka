/*
 * Copyright (c) 2014 Universita' di Firenze, Italy
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

#include "rpl.h"

#include "ns3/boolean.h"
#include "ns3/icmpv6-header.h"
#include "ns3/ipv6-header.h"
#include "ns3/ipv6-packet-info-tag.h"
#include "ns3/ipv6-raw-socket-factory.h"
#include "ns3/ipv6-route.h"
#include "ns3/log.h"
#include "ns3/loopback-net-device.h"
#include "ns3/node.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Rpl");

NS_OBJECT_ENSURE_REGISTERED(Rpl);

/// Default Rank advertised by a DODAG root (before Trickle / OF0).
static constexpr uint16_t RPL_ROOT_RANK = 1;

Rpl::Rpl()
    : m_ipv6(nullptr),
      m_isRoot(false),
      m_initialized(false),
      m_multicastRecvSocket(nullptr),
      m_dioInterval(Seconds(1.0)),
      m_rplInstanceId(0),
      m_versionNumber(1),
      m_dtsn(0),
      m_rank(RPL_ROOT_RANK),
      m_mop(ModeOfOperation::NO_DOWNWARD_ROUTES),
      m_dodagPreference(0),
      m_dodagId(Ipv6Address::GetZero())
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
            .AddAttribute("DioInterval",
                          "Interval between periodic DIO transmissions from the DODAG root.",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&Rpl::m_dioInterval),
                          MakeTimeChecker())
            .AddAttribute("RplInstanceId",
                          "RPLInstanceID advertised in DIO messages.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&Rpl::m_rplInstanceId),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("VersionNumber",
                          "DODAG Version Number advertised in DIO messages.",
                          UintegerValue(1),
                          MakeUintegerAccessor(&Rpl::m_versionNumber),
                          MakeUintegerChecker<uint8_t>())
            .AddTraceSource("RouteOutputProbe",
                            "Fired when RouteOutput is invoked (success or NOROUTETOHOST).",
                            MakeTraceSourceAccessor(&Rpl::m_routeOutputTrace),
                            "ns3::Rpl::RouteProbeTracedCallback")
            .AddTraceSource("RouteInputProbe",
                            "Fired when RouteInput is invoked for unicast forwarding.",
                            MakeTraceSourceAccessor(&Rpl::m_routeInputTrace),
                            "ns3::Rpl::RouteProbeTracedCallback")
            .AddTraceSource("DioTx",
                            "Fired when a DIO is transmitted.",
                            MakeTraceSourceAccessor(&Rpl::m_dioTxTrace),
                            "ns3::Rpl::DioTracedCallback")
            .AddTraceSource("DioRx",
                            "Fired when a DIO is received.",
                            MakeTraceSourceAccessor(&Rpl::m_dioRxTrace),
                            "ns3::Rpl::DioTracedCallback");
    return tid;
}

void
Rpl::DoDispose()
{
    NS_LOG_FUNCTION(this);

    m_dioTimerEvent.Cancel();

    for (auto& entry : m_sockets)
    {
        entry.first->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        entry.first->Close();
    }
    m_sockets.clear();

    if (m_multicastRecvSocket)
    {
        m_multicastRecvSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_multicastRecvSocket->Close();
        m_multicastRecvSocket = nullptr;
    }

    m_ipv6 = nullptr;
    m_routes.clear();
    Ipv6RoutingProtocol::DoDispose();
}

void
Rpl::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    m_initialized = true;

    NS_ASSERT_MSG(m_ipv6, "SetIpv6 must be called before DoInitialize");

    for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
    {
        if (m_interfaceExclusions.find(i) != m_interfaceExclusions.end())
        {
            continue;
        }
        if (!m_ipv6->IsUp(i))
        {
            continue;
        }
        if (DynamicCast<LoopbackNetDevice>(m_ipv6->GetNetDevice(i)))
        {
            continue;
        }

        m_ipv6->SetForwarding(i, true);
        BindToInterface(i);
    }

    if (m_isRoot)
    {
        if (m_dodagId.IsAny())
        {
            for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
            {
                Ipv6Address id = SelectDodagId(i);
                if (!id.IsAny())
                {
                    m_dodagId = id;
                    break;
                }
            }
        }
        m_rank = RPL_ROOT_RANK;
        ScheduleNextDio();
    }

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

    if (m_interfaceExclusions.find(interface) != m_interfaceExclusions.end())
    {
        return;
    }

    if (DynamicCast<LoopbackNetDevice>(m_ipv6->GetNetDevice(interface)))
    {
        return;
    }

    m_ipv6->SetForwarding(interface, true);
    BindToInterface(interface);

    if (m_isRoot && m_dodagId.IsAny())
    {
        m_dodagId = SelectDodagId(interface);
    }
}

void
Rpl::NotifyInterfaceDown(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);
    UnbindFromInterface(interface);
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

        if (m_isRoot && m_dodagId.IsAny())
        {
            m_dodagId = address.GetAddress();
        }
    }

    if (m_initialized && address.GetScope() == Ipv6InterfaceAddress::LINKLOCAL)
    {
        BindToInterface(interface);
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
                    Ipv6Address /*prefixToUse*/)
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
                       Ipv6Address /*prefixToUse*/)
{
    NS_LOG_FUNCTION(this << dst << mask << nextHop << interface);
    RemoveNetworkRoute(dst, mask);
}

void
Rpl::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit /*unit*/) const
{
    NS_LOG_FUNCTION(this);

    std::ostream* os = stream->GetStream();
    *os << "Rpl routing table (Node " << GetObject<Node>()->GetId() << ")\n";
    *os << "  IsRoot: " << m_isRoot << "\n";
    *os << "  DODAGID: " << m_dodagId << " Rank: " << m_rank << "\n";
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

    if (!m_initialized)
    {
        return;
    }

    if (m_isRoot)
    {
        if (m_dodagId.IsAny())
        {
            for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
            {
                Ipv6Address id = SelectDodagId(i);
                if (!id.IsAny())
                {
                    m_dodagId = id;
                    break;
                }
            }
        }
        m_rank = RPL_ROOT_RANK;
        if (!m_dioTimerEvent.IsPending())
        {
            ScheduleNextDio();
        }
    }
    else
    {
        m_dioTimerEvent.Cancel();
    }
}

bool
Rpl::IsRoot() const
{
    return m_isRoot;
}

void
Rpl::BindToInterface(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    Ipv6Address linkLocal = GetLinkLocalAddress(interface);
    if (linkLocal.IsAny())
    {
        NS_LOG_LOGIC("No link-local address yet on interface " << interface);
        return;
    }

    for (const auto& entry : m_sockets)
    {
        if (entry.second == interface)
        {
            return;
        }
    }

    Ptr<Node> node = GetObject<Node>();
    Ptr<Socket> socket =
        Socket::CreateSocket(node, Ipv6RawSocketFactory::GetTypeId());
    socket->SetAttribute("Protocol", UintegerValue(Ipv6Header::IPV6_ICMPV6));
    socket->BindToNetDevice(m_ipv6->GetNetDevice(interface));
    socket->Bind(Inet6SocketAddress(linkLocal, 0));
    socket->SetRecvCallback(MakeCallback(&Rpl::Receive, this));
    socket->SetRecvPktInfo(true);
    m_sockets[socket] = interface;
    NS_LOG_LOGIC("RPL: bound ICMPv6 socket on " << linkLocal << " if " << interface);

    if (!m_multicastRecvSocket)
    {
        m_multicastRecvSocket =
            Socket::CreateSocket(node, Ipv6RawSocketFactory::GetTypeId());
        m_multicastRecvSocket->SetAttribute("Protocol",
                                            UintegerValue(Ipv6Header::IPV6_ICMPV6));
        m_multicastRecvSocket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), 0));
        m_multicastRecvSocket->Ipv6JoinGroup(RPL_ALL_NODES_MULTICAST);
        m_multicastRecvSocket->SetRecvCallback(MakeCallback(&Rpl::Receive, this));
        m_multicastRecvSocket->SetRecvPktInfo(true);
        NS_LOG_LOGIC("RPL: joined all-RPL-nodes multicast " << RPL_ALL_NODES_MULTICAST);
    }
}

void
Rpl::UnbindFromInterface(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    for (auto it = m_sockets.begin(); it != m_sockets.end();)
    {
        if (it->second == interface)
        {
            it->first->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            it->first->Close();
            it = m_sockets.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void
Rpl::Receive(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        Inet6SocketAddress inetAddr = Inet6SocketAddress::ConvertFrom(from);
        Ipv6Address src = inetAddr.GetIpv6();

        uint32_t incomingIf = 0;
        Ipv6PacketInfoTag interfaceInfo;
        if (packet->RemovePacketTag(interfaceInfo))
        {
            // RecvIf is NetDevice::GetIfIndex(); map to IPv6 interface index.
            for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
            {
                if (m_ipv6->GetNetDevice(i)->GetIfIndex() == interfaceInfo.GetRecvIf())
                {
                    incomingIf = i;
                    break;
                }
            }
        }

        Ipv6Header ipv6Hdr;
        packet->RemoveHeader(ipv6Hdr);
        Ipv6Address dst = ipv6Hdr.GetDestination();

        Icmpv6Header icmp;
        packet->RemoveHeader(icmp);

        if (icmp.GetType() != RPL_ICMPV6_TYPE)
        {
            NS_LOG_LOGIC("Ignoring non-RPL ICMPv6 type=" << int(icmp.GetType()));
            continue;
        }

        auto code = static_cast<RplIcmpv6Code>(icmp.GetCode());
        if (code == RplIcmpv6Code::DIO)
        {
            HandleDio(packet, src, dst, incomingIf);
        }
        else
        {
            NS_LOG_LOGIC("Ignoring RPL code=" << int(icmp.GetCode())
                                             << " (DIS/DAO not implemented)");
        }
    }
}

void
Rpl::HandleDio(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t /*interface*/)
{
    NS_LOG_FUNCTION(this << src << dst);

    // Ignore our own multicast copies.
    for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < m_ipv6->GetNAddresses(i); ++j)
        {
            if (m_ipv6->GetAddress(i, j).GetAddress() == src)
            {
                NS_LOG_LOGIC("Ignoring own DIO from " << src);
                return;
            }
        }
    }

    DioBaseObjectHeader dio;
    if (packet->GetSize() < dio.GetSerializedSize())
    {
        NS_LOG_WARN("DIO too short from " << src);
        return;
    }
    packet->RemoveHeader(dio);

    NS_LOG_INFO("DIO Rx from " << src << " dst=" << dst << " " << dio);
    m_dioRxTrace(packet, src, dst, dio);
}

DioBaseObjectHeader
Rpl::BuildDioHeader() const
{
    DioBaseObjectHeader dio;
    dio.SetRplInstanceId(m_rplInstanceId);
    dio.SetVersionNumber(m_versionNumber);
    dio.SetRank(m_rank);
    dio.SetGrounded(true);
    dio.SetModeOfOperation(m_mop);
    dio.SetDodagPreference(m_dodagPreference);
    dio.SetDtsn(m_dtsn);
    dio.SetDodagId(m_dodagId);
    return dio;
}

void
Rpl::SendDio()
{
    NS_LOG_FUNCTION(this);

    if (!m_isRoot)
    {
        return;
    }

    if (m_dodagId.IsAny())
    {
        for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
        {
            Ipv6Address id = SelectDodagId(i);
            if (!id.IsAny())
            {
                m_dodagId = id;
                break;
            }
        }
    }

    if (m_dodagId.IsAny())
    {
        NS_LOG_WARN("Cannot send DIO: DODAGID not set yet");
        ScheduleNextDio();
        return;
    }

    for (const auto& entry : m_sockets)
    {
        SendDioOnInterface(entry.second, entry.first);
    }

    ScheduleNextDio();
}

void
Rpl::SendDioOnInterface(uint32_t interface, Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << interface << socket);

    Ipv6Address src = GetLinkLocalAddress(interface);
    if (src.IsAny())
    {
        NS_LOG_LOGIC("Skip DIO on if " << interface << ": no link-local");
        return;
    }

    DioBaseObjectHeader dio = BuildDioHeader();
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(dio);

    Icmpv6Header icmp;
    icmp.SetType(RPL_ICMPV6_TYPE);
    icmp.SetCode(static_cast<uint8_t>(RplIcmpv6Code::DIO));
    uint16_t payloadLen = icmp.GetSerializedSize() + dio.GetSerializedSize();
    icmp.CalculatePseudoHeaderChecksum(src,
                                       RPL_ALL_NODES_MULTICAST,
                                       payloadLen,
                                       Ipv6Header::IPV6_ICMPV6);
    packet->AddHeader(icmp);

    // \RFC{6550} Sec. 6: RPL Control messages use Hop Limit 255.
    SocketIpv6HopLimitTag hopLimitTag;
    hopLimitTag.SetHopLimit(255);
    packet->AddPacketTag(hopLimitTag);

    NS_LOG_INFO("DIO Tx if=" << interface << " src=" << src << " dst=" << RPL_ALL_NODES_MULTICAST
                             << " " << dio);
    m_dioTxTrace(packet, src, RPL_ALL_NODES_MULTICAST, dio);

    int sent = socket->SendTo(packet, 0, Inet6SocketAddress(RPL_ALL_NODES_MULTICAST, 0));
    if (sent <= 0)
    {
        NS_LOG_WARN("DIO SendTo failed on interface " << interface);
    }
}

void
Rpl::ScheduleNextDio()
{
    NS_LOG_FUNCTION(this);
    m_dioTimerEvent.Cancel();
    if (m_isRoot && m_dioInterval.IsStrictlyPositive())
    {
        m_dioTimerEvent = Simulator::Schedule(m_dioInterval, &Rpl::SendDio, this);
    }
}

Ipv6Address
Rpl::SelectDodagId(uint32_t interface) const
{
    Ipv6Address linkLocal = Ipv6Address::GetZero();
    for (uint32_t j = 0; j < m_ipv6->GetNAddresses(interface); ++j)
    {
        Ipv6InterfaceAddress addr = m_ipv6->GetAddress(interface, j);
        if (addr.GetScope() == Ipv6InterfaceAddress::GLOBAL)
        {
            return addr.GetAddress();
        }
        if (addr.GetScope() == Ipv6InterfaceAddress::LINKLOCAL)
        {
            linkLocal = addr.GetAddress();
        }
    }
    return linkLocal;
}

Ipv6Address
Rpl::GetLinkLocalAddress(uint32_t interface) const
{
    for (uint32_t j = 0; j < m_ipv6->GetNAddresses(interface); ++j)
    {
        Ipv6InterfaceAddress addr = m_ipv6->GetAddress(interface, j);
        if (addr.GetScope() == Ipv6InterfaceAddress::LINKLOCAL)
        {
            return addr.GetAddress();
        }
    }
    return Ipv6Address::GetZero();
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
