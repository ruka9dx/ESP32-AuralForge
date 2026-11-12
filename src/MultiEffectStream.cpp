#include "MultiEffectStream.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

namespace {

constexpr int MULTI_EFFECT_ORDER[] = {
    FX_FILTER, FX_BASS, FX_KARAOKE, FX_LOFI, FX_METALLIC,
    FX_CHORUS, FX_FLANGER, FX_TREMOLO, FX_ECHO, FX_REVERB};

inline int16_t clip16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

inline uint32_t delayedIndex(uint32_t index, uint32_t delay, uint32_t length) {
  return index >= delay ? index - delay : index + length - delay;
}

}  // namespace

MultiEffectStream::MultiEffectStream(Print &out_stream) : out(out_stream) {}

// 0.5%刻み(i=0〜200)ごとのフィルタ係数を作る
// 遮断周波数を対数で動かし、中央付近は変化大、0%/100%に近づくほど変化小になるカーブにする
void MultiEffectStream::buildFilterTable() {
  for (int i = 0; i <= 200; i++) {
    float d = fabsf((float)i - 100.0f) / 100.0f;  // 0(中央)〜1(端)
    float s = 1.0f - (1.0f - d) * (1.0f - d);     // 端に近づくほどなだらか
    float fc;
    if (i < 100)
      fc = FILTER_LP_START_HZ * powf(FILTER_LP_MIN_HZ / FILTER_LP_START_HZ, s);
    else
      fc = FILTER_HP_START_HZ * powf(FILTER_HP_MAX_HZ / FILTER_HP_START_HZ, s);
    float alpha = 1.0f - expf(-2.0f * 3.14159265f * fc / AUDIO_FS);
    int c = (int)(alpha * 65536.0f + 0.5f);
    if (c < 1) c = 1;
    if (c > 65535) c = 65535;
    filter_coef[i] = c;
  }
}

