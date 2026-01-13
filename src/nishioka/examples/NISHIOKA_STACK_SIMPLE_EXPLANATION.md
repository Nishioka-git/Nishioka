# nishioka-stack-simple.cc の動作説明

## 概要

`nishioka-stack-simple.cc`は、NishiokaStackとNishiokaStackContainerの基本的な動作を確認するための簡単なパケット送信プログラムです。2つのノード間でNishiokaHeaderを含むパケットを送受信し、スタックの動作を確認します。

## プログラムの全体構造

```
1. 初期化
   ├─ ログ設定
   ├─ ノード作成（2つ）
   ├─ モビリティモデル設定
   ├─ チャネル作成
   ├─ LrWpanNetDeviceインストール
   ├─ NishiokaStackインストール
   └─ コールバック設定

2. パケット送信スケジュール
   └─ 1.0秒後に送信

3. シミュレーション実行
   └─ 5.0秒まで実行

4. 結果表示
   └─ 送受信統計
```

## 詳細な動作説明

### 1. 初期化フェーズ

#### 1.1 ログ設定

```cpp
LogComponentEnable("NishiokaStack", LOG_LEVEL_INFO);
LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO);
```

**動作:**
- NishiokaStackとLrWpanMacのログを有効化
- デバッグ情報が出力される

#### 1.2 ノード作成

```cpp
NodeContainer nodes;
nodes.Create(2);
```

**動作:**
- 2つのノードを作成
- Node 0: 送信ノード
- Node 1: 受信ノード

#### 1.3 モビリティモデル設定

```cpp
MobilityHelper mobility;
mobility.SetPositionAllocator("ns3::GridPositionAllocator", ...);
mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
mobility.Install(nodes);
```

**動作:**
- グリッド配置でノードの位置を設定
- Node 0: (0, 0)
- Node 1: (10, 0)
- 固定位置モデルを使用（移動なし）

**重要:** NS-3では、ノードに位置情報が必要です。位置がないと、物理層での通信ができません。

#### 1.4 チャネル作成

```cpp
Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
Ptr<LogDistancePropagationLossModel> propModel = CreateObject<LogDistancePropagationLossModel>();
propModel->SetPathLossExponent(2.0);
channel->AddPropagationLossModel(propModel);
Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
channel->SetPropagationDelayModel(delayModel);
```

**動作:**
- スペクトラムチャネルを作成
- 伝播損失モデルを設定（距離に基づく損失）
- 伝播遅延モデルを設定（一定速度）

**重要:** チャネルは物理層での通信を可能にします。

#### 1.5 LrWpanNetDeviceのインストール

```cpp
LrWpanHelper lrWpanHelper;
NetDeviceContainer devices = lrWpanHelper.Install(nodes);

// アドレスとPAN IDを設定
dev0->GetMac()->SetShortAddress(Mac16Address("00:01"));
dev1->GetMac()->SetShortAddress(Mac16Address("00:02"));
dev0->GetMac()->SetPanId(0xCAFE);
dev1->GetMac()->SetPanId(0xCAFE);

// チャネルを設定
for (uint32_t i = 0; i < devices.GetN(); i++)
{
    Ptr<LrWpanNetDevice> dev = devices.Get(i)->GetObject<LrWpanNetDevice>();
    dev->SetChannel(channel);
}
```

**動作:**
- 各ノードにLrWpanNetDeviceをインストール
- MACアドレスを設定（00:01, 00:02）
- PAN IDを設定（0xCAFE）
- チャネルを各デバイスに設定

**重要:** 
- MACアドレスは通信の識別に必要
- PAN IDは同じネットワークに属するデバイスで同じ値にする必要がある
- チャネルを設定しないと通信できない

#### 1.6 NishiokaStackのインストール

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

**動作:**
1. NishiokaStackオブジェクトを作成
2. NetDeviceを設定（`SetNetDevice()`）
   - これにより、スタックがNetDeviceと関連付けられる
3. ノードにスタックを集約（`AggregateObject()`）
   - ノードからスタックにアクセス可能になる
4. スタックを初期化（`Initialize()`）
   - `DoInitialize()`が呼ばれる
   - NetDeviceからMAC層を取得
   - 層間接続が確立される

**重要:**
- `SetNetDevice()`は`Initialize()`の前に呼ぶ必要がある
- `Initialize()`でMAC層へのアクセスが可能になる
- `AggregateObject()`でノードとスタックが関連付けられる

#### 1.7 コールバック設定

```cpp
Ptr<LrWpanMacBase> mac1 = stack1->GetMac();
mac1->SetMcpsDataIndicationCallback(MakeCallback(&McpsIndication));
```

**動作:**
- 受信ノード（Node 1）のMAC層にコールバックを設定
- パケット受信時に`McpsIndication()`が呼ばれる

**重要:** コールバックを設定しないと、受信したパケットを処理できません。

### 2. パケット送信フェーズ

#### 2.1 送信スケジュール

```cpp
Simulator::ScheduleWithContext(nodes.Get(0)->GetId(),
                               Seconds(1.0),
                               &SendPacket,
                               stack0,
                               Mac16Address("00:02"));
```

