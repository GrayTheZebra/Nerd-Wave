#pragma once
#include <Arduino.h>

struct RandomPick {
  uint32_t count = 0;
  String path, name;
  uint16_t folders = 0;
  bool failed = false;
};
