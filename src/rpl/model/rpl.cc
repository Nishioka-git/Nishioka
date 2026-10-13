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
#include "ns3/uinteger.h"

#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Rpl");

NS_OBJECT_ENSURE_REGISTERED(Rpl);

/// Root が広告する既定 Rank。
static constexpr uint16_t RPL_ROOT_RANK = 1;
/// 親に接続したときの自 Rank 増分（自 Rank = 親 Rank + この値）。
static constexpr uint16_t RPL_RANK_INCREASE = 1;

Rpl::Rpl()
    : m_ipv6(nullptr),
      m_isRoot(false),
      m_initialized(false),
      m_multicastRecvSocket(nullptr),
      m_dioIntervalMin(3),
      m_dioIntervalDoublings(20),
      m_dioRedundancy(10),
      m_trickleStarted(false),
      m_disInterval(Seconds(0.5)),
      m_disMaxAttempts(5),
      m_disAttempts(0),
      m_rplInstanceId(0),
      m_versionNumber(1),
      m_dtsn(0),
      m_rank(RPL_ROOT_RANK),
      m_mop(ModeOfOperation::NO_DOWNWARD_ROUTES),
      m_dodagPreference(0),
      m_dodagId(Ipv6Address::GetZero()),
      m_joined(false),
      m_preferredParent(Ipv6Address::GetZero()),
      m_parentInterface(0),
      m_parentRank(0),
      m_pathMetric(0),
      m_parentPathMetric(0),
      m_disTx(0),
      m_disRx(0),
      m_dioTx(0),
      m_dioRx(0)
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
            .AddAttribute(
                "DioIntervalMin",
                "RFC 6550 DIOIntervalMin. Imin is 2^DioIntervalMin milliseconds.",
                UintegerValue(3),
                MakeUintegerAccessor(&Rpl::m_dioIntervalMin),
                MakeUintegerChecker<uint8_t>(0, 20))
            .AddAttribute(
                "DioIntervalDoublings",
                "RFC 6550 DIOIntervalDoublings. Imax is Imin * 2^DioIntervalDoublings.",
                UintegerValue(20),
                MakeUintegerAccessor(&Rpl::m_dioIntervalDoublings),
                MakeUintegerChecker<uint8_t>(0, 20))
            .AddAttribute(
                "DioRedundancyConstant",
                "RFC 6550 DIORedundancyConstant. Zero disables Trickle suppression.",
                UintegerValue(10),
                MakeUintegerAccessor(&Rpl::m_dioRedundancy),
                MakeUintegerChecker<uint16_t>())
            .AddAttribute("DisInterval",
                          "Interval between DIS probes while the node is unjoined.",
                          TimeValue(Seconds(0.5)),
                          MakeTimeAccessor(&Rpl::m_disInterval),
                          MakeTimeChecker())
            .AddAttribute("DisMaxAttempts",
                          "Maximum number of DIS transmissions while unjoined (0 = disabled).",
                          UintegerValue(5),
                          MakeUintegerAccessor(&Rpl::m_disMaxAttempts),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RplInstanceId",
                          "RPLInstanceID advertised in DIO messages.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&Rpl::m_rplInstanceId),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("VersionNumber",
                          "DODAG Version Number advertised in DIO messages.",
                          UintegerValue(1),
                          MakeUintegerAccessor(&Rpl::m_versionNumber),
                          MakeUintegerChecker<uint8_t>());
    return tid;
}

void
Rpl::DoDispose()  //Dispose obhect
{
    NS_LOG_FUNCTION(this);

    StopTrickle();
    m_disTimerEvent.Cancel();

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
    m_routingTable.Dispose();
    Ipv6RoutingProtocol::DoDispose();
}

