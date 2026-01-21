# nishioka-stack-simpleの動作と効果

## 概要

このドキュメントでは、`nishioka-stack-simple.cc`がどのようなことを行うのか、そしてモジュール全体に及ぼす効果について説明します。

## 1. nishioka-stack-simpleの目的

`nishioka-stack-simple.cc`は、**NishiokaStackとNishiokaNwkの基本的な動作を確認するためのシンプルなサンプルプログラム**です。

### 1.1 主な目的

1. **NishiokaStackの基本動作確認**: スタックのインストールと初期化
2. **MAC層へのアクセス**: NishiokaStack経由でMAC層にアクセス
3. **パケット送受信**: NishiokaHeaderを含むパケットの送受信
4. **NWK層のルーティング機能**: ルーティングテーブルの設定と確認

## 2. プログラムの動作フロー

### 2.1 全体の流れ

```
1. ノード作成（2ノード）
   ↓
2. モビリティモデル設定
   ↓
3. チャネル作成と設定
   ↓
4. LrWpanNetDeviceのインストール
   ↓
5. MACアドレスとPAN IDの設定
   ↓
6. NishiokaStackのインストール
   ↓
7. NWK層でのルーティング設定
   ↓
8. 受信コールバック設定
   ↓
9. パケット送信のスケジューリング
   ↓
10. シミュレーション実行
```

### 2.2 詳細な動作

#### ステップ1: ノードとネットワークデバイスの作成

```cpp
// 2つのノードを作成
NodeContainer nodes;
nodes.Create(2);

// LrWpanNetDeviceをインストール
LrWpanHelper lrWpanHelper;
NetDeviceContainer devices = lrWpanHelper.Install(nodes);
```

**効果:**
- 2つのノードが作成される
- 各ノードにLrWpanNetDeviceがインストールされる

#### ステップ2: アドレス設定

```cpp
dev0->GetMac()->SetShortAddress(Mac16Address("00:01"));
dev1->GetMac()->SetShortAddress(Mac16Address("00:02"));
dev0->GetMac()->SetPanId(0xCAFE);
dev1->GetMac()->SetPanId(0xCAFE);
```

**効果:**
- Node 0: MACアドレス `00:01`
- Node 1: MACアドレス `00:02`
- 両方のノードが同じPAN ID (`0xCAFE`) に属する

#### ステップ3: NishiokaStackのインストール

```cpp
Ptr<NishiokaStack> stack0 = CreateObject<NishiokaStack>();
Ptr<NishiokaStack> stack1 = CreateObject<NishiokaStack>();

stack0->SetNetDevice(dev0);
stack1->SetNetDevice(dev1);

nodes.Get(0)->AggregateObject(stack0);
nodes.Get(1)->AggregateObject(stack1);

stack0->Initialize();
stack1->Initialize();
```

**効果:**
- 各ノードにNishiokaStackがインストールされる
- NishiokaStackが自動的にNishiokaNwkを生成
- MAC層への接続が確立される

#### ステップ4: NWK層でのルーティング設定

```cpp
Ptr<NishiokaNwk> nwk0 = stack0->GetNwk();

// ルートを設定（宛先: 00:02, 次ホップ: 00:02）
nwk0->SetRoute(Mac16Address("00:02"), Mac16Address("00:02"));
```

**効果:**
- ルーティングテーブルにエントリが追加される
- Node 0からNode 1への直接ルートが設定される

#### ステップ5: 受信コールバック設定

```cpp
Ptr<LrWpanMacBase> mac1 = stack1->GetMac();
mac1->SetMcpsDataIndicationCallback(MakeCallback(&McpsIndication));
```

**効果:**
- Node 1でパケット受信時に`McpsIndication()`が呼び出される
- NishiokaHeaderが抽出され、情報が表示される

#### ステップ6: パケット送信

```cpp
Simulator::ScheduleWithContext(nodes.Get(0)->GetId(),
                               Seconds(1.0),
                               &SendPacket,
                               stack0,
                               Mac16Address("00:02"));
```

