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
  void setMultiMode(bool enabled);
  void resetMultiAmounts();
  void selectMultiEffect(int dir);
  bool isMultiMode();
  bool isMultiModeAvailable();

  int mode();
  int amount();

  size_t write(uint8_t d) override;
  size_t write(const uint8_t *data, size_t size) override;

 private:
  struct EffectState {
    uint32_t idx = 0;
    int32_t lp_l = 0;
    int32_t lp_r = 0;
    uint32_t lfo_counter = 0;
    uint8_t lfo_div = 0;
    int32_t f_lp_l = 0;
    int32_t f_lp_r = 0;
    int32_t met_lp_l = 0;
    int32_t met_lp_r = 0;
    uint32_t trem_phase = 0;
    int32_t bb_l = 0;
    int32_t bb_r = 0;
    int32_t hold_l = 0;
    int32_t hold_r = 0;
    int32_t lofi_lp_l = 0;
    int32_t lofi_lp_r = 0;
    int crush_cnt = 0;
  };

  void buildFilterTable();
  uint32_t delayBufferLength(int m);
  void clearEffectState(int m);
  void process(int32_t &l, int32_t &r, int m, int a,
               bool isolated_buffer);

  Print &out;
  int16_t *buf_l[MODE_COUNT] = {};
  int16_t *buf_r[MODE_COUNT] = {};
  uint32_t buf_len[MODE_COUNT] = {};
  EffectState effect_state[MODE_COUNT] = {};
  int32_t filter_coef[201] = {0};

  volatile int mode_value = 0;
  volatile int std_amount = 0;
  volatile int filter_amount = FILTER_CENTER;
  volatile bool multi_mode = false;
  bool multi_mode_available = true;
  int selected_multi_mode = FX_REVERB;
  volatile int multi_amounts[MODE_COUNT] = {0, 0, 0, 0, FILTER_CENTER};
  volatile bool clear_request = false;
};
