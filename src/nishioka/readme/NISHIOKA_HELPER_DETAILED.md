# NishiokaHelper 詳細説明

## 1. NishiokaHelperの動作と意図

### 1.1 設計思想

NishiokaHelperは、zigbee-helperと同様の設計思想に基づいて作成されました。主な目的は以下の通りです：

1. **複雑な操作の抽象化**: NishiokaHeaderの設定や抽出などの複雑な操作を、簡単なメソッド呼び出しに変換
2. **コードの再利用性**: 同じヘッダ操作ロジックを複数のプログラムで再利用可能に
3. **エラーハンドリングの統一**: パケット操作時のエラーチェックを統一
4. **保守性の向上**: ヘッダ操作のロジックが1箇所に集約されるため、変更が容易

### 1.2 各メソッドの詳細動作

#### CreatePacket() - パケット作成

**動作:**
1. 文字列データから`Packet`オブジェクトを作成
2. `NishiokaHeader`オブジェクトを生成
3. フレームタイプを`CUSTOM_DATA`に設定
4. シーケンス番号を設定（指定されていない場合は自動インクリメント）
5. 送信元・宛先アドレスとPAN IDを設定
6. バッテリー残量を設定
7. LQIとホップ数を`Evaluation`フィールドに組み合わせて設定
   - 形式: `(LQI << 8) | hops` (上位8bitがLQI、下位8bitがホップ数)
8. ヘッダをパケットに追加
9. NS_LOGでデバッグ情報を出力

**コード例:**
```cpp
NishiokaHelper helper;
Ptr<Packet> packet = helper.CreatePacket(
    "Hello World",           // データ
    Mac16Address("00:01"),   // 送信元
    Mac16Address("00:02"),   // 宛先
    0xCAFE,                 // PAN ID
    75,                      // バッテリー（%）
    200,                     // LQI
    1                        // ホップ数
);
```

**内部処理の流れ:**
```
CreatePacket() 呼び出し
  ↓
Packet作成（データから）
  ↓
NishiokaHeader作成
  ↓
各フィールド設定
  - FrameType: CUSTOM_DATA
  - SeqNum: 自動インクリメント or 指定値
  - SrcAddr, DstAddr: 指定されたアドレス
  - Battery: 指定されたバッテリー値
  - Evaluation: (LQI << 8) | hops で計算
  ↓
ヘッダをパケットに追加
  ↓
完成したパケットを返す
```

#### ExtractHeader() - ヘッダ抽出

**動作:**
1. パケットのコピーを作成（元のパケットを変更しないため）
2. パケットサイズがヘッダサイズ以上かチェック
3. アドレスモードを設定（SHORTADDR）
4. ヘッダを削除（`RemoveHeader()`）
5. 残りのペイロードデータを文字列として抽出
6. 抽出したヘッダとデータを参照引数に格納
7. 成功/失敗をbool値で返す

**コード例:**
```cpp
NishiokaHelper helper;
NishiokaHeader header;
std::string data;

if (helper.ExtractHeader(packet, header, data)) {
    // 抽出成功
    std::cout << "Source: " << header.GetShortSrcAddr() << std::endl;
    std::cout << "Data: " << data << std::endl;
} else {
    // 抽出失敗（パケットが小さすぎるなど）
    std::cout << "Failed to extract header" << std::endl;
}
```

**内部処理の流れ:**
```
ExtractHeader() 呼び出し
  ↓
パケットのコピー作成
  ↓
パケットサイズチェック
  ↓（サイズ不足の場合）
  return false
  ↓（サイズOKの場合）
アドレスモード設定
  ↓
RemoveHeader()でヘッダ削除
  ↓
残りのデータを文字列として抽出
  ↓
headerとdataに格納
  ↓
return true
```

#### ExtractRoutingInfo() - ルーティング情報抽出

**動作:**
1. ヘッダからバッテリー残量を取得
2. `Evaluation`フィールドからLQIとホップ数を分離
   - LQI: `(evaluation >> 8) & 0xFF` (上位8bit)
   - Hops: `evaluation & 0xFF` (下位8bit)
