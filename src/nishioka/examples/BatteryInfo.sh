#!/bin/bash

# battery.txtの保存先（examplesディレクトリ）
# スクリプトのディレクトリを取得
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BATTERY_FILE="${SCRIPT_DIR}/battery.txt"

# 取得回数（60秒間で30回取得）
READ_COUNT=30
# 取得間隔（秒）- 60秒 / 30回 ≈ 2秒
READ_INTERVAL=2

# バッテリー値を格納する配列
declare -a battery_values=()

# 複数回バッテリー残量を取得
for ((i=0; i<$READ_COUNT; i++)); do
    value=$(sudo i2cget -y 1 0x57 0x2A 2>/dev/null)
    if [ $? -eq 0 ] && [ -n "$value" ]; then
        # 16進数を10進数に変換して配列に追加
        battery_values+=($(printf "%d" $value))
    else
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Warning: Failed to read battery value (attempt $((i+1))/$READ_COUNT)" >&2
    fi
    # 最後の取得でない場合、間隔を空ける
    if [ $i -lt $(($READ_COUNT - 1)) ]; then
        sleep $READ_INTERVAL
    fi
done

# 取得した値の数を確認
if [ ${#battery_values[@]} -eq 0 ]; then
    echo "$(date '+%Y-%m-%d %H:%M:%S') - Error: Failed to read any battery values" >&2
    exit 1
fi

# 配列をソート
IFS=$'\n' sorted_values=($(sort -n <<<"${battery_values[*]}"))
unset IFS

# 中央値を計算
array_size=${#sorted_values[@]}
if [ $((array_size % 2)) -eq 0 ]; then
    # 偶数の場合：2つの中央値の平均を取る
    median_index1=$((($array_size / 2) - 1))
    median_index2=$(($array_size / 2))
    median1=${sorted_values[$median_index1]}
    median2=${sorted_values[$median_index2]}
    battery_level_median=$((($median1 + $median2) / 2))
else
    # 奇数の場合：中央の値
    median_index=$((($array_size - 1) / 2))
    battery_level_median=${sorted_values[$median_index]}
fi

# 10進数を16進数に変換（元の形式に合わせる）
battery_level=$(printf "0x%02x" $battery_level_median)

# 結果をbattery.txtに保存
echo "$battery_level" > "$BATTERY_FILE"

# 統計情報を計算（最小値、最大値、平均値）
min_value=${sorted_values[0]}
max_value=${sorted_values[$(($array_size - 1))]}
sum=0
for val in "${sorted_values[@]}"; do
    sum=$((sum + val))
done
avg_value=$((sum / array_size))

# ログ出力（統計情報と中央値を表示）
echo "$(date '+%Y-%m-%d %H:%M:%S') - Battery: Min=$min_value Max=$max_value Avg=$avg_value Median=$battery_level_median ($battery_level) [${array_size} readings]"
