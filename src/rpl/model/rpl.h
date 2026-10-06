#ifndef RPL_H
#define RPL_H

#include "rpl-header.h"
#include "rpl-tables.h"

#include "ns3/ipv6-routing-protocol.h"
#include "ns3/ipv6.h"
#include "ns3/nstime.h"

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
 */
static const Ipv6Address RPL_ALL_NODES_MULTICAST("ff02::1a");

/**
 * @ingroup rpl
 * @brief RPL ルーティングプロトコル（\RFC{6550}）
 *
 * Upward path (minimal): unjoined nodes solicit with DIS (no options);
 * joined nodes answer with DIO (\RFC{6550} Sec. 8.3 simplified, no Trickle).
 * Preferred-parent selection by lowest Rank, own Rank = parent Rank + 1,
 * default route via parent, and optional periodic DIO from joined nodes.
 * DAO is not implemented.
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
     */
    void SetRoot(bool isRoot);

    /**
     * @return true if this node is configured as DODAG root
     */
    bool IsRoot() const;

    /**
     * @return true if this node has joined a DODAG（親選択済み、または root）
     */
    bool IsJoined() const;

    /**
     * @return preferred parent の link-local（未設定は ::）
     */
    Ipv6Address GetPreferredParent() const;

    /**
     * @return 自ノードの Rank（DIO で広告する値）
     */
    uint16_t GetRank() const;

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    void BindToInterface(uint32_t interface);
    void UnbindFromInterface(uint32_t interface);
    void Receive(Ptr<Socket> socket);
    void HandleDio(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface);
    void HandleDis(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface);

    /**
     * @brief 受信 DIO から preferred parent を採用／切替し、自 Rank とルートを更新する。
     */
    void AcceptParent(Ipv6Address parent, uint32_t interface, const DioBaseObjectHeader& dio);

    /**
     * @brief 親向けデフォルトルート（::/0）を入れ替える。
     */
    void InstallParentDefaultRoute();

    /**
     * @return true if addr is configured on this node
     */
    bool IsLocalAddress(Ipv6Address addr) const;

    void SendDis();
    void SendDisOnInterface(uint32_t interface, Ptr<Socket> socket);
    void ScheduleNextDis();

    void SendDio();
    void SendDioOnInterface(uint32_t interface, Ptr<Socket> socket, Ipv6Address dst);
    void ScheduleNextDio();
    DioBaseObjectHeader BuildDioHeader() const;
    Ipv6Address SelectDodagId(uint32_t interface) const;
    Ipv6Address GetLinkLocalAddress(uint32_t interface) const;

    Ptr<Ipv6Route> Lookup(Ipv6Address dest, bool setSource, Ptr<NetDevice> oif);

    void AddNetworkRouteTo(Ipv6Address network,
                           Ipv6Prefix networkPrefix,
                           Ipv6Address nextHop,
                           uint32_t interface);

    void RemoveNetworkRoute(Ipv6Address network, Ipv6Prefix networkPrefix);

    void InvalidateRoutesOnInterface(uint32_t interface);

    Ptr<Ipv6> m_ipv6;
    RplRoutingTable m_routingTable; //!< Forwarding table (\RFC{6550} Sec. 9 / 10)

    bool m_isRoot;
    std::set<uint32_t> m_interfaceExclusions;
    bool m_initialized;

    std::map<Ptr<Socket>, uint32_t> m_sockets;
    Ptr<Socket> m_multicastRecvSocket;

    Time m_dioInterval;
    EventId m_dioTimerEvent;
    Time m_disInterval;       //!< Interval between DIS probes while unjoined
    uint32_t m_disMaxAttempts; //!< Max DIS transmissions before giving up
    uint32_t m_disAttempts;    //!< DIS transmissions so far
    EventId m_disTimerEvent;
    uint8_t m_rplInstanceId;
    uint8_t m_versionNumber;
    uint8_t m_dtsn;
    uint16_t m_rank;
    ModeOfOperation m_mop;
    uint8_t m_dodagPreference;
    Ipv6Address m_dodagId;

    // --- マルチホップ親選択用の最小状態 ---
    bool m_joined;                 //!< DODAG 参加済みか
    Ipv6Address m_preferredParent; //!< 親の link-local（未設定は ::）
    uint32_t m_parentInterface;    //!< 親方向の出力 IF
    uint16_t m_parentRank;         //!< 親が DIO で広告した Rank（切替比較・自 Rank 計算用）
};

} // namespace ns3

#endif /* RPL_H */
