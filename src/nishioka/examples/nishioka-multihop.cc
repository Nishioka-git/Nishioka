/*
 * Copyright (c) 2024 Tokushima University, Japan.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Nishioka, Yugo
 *
 * Multihop communication example using nishioka header module
 * with 4 devices (1 coordinator + 3 end devices)
 * using UartLrWpanNetDevice for real hardware
 */
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/lr-wpan-fields.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include "ns3/mobility-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/nishioka-header.h"

#include <iostream>
#include <map>
#include <cstring>
 
using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;
using namespace ns3::nishioka;

// ルーティングエントリ構造体
struct RoutingEntry {
    uint16_t entryId;       // エントリID（作成順の通し番号）
    Mac16Address dst;       // 到達目標アドレス
    Mac16Address nextHop;   // 該当デバイスに接続されているアドレス（次ホップ）
    uint8_t energy;         // バッテリー残量（0-100%）
    uint8_t lqi;           // 通信品質（Link Quality Indicator）
    uint8_t hops;          // 現在何ホップしているのか
    
    // コンストラクタ
    RoutingEntry() 
        : entryId(0), dst(Mac16Address()), nextHop(Mac16Address()), 
          energy(100), lqi(255), hops(0) {}
    
    RoutingEntry(uint16_t id, Mac16Address d, Mac16Address nh, 
                 uint8_t e, uint8_t l, uint8_t h)
        : entryId(id), dst(d), nextHop(nh), energy(e), lqi(l), hops(h) {}
};

static uint32_t g_txCount = 0;
static uint32_t g_rxCount = 0;
static uint16_t g_nextShortAddr = 0x0002; // 0x0002から動的割当開始
static uint16_t g_nextShortAddrDev01 = 0x0003; // dev01が子に割り当てるアドレス開始値
static uint16_t g_nextShortAddrDev02 = 0x0004; // dev02が子に割り当てるアドレス開始値

static Ptr<UartLrWpanNetDevice> g_coordinatorDevice; // コーディネータ
static Ptr<UartLrWpanNetDevice> g_uartNetDevice1; // dev01
static Ptr<UartLrWpanNetDevice> g_uartNetDevice2; // dev02
static Ptr<UartLrWpanNetDevice> g_uartNetDevice3; // dev03

// 各デバイスのショートアドレス
static Mac16Address g_coordinatorAddr = Mac16Address("00:01");
static Mac16Address g_dev01Addr = Mac16Address("FF:FE"); // 初期値、アソシエーション後に更新
static Mac16Address g_dev02Addr = Mac16Address("FF:FD"); // 初期値、アソシエーション後に更新
static Mac16Address g_dev03Addr = Mac16Address("FF:FC"); // 初期値、アソシエーション後に更新

// PAN内の総デバイス数管理
static uint16_t g_totalDevicesInPAN = 4; // コーディネータ + dev01 + dev02 + dev03
static uint16_t g_associatedDeviceCount = 0; // アソシエーション済みデバイス数

// 各デバイスの個別ルーティングテーブル（dstアドレスをキーとする）
static std::map<Mac16Address, RoutingEntry> g_routingTableCoordinator;
static std::map<Mac16Address, RoutingEntry> g_routingTableDev01;
static std::map<Mac16Address, RoutingEntry> g_routingTableDev02;
static std::map<Mac16Address, RoutingEntry> g_routingTableDev03;
static uint16_t g_routingSeqNum = 0; // src用のシーケンス番号

// ルーティングエントリを追加・更新する関数（デバイス別）
static void
UpdateRoutingEntry(std::map<Mac16Address, RoutingEntry>& routingTable,
                   Mac16Address dst, Mac16Address nextHop, 
                   uint8_t energy, uint8_t lqi, uint8_t hops)
{
    RoutingEntry entry;
    entry.entryId = g_routingSeqNum++;
    entry.dst = dst;
    entry.nextHop = nextHop;
    entry.energy = energy;
    entry.lqi = lqi;
    entry.hops = hops;
    
    routingTable[dst] = entry;
    
    std::cout << "[ROUTING TABLE UPDATE] "
              << "EntryID: " << entry.entryId
              << " | Dst: " << dst
              << " | NextHop: " << nextHop
              << " | Energy: " << (int)energy
              << " | LQI: " << (int)lqi
              << " | Hops: " << (int)hops << std::endl;
}

