#pragma once
#include <Arduino.h>

namespace Config {
constexpr const char* VERSION = "v1.0.0";
constexpr uint8_t SPI_MOSI = 11, SPI_MISO = 13, SPI_SCLK = 12;
constexpr uint8_t TFT_CS = 10, TFT_DC = 8, TFT_RST = 7;
constexpr uint8_t POWER_SWITCH_PIN = 21; // Rastschalter gegen GND: LOW=EIN, HIGH=AUS
constexpr uint8_t IR_PIN = 2;  // Y: IR Signal; GC9A01 ohne Backlight-Pin
constexpr uint8_t ETH_CS = 9, ETH_INT = 5, ETH_RST = 4;
// MCP23017 am I2C Bus; bisherige Encoder-GPIOs sind danach frei.
constexpr uint8_t SD_CLK = 16, SD_CMD = 17, SD_D0 = 18; // SD_MMC 1-Bit
constexpr uint8_t MCP_SDA = 14, MCP_SCL = 15, MCP_ADDRESS = 0x20;
constexpr uint8_t NAV_A = 0, NAV_B = 1, NAV_SW = 2; // GPA0..2
constexpr uint8_t VOL_A = 3, VOL_B = 4, VOL_SW = 5; // GPA3..5
constexpr uint32_t DISPLAY_SPI_DIAG_HZ = 20000000; // Diagnose: nur GC9A01, nicht ENC28J60
constexpr uint16_t DISPLAY_WIDTH = 240, DISPLAY_HEIGHT = 240;
constexpr uint16_t PAGE_SIZE = 25;
constexpr uint16_t MAX_TOTAL = 10000;
constexpr uint32_t BUTTON_LONG_MS = 800;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 25;
constexpr uint32_t BUTTON_DOUBLE_MS = 330;
constexpr uint32_t UI_REFRESH_MS = 100;
}
