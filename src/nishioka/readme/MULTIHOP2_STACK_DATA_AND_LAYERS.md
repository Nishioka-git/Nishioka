# multihop2でのNishiokaStackの保存情報と層の接続

## 概要

このドキュメントでは、multihop2で使用されているNishiokaStackとNishiokaStackContainerがどのような情報を保存し、どのように層間接続を行っているかを説明します。

## 1. NishiokaStackが保存する情報

### 1.1 保存されるデータ構造

NishiokaStackは、以下の3つのポインタを保存します：

```cpp
class NishiokaStack : public Object
{
  private:
    Ptr<LrWpanMacBase> m_mac;      // MAC層へのポインタ
    Ptr<Node> m_node;              // ノードへのポインタ
    Ptr<NetDevice> m_netDevice;    // NetDeviceへのポインタ
};
```

### 1.2 各メンバ変数の役割

#### `m_mac` (MAC層へのポインタ)
- **役割**: LrWpanMacBaseへのアクセスを提供
- **用途**: 
  - パケット送信（`McpsDataRequest()`）
  - パケット受信コールバック設定（`SetMcpsDataIndicationCallback()`）
  - MLME操作（`MlmeStartRequest()`, `MlmeAssociateRequest()`など）
  - PIB属性設定（`MlmeSetRequest()`）

#### `m_node` (ノードへのポインタ)
- **役割**: このスタックが属するノードへのアクセスを提供
- **用途**: 
  - ノードIDの取得（`node->GetId()`）
  - ノードへのオブジェクトの集約

#### `m_netDevice` (NetDeviceへのポインタ)
- **役割**: ネットワークデバイスへのアクセスを提供
- **用途**: 
  - MAC層の取得（`netDevice->GetObject<LrWpanMacBase>()`）
  - チャネルへのアクセス（`netDevice->GetChannel()`）

### 1.3 multihop2での使用例

```cpp
// Coordinator
g_coordinatorStack = CreateObject<NishiokaStack>();
g_coordinatorStack->SetNetDevice(g_coordinatorDevice);
node->AggregateObject(g_coordinatorStack);
g_coordinatorStack->Initialize();
```

**この時点で保存される情報:**
- `m_netDevice` = `g_coordinatorDevice` (UartLrWpanNetDevice)
- `m_node` = `node` (Node 0)
- `m_mac` = `g_coordinatorDevice->GetObject<LrWpanMacBase>()` (MAC層)

## 2. NishiokaStackContainerが保存する情報

### 2.1 保存されるデータ構造

NishiokaStackContainerは、複数のNishiokaStackへのポインタをベクターで保存します：

```cpp
class NishiokaStackContainer
{
  private:
    std::vector<Ptr<NishiokaStack>> m_stacks;  // スタックへのポインタのベクター
};
```

### 2.2 multihop2での使用例

```cpp
// グローバル変数
static NishiokaStackContainer g_stacks; // すべてのスタックを管理

// 各デバイスにスタックをインストール
g_coordinatorStack = CreateObject<NishiokaStack>();
g_coordinatorStack->SetNetDevice(g_coordinatorDevice);
g_coordinatorStack->Initialize();
g_stacks.Add(g_coordinatorStack);  // コンテナに追加

g_dev01Stack = CreateObject<NishiokaStack>();
g_dev01Stack->SetNetDevice(g_uartNetDevice1);
g_dev01Stack->Initialize();
g_stacks.Add(g_dev01Stack);  // コンテナに追加

g_dev02Stack = CreateObject<NishiokaStack>();
g_dev02Stack->SetNetDevice(g_uartNetDevice2);
g_dev02Stack->Initialize();
g_stacks.Add(g_dev02Stack);  // コンテナに追加
```

**この時点で保存される情報:**
```
g_stacks.m_stacks = [
  Ptr<NishiokaStack> (Coordinator Stack),
  Ptr<NishiokaStack> (Dev01 Stack),
  Ptr<NishiokaStack> (Dev02 Stack)
]
```

### 2.3 コンテナの利点

- **一括管理**: すべてのスタックを1つのコンテナで管理
- **統一的なアクセス**: `g_stacks.Get(i)`で任意のスタックにアクセス可能
- **イテレータ**: `g_stacks.Begin()`, `g_stacks.End()`で走査可能

## 3. 層の接続（Layer Connection）

### 3.1 層構造の全体像

```
アプリケーション層（multihop2.cc）
    ↓
NishiokaStack（プロトコルスタックラッパー）
    ↓
LrWpanMacBase（MAC層）
    ↓
UartLrWpanNetDevice（NetDevice）
    ↓
物理層（チャネル）
```

### 3.2 初期化時の層接続

#### ステップ1: NetDeviceの設定

```cpp
g_coordinatorStack->SetNetDevice(g_coordinatorDevice);
```

**この時点で:**
- `m_netDevice` = `g_coordinatorDevice`
- `m_node` = `g_coordinatorDevice->GetNode()`（自動的に取得）

#### ステップ2: スタックの初期化

