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
#include "ns3/random-variable-stream.h"

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
 * @brief RPL ルーティングテーブルエントリ（転送用の簡易表現）
 *
 * 将来 rpl-rtable.h に分離する想定。
 */
struct RplRouteEntry
{
    Ipv6Address dest;       //!< 宛先プレフィックス
    Ipv6Prefix prefix;      //!< プレフィックス長
    Ipv6Address nextHop;    //!< 次ホップ（親ノード等）
    uint32_t interface;     //!< 出力インタフェース
    uint16_t rank;          //!< 経路の Rank（DODAG 内位置）
};

/**
 * @ingroup rpl
 * @brief RPL ルーティングプロトコル（\RFC{6550}）
 */
class RPL : public Ipv6RoutingProtocol
{
  public:
    /**
     * Mode of Operation (MOP).  RFC 6550 Section 6.7.1.
     */
    enum ModeOfOperation_e
    {
        MODE_NO_DOWNWARD = 0,
        MODE_NON_STORING = 1,
        MODE_STORING = 2,
        MODE_STORING_MULTICAST = 3,
    };

    RPL();
    ~RPL() override;

    static TypeId GetTypeId();

    // --- Ipv6RoutingProtocol 必須インタフェース ---

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

    /**
     * 乱数ストリームを固定する（Trickle タイマーのジッター等に使用）。
     */
    int64_t AssignStreams(int64_t stream);

    /**
     * RPL 制御メッセージを送信しないインタフェース集合を取得する。
     */
    std::set<uint32_t> GetInterfaceExclusions() const;

    /**
     * RPL 制御メッセージを送信しないインタフェース集合を設定する。
     * 今どのIFをRPLが動かさないかを設定する。
     */
    void SetInterfaceExclusions(std::set<uint32_t> exceptions);

    /**
     * Objective Function（MRHOF 等）で加算するインタフェースメトリックを取得する。
     * 除外ifの設定
     */
    uint8_t GetInterfaceMetric(uint32_t interface) const;

    /**
     * Objective Function で加算するインタフェースメトリックを設定する。
     */
    void SetInterfaceMetric(uint32_t interface, uint8_t metric);

    /**
     * デフォルトルート（::/0）を手動追加する（Border Router 等向け）。
     */
    void AddDefaultRouteTo(Ipv6Address nextHop, uint32_t interface);

    /**
     * このノードを DODAG Root として動作させるかどうかを設定する。
     */
    void SetRoot(bool isRoot);

    /**
     * このノードが DODAG Root かどうかを返す。
     */
    bool IsRoot() const;

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    static constexpr uint16_t RPL_INFINITE_RANK = 0xFFFF; //!< RFC 6550

    // --- プロトコル起動・ICMPv6 連携 ---

    /**
     *  Icmpv6L4Protocol に Type 155 の受信コールバックを登録し、
     * DIS/DIO/DAO の受信入口を確立する。
     */
    void RegisterIcmpHandler();

    /**
     *  ICMPv6 Type 155 パケットの共通受信ハンドラ。
     * Code フィールドで DIS/DIO/DAO を振り分け、各 Process* へ委譲する。
     */
    void RecvRplMessage(Ptr<Packet> packet, Ipv6Address src, uint32_t iif);

    // --- RPL 制御メッセージ送受信（RFC 6550） ---

    /**
     *  DIS (DODAG Information Solicitation) をマルチキャスト送信する。
     * 近隣の DIO を促す。rpl-packet.h の RplDis を ICMPv6 に載せて送る。
     */
    void SendDis();

    /**
     *  DIO (DODAG Information Object) を Trickle タイマーに従い送信する。
     * Rank, DODAGID, InstanceID, MOP 等を載せ、子ノードの親選択に使わせる。
     */
    void SendDio();

    /**
     *  DAO (Destination Advertisement Object) を親へ送信する。
     * 下位プレフィックスを上位（Root）へ通知する。MOP に応じた経路登録を行う。
     */
    void SendDao();

