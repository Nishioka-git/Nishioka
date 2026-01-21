/*
* Copyright (c) 2024 Tokushima University, Japan.
*
* SPDX-License-Identifier: GPL-2.0-only
*
* Author:
* Nishioka,Yugo
*/

#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/lr-wpan-fields.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include "ns3/mobility-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/nishioka-header.h"
#include "ns3/nishioka-stack.h"
#include "ns3/nishioka-stack-container.h"
#include "ns3/nishioka-helper.h"
#include "ns3/nishioka-nwk.h"

#include <iostream>
#include <map>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>
using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;
using namespace ns3::nishioka;

// パケットタイプ列挙型
enum PacketType : uint8_t {
PACKET_TYPE_DATA = 0x01,   // データパケット
PACKET_TYPE_RREQ = 0x02,   // Route Request
PACKET_TYPE_RREP = 0x03    // Route Reply
};

// ルーティングエントリ構造体
struct RoutingEntry {
uint16_t entryId; // エントリID（作成順の通し番号）
Mac16Address dst; // 到達目標アドレス
Mac16Address nextHop; // 該当デバイスに接続されているアドレス（次ホップ）
uint8_t energy; // バッテリー残量（0-100%）
uint8_t lqi; // 通信品質（Link Quality Indicator）
uint8_t hops; // 現在何ホップしているのか
// コンストラクタ
RoutingEntry()
: entryId(0), dst(Mac16Address()), nextHop(Mac16Address()),
energy(100), lqi(255), hops(0) {}
RoutingEntry(uint16_t id, Mac16Address d, Mac16Address nh,
uint8_t e, uint8_t l, uint8_t h)
: entryId(id), dst(d), nextHop(nh), energy(e), lqi(l), hops(h) {}
};

// RREQパケット構造体
struct RREQPacket {
uint8_t packetType;     // PACKET_TYPE_RREQ
uint16_t rreqId;        // RREQ識別子（送信元とRREQ IDで一意）
Mac16Address originator; // RREQ送信元アドレス
Mac16Address dst;        // 目的地アドレス
uint8_t hopCount;        // ホップカウント
uint8_t energy;          // 送信元のバッテリー残量
uint8_t lqi;            // LQI

RREQPacket()
: packetType(PACKET_TYPE_RREQ), rreqId(0),
originator(Mac16Address()), dst(Mac16Address()),
hopCount(0), energy(100), lqi(255) {}
};

// RREPパケット構造体
struct RREPPacket {
uint8_t packetType;     // PACKET_TYPE_RREP
uint16_t rreqId;        // 対応するRREQ ID
Mac16Address originator; // 元のRREQ送信元アドレス
Mac16Address dst;        // 目的地アドレス（RREP送信者）
uint8_t hopCount;        // ホップカウント
uint8_t energy;          // 送信元のバッテリー残量
uint8_t lqi;            // LQI

RREPPacket()
: packetType(PACKET_TYPE_RREP), rreqId(0),
originator(Mac16Address()), dst(Mac16Address()),
hopCount(0), energy(100), lqi(255) {}
};

static uint32_t g_txCount = 0;
static uint32_t g_rxCount = 0;
static uint16_t g_nextShortAddr = 0x0002; // 0x0002から動的割当開始
static uint16_t g_nextShortAddrDev01 = 0x0003; // dev01が子に割り当てるアドレス開始値
static Mac16Address g_dev01ShortAddr = Mac16Address("00:02"); // dev01のアドレスを保持

static Ptr<UartLrWpanNetDevice> g_coordinatorDevice; // コーディネータ
static Ptr<UartLrWpanNetDevice> g_uartNetDevice1; // dev01
static Ptr<UartLrWpanNetDevice> g_uartNetDevice2; // dev02

// NishiokaStackへのポインタ
static Ptr<NishiokaStack> g_coordinatorStack; // コーディネータのスタック
static Ptr<NishiokaStack> g_dev01Stack; // dev01のスタック
static Ptr<NishiokaStack> g_dev02Stack; // dev02のスタック

// NishiokaStackContainer
static NishiokaStackContainer g_stacks; // すべてのスタックを管理

// 各デバイスのショートアドレス
static Mac16Address g_coordinatorAddr = Mac16Address("00:01");
static Mac16Address g_dev01Addr = Mac16Address("FF:FE"); // 初期値、アソシエーション後に更新
static Mac16Address g_dev02Addr = Mac16Address("FF:FD"); // 初期値、アソシエーション後に更新

// 各デバイスのバッテリー残量（%）
static uint8_t g_batteryLevelCoordinator = 100;
static uint8_t g_batteryLevelDev01 = 100;
static uint8_t g_batteryLevelDev02 = 100;

// RREQ管理
static uint16_t g_rreqIdCounter = 0; // RREQ IDカウンター
// 各デバイスごとの処理済みRREQ（デバイス別に管理）
static std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsCoordinator;
static std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsDev01;
static std::map<std::pair<Mac16Address, uint16_t>, bool> g_processedRREQsDev02;

// PAN内の総デバイス数管理
static uint16_t g_totalDevicesInPAN = 3; // コーディネータ + dev01 + dev02
static uint16_t g_associatedDeviceCount = 0; // アソシエーション済みデバイス数

// 各デバイスの個別ルーティングテーブル（dstアドレスをキーとする）
static std::map<Mac16Address, RoutingEntry> g_routingTableCoordinator; // コーディネータのルーティングテーブル
static std::map<Mac16Address, RoutingEntry> g_routingTableDev01; // dev01のルーティングテーブル
static std::map<Mac16Address, RoutingEntry> g_routingTableDev02; // dev02のルーティングテーブル
static uint16_t g_routingSeqNum = 0; // src用のシーケンス番号

// battery.txtからバッテリー残量を読み取る関数
// 戻り値: バッテリー残量（0-100%）、読み取り失敗時は前回の値を維持
static uint8_t
ReadBatteryLevel(const std::string& filename, uint8_t defaultValue = 100)
{
std::ifstream file(filename);
if (!file.is_open()) {
std::cout << "[BATTERY] Warning: Could not open " << filename
<< ", using default value: " << (int)defaultValue << "%" << std::endl;
return defaultValue;
}
std::string line;
if (std::getline(file, line)) {
// 16進数文字列を読み取り（例: "0x4c" または "4c"）
std::stringstream ss(line);
uint32_t hexValue;
// 0xプレフィックスがあってもなくても対応
if (line.find("0x") == 0 || line.find("0X") == 0) {
ss >> std::hex >> hexValue;
} else {
ss >> std::hex >> hexValue;
}
if (ss.fail()) {
std::cout << "[BATTERY] Warning: Failed to parse battery value from "
<< filename << ", using default: " << (int)defaultValue << "%" << std::endl;
return defaultValue;
}
// 16進数値をそのまま10進数パーセンテージとして使用
// 例: 0x4c = 76 = 76%
uint8_t percentage = static_cast<uint8_t>(hexValue);
// 0-100の範囲に制限
if (percentage > 100) {
std::cout << "[BATTERY] Warning: Battery value " << (int)percentage
<< "% is out of range, capping to 100%" << std::endl;
percentage = 100;
}
std::cout << "[BATTERY] Read from " << filename
<< ": 0x" << std::hex << hexValue << std::dec
<< " = " << (int)percentage << "%" << std::endl;
file.close();
return percentage;
}
file.close();
std::cout << "[BATTERY] Warning: Empty file " << filename
<< ", using default: " << (int)defaultValue << "%" << std::endl;
return defaultValue;
}

