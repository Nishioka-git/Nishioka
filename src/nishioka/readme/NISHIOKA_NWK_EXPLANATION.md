# NishiokaNwkモジュールの役割と効果

## 概要

このドキュメントでは、NishiokaNwkモジュールがどのようなことを行うのか、そしてモジュール全体に及ぼす効果について説明します。

## 1. NWK層（Network Layer）の一般的な役割

NWK層は、OSI参照モデルの第3層（ネットワーク層）に相当し、以下の主要な機能を提供します：

### 1.1 ルーティング管理
- **役割**: パケットを送信元から宛先まで適切な経路で転送
- **機能**:
  - ルーティングテーブルの管理
  - 最適経路の選択
  - ルート発見（Route Discovery）
  - ルート維持と更新

### 1.2 ネットワーク管理
- **役割**: ネットワーク全体の状態を管理
- **機能**:
  - ネットワーク形成（Network Formation）
  - デバイスの参加管理（Join/Leave）
  - ネットワークアドレスの割り当て
  - ネットワークトポロジーの管理

### 1.3 パケット転送
- **役割**: アプリケーション層からのデータを適切に転送
- **機能**:
  - パケットの分割と再構成（フラグメンテーション）
  - マルチホップ転送
  - ブロードキャスト/マルチキャスト処理

### 1.4 近隣管理
- **役割**: 近隣デバイスとの関係を管理
- **機能**:
  - 近隣テーブルの管理
  - リンク品質の監視
  - 近隣デバイスの発見と維持

### 1.5 セキュリティ処理
- **役割**: ネットワーク層でのセキュリティ機能
- **機能**:
  - フレームカウンターの管理
  - 暗号化/復号化
  - 認証処理

## 2. 現在のNishiokaNwkの実装

### 2.1 現在の実装内容

現在のNishiokaNwkは、基本的な構造のみを実装しています：

```cpp
class NishiokaNwk : public Object
{
  private:
    Ptr<lrwpan::LrWpanMacBase> m_mac; // MAC層へのポインタ
};
```

**提供している機能:**
- MAC層へのアクセス（`GetMac()`, `SetMac()`）
- 基本的な初期化と破棄処理

### 2.2 実装の意図

現在の実装は、**将来の拡張のための基盤**として設計されています：

1. **層構造の確立**: NWK層を独立したモジュールとして配置
2. **MAC層との接続**: MAC層へのアクセスを提供
3. **拡張性**: 将来的にルーティング機能などを追加可能

## 3. NishiokaNwkがモジュール全体に及ぼす効果

### 3.1 プロトコルスタックの構造化

#### Before (NishiokaNwkなし)
```
アプリケーション層（multihop2.cc）
    ↓
NishiokaStack
    ↓
MAC層（LrWpanMacBase）
    ↓
NetDevice
```

#### After (NishiokaNwkあり)
```
アプリケーション層（multihop2.cc）
    ↓
NishiokaStack
    ↓
NishiokaNwk（NWK層）
    ↓
MAC層（LrWpanMacBase）
    ↓
NetDevice
```

**効果:**
- プロトコルスタックが明確に層分離される
- 各層の責任が明確になる
- 将来の機能追加が容易になる

### 3.2 NishiokaStackとの統合

#### DoInitialize()での処理

```cpp
void NishiokaStack::DoInitialize()
{
    // ...
    
    // Aggregate NWK layer to the node
    if (m_nwk)
    {
        m_node->AggregateObject(m_nwk);
        m_nwk->SetMac(m_mac);
        NS_LOG_INFO("NishiokaNwk aggregated to node " << m_node->GetId());
    }
    
    // ...
}
```

**効果:**
1. **自動的なNWK層の生成**: NishiokaStackが自動的にNishiokaNwkを生成
2. **ノードへの集約**: NWK層がノードに集約され、他のモジュールからアクセス可能
3. **MAC層との接続**: NWK層がMAC層にアクセスできるようになる

### 3.3 アプリケーション層への影響

#### 現在の使用例

```cpp
// NishiokaStackからNWK層にアクセス可能
Ptr<NishiokaStack> stack = g_coordinatorStack;
Ptr<NishiokaNwk> nwk = stack->GetNwk();

// MAC層へのアクセスは従来通り
Ptr<LrWpanMacBase> mac = stack->GetMac();
```

**効果:**
- アプリケーション層は、NWK層とMAC層の両方にアクセス可能
- 将来的にNWK層の機能を利用できる準備が整う

## 4. 将来の拡張可能性

### 4.1 ルーティング機能の追加

将来的に以下のような機能を追加できます：

```cpp
class NishiokaNwk : public Object
{
  public:
    // ルーティングテーブル管理
    void AddRoute(Mac16Address dst, Mac16Address nextHop);
    Mac16Address GetNextHop(Mac16Address dst);
    
    // ルート発見
    void DiscoverRoute(Mac16Address dst);
    
    // パケット転送
    void ForwardPacket(Ptr<Packet> packet, Mac16Address dst);
    
  private:
    std::map<Mac16Address, RoutingEntry> m_routingTable;
    // ...
};
```

