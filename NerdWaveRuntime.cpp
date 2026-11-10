#include "NerdWaveLog.h"
#include <Arduino.h>
#include <WiFi.h>
#include <EthernetESP32.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <EspUsbHost.h>
#include "Config.h"
#include "NerdWaveNetwork.h"
#include "NerdWaveWifiFilter.h"
#include "NerdWaveBtUart.h"
#include "NerdWaveBtAudio.h"
#include "NerdWavePcmAudio.h"
#include "NerdWavePlayback.h"
#include "NerdWaveMp3Stream.h"
#include "NerdWaveSdCard.h"
#include "NerdWaveUsb.h"
#include "CopyManager.h"
#include "NerdWaveSources.h"
#include "SourceMetadata.h"
#include "NerdWavePlayerControl.h"
#include "NerdWaveVolume.h"
#include "NerdWaveSettings.h"
#include "NerdWaveNasConfig.h"
#include "NerdWaveWeb.h"
#include "NerdWaveMqtt.h"
#include "NerdWaveSystem.h"
#include "NerdWaveLoopMaintenance.h"
#include "NerdWaveRecovery.h"
#include "NerdWaveUiControls.h"
#include "NerdWaveCopyUi.h"
#include "NerdWaveViewState.h"
#include "NerdWaveCopyProgress.h"
#include "NerdWaveCopyEngine.h"
#include "NerdWaveDisplayController.h"
#include "NerdWaveSkins.h"
#include "NerdWaveFields.h"
#include "DisplayUI.h"
#include "NerdWaveMenus.h"
#include "InputManager.h"
using DisplayUI::centeredText;
using DisplayUI::centeredPlayerTitle;
using DisplayUI::updateArc;
using DisplayUI::drawWifiIcon;
using DisplayUI::drawRepeatIcon;
using DisplayUI::drawShuffleIcon;
using namespace InputManager;
#include "LedController.h"
#include "NerdWaveRuntime.h"
#include "Encoders.h"
#include "McpInputs.h"
#include "MusicApi.h"
#include "Browser.h"
#include "NerdWaveTrackCatalog.h"
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "driver/i2s.h"


// Nerd-Wave: runtime coordinates hardware, sources and UI modules.
// Version history is maintained in the project documentation.

// ============== 01_ConnectivityAndState ==============

// WLAN-Zugangsdaten verwaltet WiFiManager im ESP32 (kein Passwort im Sketch).
// Setup-AP-Name wird im NerdWaveNetwork verwaltet.
// Einrichtungs-Hotspot wird im Netzwerkmodul ohne Passwort gestartet.
// Netzwerk: 0 LAN, 1 WLAN (ohne automatische Umschaltung).
static uint8_t networkMode=0;
// Zugriff ausschliesslich per vorab gewaehlter Schnittstelle, auch bei Browser-Requests.
bool nerdUseWifi=false;
static bool lanReady=false, wifiReady=false;
static bool networkRestartPending=false;
static uint8_t bootNetworkMode=2;
// Quelle: 0 NAS, 1 USB, 2 Bluetooth, 3 SD; Netzmodus separat gespeichert.
// Quellenzustand und NVS-Quellenwahl liegen im Modul NerdWaveSources.
// Referenzen bewahren die bisherigen Aufrufstellen und den Ablauf im loopTask.
static uint8_t& musicSource = NerdWaveSources::current;
uint8_t nerdMusicSource() { return musicSource; }
static bool& sourceSwitchPending = NerdWaveSources::switchPending;
static uint8_t& requestedMusicSource = NerdWaveSources::requested;
// Host, Massenspeicher und Zustandsflags leben im USB-Modul.
// Aliase bewahren die existierenden Kontrollpfade einschliesslich Hot-Plug.
using NerdWaveUsb::usbDisk;
static bool& usbHostStarted = NerdWaveUsb::hostStarted;
static bool& usbPreviouslyMounted = NerdWaveUsb::previouslyMounted;
static bool& usbRemovalPending = NerdWaveUsb::removalPending;
static bool& usbBrowserRefreshPending = NerdWaveUsb::browserRefreshPending;
static bool& usbEverMounted = NerdWaveUsb::everMounted;
// SD-Status liegt im NerdWaveSdCard-Modul.
// Referenz-Aliase erhalten alle bestehenden Kontrollpfade unveraendert.
static bool& sdMounted = NerdWaveSdCard::mounted;
static bool& sdRecoveryPending = NerdWaveSdCard::recoveryPending;
static uint32_t& sdRetryAt = NerdWaveSdCard::retryAt;
static bool sdAutostartPending=false;
static bool sdRemovalPending=false;
bool nerdSdMounted() { return sdMounted; }
static bool sdTryMount() { return NerdWaveSources::enabled(3) && NerdWaveSdCard::tryMount(); }

static bool usbStartHost() { return NerdWaveSources::enabled(1) && NerdWaveUsb::startHost(); }
static bool usbTryMount() { return NerdWaveSources::enabled(1) && NerdWaveUsb::tryMount(); }
static bool usbIsMounted() { return usbDisk.mounted(); }


static String selectedMp3Path = "";
static volatile bool stopRequested = false;
static volatile bool decoderRunning = false, playerRunning = false;
static volatile bool playerActive = false;
static bool startingTrack = false;
static volatile bool& pauseRequested = NerdWavePlayerControl::pauseRequested;
static volatile bool playbackPaused = false;
// Abschlussmeldung der Audio-Task: Der Haupt-Loop startet den naechsten Titel.
static volatile bool autoNextPending = false;
static volatile bool autoNextEnabled = true;
static Preferences prefs;
static bool& repeatAll = NerdWavePlayerControl::repeatAll;
// 0 = Aus, 1 = aktueller Ordner, 2 = aktueller Ordner mit Unterordnern
static uint8_t& shuffleMode = NerdWavePlayerControl::shuffleMode;
static bool autoStartLast = false;
static String lastTrackPath, lastTrackName;
static String playingFolder = "/";
static bool settingsLoaded = false;


// 100 = Original-Amplitude; 20 = 20 % Amplitude (ca. -14 dB).
// Lautstaerkezustand und NVS-Speicherung liegen im NerdWaveVolume-Modul.
// Referenzen erhalten die bisherigen direkten Lesezugriffe im Audiopfad.
static volatile int& volumePercent = NerdWaveVolume::percent;
static bool& volumeDirty = NerdWaveVolume::dirty;
static uint32_t& volumeChangedAt = NerdWaveVolume::changedAt;
static volatile bool& muted = NerdWaveVolume::muted;
static void saveVolumeNow() { NerdWaveVolume::saveIfDirty(prefs); }
// UART-Protokoll und WROOM-Status liegen im Bluetooth-UART-Modul.
// Bluetooth-I2S-Empfang, PCM-Puffer, Ausgabetask und Zaehler: NerdWaveBtAudio.
using NerdWaveBtUart::BT_UART_RX;
using NerdWaveBtUart::BT_UART_TX;
using NerdWaveBtUart::btLink;
using NerdWaveBtUart::btConnected;
using NerdWaveBtUart::btPlaying;
using NerdWaveBtUart::btPairing;
using NerdWaveBtUart::btReady;
using NerdWaveBtUart::btUiDirty;
using NerdWaveBtUart::btUartStarted;
using NerdWaveBtUart::btSend;
using NerdWaveBtUart::btPoll;
using NerdWaveBtAudio::btStopRequested;
using NerdWaveBtAudio::btTaskRunning;
using NerdWaveBtAudio::btRxRunning;
using NerdWaveBtAudio::btAudioStartAt;
using NerdWaveBtAudio::BT_RETRY_INTERVAL_MS;
using NerdWaveBtAudio::btRxBytes;
using NerdWaveBtAudio::btTxBytes;
using NerdWaveBtAudio::btI2sErrors;
using NerdWaveBtAudio::btUnderruns;
using NerdWaveBtAudio::btOverflows;
using NerdWaveBtAudio::btStarves;
using NerdWaveBtAudio::btBufferLevel;

// MP3-Abtastrate wird beim ersten gueltigen Frame erkannt.
// 48 kHz bleibt der Default, bis der Decoder den Takt festgelegt hat.
static volatile uint32_t audioSampleRate = 48000;
static volatile bool audioSampleRateReady = false;
// MAX98357A SD: LOW = Shutdown, HIGH = aktiv (bei ueblichem Breakout).
constexpr uint8_t PIN_AMP_SD = 41;
static inline void ampOff() { digitalWrite(PIN_AMP_SD, LOW); }
static inline void ampOn()  { digitalWrite(PIN_AMP_SD, HIGH); }




// ============== 02_LedsAndSpectrum ==============
// WS2812B an GPIO1: LED-Steuerung bleibt im loopTask, kein Zugriff aus Audio-Tasks.
// Die FFT- und NeoPixel-Implementierung liegt jetzt in LedController.cpp.
static uint8_t ledSettingsIndex=0;
static bool ledEditing=false;
static bool irLedToggle=false,irLedNext=false;


// ============== 03_AudioBuffersAndI2S ==============
static volatile bool decodeDone = false, decodeError = false;
static uint64_t framesDecoded = 0, bytesPlayed = 0;
static volatile uint32_t webSeekStartSeconds = 0;
static volatile bool webSeeking = false;
static volatile uint32_t webSeekDisplayDuration = 0;
static volatile uint32_t playbackBaseSeconds = 0;
static uint32_t underruns = 0, i2sErrors = 0, maxI2sWriteUs = 0;
static uint32_t netStalls = 0;
// Dauer: bevorzugt Xing/Info/VBRI, danach CBR oder eingefrorene Schaetzung.
static volatile uint32_t estimatedDurationSec = 0;
static volatile bool durationExact = false;
// Binding fuer den ausgelagerten Decoder; referenziert ausschliesslich
// die bereits existierenden Runtime-Zustandsvariablen.
static NerdWaveMp3Stream::Context mp3StreamContext{nerdUseWifi, musicSource, sdMounted, selectedMp3Path, stopRequested, decoderRunning, decodeError, decodeDone, audioSampleRate, audioSampleRateReady, framesDecoded, netStalls, estimatedDurationSec, durationExact, webSeekStartSeconds};

