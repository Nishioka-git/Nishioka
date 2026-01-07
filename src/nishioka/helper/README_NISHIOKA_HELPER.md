# NishiokaHelper 使用方法

## 概要

`NishiokaHelper`は、NishiokaHeaderを使用したパケットの作成と管理を簡便にするためのヘルパークラスです。zigbee-helperと同様の設計思想に基づいて作成されており、マルチホップ通信におけるルーティング情報の管理を容易にします。

## 作成意図

### 1. **コードの簡略化**

multihop2.ccで行っていた以下のような複雑な操作を、シンプルなメソッド呼び出しに置き換えます：

**従来の方法（multihop2.cc）:**
```cpp
// ペイロードデータを作成
Ptr<Packet> payload = Create<Packet>((const uint8_t*)data.c_str(), data.size());

// NishiokaHeaderを作成して設定
NishiokaHeader hdr;
hdr.SetFrameType(NishiokaHeader::CUSTOM_DATA);
hdr.SetSeqNum(entry.entryId & 0xFF);
hdr.SetDstAddrFields(0xCAFE, entry.dst);
hdr.SetSrcAddrFields(0xCAFE, srcAddr);
hdr.SetBattery(entry.energy);
uint16_t evaluation = (static_cast<uint16_t>(entry.lqi) << 8) | entry.hops;
hdr.SetEvaluation(evaluation);
payload->AddHeader(hdr);
```

**NishiokaHelperを使用:**
```cpp
NishiokaHelper helper;
Ptr<Packet> packet = helper.CreatePacket(
    data, srcAddr, dstAddr, 0xCAFE, battery, lqi, hops
);
```

### 2. **ルーティング情報の統一管理**

- バッテリー残量、LQI、ホップ数を簡単に設定・取得
- EvaluationフィールドへのLQIとホップ数の組み合わせを自動処理
- シーケンス番号の自動管理

### 3. **マルチホップ通信の支援**

- 中継ノードでのルーティング情報更新を簡単に実行
- パケットの再作成を自動化
- ヘッダの抽出と情報取得を統合

### 4. **エラーハンドリングの改善**

- パケットサイズの検証
- ヘッダ抽出の失敗時の適切な処理
- デバッグログの提供

## 主な機能

### 1. パケット作成

#### 16ビットアドレスを使用
```cpp
NishiokaHelper helper;
Ptr<Packet> packet = helper.CreatePacket(
    "Hello World",           // データ
    Mac16Address("00:01"),  // 送信元アドレス
    Mac16Address("00:02"),  // 宛先アドレス
    0xCAFE,                 // PAN ID
    75,                      // バッテリー残量（%）
    200,                     // LQI
    1                        // ホップ数
);
```

#### 64ビットアドレスを使用
```cpp
Ptr<Packet> packet = helper.CreatePacket(
    "Hello World",
    Mac64Address("00:1b:c5:01:22:01:6b:d9"),
    Mac64Address("00:1b:c5:01:22:01:75:24"),
    0xCAFE, 75, 200, 1
);
```

### 2. ヘッダ抽出

```cpp
NishiokaHelper helper;
NishiokaHeader header;
std::string data;

if (helper.ExtractHeader(packet, header, data)) {
    // ヘッダとデータの抽出に成功
    std::cout << "Source: " << header.GetShortSrcAddr() << std::endl;
    std::cout << "Data: " << data << std::endl;
}
```

### 3. ルーティング情報の取得

```cpp
uint8_t battery, lqi, hops;
helper.ExtractRoutingInfo(header, battery, lqi, hops);
std::cout << "Battery: " << (int)battery << "%" << std::endl;
std::cout << "LQI: " << (int)lqi << std::endl;
std::cout << "Hops: " << (int)hops << std::endl;
```

### 4. ルーティング情報の更新（中継ノード用）

```cpp
// 中継ノードでパケットを転送する際に、ルーティング情報を更新
Ptr<Packet> updatedPacket = helper.UpdateRoutingInfo(
    originalPacket,
    Mac16Address("00:02"),  // 新しい送信元アドレス（中継ノード）
    80,                      // 新しいバッテリー残量
    180,                     // 新しいLQI
    2                        // 更新されたホップ数
);
```

### 5. シーケンス番号管理

```cpp
NishiokaHelper helper;

// シーケンス番号は自動的にインクリメントされる
Ptr<Packet> p1 = helper.CreatePacket("Data1", ...);  // SeqNum: 0
Ptr<Packet> p2 = helper.CreatePacket("Data2", ...);  // SeqNum: 1
Ptr<Packet> p3 = helper.CreatePacket("Data3", ...);  // SeqNum: 2

// シーケンス番号をリセット
helper.ResetSeqNum();
```

## 使用例

詳細な使用例は `examples/nishioka-helper-example.cc` を参照してください。

### 実行方法

```bash
cd /home/yuugo/ns-3-dev
./ns3 run src/nishioka/examples/nishioka-helper-example
```

## multihop2.ccでの使用例

NishiokaHelperを使用することで、multihop2.ccのコードを以下のように簡略化できます：

### 変更前（現在の実装）
```cpp
static Ptr<Packet>
CreatePacketWithRoutingInfo(const RoutingEntry& entry, const std::string& data, Mac16Address srcAddr)
{
    Ptr<Packet> payload = Create<Packet>((const uint8_t*)data.c_str(), data.size());
    NishiokaHeader hdr;
    hdr.SetFrameType(NishiokaHeader::CUSTOM_DATA);
    hdr.SetSeqNum(entry.entryId & 0xFF);
    hdr.SetDstAddrFields(0xCAFE, entry.dst);
    hdr.SetSrcAddrFields(0xCAFE, srcAddr);
    hdr.SetBattery(entry.energy);
    uint16_t evaluation = (static_cast<uint16_t>(entry.lqi) << 8) | entry.hops;
    hdr.SetEvaluation(evaluation);
    payload->AddHeader(hdr);
    return payload;
}
```

### 変更後（NishiokaHelper使用）
```cpp
static Ptr<Packet>
CreatePacketWithRoutingInfo(const RoutingEntry& entry, const std::string& data, Mac16Address srcAddr)
{
    NishiokaHelper helper;
    return helper.CreatePacket(
        data, srcAddr, entry.dst, 0xCAFE,
        entry.energy, entry.lqi, entry.hops, entry.entryId & 0xFF
    );
}
```

## 利点

1. **コードの可読性向上**: 複雑なヘッダ設定処理が1行に集約
2. **保守性の向上**: ヘッダ処理のロジックが1箇所に集約
3. **再利用性**: 他のプログラムでも同じヘルパーを使用可能
4. **エラー処理の統一**: ヘッダ抽出時のエラーハンドリングが統一される
5. **デバッグの容易さ**: NS_LOGを使用したデバッグ情報の提供

## 設計思想

このhelperは、zigbee-helperと同様の設計思想に基づいています：

- **簡便性**: 複雑な操作を簡単なメソッド呼び出しに変換
- **一貫性**: 同じ操作は常に同じ方法で実行
- **拡張性**: 将来的な機能追加に対応可能な設計
- **統合性**: ns-3の他のhelperクラスと同様のインターフェース