    /**
     *  受信 DIS を処理し、応答として DIO を送信する。
     */
    void ProcessDis(Ptr<Packet> packet, Ipv6Address src, uint32_t iif);

    /**
     *  受信 DIO を処理する。
     * Rank 比較・Objective Function による親選択・自身の Rank 更新・
     * ルート追加を行う。Trickle タイマーをリセットする場合もある。
     */
    void ProcessDio(Ptr<Packet> packet, Ipv6Address src, uint32_t iif);

    /**
     *  受信 DAO を処理する。
     * Storing モードでは下位プレフィックスをルーティングテーブルへ登録し、
     * 必要に応じて DAO-ACK を返す。
     */
    void ProcessDao(Ptr<Packet> packet, Ipv6Address src, uint32_t iif);

    // --- DODAG・親管理 ---

    /**
     *  受信 DIO に基づき Preferred Parent を選択する。
     * Rank, Objective Function, インタフェースメトリックを考慮する。
     */
    void SelectParent();

    /**
     *  親の Rank とリンクメトリックから自身の Rank を再計算する。
     */
    void UpdateRank();

    // --- Trickle タイマー（DIO 送信制御） ---

    /**
     *  Trickle タイマー（RFC 6206）を開始／再スケジュールする。
     * Imin, Imax, k 等の属性に基づき DIO 送信間隔を制御する。
     */
    void ScheduleDioTrickle();

    /**
     *  Trickle タイマー満了時のコールバック。SendDio() を呼び出す。
     */
    void OnDioTrickleTimeout();

    // --- 転送テーブル操作 ---

    /**
     *  宛先アドレスに対する Ipv6Route をルーティングテーブルから検索する。
     * DODAG 経由のデフォルトルートとプレフィックスルートの両方を考慮する。
     */
    Ptr<Ipv6Route> Lookup(Ipv6Address dest, bool setSource, Ptr<NetDevice> oif);

    /**
     *  プレフィックスルートを m_routes に追加する。
     * DAO 受信時や DIO 処理後に呼び出す。
     */
    void AddNetworkRouteTo(Ipv6Address network,
                           Ipv6Prefix networkPrefix,
                           Ipv6Address nextHop,
                           uint32_t interface);

    /**
     *  指定プレフィックスのルートを m_routes から削除する。
     */
    void RemoveNetworkRoute(Ipv6Address network, Ipv6Prefix networkPrefix);

    /**
     *  インタフェース Down 時に、そのインタフェース経由のルートを無効化する。
     */
    void InvalidateRoutesOnInterface(uint32_t interface);

    // --- メンバ変数 ---

    Ptr<Ipv6> m_ipv6;                         //!< 上位 IPv6 スタック
    std::list<RplRouteEntry> m_routes;        //!< 転送用ルート表

    bool m_isRoot;                            //!< DODAG Root フラグ
    uint8_t m_instanceId;                     //!< RPL Instance ID
    uint16_t m_rank;                          //!< 自身の Rank
    Ipv6Address m_dodagId;                    //!< DODAG Identifier
    Ipv6Address m_preferredParent;            //!< 選択済み親アドレス
    uint32_t m_parentInterface;               //!< 親へ向かうインタフェース
    ModeOfOperation_e m_modeOfOperation;      //!< MOP (Storing / Non-Storing 等)

    Time m_dioIntervalMin;                    //!< Trickle Imin
    Time m_dioIntervalMax;                    //!< Trickle Imax
    uint8_t m_dioRedundancyConstant;          //!< Trickle k

    EventId m_dioTrickleEvent;                //!< Trickle タイマーイベント
    Ptr<UniformRandomVariable> m_rng;         //!< 乱数（ジッター用）

    std::set<uint32_t> m_interfaceExclusions; //!< RPL を動かさない IF
    std::map<uint32_t, uint8_t> m_interfaceMetrics; //!< OF 用リンクメトリック

    bool m_initialized;                       //!< 初期化済みフラグ
};

} // namespace ns3

#endif /* RPL_H */
