#ifndef RPL_H
#define RPL_H

#include "rpl-header.h"

#include "ns3/ipv6-routing-protocol.h"
#include "ns3/ipv6.h"
#include "ns3/nstime.h"

#include <list>
#include <map>
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
 * @brief All-RPL-nodes link-local multicast (\RFC{6550} Section 6)
 *
 * "A RPL node MUST join the All-RPL-Nodes multicast address."
 */
static const Ipv6Address RPL_ALL_NODES_MULTICAST("ff02::1a");

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
 *
 * Phase 1: ICMPv6 Type 155 ソケットによる DIO（Base Object）の送受信と、
 * DODAG ルートによる周期 DIO 送信。DIS / DAO は未実装。
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

    /**
     * @brief Mark this node as DODAG root (or clear the flag).
     *
     * When set to true after initialization, starts periodic DIO
     * transmission.  When cleared, stops the DIO timer.
     */
    void SetRoot(bool isRoot);

    /**
     * @return true if this node is configured as DODAG root
     */
    bool IsRoot() const;

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    /**
     * @brief Create ICMPv6 raw sockets for an active RPL interface.
     * @param interface IPv6 interface index
     */
    void BindToInterface(uint32_t interface);

    /**
     * @brief Remove sockets bound to an interface.
     * @param interface IPv6 interface index
     */
    void UnbindFromInterface(uint32_t interface);

    /**
     * @brief Receive RPL control messages from an ICMPv6 raw socket.
     * @param socket socket that received data
     */
    void Receive(Ptr<Socket> socket);

    /**
     * @brief Handle a received DIO Base Object.
     * @param packet remaining payload after ICMPv6 header removal
     * @param src sender address
     * @param dst destination address
     * @param interface incoming IPv6 interface index
     */
    void HandleDio(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface);

    /**
     * @brief Build and multicast a DIO on all active interfaces (root only).
     */
    void SendDio();

    /**
     * @brief Send one DIO on a specific interface.
     * @param interface IPv6 interface index
     * @param socket socket bound to that interface's link-local address
     */
    void SendDioOnInterface(uint32_t interface, Ptr<Socket> socket);

    /**
     * @brief Schedule the next periodic DIO (root only).
     */
    void ScheduleNextDio();

    /**
     * @brief Fill a DIO Base Object from local DODAG state.
     * @return populated DIO header
     */
    DioBaseObjectHeader BuildDioHeader() const;

    /**
     * @brief Prefer a global address for DODAGID; fall back to link-local.
     * @param interface IPv6 interface index
     * @return address suitable as DODAGID, or :: if none
     */
    Ipv6Address SelectDodagId(uint32_t interface) const;

    /**
     * @brief Link-local address on an interface, if any.
     * @param interface IPv6 interface index
     * @return link-local address, or :: if none
     */
    Ipv6Address GetLinkLocalAddress(uint32_t interface) const;

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

    /// Per-interface ICMPv6 raw sockets (link-local bind + BindToNetDevice)
    std::map<Ptr<Socket>, uint32_t> m_sockets;
    /// Multicast receive socket joined to all-RPL-nodes (ff02::1a)
    Ptr<Socket> m_multicastRecvSocket;

    Time m_dioInterval;       //!< Periodic DIO interval (root)
    EventId m_dioTimerEvent;  //!< Next scheduled DIO send
    uint8_t m_rplInstanceId;  //!< RPLInstanceID advertised in DIO
    uint8_t m_versionNumber;  //!< DODAG Version Number
    uint8_t m_dtsn;           //!< Destination Advertisement Trigger Sequence Number
    uint16_t m_rank;          //!< Current Rank (root uses MinHopRankIncrease-like value)
    ModeOfOperation m_mop;    //!< Mode of Operation
    uint8_t m_dodagPreference; //!< DODAGPreference (Prf)
    Ipv6Address m_dodagId;    //!< DODAGID (set from root global address)
};

} // namespace ns3

#endif /* RPL_H */