// PCM-Ring und Legacy-I2S befinden sich in NerdWavePcmAudio.
static bool& nasI2SInstalled = NerdWavePcmAudio::nasI2SInstalled;
static size_t ringLevel() { return NerdWavePcmAudio::level(); }
// Signatur bleibt erhalten: NerdWaveBtAudio verwendet diese Callback-Schnittstelle.
static bool configureLegacyI2S(i2s_port_t port, bool input, uint32_t rate) {
  return NerdWavePcmAudio::configureLegacyI2S(port, input, rate);
}


// ============== 05_InputAndBrowserState ==============
// UI und Browser bleiben im Arduino-loopTask. Die Audio-Tasks laufen separat.
static Adafruit_GC9A01A tft(Config::TFT_CS, Config::TFT_DC, Config::TFT_RST);
// Startup-Livelog fuer GC9A01 (nur in setup aktiv, nicht aus Worker-Tasks).
// Das Protokoll zeigt die letzten Zeilen, ohne den Player-Renderer anzufassen.
static char bootLines[7][37] = {};
static uint8_t bootLineCount=0;
static void centeredBootText(const char* label,int16_t y,uint8_t size,uint16_t color) {
  int16_t x1,y1;uint16_t w,h;
  tft.setTextWrap(false);tft.setTextSize(size);tft.setTextColor(color);
  tft.getTextBounds(label,0,0,&x1,&y1,&w,&h);
  tft.setCursor((240-(int)w)/2-x1,y);tft.print(label);
}
static void drawBootLog() {
  // Im Kreis nur eine gut lesbare zentrale Spalte verwenden.
  tft.fillRect(22,72,196,110,0x0000);
  tft.setTextWrap(false);tft.setTextSize(1);
  for(uint8_t i=0;i<bootLineCount;i++) {
    tft.setTextColor(i+1==bootLineCount?0x07FF:0xBDF7);
    tft.setCursor(25,77+i*15);tft.print(bootLines[i]);
  }
}
static void bootLineOnDisplay(const char* line) {
  if(!line || !line[0])return;
  // Maximal 31 Zeichen pro Zeile bei Standard-GFX-Font.
  const char* p=line;
  while(*p==' ')p++;
  if(strlen(p)>31) {
    // Prioritaet auf die Statusaussage, nicht auf lange Pfade.
    char shortLine[35];memcpy(shortLine,p,28);shortLine[28]='.';shortLine[29]='.';shortLine[30]='.';shortLine[31]='\0';
    p=shortLine;
    if(bootLineCount==7) {for(uint8_t i=1;i<7;i++)strcpy(bootLines[i-1],bootLines[i]);bootLineCount=6;}
    strncpy(bootLines[bootLineCount++],p,36);bootLines[bootLineCount-1][36]='\0';
  } else {
    if(bootLineCount==7) {for(uint8_t i=1;i<7;i++)strcpy(bootLines[i-1],bootLines[i]);bootLineCount=6;}
    strncpy(bootLines[bootLineCount++],p,36);bootLines[bootLineCount-1][36]='\0';
  }
  drawBootLog();
}
static void beginBootLog() {
  tft.fillScreen(0x0000);
  centeredBootText("Nerd-Wave",34,2,0x07FF);
  centeredBootText(Config::VERSION,54,1,0xFFFF);
  // Fruehe Meldungen seit Serial.begin nachtragen.
  uint32_t next=0;bool truncated=false;
  String previous=NerdWaveLog::read(0,4096,next,truncated);
  int from=0;
  for(int i=0;i<(int)previous.length();i++) {
    if(previous[i]=='\n') {
      String line=previous.substring(from,i);line.trim();
      if(line.length())bootLineOnDisplay(line.c_str());
      from=i+1;
    }
  }
  bootLineOnDisplay("[BOOT] Display bereit");
  NerdWaveLog::setBootLineCallback(bootLineOnDisplay);
}
static void endBootLog() {
  NerdWaveLog::setBootLineCallback(nullptr);
}

// Display-Diagnose via USB-Serial: "display 10", "display 20", "display 40", "display status".
// Adafruit_SPITFT verwaltet seine eigenen SPI-Transaktionen; Ethernet bleibt unveraendert.
static uint32_t displaySpiHz = Config::DISPLAY_SPI_DIAG_HZ;
static char displayDiagCommand[32] = {};
static uint8_t displayDiagLength = 0;
static void pollDisplayDiagnostics() {
  // Nur kurze, feste Kommandos. Kein String/Heap im seriellen Eingabepfad.
  uint8_t budget = 32;
  while (budget-- && Serial.available() > 0) {
    const int incoming = Serial.read();
    if (incoming < 0) break;
    const char ch = (char)incoming;
    if (ch == '\r' || ch == '\n') {
      if (!displayDiagLength) continue;
      displayDiagCommand[displayDiagLength] = '\0';
      displayDiagLength = 0;
      NerdWaveLog::output.printf("[DISPLAY-DIAG] Kommando empfangen: %s\n", displayDiagCommand);
      if (strcmp(displayDiagCommand, "display status") == 0) {
        NerdWaveLog::output.printf("[DISPLAY-DIAG] SPI=%lu MHz; Laufzeit-Umschaltung aktiv\n",
                                   (unsigned long)(displaySpiHz / 1000000UL));
      } else {
        uint32_t mhz = 0;
        if (strcmp(displayDiagCommand, "display 10") == 0) mhz = 10;
        else if (strcmp(displayDiagCommand, "display 20") == 0) mhz = 20;
        else if (strcmp(displayDiagCommand, "display 40") == 0) mhz = 40;
        if (mhz) {
          const uint32_t nextHz = mhz * 1000000UL;
          if (nextHz != displaySpiHz) {
            NerdWaveLog::output.printf("[DISPLAY-DIAG] SPI-Wechsel BEGIN %lu -> %lu MHz\n",
                                       (unsigned long)(displaySpiHz / 1000000UL), (unsigned long)mhz);
            // Wird nur im Arduino-loopTask ausgefuehrt, nach dem Rendern.
            // Displayseitige SPI-Settings aktualisieren; Netzwerk-Einstellungen unberuehrt.
            tft.setSPISpeed(nextHz);
            displaySpiHz = nextHz;
            NerdWaveLog::output.println("[DISPLAY-DIAG] SPI-Wechsel OK");
          } else {
            NerdWaveLog::output.println("[DISPLAY-DIAG] SPI-Takt bereits eingestellt");
          }
        } else {
          NerdWaveLog::output.println("[DISPLAY-DIAG] Befehle: display 10 | display 20 | display 40 | display status");
        }
      }
    } else if (ch >= 32 && ch <= 126) {
      if (displayDiagLength < sizeof(displayDiagCommand) - 1) {
        displayDiagCommand[displayDiagLength++] = ch;
      } else {
        displayDiagLength = 0;
        NerdWaveLog::output.println("[DISPLAY-DIAG] Kommando zu lang, verworfen");
      }
    }
  }
}

static RotaryInput nav(Config::NAV_A, Config::NAV_B, Config::NAV_SW, true);
static RotaryInput volEncoder(Config::VOL_A, Config::VOL_B, Config::VOL_SW, true);
static MusicApi api;
static MusicBrowser browser(api);
static int page=0; // 0=Browser, 1=Player, 2=Diagnose, 3=Einstellungen, 6=Medienverwaltung
static int& settingsIndex = NerdWaveMenus::settingsIndex;
static int& mediaIndex = NerdWaveMenus::mediaIndex;
static int& mediaReturnPage = NerdWaveMenus::mediaReturnPage;
static bool& mediaInfo = NerdWaveMenus::mediaInfo;
static bool& mediaNotice = NerdWaveMenus::mediaNotice; // Ergebnisanzeige
static int& mediaCopyMode = NerdWaveMenus::mediaCopyMode; // 0=Song auswaehlen, 1=Ordner, 2=aktueller Song
static int& mediaStage = NerdWaveMenus::mediaStage; // 0=Hauptmenue, 1=Quelle, 2=Dateiauswahl, 3=Ziel, 4=Ergebnis
static int& mediaChoice = NerdWaveMenus::mediaChoice;
static int& mediaOrigin = NerdWaveMenus::mediaOrigin; // 0=NAS, 1=USB, 3=SD
static int& mediaDestination = NerdWaveMenus::mediaDestination; // 1=USB, 3=SD
static String& mediaPath = NerdWaveMenus::mediaPath;
static String& mediaChosenPath = NerdWaveMenus::mediaChosenPath;
static String& mediaChosenName = NerdWaveMenus::mediaChosenName;
static String& mediaResult = NerdWaveMenus::mediaResult;
static MusicPage& mediaList = NerdWaveMenus::mediaList;
static int& mediaSelection = NerdWaveMenus::mediaSelection;
static bool& mediaListReady = NerdWaveMenus::mediaListReady;
static volatile bool mediaAbort=false;
// Kopier-Worker: 0=bereit, 1=aktiv, 2=fertig. Nur loop() steuert GUI/Encoder.
// Taskstatus und Fortschrittsdaten befinden sich in CopyManager.cpp.
static bool copyResumeAfter=false;
static String copyResumePath,copyResumeName;


static int& settingsReturnPage = NerdWaveMenus::settingsReturnPage; // Ansicht vor Oeffnen der Einstellungen
// 0=kein Dialog, 1=WLAN loeschen, 2=Werkseinstellungen.
static uint8_t& resetConfirm = NerdWaveMenus::resetConfirm;
static bool& resetConfirmYes = NerdWaveMenus::resetConfirmYes;
static uint32_t browserActivityAt=0;
static uint32_t& settingsActivityAt = NerdWaveMenus::settingsActivityAt;
static constexpr uint32_t BROWSER_RETURN_MS=5000;
static bool redraw=true;
static uint32_t lastUi=0;
static String playingName="Kein Titel";