// デバイスに対応するバッテリー残量を取得
static uint8_t GetBatteryLevel(Ptr<UartLrWpanNetDevice> device)
{
if (device == g_coordinatorDevice) {
return g_batteryLevelCoordinator;
} else if (device == g_uartNetDevice1) {
return g_batteryLevelDev01;
} else if (device == g_uartNetDevice2) {
return g_batteryLevelDev02;
}
return 100; // デフォルト値
}

// スタックからデバイスを取得するヘルパー関数
static Ptr<UartLrWpanNetDevice> GetDeviceFromStack(Ptr<NishiokaStack> stack)
{
if (stack == g_coordinatorStack) {
return g_coordinatorDevice;
} else if (stack == g_dev01Stack) {
return g_uartNetDevice1;
} else if (stack == g_dev02Stack) {
return g_uartNetDevice2;
}
return nullptr;
}

// スタックに対応するバッテリー残量を取得
static uint8_t GetBatteryLevelFromStack(Ptr<NishiokaStack> stack)
{
Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
if (device) {
return GetBatteryLevel(device);
}
return 100; // デフォルト値
}

// スタックからNWK層を取得するヘルパー関数
static Ptr<NishiokaNwk> GetNwkFromStack(Ptr<NishiokaStack> stack)
{
if (stack) {
return stack->GetNwk();
}
return nullptr;
}

// デバイスからスタックを取得するヘルパー関数
static Ptr<NishiokaStack> GetStackFromDevice(Ptr<UartLrWpanNetDevice> device)
{
if (device == g_coordinatorDevice) {
return g_coordinatorStack;
} else if (device == g_uartNetDevice1) {
return g_dev01Stack;
} else if (device == g_uartNetDevice2) {
return g_dev02Stack;
}
return nullptr;
}

// バッテリーファイルのパスを取得する関数
static std::string
GetBatteryFilePath()
{
std::string batteryFilePath;
char cwd[1024];

// 1. カレントディレクトリからbattery.txtを探す
if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    batteryFilePath = std::string(cwd) + "/battery.txt";
    std::ifstream testFile(batteryFilePath);
    if (testFile.is_open()) {
        testFile.close();
        return batteryFilePath;
    }
    
    // 2. 実行ファイルと同じディレクトリを探す
    char exePath[1024];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        std::string exeDir = std::string(exePath);
        size_t lastSlash = exeDir.find_last_of("/");
        if (lastSlash != std::string::npos) {
            exeDir = exeDir.substr(0, lastSlash);
            batteryFilePath = exeDir + "/battery.txt";
            std::ifstream testFile2(batteryFilePath);
            if (testFile2.is_open()) {
                testFile2.close();
                return batteryFilePath;
            }
        }
    }
    
    // 3. examplesディレクトリからの相対パスを試す（buildディレクトリから）
    batteryFilePath = std::string(cwd) + "/../../src/nishioka/examples/battery.txt";
    std::ifstream testFile3(batteryFilePath);
    if (testFile3.is_open()) {
        testFile3.close();
        return batteryFilePath;
    }
}

// 4. 最後の手段：絶対パス（examplesディレクトリ）
batteryFilePath = "/home/yuugo/ns-3-dev/src/nishioka/examples/battery.txt";
return batteryFilePath;
}

// 定期的にバッテリー残量を更新する関数
static void
UpdateBatteryLevels()
{
std::cout << "\n========== Updating Battery Levels ==========\n";
// カレントディレクトリを表示（デバッグ用）
char cwd[1024];
if (getcwd(cwd, sizeof(cwd)) != nullptr) {
std::cout << "Current working directory: " << cwd << std::endl;
}
// battery.txtのパスを取得
std::string batteryFilePath = GetBatteryFilePath();
std::cout << "Using battery file: " << batteryFilePath << std::endl;
// 各デバイスのバッテリー情報を読み取り
// 各デバイスごとに異なるファイル名を使用する場合は、以下を変更
g_batteryLevelCoordinator = ReadBatteryLevel(batteryFilePath, g_batteryLevelCoordinator);
g_batteryLevelDev01 = ReadBatteryLevel(batteryFilePath, g_batteryLevelDev01);
g_batteryLevelDev02 = ReadBatteryLevel(batteryFilePath, g_batteryLevelDev02);
std::cout << "Coordinator Battery: " << (int)g_batteryLevelCoordinator << "%\n";
std::cout << "Dev01 Battery: " << (int)g_batteryLevelDev01 << "%\n";
std::cout << "Dev02 Battery: " << (int)g_batteryLevelDev02 << "%\n";
std::cout << "=============================================\n\n";
// 次回の更新をスケジュール（50秒後）
Simulator::Schedule(Seconds(50.0), &UpdateBatteryLevels);
}

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
}
// デフォルトはコーディネータのテーブル
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
}
// デフォルトはコーディネータのアドレス
return g_coordinatorAddr;
}

// デバイスに対応する処理済みRREQリストを取得
static std::map<std::pair<Mac16Address, uint16_t>, bool>& GetProcessedRREQs(Ptr<UartLrWpanNetDevice> device)
{
if (device == g_coordinatorDevice) {
return g_processedRREQsCoordinator;
} else if (device == g_uartNetDevice1) {
return g_processedRREQsDev01;
} else if (device == g_uartNetDevice2) {
return g_processedRREQsDev02;
}
// デフォルトはコーディネータ
return g_processedRREQsCoordinator;
}

