#include <cstring>

#include "App.h"
#include "Controls.h"
#include "DisplayUI.h"
#include "config.h"

I2SStream i2s;
MultiEffectStream multiEffect(i2s);
BluetoothA2DPSink a2dp_sink(multiEffect);
volatile int avrc_playback_status = -1;

namespace {

Button btnPlay = {IO_BTN_PLAY, HIGH, HIGH, 0, 0, false};
Button btnVolUp = {IO_BTN_VOL_UP, HIGH, HIGH, 0, 0, false};
Button btnVolDown = {IO_BTN_VOL_DOWN, HIGH, HIGH, 0, 0, false};
Button btnFxNext = {IO_MODE_BTN, HIGH, HIGH, 0, 0, false};
Button btnFxPrev = {IO_BTN_FX_PREV, HIGH, HIGH, 0, 0, false};
Button btnEncSw = {IO_ENC_SW, HIGH, HIGH, 0, 0, false};

void onPlaybackStatus(esp_avrc_playback_stat_t playback) {
  avrc_playback_status = static_cast<int>(playback);
  Serial.printf("[AVRCP] Playback status: %d\n", avrc_playback_status);
}

// BluetoothタスクからLCDを直接操作せず、メインループへタイトルを渡す。
void onMetadata(uint8_t id, const uint8_t *text) {
  if (id != 1) return;
  char tmp[65];
  size_t n = 0;
  for (const uint8_t *p = text; *p && n < sizeof(tmp) - 1; p++) {
    uint8_t c = *p;
    if (c >= 0x20 && c < 0x7F)
      tmp[n++] = static_cast<char>(c);
    else if (c >= 0xC0)
      tmp[n++] = '?';
  }
  tmp[n] = 0;
  queueTitle(tmp);
  Serial.printf("[Playing] Title : %s\n", (const char *)text);
}

}  // namespace

void changeEffect(int dir) {
  int m = (multiEffect.mode() + dir + MODE_COUNT) % MODE_COUNT;
  multiEffect.setMode(m);
  updateDisplay();
}

void changeVolume(int delta) {
  int v = a2dp_sink.get_volume() + delta;
  if (v < 0) v = 0;
  if (v > 127) v = 127;
  a2dp_sink.set_volume((uint8_t)v);
  Serial.printf("Volume: %d/127\n", v);
}

void togglePlayPause() {
  if (!a2dp_sink.is_connected()) {
    Serial.println("Play/Pause ignored: no Bluetooth device connected");
    return;
  }
  if (!a2dp_sink.is_avrc_connected()) {
    Serial.println("Play/Pause unavailable: AVRCP is not connected");
    return;
  }

  int status = avrc_playback_status;
  if (status < 0) {
    status = (a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED)
                 ? ESP_AVRC_PLAYBACK_PLAYING
                 : ESP_AVRC_PLAYBACK_PAUSED;
  }

  if (status == ESP_AVRC_PLAYBACK_PLAYING || status == ESP_AVRC_PLAYBACK_FWD_SEEK ||
      status == ESP_AVRC_PLAYBACK_REV_SEEK) {
    Serial.println("Play/Pause: sending PAUSE");
    a2dp_sink.pause();
  } else {
    Serial.println("Play/Pause: sending PLAY");
    a2dp_sink.play();
  }
}

void skipTrack(bool next) {
  if (!a2dp_sink.is_connected()) {
    Serial.println("No Bluetooth device connected");
    return;
  }
  if (next)
    a2dp_sink.next();
  else
    a2dp_sink.previous();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=========================================");

  initDisplay();

  multiEffect.begin();

  initEncoder();
  initButton(btnPlay);
  initButton(btnVolUp);
  initButton(btnVolDown);
  initButton(btnFxNext);
  initButton(btnFxPrev);
  initButton(btnEncSw);

  multiEffect.setMode(FX_OFF);
  multiEffect.resetAmounts();

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = IO_I2S_BCK;
  cfg.pin_ws = IO_I2S_LRCK;
  cfg.pin_data = IO_I2S_DOUT;
  i2s.begin(cfg);

  a2dp_sink.set_avrc_rn_playstatus_callback(onPlaybackStatus);
  a2dp_sink.set_avrc_metadata_callback(onMetadata);

  a2dp_sink.start("ESP32-MultiEffect", true);

  resetTitle();
  clearScreen();
  updateTopLine(a2dp_sink.is_connected());
  updateDisplay();
  Serial.println("=========================================");
}

void loop() {
  static bool was_connected = false;

  bool is_connected = a2dp_sink.is_connected();
  if (is_connected && !was_connected) {
    Serial.println("\n>>> Bluetooth デバイスが接続されました <<<");
    was_connected = true;
  } else if (!is_connected && was_connected) {
    Serial.println("\n>>> Bluetooth デバイスが切断されました <<<");
    was_connected = false;
    resetTitle();
  }

  pollEncoder();

  if (updateButton(btnFxNext, false) == BTN_SHORT) changeEffect(-1);
  if (updateButton(btnFxPrev, false) == BTN_SHORT) changeEffect(+1);

  if (updateButton(btnEncSw, false) == BTN_SHORT) {
    multiEffect.setMode(FX_OFF);
    multiEffect.resetAmounts();
    updateDisplay();
  }

  if (updateButton(btnPlay, false) == BTN_SHORT) togglePlayPause();

  BtnEvent up = updateButton(btnVolUp, true);
  if (up == BTN_SHORT)
    changeVolume(-VOLUME_STEP);
  else if (up == BTN_LONG)
    skipTrack(false);

  BtnEvent down = updateButton(btnVolDown, true);
  if (down == BTN_SHORT)
    changeVolume(+VOLUME_STEP);
  else if (down == BTN_LONG)
    skipTrack(true);

  int32_t diff = readEncoderSteps();
  if (diff != 0) {
    int a = multiEffect.amount() + (int)diff * ENC_DIR * AMOUNT_STEP;
    if (a < 0) a = 0;
    if (a > AMOUNT_MAX) a = AMOUNT_MAX;
    multiEffect.setAmount(a);
    updateDisplay();
  }

  updateTopLine(is_connected);

  delay(1);
}
