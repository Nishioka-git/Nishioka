# NishiokaStack と NishiokaStackContainer の利点と違い

## 概要

このドキュメントでは、NishiokaStackとNishiokaStackContainerを実装することで可能になること、および従来の実装（multihop2.ccなど）との違いについて説明します。

## 1. 実装前（従来の方法）

### 1.1 従来の実装方法（multihop2.ccなど）

従来は、MAC層に直接アクセスしていました：

```cpp
// 従来の方法：直接MAC層にアクセス
Ptr<UartLrWpanNetDevice> device = ...;
Ptr<LrWpanMacBase> mac = device->GetMac();

// パケット送信
McpsDataRequestParams params;
mac->McpsDataRequest(params, packet);

// コールバック設定
mac->SetMcpsDataIndicationCallback(MakeCallback(&SomeFunction));
```

**特徴:**
- NetDeviceから直接MAC層を取得
- 各ノードで個別にMAC層を管理
- スタックの概念がない
- 複数ノードの管理が煩雑

### 1.2 従来の方法の問題点

1. **コードの重複**
   - 各ノードで同じような初期化コードを書く必要がある
   - MAC層へのアクセス方法が分散

2. **管理の煩雑さ**
   - 複数のノードがある場合、各ノードのMAC層を個別に管理
   - スタックの統一的な管理方法がない

3. **拡張性の欠如**
   - 将来、NWK層などを追加する場合、すべてのコードを変更する必要がある
   - 層間接続の管理が困難

4. **再利用性の低さ**
   - 同じようなコードを複数のプログラムで書く必要がある

## 2. 実装後（NishiokaStack使用）

### 2.1 NishiokaStackを使用した実装

```cpp
// 新しい方法：NishiokaStackを使用
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
stack->SetNetDevice(device);
stack->Initialize();  // 層間接続が自動的に確立される

// パケット送信
Ptr<LrWpanMacBase> mac = stack->GetMac();
mac->McpsDataRequest(params, packet);

// コールバック設定
mac->SetMcpsDataIndicationCallback(MakeCallback(&SomeFunction));
```

**特徴:**
- NishiokaStackを通じてMAC層にアクセス
- 層間接続が自動的に確立される
- 統一的なインターフェース

### 2.2 NishiokaStackContainerを使用した実装

```cpp
// 複数のスタックを管理
NishiokaStackContainer container;

for (uint32_t i = 0; i < nodes.GetN(); i++)
{
    Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
    stack->SetNetDevice(devices.Get(i));
    stack->Initialize();
    container.Add(stack);  // コンテナに追加
}

// すべてのスタックにアクセス
for (uint32_t i = 0; i < container.GetN(); i++)
{
    Ptr<NishiokaStack> stack = container.Get(i);
    // 処理
}
```

**特徴:**
- 複数のスタックを統一管理
- 効率的なアクセス
- 将来の拡張に対応

## 3. 可能になること

### 3.1 層間接続の自動化

**従来:**
```cpp
// 手動でMAC層を取得
Ptr<LrWpanMacBase> mac = device->GetMac();
// コールバックを個別に設定
mac->SetMcpsDataIndicationCallback(...);
```

**NishiokaStack使用:**
```cpp
// スタックを初期化するだけで層間接続が確立される
stack->SetNetDevice(device);
stack->Initialize();  // 内部でMAC層への接続が確立される

// 統一的なインターフェースでアクセス
Ptr<LrWpanMacBase> mac = stack->GetMac();
```

**利点:**
- 初期化が簡潔
- 層間接続の管理が自動化
- エラーが発生しにくい

### 3.2 複数スタックの統一管理

**従来:**
```cpp
// 各ノードで個別に管理
Ptr<LrWpanMacBase> mac0 = devices.Get(0)->GetMac();
Ptr<LrWpanMacBase> mac1 = devices.Get(1)->GetMac();
Ptr<LrWpanMacBase> mac2 = devices.Get(2)->GetMac();
// ... 個別に管理
```

**NishiokaStackContainer使用:**
```cpp
// コンテナで統一管理
NishiokaStackContainer container;
// ... スタックを追加 ...

// すべてのスタックに統一的な方法でアクセス
for (uint32_t i = 0; i < container.GetN(); i++)
{
    Ptr<NishiokaStack> stack = container.Get(i);
    Ptr<LrWpanMacBase> mac = stack->GetMac();
    // 処理
}
```

**利点:**
- コードが簡潔
- 管理が容易
- 拡張が容易（ノード数を増やす場合）

### 3.3 将来の拡張性

**将来のNishiokaHelper::Install()の実装:**
```cpp
NishiokaStackContainer
NishiokaHelper::Install(NetDeviceContainer devices)
{
    NishiokaStackContainer container;
    
    for (uint32_t i = 0; i < devices.GetN(); i++)
    {
        Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
        stack->SetNetDevice(devices.Get(i));
        devices.Get(i)->GetNode()->AggregateObject(stack);
        stack->Initialize();
        container.Add(stack);
    }
    
    return container;
}
```