// 経路評価関数：LQI、バッテリー残量、ホップ数から経路スコアを計算
// すべての指標を0-255スケールに正規化して評価
// スコアが高いほど良い経路
static double CalculateRouteScore(uint8_t lqi, uint8_t energy, uint8_t hops)
{
// 重み付け係数
const double LQI_WEIGHT = 0.5; // LQI（通信品質）の重要度: 50%
const double ENERGY_WEIGHT = 0.3; // バッテリー残量の重要度: 30%
const double HOPS_WEIGHT = 0.2; // ホップ数の重要度: 20%
// LQIスコア（0-255）そのまま使用
double lqiScore = lqi;
// エネルギースコア（0-100を0-255スケールに正規化）
// energy × 2.55 = 0-255スケール
double energyScore = energy * 2.55;
// ホップスコア（PAN内総デバイス数で正規化し、0-255スケールに）
// (総デバイス数 - ホップ数) / 総デバイス数 × 255
// ホップ数が少ないほど高スコア
double maxHops = g_totalDevicesInPAN > 0 ? g_totalDevicesInPAN : 1;
double normalizedHops = hops < maxHops ? (maxHops - hops) / maxHops : 0.0;
double hopsScore = normalizedHops * 255.0;
// 重み付け合計スコア（最大値255）
double totalScore = (lqiScore * LQI_WEIGHT) +
(energyScore * ENERGY_WEIGHT) +
(hopsScore * HOPS_WEIGHT);
std::cout << " [SCORE DETAIL] LQI:" << lqi << "(" << (lqiScore * LQI_WEIGHT) << ") "
<< "Energy:" << (int)energy << "%(" << (energyScore * ENERGY_WEIGHT) << ") "
<< "Hops:" << (int)hops << "(" << (hopsScore * HOPS_WEIGHT) << ") "
<< "Total:" << totalScore << std::endl;
return totalScore;
}

// 経路を更新すべきか判定（新しい経路の方が良いか）
static bool ShouldUpdateRoute(const RoutingEntry& currentRoute, const RoutingEntry& newRoute)
{
double currentScore = CalculateRouteScore(currentRoute.lqi, currentRoute.energy, currentRoute.hops);
double newScore = CalculateRouteScore(newRoute.lqi, newRoute.energy, newRoute.hops);
std::cout << " [ROUTE EVALUATION] Current score: " << currentScore
<< " | New score: " << newScore << std::endl;
// 新しい経路のスコアが10%以上高い場合に更新
return newScore > (currentScore * 1.1);
}

// 動的経路学習：受信したパケット情報から経路を学習・更新
static void LearnRoute(Ptr<UartLrWpanNetDevice> device,
Mac16Address sourceDst,
Mac16Address nextHop,
uint8_t energy,
uint8_t lqi,
uint8_t hops)
{
std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
// 既存の経路があるかチェック
auto it = myRoutingTable.find(sourceDst);
if (it == myRoutingTable.end()) {
// 新しい経路を追加
std::cout << " [ROUTE LEARNING] New route discovered to " << sourceDst << std::endl;
UpdateRoutingEntry(myRoutingTable, sourceDst, nextHop, energy, lqi, hops);
} else {
// 既存経路と比較して更新すべきか判定
RoutingEntry currentRoute = it->second;
RoutingEntry newRoute;
newRoute.dst = sourceDst;
newRoute.nextHop = nextHop;
newRoute.energy = energy;
newRoute.lqi = lqi;
newRoute.hops = hops;
if (ShouldUpdateRoute(currentRoute, newRoute)) {
std::cout << " [ROUTE LEARNING] Better route found! Updating route to "
<< sourceDst << std::endl;
UpdateRoutingEntry(myRoutingTable, sourceDst, nextHop, energy, lqi, hops);
} else {
std::cout << " [ROUTE LEARNING] Existing route is better. No update." << std::endl;
}
}

// NWK層のルーティングテーブルも更新
Ptr<NishiokaStack> stack = GetStackFromDevice(device);
if (stack) {
Ptr<NishiokaNwk> nwk = stack->GetNwk();
if (nwk) {
nwk->SetRoute(sourceDst, nextHop);
std::cout << "  [NWK] Updated route to " << sourceDst << " via " << nextHop << std::endl;
}
}
}

// RREQパケットを作成
static Ptr<Packet>
CreateRREQPacket(const RREQPacket& rreq)
{
std::vector<uint8_t> packet;
packet.resize(13); // 1+2+2+2+1+1+1+3(padding for alignment) = 13 bytes
size_t offset = 0;

packet[offset++] = rreq.packetType;

// rreqId (16bit)
packet[offset++] = (rreq.rreqId >> 8) & 0xFF;
packet[offset++] = rreq.rreqId & 0xFF;

// originator (16bit)
uint8_t originatorBuf[2];
rreq.originator.CopyTo(originatorBuf);
packet[offset++] = originatorBuf[0];
packet[offset++] = originatorBuf[1];

// dst (16bit)
uint8_t dstBuf[2];
rreq.dst.CopyTo(dstBuf);
packet[offset++] = dstBuf[0];
packet[offset++] = dstBuf[1];

packet[offset++] = rreq.hopCount;
packet[offset++] = rreq.energy;
packet[offset++] = rreq.lqi;

return Create<Packet>(packet.data(), packet.size());
}

// RREQパケットを解析
static bool
ParseRREQPacket(Ptr<Packet> p, RREQPacket& rreq)
{
std::vector<uint8_t> buffer;
buffer.resize(p->GetSize());
p->CopyData(buffer.data(), p->GetSize());

if (buffer.size() < 10) return false;

size_t offset = 0;
rreq.packetType = buffer[offset++];

if (rreq.packetType != PACKET_TYPE_RREQ) return false;

rreq.rreqId = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
offset += 2;

uint8_t originatorBuf[2] = {buffer[offset], buffer[offset + 1]};
rreq.originator.CopyFrom(originatorBuf);
offset += 2;

uint8_t dstBuf[2] = {buffer[offset], buffer[offset + 1]};
rreq.dst.CopyFrom(dstBuf);
offset += 2;

rreq.hopCount = buffer[offset++];
rreq.energy = buffer[offset++];
rreq.lqi = buffer[offset++];

return true;
}

// RREPパケットを作成
static Ptr<Packet>
CreateRREPPacket(const RREPPacket& rrep)
{
std::vector<uint8_t> packet;
packet.resize(13);
size_t offset = 0;

packet[offset++] = rrep.packetType;

packet[offset++] = (rrep.rreqId >> 8) & 0xFF;
packet[offset++] = rrep.rreqId & 0xFF;

uint8_t originatorBuf[2];
rrep.originator.CopyTo(originatorBuf);
packet[offset++] = originatorBuf[0];
packet[offset++] = originatorBuf[1];

uint8_t dstBuf[2];
rrep.dst.CopyTo(dstBuf);
packet[offset++] = dstBuf[0];
packet[offset++] = dstBuf[1];

packet[offset++] = rrep.hopCount;
packet[offset++] = rrep.energy;
packet[offset++] = rrep.lqi;

return Create<Packet>(packet.data(), packet.size());
}