void
Rpl::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    m_initialized = true;  //Add RPL to available IFs(111-132)

    NS_ASSERT_MSG(m_ipv6, "SetIpv6 must be called before DoInitialize");
    ConfigureTrickle();

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
        // Root は親なしで参加済み扱いし、DIO 送信を開始する。
        m_joined = true;
        m_preferredParent = Ipv6Address::GetZero();
        m_parentRank = 0;
        m_parentPathMetric = 0;
        m_pathMetric = 0;
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
        StartTrickle();
    }
    else
    {
        // 未参加ノードは DIS で近傍の DIO を催促する。
        ScheduleNextDis();
    }

    Ipv6RoutingProtocol::DoInitialize();
}

void
Rpl::SetIpv6(Ptr<Ipv6> ipv6)
{
    NS_LOG_FUNCTION(this << ipv6);
    m_ipv6 = ipv6;
}

Ptr<Ipv6Route>  //Determines the route for packets sent by this node
Rpl::RouteOutput(Ptr<Packet> p,
                 const Ipv6Header& header,
                 Ptr<NetDevice> oif,
                 Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << header.GetDestination() << oif);

    Ptr<Ipv6Route> rtentry = Lookup(header.GetDestination(), true, oif);  //search routingtable
    if (rtentry)
    {
        sockerr = Socket::ERROR_NOTERROR;
    }
    else
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
        NS_LOG_WARN("RouteOutput FAIL dst=" << header.GetDestination() << " NOROUTETOHOST");
    }
    return rtentry;
}

bool  //Determine whether to forward packets received from other sources
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
        if (!ecb.IsNull())
        {
            ecb(p, header, Socket::ERROR_NOROUTETOHOST);  //ecb is the error callback
        }
        return false;
    }

    if (!m_ipv6->IsForwarding(iif))
    {
        if (!ecb.IsNull())
        {
            ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        }
        return true;
    }

    Ptr<Ipv6Route> rtentry = Lookup(header.GetDestination(), false, nullptr);
    if (rtentry)
    {
        ucb(idev, rtentry, p, header);  //ucb is the unicast forward callback
        return true;
    }

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

    m_ipv6->SetForwarding(interface, true);  //create socket
    BindToInterface(interface);

    if (m_isRoot && m_dodagId.IsAny())  //determine the IF
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

    if (m_initialized && address.GetScope() == Ipv6InterfaceAddress::LINKLOCAL)  //when initialized
    {
        BindToInterface(interface);
        if (!m_isRoot && !m_joined)
        {
            ScheduleNextDis();
        }
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
//Printable Routing Table (Required override for IPv6RoutingProtocol)
{
    NS_LOG_FUNCTION(this);

    std::ostream* os = stream->GetStream();
    *os << "Rpl routing table (Node " << GetObject<Node>()->GetId() << ")\n";
    *os << "  IsRoot: " << m_isRoot << " Joined: " << m_joined << "\n";
    *os << "  DODAGID: " << m_dodagId << " Rank: " << m_rank << "\n";
    *os << "  Parent: " << m_preferredParent << " parentRank=" << m_parentRank
        << " parentHops=" << m_parentPathMetric << " hops=" << m_pathMetric
        << " if=" << m_parentInterface << "\n";
    *os << "  DIS tx/rx=" << m_disTx << "/" << m_disRx << " DIO tx/rx=" << m_dioTx << "/"
        << m_dioRx << "\n";
    m_routingTable.Print(stream);
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
        // Root は親なしで参加済み扱いし、DIO 送信を開始する。
        m_joined = true;
        m_preferredParent = Ipv6Address::GetZero();
        m_parentRank = 0;
        m_parentPathMetric = 0;
        m_pathMetric = 0;
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
        StartTrickle();
        m_disTimerEvent.Cancel();
    }
    else
    {
        StopTrickle();
        if (!m_joined)
        {
            ScheduleNextDis();
        }
    }
}

bool
Rpl::IsRoot() const
{
    return m_isRoot;
}

bool
Rpl::IsJoined() const
{
    return m_joined;
}

Ipv6Address
Rpl::GetPreferredParent() const
{
    return m_preferredParent;
}

uint16_t
Rpl::GetRank() const
{
    return m_rank;
}

uint16_t
Rpl::GetPathMetric() const
{
    return m_pathMetric;
}

uint32_t
Rpl::GetDisTxCount() const
{
    return m_disTx;
}