// IR-Lernmodus: Protokoll + Adresse + Kommando statt Raw-Wert speichern.
// NEC-Repeat-Telegramme enthalten oft keinen eigenen Raw-Code.
// IR-Eingabeverarbeitung und Textaufbereitung liegen in eigenen Modulen.

// GC9A01 240x240: getrennte Halbboegen mit 8 Grad Abstand an beiden Enden.
// Der Player aktualisiert nur veraenderte Elemente (kein periodisches Loeschen).
static int& lastProgress = NerdWaveViewState::lastProgress;
static int& lastVolume = NerdWaveViewState::lastVolume;
static int& lastPage = NerdWaveViewState::lastPage;
static String& shownPlayerTitle = NerdWaveViewState::shownPlayerTitle;
static String& shownTime = NerdWaveViewState::shownTime;
static String& shownSource = NerdWaveViewState::shownSource;
static String& shownState = NerdWaveViewState::shownState;
static int& shownWifiStatus = NerdWaveViewState::shownWifiStatus;
static int& shownRepeat = NerdWaveViewState::shownRepeat;
static int& shownShuffle = NerdWaveViewState::shownShuffle;

// ============== 07_CopyEngine ==============
static bool connectNetwork(); // Implementierung weiter unten, bestehende Netzwerkroutine
// Kopierbrowser nutzt die Quellenauswahl unabhaengig von der laufenden Musikquelle.
static bool copyListAt(int index, MusicItem &out) {
  if(index<0 || index>=mediaList.total) return false;
  uint16_t offset=(index/Config::PAGE_SIZE)*Config::PAGE_SIZE;
  if(!mediaListReady || mediaList.offset!=offset) {
    if(!api.fetchFrom(mediaOrigin,mediaPath,offset,mediaList)) return false;
    mediaListReady=true;
  }
  int local=index-mediaList.offset;
  if(local<0 || local>=mediaList.count) return false;
  out=mediaList.items[local];return true;
}
static bool copyLoadFolder(const String& path) {
  MusicPage temp;
  if(!api.fetchFrom(mediaOrigin,path,0,temp)) {mediaResult=api.error();return false;}
  mediaList=temp;mediaListReady=true;mediaPath=path;mediaSelection=0;return true;
}
// Kopieranzeige: statisches Layout einmal zeichnen, danach nur geaenderte Bereiche.
// Kein wiederholtes fillScreen(), da das GC9A01 sonst sichtbar flackert.
// Vorwaertsdeklaration fuer die Wiedergabe-Wiederaufnahme im Hauptloop.
static bool startTrack(const String& path, const String& name, bool keepShuffleScope = false, uint32_t seekSeconds = 0);

static void copyExecute() {
  static NerdWaveCopyEngine::Context ctx{mediaDestination, mediaOrigin, mediaResult, autoNextPending, autoNextEnabled, stopRequested, pauseRequested, decoderRunning, playerRunning, startingTrack, mediaCopyMode, mediaChosenPath, mediaAbort, musicSource, playerActive, playbackPaused, selectedMp3Path, copyResumePath, copyResumeName, copyResumeAfter, playingName, api, nerdUseWifi, mediaStage, redraw, lanReady, connectNetwork, ampOff};
  NerdWaveCopyEngine::execute(ctx);
}

// Optionaler Rastschalter GPIO21. Ohne NVS-Freigabe bleibt der Pin inaktiv.
static bool powerSwitchEnabled=false;
// Power-Schalter an GPIO21 (RTC-faehig): geschlossen=EIN (LOW).
// Der ESP32 bleibt an 5 V; nur ein geordneter Software-Aus-Zustand.
static bool powerSwitchOff() { return digitalRead(Config::POWER_SWITCH_PIN)==HIGH; }
static uint32_t powerOffSince=0;
static bool powerShutdownPending=false;

// ============== 08_CopyUiAndMenus ==============
// Ausschliesslich aus loop() aufrufen. Im Worker nie TFT oder startTrack().
static void copyPollUi() {
  volEncoder.update();
  nav.update();
  // Beide Encoder koennen den laufenden Kopiervorgang mit langem Druck abbrechen.
  if(volEncoder.takeButton()==ButtonEvent::Long || nav.takeButton()==ButtonEvent::Long)mediaAbort=true;
  // Drehschritte waehrend des Transfers nicht nach Transferende nachholen.
  (void)volEncoder.takeSteps();
  (void)nav.takeSteps();
  if(CopyManager::consumeFinished()) {
    const bool shutdownWaiting=powerShutdownPending && powerSwitchEnabled && powerSwitchOff();
    autoNextEnabled=!shutdownWaiting;
    if(!shutdownWaiting && copyResumeAfter && !startTrack(copyResumePath,copyResumeName))
      NerdWaveLog::output.println("[COPY] Wiedergabe konnte nicht wieder gestartet werden");
    copyResumeAfter=false;
    copyResumePath="";copyResumeName="";
    mediaStage=4;
    NerdWaveCopyUi::markActivity(); // Ergebnisanzeige ab jetzt genau 5 Sekunden
    redraw=true;
    return;
  }
  static uint32_t lastDraw=0,lastSerial=UINT32_MAX;
  static bool lastShutdownWaiting=false;
  const bool shutdownWaiting=powerShutdownPending && powerSwitchEnabled && powerSwitchOff();
  if(shutdownWaiting!=lastShutdownWaiting) {
    // Statuszeile und Kopieranzeige sofort neu aufbauen, auch wenn sich
    // der Kopierfortschritt (serial) seit dem letzten Frame nicht geaendert hat.
    NerdWaveCopyProgress::reset();
    lastSerial=UINT32_MAX;
    lastDraw=0;
    lastShutdownWaiting=shutdownWaiting;
  }
  if(millis()-lastDraw>=200) {
    CopyManager::Progress progress;
    CopyManager::snapshot(progress);
    const char* name=progress.name;
    const uint64_t done=progress.done, total=progress.total;
    const uint32_t files=progress.files, serial=progress.serial;
    if(!NerdWaveCopyProgress::drawn || serial!=lastSerial) {
      NerdWaveCopyProgress::render(tft,name[0]?String(name):String("Vorbereitung ..."),done,total,files);
      if(shutdownWaiting) {
        tft.fillRect(14,66,212,13,0x0000);
        centeredBootText("AUS nach Kopie",68,1,0xFFE0);
      }
      lastSerial=serial;
    }
    lastDraw=millis();
  }
}

// Speichert Einstellungen nur bei Aenderung, letzten Song bei erfolgreichem Start.
static void saveSettings() {
  NerdWaveSettings::savePlayback(prefs, repeatAll, shuffleMode, autoStartLast);
}

// Ausschliesslich aus loop() ausfuehren. Reset wirkt erst beim Neustart.
static void performReset(bool factoryReset) {
  // Verhindert automatischen Titelwechsel beim Herunterfahren.
  autoNextEnabled=false;
  autoNextPending=false;
  stopRequested=true;
  NerdWaveSystem::performReset(prefs, factoryReset, repeatAll, shuffleMode,
                               autoStartLast, networkMode, lastTrackPath, lastTrackName);
}

// Verzeichnissuche und Shuffle-Auswahl liegen in NerdWaveTrackCatalog.
static String parentFolder(const String& path) { return NerdWaveTrackCatalog::parentFolder(path); }
static bool isMp3File(const MusicItem& item) { return NerdWaveTrackCatalog::isMp3File(item); }
static bool pickRandomTrack(String& path, String& name) {
  return NerdWaveTrackCatalog::pickRandomTrack(api, playingFolder, shuffleMode==2, selectedMp3Path, path, name);
}

static bool startTrack(const String& path, const String& name, bool keepShuffleScope, uint32_t seekSeconds) {
  if (musicSource==2 || !NerdWavePcmAudio::ready() || (musicSource==3 ? !sdMounted : musicSource==1 ? (!usbDisk.mounted() || usbRemovalPending) : (nerdUseWifi ? WiFi.status()!=WL_CONNECTED : !lanReady))) { NerdWaveLog::output.println("[AUDIO] Nicht bereit"); return false; }
  // KEINE zwei gleichzeitigen Audio-Streams: erst beide Tasks sauber stoppen.
  const uint32_t switchAt=millis();
  NerdWaveLog::output.printf("[TASKDIAG] Titelwechsel angefordert: Decoder=%d, Player=%d, Aktiv=%d, vorher=%s\n",
    decoderRunning?1:0,playerRunning?1:0,playerActive?1:0,selectedMp3Path.c_str());
  autoNextPending = false;
  stopRequested=true;
  pauseRequested=false;
  uint32_t deadline=millis()+7000;
  while ((decoderRunning || playerRunning) && (int32_t)(millis()-deadline)<0) {
    nav.update();volEncoder.update();
    delay(5);
  }
  NerdWaveLog::output.printf("[TASKDIAG] Stop-Wartezeit=%lu ms, Decoder=%d, Player=%d\n",
    (unsigned long)(millis()-switchAt),decoderRunning?1:0,playerRunning?1:0);
  if(decoderRunning || playerRunning) {
    NerdWaveLog::output.println("[AUDIO] Alte Tasks nicht beendet - Titelwechsel abgebrochen");
    return false;
  }
  // Kein Task darf jetzt auf die Ringvariablen zugreifen.
  NerdWavePcmAudio::reset();
  // Beim Spulen die bisher bekannte Titeldauer fuer die Webanzeige erhalten.
  const uint32_t previousDuration = estimatedDurationSec;
  bytesPlayed=framesDecoded=0;
  webSeekStartSeconds=seekSeconds;
  webSeeking=(seekSeconds>0);
  webSeekDisplayDuration=seekSeconds ? previousDuration : 0;
  playbackBaseSeconds=seekSeconds;
  audioSampleRateReady=false;
  audioSampleRate=48000;
  estimatedDurationSec=0; durationExact=false;
  underruns=i2sErrors=maxI2sWriteUs=netStalls=0;
  decodeDone=decodeError=false;
  selectedMp3Path=path;
  playingName=name;
  stopRequested=false;
  pauseRequested=false;
  playbackPaused=false;
  startingTrack=true;
  NerdWaveLog::output.printf("[AUDIO] Starte: %s\n",path.c_str());
  NerdWaveMp3Stream::bind(mp3StreamContext);
  BaseType_t a=xTaskCreatePinnedToCore(NerdWaveMp3Stream::decodeTask,"NerdDecode",49152,nullptr,2,nullptr,1);
  if(a!=pdPASS) {NerdWaveLog::output.println("[TASK] Decoder-Start fehlgeschlagen");startingTrack=false;return false;}
  // Auf Task-Start warten, damit ein spaeterer Titelwechsel den Task erkennt.
  uint32_t until=millis()+1000;
  while(!decoderRunning && (int32_t)(millis()-until)<0) delay(1);
  static NerdWavePlayback::Context playbackContext {
    playerRunning, stopRequested, decodeDone, decodeError,
    audioSampleRateReady, audioSampleRate, playerActive, playbackPaused,
    pauseRequested, i2sErrors, bytesPlayed, underruns, maxI2sWriteUs,
    muted, volumePercent, audioLevelQ15, autoNextEnabled, autoNextPending,
    ampOff, ampOn, spectrumCapture
  };
  BaseType_t b=xTaskCreatePinnedToCore(NerdWavePlayback::playbackTask,"NerdPlay",8192,&playbackContext,3,nullptr,0);
  if(b!=pdPASS) {
    NerdWaveLog::output.println("[TASK] Player-Start fehlgeschlagen");
    stopRequested=true;startingTrack=false;return false;
  }
  until=millis()+1000;
  while(!playerRunning && (int32_t)(millis()-until)<0) delay(1);
  startingTrack=false;
  // Speicherung erst nachdem beide Tasks erfolgreich gestartet wurden.
  lastTrackPath=path; lastTrackName=name;
  if(!keepShuffleScope) playingFolder=parentFolder(path);
  // Titel getrennt pro Musikquelle merken, statt beim Wechsel zu verwerfen.
  prefs.putString(trackPathKey(musicSource),path);
  prefs.putString(trackNameKey(musicSource),name);
  redraw=true;
  return true;
}