// ルーティングテーブルを表示する関数（デバイス別）
static void
PrintRoutingTable(const std::map<Mac16Address, RoutingEntry>& routingTable)
{
    std::cout << "\n========== ROUTING TABLE ==========\n";
    for (const auto& pair : routingTable) {
        const RoutingEntry& entry = pair.second;
        std::cout << "EntryID: " << entry.entryId
                  << " | Dst: " << entry.dst
                  << " | NextHop: " << entry.nextHop
                  << " | Energy: " << (int)entry.energy << "%"
                  << " | LQI: " << (int)entry.lqi
                  << " | Hops: " << (int)entry.hops << std::endl;
    }
    std::cout << "===================================\n\n";
}

// デバイスに対応するルーティングテーブルを取得
static std::map<Mac16Address, RoutingEntry>& GetRoutingTable(Ptr<UartLrWpanNetDevice> device)
{
    if (device == g_coordinatorDevice) {
        return g_routingTableCoordinator;
    } else if (device == g_uartNetDevice1) {
        return g_routingTableDev01;
    } else if (device == g_uartNetDevice2) {
        return g_routingTableDev02;
    } else if (device == g_uartNetDevice3) {
        return g_routingTableDev03;
    }
    return g_routingTableCoordinator;
}

// デバイスに対応するショートアドレスを取得
static Mac16Address GetDeviceAddress(Ptr<UartLrWpanNetDevice> device)
{
    if (device == g_coordinatorDevice) {
        return g_coordinatorAddr;
    } else if (device == g_uartNetDevice1) {
        return g_dev01Addr;
    } else if (device == g_uartNetDevice2) {
        return g_dev02Addr;
    } else if (device == g_uartNetDevice3) {
        return g_dev03Addr;
    }
    return g_coordinatorAddr;
}

// 経路評価関数：LQI、バッテリー残量、ホップ数から経路スコアを計算
static double CalculateRouteScore(uint8_t lqi, uint8_t energy, uint8_t hops)
{
    const double LQI_WEIGHT = 0.5;
    const double ENERGY_WEIGHT = 0.3;
    const double HOPS_WEIGHT = 0.2;
    
    double lqiScore = lqi;
    double energyScore = energy * 2.55;
    
    double maxHops = g_totalDevicesInPAN > 0 ? g_totalDevicesInPAN : 1;
    double normalizedHops = hops < maxHops ? (maxHops - hops) / maxHops : 0.0;
    double hopsScore = normalizedHops * 255.0;
    
    double totalScore = (lqiScore * LQI_WEIGHT) + 
                        (energyScore * ENERGY_WEIGHT) + 
                        (hopsScore * HOPS_WEIGHT);
    
    return totalScore;
}

// 経路を更新すべきか判定
static bool ShouldUpdateRoute(const RoutingEntry& currentRoute, const RoutingEntry& newRoute)
{
    double currentScore = CalculateRouteScore(currentRoute.lqi, currentRoute.energy, currentRoute.hops);
    double newScore = CalculateRouteScore(newRoute.lqi, newRoute.energy, newRoute.hops);
    
    return newScore > (currentScore * 1.1);
}

// 動的経路学習
static void LearnRoute(Ptr<UartLrWpanNetDevice> device, 
                       Mac16Address sourceDst, 
                       Mac16Address nextHop,
                       uint8_t energy, 
                       uint8_t lqi, 
                       uint8_t hops)
{
    std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
    
    auto it = myRoutingTable.find(sourceDst);
    
    if (it == myRoutingTable.end()) {
        std::cout << "  [ROUTE LEARNING] New route discovered to " << sourceDst << std::endl;
        UpdateRoutingEntry(myRoutingTable, sourceDst, nextHop, energy, lqi, hops);
    } else {
        RoutingEntry currentRoute = it->second;
        RoutingEntry newRoute;
        newRoute.dst = sourceDst;
        newRoute.nextHop = nextHop;
        newRoute.energy = energy;
        newRoute.lqi = lqi;
        newRoute.hops = hops;
        
        if (ShouldUpdateRoute(currentRoute, newRoute)) {
            std::cout << "  [ROUTE LEARNING] Better route found! Updating route to " 
                      << sourceDst << std::endl;
            UpdateRoutingEntry(myRoutingTable, sourceDst, nextHop, energy, lqi, hops);
        } else {
            std::cout << "  [ROUTE LEARNING] Existing route is better. No update." << std::endl;
        }
    }
}

