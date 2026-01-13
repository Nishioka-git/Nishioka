# NishiokaStack と NishiokaStackContainer の違いと役割

## 概要

NishiokaStackとNishiokaStackContainerは、異なる役割を持つクラスです。

- **NishiokaStack**: 1つのノードのプロトコルスタックを表す
- **NishiokaStackContainer**: 複数のNishiokaStackを管理するコンテナ

## 1. NishiokaStack の役割

### 1.1 基本的な役割

**NishiokaStackは、1つのノードのプロトコルスタックへのアクセスを提供するラッパークラスです。**

```cpp
class NishiokaStack : public Object
{
  private:
    Ptr<LrWpanMacBase> m_mac;      // MAC層へのポインタ
    Ptr<Node> m_node;              // ノードへのポインタ
    Ptr<NetDevice> m_netDevice;    // NetDeviceへのポインタ
    
  public:
    Ptr<LrWpanMacBase> GetMac();   // MAC層へのアクセス
    Ptr<NetDevice> GetNetDevice(); // NetDeviceへのアクセス
    Ptr<Node> GetNode();           // ノードへのアクセス
    Ptr<Channel> GetChannel();     // チャネルへのアクセス
};
```

### 1.2 保存するデータ

NishiokaStackは、以下のデータを保存します：

- **MAC層へのポインタ** (`m_mac`)
  - LrWpanMacBaseへのアクセスを提供
  - パケット送受信のためのインターフェース

- **ノードへのポインタ** (`m_node`)
  - このスタックが属するノード

- **NetDeviceへのポインタ** (`m_netDevice`)
  - ネットワークデバイスへのアクセス

### 1.3 提供する機能

1. **MAC層へのアクセス**
   ```cpp
   Ptr<LrWpanMacBase> mac = stack->GetMac();
   mac->McpsDataRequest(params, packet);
   ```

2. **NetDeviceへのアクセス**
   ```cpp
   Ptr<NetDevice> device = stack->GetNetDevice();
   ```

3. **ノードへのアクセス**
   ```cpp
   Ptr<Node> node = stack->GetNode();
   uint32_t nodeId = node->GetId();
   ```

4. **チャネルへのアクセス**
   ```cpp
   Ptr<Channel> channel = stack->GetChannel();
   ```

### 1.4 使用例

```cpp
// 1つのノードにスタックをインストール
Ptr<Node> node = CreateObject<Node>();
Ptr<LrWpanNetDevice> device = CreateObject<LrWpanNetDevice>();

// スタックを作成
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
stack->SetNetDevice(device);
stack->Initialize();

// MAC層にアクセスしてパケット送信
Ptr<LrWpanMacBase> mac = stack->GetMac();
McpsDataRequestParams params;
mac->McpsDataRequest(params, packet);
```

### 1.5 対応する実体

- **1つのNishiokaStack = 1つのノード**
- 各ノードには、1つのNishiokaStackがインストールされる

## 2. NishiokaStackContainer の役割

### 2.1 基本的な役割

**NishiokaStackContainerは、複数のNishiokaStackへのポインタを管理するコンテナクラスです。**

```cpp
class NishiokaStackContainer
{
  private:
    std::vector<Ptr<NishiokaStack>> m_stacks;  // スタックへのポインタのベクター
    
  public:
    void Add(Ptr<NishiokaStack> stack);        // スタックを追加
    Ptr<NishiokaStack> Get(uint32_t i);        // スタックを取得
    uint32_t GetN();                           // スタック数を取得
    Iterator Begin();                          // イテレータの開始
    Iterator End();                            // イテレータの終了
};
```

### 2.2 保存するデータ

NishiokaStackContainerは、以下のデータを保存します：

- **NishiokaStackへのポインタのベクター** (`m_stacks`)
  - 複数のスタックオブジェクトへのポインタを保持
  - スタックオブジェクト自体ではなく、ポインタを保持

### 2.3 提供する機能

1. **スタックの追加**
   ```cpp
   NishiokaStackContainer container;
   container.Add(stack1);
   container.Add(stack2);
   ```

2. **スタックの取得**
   ```cpp
   Ptr<NishiokaStack> stack = container.Get(0);
   ```

3. **スタック数の取得**
   ```cpp
   uint32_t n = container.GetN();
   ```

4. **イテレータによる走査**
   ```cpp
   for (NishiokaStackContainer::Iterator i = container.Begin();
        i != container.End(); i++)
   {
       Ptr<NishiokaStack> stack = *i;
       // 処理
   }
   ```

### 2.4 使用例

```cpp
// 複数のノードにスタックをインストール
NodeContainer nodes;
nodes.Create(5);

NetDeviceContainer devices;
// ... デバイスを作成 ...

// 各ノードにスタックをインストール
NishiokaStackContainer stacks;
for (uint32_t i = 0; i < devices.GetN(); i++)
{
    Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
    stack->SetNetDevice(devices.Get(i));
    stack->Initialize();
    stacks.Add(stack);  // コンテナに追加
}

// すべてのスタックにアクセス
for (uint32_t i = 0; i < stacks.GetN(); i++)
{
    Ptr<NishiokaStack> stack = stacks.Get(i);
    Ptr<LrWpanMacBase> mac = stack->GetMac();
    // 処理
}
```