// Browser-Auswahl: Ein bereits laufender (auch pausierter) Titel darf
// nicht erneut gestartet werden. Andere Startwege (Seek/Skip/Restart) bleiben erhalten.
static bool startBrowserTrack(const String& path, const String& name) {
  if (musicSource!=2 && path==selectedMp3Path && !stopRequested &&
      (decoderRunning || playerRunning)) {
    NerdWaveLog::output.println("[BROWSER] Titel bereits geladen, Wiedergabe fortsetzen");
    return true;
  }
  return startTrack(path,name);
}

// Beim Oeffnen des Browsers den aktuell gewaehlten Titel markieren.
// Die Navigation wird nur bei Bedarf aufgebaut; ein bereits markierter
// Titel benoetigt keine weiteren NAS-/Datentraegerabfragen.
static void focusBrowserOnCurrentTrack() {
  if(musicSource==2 || selectedMp3Path.isEmpty()) return;
  if((musicSource==1 && !usbDisk.mounted()) || (musicSource==3 && !sdMounted)) return;
  const MusicItem* current=browser.current();
  if(current && !current->directory && current->path==selectedMp3Path) return;
  if(!browser.focusTrack(selectedMp3Path)) {
    NerdWaveLog::output.println("[BROWSER] Gespeicherter Titel nicht in Liste gefunden; aktuelle Ansicht bleibt");
  }
}

// Prueft den gespeicherten Titel anhand der aktuellen Dateiliste, statt
// nur den Decoder-Task zu starten (der einen fehlenden Titel erst spaeter merkt).
// NAS-Probleme duerfen nicht als geloeschte Dateien interpretiert werden.
static bool& nasRecoveryPending = NerdWaveRecovery::nasPending;
static uint32_t& nasRetryAt = NerdWaveRecovery::nasRetryAt;
static uint8_t& nasRecoveryFailures = NerdWaveRecovery::nasFailures;

static bool trackExistsInSource(const String& path) {
  return NerdWaveTrackCatalog::trackExists(api, path);
}

static bool firstPlayableInSource(String& path, String& name) {
  return NerdWaveTrackCatalog::firstPlayable(api, path, name);
}

// Titelwechsel innerhalb des gerade geoeffneten Ordners. Unterordner
// werden uebersprungen; ein Dateibrowser-Abruf kann dabei stattfinden.
static bool changeTrack(int direction, bool restartCurrent = false) {
  NerdWavePlayerControl::NavigationContext ctx{
    browser, selectedMp3Path, playingName, bytesPlayed,
    decodeError,
    [](const String& path, const String& name, bool keepScope) { return startTrack(path,name,keepScope); },
    pickRandomTrack, parentFolder, isMp3File
  };
  return NerdWavePlayerControl::changeTrack(ctx, direction, restartCurrent);
}

// WiFiManager erwartet weiterhin einen funktionszeigerfreien Callback.
static void renderWifiSetup() { NerdWaveSystem::renderWifiSetup(tft); }

static bool connectNetwork() {
  return NerdWaveNetwork::connect(networkMode, nerdUseWifi, lanReady, wifiReady, renderWifiSetup);
}


// Gemeinsame Autostart-Wiederherstellung fuer Kaltstart und Quellenwechsel.
// Die urspruenglichen Statusmeldungen bleiben je nach Aufrufer erhalten.
static void restoreAutostartTrack(bool afterSourceSwitch) {
  if(!autoStartLast || musicSource==2) return;
  if(musicSource==1 && !usbDisk.mounted()) {
    NerdWaveLog::output.println("[USB] Autostart uebersprungen: kein Datentraeger eingebunden");
    return;
  }
  if(musicSource==3 && !sdMounted) {
    sdAutostartPending=true;
    NerdWaveLog::output.println(afterSourceSwitch ? "[SDREC] Autostart nach Quellenwechsel verschoben" : "[SDREC] Autostart auf erfolgreichen SD-Mount verschoben");
    return;
  }
  String path=lastTrackPath;
  String name=lastTrackName.length()?lastTrackName:path.substring(path.lastIndexOf('/')+1);
  bool savedValid=trackExistsInSource(path);
  if(musicSource==0 && api.error().length()) {
    nasRecoveryPending=true; nasRetryAt=millis();
    NerdWaveLog::output.printf("[NAS] Autostart verschoben: %s\n", api.error().c_str());
  } else if(!savedValid) {
    if(path.length()) NerdWaveLog::output.printf(afterSourceSwitch ? "[AUTOQUELLE] Gespeicherter Titel fehlt: %s\n" : "[AUTOBOOT] Gespeicherter Titel fehlt: %s\n",path.c_str());
    path="";name="";
    if(firstPlayableInSource(path,name)) NerdWaveLog::output.printf(afterSourceSwitch ? "[AUTOQUELLE] Fallback: %s\n" : "[AUTOBOOT] Fallback: %s\n",path.c_str());
    else NerdWaveLog::output.println(afterSourceSwitch ? "[AUTOQUELLE] Keine MP3 auf dieser Quelle gefunden" : "[AUTOBOOT] Keine MP3 auf dieser Quelle gefunden");
  }
  if(path.length() && !(musicSource==0 && nasRecoveryPending)) {
    if(startTrack(path,name)) {
      bool focused=browser.focusTrack(path);
      if(afterSourceSwitch && !focused) NerdWaveLog::output.println("[AUTOQUELLE] Browser-Kontext konnte nicht geladen werden");
      page=1;
      NerdWaveLog::output.println(afterSourceSwitch
        ? (savedValid ? "[AUTOQUELLE] Letzter Titel gestartet" : "[AUTOQUELLE] Ersten verfuegbaren Titel gestartet")
        : (savedValid ? "[AUTOBOOT] Letzter Titel gestartet" : "[AUTOBOOT] Ersten verfuegbaren Titel gestartet"));
    } else NerdWaveLog::output.println(afterSourceSwitch ? "[AUTOQUELLE] Titel konnte nicht gestartet werden" : "[AUTOBOOT] Titel konnte nicht gestartet werden");
  }
}