```cpp
g_coordinatorStack->Initialize();
```

**`DoInitialize()`内で実行される処理:**

```cpp
void NishiokaStack::DoInitialize()
{
    // NetDeviceを初期化
    m_netDevice->Initialize();
    
    // MAC層を取得
    m_mac = m_netDevice->GetObject<LrWpanMacBase>();
    
    // エラーチェック
    NS_ABORT_MSG_UNLESS(m_mac,
        "No valid LrWpanMacBase found in this NetDevice");
}
```

**この時点で:**
- `m_mac` = MAC層へのポインタが設定される
- 層間接続が確立される

### 3.3 パケット送信時の層接続

#### 送信フロー

```cpp
// アプリケーション層（multihop2.cc）
Ptr<NishiokaStack> stack = g_coordinatorStack;

// NishiokaStack経由でMAC層にアクセス
Ptr<LrWpanMacBase> mac = stack->GetMac();

// MAC層でパケット送信
McpsDataRequestParams params;
mac->McpsDataRequest(params, packet);
```

**層間のデータフロー:**
```
1. アプリケーション層
   └─> Ptr<Packet> packet (NishiokaHeaderを含む)

2. NishiokaStack
   └─> stack->GetMac() でMAC層を取得

3. MAC層（LrWpanMacBase）
   └─> McpsDataRequest() でパケットを処理
       └─> MACヘッダを追加
           └─> NetDeviceに渡す

4. NetDevice（UartLrWpanNetDevice）
   └─> 物理層に送信

5. 物理層（チャネル）
   └─> パケットを伝送
```

### 3.4 パケット受信時の層接続

#### 受信フロー

```cpp
// コールバック設定（アプリケーション層）
g_coordinatorStack->GetMac()->SetMcpsDataIndicationCallback(
    MakeBoundCallback(&DataIndication, g_coordinatorStack));
```

**層間のデータフロー:**
```
1. 物理層（チャネル）
   └─> パケットを受信

2. NetDevice（UartLrWpanNetDevice）
   └─> パケットをMAC層に渡す

3. MAC層（LrWpanMacBase）
   └─> MACヘッダを処理
       └─> McpsDataIndicationCallback を呼び出す

4. NishiokaStack
   └─> コールバックが設定されている

5. アプリケーション層（multihop2.cc）
   └─> DataIndication() が呼び出される
       └─> NishiokaHeaderを抽出
           └─> ルーティング処理
```

### 3.5 コールバック設定による層接続

#### 設定例

```cpp
// データ送信確認コールバック
g_coordinatorStack->GetMac()->SetMcpsDataConfirmCallback(
    MakeBoundCallback(&DataConfirm, g_coordinatorStack));

// データ受信コールバック
g_coordinatorStack->GetMac()->SetMcpsDataIndicationCallback(
    MakeBoundCallback(&DataIndication, g_coordinatorStack));

// アソシエーション指示コールバック
g_coordinatorStack->GetMac()->SetMlmeAssociateIndicationCallback(
    MakeBoundCallback(&AssociateIndication, g_coordinatorDevice));
```

**コールバックチェーン:**
```
MAC層（LrWpanMacBase）
    ↓ (イベント発生)
コールバック関数（MakeBoundCallback）
    ↓ (関数呼び出し)
アプリケーション層（multihop2.cc）
    └─> DataIndication(), DataConfirm(), AssociateIndication() など
```

## 4. データ構造の全体像

### 4.1 メモリ上の配置

```
┌─────────────────────────────────────┐
│  NishiokaStackContainer (g_stacks)  │
│  ┌───────────────────────────────┐  │
│  │ m_stacks (vector)             │  │
│  │ ┌─────────────────────────┐  │  │
│  │ │ Ptr<NishiokaStack> [0]  │──┼──┼─> Coordinator Stack
│  │ │ Ptr<NishiokaStack> [1]  │──┼──┼─> Dev01 Stack
│  │ │ Ptr<NishiokaStack> [2]  │──┼──┼─> Dev02 Stack
│  │ └─────────────────────────┘  │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘

各NishiokaStack:
┌─────────────────────────────────────┐
│  NishiokaStack (Coordinator)         │
│  ┌───────────────────────────────┐  │
│  │ m_mac      → LrWpanMacBase    │  │
│  │ m_node     → Node (0)         │  │
│  │ m_netDevice → UartLrWpanNetDevice│
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
         ↓
┌─────────────────────────────────────┐
│  UartLrWpanNetDevice                 │
│  ┌───────────────────────────────┐  │
│  │ MAC層へのアクセス              │  │
│  │ チャネルへのアクセス            │  │
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

### 4.2 層間接続の詳細

```
┌─────────────────────────────────────────────┐
│  アプリケーション層（multihop2.cc）         │
│  - SendRREQ()                               │
│  - HandleRREQ()                             │
│  - DataIndication()                         │
│  - RelayAndIndicate()                       │
└─────────────────────────────────────────────┘
                    ↑ ↓
                    │ │
                    │ │ GetMac()
                    │ │ SetMcpsDataIndicationCallback()
                    │ │ McpsDataRequest()
                    │ │