**SendPacket()関数の動作:**
```cpp
static void SendPacket(Ptr<NishiokaStack> stack, Mac16Address dstAddr)
{
    // 1. NishiokaHelperでパケットを作成
    NishiokaHelper helper;
    Ptr<Packet> packet = helper.CreatePacket(
        "Hello from NishiokaStack!",  // データ
        Mac16Address("00:01"),        // 送信元
        dstAddr,                       // 宛先
        0xCAFE,                       // PAN ID
        85,                           // バッテリー残量
        200,                          // LQI
        0,                            // ホップ数
        1                             // シーケンス番号
    );

    // 2. NishiokaStack経由でMAC層を取得
    Ptr<LrWpanMacBase> mac = stack->GetMac();

    // 3. MAC層でパケット送信
    McpsDataRequestParams params;
    params.m_dstPanId = 0xCAFE;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_dstAddr = dstAddr;
    mac->McpsDataRequest(params, packet);
}
```

**効果:**
- NishiokaHeaderを含むパケットが作成される
- MAC層経由でパケットが送信される
- 物理層を通じてNode 1に到達する

#### ステップ7: パケット受信

```cpp
static void McpsIndication(const McpsDataIndicationParams params, Ptr<Packet> p)
{
    // 1. パケット情報を表示
    std::cout << "Source: " << params.m_srcAddr << "\n";
    std::cout << "Destination: " << params.m_dstAddr << "\n";

    // 2. NishiokaHeaderを抽出
    NishiokaHelper helper;
    NishiokaHeader header;
    std::string data;
    helper.ExtractHeader(p, header, data);

    // 3. ルーティング情報を抽出
    uint8_t battery, lqi, hops;
    helper.ExtractRoutingInfo(header, battery, lqi, hops);

    // 4. 情報を表示
    std::cout << "Battery: " << (int)battery << "%\n";
    std::cout << "LQI: " << (int)lqi << "\n";
    std::cout << "Hops: " << (int)hops << "\n";
    std::cout << "Payload: \"" << data << "\"\n";
}
```

**効果:**
- パケットが受信される
- NishiokaHeaderから情報が抽出される
- ルーティング情報（バッテリー、LQI、ホップ数）が表示される

## 3. モジュール全体に及ぼす効果

### 3.1 プロトコルスタックの動作確認

#### 層間接続の確認

```
アプリケーション層（nishioka-stack-simple.cc）
    ↓ SendPacket()
NishiokaStack
    ↓ GetMac()
MAC層（LrWpanMacBase）
    ↓ McpsDataRequest()
NetDevice（LrWpanNetDevice）
    ↓ 物理層
チャネル
    ↓ 伝送
Node 1
    ↓ 受信
MAC層
    ↓ McpsDataIndicationCallback
アプリケーション層（McpsIndication）
```

**効果:**
- プロトコルスタック全体が正しく動作することを確認
- 層間の接続が適切に確立されていることを確認

### 3.2 NishiokaHelperの動作確認

#### パケット作成と抽出

```cpp
// パケット作成
NishiokaHelper helper;
Ptr<Packet> packet = helper.CreatePacket(
    "Hello from NishiokaStack!",
    Mac16Address("00:01"),
    Mac16Address("00:02"),
    0xCAFE, 85, 200, 0, 1
);

// パケット抽出
NishiokaHeader header;
std::string data;
helper.ExtractHeader(packet, header, data);
```

**効果:**
- NishiokaHelperが正しく動作することを確認
- NishiokaHeaderの作成と抽出が正しく行われることを確認

### 3.3 NishiokaNwkのルーティング機能確認

#### ルーティングテーブルの操作

```cpp
Ptr<NishiokaNwk> nwk = stack->GetNwk();

// ルート設定
nwk->SetRoute(Mac16Address("00:02"), Mac16Address("00:02"));

// ルート確認
uint32_t count = nwk->GetRouteCount();
```

**効果:**
- NWK層のルーティング機能が正しく動作することを確認
- ルーティングテーブルが正しく管理されることを確認

### 3.4 開発・デバッグの支援