static void
DataConfirm(Ptr<UartLrWpanNetDevice> device, McpsDataConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [SEND CONFIRM] Node " << device->GetNode()->GetId()
              << ", Data confirm | Status :" << static_cast<uint32_t>(params.m_status)
              << " | Msdu handle " << static_cast<uint32_t>(params.m_msduHandle)
              << " | Total sent packets: " << g_txCount << "\n";
}

// NishiokaHeaderを使用してパケットを作成
static Ptr<Packet>
CreatePacketWithNishiokaHeader(const RoutingEntry& entry, const std::string& data, 
                                Mac16Address srcAddr = Mac16Address())
{
    // ペイロードを作成（データ部分）
    Ptr<Packet> payload = Create<Packet>((const uint8_t*)data.c_str(), data.size());
    
    // NishiokaHeaderを作成
    NishiokaHeader header;
    header.SetFrameType(NishiokaHeader::CUSTOM_DATA);
    header.SetSeqNum(entry.entryId);
    
    // 宛先アドレスを設定
    header.SetDstAddrFields(0xCAFE, entry.dst);
    
    // 送信元アドレスを設定（指定されている場合）
    if (srcAddr != Mac16Address()) {
        header.SetSrcAddrFields(0xCAFE, srcAddr);
    }
    
    // ヘッダーをパケットに追加
    payload->AddHeader(header);
    
    return payload;
}

// NishiokaHeaderからルーティング情報を抽出
static bool
ExtractRoutingInfoFromHeader(Ptr<Packet> p, RoutingEntry& entry, std::string& data)
{
    // パケットをコピー（元のパケットを変更しないように）
    Ptr<Packet> packetCopy = p->Copy();
    
    // NishiokaHeaderを作成
    // 注意: RemoveHeaderを呼ぶ前にアドレスモードを設定する必要があります
    // これは、ヘッダーのサイズと構造がアドレスモードに依存するためです
    NishiokaHeader header;
    header.SetDstAddrMode(NishiokaHeader::SHORTADDR);
    header.SetSrcAddrMode(NishiokaHeader::SHORTADDR);
    
    // パケットサイズをチェック
    uint32_t headerSize = header.GetSerializedSize();
    if (packetCopy->GetSize() < headerSize) {
        std::cout << "  [ERROR] Packet too small: " << packetCopy->GetSize() 
                  << " bytes, expected at least " << headerSize << " bytes\n";
        return false;
    }
    
    // ヘッダーを削除してデシリアライズ
    // RemoveHeaderはDeserializeを内部で呼び出し、パケットからヘッダーを削除します
    packetCopy->RemoveHeader(header);
    
    // ヘッダーからルーティング情報を取得
    entry.dst = header.GetShortDstAddr();
    entry.entryId = header.GetSeqNum();
    
    // ペイロードからデータを抽出
    uint32_t payloadSize = packetCopy->GetSize();
    if (payloadSize > 0) {
        std::vector<uint8_t> buffer;
        buffer.resize(payloadSize);
        packetCopy->CopyData(buffer.data(), payloadSize);
        data = std::string(buffer.begin(), buffer.end());
    } else {
        data = "";
    }
    
    return true;
}

static void
DataIndication(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p)
{
    g_rxCount++;
    
    RoutingEntry receivedEntry;
    std::string data;
    
    if (ExtractRoutingInfoFromHeader(p, receivedEntry, data)) {
        std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
                  << " <- from Node " << params.m_srcAddr << "\n"
                  << "  Routing Info - EntryID: " << receivedEntry.entryId
                  << " | Dst: " << receivedEntry.dst
                  << " | Data: " << data
                  << " | Total received packets: " << g_rxCount << "\n";
    } else {
        std::vector<uint8_t> buffer;
        buffer.resize(p->GetSize());
        p->CopyData(buffer.data(), p->GetSize());
        data = std::string(buffer.begin(), buffer.end());
        
        std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
                  << " <- from Node " << params.m_srcAddr
                  << " | Data: " << data
                  << " | Total received packets: " << g_rxCount << "\n";
    }
}