static bool webTogglePlayback(){
  if(musicSource==2){btSend("TOGGLE");return true;}
  if(playerRunning||playerActive){NerdWavePlayerControl::togglePause();return true;}
  if(selectedMp3Path.length())return startTrack(selectedMp3Path,playingName);
  return false;
}
static bool webStopPlayback(){
  autoNextPending=false;autoNextEnabled=false;
  stopRequested=true;pauseRequested=false;
  if(musicSource==2){btSend("PAUSE");NerdWaveBtAudio::requestStop();}
  uint32_t started=millis();
  while((decoderRunning||playerRunning||startingTrack||btTaskRunning||btRxRunning) && millis()-started<5000)delay(5);
  bool done=!(decoderRunning||playerRunning||startingTrack||btTaskRunning||btRxRunning);
  if(done){ampOff();playerActive=false;playbackPaused=false;}
  NerdWaveLog::output.printf("[OTA] Audio-Stopp %s nach %lu ms\n",done?"OK":"Timeout",(unsigned long)(millis()-started));
  return done;
}
static bool webSkipTrack(int direction){
  if(musicSource==2){btSend(direction>0?"NEXT":"PREV");return true;}
  bool restart=direction<0 && bytesPlayed>(uint64_t)3*audioSampleRate*4;
  return changeTrack(direction,restart);
}
static void webSetVolume(int target){NerdWaveVolume::changeBy(target-(int)volumePercent,millis());}
static void webSetSource(uint8_t target){if(NerdWaveSources::enabled(target) && target!=musicSource){requestedMusicSource=target;sourceSwitchPending=true;}}
static void powerSleep() {
  NerdWaveLog::output.println("[POWER] AUS angefordert");
  // Niemals in Deep Sleep wechseln, solange der Kopier-Worker Dateien offen hat.
  // Der Hauptloop wartet ohne Abbruchsignal auf CopyManager::Finished/Idle.
  if(CopyManager::state()==CopyManager::State::Running) return;
  if(CopyManager::state()==CopyManager::State::Finished) CopyManager::consumeFinished();
  if(!webStopPlayback()) {
    NerdWaveLog::output.println("[POWER] Audio-Task laeuft noch: kein Deep Sleep");
    return;
  }
  // Wenn der Schalter inzwischen wieder auf EIN steht, normal weitermachen.
  if(!powerSwitchOff()) { NerdWaveLog::output.println("[POWER] Einschalten waehrend Shutdown"); return; }
  saveVolumeNow();
  ampOff();
  ledEnabled=false;ledTick(false);
  // Kurze Verabschiedung erst nach beendeten Dateizugriffen und Audio-Stopp.
  tft.fillScreen(0x0000);
  tft.setTextWrap(false);
  tft.setTextColor(0x07FF);
  tft.setTextSize(2);
  centeredBootText("Nerd-Wave",91,2,0x07FF);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  centeredBootText("Bis bald!",129,1,0xFFFF);
  NerdWaveLog::output.println("[POWER] Verabschiedung angezeigt");
  delay(1200);
  if(!powerSwitchOff()) { NerdWaveLog::output.println("[POWER] Schalter wieder EIN; Shutdown abgebrochen"); redraw=true; return; }
  tft.fillScreen(0x0000);
  // Fuer den Wakeup bleibt der RTC-Pull-up des GPIO21 eingeschaltet.
  rtc_gpio_pullup_en(GPIO_NUM_21);
  rtc_gpio_pulldown_dis(GPIO_NUM_21);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_ON);
  esp_err_t wake=esp_sleep_enable_ext0_wakeup(GPIO_NUM_21,0);
  if(wake!=ESP_OK){NerdWaveLog::output.printf("[POWER] Wakeup-Konfiguration fehlgeschlagen: %d\n",(int)wake);return;}
  prefs.end();
  NerdWaveLog::output.println("[POWER] Deep Sleep; Schalter EIN weckt GPIO21 (LOW)");
  Serial.flush();
  esp_deep_sleep_start();
}

void nerdWaveSetup() {
  Serial.begin(115200);delay(800);
  prefs.begin("nerd-wave",false);
  powerSwitchEnabled=prefs.getBool("pwrSwitch",false);
  if(powerSwitchEnabled){
    pinMode(Config::POWER_SWITCH_PIN, INPUT_PULLUP);
    if(powerSwitchOff()){
      delay(100); // Entprellen vor dem ersten Boot
      if(powerSwitchOff()){
        rtc_gpio_pullup_en(GPIO_NUM_21);
        rtc_gpio_pulldown_dis(GPIO_NUM_21);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_ON);
        esp_err_t wake=esp_sleep_enable_ext0_wakeup(GPIO_NUM_21,0);
        if(wake==ESP_OK){
          NerdWaveLog::output.println("[POWER] Schalter AUS bei Boot: Deep Sleep");
          prefs.end(); Serial.flush(); esp_deep_sleep_start();
        }
        NerdWaveLog::output.printf("[POWER] Wakeup fehlgeschlagen: %d; normaler Start\n",(int)wake);
      }
    }
  }
  NerdWaveLog::output.printf("\n=== NERD-WAVE %s ===\n", Config::VERSION);
  NerdWaveLog::output.printf("[BOOT-DIAG] reset_reason=%d\n", (int)esp_reset_reason());
  NerdWaveLog::output.println("[BOOT-DIAG] 01 onboardRgbOff BEGIN"); Serial.flush();
  NerdWaveSystem::onboardRgbOff();
  NerdWaveLog::output.println("[BOOT-DIAG] 01 onboardRgbOff OK"); Serial.flush();
  NerdWaveLog::output.println("[BOOT-DIAG] 02 initHardwarePins BEGIN"); Serial.flush();
  NerdWaveSystem::initHardwarePins();
  NerdWaveLog::output.println("[BOOT-DIAG] 02 initHardwarePins OK"); Serial.flush();
  NerdWaveLog::output.println("[BOOT-DIAG] 03 IR BEGIN"); Serial.flush();
  InputManager::beginIr(Config::IR_PIN);
  NerdWaveLog::output.println("[BOOT-DIAG] 03 IR OK"); Serial.flush();
  NerdWaveLog::output.println("[IR] Empfaenger auf GPIO2 aktiv (Arduino-IRremote 4.x)");
  NerdWaveLog::output.println("[BOOT-DIAG] 04 SPI BEGIN"); Serial.flush();
  SPI.begin(Config::SPI_SCLK,Config::SPI_MISO,Config::SPI_MOSI);
  NerdWaveLog::output.println("[BOOT-DIAG] 04 SPI OK"); Serial.flush();
  NerdWaveLog::output.println("[BOOT-DIAG] 05 MCP BEGIN"); Serial.flush();
  McpInputs::begin();
  NerdWaveLog::output.println("[BOOT-DIAG] 05 MCP OK"); Serial.flush();
  NerdWaveLog::output.println("[BOOT-DIAG] 06 Encoder BEGIN"); Serial.flush();
  nav.begin(); volEncoder.begin();
  NerdWaveLog::output.println("[BOOT-DIAG] 06 Encoder OK"); Serial.flush();
  NerdWaveLog::output.println("[BOOT-DIAG] 07 TFT BEGIN"); Serial.flush();
  tft.begin(); tft.setSPISpeed(displaySpiHz); NerdWaveLog::output.printf("[DISPLAY-DIAG] Start: GC9A01 SPI %lu MHz (Serial: display 10/20/40/status)\n", (unsigned long)(displaySpiHz / 1000000UL)); DisplayUI::attach(tft); tft.setRotation(0); tft.fillScreen(0x0000);
  NerdWaveLog::output.println("[BOOT-DIAG] 07 TFT OK"); Serial.flush();
  beginBootLog();
  NerdWaveLog::output.println("[BOOT-DIAG] 08 Bootscreen OK"); Serial.flush();

  NerdWaveSkins::begin(prefs); NerdWaveFields::begin(prefs);
  NerdWaveNasConfig::load(prefs);
  InputManager::bind(prefs,page,redraw,irLedToggle,irLedNext);
  loadIrBindings();
  ledLoad(prefs);ledBegin();
  NerdWaveSources::load(prefs);
  NerdWaveLog::output.printf("[QUELLEN] Aktivierungsmaske=0x%02X NAS=%u USB=%u BT=%u SD=%u\n",
    NerdWaveSources::enabledMask(), NerdWaveSources::enabled(0),
    NerdWaveSources::enabled(1), NerdWaveSources::enabled(2), NerdWaveSources::enabled(3));
  NerdWaveLog::output.printf("[QUELLE] %s\n",sourceName(musicSource));
  // USB darf den Bootvorgang nicht blockieren: Host und FAT erst im loopTask
  // starten, nachdem Netzwerk, Audio, Webserver und MQTT initialisiert sind.
  usbPreviouslyMounted=false;
  usbEverMounted=false;
  if(NerdWaveSources::enabled(1))
    NerdWaveLog::output.println("[USB] Initialisierung bis nach Systemstart verschoben");
  if(musicSource==3) sdTryMount();
  NerdWaveWifiFilter::begin();
  networkMode=NerdWaveSettings::loadNetworkMode(prefs);
  // Migration einer alten deaktivierten LAN-Option: WLAN bleibt erhalten.
  if(!prefs.getBool("lanOn",true)) networkMode=1;
  NerdWaveNetwork::setLanEnabled(networkMode==0);
  bootNetworkMode=networkMode;
  NerdWaveLog::output.printf("[NET] Modus: %s\n",NerdWaveSettings::modeName(networkMode));
  if(musicSource==0) {
    NerdWaveLog::output.println("[BOOT] Verbinde Netzwerk...");
    connectNetwork();
  }
  // Weboberflaeche auch fuer lokale Quellen erreichbar machen.
  if(musicSource!=0 && !(lanReady || WiFi.status()==WL_CONNECTED)) connectNetwork();
  if(!NerdWavePcmAudio::begin()) {NerdWaveLog::output.println("[RAM] Ring/Mutex Fehler");return;}
  NerdWaveLog::output.printf("[RAM] Ring 512 KB PSRAM, intern frei %u\n",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  NerdWaveVolume::load(prefs);
  NerdWaveSettings::loadPlayback(prefs, repeatAll, shuffleMode, autoStartLast);
  // Alte Quellen-NVS-Werte einmalig migrieren.
  NerdWaveSystem::migrateLegacyTrack(prefs);
  NerdWaveSources::loadTrack(prefs,lastTrackPath,lastTrackName);
  // Restore the last selected track for the display even when autostart is off
  // or the media source is temporarily unavailable after an OTA reboot.
  if(musicSource!=2) {
    selectedMp3Path=lastTrackPath;
    playingName=lastTrackName.length()?lastTrackName:(lastTrackPath.length()?lastTrackPath.substring(lastTrackPath.lastIndexOf('/')+1):String(""));
  }
  settingsLoaded=true;
  NerdWaveLog::output.printf("[NVS] Repeat=%d Shuffle=%u Autostart=%d letzter=%s\n",
    repeatAll,shuffleMode,autoStartLast,lastTrackPath.c_str());
  NerdWaveLog::output.println("[BOOT] Lade Bibliothek...");
  if(musicSource!=2 && !browser.begin()) {
    NerdWaveLog::output.printf("[BROWSER] %s (automatischer Retry fuer NAS)\n",browser.error().c_str());
    if(musicSource==0) { nasRecoveryPending=true; nasRetryAt=millis(); }
  }
  if(musicSource==2) {
    btLink.begin(115200,SERIAL_8N1,BT_UART_RX,BT_UART_TX);
    btUartStarted=true;
    playingName="Bluetooth";
    page=1;
    audioSampleRate=44100;
    audioSampleRateReady=true;
    if(!NerdWaveBtAudio::start(playerActive,nasI2SInstalled,configureLegacyI2S,ampOff,ampOn))
      NerdWaveLog::output.println("[BT] Audiotask konnte nicht gestartet werden");
    btSend("STATUS");
  }
  restoreAutostartTrack(false);
  static NerdWaveWeb::Context webContext{prefs,musicSource,networkMode,repeatAll,shuffleMode,autoStartLast,networkRestartPending,bootNetworkMode,page,redraw,playingName,selectedMp3Path,bytesPlayed,playbackBaseSeconds,audioSampleRate,estimatedDurationSec,webSeeking,webSeekDisplayDuration,[](uint32_t seconds)->bool { if(musicSource==2 || !selectedMp3Path.length() || !estimatedDurationSec || seconds>=estimatedDurationSec)return false; return startTrack(selectedMp3Path,playingName,false,seconds); },playerActive,playbackPaused,btConnected,btPlaying,volumePercent,[](const String& path,const String& name)->bool{return startBrowserTrack(path,name);},webTogglePlayback,webStopPlayback,webSkipTrack,webSetVolume,webSetSource,saveSettings};
  NerdWaveWeb::begin(webContext);
  NerdWaveMqtt::begin(prefs,{webTogglePlayback,webSkipTrack,webSetVolume,webSetSource,nullptr,nullptr,
    [](const String& request,bool isIndex)->bool {
      if(musicSource==2 || !NerdWaveSources::enabled(musicSource))return false;
      String folder=playingFolder.length()?playingFolder:String("/");
      if(isIndex){
        unsigned long wanted=request.toInt();if(wanted<1 || wanted>Config::MAX_TOTAL)return false;
        MusicApi scan;static MusicPage page;unsigned long ordinal=0;
        for(uint16_t offset=0;offset<Config::MAX_TOTAL;){
          if(!scan.fetchFrom(musicSource,folder,offset,page))return false;
          if(!page.count)return false;
          for(uint16_t i=0;i<page.count;i++){
            const MusicItem& song=page.items[i];
            if(song.directory || !NerdWaveTrackCatalog::isMp3File(song))continue;
            if(++ordinal==wanted)return startTrack(song.path,song.name);
          }
          offset+=page.count;if(offset>=page.total)break;
        }
        return false;
      }
      String path=request;path.trim();
      if(!path.startsWith("/")){
        // Dateiname oder Titel ohne .mp3 im aktuellen Ordner eindeutig suchen.
        if(path.isEmpty() || path.length()>255 || path.indexOf("..")>=0 || path.indexOf('/')>=0 || path.indexOf('\\')>=0)return false;
        String sought=path;sought.toLowerCase();
        MusicApi scan;static MusicPage list;String foundPath,foundName;
        for(uint16_t offset=0;offset<Config::MAX_TOTAL;){
          if(!scan.fetchFrom(musicSource,folder,offset,list) || !list.count)return false;
          for(uint16_t i=0;i<list.count;i++){
            const MusicItem& song=list.items[i];if(!NerdWaveTrackCatalog::isMp3File(song))continue;
            String n=song.name;n.toLowerCase();
            String stem=n;if(stem.endsWith(".mp3"))stem.remove(stem.length()-4);
            if(n==sought || stem==sought){
              if(foundPath.length())return false; // mehrfacher Treffer ist mehrdeutig
              foundPath=song.path;foundName=song.name;
            }
          }
          offset+=list.count;if(offset>=list.total)break;
        }
        return foundPath.length()?startTrack(foundPath,foundName):false;
      }
      if(path.length()<5 || path.length()>300 || path.indexOf("..")>=0 || path.indexOf('\\')>=0)return false;
      String lower=path;lower.toLowerCase();if(!lower.endsWith(".mp3"))return false;
      if(!NerdWaveTrackCatalog::trackExists(api,path))return false;
      const int slash=path.lastIndexOf('/');
      return startTrack(path,path.substring(slash+1));
    }});
  NerdWaveLog::output.println("[BOOT] System bereit");
  endBootLog();
  // Nach dem Boot sofort den Player anzeigen, sobald ein letzter Titel
  // bekannt ist. Autostart regelt ausschliesslich die Wiedergabe.
  // Kein startTrack() hier: bei deaktiviertem Autostart bleibt Audio aus.
  if(musicSource!=2 && selectedMp3Path.length()>0 &&
     !(musicSource==1 && !usbDisk.mounted()) &&
     !(musicSource==3 && !sdMounted)) {
    page=1;
    NerdWaveLog::output.println("[BOOT] Gespeicherter Titel vorhanden: Player anzeigen");
  }
  if(musicSource==1 && !usbDisk.mounted()) {
    page=0;
    NerdWaveLog::output.println("[BOOT] Kein USB-Stick: Browser statt gespeicherter Titel");
  }
  browserActivityAt=millis();
  redraw=true;
}

