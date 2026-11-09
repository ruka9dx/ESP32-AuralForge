# ESP32 Bluetooth オーディオ・マルチエフェクター

ESP32 を Bluetooth オーディオレシーバーとして使い、受信音声にエフェクトをかけて I2S 出力するプロジェクトです。
エフェクト選択・強度調整、再生操作、曲名表示に対応しています。

## 機能

- Bluetooth A2DP オーディオ受信と AVRCP 再生操作
- Reverb、Chorus、Metallic、Filter、Echo、Flanger、Tremolo、Bass、Lo-Fi、Karaoke
- ロータリーエンコーダによるエフェクト強度調整
- I2C LCD への曲名・エフェクト表示
- 一度接続した Bluetooth 端末への起動時自動再接続

## ピン配置

| 用途 | GPIO |
| --- | --- |
| I2S BCK / LRCK / DOUT | 26 / 25 / 19 |
| モードボタン | 4 |
| 再生/一時停止 | 13 |
| 音量アップ / ダウン | 14 / 18 |
| エフェクト前へ | 23 |
| エンコーダ A / B / SW | 32 / 33 / 27 |
| I2C SDA / SCL | 21 / 22 |

LCD の I2C アドレスは `0x27` です。異なる場合は `include/config.h` の `LCD_I2C_ADDR` を変更してください。

## Bluetooth 接続

初回は端末の Bluetooth 設定から `ESP32-MultiEffect` に接続してください。以降、ESP32 の起動時に最後に接続した端末への再接続を試みます。

## ビルドと書き込み

VS Code の PlatformIO 拡張では、`Ctrl+Alt+U` でESP32へ書き込めます。
コマンドパレット（`Ctrl+Shift+P`）から `PlatformIO: Upload` を実行する方法もあります。
PlatformIO Core を使う場合:

```sh
pio run
pio run --target upload --upload-port COM5
```

ポート名は接続環境に合わせて変更してください。

## シリアルモニター

Arduino IDE のシリアルモニターに相当する機能は PlatformIO にあります。
`Ctrl+Alt+S` で開くか、PlatformIO サイドバーの **Project Tasks → General → Monitor** を選択してください。
接続後のボーレートは 115200 baud です。CLI からは `pio device monitor` で開けます。