// RREPパケットを解析
static bool
ParseRREPPacket(Ptr<Packet> p, RREPPacket& rrep)
{
std::vector<uint8_t> buffer;
buffer.resize(p->GetSize());
p->CopyData(buffer.data(), p->GetSize());

if (buffer.size() < 10) return false;

size_t offset = 0;
rrep.packetType = buffer[offset++];

if (rrep.packetType != PACKET_TYPE_RREP) return false;

rrep.rreqId = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
offset += 2;

uint8_t originatorBuf[2] = {buffer[offset], buffer[offset + 1]};
rrep.originator.CopyFrom(originatorBuf);
offset += 2;

uint8_t dstBuf[2] = {buffer[offset], buffer[offset + 1]};
rrep.dst.CopyFrom(dstBuf);
offset += 2;

rrep.hopCount = buffer[offset++];
rrep.energy = buffer[offset++];
rrep.lqi = buffer[offset++];

return true;
}

static void
DataConfirm(Ptr<NishiokaStack> stack, McpsDataConfirmParams params)
{
Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
if (!device) return;
std::cout << Simulator::Now().As(Time::S) << " [SEND CONFIRM] Node " << device->GetNode()->GetId()
<< ", Data confirm | Status :" << static_cast<uint32_t>(params.m_status)
<< " | Msdu handle " << static_cast<uint32_t>(params.m_msduHandle)
<< " | Total sent packets: " << g_txCount << "\n";
}

// パケットにルーティング情報を埋め込む関数（NishiokaHeaderを使用）
static Ptr<Packet>
CreatePacketWithRoutingInfo(const RoutingEntry& entry, const std::string& data, Mac16Address srcAddr)
{
// ペイロードデータを作成
Ptr<Packet> payload = Create<Packet>((const uint8_t*)data.c_str(), data.size());
uint32_t payloadSize = payload->GetSize();

// NishiokaHeaderを作成して設定
NishiokaHeader hdr;
hdr.SetFrameType(NishiokaHeader::CUSTOM_DATA);
hdr.SetSeqNum(entry.entryId & 0xFF); // entryIdの下位8bitをシーケンス番号として使用
hdr.SetDstAddrFields(0xCAFE, entry.dst); // 目的地アドレス
hdr.SetSrcAddrFields(0xCAFE, srcAddr); // 送信元アドレス
hdr.SetBattery(entry.energy); // バッテリー残量
// EvaluationフィールドにLQIとホップ数を組み合わせて設定
// (LQI << 8) | hops の形式で評価値を設定
uint16_t evaluation = (static_cast<uint16_t>(entry.lqi) << 8) | entry.hops;
hdr.SetEvaluation(evaluation);

// ヘッダを追加
payload->AddHeader(hdr);

// NishiokaHeaderの詳細情報を視覚的に表示
std::cout << "\n========== [NISHIOKA HEADER ADDED] ==========\n";
std::cout << "  Frame Type: " << static_cast<int>(hdr.GetFrameType()) 
          << " (CUSTOM_DATA=" << static_cast<int>(NishiokaHeader::CUSTOM_DATA) << ")\n";
std::cout << "  Sequence Number: " << static_cast<int>(hdr.GetSeqNum()) << "\n";
std::cout << "  Source Address: " << hdr.GetShortSrcAddr() 
          << " (PAN ID: 0x" << std::hex << hdr.GetSrcPanId() << std::dec << ")\n";
std::cout << "  Destination Address: " << hdr.GetShortDstAddr()
          << " (PAN ID: 0x" << std::hex << hdr.GetDstPanId() << std::dec << ")\n";
std::cout << "  Battery Level: " << static_cast<int>(hdr.GetBattery()) << "%\n";
std::cout << "  Evaluation: " << hdr.GetEvaluation() 
          << " (LQI: " << static_cast<int>(entry.lqi) 
          << ", Hops: " << static_cast<int>(entry.hops) << ")\n";
std::cout << "  Header Size: " << hdr.GetSerializedSize() << " bytes\n";
std::cout << "  Payload Size: " << payloadSize << " bytes\n";
std::cout << "  Total Packet Size: " << payload->GetSize() << " bytes\n";
std::cout << "==========================================\n\n";

return payload;
}

// パケットからルーティング情報を抽出する関数（NishiokaHeaderを使用）
static bool
ExtractRoutingInfo(Ptr<Packet> p, RoutingEntry& entry, std::string& data, bool verbose = true)
{
// パケットのコピーを作成（ヘッダを削除するため）
Ptr<Packet> pktCopy = p->Copy();
uint32_t totalSize = pktCopy->GetSize();

// NishiokaHeaderを抽出
NishiokaHeader hdr;
hdr.SetDstAddrMode(NishiokaHeader::SHORTADDR);
hdr.SetSrcAddrMode(NishiokaHeader::SHORTADDR);

// ヘッダを削除
if (pktCopy->GetSize() < hdr.GetSerializedSize()) {
return false; // パケットサイズが不足
}

pktCopy->RemoveHeader(hdr);
uint32_t headerSize = hdr.GetSerializedSize();
uint32_t payloadSize = pktCopy->GetSize();

// ルーティング情報を設定
entry.entryId = hdr.GetSeqNum(); // シーケンス番号をentryIdとして使用
entry.dst = hdr.GetShortDstAddr(); // 目的地アドレス
entry.energy = static_cast<uint8_t>(hdr.GetBattery()); // バッテリー残量
// EvaluationフィールドからLQIとホップ数を抽出
uint16_t evaluation = hdr.GetEvaluation();
entry.lqi = static_cast<uint8_t>((evaluation >> 8) & 0xFF); // 上位8bitがLQI
entry.hops = static_cast<uint8_t>(evaluation & 0xFF); // 下位8bitがホップ数

// ペイロードデータを抽出
std::vector<uint8_t> buffer;
buffer.resize(pktCopy->GetSize());
pktCopy->CopyData(buffer.data(), pktCopy->GetSize());
data = std::string(buffer.begin(), buffer.end());

// NishiokaHeaderの詳細情報を視覚的に表示
if (verbose) {
std::cout << "\n========== [NISHIOKA HEADER EXTRACTED] ==========\n";
std::cout << "  Frame Type: " << static_cast<int>(hdr.GetFrameType()) 
          << " (CUSTOM_DATA=" << static_cast<int>(NishiokaHeader::CUSTOM_DATA) << ")\n";
std::cout << "  Sequence Number: " << static_cast<int>(hdr.GetSeqNum()) << "\n";
std::cout << "  Source Address: " << hdr.GetShortSrcAddr()
          << " (PAN ID: 0x" << std::hex << hdr.GetSrcPanId() << std::dec << ")\n";
std::cout << "  Destination Address: " << hdr.GetShortDstAddr()
          << " (PAN ID: 0x" << std::hex << hdr.GetDstPanId() << std::dec << ")\n";
std::cout << "  Battery Level: " << static_cast<int>(hdr.GetBattery()) << "%\n";
std::cout << "  Evaluation: " << hdr.GetEvaluation() 
          << " (LQI: " << static_cast<int>(entry.lqi) 
          << ", Hops: " << static_cast<int>(entry.hops) << ")\n";
std::cout << "  Header Size: " << headerSize << " bytes\n";
std::cout << "  Payload Size: " << payloadSize << " bytes\n";
std::cout << "  Total Packet Size: " << totalSize << " bytes\n";
std::cout << "  Payload Data: \"" << data << "\"\n";
std::cout << "============================================\n\n";
}

return true;
}