// 受信したパケットを処理し、必要に応じて転送
static void
RelayAndIndicate(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p)
{
    g_rxCount++;
    
    RoutingEntry receivedEntry;
    std::string data;
    
    if (!ExtractRoutingInfoFromHeader(p, receivedEntry, data)) {
        std::cout << "[ERROR] Failed to extract routing info from packet\n";
        return;
    }

    Mac16Address myAddress = GetDeviceAddress(device);

    std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
              << " (Addr: " << myAddress << ") <- from Node " << params.m_srcAddr << "\n"
              << "  Routing Info - EntryID: " << receivedEntry.entryId
              << " | Dst: " << receivedEntry.dst
              << " | Data: " << data
              << " | Total received packets: " << g_rxCount << "\n";

    // 経路学習
    if (params.m_srcAddr != myAddress) {
        std::cout << "  [INFO] Learning reverse route to sender " << params.m_srcAddr << std::endl;
        LearnRoute(device, 
                   params.m_srcAddr,
                   params.m_srcAddr,
                   100,
                   params.m_mpduLinkQuality,
                   1);
    }

    // 最終目的地が自分かチェック
    if (receivedEntry.dst == myAddress) {
        std::cout << "  [INFO] I am the final destination. Packet delivered.\n";
        return;
    }

    // 転送処理
    std::cout << "  [INFO] Not for me (dst=" << receivedEntry.dst << "). Looking for route...\n";
    
    std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
    
    auto it = myRoutingTable.find(receivedEntry.dst);
    if (it != myRoutingTable.end()) {
        RoutingEntry& routeEntry = it->second;
        
        // 逆走防止
        if (routeEntry.nextHop == params.m_srcAddr) {
            std::cout << "  [ROUTING ERROR] NextHop(" << routeEntry.nextHop 
                      << ") is same as sender(" << params.m_srcAddr 
                      << "). Preventing reverse routing!\n";
            return;
        }
        
        // ホップ数を更新
        receivedEntry.hops++;
        receivedEntry.lqi = params.m_mpduLinkQuality;
        
        std::cout << "  [ROUTING] Route found - NextHop: " << routeEntry.nextHop
                  << " | Updated Hops: " << (int)receivedEntry.hops
                  << " | Updated LQI: " << (int)receivedEntry.lqi << std::endl;
        
        // 更新したルーティング情報でパケットを再作成
        // 送信元アドレスを現在のデバイスアドレスに設定
        Mac16Address myAddr = GetDeviceAddress(device);
        Ptr<Packet> forwardPacket = CreatePacketWithNishiokaHeader(receivedEntry, data, myAddr);
        
        // 次ホップへ転送
        McpsDataRequestParams relayParams;
        relayParams.m_dstPanId = 0xCAFE;
        relayParams.m_dstAddrMode = SHORT_ADDR;
        relayParams.m_dstAddr = routeEntry.nextHop;
        relayParams.m_msduHandle = 2;
        relayParams.m_txOptions = 0;
        relayParams.m_srcAddrMode = SHORT_ADDR;

        g_txCount++;
        std::cout << Simulator::Now().As(Time::S)
                  << " [RELAY/SEND] Node " << device->GetNode()->GetId()
                  << " -> Node " << relayParams.m_dstAddr
                  << " | Data: " << data
                  << " | Total sent packets: " << g_txCount << std::endl;
        device->GetMac()->McpsDataRequest(relayParams, forwardPacket);
    } else {
        std::cout << "  [ROUTING ERROR] No route found for destination: " 
                  << receivedEntry.dst << std::endl;
    }
}
 
