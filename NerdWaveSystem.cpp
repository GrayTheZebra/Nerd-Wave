#include "NerdWaveLog.h"
#include "NerdWaveSystem.h"
#include "Config.h"
#include "NerdWaveVolume.h"
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>

namespace NerdWaveSystem {
void onboardRgbOff() {
  {
    Adafruit_NeoPixel onboardPixel(1, 48, NEO_GRB + NEO_KHZ800);
    onboardPixel.begin();
    onboardPixel.setBrightness(0);
    onboardPixel.setPixelColor(0, 0, 0, 0);
    onboardPixel.show();
    delay(2);
  }
  pinMode(48, INPUT);
  NerdWaveLog::output.println("[RGB] Onboard WS2812 GPIO48 ausgeschaltet");
}

void initHardwarePins() {
  digitalWrite(41, LOW);
  pinMode(41, OUTPUT);
  digitalWrite(41, LOW);
  NerdWaveLog::output.println("[AMP] SD GPIO41 LOW (Shutdown)");
  pinMode(Config::TFT_CS,OUTPUT);digitalWrite(Config::TFT_CS,HIGH);
  pinMode(Config::ETH_CS,OUTPUT);digitalWrite(Config::ETH_CS,HIGH);
  pinMode(Config::ETH_RST,OUTPUT);
  digitalWrite(Config::ETH_RST,LOW);
  delay(150);
  digitalWrite(Config::ETH_RST,HIGH);
  delay(500);
}

void showBootScreen(Adafruit_GC9A01A& tft) {
  tft.setTextWrap(false);
  tft.setTextColor(0xFEC0, 0x0000);
  tft.setTextSize(3);
  tft.setCursor(31, 68); tft.print("NERD-WAVE");
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(75, 108); tft.print(Config::VERSION);
  tft.setCursor(58, 137); tft.print("Starte System...");
}

void renderWifiSetup(Adafruit_GC9A01A& tft) {
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF); tft.setTextSize(2);
  tft.setCursor(34,70); tft.print("WLAN-SETUP");
  tft.setTextSize(1); tft.setCursor(54,110); tft.print("Nerd-Wave-Setup");
  tft.setCursor(61,132); tft.print("192.168.4.1");
}

void migrateLegacyTrack(Preferences& prefs) {
  const String legacyPath=prefs.getString("lastPath","");
  const String legacyName=prefs.getString("lastName","");
  const uint8_t legacySource=prefs.getUChar("lastSrc",0);
  if(legacyPath.length() && legacySource<2) {
    const char* pathKey=legacySource==1 ? "usbPath" : "nasPath";
    const char* nameKey=legacySource==1 ? "usbName" : "nasName";
    if(!prefs.getString(pathKey,"").length()) {
      prefs.putString(pathKey,legacyPath);
      prefs.putString(nameKey,legacyName);
      NerdWaveLog::output.printf("[NVS] Alter letzter Titel fuer %s uebernommen\n",legacySource==1?"USB":"NAS");
    }
    prefs.remove("lastPath");prefs.remove("lastName");prefs.remove("lastSrc");
  }
}

void performReset(Preferences& prefs, bool factoryReset,
                  bool& repeatAll, uint8_t& shuffleMode,
                  bool& autoStartLast, uint8_t& networkMode,
                  String& lastTrackPath, String& lastTrackName) {
  NerdWaveLog::output.printf("[RESET] %s: starte Reset\n", factoryReset?"WERK":"WLAN");
  digitalWrite(41, LOW);
  WiFiManager manager;
  manager.resetSettings();
  NerdWaveLog::output.println("[RESET] WiFiManager resetSettings aufgerufen");
  if(factoryReset) {
    prefs.clear();
    repeatAll=false;
    shuffleMode=0;
    autoStartLast=false;
    networkMode=0;
    lastTrackPath="";
    lastTrackName="";
    NerdWaveLog::output.println("[RESET] Nerd-Wave Einstellungen geloescht");
  }
  if(factoryReset) NerdWaveVolume::discardPendingSave();
  else NerdWaveVolume::saveIfDirty(prefs);
  prefs.end();
  NerdWaveLog::output.println("[RESET] Neustart ...");
  Serial.flush();
  delay(350);
  ESP.restart();
}
} // namespace NerdWaveSystem
