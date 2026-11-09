#pragma once

#include <Arduino.h>

// --- LCD 表示 ---
void initDisplay();  // LCD初期化 + 起動メッセージ
void clearScreen();  // 画面消去 + 上段キャッシュ初期化
void resetTitle();   // 曲名状態をリセット
void queueTitle(const char *ascii_title);  // BTタスクから曲名を渡す
void updateTopLine(bool bluetooth_connected);  // 上段: 曲名スクロール
void updateDisplay();                          // 下段: エフェクト名 + 強度
