#include "NerdWaveLog.h"
#include "NerdWaveNetwork.h"
#include "Config.h"
#include "NerdWaveWifiFilter.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <EthernetESP32.h>
#include <SPI.h>

namespace {
constexpr char WIFI_SETUP_AP[] = "Nerd-Wave-Setup";
ENC28J60Driver enc(Config::ETH_CS, Config::ETH_INT, Config::ETH_RST);
// WiFiManager akzeptiert fuer den AP-Callback keine Captures.
void (*setupPortalCallback)() = nullptr;
// ESP32-Core 3.x: Eventhandler nur einmal anmelden, auch bei Quellenwechseln.
bool wifiEventsRegistered = false;
bool allowLan=true;
bool activeLanUsable=false;
// Fehlgeschlagenes LAN nicht bei jedem Quellenwechsel erneut initialisieren.
// millis()-Vergleich ist auch bei Ueberlauf gueltig.
uint32_t lastLanFailure=0;
bool lanFailureKnown=false;
constexpr uint32_t LAN_RETRY_PAUSE_MS = 60000;
constexpr uint8_t LAN_BOOT_ATTEMPTS = 1; // Kein zweiter MAC-Init-Versuch auf fehlerhaftem Treiber

void wifiEventLogger(WiFiEvent_t event, WiFiEventInfo_t info) {
  if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    NerdWaveLog::output.printf("[WIFI-EVENT] STA getrennt: Reason=%u\n",
                  (unsigned)info.wifi_sta_disconnected.reason);
  } else if(event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    const uint8_t* b = info.wifi_sta_connected.bssid;
    NerdWaveLog::output.printf("[WIFI-EVENT] AP verbunden: BSSID=%02X:%02X:%02X:%02X:%02X:%02X Kanal=%u\n",
                  b[0],b[1],b[2],b[3],b[4],b[5],
                  (unsigned)info.wifi_sta_connected.channel);
  } else if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    NerdWaveLog::output.printf("[WIFI-EVENT] STA IP erhalten: %s\n",
                  IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
  }
}

void ensureWifiEvents() {
  if(wifiEventsRegistered) return;
  WiFi.onEvent(wifiEventLogger);
  wifiEventsRegistered = true;
}

// ENC28J60-Praesenztest, bevor EthernetESP32 einen MAC-Treiber installiert.
// Eine fehlerhafte MAC-Initialisierung kann einen unbrauchbaren Treiberzustand
// hinterlassen; in diesem Fall soll WLAN ohne Ethernet.begin() starten.
// EREVID = Bank 3, Adresse 0x12; ECON1 = bankunabhaengig 0x1F.
uint8_t encReadRegister(uint8_t reg) {
  digitalWrite(Config::ETH_CS, LOW);
  SPI.transfer(reg & 0x1f);              // RCR opcode 000xxxxx
  uint8_t value = SPI.transfer(0x00);
  digitalWrite(Config::ETH_CS, HIGH);
  return value;
}
void encBitField(uint8_t command, uint8_t bits) {
  digitalWrite(Config::ETH_CS, LOW);
  SPI.transfer(command);                  // BFS/BFC + ECON1
  SPI.transfer(bits);
  digitalWrite(Config::ETH_CS, HIGH);
}
bool encPresent() {
  pinMode(Config::ETH_CS, OUTPUT);
  digitalWrite(Config::ETH_CS, HIGH);
  pinMode(Config::ETH_RST, OUTPUT);
  digitalWrite(Config::ETH_RST, LOW);
  delay(2);
  digitalWrite(Config::ETH_RST, HIGH);
  delay(5);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  const uint8_t oldBank = encReadRegister(0x1f) & 0x03;
  encBitField(0xbf, 0x03);                // BFC ECON1.BSEL (Bank 0)
  encBitField(0x9f, 0x03);                // BFS ECON1.BSEL (Bank 3)
  const uint8_t revision = encReadRegister(0x12);
  encBitField(0xbf, 0x03);
  if(oldBank) encBitField(0x9f, oldBank);
  SPI.endTransaction();
  NerdWaveLog::output.printf("[LAN] SPI-Chippruefung EREVID=0x%02X\n", revision);
  // 0x00 / 0xFF sind typische Antworten bei fehlendem/nicht ansprechbarem Chip.
  // ENC28J60 hat gueltige Revisionskennungen im unteren Wertebereich.
  return revision >= 0x01 && revision <= 0x0f;
}

