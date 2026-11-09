#pragma once

#include <AudioTools.h>
#include <BluetoothA2DPSink.h>

#include "MultiEffectStream.h"
#include "config.h"

// main.cpp で定義しているグローバルオブジェクト
extern I2SStream i2s;
extern MultiEffectStream multiEffect;
extern BluetoothA2DPSink a2dp_sink;
extern volatile int avrc_playback_status;

// --- 操作系 (main.cpp) ---
void changeEffect(int dir);
void changeVolume(int delta);
void togglePlayPause();
void skipTrack(bool next);
