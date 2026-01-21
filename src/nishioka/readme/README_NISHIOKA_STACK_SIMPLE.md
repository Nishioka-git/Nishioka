# NishiokaStack Simple Example

## 概要

このプログラムは、`NishiokaStack`と`NishiokaStackContainer`の動作を理解するための簡単なパケット送信の例です。

**重要**: このプログラムは、**NishiokaStackとNishiokaStackContainerのみ**を使用します。**NishiokaNwkは使用しません**。

## 前提条件

このプログラムを実行するには、以下のファイルが必要です：

- `src/nishioka/model/nishioka-stack.h`
- `src/nishioka/model/nishioka-stack.cc`
- `src/nishioka/helper/nishioka-stack-container.h`
- `src/nishioka/helper/nishioka-stack-container.cc`

**注意**: このプログラムは、NishiokaStackとNishiokaStackContainerのみを使用します。NishiokaNwkは使用しません。

## プログラムの動作

1. **2つのノードを作成**
   - Node 0: 送信ノード（アドレス: 00:01）
   - Node 1: 受信ノード（アドレス: 00:02）

2. **LrWpanNetDeviceをインストール**
   - 各ノードにLR-WPANデバイスを設定
   - PAN ID: 0xCAFE

3. **NishiokaStackをインストール**
   - 各ノードにNishiokaStackを作成
   - NetDeviceを設定して初期化（MAC層への接続を確立）

4. **パケット送信**
   - 1.0秒後にNode 0からNode 1へパケットを送信
   - NishiokaHelperでNishiokaHeaderを含むパケットを作成
   - NishiokaStackからMAC層を取得して直接送信

5. **パケット受信**
   - Node 1でパケットを受信（MAC層のコールバック）
   - NishiokaHeaderを抽出して情報を表示

## ビルド方法

```bash
cd /home/yuugo/ns-3-dev
./ns3 configure --enable-modules=nishioka
./ns3 build
```

## 実行方法

```bash
./ns3 run src/nishioka/examples/nishioka-stack-simple
```

または

```bash
cd /home/yuugo/ns-3-dev
./ns3 run nishioka-stack-simple
```

## 期待される出力

```
=== NishiokaStack Simple Example ===

Created 2 nodes

Installed mobility models

Created channel

Installed LrWpanNetDevices
  Node 0: Address 00:01
  Node 1: Address 00:02

Installed NishiokaStack on both nodes

Set up data indication callback

Scheduled packet transmission at 1.0 seconds

Starting simulation...

1.0s [TX] Sending packet to 00:02
  Packet sent successfully

1.0s [RX] Node received packet:
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

## プログラムの構造

### 主要な関数

1. **`McpsIndication()`**: パケット受信時のコールバック（MAC層のコールバック）
   - MAC層から呼ばれる
   - NishiokaHeaderを抽出
   - パケット情報を表示

2. **`SendPacket()`**: パケット送信関数
   - NishiokaHelperを使用してパケットを作成
   - NishiokaStackからMAC層を取得（`stack->GetMac()`）
   - MAC層の`McpsDataRequest()`を直接呼び出して送信
   - **NishiokaNwkは使用しません**

### 層間接続の確認

このプログラムでは、以下の層間接続が確認できます：

```
NishiokaStack
  └─ LrWpanMacBase (MAC層)
      ├─ GetMac() → m_mac->McpsDataRequest() (Stack → MAC: 送信)
      └─ SetMcpsDataIndicationCallback() → アプリケーションコールバック (MAC → Stack: 受信)
```

**重要**: 
- NishiokaStackは直接MAC層にアクセスします
- NishiokaNwkは使用しません
- アプリケーションが直接MAC層のコールバックを設定します

### パケット送信の流れ

```
アプリケーション
    ↓
NishiokaHelper::CreatePacket()  // パケット作成（NishiokaHeader追加）
    ↓
NishiokaStack::GetMac()  // MAC層取得
    ↓
LrWpanMacBase::McpsDataRequest()  // 送信
    ↓
物理層 → チャネル → 受信ノード
```

### パケット受信の流れ

```
チャネル
    ↓
物理層 → MAC層
    ↓
McpsDataIndicationCallback  // コールバック（アプリケーションが設定）
    ↓
McpsIndication()関数
    ↓
NishiokaHelper::ExtractHeader()  // ヘッダ抽出
    ↓
アプリケーション処理
```

## トラブルシューティング

### エラー: nishioka-stack.hが見つからない

`nishioka-stack.h`と`nishioka-stack.cc`が実装されていない可能性があります。
これらのファイルを先に実装する必要があります。

### エラー: nishioka-stack-container.hが見つからない

`nishioka-stack-container.h`と`nishioka-stack-container.cc`が実装されていない可能性があります。
これらのファイルを先に実装する必要があります。

**注意**: NishiokaNwkは不要です。このプログラムでは使用しません。

### パケットが受信されない

- チャネルが正しく設定されているか確認
- MACアドレスが正しく設定されているか確認
- モビリティモデルが設定されているか確認
- コールバックが正しく設定されているか確認

## 重要なポイント

### 1. NishiokaStackの役割

- **NetDeviceとMAC層へのアクセスを提供**
- NishiokaNwkなしで動作
- アプリケーションが直接MAC層にアクセス可能

### 2. 初期化の順序

```cpp
1. SetNetDevice()    // NetDeviceを設定
2. AggregateObject() // ノードに集約
3. Initialize()      // 初期化（MAC層への接続を確立）
```

### 3. パケット送信

```cpp
// NishiokaStackからMAC層を取得
Ptr<LrWpanMacBase> mac = stack->GetMac();

// MAC層を通じて直接送信
mac->McpsDataRequest(params, packet);
```

### 4. パケット受信

```cpp
// MAC層のコールバックを設定
mac->SetMcpsDataIndicationCallback(MakeCallback(&McpsIndication));
```

## 次のステップ

このプログラムを理解したら、以下の拡張を試すことができます：

1. **NishiokaStackContainerの使用**: 複数のスタックを管理
2. **複数のノード**: 3つ以上のノードで通信
3. **マルチホップ**: 中継ノードを経由した通信
4. **ルーティング**: ルーティングテーブルを使用した通信
5. **バッテリー管理**: バッテリーレベルの動的更新