bool wifiHasAddress() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

bool connectLan(bool& lanReady) {
  activeLanUsable=false;
  lanReady=false;
  if(!allowLan){lanReady=false;return false;}
  if(lanFailureKnown && (uint32_t)(millis()-lastLanFailure)<LAN_RETRY_PAUSE_MS) {
    lanReady=false;
    NerdWaveLog::output.println("[LAN] Letzter Versuch fehlgeschlagen; Wiederholung vorerst ausgesetzt");
    return false;
  }
  if(!encPresent()) {
    lanFailureKnown=true;
    lastLanFailure=millis();
    NerdWaveLog::output.println("[LAN] Keine gueltige ENC28J60-Chip-ID; Ethernet-Treiber wird nicht gestartet");
    return false;
  }
  enc.setSpiFreq(10);
  Ethernet.init(enc);
  // Ein Startversuch statt einer unbeschraenkten Boot-Schleife.
  // Die PHY-/DHCP-Wartezeit wird von EthernetESP32 selbst bestimmt.
  for(uint8_t attempt=1; attempt<=LAN_BOOT_ATTEMPTS; ++attempt) {
    NerdWaveLog::output.printf("[LAN] ENC28J60 DHCP Versuch %u/%u ...\n",
                               attempt, LAN_BOOT_ATTEMPTS);
    lanReady = Ethernet.begin();
    if(lanReady && Ethernet.localIP()!=IPAddress(0,0,0,0)) {
      lanFailureKnown=false;
      activeLanUsable=true;
      NerdWaveLog::output.printf("[LAN] bereit IP=%s\n",Ethernet.localIP().toString().c_str());
      return true;
    }
    lanReady=false;
    NerdWaveLog::output.printf("[LAN] Versuch %u fehlgeschlagen\n",attempt);
    // Nach fehlgeschlagener PHY/MAC-Initialisierung NICHT erneut Ethernet.begin()
    // auf dem teilweise initialisierten Treiber aufrufen (Absturzgefahr).
    // Ein spaeterer Neuversuch erfolgt erst nach LAN_RETRY_PAUSE_MS.
    break;
  }
  lanFailureKnown=true;
  lastLanFailure=millis();
  NerdWaveLog::output.println("[LAN] Keine Verbindung; WLAN-Fallback wird versucht");
  return false;
}