**効果:**
- アプリケーション層からルーティング処理を分離
- マルチホップ通信の自動化
- ルーティングアルゴリズムの実装が容易

### 4.2 ネットワーク管理機能の追加

```cpp
class NishiokaNwk : public Object
{
  public:
    // ネットワーク形成
    void FormNetwork(uint16_t panId, uint8_t channel);
    
    // デバイス参加
    void JoinNetwork(Mac16Address coordinator);
    
    // 近隣管理
    void AddNeighbor(Mac16Address addr, uint8_t lqi);
    std::vector<Mac16Address> GetNeighbors();
    
  private:
    std::map<Mac16Address, NeighborEntry> m_neighborTable;
    // ...
};
```

**効果:**
- ネットワーク管理の自動化
- アプリケーション層のコードを簡素化
- 標準的なネットワーク管理機能の提供

### 4.3 パケット処理の自動化

```cpp
class NishiokaNwk : public Object
{
  public:
    // データ送信（NWK層で自動的にルーティング）
    void SendData(Ptr<Packet> packet, Mac16Address dst);
    
    // データ受信コールバック
    void SetDataIndicationCallback(Callback<void, Ptr<Packet>, Mac16Address> callback);
    
  private:
    // MAC層のコールバックをNWK層で処理
    void McpsDataIndication(McpsDataIndicationParams params, Ptr<Packet> p);
    // ...
};
```

**効果:**
- アプリケーション層は宛先アドレスを指定するだけで送信可能
- ルーティング処理が自動化される
- マルチホップ通信が透過的に動作

## 5. 現在の実装での実際の効果

### 5.1 コード構造の改善

#### Before (NishiokaNwkなし)
- アプリケーション層（multihop2.cc）で直接MAC層を操作
- ルーティング処理がアプリケーション層に混在
- 層の責任が不明確

#### After (NishiokaNwkあり)
- プロトコルスタックが明確に層分離
- NWK層が独立したモジュールとして存在
- 将来の機能追加の準備が整う

### 5.2 拡張性の向上

現在の実装により、以下のような拡張が容易になります：

1. **ルーティング機能の追加**: ルーティングテーブルやルート発見機能を追加可能
2. **ネットワーク管理の自動化**: ネットワーク形成やデバイス参加を自動化可能
3. **パケット処理の統一**: すべてのパケット処理をNWK層で統一可能

### 5.3 保守性の向上

- **責任の分離**: 各層の責任が明確になる
- **テスト容易性**: NWK層を独立してテスト可能
- **再利用性**: NWK層の機能を他のアプリケーションでも利用可能

## 6. 現在のmultihop2.ccでの使用状況

### 6.1 現在の実装

現在のmultihop2.ccでは、NWK層は存在しますが、まだ直接的な機能は使用していません：

```cpp
// NishiokaStackからNWK層にアクセス可能
Ptr<NishiokaStack> stack = g_coordinatorStack;
Ptr<NishiokaNwk> nwk = stack->GetNwk(); // アクセス可能だが、まだ機能は未実装

// 現在はMAC層を直接使用
stack->GetMac()->McpsDataRequest(params, packet);
```

### 6.2 将来の移行パス

将来的に、以下のように移行できます：

```cpp
// 将来の実装（例）
Ptr<NishiokaNwk> nwk = stack->GetNwk();

// NWK層経由で送信（ルーティングが自動化）
nwk->SendData(packet, dstAddr);

// アプリケーション層は宛先アドレスを指定するだけ
// ルーティング処理はNWK層が自動的に行う
```

## 7. まとめ

### 7.1 NishiokaNwkの現在の役割

1. **プロトコルスタックの構造化**: NWK層を独立したモジュールとして配置
2. **将来の拡張の基盤**: ルーティング機能などを追加する準備
3. **層間接続の確立**: MAC層へのアクセスを提供

### 7.2 モジュール全体への効果

1. **コード構造の改善**: 層の責任が明確になる
2. **拡張性の向上**: 将来の機能追加が容易
3. **保守性の向上**: 各層を独立して開発・テスト可能

### 7.3 将来の展開

現在の実装は基本的な構造のみですが、以下のような機能を追加することで、より強力なネットワーク層を実現できます：

- **ルーティング管理**: ルーティングテーブルとルート発見
- **ネットワーク管理**: ネットワーク形成とデバイス参加
- **パケット転送**: マルチホップ通信の自動化
- **近隣管理**: 近隣テーブルとリンク品質管理

これらの機能を追加することで、アプリケーション層のコードを大幅に簡素化し、より標準的なネットワーク層の機能を提供できるようになります。