void MultiEffectStream::process(int32_t &l, int32_t &r, int m, int a,
                                bool isolated_buffer) {
  EffectState &state = effect_state[m];
  int buffer_mode = isolated_buffer ? m : FX_REVERB;
  int32_t left = l;
  int32_t right = r;

  state.lp_l += (left - state.lp_l) >> 3;
  state.lp_r += (right - state.lp_r) >> 3;

  state.bb_l += (left - state.bb_l) >> 6;
  state.bb_r += (right - state.bb_r) >> 6;

  int32_t hp_l = left - state.lp_l;
  int32_t hp_r = right - state.lp_r;
  int32_t bl = left + (state.lp_l >> 1) + (hp_l >> 2);
  int32_t br = right + (state.lp_r >> 1) + (hp_r >> 2);

  int32_t fx_l = left;
  int32_t fx_r = right;

  if (m == FX_REVERB) {
    int32_t dl = buf_l[buffer_mode][state.idx];
    int32_t dr = buf_r[buffer_mode][state.idx];
    fx_l = bl + (dl >> 1);
    fx_r = br + (dr >> 1);
    buf_l[buffer_mode][state.idx] = clip16(bl + dl - (dl >> 2));
    buf_r[buffer_mode][state.idx] = clip16(br + dr - (dr >> 2));
    if (++state.idx == buf_len[buffer_mode]) state.idx = 0;
  } else if (m == FX_CHORUS) {
    state.lfo_div++;
    if (state.lfo_div >= 32) {
      state.lfo_div = 0;
      state.lfo_counter++;
    }
    int32_t lfo = state.lfo_counter & 2047;
    if (lfo > 1024) lfo = 2048 - lfo;
    uint32_t mod_delay = 1500 + (lfo >> 1);
    uint32_t ri_l = delayedIndex(state.idx, mod_delay, buf_len[buffer_mode]);
    uint32_t ri_r = delayedIndex(state.idx, mod_delay + 100, buf_len[buffer_mode]);
    int32_t dl = buf_l[buffer_mode][ri_l];
    int32_t dr = buf_r[buffer_mode][ri_r];
    fx_l = bl + dl;
    fx_r = br + dr;
    buf_l[buffer_mode][state.idx] = clip16(bl + (dl >> 2));
    buf_r[buffer_mode][state.idx] = clip16(br + (dr >> 2));
    if (++state.idx == buf_len[buffer_mode]) state.idx = 0;
  } else if (m == FX_METALLIC) {
    uint32_t ri = delayedIndex(state.idx, METAL_LEN, buf_len[buffer_mode]);
    int32_t dl = buf_l[buffer_mode][ri];
    int32_t dr = buf_r[buffer_mode][ri];
    state.met_lp_l += (dl - state.met_lp_l) >> 1;
    state.met_lp_r += (dr - state.met_lp_r) >> 1;
    fx_l = left + (dl >> 1);
    fx_r = right + (dr >> 1);
    buf_l[buffer_mode][state.idx] =
        clip16(left + state.met_lp_l - (state.met_lp_l >> 2));
    buf_r[buffer_mode][state.idx] =
        clip16(right + state.met_lp_r - (state.met_lp_r >> 2));
    if (++state.idx == buf_len[buffer_mode]) state.idx = 0;
  } else if (m == FX_FILTER) {
    if (a == FILTER_CENTER) {
      fx_l = left;
      fx_r = right;
      state.f_lp_l = left * 256;
      state.f_lp_r = right * 256;
    } else {
      int32_t coef = filter_coef[a / 5];
      state.f_lp_l +=
          (int32_t)(((int64_t)(left * 256 - state.f_lp_l) * coef) >> 16);
      state.f_lp_r +=
          (int32_t)(((int64_t)(right * 256 - state.f_lp_r) * coef) >> 16);
      int32_t lo_l = state.f_lp_l >> 8;
      int32_t lo_r = state.f_lp_r >> 8;
      if (a < FILTER_CENTER) {
        fx_l = lo_l;
        fx_r = lo_r;
      } else {
        fx_l = left - lo_l;
        fx_r = right - lo_r;
      }
    }
    a = AMOUNT_MAX;
  } else if (m == FX_ECHO) {
    uint32_t ri = delayedIndex(state.idx, ECHO_LEN, buf_len[buffer_mode]);
    int32_t dl = buf_l[buffer_mode][ri];
    int32_t dr = buf_r[buffer_mode][ri];
    fx_l = left + dl;
    fx_r = right + dr;
    buf_l[buffer_mode][state.idx] = clip16(left + (dl >> 1));
    buf_r[buffer_mode][state.idx] = clip16(right + (dr >> 1));
    if (++state.idx == buf_len[buffer_mode]) state.idx = 0;
  } else if (m == FX_FLANGER) {
    state.lfo_div++;
    if (state.lfo_div >= 32) {
      state.lfo_div = 0;
      state.lfo_counter++;
    }
    int32_t lfo = state.lfo_counter & 2047;
    if (lfo > 1024) lfo = 2048 - lfo;
    uint32_t d_l = 8 + (lfo >> 3);
    uint32_t d_r = 144 - (lfo >> 3);
    uint32_t ri_l = delayedIndex(state.idx, d_l, buf_len[buffer_mode]);
    uint32_t ri_r = delayedIndex(state.idx, d_r, buf_len[buffer_mode]);
    int32_t dl = buf_l[buffer_mode][ri_l];
    int32_t dr = buf_r[buffer_mode][ri_r];
    fx_l = left + dl;
    fx_r = right + dr;
    buf_l[buffer_mode][state.idx] = clip16(left + (dl >> 1));
    buf_r[buffer_mode][state.idx] = clip16(right + (dr >> 1));
    if (++state.idx == buf_len[buffer_mode]) state.idx = 0;
  } else if (m == FX_TREMOLO) {
    state.trem_phase += 4;
    int32_t t = (state.trem_phase >> 4) & 2047;
    if (t > 1024) t = 2048 - t;
    int32_t g = 1024 - t;
    fx_l = (left * g) >> 10;
    fx_r = (right * g) >> 10;
  } else if (m == FX_BASS) {
    fx_l = left + state.bb_l + (state.bb_l >> 1);
    fx_r = right + state.bb_r + (state.bb_r >> 1);
  } else if (m == FX_LOFI) {
    int shift = (a * 9) / AMOUNT_MAX;
    int hold = 1 + (a * 5) / AMOUNT_MAX;
    if (++state.crush_cnt >= hold) {
      state.crush_cnt = 0;
      state.hold_l = left;
      state.hold_r = right;
    }
    int32_t half = (1 << shift) >> 1;
    int32_t q_l = ((state.hold_l + half) >> shift) << shift;
    int32_t q_r = ((state.hold_r + half) >> shift) << shift;
    int coef = 512 / (hold + 1);
    if (coef > 256) coef = 256;
    state.lofi_lp_l += ((q_l - state.lofi_lp_l) * coef) >> 8;
    state.lofi_lp_r += ((q_r - state.lofi_lp_r) * coef) >> 8;
    fx_l = state.lofi_lp_l;
    fx_r = state.lofi_lp_r;
    a = AMOUNT_MAX;
  } else if (m == FX_KARAOKE) {
    int32_t side = (left - right) >> 1;
    int32_t bass = (state.bb_l + state.bb_r) >> 1;
    fx_l = side + bass;
    fx_r = side + bass;
  }

  l = left + (fx_l - left) * a / AMOUNT_MAX;
  r = right + (fx_r - right) * a / AMOUNT_MAX;
}

