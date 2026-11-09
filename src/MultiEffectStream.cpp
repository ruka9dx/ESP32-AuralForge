#include "MultiEffectStream.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

namespace {

inline int16_t clip16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
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

// a は 0〜AMOUNT_MAX (0.1%単位)
void MultiEffectStream::process(int32_t &l, int32_t &r, int m, int a) {
  int32_t left = l;
  int32_t right = r;

  lp_l += (left - lp_l) >> 3;
  lp_r += (right - lp_r) >> 3;
  lp2_l += (lp_l - lp2_l) >> 3;
  lp2_r += (lp_r - lp2_r) >> 3;

  // 約110Hz以下を取り出すLPF (Bass / Karaoke で使用)
  bb_l += (left - bb_l) >> 6;
  bb_r += (right - bb_r) >> 6;

  int32_t hp_l = left - lp_l;
  int32_t hp_r = right - lp_r;
  int32_t bl = left + (lp_l >> 1) + (hp_l >> 2);
  int32_t br = right + (lp_r >> 1) + (hp_r >> 2);

  int32_t fx_l = left;
  int32_t fx_r = right;

  if (m == FX_REVERB) {
    int32_t dl = buf_l[idx];
    int32_t dr = buf_r[idx];
    fx_l = bl + (dl >> 1);
    fx_r = br + (dr >> 1);
    buf_l[idx] = clip16(bl + dl - (dl >> 2));
    buf_r[idx] = clip16(br + dr - (dr >> 2));
    idx = (idx + 1) % DELAY_LEN;
  } else if (m == FX_CHORUS) {
    lfo_div++;
    if (lfo_div >= 32) {
      lfo_div = 0;
      lfo_counter++;
    }
    int32_t lfo = lfo_counter & 2047;
    if (lfo > 1024) lfo = 2048 - lfo;
    uint32_t mod_delay = 1500 + (lfo >> 1);
    uint32_t ri_l = (idx + DELAY_LEN - mod_delay) % DELAY_LEN;
    uint32_t ri_r = (idx + DELAY_LEN - (mod_delay + 100)) % DELAY_LEN;
    int32_t dl = buf_l[ri_l];
    int32_t dr = buf_r[ri_r];
    fx_l = bl + dl;
    fx_r = br + dr;
    buf_l[idx] = clip16(bl + (dl >> 2));
    buf_r[idx] = clip16(br + (dr >> 2));
    idx = (idx + 1) % DELAY_LEN;
  } else if (m == FX_METALLIC) {
    // 短いディレイの共鳴で金属的な響きを作る
    // 耳に痛くならないよう、フィードバックを約75%に抑え、帰還経路で高域を丸める
    uint32_t ri = (idx + DELAY_LEN - METAL_LEN) % DELAY_LEN;
    int32_t dl = buf_l[ri];
    int32_t dr = buf_r[ri];
    met_lp_l += (dl - met_lp_l) >> 1;
    met_lp_r += (dr - met_lp_r) >> 1;
    fx_l = left + (dl >> 1);
    fx_r = right + (dr >> 1);
    buf_l[idx] = clip16(left + met_lp_l - (met_lp_l >> 2));  // フィードバック約75%
    buf_r[idx] = clip16(right + met_lp_r - (met_lp_r >> 2));
    idx = (idx + 1) % DELAY_LEN;
  } else if (m == FX_FILTER) {
    // 50%=素通し / 0%側=ローパス / 100%側=ハイパス
    if (a == FILTER_CENTER) {
      fx_l = left;
      fx_r = right;
      f_lp_l = left * 256;
      f_lp_r = right * 256;
    } else {
      int32_t coef = filter_coef[a / 5];
      f_lp_l += (int32_t)(((int64_t)(left * 256 - f_lp_l) * coef) >> 16);
      f_lp_r += (int32_t)(((int64_t)(right * 256 - f_lp_r) * coef) >> 16);
      int32_t lo_l = f_lp_l >> 8;
      int32_t lo_r = f_lp_r >> 8;
      if (a < FILTER_CENTER) {
        fx_l = lo_l;
        fx_r = lo_r;
      } else {
        fx_l = left - lo_l;
        fx_r = right - lo_r;
      }
    }
    a = AMOUNT_MAX;
  }
  else if (m == FX_ECHO) {
    // 約270msのやまびこ。フィードバック50%
    uint32_t ri = (idx + DELAY_LEN - ECHO_LEN) % DELAY_LEN;
    int32_t dl = buf_l[ri];
    int32_t dr = buf_r[ri];
    fx_l = left + dl;
    fx_r = right + dr;
    buf_l[idx] = clip16(left + (dl >> 1));
    buf_r[idx] = clip16(right + (dr >> 1));
    idx = (idx + 1) % DELAY_LEN;
  } else if (m == FX_FLANGER) {
    // 極短ディレイをLFOで揺らす「ジェット機」のような音。L/Rで逆位相に揺らす
    lfo_div++;
    if (lfo_div >= 32) {
      lfo_div = 0;
      lfo_counter++;
    }
    int32_t lfo = lfo_counter & 2047;
    if (lfo > 1024) lfo = 2048 - lfo;
    uint32_t d_l = 8 + (lfo >> 3);    // 8〜136サンプル
    uint32_t d_r = 144 - (lfo >> 3);  // 逆方向に動く
    uint32_t ri_l = (idx + DELAY_LEN - d_l) % DELAY_LEN;
    uint32_t ri_r = (idx + DELAY_LEN - d_r) % DELAY_LEN;
    int32_t dl = buf_l[ri_l];
    int32_t dr = buf_r[ri_r];
    fx_l = left + dl;
    fx_r = right + dr;
    buf_l[idx] = clip16(left + (dl >> 1));  // フィードバック50%
    buf_r[idx] = clip16(right + (dr >> 1));
    idx = (idx + 1) % DELAY_LEN;
  } else if (m == FX_TREMOLO) {
    // 約5Hzで音量を揺らす。強度=深さ
    trem_phase += 4;
    int32_t t = (trem_phase >> 4) & 2047;
    if (t > 1024) t = 2048 - t;  // 三角波 0〜1024
    int32_t g = 1024 - t;
    fx_l = (left * g) >> 10;
    fx_r = (right * g) >> 10;
  } else if (m == FX_BASS) {
    // 低音ブースト (最大で低域を約1.5倍上乗せ)
    fx_l = left + bb_l + (bb_l >> 1);
    fx_r = right + bb_r + (bb_r >> 1);
  } else if (m == FX_LOFI) {
    // ビット数とサンプルレートを落とす。強度=荒さ (0で原音)
    int shift = (a * 9) / AMOUNT_MAX;     // 0〜9bit削減 (最大で7bit相当)
    int hold = 1 + (a * 5) / AMOUNT_MAX;  // 1〜6サンプル保持
    if (++crush_cnt >= hold) {
      crush_cnt = 0;
      hold_l = left;
      hold_r = right;
    }
    // 四捨五入で量子化 (切り捨てだと負側に偏って直流成分と歪みが出る)
    int32_t half = (1 << shift) >> 1;
    int32_t q_l = ((hold_l + half) >> shift) << shift;
    int32_t q_r = ((hold_r + half) >> shift) << shift;
    // ホールドで生じる高域のざらつきをなだめるLPF (hold=1のときは素通し)
    int coef = 512 / (hold + 1);
    if (coef > 256) coef = 256;
    lofi_lp_l += ((q_l - lofi_lp_l) * coef) >> 8;
    lofi_lp_r += ((q_r - lofi_lp_r) * coef) >> 8;
    fx_l = lofi_lp_l;
    fx_r = lofi_lp_r;
    a = AMOUNT_MAX;
  } else if (m == FX_KARAOKE) {
    // センター定位(ボーカル)を除去。低音は残す
    int32_t side = (left - right) >> 1;
    int32_t bass = (bb_l + bb_r) >> 1;
    fx_l = side + bass;
    fx_r = side + bass;
  }

  l = left + (fx_l - left) * a / AMOUNT_MAX;
  r = right + (fx_r - right) * a / AMOUNT_MAX;
}

bool MultiEffectStream::begin() {
  buildFilterTable();
  // ディレイバッファは2本で合計64KB。WROVER で MALLOC_CAP_8BIT のみを指定すると
  // 外部 PSRAM に割り当てられ、オーディオタスクからのアクセスで不安定になるため
  // 内部 RAM を優先して確保する。
  buf_l = (int16_t *)heap_caps_calloc(DELAY_LEN, sizeof(int16_t),
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  buf_r = (int16_t *)heap_caps_calloc(DELAY_LEN, sizeof(int16_t),
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buf_l == nullptr) {
    buf_l = (int16_t *)heap_caps_calloc(DELAY_LEN, sizeof(int16_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (buf_r == nullptr) {
    buf_r = (int16_t *)heap_caps_calloc(DELAY_LEN, sizeof(int16_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (buf_l == nullptr || buf_r == nullptr) {
    Serial.println("[MultiEffectStream] delay buffer allocation failed");
    return false;
  }
  return true;
}

void MultiEffectStream::setMode(int m) {
  mode_value = m;
  clear_request = true;
}

void MultiEffectStream::setAmount(int a) {
  if (mode_value == FX_FILTER)
    filter_amount = a;
  else
    std_amount = a;
}

void MultiEffectStream::resetAmounts() {
  std_amount = 0;
  filter_amount = FILTER_CENTER;
}

int MultiEffectStream::mode() { return mode_value; }

int MultiEffectStream::amount() {
  if (mode_value == FX_FILTER) return filter_amount;
  return std_amount;
}

size_t MultiEffectStream::write(uint8_t d) { return out.write(d); }

size_t MultiEffectStream::write(const uint8_t *data, size_t size) {
  int m = mode_value;
  int a = amount();
  if (m == FX_OFF || buf_l == nullptr || buf_r == nullptr) {
    return out.write(data, size);
  }
  if (clear_request) {
    memset(buf_l, 0, DELAY_LEN * sizeof(int16_t));
    memset(buf_r, 0, DELAY_LEN * sizeof(int16_t));
    idx = 0;
    bb_l = bb_r = 0;
    hold_l = hold_r = 0;
    lofi_lp_l = lofi_lp_r = 0;
    met_lp_l = met_lp_r = 0;
    f_lp_l = f_lp_r = 0;
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
      process(l, r, m, a);
      tmp[k * 2] = clip16(l);
      tmp[k * 2 + 1] = clip16(r);
    }
    out.write((const uint8_t *)tmp, n * 4);
    done += n;
  }
  return size;
}
