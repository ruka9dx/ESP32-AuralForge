# AuralForge — ESP32 Bluetoothオーディオ・マルチエフェクター

AuralForgeは、スマートフォンやパソコンの音楽をBluetoothでESP32に送り、音にエフェクトをかけて再生するプロジェクトです。エフェクトの選択や強さの調整は本体のボタンとロータリーエンコーダで行います。

音声はESP32からI2S DAC（デジタル音声をアナログに変換する基板）へ出力します。LCDにはBluetoothの接続状態、曲名、選択中のエフェクトと強度を表示します。一度接続した端末は、次回からESP32起動時に自動再接続を試みます。

## できること

- Bluetooth（A2DP）でステレオ音声を受信し、I2Sで出力
- 10種類のエフェクトを選択：Reverb、Chorus、Metallic、Filter、Echo、Flanger、Tremolo、Bass、Lo-Fi、Karaoke
- ロータリーエンコーダでエフェクトの強度を調整
- エフェクト切り替えボタンの同時長押しで、複数エフェクトを重ねるマルチモードに切り替え
- ボタンから再生・一時停止、音量調整、曲送り・曲戻しを操作
- Bluetooth音量をESP32に保存し、接続時に復元
- 16桁2行のI2C LCDに接続状態、曲名、エフェクトと強度を表示
- 前回接続したBluetooth端末への自動再接続

## 必要なもの

- Freenove ESP32-WROVER
- I2S DACまたはアンプ基板
- I2C接続の16桁2行LCD（初期設定のアドレスは `0x27`）
- プッシュボタンとロータリーエンコーダ
- 音楽を送信するスマートフォンまたはパソコン
- ファームウェアを書き込むためのUSBケーブル

配線前に、下のGPIO割り当てを確認してください。ボタンは `INPUT_PULLUP` を使用するため、それぞれGPIOとGNDの間に接続します。LCDのアドレスが異なる場合は、`include/config.h` の `LCD_I2C_ADDR` を実際のアドレスに変更してください。

### GPIO割り当て

| 接続する部品 | ESP32のGPIO |
| --- | ---: |
| I2S BCK（クロック） | 26 |
| I2S LRCK / WS（左右チャンネル） | 25 |
| I2S DOUT（音声データ出力） | 19 |
| エフェクトボタン（前へ） | 4 |
| 再生 / 一時停止ボタン | 13 |
| 音量ダウン / 曲戻しボタン | 14 |
| 音量アップ / 曲送りボタン | 18 |
| エフェクトボタン（次へ） | 23 |
| エンコーダ A / B / 押しボタン | 32 / 33 / 27 |
| LCD SDA / SCL | 21 / 22 |

## 本体の操作

| 操作部 | 動作 |
| --- | --- |
| GPIO 4のボタン | 前のエフェクトを選択 |
| GPIO 23のボタン | 次のエフェクトを選択 |
| エンコーダを回す | 選択中のエフェクトの強度を調整 |
| エンコーダを押す | エフェクトをOFFにして強度をリセット |
| GPIO 4とGPIO 23のボタンを同時に長押し | 通常モード / マルチモードを切り替え |
| GPIO 13のボタン | 再生 / 一時停止 |
| GPIO 14を短く押す | Bluetooth音量を下げる |
| GPIO 14を長押し | 前の曲へ |
| GPIO 18を短く押す | Bluetooth音量を上げる |
| GPIO 18を長押し | 次の曲へ |

Bluetooth音量はESP32の不揮発性メモリに保存され、AVRCP接続後に復元されます。初回起動時の音量は64/127です。Windowsのミュート状態はWindows側でも管理されるため、環境によってはWindows側でミュート解除が必要です。

Filterでは、エンコーダを回してローパス（低い音を通す）から中立位置を経てハイパス（高い音を通す）まで調整できます。

マルチモードではLCD上段に `MULTI EFFECT ON` と表示されます。エフェクトボタンの短押しで調整対象を選び、エンコーダでそのエフェクトの強度を調整します。各エフェクトの強度は通常モードとは別に保持され、再度切り替えても設定は残ります。初期値はすべて0%で、Filterのみ中立位置の50%です。エンコーダを押すと、マルチモード側の強度だけを初期値に戻します。

マルチモードでは、音色を整えるエフェクト（Filter、Bass、Karaoke、Lo-Fi）の後に、Metallic・Chorus・Flanger・Tremoloを適用し、最後にEcho、Reverbの順で空間系を加えます。これは一般的な「音色系 → モジュレーション系 → ディレイ → リバーブ」の信号順を参考にした初期設定です。

## Bluetoothで接続する

1. ESP32の電源を入れます。
2. スマートフォンまたはパソコンのBluetooth設定を開き、**ESP32-MultiEffect** に接続します。
3. 接続後は、ESP32の次回起動時から最後に接続した端末への再接続を試みます。

自動再接続するには、接続先端末のBluetoothを有効にし、ESP32の近くに置いてください。

## ビルドと書き込み

### VS Codeを使う場合

1. [Visual Studio Code](https://code.visualstudio.com/) と **PlatformIO IDE** 拡張機能をインストールします。
2. このプロジェクトのフォルダーをVS Codeで開き、PlatformIOによるライブラリの準備が終わるまで待ちます。
3. ESP32をUSBでパソコンに接続します。
4. **PlatformIO: Build** でビルドします。書き込むときは `Ctrl+Alt+U` を押すか、コマンドパレット（`Ctrl+Shift+P`）から **PlatformIO: Upload** を実行します。

### PlatformIO Coreを使う場合

プロジェクトのフォルダーで次のコマンドを実行します。

```sh
pio run
pio run --target upload --upload-port COM5
```

`COM5` はESP32が接続されているシリアルポート名に置き換えてください。

## シリアルモニター

シリアルモニターはArduino IDEの同名機能に相当し、ESP32からのログを確認できます。VS Codeでは `Ctrl+Alt+S` を押すか、コマンドパレットから **PlatformIO: Serial Monitor** を実行します。通信速度は **115200 baud** です。

PlatformIO Coreを使う場合:

```sh
pio device monitor --baud 115200
```

## ファイル構成

| ファイル | 内容 |
| --- | --- |
| `src/main.cpp` | 起動処理、Bluetoothオーディオ、各操作の制御 |
| `src/MultiEffectStream.cpp` | 音声エフェクト処理 |
| `src/Controls.cpp` | ボタンとロータリーエンコーダの処理 |
| `src/DisplayUI.cpp` | LCD表示と曲名スクロール |
| `include/config.h` | GPIO割り当てやエフェクトの設定 |
| `platformio.ini` | 対象ボード、ライブラリ、書き込み設定 |

## 制限事項

現在、曲名はLCD表示用にASCII文字へ変換しています。そのため、日本語などASCII以外の文字は `?` と表示されます。
