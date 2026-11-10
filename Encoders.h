#pragma once
#include <Arduino.h>

enum class ButtonEvent : uint8_t { None, Short, Long, Double };

class RotaryInput {
public:
  RotaryInput(uint8_t a, uint8_t b, uint8_t sw, bool doubleClick = false);
  void begin();
  void update();
  int takeSteps();
  ButtonEvent takeButton();
private:
  uint8_t a_, b_, sw_, previous_ = 0;
  int8_t movement_ = 0;
  int steps_ = 0;
  bool rawPressed_ = false, stablePressed_ = false, longSent_ = false;
  uint32_t changedAt_ = 0, pressedAt_ = 0, pendingAt_ = 0;
  bool doubleClick_ = false, shortPending_ = false;
  ButtonEvent button_ = ButtonEvent::None;
};
