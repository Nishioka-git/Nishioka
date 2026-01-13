# NishiokaStack と NishiokaStackContainer の違いと役割

## 重要な誤解の解消

**NishiokaStackは、パケットのヘッダ要素を保存するコンテナではありません。**

NishiokaStackとNishiokaStackContainerは、異なる役割を持っています。

## 1. NishiokaStackの役割

### 1.1 実際の役割

**NishiokaStackは、プロトコルスタックへのアクセスを提供するラッパークラスです。**

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
    // ...
};
```

**保存するもの:**
- MAC層へのポインタ（`m_mac`）
- ノードへのポインタ（`m_node`）
- NetDeviceへのポインタ（`m_netDevice`）

**保存しないもの:**
- パケットのヘッダ要素
- ルーティング情報
- パケットデータ

### 1.2 パケットヘッダ要素はどこに保存されるか

パケットのヘッダ要素は、**パケット自体（`Ptr<Packet>`）に保存されます**。

```cpp
// パケット作成時
NishiokaHelper helper;
Ptr<Packet> packet = helper.CreatePacket(...);
// この時点で、NishiokaHeaderがパケットに追加される
// パケットオブジェクト内にヘッダ情報が保存される

// パケット受信時
NishiokaHelper helper;
NishiokaHeader header;
std::string data;
helper.ExtractHeader(packet, header, data);
// パケットからヘッダを抽出
// ヘッダ情報はパケットオブジェクト内に保存されていた
```

**重要:** パケットのヘッダ要素は、`Ptr<Packet>`オブジェクト内に保存されます。NishiokaStackは保存しません。

## 2. NishiokaStackContainerの役割

### 2.1 実際の役割

**NishiokaStackContainerは、複数のNishiokaStackへのポインタを管理するコンテナです。**

```cpp
class NishiokaStackContainer
{
  private:
    std::vector<Ptr<NishiokaStack>> m_stacks;  // スタックへのポインタのベクター
    
  public:
    void Add(Ptr<NishiokaStack> stack);        // スタックを追加
    Ptr<NishiokaStack> Get(uint32_t i);        // スタックを取得
    uint32_t GetN();                           // スタック数を取得
    // ...
};
```

**保存するもの:**
- `Ptr<NishiokaStack>`（NishiokaStackへのポインタ）のベクター

**保存しないもの:**
- パケットのヘッダ要素
- パケットデータ
- ルーティング情報

### 2.2 コンテナの意味

「コンテナ」という言葉は、**複数のオブジェクトを保持するデータ構造**を意味します。

- **NishiokaStackContainer**: 複数のNishiokaStackオブジェクトを保持
- **NodeContainer**: 複数のNodeオブジェクトを保持
- **NetDeviceContainer**: 複数のNetDeviceオブジェクトを保持

**パケットのヘッダ要素を保持するコンテナではありません。**

## 3. パケットヘッダ要素の保存場所

### 3.1 パケットオブジェクト内

パケットのヘッダ要素は、**`Ptr<Packet>`オブジェクト内に保存**されます。

```cpp
// パケット作成
NishiokaHelper helper;
Ptr<Packet> packet = helper.CreatePacket(
    "Hello",           // データ
    Mac16Address("00:01"),  // 送信元
    Mac16Address("00:02"),  // 宛先
    0xCAFE,            // PAN ID
    85,                // バッテリー
    200,               // LQI
    0,                 // ホップ数
    1                  // シーケンス番号
);

// この時点で、NishiokaHeaderがパケットに追加される
// ヘッダ情報は packet オブジェクト内に保存される
```

**内部構造:**
```
Ptr<Packet>
  └─ NishiokaHeader（ヘッダ情報）
      ├─ FrameType
      ├─ SeqNum
      ├─ SrcAddr, DstAddr
      ├─ Battery
      ├─ Evaluation (LQI, Hops)
      └─ ...
  └─ Payload（データ）
```

### 3.2 ヘッダ要素の抽出

```cpp
// パケット受信時
NishiokaHelper helper;
NishiokaHeader header;
std::string data;

// パケットからヘッダを抽出
helper.ExtractHeader(packet, header, data);