┌─────────────────────────────────────────────┐
│  NishiokaStack                               │
│  - m_mac: Ptr<LrWpanMacBase>                │
│  - m_node: Ptr<Node>                        │
│  - m_netDevice: Ptr<NetDevice>              │
│                                              │
│  提供する機能:                               │
│  - GetMac() → MAC層へのアクセス             │
│  - GetNode() → ノードへのアクセス           │
│  - GetNetDevice() → NetDeviceへのアクセス    │
└─────────────────────────────────────────────┘
                    ↑ ↓
                    │ │
                    │ │ GetObject<LrWpanMacBase>()
                    │ │ McpsDataRequest()
                    │ │ McpsDataIndicationCallback()
                    │ │
┌─────────────────────────────────────────────┐
│  LrWpanMacBase (MAC層)                      │
│  - パケット送受信処理                        │
│  - MACヘッダの追加・削除                     │
│  - コールバック管理                          │
└─────────────────────────────────────────────┘
                    ↑ ↓
                    │ │
                    │ │ パケット送受信
                    │ │
┌─────────────────────────────────────────────┐
│  UartLrWpanNetDevice (NetDevice)            │
│  - 物理層とのインターフェース               │
│  - チャネル管理                              │
└─────────────────────────────────────────────┘
                    ↑ ↓
                    │ │
                    │ │ パケット伝送
                    │ │
┌─────────────────────────────────────────────┐
│  物理層（チャネル）                          │
│  - パケットの伝送                            │
└─────────────────────────────────────────────┘
```

## 5. 実際のコードでの使用例

### 5.1 パケット送信

```cpp
// アプリケーション層からMAC層へのアクセス
static void SendRREQ(Ptr<NishiokaStack> stack, Mac16Address dst)
{
    // スタックからMAC層を取得
    Ptr<LrWpanMacBase> mac = stack->GetMac();
    
    // パケットを作成
    Ptr<Packet> packet = CreateRREQPacket(rreq);
    
    // MAC層でパケット送信
    McpsDataRequestParams params;
    mac->McpsDataRequest(params, packet);
}
```

**層間接続:**
```
SendRREQ()
  └─> stack->GetMac()  // NishiokaStack経由でMAC層を取得
      └─> mac->McpsDataRequest()  // MAC層で送信
          └─> NetDevice経由で物理層に送信
```

### 5.2 パケット受信

```cpp
// コールバック設定
g_coordinatorStack->GetMac()->SetMcpsDataIndicationCallback(
    MakeBoundCallback(&DataIndication, g_coordinatorStack));

// 受信コールバック
static void DataIndication(Ptr<NishiokaStack> stack, 
                           McpsDataIndicationParams params, 
                           Ptr<Packet> p)
{
    // スタックからデバイスを取得
    Ptr<UartLrWpanNetDevice> device = GetDeviceFromStack(stack);
    
    // パケット処理
    // ...
}
```

**層間接続:**
```
物理層でパケット受信
  └─> NetDeviceがMAC層に渡す
      └─> MAC層がコールバックを呼び出す
          └─> DataIndication() が実行される
              └─> NishiokaHeaderを抽出
                  └─> ルーティング処理
```

## 6. まとめ

### 6.1 NishiokaStackが保存する情報

| メンバ変数 | 型 | 役割 |
|-----------|-----|------|
| `m_mac` | `Ptr<LrWpanMacBase>` | MAC層へのアクセス |
| `m_node` | `Ptr<Node>` | ノードへのアクセス |
| `m_netDevice` | `Ptr<NetDevice>` | NetDeviceへのアクセス |

### 6.2 NishiokaStackContainerが保存する情報

| メンバ変数 | 型 | 役割 |
|-----------|-----|------|
| `m_stacks` | `std::vector<Ptr<NishiokaStack>>` | 複数のスタックへのポインタ |

### 6.3 層間接続の流れ

**送信:**
```
アプリケーション → NishiokaStack → MAC層 → NetDevice → 物理層
```

**受信:**
```
物理層 → NetDevice → MAC層 → コールバック → アプリケーション
```

### 6.4 重要なポイント

1. **NishiokaStackは情報を保存するコンテナではない**
   - MAC層、Node、NetDeviceへのポインタを保持
   - パケットデータやルーティング情報は保存しない

2. **層間接続は初期化時に確立される**
   - `DoInitialize()`でMAC層への接続を確立
   - コールバック設定で受信フローを確立

3. **統一的なインターフェースを提供**
   - `stack->GetMac()`でMAC層にアクセス
   - すべてのMAC層操作がNishiokaStack経由

4. **NishiokaStackContainerで一括管理**
   - 複数のスタックを1つのコンテナで管理
   - 統一的なアクセスが可能

この構造により、multihop2はNishiokaStackとNishiokaStackContainerを使用して、統一的なインターフェースでMAC層にアクセスし、パケット送受信を行っています。


