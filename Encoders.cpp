#include "Encoders.h"
#include "Config.h"
#include "McpInputs.h"

RotaryInput::RotaryInput(uint8_t a, uint8_t b, uint8_t sw, bool doubleClick)
  : a_(a), b_(b), sw_(sw), doubleClick_(doubleClick) {}

void RotaryInput::begin() {
  uint8_t pins = 0xFF;
  McpInputs::read(pins);
  // Test v1.0.2: A ist das niederwertige, B das hoeherwertige Bit.
  previous_ = ((pins >> b_) & 1U) << 1 | ((pins >> a_) & 1U);
  rawPressed_ = stablePressed_ = ((pins & (1U << sw_)) == 0);
  changedAt_ = millis();
  if (stablePressed_) pressedAt_ = millis();
}

void RotaryInput::update() {
  static const int8_t transitions[16] = {
     0, -1,  1,  0, 1, 0, 0, -1,
    -1,  0,  0,  1, 0, 1, -1, 0
  };
  uint8_t pins;
  if (!McpInputs::read(pins)) return;
  uint8_t current = (((pins >> b_) & 1U) << 1) | ((pins >> a_) & 1U);
  if (current != previous_) {
    // Im Hardwaretest war die physische Richtung invertiert.
    movement_ -= transitions[(previous_ << 2) | current];
    previous_ = current;
    if (movement_ >= 4) { ++steps_; movement_ = 0; }
    if (movement_ <= -4) { --steps_; movement_ = 0; }
  }

  const uint32_t now = millis();
  bool pressed = (pins & (1U << sw_)) == 0;
  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    changedAt_ = now;
  }
  if (pressed != stablePressed_ && now - changedAt_ >= Config::BUTTON_DEBOUNCE_MS) {
    stablePressed_ = pressed;
    if (pressed) {
      pressedAt_ = now;
      longSent_ = false;
    } else if (!longSent_) {
      if (!doubleClick_) {
        button_ = ButtonEvent::Short;
      } else if (shortPending_ && now - pendingAt_ <= Config::BUTTON_DOUBLE_MS) {
        shortPending_ = false;
        button_ = ButtonEvent::Double;
      } else {
        shortPending_ = true;
        pendingAt_ = now;
      }
    }
  }
  if (stablePressed_ && !longSent_ && now - pressedAt_ >= Config::BUTTON_LONG_MS) {
    longSent_ = true;
    shortPending_ = false;
    button_ = ButtonEvent::Long;
  }
  if (doubleClick_ && shortPending_ && !stablePressed_ &&
      now - pendingAt_ > Config::BUTTON_DOUBLE_MS) {
    shortPending_ = false;
    button_ = ButtonEvent::Short;
  }
}

int RotaryInput::takeSteps() { int s = steps_; steps_ = 0; return s; }
ButtonEvent RotaryInput::takeButton() { ButtonEvent e = button_; button_ = ButtonEvent::None; return e; }