3. 参照引数に値を格納

**コード例:**
```cpp
NishiokaHelper helper;
NishiokaHeader header;
// ... headerを抽出後 ...

uint8_t battery, lqi, hops;
helper.ExtractRoutingInfo(header, battery, lqi, hops);

std::cout << "Battery: " << (int)battery << "%" << std::endl;
std::cout << "LQI: " << (int)lqi << std::endl;
std::cout << "Hops: " << (int)hops << std::endl;
```

**Evaluationフィールドの構造:**
```
16bit Evaluationフィールド:
┌─────────┬─────────┐
│   LQI   │  Hops   │
│  (8bit) │  (8bit) │
└─────────┴─────────┘
  上位8bit  下位8bit

例: LQI=200, Hops=3
Evaluation = (200 << 8) | 3 = 51203
```

#### UpdateRoutingInfo() - ルーティング情報更新

**動作:**
1. 元のパケットからヘッダとデータを抽出
2. 宛先アドレスとPAN IDを元のヘッダから取得
3. 新しい送信元アドレス、バッテリー、LQI、ホップ数で新しいパケットを作成
4. 新しいパケットを返す

**用途:** 中継ノードでパケットを転送する際に、ルーティング情報を更新する

**コード例:**
```cpp
NishiokaHelper helper;

// 中継ノードでパケットを受信
Ptr<Packet> receivedPacket = ...;

// ルーティング情報を更新（ホップ数+1、新しい送信元アドレスなど）
Ptr<Packet> forwardedPacket = helper.UpdateRoutingInfo(
    receivedPacket,
    Mac16Address("00:02"),  // 中継ノードのアドレス
    80,                      // 中継ノードのバッテリー
    180,                     // 新しいLQI
    2                        // ホップ数+1
);
```

**内部処理の流れ:**
```
UpdateRoutingInfo() 呼び出し
  ↓
ExtractHeader()で元のパケットから情報取得
  ↓
元のヘッダから宛先アドレスとPAN IDを取得
  ↓
CreatePacket()で新しいパケットを作成
  - データ: 元のデータ
  - 送信元: 新しい送信元アドレス（中継ノード）
  - 宛先: 元の宛先アドレス
  - バッテリー: 新しいバッテリー値
  - LQI: 新しいLQI
  - ホップ数: 新しいホップ数
  ↓
新しいパケットを返す
```

#### シーケンス番号管理

**動作:**
- `m_seqNumCounter`が内部で管理される
- `CreatePacket()`が呼ばれるたびに自動的にインクリメント
- `ResetSeqNum()`でリセット可能
- `GetNextSeqNum()`で次のシーケンス番号を取得（インクリメントしない）

**コード例:**
```cpp
NishiokaHelper helper;

Ptr<Packet> p1 = helper.CreatePacket("Data1", ...);  // SeqNum: 0
Ptr<Packet> p2 = helper.CreatePacket("Data2", ...);  // SeqNum: 1
Ptr<Packet> p3 = helper.CreatePacket("Data3", ...);  // SeqNum: 2

// シーケンス番号をリセット
helper.ResetSeqNum();

Ptr<Packet> p4 = helper.CreatePacket("Data4", ...);  // SeqNum: 0
```

## 2. multihop2.ccでの使用例と変化

### 2.1 現在の実装（NishiokaHelper使用前）

#### CreatePacketWithRoutingInfo()関数