#### 動作確認の容易さ

1. **シンプルな構成**: 2ノードのみのシンプルな構成
2. **明確な動作**: 送信→受信の流れが明確
3. **詳細なログ**: 各ステップで詳細な情報を出力

**効果:**
- 新機能の動作確認が容易
- バグの特定が容易
- モジュールの理解が深まる

### 3.5 教育・学習ツールとしての効果

#### モジュールの理解促進

1. **実装例の提供**: 実際の使用方法を示す
2. **ベストプラクティス**: 推奨される使用方法を示す
3. **動作の可視化**: ログ出力で動作を可視化

**効果:**
- 新しい開発者がモジュールを理解しやすい
- 使用方法が明確になる
- トラブルシューティングの参考になる

## 4. 実際の出力例

### 4.1 実行時の出力

```
=== NishiokaStack Simple Example ===

Created 2 nodes

Installed mobility models

Created channel

Installed LrWpanNetDevices
  Node 0: Address 00:01
  Node 1: Address 00:02

Installed NishiokaStack on both nodes

Set route in NWK layer: Node 0 -> Node 1 (direct)
Routing table size: 1 routes

Set up data indication callback

Scheduled packet transmission at 1.0 seconds

Starting simulation...

1s [TX] Sending packet to 00:02
  Packet sent successfully

1.0001s [RX] Node received packet:
  Source: 00:01
  Destination: 00:02
  Battery: 85%
  LQI: 200
  Hops: 0
  Payload: "Hello from NishiokaStack!"

=== Simulation Results ===
Packets sent: 1
Packets received: 1
Success rate: 100%
```

### 4.2 出力の意味

- **TX**: パケット送信
- **RX**: パケット受信
- **Battery**: バッテリー残量（85%）
- **LQI**: リンク品質指標（200）
- **Hops**: ホップ数（0 = 直接通信）
- **Payload**: パケットのペイロードデータ

## 5. 拡張可能性

### 5.1 マルチホップ通信の確認

現在の実装は直接通信ですが、以下のように拡張できます：

```cpp
// 3ノード構成でマルチホップ通信
// Node 0 -> Node 1 -> Node 2

// Node 0からNode 2へのルート（Node 1経由）
nwk0->SetRoute(Mac16Address("00:03"), Mac16Address("00:02"));

// Node 1からNode 2へのルート（直接）
nwk1->SetRoute(Mac16Address("00:03"), Mac16Address("00:03"));
```

### 5.2 ルーティングアルゴリズムのテスト

```cpp
// 複数のルートを設定して最適経路を選択
nwk->SetRoute(Mac16Address("00:03"), Mac16Address("00:02")); // 経路1
nwk->SetRoute(Mac16Address("00:03"), Mac16Address("00:04")); // 経路2（上書き）

// 最適経路を取得
Mac16Address nextHop;
if (nwk->GetNextHop(Mac16Address("00:03"), nextHop)) {
    // nextHop = 00:04（最新の設定）
}
```

## 6. まとめ

### 6.1 nishioka-stack-simpleの役割

1. **動作確認**: NishiokaStackとNishiokaNwkの基本動作を確認
2. **統合テスト**: プロトコルスタック全体の統合をテスト
3. **使用例の提供**: 実際の使用方法を示す
4. **デバッグ支援**: 問題の特定と解決を支援

### 6.2 モジュール全体への効果

1. **品質保証**: モジュールが正しく動作することを保証
2. **開発効率**: 新機能の動作確認を容易にする
3. **理解促進**: モジュールの使用方法を明確にする
4. **教育効果**: 新しい開発者の学習を支援

### 6.3 重要なポイント

- **シンプルな構成**: 2ノードのみで動作確認が可能
- **明確な動作**: 送信→受信の流れが明確
- **詳細なログ**: 各ステップで詳細な情報を出力
- **拡張可能**: マルチホップ通信などに拡張可能

このサンプルプログラムにより、NishiokaStackとNishiokaNwkの動作を確認し、モジュール全体の品質を保証できます。