uint32_t
Rpl::GetDisRxCount() const
{
    return m_disRx;
}

uint32_t
Rpl::GetDioTxCount() const
{
    return m_dioTx;
}

uint32_t
Rpl::GetDioRxCount() const
{
    return m_dioRx;
}

void
Rpl::BindToInterface(uint32_t interface)   //Create socket for ICMPv6
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
        Ipv6Address srcFrom = inetAddr.GetIpv6();

        uint32_t incomingIf = 0;
        Ipv6PacketInfoTag interfaceInfo;
        if (packet->RemovePacketTag(interfaceInfo))
        {
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
        Ipv6Address src = ipv6Hdr.GetSource().IsAny() ? srcFrom : ipv6Hdr.GetSource();

        Icmpv6Header icmp;
        packet->RemoveHeader(icmp);
        if (icmp.GetType() != RPL_ICMPV6_TYPE)
        {
            continue;
        }

        auto code = static_cast<RplIcmpv6Code>(icmp.GetCode());
        if (code == RplIcmpv6Code::DIO)
        {
            HandleDio(packet, src, dst, incomingIf);
        }
        else if (code == RplIcmpv6Code::DIS)
        {
            HandleDis(packet, src, dst, incomingIf);
        }
        else
        {
            NS_LOG_LOGIC("Ignore unimplemented RPL code " << int(icmp.GetCode()));
        }
    }
}

void
Rpl::HandleDio(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface)
{
    NS_LOG_FUNCTION(this << src << dst << interface);

    if (IsLocalAddress(src))
    {
        return;
    }

    ++m_dioRx;
    if (m_isRoot)
    {
        return;
    }

    DioBaseObjectHeader dio;
    if (packet->GetSize() < dio.GetSerializedSize())
    {
        NS_LOG_WARN("Drop DIO: too short size=" << packet->GetSize());
        return;
    }
    packet->RemoveHeader(dio);

    RplHopCountHeader hops;
    if (packet->GetSize() < hops.GetSerializedSize())
    {
        NS_LOG_WARN("Drop DIO: hop-count path metric missing");
        return;
    }
    packet->RemoveHeader(hops);

    if (dio.GetRplInstanceId() != m_rplInstanceId)
    {
        return;
    }

    if (m_joined && dio.GetDodagId() != m_dodagId)
    {
        return;
    }

    const uint16_t advertisedHops = hops.GetHopCount();
    const bool lowerRank = dio.GetRank() < m_parentRank;
    const bool sameRankFewerHops =
        dio.GetRank() == m_parentRank && advertisedHops < m_parentPathMetric;
    const uint8_t previousVersion = m_versionNumber;
    const bool wasJoined = m_joined;
    if (!m_joined || lowerRank || sameRankFewerHops)
    {
        AcceptParent(src, interface, dio, advertisedHops);
        // 新しい DODAG Version への参加は Trickle の不整合（\RFC{6550} Sec. 8.3）。
        if (wasJoined && dio.GetVersionNumber() != previousVersion)
        {
            m_trickle.InconsistentEvent();
        }
    }
    else if (m_trickleStarted && dio.GetRank() < m_rank)
    {
        // より根に近い Rank で、親も自 Rank も変わらない DIO は consistent。
        m_trickle.ConsistentEvent();
    }
}