**使用例:**
```cpp
// 簡単に複数のノードにスタックをインストール
NishiokaHelper helper;
NishiokaStackContainer stacks = helper.Install(devices);

// すべてのスタックにアクセス
for (uint32_t i = 0; i < stacks.GetN(); i++)
{
    Ptr<NishiokaStack> stack = stacks.Get(i);
    // 処理
}
```

**利点:**
- 1行で複数のスタックをインストール
- コードが大幅に簡潔化
- 他のNS-3ヘルパーと同じパターン

### 3.4 コードの再利用性

**従来:**
```cpp
// 各プログラムで同じようなコードを書く必要がある
void SetupDevice(Ptr<NetDevice> device)
{
    Ptr<LrWpanMacBase> mac = device->GetMac();
    mac->SetMcpsDataIndicationCallback(...);
    // ... 各プログラムで異なる実装
}
```

**NishiokaStack使用:**
```cpp
// 統一的な方法でスタックを設定
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
stack->SetNetDevice(device);
stack->Initialize();
// すべてのプログラムで同じ方法
```

**利点:**
- コードの再利用性が向上
- 保守性が向上
- バグが発生しにくい

### 3.5 オブジェクト集約によるアクセス

**従来:**
```cpp
// NetDeviceから直接MAC層を取得
Ptr<LrWpanMacBase> mac = device->GetMac();
```

**NishiokaStack使用:**
```cpp
// ノードからスタックにアクセス可能
Ptr<NishiokaStack> stack = node->GetObject<NishiokaStack>();
Ptr<LrWpanMacBase> mac = stack->GetMac();
```

**利点:**
- NS-3の標準パターンに従う
- オブジェクト間の関係が明確
- デバッグが容易

## 4. 今までとは異なる点

### 4.1 アクセス方法の違い

| 項目 | 従来（multihop2.ccなど） | NishiokaStack使用 |
|------|------------------------|------------------|
| **MAC層へのアクセス** | `device->GetMac()` | `stack->GetMac()` |
| **初期化** | 手動でMAC層を取得 | `stack->Initialize()`で自動化 |
| **複数ノード管理** | 個別に管理 | NishiokaStackContainerで統一管理 |
| **層間接続** | 手動で設定 | 自動的に確立 |

### 4.2 コードの比較

#### 従来の方法（multihop2.ccなど）

```cpp
// 各ノードで個別に設定
Ptr<UartLrWpanNetDevice> device0 = ...;
Ptr<UartLrWpanNetDevice> device1 = ...;
Ptr<UartLrWpanNetDevice> device2 = ...;

// MAC層を個別に取得
Ptr<LrWpanMacBase> mac0 = device0->GetMac();
Ptr<LrWpanMacBase> mac1 = device1->GetMac();
Ptr<LrWpanMacBase> mac2 = device2->GetMac();

// コールバックを個別に設定
mac0->SetMcpsDataIndicationCallback(MakeCallback(&Callback0));
mac1->SetMcpsDataIndicationCallback(MakeCallback(&Callback1));
mac2->SetMcpsDataIndicationCallback(MakeCallback(&Callback2));

// パケット送信も個別に
mac0->McpsDataRequest(params0, packet0);
mac1->McpsDataRequest(params1, packet1);
mac2->McpsDataRequest(params2, packet2);
```

#### NishiokaStack使用

```cpp
// スタックを作成してコンテナに追加
NishiokaStackContainer container;

for (uint32_t i = 0; i < devices.GetN(); i++)
{
    Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
    stack->SetNetDevice(devices.Get(i));
    stack->Initialize();
    container.Add(stack);
}

// 統一的な方法でアクセス
for (uint32_t i = 0; i < container.GetN(); i++)
{
    Ptr<NishiokaStack> stack = container.Get(i);
    Ptr<LrWpanMacBase> mac = stack->GetMac();
    mac->SetMcpsDataIndicationCallback(MakeCallback(&Callback));
    mac->McpsDataRequest(params, packet);
}
```

**違い:**
- コードが約50%短縮
- ループで統一処理が可能
- 拡張が容易（ノード数を増やす場合）

### 4.3 将来の拡張性

#### 従来の方法

将来、NWK層などを追加する場合：

```cpp
// すべてのコードを変更する必要がある
// 各ノードで個別にNWK層を設定
Ptr<NishiokaNwk> nwk0 = CreateObject<NishiokaNwk>();
nwk0->SetMac(mac0);
mac0->SetMcpsDataIndicationCallback(MakeCallback(&NishiokaNwk::McpsDataIndication, nwk0));
// ... すべてのノードで同じことを繰り返す
```

#### NishiokaStack使用

```cpp
// NishiokaStackの実装を変更するだけで、すべてのプログラムで自動的に反映される
// ユーザーコードは変更不要
Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
stack->SetNetDevice(device);
stack->Initialize();  // 内部でNWK層も自動的に設定される（将来の実装）
```

**違い:**
- ユーザーコードの変更が不要
- NishiokaStackの実装を変更するだけで反映
- 後方互換性を維持

### 4.4 エラーハンドリング

#### 従来の方法

