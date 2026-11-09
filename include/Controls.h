#pragma once

#include <Arduino.h>

#include "config.h"

// --- ボタン ---
void initButton(Button &b);
BtnEvent updateButton(Button &b, bool allowLong);

// --- ロータリーエンコーダ ---
void initEncoder();
void pollEncoder();

// 前回呼び出しからの回転量を「ノッチ数」で返す (呼ぶとカウンタが進む)
int32_t readEncoderSteps();
