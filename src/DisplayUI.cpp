#include "DisplayUI.h"

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

#include "App.h"
#include "config.h"

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

namespace {

// --- 曲名 (Bluetoothタスクから受け取り、loop側で表示) ---
portMUX_TYPE titleMux = portMUX_INITIALIZER_UNLOCKED;
char pending_title[65];
volatile bool title_pending = false;

char title[65] = "";
bool title_valid = false;
int scroll_off = 0;
int scroll_dir = 1;
uint32_t scroll_last_step = 0;
uint32_t scroll_wait_until = 0;
char last_top[LCD_COLS + 1];

}  // namespace

void initDisplay() {
  lcd.init();
  Wire.setClock(400000);  // LCD更新でエンコーダ読み取りが遅れないよう高速化
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Booting..");
}

void clearScreen() {
  lcd.clear();
  memset(last_top, ' ', LCD_COLS);
  last_top[LCD_COLS] = 0;
}

void resetTitle() {
  title[0] = 0;
  title_valid = false;
  scroll_off = 0;
  scroll_dir = 1;
  scroll_wait_until = millis() + SCROLL_PAUSE_MS;
  scroll_last_step = millis();
}

void queueTitle(const char *ascii_title) {
  portENTER_CRITICAL(&titleMux);
  strncpy(pending_title, ascii_title, sizeof(pending_title) - 1);
  pending_title[sizeof(pending_title) - 1] = 0;
  title_pending = true;
  portEXIT_CRITICAL(&titleMux);
}

// 下段: エフェクト名(左) + 強度%(右, 小数1桁)
void updateDisplay() {
  char line[LCD_COLS + 1];
  int amt = multiEffect.amount();
  snprintf(line, sizeof(line), "%-10s%3d.%d%%", MODE_NAMES[multiEffect.mode()],
           amt / 10, amt % 10);
  lcd.setCursor(0, 1);
  lcd.print(line);

  Serial.printf("Effect: %s | Intensity: %d.%d%%\n", MODE_NAMES[multiEffect.mode()],
                amt / 10, amt % 10);
}

// 上段: 曲名を左右にバウンススクロール (変化があった時だけ書き込む)
void updateTopLine(bool bluetooth_connected) {
  uint32_t now = millis();

  // 新しい曲名が届いていたら取り込む
  if (title_pending) {
    portENTER_CRITICAL(&titleMux);
    strncpy(title, pending_title, sizeof(title) - 1);
    title[sizeof(title) - 1] = 0;
    title_pending = false;
    portEXIT_CRITICAL(&titleMux);
    title_valid = title[0] != 0;
    scroll_off = 0;
    scroll_dir = 1;
    scroll_last_step = now;
    scroll_wait_until = now + SCROLL_PAUSE_MS;
  }

  const char *text;
  if (!bluetooth_connected)
    text = "Waiting BT...";
  else if (!title_valid)
    text = "BT Connected";
  else
    text = title;

  int len = strlen(text);

  if (len > LCD_COLS) {
    int max_off = len - LCD_COLS;
    if ((int32_t)(now - scroll_wait_until) >= 0 && now - scroll_last_step >= SCROLL_STEP_MS) {
      scroll_last_step = now;
      scroll_off += scroll_dir;
      if (scroll_off >= max_off) {
        scroll_off = max_off;
        scroll_dir = -1;
        scroll_wait_until = now + SCROLL_PAUSE_MS;
      } else if (scroll_off <= 0) {
        scroll_off = 0;
        scroll_dir = 1;
        scroll_wait_until = now + SCROLL_PAUSE_MS;
      }
    }
  } else {
    scroll_off = 0;
  }

  char win[LCD_COLS + 1];
  for (int i = 0; i < LCD_COLS; i++) {
    int p = scroll_off + i;
    win[i] = (p < len) ? text[p] : ' ';
  }
  win[LCD_COLS] = 0;

  if (memcmp(win, last_top, LCD_COLS) != 0) {
    lcd.setCursor(0, 0);
    lcd.print(win);
    memcpy(last_top, win, LCD_COLS + 1);
  }
}