static void
AssociateIndication(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC IND] Node " << device->GetNode()->GetId()
              << " received association request from device with capability "
              << std::hex << static_cast<uint32_t>(params.capabilityInfo) << std::dec
              << " | Dev Addr: " << params.m_extDevAddr << std::endl;
    std::cout << "Sending Association Response..." << std::endl;

    Mac16Address assignedAddr = Mac16Address::ConvertFrom(Mac16Address(g_nextShortAddr));
    g_nextShortAddr++;

    MlmeAssociateResponseParams respParams;
    respParams.m_assocShortAddr = assignedAddr;
    respParams.m_extDevAddr = params.m_extDevAddr;
    respParams.m_status = MacStatus::SUCCESS;
    device->GetMac()->MlmeAssociateResponse(respParams);

    g_associatedDeviceCount++;
    std::cout << "Assigned short address: " << assignedAddr 
              << " | Total associated devices: " << g_associatedDeviceCount 
              << "/" << g_totalDevicesInPAN << std::endl;
}

static void
AssociateConfirm(Ptr<UartLrWpanNetDevice> device, MlmeAssociateConfirmParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC CONFIRM] Node " << device->GetNode()->GetId()
              << ", Associate Confirm: Status " << static_cast<uint32_t>(params.m_status)
              << " | Address: " << params.m_assocShortAddr << std::endl;

    // dev01のアドレスを保存
    if (device == g_uartNetDevice1 && params.m_status == MacStatus::SUCCESS) {
        g_dev01Addr = params.m_assocShortAddr;
        
        // dev01のルーティングテーブルを初期化
        std::cout << "\n========== Initializing Routing Table (dev01) ==========\n";
        // dev02への直接経路
        UpdateRoutingEntry(
            g_routingTableDev01,
            Mac16Address("00:03"),
            Mac16Address("00:03"),
            100, 255, 0
        );
        // dev03への経路（dev02経由）
        UpdateRoutingEntry(
            g_routingTableDev01,
            Mac16Address("00:04"),
            Mac16Address("00:03"),
            100, 255, 0
        );
        // コーディネータへの直接経路
        UpdateRoutingEntry(
            g_routingTableDev01,
            Mac16Address("00:01"),
            Mac16Address("00:01"),
            100, 255, 0
        );
        PrintRoutingTable(g_routingTableDev01);
        
        // dev02のアソシエーション要求をスケジューリング
        Simulator::Schedule(Seconds(0.5), [=]() {
            MlmeAssociateRequestParams associateParams;
            associateParams.m_chNum = 0xD;
            associateParams.m_chPage = 0;
            associateParams.m_coordAddrMode = SHORT_ADDR;
            associateParams.m_coordPanId = 0xCAFE;
            associateParams.m_capabilityInfo = 0x80;
            associateParams.m_coordShortAddr = g_dev01Addr;
            g_uartNetDevice2->GetMac()->MlmeAssociateRequest(associateParams);
        });
    }
    
    // dev02のアソシエーション成功後
    if (params.m_status == MacStatus::SUCCESS && device == g_uartNetDevice2) {
        g_dev02Addr = params.m_assocShortAddr;
        
        // dev02のルーティングテーブルを初期化
        std::cout << "\n========== Initializing Routing Table (dev02) ==========\n";
        // dev03への直接経路
        UpdateRoutingEntry(
            g_routingTableDev02,
            Mac16Address("00:04"),
            Mac16Address("00:04"),
            100, 255, 0
        );
        // dev01への直接経路
        UpdateRoutingEntry(
            g_routingTableDev02,
            Mac16Address("00:02"),
            Mac16Address("00:02"),
            100, 255, 0
        );
        PrintRoutingTable(g_routingTableDev02);
        
        // dev03のアソシエーション要求をスケジューリング
        Simulator::Schedule(Seconds(0.5), [=]() {
            MlmeAssociateRequestParams associateParams;
            associateParams.m_chNum = 0xD;
            associateParams.m_chPage = 0;
            associateParams.m_coordAddrMode = SHORT_ADDR;
            associateParams.m_coordPanId = 0xCAFE;
            associateParams.m_capabilityInfo = 0x80;
            associateParams.m_coordShortAddr = g_dev02Addr;
            g_uartNetDevice3->GetMac()->MlmeAssociateRequest(associateParams);
        });
    }
    
    // dev03のアソシエーション成功後、コーディネータからdev03宛のメッセージを送信
    if (params.m_status == MacStatus::SUCCESS && device == g_uartNetDevice3) {
        g_dev03Addr = params.m_assocShortAddr;
        
        // コーディネータからdev03宛のメッセージを送信（1回のみ）
        Simulator::Schedule(Seconds(0.2), [=]() {
            auto it = g_routingTableCoordinator.find(Mac16Address("00:04"));
            if (it != g_routingTableCoordinator.end()) {
                RoutingEntry& entry = it->second;
                
                std::string sendMsg = "Hello from Coordinator to dev03";
                
                // コーディネータのアドレスを送信元として設定
                Ptr<Packet> packet = CreatePacketWithNishiokaHeader(entry, sendMsg, g_coordinatorAddr);
                
                McpsDataRequestParams dataParams;
                dataParams.m_dstPanId = 0xCAFE;
                dataParams.m_dstAddrMode = SHORT_ADDR;
                dataParams.m_dstAddr = entry.nextHop;
                dataParams.m_msduHandle = 3;
                dataParams.m_txOptions = 0;
                dataParams.m_srcAddrMode = SHORT_ADDR;

                g_txCount++;
                
                std::cout << Simulator::Now().As(Time::S)
                          << " [SEND] Coordinator (Node 0)"
                          << " -> Node " << dataParams.m_dstAddr << " (NextHop from routing table)\n"
                          << "  Routing Info - EntryID: " << entry.entryId
                          << " | Dst: " << entry.dst
                          << " | Energy: " << (int)entry.energy << "%"
                          << " | LQI: " << (int)entry.lqi
                          << " | Hops: " << (int)entry.hops << "\n"
                          << "  Data: " << sendMsg
                          << " | Total sent packets: " << g_txCount << std::endl;
                
                g_coordinatorDevice->GetMac()->McpsDataRequest(dataParams, packet);
            }
        });
    }
}