// パケットタイプを取得する関数
static PacketType
GetPacketType(Ptr<Packet> p)
{
std::vector<uint8_t> buffer;
buffer.resize(p->GetSize());
p->CopyData(buffer.data(), p->GetSize());
if (buffer.size() < 1) {
return PACKET_TYPE_DATA; // デフォルト
}
return static_cast<PacketType>(buffer[0]);
}

// RREQをブロードキャストする関数
static void
SendRREQ(Ptr<NishiokaStack> stack, Mac16Address dst)
{
Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
if (!device) return;
Mac16Address myAddress = GetDeviceAddress(device);

RREQPacket rreq;
rreq.rreqId = g_rreqIdCounter++;
rreq.originator = myAddress;
rreq.dst = dst;
rreq.hopCount = 0;
rreq.energy = GetBatteryLevelFromStack(stack);
rreq.lqi = 255;

// このデバイスの処理済みRREQリストに追加
std::map<std::pair<Mac16Address, uint16_t>, bool>& processedRREQs = GetProcessedRREQs(device);
processedRREQs[std::make_pair(rreq.originator, rreq.rreqId)] = true;

Ptr<Packet> packet = CreateRREQPacket(rreq);

McpsDataRequestParams params;
params.m_dstPanId = 0xCAFE;
params.m_dstAddrMode = SHORT_ADDR;
params.m_dstAddr = Mac16Address("ff:ff"); // ブロードキャスト
params.m_msduHandle = 1;
params.m_txOptions = 0;
params.m_srcAddrMode = SHORT_ADDR;

g_txCount++;
std::cout << Simulator::Now().As(Time::S)
<< " [RREQ SEND] Node " << device->GetNode()->GetId()
<< " (Addr: " << myAddress << ") -> BROADCAST\n"
<< "  RREQ ID: " << rreq.rreqId
<< " | Originator: " << rreq.originator
<< " | Dst: " << rreq.dst
<< " | Energy: " << (int)rreq.energy << "%"
<< " | Total sent packets: " << g_txCount << std::endl;

stack->GetMac()->McpsDataRequest(params, packet);
}

// RREQ受信処理
static void
HandleRREQ(Ptr<NishiokaStack> stack, McpsDataIndicationParams params, const RREQPacket& rreq)
{
Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
if (!device) return;
Mac16Address myAddress = GetDeviceAddress(device);

std::cout << Simulator::Now().As(Time::S)
<< " [RREQ RECEIVE] Node " << device->GetNode()->GetId()
<< " (Addr: " << myAddress << ") <- from " << params.m_srcAddr << "\n"
<< "  RREQ ID: " << rreq.rreqId
<< " | Originator: " << rreq.originator
<< " | Dst: " << rreq.dst
<< " | HopCount: " << (int)rreq.hopCount << std::endl;

// 自分宛のRREQは無視（自分が送信元）
if (rreq.originator == myAddress) {
std::cout << "  [INFO] Ignoring my own RREQ" << std::endl;
return;
}

// このデバイスの処理済みRREQリストを取得
std::map<std::pair<Mac16Address, uint16_t>, bool>& processedRREQs = GetProcessedRREQs(device);

// 処理済みRREQチェック
auto rreqKey = std::make_pair(rreq.originator, rreq.rreqId);
if (processedRREQs.find(rreqKey) != processedRREQs.end()) {
std::cout << "  [INFO] Already processed this RREQ" << std::endl;
return;
}
processedRREQs[rreqKey] = true;

// RREQの送信元への逆経路を学習
LearnRoute(device, rreq.originator, params.m_srcAddr,
rreq.energy, params.m_mpduLinkQuality, rreq.hopCount + 1);

// 自分が目的地の場合、RREPを返送
if (rreq.dst == myAddress) {
std::cout << "  [INFO] I am the destination! Sending RREP" << std::endl;

RREPPacket rrep;
rrep.rreqId = rreq.rreqId;
rrep.originator = rreq.originator;
rrep.dst = myAddress;
rrep.hopCount = 0;
rrep.energy = GetBatteryLevelFromStack(stack);
rrep.lqi = params.m_mpduLinkQuality;

Ptr<Packet> packet = CreateRREPPacket(rrep);

McpsDataRequestParams replyParams;
replyParams.m_dstPanId = 0xCAFE;
replyParams.m_dstAddrMode = SHORT_ADDR;
replyParams.m_dstAddr = params.m_srcAddr; // RREQの送信元に返送
replyParams.m_msduHandle = 2;
replyParams.m_txOptions = 0;
replyParams.m_srcAddrMode = SHORT_ADDR;

g_txCount++;
std::cout << Simulator::Now().As(Time::S)
<< " [RREP SEND] Node " << device->GetNode()->GetId()
<< " -> Node " << replyParams.m_dstAddr
<< " | RREP ID: " << rrep.rreqId
<< " | Total sent packets: " << g_txCount << std::endl;

stack->GetMac()->McpsDataRequest(replyParams, packet);
} else {
// 目的地ではない場合、RREQを転送（ブロードキャスト）
std::cout << "  [INFO] Forwarding RREQ" << std::endl;

RREQPacket forwardRreq = rreq;
forwardRreq.hopCount++;
forwardRreq.energy = GetBatteryLevelFromStack(stack);
forwardRreq.lqi = params.m_mpduLinkQuality;

Ptr<Packet> packet = CreateRREQPacket(forwardRreq);

McpsDataRequestParams forwardParams;
forwardParams.m_dstPanId = 0xCAFE;
forwardParams.m_dstAddrMode = SHORT_ADDR;
forwardParams.m_dstAddr = Mac16Address("ff:ff"); // ブロードキャスト
forwardParams.m_msduHandle = 1;
forwardParams.m_txOptions = 0;
forwardParams.m_srcAddrMode = SHORT_ADDR;

g_txCount++;
stack->GetMac()->McpsDataRequest(forwardParams, packet);
}
}