void
Rpl::HandleDis(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface)
{
    NS_LOG_FUNCTION(this << src << dst << interface);

    if (IsLocalAddress(src))
    {
        return;
    }

    ++m_disRx;

    // 未参加ノードは応答する DIO を持たない。受信したことは数える。
    if (!m_joined)
    {
        return;
    }

    DisBaseObjectHeader dis;
    if (packet->GetSize() < dis.GetSerializedSize())
    {
        NS_LOG_WARN("Drop DIS: too short size=" << packet->GetSize());
        return;
    }
    packet->RemoveHeader(dis);

    const uint32_t nodeId = GetObject<Node>()->GetId();
    const bool unicast = !dst.IsMulticast();

    std::cout << Simulator::Now().As(Time::S) << " [RPL DIS Rx] node=" << nodeId << " src=" << src
              << " dst=" << dst << " unicast=" << unicast << "\n";

    // \RFC{6550} Sec. 8.3: multicast DIS resets Trickle. It does not emit a DIO immediately.
    if (!unicast)
    {
        if (m_trickleStarted)
        {
            m_trickle.InconsistentEvent();
        }
        return;
    }

    Ptr<Socket> socket = nullptr;
    for (const auto& entry : m_sockets)
    {
        if (entry.second == interface)
        {
            socket = entry.first;
            break;
        }
    }
    if (!socket && !m_sockets.empty())
    {
        socket = m_sockets.begin()->first;
        interface = m_sockets.begin()->second;
    }
    if (!socket)
    {
        NS_LOG_WARN("No socket to reply DIO for DIS");
        return;
    }

    // ユニキャスト DIS は Trickle をリセットせず、送信元へ DIO を1回返す。
    SendDioOnInterface(interface, socket, src);
}

void
Rpl::AcceptParent(Ipv6Address parent,
                  uint32_t interface,
                  const DioBaseObjectHeader& dio,
                  uint16_t parentHopCount)
{
    NS_LOG_FUNCTION(this << parent << interface << dio.GetRank() << parentHopCount);

    const bool firstJoin = !m_joined;

    // 親・DODAG 情報を反映し、自 Rank を親 Rank+1、ホップ数を親+1 にする。
    m_joined = true;
    m_dodagId = dio.GetDodagId();
    m_preferredParent = parent;       // DIO 送信元の link-local
    m_parentInterface = interface;    // 受信 IF = 親方向
    m_parentRank = dio.GetRank();
    m_parentPathMetric = parentHopCount;
    m_rank = static_cast<uint16_t>(m_parentRank + RPL_RANK_INCREASE);
    m_pathMetric = static_cast<uint16_t>(parentHopCount + 1);
    m_versionNumber = dio.GetVersionNumber();
    m_mop = dio.GetModeOfOperation();
    m_dodagPreference = dio.GetDodagPreference();
    m_dtsn = dio.GetDtsn();

    InstallParentDefaultRoute();

    m_disTimerEvent.Cancel();

    std::cout << Simulator::Now().As(Time::S) << " [RPL Parent] node=" << GetObject<Node>()->GetId()
              << " parent=" << m_preferredParent << " parentRank=" << m_parentRank
              << " parentHops=" << m_parentPathMetric << " myRank=" << m_rank
              << " hops=" << m_pathMetric << " DODAGID=" << m_dodagId << "\n";

    // 初回参加で Trickle を Imin から開始する（\RFC{6550} Sec. 8.3）。
    if (firstJoin)
    {
        StartTrickle();
    }
}

void
Rpl::InstallParentDefaultRoute()
{
    NS_LOG_FUNCTION(this);
    // 既存 ::/0 を消し、親経由のデフォルトルートを入れる。
    RemoveNetworkRoute(Ipv6Address("::"), Ipv6Prefix::GetZero());
    AddDefaultRouteTo(m_preferredParent, m_parentInterface);
}

bool
Rpl::IsLocalAddress(Ipv6Address addr) const
{
    for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < m_ipv6->GetNAddresses(i); ++j)
        {
            if (m_ipv6->GetAddress(i, j).GetAddress() == addr)
            {
                return true;
            }
        }
    }
    return false;
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
Rpl::SendDis()
{
    NS_LOG_FUNCTION(this);

    if (m_joined || m_isRoot)
    {
        return;
    }

    if (m_disMaxAttempts == 0 || m_disAttempts >= m_disMaxAttempts)
    {
        return;
    }

    if (m_sockets.empty())
    {
        ScheduleNextDis();
        return;
    }

    for (const auto& entry : m_sockets)
    {
        SendDisOnInterface(entry.second, entry.first);
    }
    ++m_disAttempts;
    ScheduleNextDis();
}

