#!/bin/bash

# battery.txtの保存先（絶対パス）
BATTERY_FILE="/home/yuugo/ns-3-dev/scratch/battery.txt"

# PiSugarのバッテリー残量をi2cで読み取る
battery_level=$(sudo i2cget -y 1 0x57 0x2A)

# 結果をbattery.txtに保存
echo "$battery_level" > "$BATTERY_FILE"

echo "$(date '+%Y-%m-%d %H:%M:%S') - Battery level: $battery_level"

