#pragma once
#include <Arduino.h>

// Reine Textaufbereitung, ohne Display- oder Playerzustand.
String lcdText(const String& s, size_t maxChars);
String displayTitle(const String& original);