**現在のコード（約40行）:**
```cpp
static Ptr<Packet>
CreatePacketWithRoutingInfo(const RoutingEntry& entry, const std::string& data, Mac16Address srcAddr)
{
    // ペイロードデータを作成
    Ptr<Packet> payload = Create<Packet>((const uint8_t*)data.c_str(), data.size());
    uint32_t payloadSize = payload->GetSize();

    // NishiokaHeaderを作成して設定
    NishiokaHeader hdr;
    hdr.SetFrameType(NishiokaHeader::CUSTOM_DATA);
    hdr.SetSeqNum(entry.entryId & 0xFF);
    hdr.SetDstAddrFields(0xCAFE, entry.dst);
    hdr.SetSrcAddrFields(0xCAFE, srcAddr);
    hdr.SetBattery(entry.energy);
    uint16_t evaluation = (static_cast<uint16_t>(entry.lqi) << 8) | entry.hops;
    hdr.SetEvaluation(evaluation);

    // ヘッダを追加
    payload->AddHeader(hdr);

    // 詳細なログ出力（約20行）
    std::cout << "\n========== [NISHIOKA HEADER ADDED] ==========\n";
    // ... ログ出力 ...
    std::cout << "==========================================\n\n";

    return payload;
}
```

**問題点:**
1. コードが長く、可読性が低い
2. ヘッダ設定のロジックが複雑
3. Evaluationフィールドの計算が分散している
4. ログ出力が関数内に混在している

### 2.2 NishiokaHelper使用後の実装

#### CreatePacketWithRoutingInfo()関数（改善版）

**改善後のコード（約10行）:**
```cpp
static Ptr<Packet>
CreatePacketWithRoutingInfo(const RoutingEntry& entry, const std::string& data, Mac16Address srcAddr)
{
    NishiokaHelper helper;
    
    // NishiokaHelperを使用してパケットを作成
    Ptr<Packet> packet = helper.CreatePacket(
        data,                    // データ
        srcAddr,                 // 送信元アドレス
        entry.dst,               // 宛先アドレス
        0xCAFE,                  // PAN ID
        entry.energy,            // バッテリー残量
        entry.lqi,               // LQI
        entry.hops,              // ホップ数
        entry.entryId & 0xFF     // シーケンス番号
    );
    
    // ログ出力（必要に応じて）
    // ログ出力はNishiokaHelper内部でNS_LOGを使用して行われる
    
    return packet;
}
```

**改善点:**
1. コードが約75%短縮（40行 → 10行）
2. 可読性が大幅に向上
3. ヘッダ設定ロジックがNishiokaHelperに集約
4. ログ出力はNS_LOGで統一管理可能

### 2.3 ExtractRoutingInfo()関数の変化

#### 現在の実装（約60行）:

```cpp
static bool
ExtractRoutingInfo(Ptr<Packet> p, RoutingEntry& entry, std::string& data, bool verbose = true)
{
    // パケットのコピーを作成
    Ptr<Packet> pktCopy = p->Copy();
    uint32_t totalSize = pktCopy->GetSize();

    // NishiokaHeaderを抽出
    NishiokaHeader hdr;
    hdr.SetDstAddrMode(NishiokaHeader::SHORTADDR);
    hdr.SetSrcAddrMode(NishiokaHeader::SHORTADDR);

    // ヘッダを削除
    if (pktCopy->GetSize() < hdr.GetSerializedSize()) {
        return false;
    }

    pktCopy->RemoveHeader(hdr);
    uint32_t headerSize = hdr.GetSerializedSize();
    uint32_t payloadSize = pktCopy->GetSize();

    // ルーティング情報を設定
    entry.entryId = hdr.GetSeqNum();
    entry.dst = hdr.GetShortDstAddr();
    entry.energy = static_cast<uint8_t>(hdr.GetBattery());
    uint16_t evaluation = hdr.GetEvaluation();
    entry.lqi = static_cast<uint8_t>((evaluation >> 8) & 0xFF);
    entry.hops = static_cast<uint8_t>(evaluation & 0xFF);

    // ペイロードデータを抽出
    std::vector<uint8_t> buffer;
    buffer.resize(pktCopy->GetSize());
    pktCopy->CopyData(buffer.data(), pktCopy->GetSize());
    data = std::string(buffer.begin(), buffer.end());

    // ログ出力（約20行）
    if (verbose) {
        // ... ログ出力 ...
    }
    
    return true;
}
```

#### NishiokaHelper使用後（約15行）:

```cpp
static bool
ExtractRoutingInfo(Ptr<Packet> p, RoutingEntry& entry, std::string& data, bool verbose = true)
{
    NishiokaHelper helper;
    NishiokaHeader header;
    
    // NishiokaHelperを使用してヘッダとデータを抽出
    if (!helper.ExtractHeader(p, header, data)) {
        return false;  // 抽出失敗
    }
    
    // ルーティング情報を抽出
    helper.ExtractRoutingInfo(header, entry.energy, entry.lqi, entry.hops);
    
    // RoutingEntryに情報を設定
    entry.entryId = header.GetSeqNum();
    entry.dst = header.GetShortDstAddr();
    
    // ログ出力（必要に応じて）
    if (verbose) {
        std::cout << "\n========== [NISHIOKA HEADER EXTRACTED] ==========\n";
        std::cout << "  Source: " << header.GetShortSrcAddr() << "\n";
        std::cout << "  Destination: " << header.GetShortDstAddr() << "\n";
        std::cout << "  Battery: " << (int)entry.energy << "%\n";
        std::cout << "  LQI: " << (int)entry.lqi << "\n";
        std::cout << "  Hops: " << (int)entry.hops << "\n";
        std::cout << "  Data: \"" << data << "\"\n";
        std::cout << "============================================\n\n";
    }
    
    return true;
}
```

**改善点:**
1. コードが約75%短縮（60行 → 15行）
2. ヘッダ抽出ロジックがNishiokaHelperに集約
3. Evaluationフィールドの分解処理が簡潔に
4. エラーハンドリングが統一

### 2.4 中継ノードでのパケット転送の変化

#### 現在の実装:

```cpp
// 中継ノードでパケットを受信
RoutingEntry receivedEntry;
std::string data;
ExtractRoutingInfo(packet, receivedEntry, data);

// 新しいルーティング情報でパケットを作成
RoutingEntry forwardEntry;
forwardEntry.dst = receivedEntry.dst;
forwardEntry.energy = GetBatteryLevel(device);
forwardEntry.lqi = params.m_mpduLinkQuality;
forwardEntry.hops = receivedEntry.hops + 1;
forwardEntry.entryId = receivedEntry.entryId;

Mac16Address myAddr = GetDeviceAddress(device);
Ptr<Packet> forwardPacket = CreatePacketWithRoutingInfo(forwardEntry, data, myAddr);
```

#### NishiokaHelper使用後:

```cpp
// 中継ノードでパケットを受信
NishiokaHelper helper;

// ルーティング情報を更新して新しいパケットを作成
Mac16Address myAddr = GetDeviceAddress(device);
uint8_t myBattery = GetBatteryLevel(device);
uint8_t newLqi = params.m_mpduLinkQuality;

// 元のパケットからホップ数を取得
NishiokaHeader originalHeader;
std::string data;
helper.ExtractHeader(packet, originalHeader, data);
uint8_t battery, lqi, hops;
helper.ExtractRoutingInfo(originalHeader, battery, lqi, hops);

// ルーティング情報を更新した新しいパケットを作成
Ptr<Packet> forwardPacket = helper.UpdateRoutingInfo(
    packet,
    myAddr,      // 新しい送信元アドレス
    myBattery,   // 新しいバッテリー
    newLqi,      // 新しいLQI
    hops + 1     // ホップ数+1
);
```

**または、より簡潔に:**

```cpp
// UpdateRoutingInfoを使用（内部でExtractHeaderが呼ばれる）
Ptr<Packet> forwardPacket = helper.UpdateRoutingInfo(
    packet,
    myAddr,
    myBattery,
    newLqi,
    hops + 1
);
```

## 3. プログラム全体への影響

### 3.1 コード量の削減

- `CreatePacketWithRoutingInfo()`: 約40行 → 約10行（75%削減）
- `ExtractRoutingInfo()`: 約60行 → 約15行（75%削減）
- **合計: 約100行 → 約25行（75%削減）**

### 3.2 保守性の向上

1. **ロジックの集約**: ヘッダ操作のロジックがNishiokaHelperに集約されるため、変更が容易
2. **バグの減少**: 同じロジックを複数箇所で書く必要がなくなり、バグが発生しにくい
3. **テストの容易さ**: NishiokaHelper単体でテスト可能