// Wechsel nur im loopTask. Niemals RX und MP3 gleichzeitig auf I2S0 betreiben.
static bool applyMusicSource(uint8_t target) {
  if(!NerdWaveSources::enabled(target))return false;
  if(target==musicSource) return true;
  NerdWaveLog::output.printf("[QUELLE] Umschalten %u -> %u\n",musicSource,target);
  autoNextPending=false;autoNextEnabled=false;
  stopRequested=true;pauseRequested=false;
  uint32_t until=millis()+8000;
  while((decoderRunning || playerRunning || startingTrack) && (int32_t)(millis()-until)<0) delay(5);
  if(decoderRunning || playerRunning || startingTrack) {
    NerdWaveLog::output.println("[QUELLE] MP3-Tasks nicht beendet - Wechsel abgebrochen");
    return false;
  }
  if(musicSource==2) {
    btSend("PAUSE"); // A2DP empfangsbereit lassen; keinen Bluetooth-Stack stoppen
    NerdWaveBtAudio::requestStop();
    until=millis()+3000;
    while((btTaskRunning || btRxRunning) && (int32_t)(millis()-until)<0) delay(5);
    if(btTaskRunning || btRxRunning) {
      NerdWaveLog::output.println("[QUELLE] BT-Tasks nicht beendet - Wechsel abgebrochen");
      return false;
    }
  }
  ampOff();playerActive=false;audioLevelQ15=0;
  if(target==0 && !(lanReady || WiFi.status()==WL_CONNECTED)) {
    if(!connectNetwork()) NerdWaveLog::output.println("[QUELLE] NAS gewaehlt, Netzwerk aktuell offline");
  }
  // USB-Mount uebernimmt der regulaere Hotplug-Poll, nicht der Quellenwechsel.
  if(target==3 && !sdMounted) sdTryMount();
  if(target!=3) sdRecoveryPending=false;
  sdAutostartPending=false;
  nasRecoveryPending=false;
  nasRecoveryFailures=0;
  NerdWaveSources::commit(target,prefs,lastTrackPath,lastTrackName);
  // Zuletzt gewaehlten Titel der neuen Quelle auch ohne Autostart anzeigen.
  playingName=musicSource==2?"Bluetooth":lastTrackName;
  selectedMp3Path=musicSource==2?String(""):lastTrackPath;bytesPlayed=0;estimatedDurationSec=0;durationExact=false;
  webSeeking=false;webSeekDisplayDuration=0;
  if(musicSource==2) {
    if(!btUartStarted){btLink.begin(115200,SERIAL_8N1,BT_UART_RX,BT_UART_TX);btUartStarted=true;}
    btConnected=false;btPlaying=false;
    audioSampleRate=44100;audioSampleRateReady=true;
    if(!NerdWaveBtAudio::start(playerActive,nasI2SInstalled,configureLegacyI2S,ampOff,ampOn))
      NerdWaveLog::output.println("[BT] Audiotask konnte nicht gestartet werden");
    btSend("STATUS");
  } else {
    audioSampleRateReady=false;
    if(!browser.begin()) {
      NerdWaveLog::output.printf("[BROWSER] %s\n",browser.error().c_str());
      if(musicSource==0) { nasRecoveryPending=true; nasRetryAt=millis(); }
    }
  }
  autoNextEnabled=true;
  restoreAutostartTrack(true);
  NerdWaveViewState::sourceChanged(page,redraw);
  // Gleiche USB-Startansicht wie beim Boot: Ohne eingebundenen Stick
  // keinen (moeglicherweise veralteten) gespeicherten Titel anzeigen.
  // Der USB-Hotplug-Poll wechselt beim Einstecken zum Dateibrowser.
  if(musicSource==1 && !usbDisk.mounted()) {
    page=0;
    browserActivityAt=millis();
    redraw=true;
    NerdWaveLog::output.println("[USB] Quellenwechsel: Kein USB Stick, Browser anzeigen");
  }
  // Wie bei USB darf ein alter gemerkter Titel den Player nicht oeffnen,
  // solange der SD-Datentraeger nicht eingebunden ist.
  if(musicSource==3 && !sdMounted) {
    page=0;
    browserActivityAt=millis();
    redraw=true;
    NerdWaveLog::output.println("[SD] Quellenwechsel: Keine SD Karte, Browser anzeigen");
  }
  return true;
}

// USB-Hotplug benoetigt nur einen Browser-Refresh, keine Kenntnis der UI-Klassen.
static void refreshUsbBrowser() { browser.begin(); }

static bool recoveryStartTrack(const String& path, const String& name) {
  return startTrack(path,name);
}
static NerdWaveRecovery::Context recoveryContext{
  musicSource, nasRecoveryPending, nasRetryAt, nasRecoveryFailures,
  sdRecoveryPending, sdMounted, sdRetryAt, sdAutostartPending,
  startingTrack, decoderRunning, playerRunning, nerdUseWifi, wifiReady,
  autoStartLast, selectedMp3Path, prefs, browser, api, page, redraw,
  recoveryStartTrack
};