**動作:**
- 1.0秒後に`SendPacket()`を実行
- Node 0のコンテキストで実行
- 宛先アドレスは00:02（Node 1）

#### 2.2 SendPacket()関数の動作

```cpp
static void
SendPacket(Ptr<NishiokaStack> stack, Mac16Address dstAddr)
{
    // 1. NishiokaHelperでパケットを作成
    NishiokaHelper helper;
    Ptr<Packet> packet = helper.CreatePacket(
        "Hello from NishiokaStack!",  // データ
        Mac16Address("00:01"),        // 送信元アドレス
        dstAddr,                       // 宛先アドレス
        0xCAFE,                       // PAN ID
        85,                           // バッテリー残量（85%）
        200,                          // LQI
        0,                            // ホップ数
        1                             // シーケンス番号
    );

    // 2. MAC層を取得
    Ptr<LrWpanMacBase> mac = stack->GetMac();

    // 3. MCPS-DATA.requestパラメータを設定
    McpsDataRequestParams params;
    params.m_dstPanId = 0xCAFE;
    params.m_dstAddrMode = SHORT_ADDR;
    params.m_dstAddr = dstAddr;
    params.m_msduHandle = 0;
    params.m_txOptions = TX_OPTION_NONE;

    // 4. MAC層を通じてパケットを送信
    mac->McpsDataRequest(params, packet);
}
```

**動作の流れ:**

1. **パケット作成**
   - NishiokaHelperを使用してNishiokaHeaderを含むパケットを作成
   - データ、アドレス、バッテリー、LQI、ホップ数などの情報を設定

2. **MAC層へのアクセス**
   - `stack->GetMac()`でMAC層を取得
   - NishiokaStackがNetDeviceから取得したMAC層へのポインタを返す

3. **送信パラメータ設定**
   - 宛先PAN ID、アドレスモード、宛先アドレスなどを設定

4. **パケット送信**
   - `mac->McpsDataRequest()`でMAC層を通じてパケットを送信
   - MAC層が物理層にパケットを渡す

**重要:**
- NishiokaStackを通じてMAC層にアクセスできる
- NishiokaHelperでパケット作成が簡単になる
- MAC層のAPI（`McpsDataRequest()`）を使用して送信

### 3. パケット受信フェーズ

#### 3.1 受信フロー

```
物理層（PHY）
    ↓
MAC層（LrWpanMacBase）
    ↓
McpsDataIndicationCallback
    ↓
McpsIndication()関数
    ↓
NishiokaHeader抽出
    ↓
情報表示
```

#### 3.2 McpsIndication()関数の動作

```cpp
static void
McpsIndication(const McpsDataIndicationParams params, Ptr<Packet> p)
{
    // 1. 受信情報を表示
    std::cout << "Source: " << params.m_srcAddr << "\n";
    std::cout << "Destination: " << params.m_dstAddr << "\n";

    // 2. NishiokaHeaderを抽出
    NishiokaHelper helper;
    NishiokaHeader header;
    std::string data;

    if (helper.ExtractHeader(p, header, data))
    {
        // 3. ルーティング情報を抽出
        uint8_t battery, lqi, hops;
        helper.ExtractRoutingInfo(header, battery, lqi, hops);

        // 4. 情報を表示
        std::cout << "Battery: " << (int)battery << "%\n";
        std::cout << "LQI: " << (int)lqi << "\n";
        std::cout << "Hops: " << (int)hops << "\n";
        std::cout << "Payload: \"" << data << "\"\n";
    }
}
```

**動作:**
1. MAC層から受信パラメータとパケットを受け取る
2. NishiokaHelperを使用してNishiokaHeaderを抽出
3. ルーティング情報（バッテリー、LQI、ホップ数）を抽出
4. 情報を表示

**重要:**
- MAC層のコールバックでパケットを受信
- NishiokaHelperでヘッダ抽出が簡単になる
- ルーティング情報が正しく抽出できる

### 4. シミュレーション実行

```cpp
Simulator::Stop(Seconds(5.0));
Simulator::Run();
Simulator::Destroy();
```

**動作:**
- 5.0秒までシミュレーションを実行
- すべてのイベントを処理
- シミュレーション終了後にリソースを解放

### 5. 結果表示

```cpp
std::cout << "Packets sent: " << g_txCount << std::endl;
std::cout << "Packets received: " << g_rxCount << std::endl;
std::cout << "Success rate: " << (g_rxCount * 100.0 / g_txCount) << "%" << std::endl;
```

**動作:**
- 送信パケット数、受信パケット数、成功率を表示

## 重要なポイント

### 1. NishiokaStackの役割

**NishiokaStackは、NetDeviceとMAC層へのアクセスを提供するラッパークラスです。**

- **NetDeviceへのアクセス**: `SetNetDevice()`でNetDeviceを設定
- **MAC層へのアクセス**: `GetMac()`でMAC層を取得
- **層間接続の確立**: `Initialize()`でMAC層への接続を確立