void
Rpl::SendDisOnInterface(uint32_t interface, Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << interface << socket);

    Ipv6Address src = GetLinkLocalAddress(interface);
    if (src.IsAny())
    {
        NS_LOG_LOGIC("Skip DIS on if " << interface << ": no link-local");
        return;
    }

    DisBaseObjectHeader dis;
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(dis);

    Icmpv6Header icmp;
    icmp.SetType(RPL_ICMPV6_TYPE);
    icmp.SetCode(static_cast<uint8_t>(RplIcmpv6Code::DIS));
    uint16_t payloadLen = icmp.GetSerializedSize() + dis.GetSerializedSize();
    icmp.CalculatePseudoHeaderChecksum(src,
                                       RPL_ALL_NODES_MULTICAST,
                                       payloadLen,
                                       Ipv6Header::IPV6_ICMPV6);
    packet->AddHeader(icmp);

    SocketIpv6HopLimitTag hopLimitTag;
    hopLimitTag.SetHopLimit(255);
    packet->AddPacketTag(hopLimitTag);

    ++m_disTx;
    std::cout << Simulator::Now().As(Time::S) << " [RPL DIS Tx] node=" << GetObject<Node>()->GetId()
              << " if=" << interface << " src=" << src << " dst=" << RPL_ALL_NODES_MULTICAST
              << " attempt=" << (m_disAttempts + 1) << "/" << m_disMaxAttempts << "\n";

    int sent = socket->SendTo(packet, 0, Inet6SocketAddress(RPL_ALL_NODES_MULTICAST, 0));
    if (sent <= 0)
    {
        NS_LOG_WARN("DIS SendTo failed on interface " << interface);
    }
}

void
Rpl::ScheduleNextDis()
{
    NS_LOG_FUNCTION(this);
    m_disTimerEvent.Cancel();
    if (m_joined || m_isRoot)
    {
        return;
    }
    if (m_disMaxAttempts == 0 || m_disAttempts >= m_disMaxAttempts)
    {
        return;
    }
    if (!m_disInterval.IsStrictlyPositive())
    {
        return;
    }
    m_disTimerEvent = Simulator::Schedule(m_disInterval, &Rpl::SendDis, this);
}

void
Rpl::SendDio()
{
    NS_LOG_FUNCTION(this);

    // 未参加ノードは DIO を送らない（root/参加済みのみ）。
    if (!m_joined)
    {
        return;
    }

    if (m_isRoot && m_dodagId.IsAny())
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
        return;
    }

    for (const auto& entry : m_sockets)
    {
        SendDioOnInterface(entry.second, entry.first, RPL_ALL_NODES_MULTICAST);
    }
}

void
Rpl::SendDioOnInterface(uint32_t interface, Ptr<Socket> socket, Ipv6Address dst)
{
    NS_LOG_FUNCTION(this << interface << socket << dst);

    if (!m_joined)
    {
        return;
    }

    if (m_isRoot && m_dodagId.IsAny())
    {
        m_dodagId = SelectDodagId(interface);
    }
    if (m_dodagId.IsAny())
    {
        NS_LOG_WARN("Cannot send DIO: DODAGID not set yet");
        return;
    }

    Ipv6Address src = GetLinkLocalAddress(interface);
    if (src.IsAny())
    {
        NS_LOG_LOGIC("Skip DIO on if " << interface << ": no link-local");
        return;
    }

    RplHopCountHeader hops;
    hops.SetHopCount(m_pathMetric);
    DioBaseObjectHeader dio = BuildDioHeader();
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(hops);
    packet->AddHeader(dio);

    Icmpv6Header icmp;
    icmp.SetType(RPL_ICMPV6_TYPE);
    icmp.SetCode(static_cast<uint8_t>(RplIcmpv6Code::DIO));
    uint16_t payloadLen =
        icmp.GetSerializedSize() + dio.GetSerializedSize() + hops.GetSerializedSize();
    icmp.CalculatePseudoHeaderChecksum(src, dst, payloadLen, Ipv6Header::IPV6_ICMPV6);
    packet->AddHeader(icmp);

    // \RFC{6550} Sec. 6: RPL Control messages use Hop Limit 255.
    SocketIpv6HopLimitTag hopLimitTag;
    hopLimitTag.SetHopLimit(255);
    packet->AddPacketTag(hopLimitTag);

    ++m_dioTx;
    std::cout << Simulator::Now().As(Time::S) << " [RPL DIO Tx] node=" << GetObject<Node>()->GetId()
              << " if=" << interface << " src=" << src << " dst=" << dst
              << " Rank=" << dio.GetRank() << " hops=" << hops.GetHopCount()
              << " DODAGID=" << dio.GetDodagId() << "\n";

    int sent = socket->SendTo(packet, 0, Inet6SocketAddress(dst, 0));
    if (sent <= 0)
    {
        NS_LOG_WARN("DIO SendTo failed on interface " << interface);
    }
}

