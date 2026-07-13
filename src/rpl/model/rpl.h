/*
 * Copyright (c) 2014 Universita' di Firenze, Italy
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

#ifndef RPL_H
#define RPL_H

#include "ns3/ipv6-routing-protocol.h"
#include "ns3/ipv6.h"
#include "ns3/traced-callback.h"

#include <list>
#include <set>

namespace ns3
{

/**
 * @ingroup ipv6Routing
 * @defgroup rpl RPL
 *
 * RPL (IPv6 Routing Protocol for Low-Power and Lossy Networks) defined in
 * \RFC{6550}.  Control messages are carried in ICMPv6 (type 155).
 */

/**
 * @ingroup rpl
 * @brief RPL ルーティングテーブルエントリ（転送用の簡易表現）
 */
struct RplRouteEntry
{
    Ipv6Address dest;    //!< 宛先プレフィックス
    Ipv6Prefix prefix;   //!< プレフィックス長
    Ipv6Address nextHop; //!< 次ホップ
    uint32_t interface;  //!< 出力インタフェース
};

/**
 * @ingroup rpl
 * @brief RPL ルーティングプロトコル（\RFC{6550}）
 */
class Rpl : public Ipv6RoutingProtocol
{
  public:
    Rpl();
    ~Rpl() override;

    static TypeId GetTypeId();

    Ptr<Ipv6Route> RouteOutput(Ptr<Packet> p,
                               const Ipv6Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr) override;
    bool RouteInput(Ptr<const Packet> p,
                    const Ipv6Header& header,
                    Ptr<const NetDevice> idev,
                    const UnicastForwardCallback& ucb,
                    const MulticastForwardCallback& mcb,
                    const LocalDeliverCallback& lcb,
                    const ErrorCallback& ecb) override;
    void NotifyInterfaceUp(uint32_t interface) override;
    void NotifyInterfaceDown(uint32_t interface) override;
    void NotifyAddAddress(uint32_t interface, Ipv6InterfaceAddress address) override;
    void NotifyRemoveAddress(uint32_t interface, Ipv6InterfaceAddress address) override;
    void NotifyAddRoute(Ipv6Address dst,
                        Ipv6Prefix mask,
                        Ipv6Address nextHop,
                        uint32_t interface,
                        Ipv6Address prefixToUse = Ipv6Address::GetZero()) override;
    void NotifyRemoveRoute(Ipv6Address dst,
                           Ipv6Prefix mask,
                           Ipv6Address nextHop,
                           uint32_t interface,
                           Ipv6Address prefixToUse = Ipv6Address::GetZero()) override;
    void SetIpv6(Ptr<Ipv6> ipv6) override;
    void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,
                           Time::Unit unit = Time::S) const override;

    void SetInterfaceExclusions(std::set<uint32_t> exceptions);

    void AddDefaultRouteTo(Ipv6Address nextHop, uint32_t interface);

    void SetRoot(bool isRoot);

    /**
     * TracedCallback signature for RouteOutput / RouteInput probes.
     * @param packet packet being routed (may be null for RouteOutput)
     * @param dst destination address
     * @param success true if a route was found / packet was forwarded
     * @param errno socket errno (RouteOutput) or ERROR_NOROUTETOHOST on input failure
     */
    typedef void (*RouteProbeTracedCallback)(Ptr<const Packet> packet,
                                             Ipv6Address dst,
                                             bool success,
                                             Socket::SocketErrno sockerr);

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    Ptr<Ipv6Route> Lookup(Ipv6Address dest, bool setSource, Ptr<NetDevice> oif);

    void AddNetworkRouteTo(Ipv6Address network,
                           Ipv6Prefix networkPrefix,
                           Ipv6Address nextHop,
                           uint32_t interface);

    void RemoveNetworkRoute(Ipv6Address network, Ipv6Prefix networkPrefix);

    void InvalidateRoutesOnInterface(uint32_t interface);

    Ptr<Ipv6> m_ipv6;
    std::list<RplRouteEntry> m_routes;

    bool m_isRoot;
    std::set<uint32_t> m_interfaceExclusions;
    bool m_initialized;

    TracedCallback<Ptr<const Packet>, Ipv6Address, bool, Socket::SocketErrno> m_routeOutputTrace;
    TracedCallback<Ptr<const Packet>, Ipv6Address, bool, Socket::SocketErrno> m_routeInputTrace;
};

} // namespace ns3

#endif /* RPL_H */