bool connectWifi(bool& wifiReady, bool fallback = false) {
  ensureWifiEvents();
  // Bei einem Quellenwechsel keine bereits funktionierende STA-Verbindung anruehren.
  if(wifiHasAddress() && !NerdWaveWifiFilter::blocked(WiFi.BSSIDstr())) {
    wifiReady = true;
    WiFi.setSleep(false);
    NerdWaveLog::output.printf("[WLAN] Bestehende Verbindung weiterverwenden: IP=%s RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    NerdWaveNetwork::logWifiDetails("WLAN wiederverwendet");
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setHostname("nerd-wave");
  wifiManager.setConnectTimeout(20);
  wifiManager.setSaveConnectTimeout(20);
  wifiManager.setWiFiAutoReconnect(true);
  // Bei LAN-Fehlern darf auch der WiFiManager den Boot nicht endlos blockieren.
  // Im reinen WLAN-Betrieb bleibt der gewohnte offene Setup-AP bestehen.
  if(fallback) wifiManager.setConfigPortalTimeout(45);
  wifiManager.setAPCallback([](WiFiManager* wm) {
    NerdWaveLog::output.printf("[WLAN] Offener Setup-AP: %s http://%s/\n",wm->getConfigPortalSSID().c_str(),WiFi.softAPIP().toString().c_str());
    if(setupPortalCallback) setupPortalCallback();
  });
  NerdWaveLog::output.println("[WLAN] WiFiManager.autoConnect ...");
  wifiReady = wifiManager.autoConnect(WIFI_SETUP_AP) && WiFi.status()==WL_CONNECTED;
  if(wifiReady && NerdWaveWifiFilter::blocked(WiFi.BSSIDstr())) {
    NerdWaveLog::output.println("[WIFI-FILTER] WiFiManager traf gesperrten AP. Suche erlaubten AP ...");
    wifiReady=NerdWaveWifiFilter::connectAllowed();
  }
  if(wifiReady) WiFi.setSleep(false);
  NerdWaveLog::output.printf("[WLAN] %s IP=%s\n",wifiReady?"bereit":"Fehler",WiFi.localIP().toString().c_str());
  if(wifiReady) NerdWaveNetwork::logWifiDetails("WLAN verbunden");
  return wifiReady;
}
} // namespace

void NerdWaveNetwork::setLanEnabled(bool enabled){allowLan=enabled;}
bool NerdWaveNetwork::lanEnabled(){return allowLan;}
bool NerdWaveNetwork::lanUsable(){return activeLanUsable;}

bool NerdWaveNetwork::connect(uint8_t mode, bool& useWifi, bool& lanReady,
                             bool& wifiReady, void (*showSetupPortal)()) {
  setupPortalCallback = showSetupPortal;
  // Vor jeder Initialisierung konservativ WLAN als Clientroute setzen.
  // Im Fehlerfall darf kein EthernetClient auf einen defekten MAC zugreifen.
  useWifi=true;
  lanReady=false;
  wifiReady=wifiHasAddress();
  activeLanUsable=false;
  if(!allowLan){mode=1;lanReady=false;}
  bool connected=false;
  if(mode==1) {
    if(connectWifi(wifiReady)) {useWifi=true;connected=true;}
  } else {
    if(connectLan(lanReady)) {useWifi=false;connected=true;}
    else NerdWaveLog::output.println("[NET] LAN nicht verfuegbar; kein automatischer WLAN-Wechsel");
  }
  if(!connected) {
    useWifi=true; // garantiert: niemals EthernetClient ohne LAN-Initialisierung
    NerdWaveLog::output.println("[NET] Keine Verbindung; System startet trotzdem weiter (NAS offline)");
    return false;
  }
  NerdWaveLog::output.printf("[NET] Aktiv %s\n",useWifi?"WLAN":"LAN");
  return useWifi ? WiFi.status()==WL_CONNECTED : lanReady;
}

// Verwendet die bereits durch WiFiManager gespeicherte STA-Konfiguration.
// Kein autoConnect(): bei NAS-Ausfall darf kein offenes Konfigurationsportal starten.
bool NerdWaveNetwork::recoverWifi(uint32_t timeoutMs) {
  ensureWifiEvents();
  NerdWaveLog::output.printf("[NETREC] Vorher: Status=%d IP=%s Gateway=%s RSSI=%d dBm Heap=%u\n",
                (int)WiFi.status(), WiFi.localIP().toString().c_str(),
                WiFi.gatewayIP().toString().c_str(), WiFi.RSSI(), (unsigned)ESP.getFreeHeap());
  if(!NerdWaveWifiFilter::entries().isEmpty()) {
    const bool ok=NerdWaveWifiFilter::connectAllowed(timeoutMs);
    NerdWaveNetwork::logWifiDetails("WLAN Recovery mit Sperrliste");
    return ok;
  }
  WiFi.disconnect(false, false); // Funk eingeschaltet lassen, Zugangsdaten behalten.
  delay(150);
  WiFi.mode(WIFI_STA);
  NerdWaveLog::output.println("[NETREC] WLAN-Neuverbindung mit gespeicherter Konfiguration (ohne Portal)");
  WiFi.begin();
  const uint32_t started = millis();
  while(WiFi.status()!=WL_CONNECTED && (uint32_t)(millis()-started)<timeoutMs) {
    delay(100);
  }
  const bool connected = WiFi.status()==WL_CONNECTED;
  if(connected) WiFi.setSleep(false);
  NerdWaveLog::output.printf("[NETREC] Danach: verbunden=%d Status=%d IP=%s Gateway=%s RSSI=%d dBm Dauer=%lu ms\n",
                connected, (int)WiFi.status(), WiFi.localIP().toString().c_str(),
                WiFi.gatewayIP().toString().c_str(), WiFi.RSSI(),
                (unsigned long)(millis()-started));
  if(connected) NerdWaveNetwork::logWifiDetails("WLAN Recovery");
  return connected;
}

void NerdWaveNetwork::logWifiDetails(const char* context) {
  if(WiFi.status() != WL_CONNECTED) {
    NerdWaveLog::output.printf("[WIFI-DIAG] %s: nicht verbunden (Status=%d)\n", context, (int)WiFi.status());
    return;
  }
  const String bssid = WiFi.BSSIDstr();
  NerdWaveLog::output.printf("[WIFI-DIAG] %s: SSID=%s BSSID=%s Kanal=%d RSSI=%d dBm IP=%s\n",
                context, WiFi.SSID().c_str(), bssid.c_str(),
                (int)WiFi.channel(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
}