### 3.3 再利用性の向上

- 他のプログラムでも同じNishiokaHelperを使用可能
- 新しい機能追加時も、NishiokaHelperに追加するだけで全プログラムで利用可能

### 3.4 デバッグの改善

- NS_LOGを使用した統一的なログ出力
- デバッグレベルを調整することで、詳細度を制御可能

## 4. 実際の使用例

### 4.1 送信側（Coordinator → dev02）

**変更前:**
```cpp
RoutingEntry entry;
entry.dst = dev02Addr;
entry.energy = g_batteryLevelCoordinator;
entry.lqi = 255;
entry.hops = 0;
entry.entryId = 1;

Mac16Address coordinatorAddr = GetDeviceAddress(g_coordinatorDevice);
Ptr<Packet> packet = CreatePacketWithRoutingInfo(entry, sendMsg, coordinatorAddr);
```

**変更後:**
```cpp
NishiokaHelper helper;
Mac16Address coordinatorAddr = GetDeviceAddress(g_coordinatorDevice);
Ptr<Packet> packet = helper.CreatePacket(
    sendMsg,
    coordinatorAddr,
    dev02Addr,
    0xCAFE,
    g_batteryLevelCoordinator,
    255,
    0,
    1
);
```

### 4.2 受信側（dev02で受信）

**変更前:**
```cpp
RoutingEntry entry;
std::string data;
if (ExtractRoutingInfo(p, entry, data)) {
    // ルーティング情報を使用
    uint8_t battery = entry.energy;
    uint8_t lqi = entry.lqi;
    uint8_t hops = entry.hops;
}
```

**変更後:**
```cpp
NishiokaHelper helper;
NishiokaHeader header;
std::string data;
if (helper.ExtractHeader(p, header, data)) {
    uint8_t battery, lqi, hops;
    helper.ExtractRoutingInfo(header, battery, lqi, hops);
    // ルーティング情報を使用
}
```

### 4.3 中継ノード（dev01で中継）

**変更前:**
```cpp
RoutingEntry receivedEntry;
std::string data;
ExtractRoutingInfo(p, receivedEntry, data);

RoutingEntry forwardEntry;
forwardEntry.dst = receivedEntry.dst;
forwardEntry.energy = g_batteryLevelDev01;
forwardEntry.lqi = params.m_mpduLinkQuality;
forwardEntry.hops = receivedEntry.hops + 1;
forwardEntry.entryId = receivedEntry.entryId;

Mac16Address myAddr = GetDeviceAddress(device);
Ptr<Packet> forwardPacket = CreatePacketWithRoutingInfo(forwardEntry, data, myAddr);
```

**変更後:**
```cpp
NishiokaHelper helper;

// 元のパケットから情報を取得
NishiokaHeader originalHeader;
std::string data;
helper.ExtractHeader(p, originalHeader, data);
uint8_t battery, lqi, hops;
helper.ExtractRoutingInfo(originalHeader, battery, lqi, hops);

// ルーティング情報を更新
Mac16Address myAddr = GetDeviceAddress(device);
Ptr<Packet> forwardPacket = helper.UpdateRoutingInfo(
    p,
    myAddr,
    g_batteryLevelDev01,
    params.m_mpduLinkQuality,
    hops + 1
);
```

## 5. まとめ

NishiokaHelperを使用することで：

1. **コードの簡潔化**: 約75%のコード削減
2. **可読性の向上**: 複雑な操作が1行のメソッド呼び出しに
3. **保守性の向上**: ロジックの集約により変更が容易
4. **再利用性の向上**: 他のプログラムでも同じヘルパーを使用可能
5. **エラーハンドリングの統一**: パケット操作時のエラーチェックが統一
6. **デバッグの改善**: NS_LOGによる統一的なログ出力

これらの改善により、multihop2.ccのような複雑なマルチホップ通信プログラムでも、コードが簡潔で理解しやすくなります。

