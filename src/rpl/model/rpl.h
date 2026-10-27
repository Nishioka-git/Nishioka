#ifndef RPL_H
#define RPL_H

#include "rpl-header.h"
#include "rpl-tables.h"

#include "ns3/ipv6-routing-protocol.h"
#include "ns3/ipv6.h"
#include "ns3/nstime.h"
#include "ns3/trickle-timer.h"

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
 * Upward path (minimal): unjoined nodes solicit with DIS (no options).
 * Joined nodes advertise DIO under a Trickle timer (\RFC{6550} Sec. 8.3,
 * \RFC{6206}): the DIO period starts at Imin, doubles up to Imax while the
 * DODAG is stable, and is suppressed when enough consistent DIOs are heard.
 * A multicast DIS is an inconsistency and resets Trickle to Imin.
 * A unicast DIS is answered with one unicast DIO and does not touch Trickle.
 * Preferred-parent selection by lowest Rank, own Rank = parent Rank + 1,
 * default route via parent.
 *
 * Downward path (Storing mode only, MOP 2/3; \RFC{6550} Sec. 6.4, 9.2, 9.3):
 * after joining, a node sends a unicast DAO to its preferred parent
 * (link-local) after DelayDAO.  The DAO carries RPL Target options (own
 * global addresses and every Downward route it already stores) followed by one
 * Transit Information option.  The receiving parent installs a Downward
 * route to each target via the DAO sender's link-local address, sends a
 * DAO-ACK when K is set, and (unless it is the root) aggregates the targets
 * into its own DAO.  Non-Storing mode and No-Path handling for parent
 * switches are not implemented.
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

    /**
     * @return dest に対する VALID な DOWNWARD エントリがあれば true（DAO で学習した下り）
     */
    bool HasDownwardRoute(Ipv6Address dest);

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    void BindToInterface(uint32_t interface);
    void UnbindFromInterface(uint32_t interface);
    void Receive(Ptr<Socket> socket);
    void HandleDio(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface);
    void HandleDis(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface);
    void HandleDao(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface);
    void HandleDaoAck(Ptr<Packet> packet, Ipv6Address src, Ipv6Address dst, uint32_t interface);

    /**
     * @return true if the DODAG runs in Storing mode (MOP 2 or 3)
     */
    bool IsStoringMode() const;

    /**
     * @brief Arm the DelayDAO timer (\RFC{6550} Sec. 9.5). Does nothing if already armed,
     *        so changes made within the delay are aggregated into one DAO.
     */
    void ScheduleDao();

    /**
     * @brief Send a unicast DAO (K=1) with all reachable targets to the preferred parent.
     */
    void SendDao();

    /**
     * @brief Send a DAO-ACK for a received DAO back to the child.
     */
    void SendDaoAck(Ipv6Address dst, uint32_t interface, uint8_t daoSequence, uint8_t status);

    /**
     * @return the RPL raw socket bound to interface, or null
     */
    Ptr<Socket> GetSocketForInterface(uint32_t interface) const;

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

    /**
     * @brief Trickle のパラメータと発火関数（SendDio）を設定する。
     */
    void ConfigureTrickle();

    /**
     * @brief 参加済みなら Trickle を Imin から開始する（開始済みなら何もしない）。
     */
    void StartTrickle();

    /**
     * @brief Trickle を停止する。
     */
    void StopTrickle();

    /**
     * @brief Trickle が発火したときに全 IF へマルチキャスト DIO を送る。
     */
    void SendDio();
    void SendDioOnInterface(uint32_t interface, Ptr<Socket> socket, Ipv6Address dst);
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

    uint8_t m_dioIntervalMin;       //!< DIOIntervalMin。Imin = 2^この値 ミリ秒
    uint8_t m_dioIntervalDoublings; //!< DIOIntervalDoublings。Imax = Imin × 2^この値
    uint16_t m_dioRedundancy;       //!< DIORedundancyConstant k。0 は抑制なし
    TrickleTimer m_trickle;         //!< 参加ノードの DIO 送信タイマー（\RFC{6206}）
    bool m_trickleStarted;          //!< Trickle 動作中か
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

    // --- DAO（Storing mode）---
    Time m_daoDelay;         //!< DelayDAO（\RFC{6550} Sec. 9.5, DEFAULT_DAO_DELAY）
    EventId m_daoTimerEvent; //!< DelayDAO タイマー
    uint8_t m_daoSequence;   //!< 次に送る DAOSequence
    uint8_t m_pathSequence;  //!< Transit Information の Path Sequence
};

} // namespace ns3

#endif /* RPL_H */
