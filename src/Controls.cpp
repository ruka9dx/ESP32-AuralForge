#include "Controls.h"

namespace {

const int8_t ENC_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

uint8_t enc_state = 0;
int32_t enc_count = 0;
int32_t enc_last = 0;

}  // namespace

void initButton(Button &b) {
  pinMode(b.pin, INPUT_PULLUP);
  b.reading = digitalRead(b.pin);
  b.stable = b.reading;
}

BtnEvent updateButton(Button &b, bool allowLong) {
  uint32_t now = millis();
  int r = digitalRead(b.pin);
  if (r != b.reading) {
    b.reading = r;
    b.changed = now;
  }
  if (now - b.changed > DEBOUNCE_MS && r != b.stable) {
    b.stable = r;
    if (r == LOW) {
      b.pressed_at = now;
      b.long_fired = false;
      if (!allowLong) return BTN_SHORT;
    } else if (allowLong && !b.long_fired) {
      return BTN_SHORT;
    }
  } else if (allowLong && b.stable == LOW && !b.long_fired &&
             now - b.pressed_at >= LONG_PRESS_MS) {
    b.long_fired = true;
    return BTN_LONG;
  }
  return BTN_NONE;
}

void initEncoder() {
  pinMode(IO_ENC_A, INPUT_PULLUP);
  pinMode(IO_ENC_B, INPUT_PULLUP);
  enc_state = (uint8_t)((digitalRead(IO_ENC_A) << 1) | digitalRead(IO_ENC_B));
  enc_count = 0;
  enc_last = 0;
}

void pollEncoder() {
  uint8_t s = (uint8_t)((digitalRead(IO_ENC_A) << 1) | digitalRead(IO_ENC_B));
  if (s != (enc_state & 0x03)) {
    enc_state = (uint8_t)(((enc_state << 2) | s) & 0x0F);
    enc_count = enc_count + ENC_TABLE[enc_state];
  }
}

int32_t readEncoderSteps() {
  int32_t diff = (enc_count - enc_last) / COUNTS_PER_DETENT;
  if (diff != 0) {
    enc_last += diff * COUNTS_PER_DETENT;
  }
  return diff;
}