// RREP受信処理
static void
HandleRREP(Ptr<NishiokaStack> stack, McpsDataIndicationParams params, const RREPPacket& rrep)
{
Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
if (!device) return;
Mac16Address myAddress = GetDeviceAddress(device);

std::cout << Simulator::Now().As(Time::S)
<< " [RREP RECEIVE] Node " << device->GetNode()->GetId()
<< " (Addr: " << myAddress << ") <- from " << params.m_srcAddr << "\n"
<< "  RREP ID: " << rrep.rreqId
<< " | Originator: " << rrep.originator
<< " | Dst: " << rrep.dst
<< " | HopCount: " << (int)rrep.hopCount << std::endl;

// 目的地への経路を学習
LearnRoute(device, rrep.dst, params.m_srcAddr,
rrep.energy, params.m_mpduLinkQuality, rrep.hopCount + 1);

// 自分がRREQの送信元の場合、経路確立完了
if (rrep.originator == myAddress) {
std::cout << "  [INFO] Route established to " << rrep.dst << "!" << std::endl;
return;
}

// 自分が中継ノードの場合、RREPを転送
std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
auto it = myRoutingTable.find(rrep.originator);
if (it != myRoutingTable.end()) {
std::cout << "  [INFO] Forwarding RREP to originator" << std::endl;

RREPPacket forwardRrep = rrep;
forwardRrep.hopCount++;
forwardRrep.energy = GetBatteryLevelFromStack(stack);
forwardRrep.lqi = params.m_mpduLinkQuality;

Ptr<Packet> packet = CreateRREPPacket(forwardRrep);

McpsDataRequestParams forwardParams;
forwardParams.m_dstPanId = 0xCAFE;
forwardParams.m_dstAddrMode = SHORT_ADDR;
forwardParams.m_dstAddr = it->second.nextHop; // RREQの送信元への次ホップ
forwardParams.m_msduHandle = 2;
forwardParams.m_txOptions = 0;
forwardParams.m_srcAddrMode = SHORT_ADDR;

g_txCount++;
stack->GetMac()->McpsDataRequest(forwardParams, packet);
} else {
std::cout << "  [ERROR] No route back to originator " << rrep.originator << std::endl;
}
}

