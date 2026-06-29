/*
 * Copyright (c) 2014 Universita' di Firenze, Italy
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

#include "rpl.h"

#include "ns3/boolean.h"
#include "ns3/enum.h"
#include "ns3/ipv6-route.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RPL");

NS_OBJECT_ENSURE_REGISTERED(RPL);

RPL::RPL()
    : m_ipv6(nullptr),              // IPv6 スタック未接続
      m_isRoot(false),               // デフォルトは Root ではない
      m_instanceId(0),               // RPL Instance ID = 0
      m_rank(RPL_INFINITE_RANK),     // Rank = 無限（未参加）
      m_parentInterface(0),
      m_modeOfOperation(MODE_STORING), // Storing モード
      m_dioRedundancyConstant(1),    // Trickle の k = 1
      m_initialized(false)           // まだプロトコル未起動
{
    m_rng = CreateObject<UniformRandomVariable>();  // 乱数（ジッター用）
}

RPL::~RPL()
{
}

TypeId
RPL::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RPL")
            .SetParent<Ipv6RoutingProtocol>()
            .SetGroupName("Rpl")
            .AddConstructor<RPL>()
            .AddAttribute("IsRoot",
                          "True if this node acts as the DODAG root.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RPL::m_isRoot),
                          MakeBooleanChecker())
            .AddAttribute("InstanceId",
                          "RPL Instance ID.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RPL::m_instanceId),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("ModeOfOperation",
                          "RPL Mode of Operation (MOP).",
                          EnumValue(MODE_STORING),
                          MakeEnumAccessor<ModeOfOperation_e>(&RPL::m_modeOfOperation),
                          MakeEnumChecker(MODE_NO_DOWNWARD,
                                          "NoDownward",
                                          MODE_NON_STORING,
                                          "NonStoring",
                                          MODE_STORING,
                                          "Storing",
                                          MODE_STORING_MULTICAST,
                                          "StoringMulticast"))
            .AddAttribute("DioIntervalMin",
                          "Trickle timer minimum interval (Imin) for DIO transmission.",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&RPL::m_dioIntervalMin),
                          MakeTimeChecker())
            .AddAttribute("DioIntervalMax",
                          "Trickle timer maximum interval (Imax) for DIO transmission.",
                          TimeValue(Seconds(64)),
                          MakeTimeAccessor(&RPL::m_dioIntervalMax),
                          MakeTimeChecker())
            .AddAttribute("DioRedundancyConstant",
                          "Trickle redundancy constant (k).",
                          UintegerValue(1),
                          MakeUintegerAccessor(&RPL::m_dioRedundancyConstant),
                          MakeUintegerChecker<uint8_t>());
    return tid;
}

void
RPL::DoDispose()
{
    NS_LOG_FUNCTION(this);

    //  登録済み ICMPv6 コールバックの解除、Trickle イベントのキャンセルを行う？
    m_dioTrickleEvent.Cancel();
    m_ipv6 = nullptr;
    m_routes.clear();
    Ipv6RoutingProtocol::DoDispose();
}

void
RPL::DoInitialize()
{
    NS_LOG_FUNCTION(this);

  //  RegisterIcmpHandler() で ICMPv6 Type 155 受信を開始？
  // Root ノードは Rank=0 (BASE_RANK) を設定し ScheduleDioTrickle() で DIO 送信を開始する。
  // 非 Root ノードは SendDis() で近隣 DIO を索求するか、受動的に DIO を待つ。
    m_initialized = true;

    if (m_isRoot)
    {
        m_rank = 0;
        // 将来Root のグローバルアドレス等から安定した DODAGID を設定
        m_dodagId = Ipv6Address::GetZero();
        ScheduleDioTrickle();
    }

    Ipv6RoutingProtocol::DoInitialize();
}

void
RPL::SetIpv6(Ptr<Ipv6> ipv6)
{
    NS_LOG_FUNCTION(this << ipv6);

    // 既存の ICMP ハンドラ登録を解除してから新しい IPv6 スタックへ再登録？
    m_ipv6 = ipv6;
}

int64_t
RPL::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);

    //  Trickle ジッター等で使う乱数ストリームを割り当て？
    m_rng->SetStream(stream);
    return 1;
}

Ptr<Ipv6Route>
RPL::RouteOutput(Ptr<Packet> p,
                 const Ipv6Header& header,
                 Ptr<NetDevice> oif,
                 Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << header.GetDestination() << oif);

    // Lookup() で DODAG 経由のルートを検索し Ipv6Route を返す必要がある？
    // ルートが無い場合は ERROR_NOROUTETOHOST を設定
    Ptr<Ipv6Route> rtentry = Lookup(header.GetDestination(), true, oif);
    if (rtentry)
    {
        sockerr = Socket::ERROR_NOTERROR;
    }
    else
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
    }
    return rtentry;
}

bool
RPL::RouteInput(Ptr<const Packet> p,
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

    //  ローカル配送は Ipv6L3Protocol が先に処理。ここでは転送のみ扱う
    if (header.GetDestination().IsMulticast())
    {
        return false;
    }

    if (header.GetDestination().IsLinkLocal() || header.GetSource().IsLinkLocal())
    {
        if (!ecb.IsNull())
        {
            ecb(p, header, Socket::ERROR_NOROUTETOHOST);
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
        ucb(idev, rtentry, p, header);
        return true;
    }

    return false;
}

void
RPL::NotifyInterfaceUp(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    // Root なら接続プレフィックスを DIO で広告する。非 Root は DIS 送信を検討。
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
RPL::NotifyInterfaceDown(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    // 親が当該 IF 上にいる場合は親を再選択し Rank を更新する。
    InvalidateRoutesOnInterface(interface);
}

void
RPL::NotifyAddAddress(uint32_t interface, Ipv6InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);

    // 将来グローバルプレフィックス追加時、Root は DIO にプレフィックスオプション
    // 子ノードは DAO で下位プレフィックスを親へ通知する
    if (!m_ipv6->IsUp(interface))
    {
        return;
    }
    if (m_interfaceExclusions.find(interface) != m_interfaceExclusions.end())
    {
        return;
    }
}

void
RPL::NotifyRemoveAddress(uint32_t interface, Ipv6InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);

    // 将来削除されたプレフィックスに対応するルートを RemoveNetworkRoute() で除去
    // DAO でプレフィックスのライフタイム切れを上位へ通知する？
    if (!m_ipv6->IsUp(interface))
    {
        return;
    }
}

void
RPL::NotifyAddRoute(Ipv6Address dst,
                    Ipv6Prefix mask,
                    Ipv6Address nextHop,
                    uint32_t interface,
                    Ipv6Address prefixToUse)
{
    NS_LOG_FUNCTION(this << dst << mask << nextHop << interface);

    // 外部から注入された静的ルートを尊重する場合はここで m_routes に統合する？
}

void
RPL::NotifyRemoveRoute(Ipv6Address dst,
                       Ipv6Prefix mask,
                       Ipv6Address nextHop,
                       uint32_t interface,
                       Ipv6Address prefixToUse)
{
    NS_LOG_FUNCTION(this << dst << mask << nextHop << interface);

    // 外部から削除されたルートを m_routes からも除去
    RemoveNetworkRoute(dst, mask);
}

void
RPL::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    NS_LOG_FUNCTION(this);

    //  DODAG 状態（Rank, Parent, DODAGID）と m_routes の内容を整形出力する必要あり
    std::ostream* os = stream->GetStream();
    *os << "RPL routing table (Node " << GetObject<Node>()->GetId() << ")\n";
    *os << "  IsRoot: " << m_isRoot << ", Rank: " << m_rank
        << ", Parent: " << m_preferredParent << "\n";
    for (const auto& route : m_routes)
    {
        *os << "  " << route.dest << "/" << int(route.prefix.GetPrefixLength()) << " via "
            << route.nextHop << " if " << route.interface << " rank " << route.rank << "\n";
    }
}

std::set<uint32_t>
RPL::GetInterfaceExclusions() const
{
    return m_interfaceExclusions;
}

void
RPL::SetInterfaceExclusions(std::set<uint32_t> exceptions)
{
    NS_LOG_FUNCTION(this);
    m_interfaceExclusions = exceptions;
}

uint8_t
RPL::GetInterfaceMetric(uint32_t interface) const
{
    NS_LOG_FUNCTION(this << interface);

    // Objective Function（MRHOF 等）で ETX 等の実測値に差し替え
    auto iter = m_interfaceMetrics.find(interface);
    if (iter != m_interfaceMetrics.end())
    {
        return iter->second;
    }
    return 1;
}

void
RPL::SetInterfaceMetric(uint32_t interface, uint8_t metric)
{
    NS_LOG_FUNCTION(this << interface << int(metric));
    m_interfaceMetrics[interface] = metric;
}

void
RPL::AddDefaultRouteTo(Ipv6Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(this << nextHop << interface);

    AddNetworkRouteTo(Ipv6Address("::"), Ipv6Prefix::GetZero(), nextHop, interface);
}

void
RPL::SetRoot(bool isRoot)
{
    NS_LOG_FUNCTION(this << isRoot);
    m_isRoot = isRoot;
}

bool
RPL::IsRoot() const
{
    return m_isRoot;
}

void
RPL::RegisterIcmpHandler()
{
    NS_LOG_FUNCTION(this);

    // ノードの Icmpv6L4Protocol を取得し、Type=155 の受信時に RecvRplMessage() を呼ぶコールバックを登録？
}

void
RPL::RecvRplMessage(Ptr<Packet> packet, Ipv6Address src, uint32_t iif)
{
    NS_LOG_FUNCTION(this << src << iif << packet->GetSize());


}

void
RPL::SendDis()
{
    NS_LOG_FUNCTION(this);

}

void
RPL::SendDio()
{
    NS_LOG_FUNCTION(this);

}

void
RPL::SendDao()
{
    NS_LOG_FUNCTION(this);

}

void
RPL::ProcessDis(Ptr<Packet> packet, Ipv6Address src, uint32_t iif)
{
    NS_LOG_FUNCTION(this << src << iif);


    if (m_interfaceExclusions.find(iif) != m_interfaceExclusions.end())
    {
        return;
    }
    SendDio();
}

void
RPL::ProcessDio(Ptr<Packet> packet, Ipv6Address src, uint32_t iif)
{
    NS_LOG_FUNCTION(this << src << iif);

    //  DIO をデシリアライズし、Rank が自身より良ければ SelectParent() を呼ぶ
    // UpdateRank() 後、AddNetworkRouteTo() でデフォルトルートを親経由に設定
    // Trickle タイマーを抑制（一致 DIO 受信）またはリセット
    //上はすべて未実装
    if (m_interfaceExclusions.find(iif) != m_interfaceExclusions.end())
    {
        return;
    }
}

void
RPL::ProcessDao(Ptr<Packet> packet, Ipv6Address src, uint32_t iif)
{
    NS_LOG_FUNCTION(this << src << iif);

    // 今後DAO 内のプレフィックスを m_routes に登録する（Storing モード）。
    // Root へ到達した DAO は上位へ転送する場合もある。DAO-ACK を返す。
}

void
RPL::SelectParent()
{
    NS_LOG_FUNCTION(this);

    //  近隣テーブル（未実装）から Rank 最小の候補を選び m_preferredParent を更新
}

void
RPL::UpdateRank()
{
    NS_LOG_FUNCTION(this);


}

void
RPL::ScheduleDioTrickle()
{
    NS_LOG_FUNCTION(this);

    if (m_dioTrickleEvent.IsPending())
    {
        m_dioTrickleEvent.Cancel();
    }
    Time delay = m_dioIntervalMin;
    m_dioTrickleEvent = Simulator::Schedule(delay, &RPL::OnDioTrickleTimeout, this);
}

void
RPL::OnDioTrickleTimeout()
{
    NS_LOG_FUNCTION(this);

    // Trickle の k 回一致前なら SendDio() を実行し、次の間隔を倍にして再スケジュール
    SendDio();
    ScheduleDioTrickle();
}

Ptr<Ipv6Route>
RPL::Lookup(Ipv6Address dest, bool setSource, Ptr<NetDevice> oif)
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
RPL::AddNetworkRouteTo(Ipv6Address network,
                       Ipv6Prefix networkPrefix,
                       Ipv6Address nextHop,
                       uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkPrefix << nextHop << interface);

    // 既存エントリの更新・Rank 比較・ライフタイム管理を行う予定
    RplRouteEntry entry;
    entry.dest = network;
    entry.prefix = networkPrefix;
    entry.nextHop = nextHop;
    entry.interface = interface;
    entry.rank = m_rank;
    m_routes.push_back(entry);
}

void
RPL::RemoveNetworkRoute(Ipv6Address network, Ipv6Prefix networkPrefix)
{
    NS_LOG_FUNCTION(this << network << networkPrefix);

    //  プレフィックス一致エントリを m_routes から削除するながれ？
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
RPL::InvalidateRoutesOnInterface(uint32_t interface)
{
    NS_LOG_FUNCTION(this << interface);

    // 当該 IF を出力とするルートを削除し、親喪失時は SendDis() で再収束する。
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
