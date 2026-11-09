#pragma once

#include <Arduino.h>

// --- ピン定義 ---
// I2CのSCL(22番)と被らないよう、I2SのDOUTは19番を使用
constexpr int IO_I2S_BCK = 26;
constexpr int IO_I2S_LRCK = 25;
constexpr int IO_I2S_DOUT = 19;

constexpr int IO_MODE_BTN = 4;
constexpr int IO_BTN_PLAY = 13;
constexpr int IO_BTN_VOL_UP = 14;
constexpr int IO_BTN_VOL_DOWN = 18;
constexpr int IO_BTN_FX_PREV = 23;
constexpr int IO_ENC_A = 32;
constexpr int IO_ENC_B = 33;
constexpr int IO_ENC_SW = 27;

// I2C LCD設定: アドレス0x27, 16桁, 2行 (動かない場合は0x3Fに変更)
constexpr uint8_t LCD_I2C_ADDR = 0x27;
constexpr int LCD_COLS = 16;
constexpr int LCD_ROWS = 2;

// --- 曲名スクロール設定 ---
constexpr uint32_t SCROLL_STEP_MS = 350;    // 1文字ずれる間隔
constexpr uint32_t SCROLL_PAUSE_MS = 1200;  // 端で止まる時間

// --- エフェクト定義 ---
enum Mode {
  FX_OFF = 0,
  FX_REVERB,
  FX_CHORUS,
  FX_METALLIC,
  FX_FILTER,
  FX_ECHO,
  FX_FLANGER,
  FX_TREMOLO,
  FX_BASS,
  FX_LOFI,
  FX_KARAOKE,
  MODE_COUNT
};

constexpr const char *MODE_NAMES[MODE_COUNT] = {
    "OFF",     "Reverb", "Chorus",  "Metallic", "Filter", "Echo",
    "Flanger", "Tremolo", "Bass",   "Lo-Fi",    "Karaoke"};

constexpr int COUNTS_PER_DETENT = 4;
constexpr int ENC_DIR = -1;
constexpr uint32_t DEBOUNCE_MS = 20;
constexpr uint32_t LONG_PRESS_MS = 700;
constexpr int VOLUME_STEP = 6;

// 強度は0.1%単位の整数で扱う (0〜1000 = 0.0〜100.0%)
constexpr int AMOUNT_MAX = 1000;
constexpr int AMOUNT_STEP = 25;     // エンコーダ1ノッチ = 2.5%
constexpr int FILTER_CENTER = 500;  // Filterの中央 (50.0%)

constexpr uint32_t DELAY_LEN = 16384;
constexpr uint32_t ECHO_LEN = 12000;  // 約270ms
constexpr uint32_t METAL_LEN = 120;   // Metallicの遅延 (約2.7ms。長くすると共鳴音が低くなる)

// --- Filterの設定 ---
constexpr float AUDIO_FS = 44100.0f;
constexpr float FILTER_LP_START_HZ = 20000.0f;  // 49%付近のローパス遮断周波数 (ほぼ素通し)
constexpr float FILTER_LP_MIN_HZ = 150.0f;      // 0%のローパス遮断周波数
constexpr float FILTER_HP_START_HZ = 30.0f;     // 51%付近のハイパス遮断周波数 (ほぼ素通し)
constexpr float FILTER_HP_MAX_HZ = 5000.0f;     // 100%のハイパス遮断周波数

// --- ボタン ---
enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

struct Button {
  int pin;
  int reading;
  int stable;
  uint32_t changed;
  uint32_t pressed_at;
  bool long_fired;
};