// dev01がdev02の親としてアソシエーション要求を受ける
static void
AssociateIndicationDev01(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC IND] Node " << device->GetNode()->GetId()
              << " received association request from device with capability "
              << std::hex << static_cast<uint32_t>(params.capabilityInfo) << std::dec
              << " | Dev Addr: " << params.m_extDevAddr << std::endl;
    std::cout << "Sending Association Response (dev01)..." << std::endl;

    Mac16Address assignedAddr = Mac16Address::ConvertFrom(Mac16Address(g_nextShortAddrDev01));
    g_nextShortAddrDev01++;

    MlmeAssociateResponseParams respParams;
    respParams.m_assocShortAddr = assignedAddr;
    respParams.m_extDevAddr = params.m_extDevAddr;
    respParams.m_status = MacStatus::SUCCESS;
    device->GetMac()->MlmeAssociateResponse(respParams);

    g_associatedDeviceCount++;
    std::cout << "Assigned short address (dev01): " << assignedAddr 
              << " | Total associated devices: " << g_associatedDeviceCount 
              << "/" << g_totalDevicesInPAN << std::endl;
}

// dev02がdev03の親としてアソシエーション要求を受ける
static void
AssociateIndicationDev02(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params)
{
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC IND] Node " << device->GetNode()->GetId()
              << " received association request from device with capability "
              << std::hex << static_cast<uint32_t>(params.capabilityInfo) << std::dec
              << " | Dev Addr: " << params.m_extDevAddr << std::endl;
    std::cout << "Sending Association Response (dev02)..." << std::endl;

    Mac16Address assignedAddr = Mac16Address::ConvertFrom(Mac16Address(g_nextShortAddrDev02));
    g_nextShortAddrDev02++;

    MlmeAssociateResponseParams respParams;
    respParams.m_assocShortAddr = assignedAddr;
    respParams.m_extDevAddr = params.m_extDevAddr;
    respParams.m_status = MacStatus::SUCCESS;
    device->GetMac()->MlmeAssociateResponse(respParams);

    g_associatedDeviceCount++;
    std::cout << "Assigned short address (dev02): " << assignedAddr 
              << " | Total associated devices: " << g_associatedDeviceCount 
              << "/" << g_totalDevicesInPAN << std::endl;
}