// ヘッダ情報を取得
Mac16Address src = header.GetShortSrcAddr();
Mac16Address dst = header.GetShortDstAddr();
uint8_t battery = header.GetBattery();
```

**重要:** ヘッダ情報は、パケットオブジェクトから抽出されます。NishiokaStackやNishiokaStackContainerには保存されません。

## 4. NishiokaStackとNishiokaStackContainerの違い

### 4.1 役割の違い

| 項目 | NishiokaStack | NishiokaStackContainer |
|------|--------------|----------------------|
| **役割** | プロトコルスタックへのアクセス | 複数のスタックを管理 |
| **保存するもの** | MAC層、NetDevice、Nodeへのポインタ | NishiokaStackへのポインタのベクター |
| **パケットヘッダ要素** | 保存しない | 保存しない |
| **対応する実体** | 1つのノード | 複数のノード |

### 4.2 使用例の違い

#### NishiokaStackの使用例

```cpp
// 単一のスタックを使用
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
stack->SetNetDevice(device);
stack->Initialize();

// MAC層にアクセス
Ptr<LrWpanMacBase> mac = stack->GetMac();

// パケット送信（パケットは別途作成）
NishiokaHelper helper;
Ptr<Packet> packet = helper.CreatePacket(...);  // パケット作成（ヘッダ情報はパケット内に保存）
mac->McpsDataRequest(params, packet);           // 送信
```

**重要:** パケットのヘッダ要素は、`packet`オブジェクト内に保存されます。`stack`には保存されません。

#### NishiokaStackContainerの使用例

```cpp
// 複数のスタックを管理
NishiokaStackContainer container;

for (uint32_t i = 0; i < devices.GetN(); i++)
{
    Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
    stack->SetNetDevice(devices.Get(i));
    stack->Initialize();
    container.Add(stack);  // スタックをコンテナに追加
}

// すべてのスタックにアクセス
for (uint32_t i = 0; i < container.GetN(); i++)
{
    Ptr<NishiokaStack> stack = container.Get(i);
    // 処理
}
```

**重要:** コンテナは、スタックオブジェクトへのポインタを保持します。パケットのヘッダ要素は保持しません。

## 5. パケットヘッダ要素の保存場所のまとめ

### 5.1 保存場所

```
パケットのヘッダ要素
    ↓
Ptr<Packet>オブジェクト内
    ├─ NishiokaHeader（ヘッダ情報）
    │   ├─ FrameType
    │   ├─ SeqNum
    │   ├─ SrcAddr, DstAddr
    │   ├─ Battery
    │   └─ Evaluation (LQI, Hops)
    └─ Payload（データ）
```

### 5.2 各クラスの役割

| クラス | 役割 | パケットヘッダ要素との関係 |
|--------|------|------------------------|
| **NishiokaHeader** | パケットヘッダの定義 | ヘッダ情報を保持（パケット内に含まれる） |
| **NishiokaHelper** | パケット操作のヘルパー | ヘッダの作成・抽出を支援 |
| **NishiokaStack** | スタックへのアクセス | パケット送受信のためのMAC層アクセスを提供 |
| **NishiokaStackContainer** | 複数スタックの管理 | スタックオブジェクトを管理 |

### 5.3 データフロー

```
1. パケット作成
   NishiokaHelper::CreatePacket()
       ↓
   Ptr<Packet>（NishiokaHeaderを含む）
       ↓
   パケットオブジェクト内にヘッダ情報が保存される

2. パケット送信
   NishiokaStack::GetMac()
       ↓
   LrWpanMacBase::McpsDataRequest()
       ↓
   パケット（ヘッダ情報を含む）が送信される

3. パケット受信
   パケット（ヘッダ情報を含む）を受信
       ↓
   NishiokaHelper::ExtractHeader()
       ↓
   パケットからヘッダ情報を抽出