### 2.5 対応する実体

- **1つのNishiokaStackContainer = 複数のノード**
- 複数のノードのスタックを一括管理

## 3. 主な違い

### 3.1 役割の違い

| 項目 | NishiokaStack | NishiokaStackContainer |
|------|--------------|----------------------|
| **役割** | 1つのノードのスタックへのアクセス | 複数のスタックを管理 |
| **対応する実体** | 1つのノード | 複数のノード |
| **保存するもの** | MAC層、NetDevice、Nodeへのポインタ | NishiokaStackへのポインタのベクター |
| **使用場面** | 単一ノードの操作 | 複数ノードの一括操作 |

### 3.2 データ構造の違い

**NishiokaStack:**
```
NishiokaStack
  ├─ m_mac (MAC層へのポインタ)
  ├─ m_node (ノードへのポインタ)
  └─ m_netDevice (NetDeviceへのポインタ)
```

**NishiokaStackContainer:**
```
NishiokaStackContainer
  └─ m_stacks (ベクター)
      ├─ Ptr<NishiokaStack> (スタック0)
      ├─ Ptr<NishiokaStack> (スタック1)
      ├─ Ptr<NishiokaStack> (スタック2)
      └─ ...
```

### 3.3 使用場面の違い

**NishiokaStackを使用する場面:**
- 単一ノードのパケット送受信
- 単一ノードのMAC層へのアクセス
- 単一ノードの設定変更

**NishiokaStackContainerを使用する場面:**
- 複数ノードへの一括操作
- ネットワーク全体の設定
- 複数ノードの統計情報収集

## 4. 実際の使用例

### 4.1 単一ノードの操作（NishiokaStack）

```cpp
// 1つのノードにスタックをインストール
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
stack->SetNetDevice(device);
stack->Initialize();

// このノードからパケット送信
Ptr<LrWpanMacBase> mac = stack->GetMac();
McpsDataRequestParams params;
mac->McpsDataRequest(params, packet);
```

### 4.2 複数ノードの操作（NishiokaStackContainer）

```cpp
// 複数のノードにスタックをインストール
NishiokaStackContainer stacks;
for (uint32_t i = 0; i < devices.GetN(); i++)
{
    Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
    stack->SetNetDevice(devices.Get(i));
    stack->Initialize();
    stacks.Add(stack);
}

// すべてのノードからパケット送信
for (uint32_t i = 0; i < stacks.GetN(); i++)
{
    Ptr<NishiokaStack> stack = stacks.Get(i);
    Ptr<LrWpanMacBase> mac = stack->GetMac();
    McpsDataRequestParams params;
    mac->McpsDataRequest(params, packet);
}
```

### 4.3 組み合わせ使用

```cpp
// コンテナで複数のスタックを管理
NishiokaStackContainer allStacks;
// ... スタックを追加 ...

// 特定のスタックにアクセス
Ptr<NishiokaStack> specificStack = allStacks.Get(0);

// そのスタックからMAC層にアクセス
Ptr<LrWpanMacBase> mac = specificStack->GetMac();
```

## 5. NS-3の他のコンテナとの類似性

NishiokaStackContainerは、NS-3の他のコンテナクラスと同様の設計パターンに従っています：

- **NodeContainer**: 複数のNodeを管理
- **NetDeviceContainer**: 複数のNetDeviceを管理
- **NishiokaStackContainer**: 複数のNishiokaStackを管理

これらはすべて、複数のオブジェクトを一括管理するためのコンテナクラスです。

## 6. まとめ

### 6.1 NishiokaStack

- **役割**: 1つのノードのプロトコルスタックへのアクセスを提供
- **保存するもの**: MAC層、NetDevice、Nodeへのポインタ
- **使用場面**: 単一ノードの操作

### 6.2 NishiokaStackContainer

- **役割**: 複数のNishiokaStackを管理
- **保存するもの**: NishiokaStackへのポインタのベクター
- **使用場面**: 複数ノードの一括操作

### 6.3 関係性

```
NishiokaStackContainer
  └─ m_stacks (ベクター)
      ├─ Ptr<NishiokaStack> (ノード0のスタック)
      │   ├─ m_mac (MAC層)
      │   ├─ m_node (ノード0)
      │   └─ m_netDevice (デバイス0)
      ├─ Ptr<NishiokaStack> (ノード1のスタック)
      │   ├─ m_mac (MAC層)
      │   ├─ m_node (ノード1)
      │   └─ m_netDevice (デバイス1)
      └─ ...
```

### 6.4 重要なポイント

1. **NishiokaStackは1つのノードに対応**
   - 各ノードには1つのNishiokaStackがインストールされる
   - スタックは、そのノードのMAC層、NetDevice、Nodeへのアクセスを提供

2. **NishiokaStackContainerは複数のノードを管理**
   - 複数のNishiokaStackへのポインタを保持
   - 複数ノードの一括操作を可能にする

3. **両者は異なる役割を持つ**
   - NishiokaStack: スタックへのアクセス
   - NishiokaStackContainer: 複数スタックの管理

4. **組み合わせて使用**
   - コンテナで複数のスタックを管理
   - 個別のスタックにアクセスして操作

この理解により、適切なクラスを選択して使用できます。