int
main(int argc, char* argv[])
{
    // 実デバイスを使用するためリアルタイムシミュレータを使用
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    std::cout << "\n========== Network Configuration ==========\n";
    std::cout << "Total devices in PAN: " << g_totalDevicesInPAN << std::endl;
    std::cout << "  - Coordinator: 1\n";
    std::cout << "  - End Devices: " << (g_totalDevicesInPAN - 1) << std::endl;
    std::cout << "==========================================\n\n";
 
    // Coordinator
    Ptr<Node> node = CreateObject<Node>();
    g_coordinatorDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
    node->AddDevice(g_coordinatorDevice);
    Ptr<ConstantPositionMobilityModel> mobility0 = CreateObject<ConstantPositionMobilityModel>();
    mobility0->SetPosition(Vector(0, 0, 0));
    node->AggregateObject(mobility0);

    // End Device 1 (dev01)
    Ptr<Node> node2 = CreateObject<Node>();
    g_uartNetDevice1 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB1");
    node2->AddDevice(g_uartNetDevice1);
    Ptr<ConstantPositionMobilityModel> mobility1 = CreateObject<ConstantPositionMobilityModel>();
    mobility1->SetPosition(Vector(0, 90, 0));
    node2->AggregateObject(mobility1);

    // End Device 2 (dev02)
    Ptr<Node> node3 = CreateObject<Node>();
    g_uartNetDevice2 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB2");
    node3->AddDevice(g_uartNetDevice2);
    Ptr<ConstantPositionMobilityModel> mobility2 = CreateObject<ConstantPositionMobilityModel>();
    mobility2->SetPosition(Vector(0, 180, 0));
    node3->AggregateObject(mobility2);

    // End Device 3 (dev03) - 新規追加
    Ptr<Node> node4 = CreateObject<Node>();
    g_uartNetDevice3 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB3");
    node4->AddDevice(g_uartNetDevice3);
    Ptr<ConstantPositionMobilityModel> mobility3 = CreateObject<ConstantPositionMobilityModel>();
    mobility3->SetPosition(Vector(0, 270, 0));
    node4->AggregateObject(mobility3);
 
    // コールバック設定
    g_coordinatorDevice->GetMac()->SetMcpsDataConfirmCallback(
        MakeBoundCallback(&DataConfirm, g_coordinatorDevice));
    g_coordinatorDevice->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&DataIndication, g_coordinatorDevice));
    g_coordinatorDevice->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&AssociateIndication, g_coordinatorDevice));
    
    g_uartNetDevice1->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&RelayAndIndicate, g_uartNetDevice1));
    g_uartNetDevice1->GetMac()->SetMlmeAssociateConfirmCallback(
        MakeBoundCallback(&AssociateConfirm, g_uartNetDevice1));
    g_uartNetDevice1->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&AssociateIndicationDev01, g_uartNetDevice1));
    
    g_uartNetDevice2->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&RelayAndIndicate, g_uartNetDevice2));
    g_uartNetDevice2->GetMac()->SetMlmeAssociateConfirmCallback(
        MakeBoundCallback(&AssociateConfirm, g_uartNetDevice2));
    g_uartNetDevice2->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&AssociateIndicationDev02, g_uartNetDevice2));
    
    g_uartNetDevice3->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&DataIndication, g_uartNetDevice3));
    g_uartNetDevice3->GetMac()->SetMlmeAssociateConfirmCallback(
        MakeBoundCallback(&AssociateConfirm, g_uartNetDevice3));
 
    // チャンネル・アドレス設定
    Ptr<MacPibAttributes> pibAttr0 = Create<MacPibAttributes>();
    pibAttr0->pCurrentChannel = 0xD; // 13ch
    g_coordinatorDevice->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttr0);
    g_uartNetDevice1->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttr0);
    g_uartNetDevice2->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttr0);
    g_uartNetDevice3->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, pibAttr0);

    Ptr<MacPibAttributes> pibAttr1 = Create<MacPibAttributes>();
    pibAttr1->macShortAddress = Mac16Address("00:01");
    g_coordinatorDevice->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttr1);
    
    Ptr<MacPibAttributes> pibAttr2 = Create<MacPibAttributes>();
    pibAttr2->macShortAddress = Mac16Address("FF:FE");
    g_uartNetDevice1->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttr2);
    
    Ptr<MacPibAttributes> pibAttr3 = Create<MacPibAttributes>();
    pibAttr3->macShortAddress = Mac16Address("FF:FD");
    g_uartNetDevice2->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttr3);
    
    Ptr<MacPibAttributes> pibAttr4 = Create<MacPibAttributes>();
    pibAttr4->macShortAddress = Mac16Address("FF:FC");
    g_uartNetDevice3->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, pibAttr4);

    // PAN ID設定
    Ptr<MacPibAttributes> pibAttrPan1 = Create<MacPibAttributes>();
    pibAttrPan1->macPanId = 0xCAFE;
    g_uartNetDevice1->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, pibAttrPan1);
    
    Ptr<MacPibAttributes> pibAttrPan2 = Create<MacPibAttributes>();
    pibAttrPan2->macPanId = 0xCAFE;
    g_uartNetDevice2->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, pibAttrPan2);
    
    Ptr<MacPibAttributes> pibAttrPan3 = Create<MacPibAttributes>();
    pibAttrPan3->macPanId = 0xCAFE;
    g_uartNetDevice3->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macPanId, pibAttrPan3);
 
    // コーディネータとしてネットワーク開始
    MlmeStartRequestParams startParams;
    startParams.m_PanId = 0xCAFE;
    startParams.m_logCh = 0xD;
    startParams.m_logChPage = 0;
    startParams.m_bcnOrd = 15;
    startParams.m_sfrmOrd = 15;
    startParams.m_panCoor = true;
    startParams.m_battLifeExt = false;
    startParams.m_coorRealgn = false;
    g_coordinatorDevice->GetMac()->MlmeStartRequest(startParams);

    // ルーティングテーブルの初期化（コーディネータ視点）
    std::cout << "\n========== Initializing Routing Table (Coordinator) ==========\n";
    
    // dev03（00:04）への経路：dev01（00:02）を経由してdev02（00:03）経由で到達
    UpdateRoutingEntry(
        g_routingTableCoordinator,
        Mac16Address("00:04"),
        Mac16Address("00:02"),
        100, 255, 0
    );

    // dev02への経路：dev01（00:02）経由
    UpdateRoutingEntry(
        g_routingTableCoordinator,
        Mac16Address("00:03"),
        Mac16Address("00:02"),
        100, 255, 0
    );

    // dev01への直接経路
    UpdateRoutingEntry(
        g_routingTableCoordinator,
        Mac16Address("00:02"),
        Mac16Address("00:02"),
        100, 255, 0
    );
    
    PrintRoutingTable(g_routingTableCoordinator);

    // dev01がdev00にアソシエーション要求
    Simulator::Schedule(Seconds(0.5), [=]() {
        MlmeAssociateRequestParams associateParams;
        associateParams.m_chNum = 0xD;
        associateParams.m_chPage = 0;
        associateParams.m_coordAddrMode = SHORT_ADDR;
        associateParams.m_coordPanId = 0xCAFE;
        associateParams.m_capabilityInfo = 0x80;
        associateParams.m_coordShortAddr = Mac16Address("00:01");
        g_uartNetDevice1->GetMac()->MlmeAssociateRequest(associateParams);
    });

    // シミュレーション終了前にルーティングテーブルを表示
    Simulator::Schedule(Seconds(4.9), []() {
        std::cout << "\n========== Final Routing Tables ==========\n";
        std::cout << "Coordinator:\n";
        PrintRoutingTable(g_routingTableCoordinator);
        std::cout << "Dev01:\n";
        PrintRoutingTable(g_routingTableDev01);
        std::cout << "Dev02:\n";
        PrintRoutingTable(g_routingTableDev02);
        std::cout << "Dev03:\n";
        PrintRoutingTable(g_routingTableDev03);
    });

    Simulator::Stop(Seconds(5));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

