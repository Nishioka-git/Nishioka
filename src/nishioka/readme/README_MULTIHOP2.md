# multihop2 実行方法

## 概要
multihop2は、nishiokaモジュールのexamplesディレクトリで動作するマルチホップ通信のサンプルプログラムです。

## ファイル構成
- `multihop2.cc`: メインプログラム
- `BatteryInfo.sh`: バッテリー残量を取得するシェルスクリプト
- `battery.txt`: バッテリー残量を保存するファイル

## ビルド方法

```bash
cd /home/yuugo/ns-3-dev
./ns3 configure --enable-modules=uart-net-device,lr-wpan,nishioka
./ns3 build
```

## 実行方法

### 1. バッテリー情報の更新

#### 手動実行
実行前にバッテリー情報を更新する場合：

```bash
cd /home/yuugo/ns-3-dev/src/nishioka/examples
./BatteryInfo.sh
```

これにより、`battery.txt`に最新のバッテリー残量が保存されます。

#### crontabを使用した自動更新（推奨）

BatteryInfo.shを1分ごとに自動実行するようにcrontabを設定します。

##### 設定手順

1. **crontabエディタを開く**
   ```bash
   crontab -e
   ```

2. **以下の行を追加**
   ```cron
   * * * * * /home/yuugo/ns-3-dev/src/nishioka/examples/BatteryInfo.sh >> /home/yuugo/ns-3-dev/src/nishioka/examples/battery.log 2>&1
   ```
   
   この設定により、毎分BatteryInfo.shが実行され、ログが`battery.log`に保存されます。

3. **crontabの設定を確認**
   ```bash
   crontab -l
   ```

4. **cronサービスの状態を確認（必要に応じて）**
   ```bash
   sudo systemctl status cron
   # または
   sudo systemctl status crond
   ```

##### crontabの書式説明

```
* * * * * コマンド
│ │ │ │ │
│ │ │ │ └─── 曜日 (0-7, 0と7は日曜日)
│ │ │ └───── 月 (1-12)
│ │ └─────── 日 (1-31)
│ └───────── 時 (0-23)
└─────────── 分 (0-59)
```

例：
- `* * * * *`: 毎分実行
- `*/5 * * * *`: 5分ごとに実行
- `0 * * * *`: 毎時0分に実行
- `0 0 * * *`: 毎日0時0分に実行

##### ログの確認

```bash
# ログファイルの確認
tail -f /home/yuugo/ns-3-dev/src/nishioka/examples/battery.log

# 最新の20行を表示
tail -n 20 /home/yuugo/ns-3-dev/src/nishioka/examples/battery.log
```

##### crontabの削除

設定を削除する場合：

```bash
crontab -e
# 該当行を削除して保存
```

または、すべてのcrontab設定を削除：

```bash
crontab -r
```

##### 注意事項

- BatteryInfo.shは`sudo`権限が必要な場合があります（i2cgetコマンドの実行）
- sudoが必要な場合、crontabで実行するには以下のいずれかの方法が必要です：
  
  **方法1: sudoersに設定を追加（推奨）**
  ```bash
  sudo visudo
  # 以下の行を追加（パスワードなしでsudo実行を許可）
  yuugo ALL=(ALL) NOPASSWD: /usr/sbin/i2cget
  ```
  
  **方法2: crontabでsudoを使用**
  ```cron
  * * * * * sudo /home/yuugo/ns-3-dev/src/nishioka/examples/BatteryInfo.sh >> /home/yuugo/ns-3-dev/src/nishioka/examples/battery.log 2>&1
  ```
  
  **方法3: rootのcrontabに設定**
  ```bash
  sudo crontab -e
  # 同じ設定を追加
  ```

### 2. プログラムの実行

#### 実行ディレクトリ

プログラムは以下のいずれかのディレクトリから実行できます：

1. **プロジェクトルートディレクトリ（推奨）**
   ```bash
   cd /home/yuugo/ns-3-dev
   ```

2. **実行ファイルがあるディレクトリ**
   ```bash
   cd /home/yuugo/ns-3-dev/build/src/nishioka/examples
   ```

3. **examplesソースディレクトリ**
   ```bash
   cd /home/yuugo/ns-3-dev/src/nishioka/examples
   ```

#### 実行コマンド

##### 方法1: ns3コマンドを使用（推奨）

```bash
cd /home/yuugo/ns-3-dev
./ns3 run src/nishioka/examples/multihop2
```

**メリット:**
- ns-3の標準的な実行方法
- ビルドと実行を統合的に管理できる
- パス解決が確実

**注意:** 
- `scratch/multihop2` も存在する場合、`./ns3 run multihop2` は曖昧なためエラーになります
- その場合は、明示的にパスを指定してください：
  ```bash
  ./ns3 run src/nishioka/examples/multihop2  # examplesディレクトリのmultihop2
  ./ns3 run scratch/multihop2                 # scratchディレクトリのmultihop2
  ```

##### 方法2: プロジェクトルートから直接実行

```bash
cd /home/yuugo/ns-3-dev
./build/src/nishioka/examples/ns3-dev-multihop2-default
```

**メリット:**
- プロジェクトルートから実行するため、パス解決が確実
- battery.txtの検索が複数の場所で試行される

##### 方法3: 実行ファイルのディレクトリから実行

```bash
cd /home/yuugo/ns-3-dev/build/src/nishioka/examples
./ns3-dev-multihop2-default
```

**メリット:**
- 実行ファイルと同じディレクトリにbattery.txtを配置すれば確実に見つかる
- パスが短くて入力しやすい

##### 方法4: 絶対パスで実行

どのディレクトリからでも実行可能：

```bash
/home/yuugo/ns-3-dev/build/src/nishioka/examples/ns3-dev-multihop2-default
```

#### 実行例

**推奨方法（ns3コマンド使用）:**
```bash
# プロジェクトルートに移動
cd /home/yuugo/ns-3-dev

# ns3コマンドで実行
./ns3 run src/nishioka/examples/multihop2
```

**直接実行:**
```bash
# プロジェクトルートに移動
cd /home/yuugo/ns-3-dev

# 実行ファイルを直接実行
./build/src/nishioka/examples/ns3-dev-multihop2-default
```

#### 実行時の注意事項

- 実行ファイルは`build/src/nishioka/examples/ns3-dev-multihop2-default`に配置されます
- 実行時のカレントディレクトリによって、`battery.txt`の検索パスが変わります
- プロジェクトルートから実行することを推奨します（パス解決が最も確実）

## バッテリーファイルの配置

プログラムは以下の順序で`battery.txt`を探します：

1. カレントディレクトリ（実行時の作業ディレクトリ）
2. 実行ファイルと同じディレクトリ（`build/src/nishioka/examples/`）
3. プロジェクトルートからの相対パス（`../../src/nishioka/examples/battery.txt`）
4. 絶対パス（`/home/yuugo/ns-3-dev/src/nishioka/examples/battery.txt`）

### 推奨: 実行ファイルと同じディレクトリに配置

```bash
# battery.txtを実行ファイルと同じディレクトリにコピー
cp /home/yuugo/ns-3-dev/src/nishioka/examples/battery.txt \
   /home/yuugo/ns-3-dev/build/src/nishioka/examples/battery.txt
```

これにより、どのディレクトリから実行しても`battery.txt`が見つかります。

## 注意事項

- `BatteryInfo.sh`はPiSugarのバッテリーを読み取るスクリプトです
- 実行には`sudo`権限が必要な場合があります
- `battery.txt`が存在しない場合、デフォルト値（100%）が使用されます

