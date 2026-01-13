# nishioka-stack-simple 実行結果の分析

## 実行結果

```
=== NishiokaStack Simple Example ===

Created 2 nodes

Installed mobility models

Created channel

Installed LrWpanNetDevices
  Node 0: Address 00:01
  Node 1: Address 00:02

NishiokaStack initialized: Node=0 NetDevice=0x5555b96440d0
NishiokaStack initialized: Node=1 NetDevice=0x5555b9643ce0
Installed NishiokaStack on both nodes

Set up data indication callback

Scheduled packet transmission at 1.0 seconds

Starting simulation...

+1s [TX] Sending packet to 00:02
  Packet sent successfully

+1.00371s [RX] Node received packet:
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

## 実行結果の分析

### 1. 初期化フェーズ

#### ノードとデバイスの作成
```
Created 2 nodes
Installed mobility models
Created channel
Installed LrWpanNetDevices
  Node 0: Address 00:01
  Node 1: Address 00:02
```

**確認できること:**
- 2つのノードが正常に作成された
- モビリティモデルが設定された（位置情報）
- チャネルが作成された
- LrWpanNetDeviceがインストールされた
- MACアドレスが正しく設定された（00:01, 00:02）

#### NishiokaStackの初期化
```
NishiokaStack initialized: Node=0 NetDevice=0x5555b96440d0
NishiokaStack initialized: Node=1 NetDevice=0x5555b9643ce0
Installed NishiokaStack on both nodes
```

**確認できること:**
- NishiokaStackが正常に初期化された
- NetDeviceが正しく設定された（アドレスが表示されている）
- 両方のノードにスタックがインストールされた
- `DoInitialize()`が正常に実行された

**重要:**
- `NishiokaStack initialized`のログは、`DoInitialize()`内で出力されている
- これは、MAC層への接続が確立されたことを示している

### 2. パケット送信フェーズ

```
+1s [TX] Sending packet to 00:02
  Packet sent successfully
```

**確認できること:**
- 1.0秒にパケット送信が実行された
- 宛先アドレス（00:02）が正しく設定された
- パケット送信が成功した

**動作の流れ:**
1. `SendPacket()`関数が実行された
2. NishiokaHelperでパケットが作成された（NishiokaHeaderを含む）
3. NishiokaStackからMAC層を取得（`stack->GetMac()`）
4. MAC層の`McpsDataRequest()`が呼ばれた
5. パケットが物理層に渡された

### 3. パケット受信フェーズ

```
+1.00371s [RX] Node received packet:
  Source: 00:01
  Destination: 00:02
  Battery: 85%
  LQI: 200
  Hops: 0
  Payload: "Hello from NishiokaStack!"
```

**確認できること:**
- 1.00371秒にパケットが受信された
- 送信から受信まで約3.71ミリ秒の遅延（物理層の処理時間）
- 送信元アドレス（00:01）が正しく抽出された
- 宛先アドレス（00:02）が正しく抽出された
- NishiokaHeaderが正しく抽出された
- ルーティング情報（バッテリー、LQI、ホップ数）が正しく抽出された
- ペイロードデータが正しく抽出された

**動作の流れ:**
1. 物理層でパケットを受信
2. MAC層で処理
3. `McpsDataIndicationCallback`が呼ばれた
4. `McpsIndication()`関数が実行された
5. NishiokaHelperでNishiokaHeaderが抽出された
6. ルーティング情報が抽出された
7. 情報が表示された

### 4. 統計情報

```
=== Simulation Results ===
Packets sent: 1
Packets received: 1
Success rate: 100%
```

**確認できること:**
- パケット送信が1回成功
- パケット受信が1回成功
- 成功率100%（パケットロスなし）

## この結果から分かる重要なこと

### 1. NishiokaStackが正常に動作している

- **層間接続が確立されている**
  - `NishiokaStack initialized`のログから、`DoInitialize()`が正常に実行されたことが確認できる
  - MAC層への接続が確立されている

- **MAC層へのアクセスが可能**
  - `stack->GetMac()`でMAC層を取得できている
  - `McpsDataRequest()`でパケット送信が成功している

- **コールバックが正常に動作している**
  - MAC層からのコールバック（`McpsDataIndication`）が正常に呼ばれている
  - パケット受信処理が正常に動作している

### 2. NishiokaHeaderが正しく機能している

- **ヘッダの作成が成功**
  - NishiokaHelperでパケット作成時にヘッダが追加された

- **ヘッダの抽出が成功**
  - 受信時にNishiokaHeaderが正しく抽出された
  - ルーティング情報（バッテリー、LQI、ホップ数）が正しく抽出された

- **データの整合性**
  - 送信したデータ（"Hello from NishiokaStack!"）が正しく受信された
  - 送信元・宛先アドレスが正しく保持された

### 3. 層間接続の確認

この結果から、以下の層間接続が正常に動作していることが確認できます：

```
アプリケーション（SendPacket）
    ↓
NishiokaStack::GetMac()  // MAC層取得
    ↓
LrWpanMacBase::McpsDataRequest()  // 送信
    ↓
物理層 → チャネル → 受信ノード
    ↓
LrWpanMacBase（受信）
    ↓
McpsDataIndicationCallback  // コールバック
    ↓
アプリケーション（McpsIndication）
```

### 4. パケット送受信のタイミング

- **送信時刻**: 1.0秒
- **受信時刻**: 1.00371秒
- **遅延**: 約3.71ミリ秒

この遅延は、物理層での処理時間（伝播遅延、MAC層の処理時間など）によるものです。これは正常な動作です。

## 重要なポイント

### 1. NishiokaStackの役割が確認できた

- NetDeviceとMAC層へのアクセスを提供
- 層間接続を自動的に確立
- 統一的なインターフェースを提供

### 2. NishiokaHelperの役割が確認できた

- パケット作成時にNishiokaHeaderを追加
- 受信時にNishiokaHeaderを抽出
- ルーティング情報の抽出

### 3. 層間接続が正常に動作している

- 送信: アプリケーション → NishiokaStack → MAC層 → 物理層
- 受信: 物理層 → MAC層 → コールバック → アプリケーション

### 4. パケットの整合性が保たれている

- 送信したデータが正しく受信された
- ヘッダ情報が正しく保持された
- ルーティング情報が正しく抽出された

## まとめ

この実行結果から、以下が確認できました：

1. **NishiokaStackが正常に動作している**
   - 層間接続が確立されている
   - MAC層へのアクセスが可能

2. **NishiokaHeaderが正しく機能している**
   - ヘッダの作成と抽出が成功
   - ルーティング情報が正しく保持されている

3. **パケット送受信が正常に動作している**
   - 送信が成功
   - 受信が成功
   - データの整合性が保たれている

4. **層間接続が正常に動作している**
   - 送信フローが正常
   - 受信フローが正常

この結果により、NishiokaStackとNishiokaStackContainerの基本的な動作が確認できました。