uint32_t MultiEffectStream::delayBufferLength(int m) {
  switch (m) {
    case FX_REVERB:
      return DELAY_LEN;
    case FX_CHORUS:
      return 4096;
    case FX_METALLIC:
      return METAL_LEN + 1;
    case FX_ECHO:
      return ECHO_LEN + 1;
    case FX_FLANGER:
      return 256;
    default:
      return 0;
  }
}

void MultiEffectStream::clearEffectState(int m) {
  memset(&effect_state[m], 0, sizeof(effect_state[m]));
  if (buf_l[m] != nullptr) {
    memset(buf_l[m], 0, buf_len[m] * sizeof(int16_t));
    memset(buf_r[m], 0, buf_len[m] * sizeof(int16_t));
  }
}

bool MultiEffectStream::begin() {
  buildFilterTable();
  buf_len[FX_REVERB] = DELAY_LEN;
  buf_l[FX_REVERB] = (int16_t *)heap_caps_calloc(
      DELAY_LEN, sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  buf_r[FX_REVERB] = (int16_t *)heap_caps_calloc(
      DELAY_LEN, sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buf_l[FX_REVERB] == nullptr || buf_r[FX_REVERB] == nullptr) {
    Serial.println("[MultiEffectStream] base delay buffer allocation failed");
    if (buf_l[FX_REVERB] != nullptr) heap_caps_free(buf_l[FX_REVERB]);
    if (buf_r[FX_REVERB] != nullptr) heap_caps_free(buf_r[FX_REVERB]);
    buf_l[FX_REVERB] = nullptr;
    buf_r[FX_REVERB] = nullptr;
    return false;
  }

  for (int m = FX_CHORUS; m < MODE_COUNT; ++m) {
    buf_len[m] = delayBufferLength(m);
    if (buf_len[m] == 0) continue;

    buf_l[m] = (int16_t *)heap_caps_calloc(
        buf_len[m], sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    buf_r[m] = (int16_t *)heap_caps_calloc(
        buf_len[m], sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf_l[m] == nullptr || buf_r[m] == nullptr) {
      Serial.printf("[MultiEffectStream] PSRAM allocation failed: %s\n",
                    MODE_NAMES[m]);
      multi_mode_available = false;
      for (int i = FX_CHORUS; i < MODE_COUNT; ++i) {
        if (buf_l[i] != nullptr) heap_caps_free(buf_l[i]);
        if (buf_r[i] != nullptr) heap_caps_free(buf_r[i]);
        buf_l[i] = nullptr;
        buf_r[i] = nullptr;
      }
      break;
    }
  }
  if (multi_mode_available) {
    Serial.printf("[MultiEffectStream] Internal free: %u, PSRAM free: %u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                                    MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM |
                                                    MALLOC_CAP_8BIT));
  } else {
    Serial.println("[MultiEffectStream] Multi-effect mode disabled");
  }
  return true;
}

void MultiEffectStream::setMode(int m) {
  if (m < FX_OFF || m >= MODE_COUNT) m = FX_OFF;
  mode_value = m;
  clear_request = true;
}

void MultiEffectStream::setAmount(int a) {
  if (a < 0) a = 0;
  if (a > AMOUNT_MAX) a = AMOUNT_MAX;
  int m = mode();
  if (multi_mode) {
    multi_amounts[m] = a;
  } else if (m == FX_FILTER) {
    filter_amount = a;
  } else {
    std_amount = a;
  }
}

void MultiEffectStream::resetAmounts() {
  std_amount = 0;
  filter_amount = FILTER_CENTER;
}

void MultiEffectStream::setMultiMode(bool enabled) {
  if (enabled && !multi_mode_available) return;
  multi_mode = enabled;
  clear_request = true;
}

void MultiEffectStream::resetMultiAmounts() {
  for (int m = FX_OFF; m < MODE_COUNT; ++m) multi_amounts[m] = 0;
  multi_amounts[FX_FILTER] = FILTER_CENTER;
  selected_multi_mode = FX_REVERB;
  clear_request = true;
}

void MultiEffectStream::selectMultiEffect(int dir) {
  int index = selected_multi_mode - FX_REVERB + dir;
  const int effect_count = MODE_COUNT - 1;
  index = (index % effect_count + effect_count) % effect_count;
  selected_multi_mode = index + FX_REVERB;
}

bool MultiEffectStream::isMultiMode() { return multi_mode; }

bool MultiEffectStream::isMultiModeAvailable() { return multi_mode_available; }

int MultiEffectStream::mode() {
  return multi_mode ? selected_multi_mode : mode_value;
}

int MultiEffectStream::amount() {
  int m = mode();
  if (multi_mode) return multi_amounts[m];
  return m == FX_FILTER ? filter_amount : std_amount;
}

size_t MultiEffectStream::write(uint8_t d) { return out.write(d); }

size_t MultiEffectStream::write(const uint8_t *data, size_t size) {
  bool use_multi_mode = multi_mode;
  int current_mode = mode_value;
  if ((!use_multi_mode && current_mode == FX_OFF) ||
      buf_l[FX_REVERB] == nullptr) {
    return out.write(data, size);
  }
  if (clear_request) {
    for (int m = FX_REVERB; m < MODE_COUNT; ++m) clearEffectState(m);
    clear_request = false;
  }
  const int16_t *in = (const int16_t *)data;
  size_t frames = size / 4;
  int16_t tmp[256];
  size_t done = 0;
  while (done < frames) {
    size_t n = frames - done;
    if (n > 128) n = 128;
    for (size_t k = 0; k < n; k++) {
      int32_t l = in[(done + k) * 2];
      int32_t r = in[(done + k) * 2 + 1];
      if (use_multi_mode) {
        for (int stage = 0; stage < MODE_COUNT - 1; ++stage) {
          int m = MULTI_EFFECT_ORDER[stage];
          int a = multi_amounts[m];
          if ((m == FX_FILTER && a == FILTER_CENTER) ||
              (m != FX_FILTER && a == 0)) {
            continue;
          }
          process(l, r, m, a, true);
        }
      } else {
        int a = current_mode == FX_FILTER ? filter_amount : std_amount;
        process(l, r, current_mode, a, false);
      }
      tmp[k * 2] = clip16(l);
      tmp[k * 2 + 1] = clip16(r);
    }
    out.write((const uint8_t *)tmp, n * 4);
    done += n;
  }
  return size;
}