```

**重要:** ヘッダ情報は、常にパケットオブジェクト内に保存されます。NishiokaStackやNishiokaStackContainerには保存されません。

## 6. よくある誤解

### 誤解1: NishiokaStackがヘッダ要素を保存する

**誤解:**
```
NishiokaStackがパケットのヘッダ要素（バッテリー、LQI、ホップ数など）を保存する
```

**正解:**
```
NishiokaStackは、MAC層、NetDevice、Nodeへのポインタを保存する。
パケットのヘッダ要素は、パケットオブジェクト（Ptr<Packet>）内に保存される。
```

### 誤解2: NishiokaStackContainerがヘッダ要素を保存する

**誤解:**
```
NishiokaStackContainerがパケットのヘッダ要素を保存するコンテナ
```

**正解:**
```
NishiokaStackContainerは、複数のNishiokaStackへのポインタを保存するコンテナ。
パケットのヘッダ要素は保存しない。
```

### 誤解3: コンテナ = データを保存するもの

**誤解:**
```
「コンテナ」= データ（ヘッダ要素など）を保存するもの
```

**正解:**
```
「コンテナ」= 複数のオブジェクトへのポインタを保持するデータ構造
- NishiokaStackContainer: 複数のNishiokaStackを保持
- NodeContainer: 複数のNodeを保持
- NetDeviceContainer: 複数のNetDeviceを保持
```

## 7. 正しい理解

### 7.1 NishiokaStackの役割

**NishiokaStackは、プロトコルスタックへのアクセスを提供するラッパークラスです。**

- **提供するもの:**
  - MAC層へのアクセス（`GetMac()`）
  - NetDeviceへのアクセス（`GetNetDevice()`）
  - ノードへのアクセス（`GetNode()`）
  - チャネルへのアクセス（`GetChannel()`）

- **保存するもの:**
  - MAC層、NetDevice、Nodeへのポインタ

- **保存しないもの:**
  - パケットのヘッダ要素
  - パケットデータ
  - ルーティング情報

### 7.2 NishiokaStackContainerの役割

**NishiokaStackContainerは、複数のNishiokaStackを管理するコンテナです。**

- **提供するもの:**
  - 複数のスタックへの統一的なアクセス
  - スタックの追加・取得・走査

- **保存するもの:**
  - `Ptr<NishiokaStack>`のベクター

- **保存しないもの:**
  - パケットのヘッダ要素
  - パケットデータ
  - ルーティング情報

### 7.3 パケットヘッダ要素の保存場所

**パケットのヘッダ要素は、`Ptr<Packet>`オブジェクト内に保存されます。**

```cpp
// パケット作成時
Ptr<Packet> packet = helper.CreatePacket(...);
// この時点で、NishiokaHeaderがパケットに追加される
// ヘッダ情報は packet オブジェクト内に保存される

// パケット送信
mac->McpsDataRequest(params, packet);
// パケット（ヘッダ情報を含む）が送信される

// パケット受信
helper.ExtractHeader(packet, header, data);
// パケットからヘッダ情報を抽出
```

## 8. まとめ

### 8.1 各クラスの役割

| クラス | 役割 | 保存するもの |
|--------|------|------------|
| **NishiokaStack** | スタックへのアクセス | MAC層、NetDevice、Nodeへのポインタ |
| **NishiokaStackContainer** | 複数スタックの管理 | NishiokaStackへのポインタのベクター |
| **NishiokaHeader** | パケットヘッダの定義 | ヘッダ情報（パケット内に含まれる） |
| **Ptr<Packet>** | パケットオブジェクト | NishiokaHeaderとペイロードデータ |

### 8.2 重要なポイント

1. **NishiokaStackはコンテナではない**
   - パケットのヘッダ要素を保存しない
   - スタックへのアクセスを提供するラッパークラス

2. **NishiokaStackContainerはスタックのコンテナ**
   - 複数のNishiokaStackを管理
   - パケットのヘッダ要素は保存しない

3. **パケットのヘッダ要素はパケットオブジェクト内に保存される**
   - `Ptr<Packet>`オブジェクト内にNishiokaHeaderが含まれる
   - NishiokaHelperで作成・抽出が可能

4. **各クラスは異なる役割を持つ**
   - NishiokaStack: スタックへのアクセス
   - NishiokaStackContainer: 複数スタックの管理
   - NishiokaHeader: パケットヘッダの定義
   - Ptr<Packet>: パケットデータの保持

この理解により、各クラスの役割が明確になります。