void nerdWaveLoop() {
  // Nach abgeschlossenem Setup USB-Host starten; kein FAT-Zugriff beim Boot.
  if(NerdWaveSources::enabled(1) && !usbHostStarted) {
    static uint32_t nextUsbHostTry=0;
    if((int32_t)(millis()-nextUsbHostTry)>=0) {
      nextUsbHostTry=millis()+5000;
      usbStartHost();
    }
  }
  // Entprellt: AUS muss mindestens 100 ms stabil anliegen.
  // Diese Option wird nach dem Speichern im Web ohne Neustart uebernommen.
  static uint32_t powerConfigChecked=0;
  if((uint32_t)(millis()-powerConfigChecked)>1500){
    powerConfigChecked=millis();
    const bool enabled=prefs.getBool("pwrSwitch",false);
    if(enabled && !powerSwitchEnabled) pinMode(Config::POWER_SWITCH_PIN,INPUT_PULLUP);
    powerSwitchEnabled=enabled;
  }
  if(powerSwitchEnabled && powerSwitchOff()) {
    if(!powerOffSince)powerOffSince=millis();
    if((uint32_t)(millis()-powerOffSince)>=100){
      if(!powerShutdownPending) {
        powerShutdownPending=true;
        NerdWaveLog::output.println("[POWER] AUS vorgemerkt; laufende Kopien werden fertiggestellt");
        if(CopyManager::state()!=CopyManager::State::Idle) {
          NerdWaveCopyProgress::reset();
        }
      }
      // Kopieren im Hintergrund normal weiterlaufen lassen, einschliesslich
      // Fortschrittsanzeige. Keinen mediaAbort setzen, keine FAT-Zugriffe unterbrechen.
      if(CopyManager::state()==CopyManager::State::Idle) {
        powerSleep();
        delay(25);
        return;
      }
    }
  }else {
    if(powerShutdownPending) NerdWaveLog::output.println("[POWER] Ausschaltwunsch zurueckgenommen");
    powerShutdownPending=false;
    powerOffSince=0;
  }
  NerdWaveWeb::poll();
  static uint32_t mqttPollAt=0;
  if((uint32_t)(millis()-mqttPollAt)>=250){
    mqttPollAt=millis();
    const uint32_t elapsed=(uint32_t)(playbackBaseSeconds+(audioSampleRate?bytesPlayed/((uint64_t)audioSampleRate*4):0));
    NerdWaveMqtt::poll({musicSource,NerdWaveSources::enabledMask(),bool(playerActive&&!playbackPaused),bool(playbackPaused),(int)volumePercent,repeatAll,shuffleMode,playingName,elapsed,(uint32_t)estimatedDurationSec},wifiReady||lanReady);
  }
  NerdWaveWifiFilter::poll();
  if(CopyManager::state()!=CopyManager::State::Idle) {
    copyPollUi();
    delay(1);
    return;
  }
  // SD-Hotplug: Ein erfolgreicher Mount bleibt sonst nach physischem Entfernen
  // als "mounted" gespeichert. Nur bei ausgewaehlter SD-Quelle und ohne
  // Dateikopie pruefen, damit wir keinen laufenden Kopierauftrag stoeren.
  if(musicSource==3 && sdMounted && !sdRemovalPending && CopyManager::state()==CopyManager::State::Idle) {
    static uint32_t sdPresenceAt=0;
    if((uint32_t)(millis()-sdPresenceAt)>=1200) {
      sdPresenceAt=millis();
      // SD_MMC.cardType() und Verzeichniszugriffe koennen nach dem Ziehen
      // weiterhin gecachte Informationen liefern. Ein echter Sektor-Read
      // fragt dagegen den physisch vorhandenen Datentraeger ab.
      // Statischer, ausgerichteter Puffer: kein Heap-Churn im Hauptloop.
      alignas(4) static uint8_t sdProbeSector[512];
      const bool sdPresent=SD_MMC.readRAW(sdProbeSector, 0);
      if(!sdPresent) {
        NerdWaveLog::output.println("[SD] Karte entfernt; Wiedergabe stoppen und Unmount vormerken");
        stopRequested=true;
        autoNextPending=false;
        autoNextEnabled=false;
        sdRemovalPending=true;
        page=0;
        browserActivityAt=millis();
        redraw=true;
      }
    }
  }
  if(sdRemovalPending && !decoderRunning && !playerRunning && !startingTrack &&
     CopyManager::state()==CopyManager::State::Idle) {
    SD_MMC.end();
    sdMounted=false;
    sdRemovalPending=false;
    sdRecoveryPending=true;
    sdRetryAt=millis();
    playerActive=false;
    playbackPaused=false;
    decodeDone=false;
    ampOff();
    browser.begin();
    page=0;
    redraw=true;
    NerdWaveLog::output.println("[SD] Karte freigegeben; erneutes Einstecken wird erkannt");
  }
  // NAS- und SD-Recovery in eigenem Modul, bisherige Runtime-Zustaende als Referenzen.
  NerdWaveRecovery::poll(recoveryContext);
  // Hotplug und BT-Wartung in einem eigenen Modul; Audio-/Browserzustand bleibt in der Runtime.
  if(NerdWaveSources::enabled(1)) NerdWaveLoopMaintenance::pollUsb(musicSource, decoderRunning || playerRunning || startingTrack,
                                    stopRequested, pauseRequested, autoNextPending,
                                    page, redraw, ampOff, refreshUsbBrowser);
  if(NerdWaveSources::enabled(2)) NerdWaveLoopMaintenance::pollBluetooth(musicSource, sourceSwitchPending, playerActive,
                                          nasI2SInstalled, redraw,
                                          configureLegacyI2S, ampOff, ampOn);
  handleIr();
  if(irDirectSource>=0) {
    const uint8_t target=(uint8_t)irDirectSource;
    irDirectSource=-1;
    if(NerdWaveSources::enabled(target)) {
      // Identischer Wechselpfad wie im Webinterface; kein Reboot.
      webSetSource(target);
      // Ein IR-Direktbefehl soll auch aus dem Einstellungsmenue funktionieren.
      if(sourceSwitchPending && page==3) {page=1;redraw=true;}
    } else {
      NerdWaveLog::output.printf("[IR] Quelle %s ist deaktiviert\n",sourceName(target));
    }
  }
  if(irLedToggle){irLedToggle=false;ledEnabled=!ledEnabled;ledSave();redraw=true;}
  if(irLedNext){irLedNext=false;ledEffect=(ledEffect+1)%LED_EFFECT_COUNT;ledEnabled=true;ledSave();redraw=true;}
  ledTick(playerActive && !pauseRequested && !playbackPaused);

  // Nur der Haupt-Loop darf nach dem Ende der beiden Audio-Tasks umschalten.
  if (autoNextPending && !decoderRunning && !playerRunning && !startingTrack && !usbRemovalPending) {
    autoNextPending = false;
    NerdWaveLog::output.println("[AUTO] Suche naechste MP3 im aktuellen Ordner");
    if (changeTrack(+1)) {
      page = 1;
      NerdWaveLog::output.println("[AUTO] Naechster Titel gestartet");
    } else {
      NerdWaveLog::output.println("[AUTO] Ordnerende erreicht - Wiedergabe beendet");
      ampOff();
    }
    redraw = true;
  }
  nav.update();volEncoder.update();
  int vs=volEncoder.takeSteps()+irVolumeStep;irVolumeStep=0;
  // In der Kopieransicht gehoeren Drehung und Klick dem VOL-Encoder,
  // ausserhalb bleiben Lautstaerke und Mute unveraendert.
  if(vs && page!=6) {
    NerdWaveVolume::changeBy(vs*2, millis());
    redraw=true;
  }
  if(volumeDirty && (uint32_t)(millis()-volumeChangedAt)>=1500) saveVolumeNow();
  ButtonEvent volButton=volEncoder.takeButton();
  if(irToggleMute || (volButton==ButtonEvent::Short && page!=6)) {
    irToggleMute=false;NerdWaveVolume::toggleMute();redraw=true;
  }
  if(volButton==ButtonEvent::Double && page==6) {
    NerdWaveCopyUi::returnToPlayer(page,mediaStage,mediaInfo,mediaNotice,redraw,browserActivityAt);
  } else if(volButton==ButtonEvent::Double) {
    mediaReturnPage=page;
    mediaIndex=0;mediaCopyMode=0;
    mediaNotice=false;mediaInfo=false;mediaStage=0;
    page=6;redraw=true;
    NerdWaveCopyUi::markActivity();
  }
  if(volButton==ButtonEvent::Long && page!=0 && page!=6) {
    page=page==2?1:2;redraw=true;
  }
  int step=nav.takeSteps()+irVirtualStep;irVirtualStep=0;
  ButtonEvent press=nav.takeButton();
  if(irVirtualPress!=ButtonEvent::None){press=irVirtualPress;irVirtualPress=ButtonEvent::None;}
  if(irOpenBrowser){irOpenBrowser=false;focusBrowserOnCurrentTrack();page=0;browserActivityAt=millis();redraw=true;}
  if(irOpenSettings){irOpenSettings=false;if(page!=3 && page!=4 && page!=5 && page!=6){settingsReturnPage=page;page=3;settingsIndex=0;resetConfirm=0;settingsActivityAt=millis();redraw=true;}}
  if(irTogglePause){irTogglePause=false;if(musicSource==2){btSend("TOGGLE");redraw=true;}else if(playerRunning||playerActive){NerdWavePlayerControl::togglePause();redraw=true;}else if(selectedMp3Path.length()){startTrack(selectedMp3Path,playingName);redraw=true;}}
  if(irTrackStep){int direction=irTrackStep;irTrackStep=0;
    if(musicSource==2){btSend(direction>0?"NEXT":"PREV");direction=0;redraw=true;}
    if(direction) {
    bool restart=direction<0 && bytesPlayed>(uint64_t)3*audioSampleRate*4;
    if(changeTrack(direction,restart)){page=1;redraw=true;}}}

  bool openedSettings=false;
  // Im Browser gilt jede Encoder- und Tasteraktivitaet als Bedienung.
  if (page==0 && (step!=0 || press!=ButtonEvent::None || vs!=0 || volButton!=ButtonEvent::None)) {
    browserActivityAt=millis();
  }
  // NAV-Doppelklick aus Browser, Player oder Diagnose: Einstellungen.
  if (page!=3 && page!=4 && page!=5 && page!=6 && press==ButtonEvent::Double) {
    settingsReturnPage=page;
    page=3;
    settingsIndex=0;
    resetConfirm=0;
    resetConfirmYes=false;
    settingsActivityAt=millis();
    redraw=true;
    openedSettings=true;
    press=ButtonEvent::None;
  }
  if(page==6) {
    // VOL und NAV erhalten in der Kopieroberflaeche dieselbe Bedienfunktion.
    // Bei gleichzeitigem Tastendruck hat VOL Vorrang.
    if(press==ButtonEvent::Double) {
      NerdWaveCopyUi::returnToPlayer(page,mediaStage,mediaInfo,mediaNotice,redraw,browserActivityAt);
    } else {
    const int copySteps=vs+step;
    const ButtonEvent copyButton=volButton!=ButtonEvent::None?volButton:press;
    if(copySteps || copyButton!=ButtonEvent::None) NerdWaveCopyUi::markActivity();
    const int oldCopyStage=mediaStage;
    NerdWaveCopyUi::Context copyUi{mediaStage, mediaChoice, redraw, mediaOrigin, sdMounted, mediaPath, mediaCopyMode, mediaChosenPath, mediaChosenName, mediaDestination, mediaSelection, mediaList, mediaIndex, mediaInfo, mediaReturnPage, browserActivityAt, musicSource, selectedMp3Path, lastTrackPath, mediaResult, page, sdTryMount, usbTryMount, usbIsMounted, copyLoadFolder, copyListAt, copyExecute};
    NerdWaveCopyUi::handle(copyUi,copySteps,copyButton);
    if(mediaStage==4 && oldCopyStage!=4) NerdWaveCopyUi::markActivity();
    }
    if(page==6 && NerdWaveCopyUi::idleExpired())
      NerdWaveCopyUi::returnToPlayer(page,mediaStage,mediaInfo,mediaNotice,redraw,browserActivityAt);
  } else if(page==5) {
    NerdWaveUiControls::handleLed(step, press, page, ledEditing, ledSettingsIndex,
                                  settingsActivityAt, redraw);
  } else if(page==4) {
    NerdWaveUiControls::handleIr(step, press, page, settingsActivityAt, redraw);
  } else if(page==3) {
    if(step!=0 || press!=ButtonEvent::None || vs!=0 || volButton!=ButtonEvent::None)
      settingsActivityAt=millis();
    if(openedSettings) step=0;
    if(resetConfirm) {
      if(step) {resetConfirmYes = (step>0); redraw=true;}
      if(press==ButtonEvent::Long) {
        resetConfirm=0; resetConfirmYes=false; redraw=true;
      } else if(press==ButtonEvent::Short) {
        if(resetConfirmYes) performReset(resetConfirm==2);
        resetConfirm=0; resetConfirmYes=false; redraw=true;
      }
    } else {
      if(step) {settingsIndex=NerdWaveMenus::wrapIndex(settingsIndex,step,NerdWaveMenus::settingsCount());redraw=true;}
      if(press==ButtonEvent::Long) {page=settingsReturnPage;browserActivityAt=millis();redraw=true;}
      else if(press==ButtonEvent::Short) {
        const int action=NerdWaveMenus::settingsAction(settingsIndex);
        if(action==0) {
          NerdWaveSources::queueNext();
          NerdWaveLog::output.printf("[QUELLE] Vorgemerkt: %s (ohne Reboot)\n",sourceName(requestedMusicSource));
        }
        else if(action==1) {
          if(musicSource==2 && !sourceSwitchPending) btSend(btPairing?"PAIR_OFF":"PAIR_ON");
          else NerdWaveLog::output.println("[BT] Koppeln erst nach Wechsel zur BT-Quelle");
        }
        else if(action==2) NerdWavePlayerControl::toggleRepeat();
        else if(action==3) {NerdWavePlayerControl::cycleShuffle(); playingFolder=browser.path();}
        else if(action==4) autoStartLast=!autoStartLast;
        else if(action==5) {
          networkMode=networkMode==0?1:0;
          // Aktiven Netzwerk-Treiber erst beim Neustart umstellen.
          prefs.putBool("lanOn",networkMode==0);
          NerdWaveSettings::saveNetworkMode(prefs, networkMode);
          networkRestartPending=(networkMode!=bootNetworkMode);
          NerdWaveLog::output.printf("[NET] Neuer Modus %s: Neustart beim Verlassen der Einstellungen\n",NerdWaveSettings::modeName(networkMode));
        }
        else if(action==6) {page=5;ledSettingsIndex=0;ledEditing=false;}
        else if(action==7) {page=4;irLearnIndex=0;irCapture=false;}
        else if(action==8 || action==9) {
          resetConfirm=(action==8)?1:2;
          resetConfirmYes=false;
        }
        else if(action==10) {page=settingsReturnPage;browserActivityAt=millis();}
        // action 11: read-only actual network address, no action on click.
        if(action>=2 && action<=4) saveSettings();
        redraw=true;
      }
    }
  } else if(page==0) {
    if(step && musicSource!=2) {browser.move(step);redraw=true;}
    if(press==ButtonEvent::Long && musicSource!=2) {browser.back();redraw=true;}
    if(press==ButtonEvent::Short) {
      if(musicSource==2) {page=1;}
      else if(browser.error().length()) browser.retry();
      else {
        const MusicItem* item=browser.current();
        if(item) {
          String name=item->name,path=item->path;
          if(item->directory) browser.activate();
          else if(name.endsWith(".mp3")||name.endsWith(".MP3")||name.endsWith(".Mp3")) {
            if(startBrowserTrack(path,name)) page=1;
          } else NerdWaveLog::output.println("[BROWSER] Derzeit nur MP3");
        }
      }
      redraw=true;
    }
  } else {
    if (press == ButtonEvent::Long) {
      // Beim Rueckweg spielt der aktuelle Titel weiter.
      focusBrowserOnCurrentTrack();
      page=0; browserActivityAt=millis(); redraw=true;
    } else if (press == ButtonEvent::Short && musicSource==2) {
      btSend("TOGGLE");redraw=true;
    } else if (press == ButtonEvent::Short && (playerRunning || playerActive)) {
      NerdWavePlayerControl::togglePause();
      redraw=true;
    } else if (press == ButtonEvent::Short && musicSource!=2 && selectedMp3Path.length()) {
      // Nach einem Kopiervorgang existiert kein pausierter Audio-Task mehr.
      // Play startet deshalb den gemerkten Titel erneut.
      startTrack(selectedMp3Path,playingName);
      redraw=true;
    }
    if (step && musicSource==2) {
      for(int i=0;i<min(abs(step),8);++i) btSend(step>0?"NEXT":"PREV");
      redraw=true;
    }
    if (step && musicSource!=2) {
      const int direction = step > 0 ? 1 : -1;
      const int count = min(abs(step), 8);
      for (int i=0; i<count; ++i) {
        bool restart = direction < 0 && bytesPlayed > (uint64_t)3*audioSampleRate*4;
        if (!changeTrack(direction, restart)) break;
      }
      page=1; redraw=true;
    }
  }
  if(sourceSwitchPending && page!=3) {
    if(applyMusicSource(requestedMusicSource)) NerdWaveSources::clearPending();
    else {NerdWaveSources::cancelPending();redraw=true;}
  }
  // Netzmodus weiterhin wie zuvor nur nach Neustart aendern.
  if(networkRestartPending && page!=3) {
    NerdWaveLog::output.println("[NET] Modus gespeichert, Neustart ...");
    ampOff();stopRequested=true;autoNextEnabled=false;
    saveVolumeNow();prefs.end();Serial.flush();delay(250);ESP.restart();
  }
  NerdWaveUiControls::returnToPlayerWhenIdle(page, resetConfirm,
      playerRunning, musicSource, btPlaying, pauseRequested,
      (musicSource!=2 && selectedMp3Path.length()>0 &&
       !(musicSource==1 && !usbDisk.mounted()) &&
       !(musicSource==3 && !sdMounted)),
      settingsActivityAt, browserActivityAt, BROWSER_RETURN_MS, redraw);
  if(webSeeking && (playerActive || (decodeDone && !decoderRunning))) {
    webSeeking=false;
    webSeekDisplayDuration=0;
  }
  if(redraw || ((page==0 || page==1 || page==2) && millis()-lastUi>=(page==0?40U:750U)) ||
     (page==0 && musicSource==0 && nasRecoveryPending && browser.error().length() && millis()-lastUi>=1000)) {
    if(page==0 && musicSource==0 && nasRecoveryPending && browser.error().length()) redraw=true;
    lastUi=millis();NerdWaveDisplayController::Context view{tft, page, redraw, bytesPlayed, playbackBaseSeconds, audioSampleRate, estimatedDurationSec, musicSource, muted, volumePercent, nerdUseWifi, repeatAll, shuffleMode, playingName, btConnected, btPlaying, btPairing, playbackPaused, playerActive, startingTrack, decodeDone, browser, lastPage, lastProgress, lastVolume, shownPlayerTitle, shownTime, shownSource, shownState, shownWifiStatus, shownRepeat, shownShuffle, mediaStage, mediaChoice, mediaOrigin, mediaDestination, mediaCopyMode, mediaIndex, mediaSelection, settingsIndex, mediaInfo, sdMounted, ledEditing, ledEnabled, irCapture, resetConfirmYes, autoStartLast, resetConfirm, requestedMusicSource, networkMode, ledEffect, ledBrightness, ledBaseBrightness, ledSensitivity, ledSettingsIndex, ledCount, mediaPath, mediaChosenName, mediaResult, mediaList, underruns, i2sErrors, maxI2sWriteUs, ringLevel(), copyListAt}; NerdWaveDisplayController::render(view);
  }
  pollDisplayDiagnostics();
  delay(1);
}
