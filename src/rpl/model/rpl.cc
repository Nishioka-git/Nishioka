#include "rpl.h"

#include "ns3/boolean.h"
#include "ns3/enum.h"
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
      m_daoDelay(Seconds(1)),
      m_daoSequence(0),
      m_pathSequence(0)
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
            .AddAttribute("DioIntervalMin",
                          "RFC 6550 DIOIntervalMin. Trickle Imin is 2^DioIntervalMin milliseconds.",
                          UintegerValue(3),
                          MakeUintegerAccessor(&Rpl::m_dioIntervalMin),
                          MakeUintegerChecker<uint8_t>(0, 20))
            .AddAttribute("DioIntervalDoublings",
                          "RFC 6550 DIOIntervalDoublings. Trickle Imax is Imin * 2^DioIntervalDoublings.",
                          UintegerValue(20),
                          MakeUintegerAccessor(&Rpl::m_dioIntervalDoublings),
                          MakeUintegerChecker<uint8_t>(0, 20))
            .AddAttribute("DioRedundancyConstant",
                          "RFC 6550 DIORedundancyConstant k. Zero disables Trickle suppression.",
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
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("ModeOfOperation",
                          "RFC 6550 MOP advertised by the root in DIO. Non-root nodes adopt the "
                          "value from the DIO of their parent. 2 = Storing mode (DAO enabled).",
                          EnumValue<ModeOfOperation>(ModeOfOperation::NO_DOWNWARD_ROUTES),
                          MakeEnumAccessor<ModeOfOperation>(&Rpl::m_mop),
                          MakeEnumChecker(ModeOfOperation::NO_DOWNWARD_ROUTES,
                                          "NoDownwardRoutes",
                                          ModeOfOperation::NON_STORING,
                                          "NonStoring",
                                          ModeOfOperation::STORING_NO_MULTICAST,
                                          "StoringNoMulticast",
                                          ModeOfOperation::STORING_WITH_MULTICAST,
                                          "StoringWithMulticast"))
            .AddAttribute("DaoDelay",
                          "RFC 6550 DelayDAO: time between a route change and the DAO that "
                          "advertises it (changes within this time are aggregated).",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&Rpl::m_daoDelay),
                          MakeTimeChecker());
    return tid;
}

