#pragma once

#include <Arduino.h>

#include "config.h"

// A2DPのPCM音声を加工してI2S出力へ渡す。
class MultiEffectStream : public Print {
 public:
  explicit MultiEffectStream(Print &out_stream);

  bool begin();

  void setMode(int m);
  void setAmount(int a);
  void resetAmounts();

  int mode();
  int amount();

  size_t write(uint8_t d) override;
  size_t write(const uint8_t *data, size_t size) override;

 private:
  void buildFilterTable();
  void process(int32_t &l, int32_t &r, int m, int a);

  Print &out;
  int16_t *buf_l = nullptr;
  int16_t *buf_r = nullptr;
  uint32_t idx = 0;

  int32_t lp_l = 0;
  int32_t lp_r = 0;
  int32_t lp2_l = 0;
  int32_t lp2_r = 0;
  uint32_t lfo_counter = 0;
  uint8_t lfo_div = 0;

  // Filter状態はQ8固定小数点で保持し、係数は0.5%刻みで事前計算する。
  int32_t f_lp_l = 0;
  int32_t f_lp_r = 0;
  int32_t filter_coef[201] = {0};

  // Metallicのフィードバック経路に使うLPF状態。
  int32_t met_lp_l = 0;
  int32_t met_lp_r = 0;

  uint32_t trem_phase = 0;  // Tremolo
  int32_t bb_l = 0;         // Bass / Karaoke 用の低域LPF
  int32_t bb_r = 0;
  int32_t hold_l = 0;  // Lo-Fi 用サンプルホールド
  int32_t hold_r = 0;
  int32_t lofi_lp_l = 0;  // Lo-Fi 用の高域なだめLPF
  int32_t lofi_lp_r = 0;
  int crush_cnt = 0;

  volatile int mode_value = 0;
  volatile int std_amount = 0;
  volatile int filter_amount = FILTER_CENTER;
  volatile bool clear_request = false;
};