```cpp
// 各ノードで個別にエラーチェック
Ptr<LrWpanMacBase> mac = device->GetMac();
if (!mac)
{
    // エラー処理
    return;
}
// ... 各ノードで同じチェックを繰り返す
```

#### NishiokaStack使用

```cpp
// NishiokaStack::DoInitialize()で統一的なエラーチェック
stack->SetNetDevice(device);
stack->Initialize();  // 内部でエラーチェックが行われる

// 使用時も簡潔
Ptr<LrWpanMacBase> mac = stack->GetMac();
if (!mac)  // 通常はnullptrにならない（Initialize()でチェック済み）
{
    // エラー処理
}
```

**違い:**
- エラーチェックが統一化
- エラーが発生しにくい
- デバッグが容易

## 5. 具体的な使用例の比較

### 5.1 3つのノードで通信する場合

#### 従来の方法

```cpp
// 3つのデバイスを個別に設定
Ptr<LrWpanNetDevice> dev0 = devices.Get(0);
Ptr<LrWpanNetDevice> dev1 = devices.Get(1);
Ptr<LrWpanNetDevice> dev2 = devices.Get(2);

// MAC層を個別に取得
Ptr<LrWpanMacBase> mac0 = dev0->GetMac();
Ptr<LrWpanMacBase> mac1 = dev1->GetMac();
Ptr<LrWpanMacBase> mac2 = dev2->GetMac();

// コールバックを個別に設定
mac0->SetMcpsDataIndicationCallback(MakeCallback(&Callback0));
mac1->SetMcpsDataIndicationCallback(MakeCallback(&Callback1));
mac2->SetMcpsDataIndicationCallback(MakeCallback(&Callback2));

// パケット送信も個別に
mac0->McpsDataRequest(params0, packet0);
mac1->McpsDataRequest(params1, packet1);
mac2->McpsDataRequest(params2, packet2);
```

**コード量**: 約30行

#### NishiokaStackContainer使用

```cpp
// スタックを作成してコンテナに追加
NishiokaStackContainer container;

for (uint32_t i = 0; i < devices.GetN(); i++)
{
    Ptr<NishiokaStack> stack = CreateObject<NishiokaStack>();
    stack->SetNetDevice(devices.Get(i));
    stack->Initialize();
    container.Add(stack);
}

// 統一的な方法でアクセス
for (uint32_t i = 0; i < container.GetN(); i++)
{
    Ptr<NishiokaStack> stack = container.Get(i);
    Ptr<LrWpanMacBase> mac = stack->GetMac();
    mac->SetMcpsDataIndicationCallback(MakeCallback(&Callback));
    mac->McpsDataRequest(params, packet);
}
```

**コード量**: 約15行（50%削減）

**利点:**
- コードが簡潔
- ノード数を増やす場合も簡単
- 保守性が向上

### 5.2 将来のNishiokaHelper::Install()の使用

#### 従来の方法

```cpp
// 各ノードで個別に設定（約30行）
// ... 上記のコード ...
```

#### NishiokaHelper::Install()使用（将来の実装）

```cpp
// 1行で複数のスタックをインストール
NishiokaHelper helper;
NishiokaStackContainer stacks = helper.Install(devices);

// すべてのスタックにアクセス
for (uint32_t i = 0; i < stacks.GetN(); i++)
{
    Ptr<NishiokaStack> stack = stacks.Get(i);
    // 処理
}
```

**コード量**: 約10行（67%削減）

**利点:**
- コードが大幅に簡潔化
- 他のNS-3ヘルパーと同じパターン
- 学習コストの削減

## 6. まとめ

### 6.1 可能になること

1. **層間接続の自動化**
   - `Initialize()`で自動的に確立
   - 手動設定が不要

2. **複数スタックの統一管理**
   - NishiokaStackContainerで効率的に管理
   - コードが簡潔化

3. **将来の拡張性**
   - NishiokaHelper::Install()の実装が可能
   - ユーザーコードの変更が不要

4. **コードの再利用性**
   - 統一的なインターフェース
   - 保守性の向上

5. **エラーハンドリングの統一**
   - 統一的なエラーチェック
   - デバッグが容易

### 6.2 今までとは異なる点

| 項目 | 従来 | NishiokaStack使用 |
|------|------|------------------|
| **アクセス方法** | `device->GetMac()` | `stack->GetMac()` |
| **初期化** | 手動 | 自動化 |
| **複数ノード管理** | 個別管理 | コンテナで統一管理 |
| **コード量** | 多い | 約50%削減 |
| **拡張性** | 低い | 高い |
| **再利用性** | 低い | 高い |

### 6.3 実用的な利点

1. **開発効率の向上**
   - コードが簡潔
   - バグが発生しにくい
   - デバッグが容易

2. **保守性の向上**
   - 統一的なインターフェース
   - 変更の影響範囲が限定的

3. **学習コストの削減**
   - NS-3の標準パターンに従う
   - 他のモジュールと同じ使い方

4. **将来の拡張に対応**
   - NWK層などの追加が容易
   - 後方互換性を維持

NishiokaStackとNishiokaStackContainerの実装により、コードが簡潔になり、保守性と拡張性が向上します。