void
Rpl::ConfigureTrickle()
{
    NS_LOG_FUNCTION(this);
    // Imin = 2^DIOIntervalMin ms（\RFC{6550} Sec. 8.3.1）。既定は 8 ms。
    const uint32_t iminMs = 1u << m_dioIntervalMin;
    m_trickle.SetParameters(MilliSeconds(iminMs), m_dioIntervalDoublings, m_dioRedundancy);
    m_trickle.SetFunction(&Rpl::SendDio, this);
}

void
Rpl::StartTrickle()
{
    NS_LOG_FUNCTION(this);
    if (!m_joined || m_trickleStarted)
    {
        return;
    }
    // Enable() は最初の周期を [Imin, Imax] から選ぶ。参加直後は Imin から出す。
    m_trickle.Reset();
    m_trickleStarted = true;
}

void
Rpl::StopTrickle()
{
    NS_LOG_FUNCTION(this);
    m_trickle.Stop();
    m_trickleStarted = false;
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

    int32_t oifIndex = -1;
    if (oif)
    {
        oifIndex = m_ipv6->GetInterfaceForDevice(oif);
    }

    Ptr<RplRoutingTableEntry> found;
    if (m_routingTable.LookUpEntry(dest, found, oifIndex))
    {
        longestMask = found->GetPrefix().GetPrefixLength();
        rtentry = Create<Ipv6Route>();
        if (setSource)
        {
            rtentry->SetSource(m_ipv6->SourceAddressSelection(found->GetInterface(), dest));
        }
        rtentry->SetDestination(found->GetDestination());
        rtentry->SetGateway(found->GetNextHop());
        rtentry->SetOutputDevice(m_ipv6->GetNetDevice(found->GetInterface()));
        NS_LOG_LOGIC("Lookup dst=" << dest << " match=" << found->GetDestination() << "/"
                                   << int(longestMask) << " via " << found->GetNextHop());
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

    RplRouteType type = RplRouteType::DOWNWARD;
    if (nextHop.IsAny())
    {
        type = RplRouteType::LOCAL;
    }
    else if (network.IsAny() && networkPrefix.GetPrefixLength() == 0)
    {
        type = RplRouteType::UPWARD;
    }

    Ptr<RplRoutingTableEntry> existing;
    if (m_routingTable.LookUpExact(network, networkPrefix, existing))
    {
        existing->SetNextHop(nextHop);
        existing->SetInterface(interface);
        existing->SetType(type);
        existing->SetStatus(RplRouteStatus::VALID);
        return;
    }

    Ptr<RplRoutingTableEntry> entry =
        Create<RplRoutingTableEntry>(network, networkPrefix, nextHop, interface, type);
    if (!m_routingTable.AddEntry(entry))
    {
        NS_LOG_WARN("Routing table full, cannot add " << network << "/"
                                                      << int(networkPrefix.GetPrefixLength()));
    }
}

void
Rpl::RemoveNetworkRoute(Ipv6Address network, Ipv6Prefix networkPrefix)
{
    NS_LOG_FUNCTION(this << network << networkPrefix);

    m_routingTable.Delete(network, networkPrefix);
}

void
Rpl::InvalidateRoutesOnInterface(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    m_routingTable.DeleteByInterface(interface);
}

} // namespace ns3