static void
DataIndication(Ptr<NishiokaStack> stack, McpsDataIndicationParams params, Ptr<Packet> p)
{
Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
if (!device) return;
g_rxCount++;

// パケットタイプを確認
PacketType pktType = GetPacketType(p);

// RREQ処理
if (pktType == PACKET_TYPE_RREQ) {
RREQPacket rreq;
if (ParseRREQPacket(p, rreq)) {
HandleRREQ(stack, params, rreq);
}
return;
}

// RREP処理
if (pktType == PACKET_TYPE_RREP) {
RREPPacket rrep;
if (ParseRREPPacket(p, rrep)) {
HandleRREP(stack, params, rrep);
}
return;
}

// データパケット処理（NishiokaHeaderを使用）
RoutingEntry receivedEntry;
std::string data;
std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
<< " <- from Node " << params.m_srcAddr << "\n";
if (ExtractRoutingInfo(p, receivedEntry, data, true)) {
// NishiokaHeaderからルーティング情報を抽出したパケット
// 詳細情報はExtractRoutingInfo内で表示される
std::cout << "  Total received packets: " << g_rxCount << "\n";
} else {
// NishiokaHeaderなしパケット（従来形式）
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
RelayAndIndicate(Ptr<NishiokaStack> stack, McpsDataIndicationParams params, Ptr<Packet> p)
{
Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
if (!device) return;
g_rxCount++;

// パケットタイプを確認
PacketType pktType = GetPacketType(p);

// RREQ処理
if (pktType == PACKET_TYPE_RREQ) {
RREQPacket rreq;
if (ParseRREQPacket(p, rreq)) {
HandleRREQ(stack, params, rreq);
}
return;
}

// RREP処理
if (pktType == PACKET_TYPE_RREP) {
RREPPacket rrep;
if (ParseRREPPacket(p, rrep)) {
HandleRREP(stack, params, rrep);
}
return;
}

// データパケット処理
RoutingEntry receivedEntry;
std::string data;

// 自分のアドレスを取得
Mac16Address myAddress = GetDeviceAddress(device);

std::cout << Simulator::Now().As(Time::S) << " [RECEIVE] Node " << device->GetNode()->GetId()
<< " (Addr: " << myAddress << ") <- from Node " << params.m_srcAddr << "\n";

if (!ExtractRoutingInfo(p, receivedEntry, data, true)) {
std::cout << "[ERROR] Failed to extract routing info from packet\n";
return;
}

// 詳細情報はExtractRoutingInfo内で表示される
std::cout << "  Total received packets: " << g_rxCount << "\n";

// 経路学習：送信元への逆経路を学習（リバースパス学習）
// パケットが来た方向（params.m_srcAddr）を使って、送信元への経路を保存
if (params.m_srcAddr != myAddress) {
std::cout << " [INFO] Learning reverse route to sender " << params.m_srcAddr << std::endl;
LearnRoute(device,
params.m_srcAddr, // 送信元への経路
params.m_srcAddr, // 次ホップは送信元（直接通信できた）
receivedEntry.energy, // エネルギー情報
params.m_mpduLinkQuality, // 実測のLQI
1); // 1ホップで到達
}

// 最終目的地が自分かチェック
if (receivedEntry.dst == myAddress) {
std::cout << " [INFO] I am the final destination. Packet delivered.\n";
return; // 自分宛なので転送せず終了
}

// 最終目的地ではないので転送処理
std::cout << " [INFO] Not for me (dst=" << receivedEntry.dst << "). Looking for route...\n";
// まずNWK層のルーティングテーブルを参照
Ptr<NishiokaNwk> nwk = stack->GetNwk();
Mac16Address nextHop;
bool routeFound = false;

if (nwk && nwk->GetNextHop(receivedEntry.dst, nextHop)) {
// NWK層からルートが見つかった
std::cout << " [ROUTING] Route found via NWK layer - NextHop: " << nextHop << std::endl;
routeFound = true;
} else {
// フォールバック: アプリケーション層のルーティングテーブルを参照
std::map<Mac16Address, RoutingEntry>& myRoutingTable = GetRoutingTable(device);
auto it = myRoutingTable.find(receivedEntry.dst);
if (it != myRoutingTable.end()) {
nextHop = it->second.nextHop;
routeFound = true;
std::cout << " [ROUTING] Route found via application layer - NextHop: " << nextHop << std::endl;
}
}

if (routeFound) {
// 逆走防止アルゴリズム：nextHopから送信元アドレス（params.m_srcAddr）を除外
// → nextHopが送信元と同じ場合は、そこには送信しない（ループ防止）
if (nextHop == params.m_srcAddr) {
std::cout << " [ROUTING ERROR] NextHop(" << nextHop
<< ") is same as sender(" << params.m_srcAddr
<< "). Preventing reverse routing!\n";
return;
}
// 受信したエントリのホップ数、LQI、バッテリー残量を更新
receivedEntry.hops++;
receivedEntry.lqi = params.m_mpduLinkQuality; // 最新のLQIで更新
receivedEntry.energy = GetBatteryLevelFromStack(stack); // 中継デバイスの最新バッテリー残量
// dst と nextHop が一致しているかチェック
if (receivedEntry.dst == nextHop) {
std::cout << " [ROUTING] Dst matches NextHop - Final hop to destination!\n";
} else {
std::cout << " [ROUTING] Intermediate hop - NextHop: " << nextHop << "\n";
}
std::cout << " [ROUTING] Route found - NextHop: " << nextHop
<< " | Updated Hops: " << (int)receivedEntry.hops
<< " | Updated LQI: " << (int)receivedEntry.lqi
<< " | Battery: " << (int)receivedEntry.energy << "%" << std::endl;
// 更新したルーティング情報でパケットを再作成（NishiokaHeaderを使用）
Mac16Address myAddr = GetDeviceAddress(device);
std::cout << Simulator::Now().As(Time::S)
<< " [RELAY/SEND] Node " << device->GetNode()->GetId()
<< " -> Node " << nextHop << " (Forwarding with updated NishiokaHeader)\n";
Ptr<Packet> forwardPacket = CreatePacketWithRoutingInfo(receivedEntry, data, myAddr);
// 次ホップへ転送
McpsDataRequestParams relayParams;
relayParams.m_dstPanId = 0xCAFE;
relayParams.m_dstAddrMode = SHORT_ADDR;
relayParams.m_dstAddr = nextHop; // NWK層またはアプリケーション層から取得したnextHop
relayParams.m_msduHandle = 2;
relayParams.m_txOptions = 0;
relayParams.m_srcAddrMode = SHORT_ADDR;

g_txCount++;
std::cout << "  Total sent packets: " << g_txCount << std::endl;
stack->GetMac()->McpsDataRequest(relayParams, forwardPacket);
} else {
std::cout << " [ROUTING ERROR] No route found for destination: "
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

// 動的にショートアドレスを割り当て
Mac16Address assignedAddr;
assignedAddr = Mac16Address::ConvertFrom(Mac16Address(g_nextShortAddr));
g_nextShortAddr++;

MlmeAssociateResponseParams respParams;
respParams.m_assocShortAddr = assignedAddr;
respParams.m_extDevAddr = params.m_extDevAddr;
respParams.m_status = MacStatus::SUCCESS;
device->GetMac()->MlmeAssociateResponse(respParams);

// アソシエーション成功時にデバイス数をカウント
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
g_dev01ShortAddr = params.m_assocShortAddr;
g_dev01Addr = params.m_assocShortAddr; // グローバル変数も更新

std::cout << "\n========== dev01 Association Complete ==========\n";
std::cout << "dev01 will use RREQ for route discovery\n";
std::cout << "===============================================\n\n";

// dev02のアソシエーション要求をここでスケジューリング
Simulator::Schedule(Seconds(0.5), [=]() {
MlmeAssociateRequestParams associateParams;
associateParams.m_chNum = 0xD;
associateParams.m_chPage = 0;
associateParams.m_coordAddrMode = SHORT_ADDR;
associateParams.m_coordPanId = 0xCAFE;
associateParams.m_capabilityInfo = 0x80; // short address割当希望
associateParams.m_coordShortAddr = g_dev01ShortAddr;
g_dev02Stack->GetMac()->MlmeAssociateRequest(associateParams);
});
}
// dev02のアソシエーション成功後、RREQでルート発見してからメッセージを送信
if (params.m_status == MacStatus::SUCCESS && device == g_uartNetDevice2) {
g_dev02Addr = params.m_assocShortAddr; // グローバル変数を更新

std::cout << "\n========== dev02 Association Complete ==========\n";
std::cout << "All devices associated. Starting route discovery...\n";
std::cout << "===============================================\n\n";

// コーディネータからdev02へのRREQを送信
Simulator::Schedule(Seconds(0.2), [=]() {
Mac16Address targetAddr = Mac16Address("00:03"); // dev02のアドレス
std::cout << "Coordinator initiating route discovery to dev02\n";
SendRREQ(g_coordinatorStack, targetAddr);

// RREPを待ってからデータ送信（1秒後）
Simulator::Schedule(Seconds(1.0), [=]() {
// コーディネータのルーティングテーブルからdev02への経路を検索
auto it = g_routingTableCoordinator.find(Mac16Address("00:03"));
if (it != g_routingTableCoordinator.end()) {
RoutingEntry& entry = it->second;
// 送信時にコーディネータの最新バッテリー残量を反映
entry.energy = GetBatteryLevel(g_coordinatorDevice);
std::string sendMsg = "Hello from Coordinator to dev02";
// ルーティング情報を埋め込んだパケットを作成（NishiokaHeaderを使用）
Mac16Address coordinatorAddr = GetDeviceAddress(g_coordinatorDevice);
std::cout << Simulator::Now().As(Time::S)
<< " [SEND DATA] Coordinator (Node 0)"
<< " -> Node " << entry.nextHop << " (NextHop from routing table)\n";
Ptr<Packet> packet = CreatePacketWithRoutingInfo(entry, sendMsg, coordinatorAddr);
McpsDataRequestParams dataParams;
dataParams.m_dstPanId = 0xCAFE;
dataParams.m_dstAddrMode = SHORT_ADDR;
dataParams.m_dstAddr = entry.nextHop; // ルーティングテーブルのnextHopを使用
dataParams.m_msduHandle = 3;
dataParams.m_txOptions = 0;
dataParams.m_srcAddrMode = SHORT_ADDR;

g_txCount++;
std::cout << "  Total sent packets: " << g_txCount << std::endl;
g_coordinatorStack->GetMac()->McpsDataRequest(dataParams, packet);
} else {
std::cout << "  [ERROR] No route to dev02 found after RREQ!" << std::endl;
}
});
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

// dev01がdev02に動的にショートアドレスを割り当て
Mac16Address assignedAddr = Mac16Address::ConvertFrom(Mac16Address(g_nextShortAddrDev01));
g_nextShortAddrDev01++;

MlmeAssociateResponseParams respParams;
respParams.m_assocShortAddr = assignedAddr;
respParams.m_extDevAddr = params.m_extDevAddr;
respParams.m_status = MacStatus::SUCCESS;
device->GetMac()->MlmeAssociateResponse(respParams);

// アソシエーション成功時にデバイス数をカウント
g_associatedDeviceCount++;
std::cout << "Assigned short address (dev01): " << assignedAddr
<< " | Total associated devices: " << g_associatedDeviceCount
<< "/" << g_totalDevicesInPAN << std::endl;
}
int
main(int argc, char* argv[])
{
// We are using a real piece of hardware, therefore we need to use realtime
GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

// PAN内デバイス数の表示
std::cout << "\n========== Network Configuration ==========\n";
std::cout << "Total devices in PAN: " << g_totalDevicesInPAN << std::endl;
std::cout << " - Coordinator: 1\n";
std::cout << " - End Devices: " << (g_totalDevicesInPAN - 1) << std::endl;
std::cout << "==========================================\n\n";

// Create NetDevices
g_coordinatorDevice = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
g_uartNetDevice1 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB1");
g_uartNetDevice2 = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB2");

// Create NetDeviceContainer
NetDeviceContainer netDevices;
netDevices.Add(g_coordinatorDevice);
netDevices.Add(g_uartNetDevice1);
netDevices.Add(g_uartNetDevice2);

// Define positions for each device
std::vector<Vector> positions;
positions.push_back(Vector(0, 0, 0));    // Coordinator
positions.push_back(Vector(0, 90, 0));   // Dev01
positions.push_back(Vector(0, 180, 0));   // Dev02

// Use NishiokaHelper to install stacks
NishiokaHelper helper;
g_stacks = helper.Install(netDevices, positions);

// Get stacks from container
g_coordinatorStack = g_stacks.Get(0);
g_dev01Stack = g_stacks.Get(1);
g_dev02Stack = g_stacks.Get(2);

// Configure MAC layer settings using helper (channel, PAN ID, addresses)
std::vector<Mac16Address> addresses;
addresses.push_back(Mac16Address("00:01"));  // Coordinator
addresses.push_back(Mac16Address("FF:FE"));  // Dev01 (temporary, will be assigned during association)
addresses.push_back(Mac16Address("FF:FD"));  // Dev02 (temporary, will be assigned during association)
helper.ConfigureMac(g_stacks, 0xD, 0xCAFE, addresses);

// Set up callbacks (application-specific, cannot be automated in helper)
g_coordinatorStack->GetMac()->SetMcpsDataConfirmCallback(
MakeBoundCallback(&DataConfirm, g_coordinatorStack));
g_coordinatorStack->GetMac()->SetMcpsDataIndicationCallback(
MakeBoundCallback(&DataIndication, g_coordinatorStack));
g_coordinatorStack->GetMac()->SetMlmeAssociateIndicationCallback(
MakeBoundCallback(&AssociateIndication, g_coordinatorDevice));
g_dev01Stack->GetMac()->SetMcpsDataIndicationCallback(
MakeBoundCallback(&RelayAndIndicate, g_dev01Stack));
g_dev01Stack->GetMac()->SetMlmeAssociateConfirmCallback(
MakeBoundCallback(&AssociateConfirm, g_uartNetDevice1));
g_dev01Stack->GetMac()->SetMlmeAssociateIndicationCallback(
MakeBoundCallback(&AssociateIndicationDev01, g_uartNetDevice1));
g_dev02Stack->GetMac()->SetMcpsDataIndicationCallback(
MakeBoundCallback(&DataIndication, g_dev02Stack));
g_dev02Stack->GetMac()->SetMlmeAssociateConfirmCallback(
MakeBoundCallback(&AssociateConfirm, g_uartNetDevice2));
// コーディネータとしてネットワーク開始
MlmeStartRequestParams startParams;
startParams.m_PanId = 0xCAFE;
startParams.m_logCh = 0xD; // 13ch
startParams.m_logChPage = 0;
startParams.m_bcnOrd = 15;
startParams.m_sfrmOrd = 15;
startParams.m_panCoor = true;
startParams.m_battLifeExt = false;
startParams.m_coorRealgn = false;
g_coordinatorStack->GetMac()->MlmeStartRequest(startParams);

// バッテリー残量の初期読み込み
std::cout << "\n========== Initial Battery Level Reading ==========\n";
UpdateBatteryLevels();

std::cout << "\n========== AODV-style Route Discovery Enabled ==========\n";
std::cout << "Routing tables will be populated dynamically using RREQ/RREP\n";
std::cout << "========================================================\n\n";

// dev01がdev00にアソシエーション要求
Simulator::Schedule(Seconds(0.5), [=]() {
MlmeAssociateRequestParams associateParams;
associateParams.m_chNum = 0xD;
associateParams.m_chPage = 0;
associateParams.m_coordAddrMode = SHORT_ADDR;
associateParams.m_coordPanId = 0xCAFE;
associateParams.m_capabilityInfo = 0x80; // short address割当希望
associateParams.m_coordShortAddr = Mac16Address("00:01"); // コーディネータのアドレス
g_dev01Stack->GetMac()->MlmeAssociateRequest(associateParams);
});
// dev02のアソシエーション要求はAssociateConfirmでスケジューリング
// シミュレーション終了前にルーティングテーブルを表示
Simulator::Schedule(Seconds(4.9), []() {
std::cout << "\n========== Final Routing Tables (After RREQ/RREP) ==========\n";
std::cout << "Coordinator:\n";
PrintRoutingTable(g_routingTableCoordinator);
if (g_coordinatorStack && g_coordinatorStack->GetNwk()) {
std::cout << "  NWK Layer Routes: " << g_coordinatorStack->GetNwk()->GetRouteCount() << std::endl;
}
std::cout << "Dev01:\n";
PrintRoutingTable(g_routingTableDev01);
if (g_dev01Stack && g_dev01Stack->GetNwk()) {
std::cout << "  NWK Layer Routes: " << g_dev01Stack->GetNwk()->GetRouteCount() << std::endl;
}
std::cout << "Dev02:\n";
PrintRoutingTable(g_routingTableDev02);
if (g_dev02Stack && g_dev02Stack->GetNwk()) {
std::cout << "  NWK Layer Routes: " << g_dev02Stack->GetNwk()->GetRouteCount() << std::endl;
}
});

Simulator::Stop(Seconds(5));
Simulator::Run();
Simulator::Destroy();
return 0;
}