**使用例:**
```cpp
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
stack->SetNetDevice(device);  // NetDeviceを設定
stack->Initialize();          // 初期化（MAC層への接続を確立）
Ptr<LrWpanMacBase> mac = stack->GetMac();  // MAC層にアクセス
```

### 2. 層間接続の流れ

```
NishiokaStack
    ↓ SetNetDevice()
NetDevice (LrWpanNetDevice)
    ↓ GetObject<LrWpanMacBase>()
MAC層 (LrWpanMacBase)
    ↓ McpsDataRequest()
物理層 (PHY)
    ↓
チャネル
```

**重要:**
- NishiokaStackはNetDeviceを通じてMAC層にアクセス
- `GetObject<>()`でMAC層を取得（NS-3のオブジェクト集約システム）
- MAC層のAPIを使用してパケットを送受信

### 3. パケット送信の流れ

```
アプリケーション
    ↓
NishiokaHelper::CreatePacket()  // パケット作成
    ↓
NishiokaStack::GetMac()  // MAC層取得
    ↓
LrWpanMacBase::McpsDataRequest()  // 送信
    ↓
物理層 → チャネル → 受信ノード
```

**重要:**
- NishiokaHelperでパケット作成が簡単
- NishiokaStackでMAC層にアクセス
- MAC層の標準APIを使用

### 4. パケット受信の流れ

```
チャネル
    ↓
物理層 → MAC層
    ↓
McpsDataIndicationCallback  // コールバック
    ↓
McpsIndication()関数
    ↓
NishiokaHelper::ExtractHeader()  // ヘッダ抽出
    ↓
アプリケーション処理
```

**重要:**
- MAC層のコールバックで受信を処理
- NishiokaHelperでヘッダ抽出が簡単
- コールバックを設定しないと受信できない

### 5. NishiokaStackContainerの使用（将来の拡張）

現在のプログラムでは直接使用していませんが、複数のスタックを管理する場合に使用します：

```cpp
NishiokaStackContainer container;
container.Add(stack0);
container.Add(stack1);

// すべてのスタックにアクセス
for (uint32_t i = 0; i < container.GetN(); i++)
{
    Ptr<NishiokaStack> stack = container.Get(i);
    // 処理
}
```

## 実行時の動作フロー

### タイムライン

```
t=0.0s: シミュレーション開始
    ├─ ノード作成
    ├─ デバイスインストール
    ├─ スタックインストール
    └─ コールバック設定

t=1.0s: パケット送信
    ├─ SendPacket()実行
    ├─ パケット作成（NishiokaHelper）
    ├─ MAC層に送信
    └─ 物理層経由で送信

t=1.0s+α: パケット受信
    ├─ 物理層で受信
    ├─ MAC層で処理
    ├─ McpsIndication()呼び出し
    ├─ NishiokaHeader抽出
    └─ 情報表示

t=5.0s: シミュレーション終了
    └─ 統計表示
```

## 重要な概念

### 1. オブジェクト集約（AggregateObject）

```cpp
nodes.Get(0)->AggregateObject(stack0);
```

**意味:**
- ノードとスタックを関連付け
- `GetObject<NishiokaStack>()`でスタックにアクセス可能に
- NS-3の標準的なパターン

### 2. スマートポインタ（Ptr<>）

```cpp
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
```

**意味:**
- 参照カウント方式のメモリ管理
- 自動的にメモリが解放される
- NS-3の標準的なメモリ管理

### 3. コールバック（Callback）

```cpp
mac1->SetMcpsDataIndicationCallback(MakeCallback(&McpsIndication));
```

**意味:**
- 非同期イベントの処理
- MAC層からアプリケーションへの通知
- NS-3の標準的なイベント処理パターン

### 4. イベントスケジューリング

```cpp
Simulator::ScheduleWithContext(..., Seconds(1.0), &SendPacket, ...);
```

**意味:**
- 将来の時刻にイベントをスケジュール
- シミュレーション時間の管理
- NS-3のコア機能

## まとめ

### プログラムの目的

1. **NishiokaStackの基本動作確認**
   - NetDeviceとの関連付け
   - MAC層へのアクセス
   - 層間接続の確立

2. **パケット送受信の確認**
   - NishiokaHelperを使用したパケット作成
   - MAC層を通じた送信
   - コールバックによる受信処理

3. **NishiokaStackContainerの準備**
   - 複数のスタックを管理する準備
   - 将来の拡張への対応

### 重要なポイント

1. **NishiokaStackはラッパークラス**
   - NetDeviceとMAC層へのアクセスを提供
   - 層間接続を簡潔に

2. **NishiokaHelperでパケット操作が簡単**
   - パケット作成とヘッダ抽出を簡潔に

3. **MAC層のコールバックで受信処理**
   - 非同期イベントの処理
   - 標準的なNS-3パターン

4. **初期化の順序が重要**
   - `SetNetDevice()` → `AggregateObject()` → `Initialize()`
   - この順序で実行する必要がある

5. **位置情報が必要**
   - モビリティモデルを設定しないと通信できない

このプログラムにより、NishiokaStackとNishiokaStackContainerの基本的な動作を理解できます。

