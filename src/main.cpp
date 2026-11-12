#include <cstring>

#include <Preferences.h>

#include "App.h"
#include "Controls.h"
#include "DisplayUI.h"
#include "config.h"

I2SStream i2s;
MultiEffectStream multiEffect(i2s);
BluetoothA2DPSink a2dp_sink(multiEffect);
volatile int avrc_playback_status = -1;

namespace {

constexpr uint8_t DEFAULT_VOLUME = 64;

Preferences volumePreferences;
bool volume_preferences_ready = false;
uint8_t saved_volume = DEFAULT_VOLUME;
volatile bool volume_restore_complete = false;
volatile int pending_remote_volume = -1;

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

void onRemoteVolumeChanged(int volume) {
  if (volume_restore_complete && volume >= 0 && volume <= 127) {
    pending_remote_volume = volume;
  }
}

void saveVolume(uint8_t volume) {
  saved_volume = volume;
  if (!volume_preferences_ready) return;

  if (volumePreferences.putUChar("volume", volume) != sizeof(volume)) {
    Serial.println("[Volume] Failed to save volume to NVS");
  }
}

void loadVolume() {
  volume_preferences_ready = volumePreferences.begin("auralforge", false);
  if (!volume_preferences_ready) {
    Serial.println("[Volume] Failed to open NVS; using default volume");
    return;
  }

  if (volumePreferences.isKey("volume")) {
    saved_volume = volumePreferences.getUChar("volume", DEFAULT_VOLUME);
    if (saved_volume > 127) {
      saved_volume = 127;
      saveVolume(saved_volume);
      Serial.println("[Volume] Saved volume was out of range; clamped to 127");
    } else {
      Serial.printf("[Volume] Loaded %u/127\n", saved_volume);
    }
  } else {
    saved_volume = DEFAULT_VOLUME;
    Serial.printf("[Volume] No saved volume; using %u/127\n", saved_volume);
  }
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
  if (multiEffect.isMultiMode()) {
    multiEffect.selectMultiEffect(dir);
    updateDisplay();
    return;
  }

  int m = (multiEffect.mode() + dir + MODE_COUNT) % MODE_COUNT;
  multiEffect.setMode(m);
  updateDisplay();
}

void changeVolume(int delta) {
  int v = a2dp_sink.get_volume() + delta;
  if (v < 0) v = 0;
  if (v > 127) v = 127;
  a2dp_sink.set_volume((uint8_t)v);
  saveVolume((uint8_t)v);
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

  loadVolume();

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
  a2dp_sink.set_avrc_rn_volumechange(onRemoteVolumeChanged);
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
  static bool effect_combo_handled = false;
  static bool was_avrc_connected = false;
  static bool volume_restore_done = false;
  static uint32_t avrc_connected_at = 0;

  bool is_connected = a2dp_sink.is_connected();
  bool is_avrc_connected = a2dp_sink.is_avrc_connected();
  uint32_t now = millis();

  if (is_avrc_connected && !was_avrc_connected) {
    avrc_connected_at = now;
    volume_restore_done = false;
    volume_restore_complete = false;
  } else if (!is_avrc_connected && was_avrc_connected) {
    volume_restore_complete = false;
  }
  was_avrc_connected = is_avrc_connected;

  if (is_avrc_connected && !volume_restore_done &&
      now - avrc_connected_at >= 1000) {
    a2dp_sink.set_volume(saved_volume);
    volume_restore_complete = true;
    volume_restore_done = true;
    Serial.printf("[Volume] Restored %u/127\n", saved_volume);
  }

  int remote_volume = pending_remote_volume;
  if (remote_volume >= 0) {
    pending_remote_volume = -1;
    saveVolume((uint8_t)remote_volume);
    Serial.printf("[Volume] Saved remote volume %d/127\n", remote_volume);
  }

  if (is_connected && !was_connected) {
    Serial.println("\n>>> Bluetooth デバイスが接続されました <<<");
    was_connected = true;
  } else if (!is_connected && was_connected) {
    Serial.println("\n>>> Bluetooth デバイスが切断されました <<<");
    was_connected = false;
    resetTitle();
  }

  pollEncoder();

  BtnEvent fxNext = updateButton(btnFxNext, true);
  BtnEvent fxPrev = updateButton(btnFxPrev, true);
  bool bothFxButtonsPressed =
      btnFxNext.stable == LOW && btnFxPrev.stable == LOW;
  uint32_t now = millis();

  if (bothFxButtonsPressed && !effect_combo_handled &&
      now - btnFxNext.pressed_at >= LONG_PRESS_MS &&
      now - btnFxPrev.pressed_at >= LONG_PRESS_MS) {
    multiEffect.setMultiMode(!multiEffect.isMultiMode());
    effect_combo_handled = true;
    Serial.printf("Multi-effect mode: %s\n",
                  multiEffect.isMultiMode() ? "ON" : "OFF");
    updateDisplay();
  }

  if (!bothFxButtonsPressed && btnFxNext.stable == HIGH &&
      btnFxPrev.stable == HIGH) {
    effect_combo_handled = false;
  }

  if (!effect_combo_handled) {
    if (fxNext == BTN_SHORT ||
        (fxNext == BTN_LONG && !bothFxButtonsPressed)) {
      changeEffect(-1);
    }
    if (fxPrev == BTN_SHORT ||
        (fxPrev == BTN_LONG && !bothFxButtonsPressed)) {
      changeEffect(+1);
    }
  }

  if (updateButton(btnEncSw, false) == BTN_SHORT) {
    if (multiEffect.isMultiMode()) {
      multiEffect.resetMultiAmounts();
    } else {
      multiEffect.setMode(FX_OFF);
      multiEffect.resetAmounts();
    }
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