void
Rpl::DoDispose()  //Dispose obhect
{
    NS_LOG_FUNCTION(this);

    StopTrickle();
    m_disTimerEvent.Cancel();
    m_daoTimerEvent.Cancel();

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
        // on-link のときだけ「同一リンク（直接到達）」の /prefix ルートを入れる。
        // route-over のマルチホップでは、同じ /64 に属するアドレスでも隣接とは限らない。
        // on-link でないアドレスは DAO で学習した /128 の Downward ルートだけで到達する。
        if (address.GetOnLink())
        {
            Ipv6Address networkAddress = address.GetAddress().CombinePrefix(address.GetPrefix());
            AddNetworkRouteTo(networkAddress,
                              address.GetPrefix(),
                              Ipv6Address::GetZero(),
                              interface);
        }

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

    if (address.GetScope() == Ipv6InterfaceAddress::GLOBAL && address.GetOnLink())
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
        << " if=" << m_parentInterface << "\n";
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

bool
Rpl::HasDownwardRoute(Ipv6Address dest)
{
    Ptr<RplRoutingTableEntry> found;
    if (!m_routingTable.LookUpEntry(dest, found))
    {
        return false;
    }
    return found->GetType() == RplRouteType::DOWNWARD &&
           found->GetStatus() == RplRouteStatus::VALID;
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

        // ::（任意アドレス）にバインドした受信ソケットはユニキャストも拾う。ユニキャストは
        // IF ごとのソケット（link-local にバインド）が受けるので、ここでは捨てて二重処理を防ぐ。
        if (socket == m_multicastRecvSocket && !dst.IsMulticast())
        {
            continue;
        }

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
        else if (code == RplIcmpv6Code::DAO)
        {
            HandleDao(packet, src, dst, incomingIf);
        }
        else if (code == RplIcmpv6Code::DAO_ACK)
        {
            HandleDaoAck(packet, src, dst, incomingIf);
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

    DioBaseObjectHeader dio;
    if (packet->GetSize() < dio.GetSerializedSize())
    {
        NS_LOG_WARN("Drop DIO: too short size=" << packet->GetSize());
        return;
    }
    packet->RemoveHeader(dio);

    if (dio.GetRplInstanceId() != m_rplInstanceId)
    {
        return;
    }

    if (m_joined && dio.GetDodagId() != m_dodagId)
    {
        return;
    }

    // Root は親を持たない。同じ DODAG の DIO は Trickle の consistent として数える。
    if (m_isRoot)
    {
        if (m_trickleStarted)
        {
            m_trickle.ConsistentEvent();
        }
        return;
    }

    if (!m_joined || dio.GetRank() < m_parentRank)
    {
        AcceptParent(src, interface, dio);
    }
    else if (m_trickleStarted)
    {
        // 親も自 Rank も変えない DIO は consistent（\RFC{6550} Sec. 8.3, \RFC{6206} Sec. 4.2）。
        // 近傍の DIO 数が k 以上なら、この周期の自 DIO 送信は抑制される。
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

    // 未参加ノードは応答する DIO を持たない。
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

    // \RFC{6550} Sec. 8.3: マルチキャスト DIS は不整合イベント。Trickle を Imin に戻し、
    // 直後（Imin/2 ～ Imin）の周期 DIO で応答する。ここで DIO は即送しない。
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

    // ユニキャスト DIS は Trickle に触れず、送信元へユニキャスト DIO を1回返す。
    SendDioOnInterface(interface, socket, src);
}

void
Rpl::AcceptParent(Ipv6Address parent, uint32_t interface, const DioBaseObjectHeader& dio)
{
    NS_LOG_FUNCTION(this << parent << interface << dio.GetRank());

    const bool firstJoin = !m_joined;

    // 親・DODAG 情報を反映し、自 Rank を親 Rank+1 にする。
    m_joined = true;
    m_dodagId = dio.GetDodagId();
    m_preferredParent = parent;       // DIO 送信元の link-local
    m_parentInterface = interface;    // 受信 IF = 親方向
    m_parentRank = dio.GetRank();
    m_rank = static_cast<uint16_t>(m_parentRank + RPL_RANK_INCREASE);
    m_versionNumber = dio.GetVersionNumber();
    m_mop = dio.GetModeOfOperation();
    m_dodagPreference = dio.GetDodagPreference();
    m_dtsn = dio.GetDtsn();

    InstallParentDefaultRoute();

    m_disTimerEvent.Cancel();

    std::cout << Simulator::Now().As(Time::S) << " [RPL Parent] node=" << GetObject<Node>()->GetId()
              << " parent=" << m_preferredParent << " parentRank=" << m_parentRank
              << " myRank=" << m_rank << " DODAGID=" << m_dodagId << "\n";

    if (firstJoin)
    {
        // 初回参加：Imin から Trickle を開始し、次ホップへ DODAG 情報を素早く広げる。
        StartTrickle();
    }
    else if (m_trickleStarted)
    {
        // 親の切替で自 Rank が変わった：周囲に知らせるため Imin に戻す。
        m_trickle.InconsistentEvent();
    }

    // 親が決まった（または変わった）ので、自分と配下の宛先を新しい親へ DAO で知らせる。
    ScheduleDao();
}

bool
Rpl::IsStoringMode() const
{
    return m_mop == ModeOfOperation::STORING_NO_MULTICAST ||
           m_mop == ModeOfOperation::STORING_WITH_MULTICAST;
}

Ptr<Socket>
Rpl::GetSocketForInterface(uint32_t interface) const
{
    for (const auto& entry : m_sockets)
    {
        if (entry.second == interface)
        {
            return entry.first;
        }
    }
    return nullptr;
}

void
Rpl::ScheduleDao()
{
    NS_LOG_FUNCTION(this);

    if (m_isRoot || !m_joined || !IsStoringMode())
    {
        return;
    }
    if (m_daoTimerEvent.IsPending())
    {
        return; // DelayDAO 中の変更は 1 つの DAO にまとめる
    }
    m_daoTimerEvent = Simulator::Schedule(m_daoDelay, &Rpl::SendDao, this);
}

void
Rpl::SendDao()
{
    NS_LOG_FUNCTION(this);

    if (m_isRoot || !m_joined || m_preferredParent.IsAny() || !IsStoringMode())
    {
        return;
    }

    Ptr<Socket> socket = GetSocketForInterface(m_parentInterface);
    Ipv6Address src = GetLinkLocalAddress(m_parentInterface);
    if (!socket || src.IsAny())
    {
        NS_LOG_WARN("Cannot send DAO: no socket/link-local on parent interface");
        return;
    }

    // Target = 自ノードの global アドレス + すでに保持している Downward ルートの宛先（Storing mode）
    std::vector<std::pair<Ipv6Address, uint8_t>> targets;
    for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < m_ipv6->GetNAddresses(i); ++j)
        {
            Ipv6InterfaceAddress addr = m_ipv6->GetAddress(i, j);
            if (addr.GetScope() == Ipv6InterfaceAddress::GLOBAL)
            {
                targets.emplace_back(addr.GetAddress(), 128);
            }
        }
    }
    for (const auto& entry : m_routingTable.GetEntries())
    {
        if (entry->GetType() == RplRouteType::DOWNWARD &&
            entry->GetStatus() == RplRouteStatus::VALID)
        {
            targets.emplace_back(entry->GetDestination(), entry->GetPrefix().GetPrefixLength());
        }
    }
    if (targets.empty())
    {
        return;
    }

    // Header は先頭に積まれるので、後ろのオプションから順に追加する。
    // 最終形: [ICMPv6][DAO base][Target]...[Target][Transit Information]
    Ptr<Packet> packet = Create<Packet>();

    TransitInformationOptionHeader transit;
    transit.SetPathSequence(++m_pathSequence);
    transit.SetPathLifetime(0xFF); // infinite
    packet->AddHeader(transit);

    for (auto it = targets.rbegin(); it != targets.rend(); ++it)
    {
        RplTargetOptionHeader target;
        target.SetTarget(it->first, it->second);
        packet->AddHeader(target);
    }

    DaoBaseObjectHeader dao;
    dao.SetRplInstanceId(m_rplInstanceId);
    dao.SetK(true);
    dao.SetDaoSequence(++m_daoSequence);
    packet->AddHeader(dao);

    Icmpv6Header icmp;
    icmp.SetType(RPL_ICMPV6_TYPE);
    icmp.SetCode(static_cast<uint8_t>(RplIcmpv6Code::DAO));
    uint16_t payloadLen = icmp.GetSerializedSize() + packet->GetSize();
    icmp.CalculatePseudoHeaderChecksum(src,
                                       m_preferredParent,
                                       payloadLen,
                                       Ipv6Header::IPV6_ICMPV6);
    packet->AddHeader(icmp);

    SocketIpv6HopLimitTag hopLimitTag;
    hopLimitTag.SetHopLimit(255);
    packet->AddPacketTag(hopLimitTag);

    std::cout << Simulator::Now().As(Time::S) << " [RPL DAO Tx] node=" << GetObject<Node>()->GetId()
              << " if=" << m_parentInterface << " src=" << src << " dst=" << m_preferredParent
              << " seq=" << int(dao.GetDaoSequence()) << " K=1 targets=";
    for (size_t k = 0; k < targets.size(); ++k)
    {
        std::cout << (k ? "," : "") << targets[k].first << "/" << int(targets[k].second);
    }
    std::cout << "\n";

    int sent = socket->SendTo(packet, 0, Inet6SocketAddress(m_preferredParent, 0));
    if (sent <= 0)
    {
        NS_LOG_WARN("DAO SendTo failed on interface " << m_parentInterface);
    }
}

void
Rpl::SendDaoAck(Ipv6Address dst, uint32_t interface, uint8_t daoSequence, uint8_t status)
{
    NS_LOG_FUNCTION(this << dst << interface << int(daoSequence) << int(status));

    Ptr<Socket> socket = GetSocketForInterface(interface);
    Ipv6Address src = GetLinkLocalAddress(interface);
    if (!socket || src.IsAny())
    {
        return;
    }

    DaoAckBaseObjectHeader ack;
    ack.SetRplInstanceId(m_rplInstanceId);
    ack.SetDaoSequence(daoSequence);
    ack.SetStatus(status);

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(ack);

    Icmpv6Header icmp;
    icmp.SetType(RPL_ICMPV6_TYPE);
    icmp.SetCode(static_cast<uint8_t>(RplIcmpv6Code::DAO_ACK));
    uint16_t payloadLen = icmp.GetSerializedSize() + ack.GetSerializedSize();
    icmp.CalculatePseudoHeaderChecksum(src, dst, payloadLen, Ipv6Header::IPV6_ICMPV6);
    packet->AddHeader(icmp);

    SocketIpv6HopLimitTag hopLimitTag;
    hopLimitTag.SetHopLimit(255);
    packet->AddPacketTag(hopLimitTag);

    std::cout << Simulator::Now().As(Time::S) << " [RPL DAO-ACK Tx] node=" << GetObject<Node>()->GetId()
              << " if=" << interface << " src=" << src << " dst=" << dst
              << " seq=" << int(daoSequence) << " status=" << int(status) << "\n";

    int sent = socket->SendTo(packet, 0, Inet6SocketAddress(dst, 0));
    if (sent <= 0)
    {
        NS_LOG_WARN("DAO-ACK SendTo failed on interface " << interface);
    }
}

void
Rpl::HandleDao(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface)
{
    NS_LOG_FUNCTION(this << src << dst << interface);

    if (IsLocalAddress(src) || dst.IsMulticast())
    {
        return;
    }
    if (!m_joined || !IsStoringMode())
    {
        NS_LOG_LOGIC("Ignore DAO: not joined or not in Storing mode");
        return;
    }

    // D フラグ付きなら DODAGID(16B) も含めた長さが必要。
    if (packet->GetSize() < 4)
    {
        NS_LOG_WARN("Drop DAO: too short size=" << packet->GetSize());
        return;
    }
    uint8_t first[2];
    packet->CopyData(first, 2);
    const uint32_t baseSize = (first[1] & 0x40) ? 20 : 4;
    if (packet->GetSize() < baseSize)
    {
        NS_LOG_WARN("Drop DAO: too short size=" << packet->GetSize());
        return;
    }

    DaoBaseObjectHeader dao;
    packet->RemoveHeader(dao);
    if (dao.GetRplInstanceId() != m_rplInstanceId)
    {
        return;
    }

    // オプションを解析する。Transit Information は直前の Target 群に適用される。
    struct Target
    {
        Ipv6Address address;
        uint8_t prefixLength;
        uint8_t pathLifetime;
        uint8_t pathSequence;
    };
    std::vector<Target> targets;
    size_t pendingFrom = 0; // まだ Transit Information が付いていない Target の先頭位置

    while (packet->GetSize() > 0)
    {
        uint8_t type;
        packet->CopyData(&type, 1);
        if (type == static_cast<uint8_t>(DaoOptions::PAD1))
        {
            packet->RemoveAtStart(1);
            continue;
        }
        if (packet->GetSize() < 2)
        {
            break;
        }
        uint8_t tl[2];
        packet->CopyData(tl, 2);
        const uint32_t total = 2u + tl[1];
        if (packet->GetSize() < total)
        {
            break;
        }

        Ptr<Packet> option = packet->CreateFragment(0, total);
        packet->RemoveAtStart(total);

        if (type == static_cast<uint8_t>(DaoOptions::RPL_TARGET) && total >= 4)
        {
            RplTargetOptionHeader opt;
            option->RemoveHeader(opt);
            targets.push_back({opt.GetTarget(), opt.GetPrefixLength(), 0xFF, 0});
        }
        else if (type == static_cast<uint8_t>(DaoOptions::TRANSIT_INFORMATION) && total >= 6)
        {
            TransitInformationOptionHeader opt;
            option->RemoveHeader(opt);
            for (size_t k = pendingFrom; k < targets.size(); ++k)
            {
                targets[k].pathLifetime = opt.GetPathLifetime();
                targets[k].pathSequence = opt.GetPathSequence();
            }
            pendingFrom = targets.size();
        }
        // それ以外（PadN, Target Descriptor など）は読み飛ばす
    }

    const uint32_t nodeId = GetObject<Node>()->GetId();
    std::cout << Simulator::Now().As(Time::S) << " [RPL DAO Rx] node=" << nodeId << " src=" << src
              << " dst=" << dst << " seq=" << int(dao.GetDaoSequence()) << " K=" << dao.GetK()
              << " targets=" << targets.size() << "\n";

    bool changed = false;
    for (const auto& t : targets)
    {
        if (IsLocalAddress(t.address) || t.address.IsLinkLocal() || t.address.IsMulticast())
        {
            continue;
        }

        Ipv6Prefix prefix(t.prefixLength);
        Ptr<RplRoutingTableEntry> existing;
        const bool found = m_routingTable.LookUpExact(t.address, prefix, existing);

        if (t.pathLifetime == 0) // No-Path：経路の取り消し
        {
            if (found && existing->GetNextHop() == src)
            {
                m_routingTable.Delete(t.address, prefix);
                changed = true;
                std::cout << Simulator::Now().As(Time::S) << " [RPL Route] node=" << nodeId
                          << " del DOWNWARD " << t.address << "/" << int(t.prefixLength)
                          << " (No-Path from " << src << ")\n";
            }
            continue;
        }

        if (!found || existing->GetNextHop() != src || existing->GetInterface() != interface)
        {
            AddNetworkRouteTo(t.address, prefix, src, interface);
            changed = true;
            std::cout << Simulator::Now().As(Time::S) << " [RPL Route] node=" << nodeId
                      << " add DOWNWARD " << t.address << "/" << int(t.prefixLength) << " via "
                      << src << " if=" << interface << "\n";
        }

        if (m_routingTable.LookUpExact(t.address, prefix, existing))
        {
            existing->SetPathSequence(t.pathSequence);
            existing->SetLifeTime(t.pathLifetime == 0xFF ? Time::Max()
                                                         : Seconds(t.pathLifetime));
        }
    }

    if (dao.GetK())
    {
        SendDaoAck(src, interface, dao.GetDaoSequence(), 0);
    }

    // 新しい宛先を覚えたら、自分の親にも伝える（root は終点なので不要）。
    if (changed)
    {
        ScheduleDao();
    }
}

void
Rpl::HandleDaoAck(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface)
{
    NS_LOG_FUNCTION(this << src << dst << interface);

    if (IsLocalAddress(src) || packet->GetSize() < 4)
    {
        return;
    }
    uint8_t first[2];
    packet->CopyData(first, 2);
    if (packet->GetSize() < ((first[1] & 0x80) ? 20u : 4u))
    {
        return;
    }

    DaoAckBaseObjectHeader ack;
    packet->RemoveHeader(ack);
    if (ack.GetRplInstanceId() != m_rplInstanceId)
    {
        return;
    }

    std::cout << Simulator::Now().As(Time::S) << " [RPL DAO-ACK Rx] node=" << GetObject<Node>()->GetId()
              << " src=" << src << " seq=" << int(ack.GetDaoSequence())
              << " status=" << int(ack.GetStatus()) << "\n";
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

    DioBaseObjectHeader dio = BuildDioHeader();
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(dio);

    Icmpv6Header icmp;
    icmp.SetType(RPL_ICMPV6_TYPE);
    icmp.SetCode(static_cast<uint8_t>(RplIcmpv6Code::DIO));
    uint16_t payloadLen = icmp.GetSerializedSize() + dio.GetSerializedSize();
    icmp.CalculatePseudoHeaderChecksum(src, dst, payloadLen, Ipv6Header::IPV6_ICMPV6);
    packet->AddHeader(icmp);

    // \RFC{6550} Sec. 6: RPL Control messages use Hop Limit 255.
    SocketIpv6HopLimitTag hopLimitTag;
    hopLimitTag.SetHopLimit(255);
    packet->AddPacketTag(hopLimitTag);

    std::cout << Simulator::Now().As(Time::S) << " [RPL DIO Tx] node=" << GetObject<Node>()->GetId()
              << " if=" << interface << " src=" << src << " dst=" << dst
              << " Rank=" << dio.GetRank() << " DODAGID=" << dio.GetDodagId() << "\n";

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
    // Imin = 2^DIOIntervalMin ms（\RFC{6550} Sec. 8.3.1）。既定 8 ms。
    // Imax = Imin × 2^DIOIntervalDoublings。既定は約 2.3 時間。
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
    // Enable() は最初の周期を [Imin, Imax] から選ぶ。参加直後は Imin から始めたいので Reset()。
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

    // リンクローカル・ユニキャストは常に on-link（ルーティングテーブルには載せない）。
    // DAO/DAO-ACK や ND（NA 応答）など、隣接ノード宛の送信に使う。
    if (dest.IsLinkLocal())
    {
        int32_t linkIf = oifIndex;
        if (linkIf < 0)
        {
            // 出力デバイス指定がなければ、最初の非ループバック IF を使う。
            for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
            {
                if (!DynamicCast<LoopbackNetDevice>(m_ipv6->GetNetDevice(i)))
                {
                    linkIf = static_cast<int32_t>(i);
                    break;
                }
            }
        }
        if (linkIf < 0)
        {
            return nullptr;
        }
        rtentry = Create<Ipv6Route>();
        if (setSource)
        {
            rtentry->SetSource(m_ipv6->SourceAddressSelection(linkIf, dest));
        }
        rtentry->SetDestination(dest);
        rtentry->SetGateway(Ipv6Address::GetZero());
        rtentry->SetOutputDevice(m_ipv6->GetNetDevice(linkIf));
        return rtentry;
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
