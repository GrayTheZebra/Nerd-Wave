#include "NerdWaveLog.h"
#include "NerdWaveWeb.h"
#include "NerdWaveSources.h"
#include "NerdWaveMqtt.h"
#include "NerdWaveNasConfig.h"
#include "NerdWaveSettings.h"
#include "NerdWaveNetwork.h"
#include "NerdWaveSkins.h"
#include "NerdWaveFields.h"
#include "NerdWaveSkinLibrary.h"
#include "NerdWaveWifiFilter.h"
#include "MusicApi.h"
#include "Config.h"
#include "LedController.h"
#include <WebServer.h>
#include <WiFi.h>
#include <EthernetESP32.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace NerdWaveWeb {
namespace {
WebServer server(80);
Context* c=nullptr;
String password;
String username="admin";
bool authEnabled=false;
bool started=false;
bool otaActive=false;
bool otaSuccess=false;
bool otaRebootPending=false;
uint32_t otaRebootAt=0;
size_t otaExpected=0;
size_t otaWritten=0;
String otaError;

const esp_partition_t* otaPartition(){
  const esp_partition_t* target=esp_ota_get_next_update_partition(nullptr);
  const esp_partition_t* running=esp_ota_get_running_partition();
  // OTA darf niemals die gerade laufende Firmware ueberschreiben.
  return (target && running && target->address!=running->address &&
          target->type==ESP_PARTITION_TYPE_APP)?target:nullptr;
}
bool otaBusy(){return c && (c->playerActive || c->btPlaying);}

const char WEB_PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Nerd-Wave</title><style>
:root{color-scheme:dark;font:16px system-ui}body{margin:0;background:#101923;color:#f3f7fb}main{max-width:1480px;margin:auto;padding:16px 22px}h1{color:#43d9df}section{background:#1c2a37;padding:18px;border-radius:16px;margin:16px 0}button,input,select{font:inherit;border:1px solid #4c6976;border-radius:10px;padding:11px;background:#122331;color:white}button{cursor:pointer;background:#174e64}button:active{background:#317e93}.browserDivider{border-top:1px solid #405665;margin:22px 0 16px}input,select{box-sizing:border-box;width:100%;margin:5px 0 12px}label{display:block;margin-top:10px}.actions{display:flex;gap:10px;flex-wrap:wrap;margin:5px 0}.actions button{flex:1;min-width:0}.note{color:#a7c1ce;font-size:.88rem}#msg{color:#78e6a6}.browserList{display:grid;gap:7px;margin:12px 0;max-height:440px;overflow-y:auto;overscroll-behavior:contain}.browserList button{text-align:left;overflow-wrap:anywhere;background:#173548;width:100%}.browserList button.folder{background:#23445b}.browserList button:disabled{opacity:.55}#state{color:#9ddde8}
.tabs{display:flex;gap:7px;flex-wrap:wrap;position:sticky;top:0;background:#101923;padding:12px 0;z-index:3}.tabs button{flex:1;min-width:105px}.tabs button.active{background:#168096;border-color:#51cbd3}.tabPanel{display:none}.tabPanel.active{display:block}#apList{display:grid;gap:7px}#apList .ap{display:flex;align-items:center;justify-content:space-between;gap:12px;background:#173548;padding:9px;border-radius:9px}#apList button{width:auto;min-width:90px}#apList .ap{flex-wrap:wrap}#apList .apDetails{flex:1;min-width:210px;overflow-wrap:anywhere}#apList .apActions{display:flex;gap:6px;flex-wrap:wrap;align-items:center}#apList .apActions input{margin:0;max-width:230px}
.ledOptional[hidden]{display:none}.seekLine{margin:16px 0}.seekNotice{color:#43d9df;font-size:.88rem;min-height:1.3em;margin:3px 0 8px}.seekLine input{width:100%;cursor:pointer}.timeLabels{display:flex;justify-content:space-between;color:#a7c1ce;font-size:.88rem}.crumbs{display:flex;flex-wrap:wrap;gap:4px;margin:10px 0}.crumbs button{padding:5px 8px;background:#24475a}.browserList button.current{border-color:#43d9df;background:#145367}.ledPreview{display:flex;gap:6px;padding:15px 0}.ledDot{width:18px;height:18px;border-radius:50%;box-shadow:0 0 8px currentColor;flex:1;max-width:26px}
/* Sperrt die gesamte Oberfläche während des OTA-Neustarts. */
#otaOverlay[hidden]{display:none}
#otaOverlay{position:fixed;inset:0;z-index:1000;background:rgba(7,15,24,.93);display:flex;align-items:center;justify-content:center;padding:20px;box-sizing:border-box}
.otaDialog{max-width:420px;width:100%;padding:32px 26px;background:#1c2a37;border:1px solid #356176;border-radius:18px;text-align:center;box-shadow:0 16px 60px #0008}
.otaSpinner{width:50px;height:50px;margin:0 auto 22px;border:5px solid #36566a;border-top-color:#43d9df;border-radius:50%;animation:otaSpin .9s linear infinite}
@keyframes otaSpin{to{transform:rotate(360deg)}}
@media(prefers-reduced-motion:reduce){.otaSpinner{animation-duration:2.5s}}
#otaOverlayMessage{line-height:1.5;color:#b7d3df}
#otaOverlayRetry[hidden]{display:none}
body.otaLocked{overflow:hidden}

/* Grafischer Skin-Editor: Vorschau skaliert im Browser, Firmware bleibt 240x240. */
.skinEditorGrid{display:grid;grid-template-columns:minmax(0,1fr) minmax(190px,1fr);gap:18px;align-items:start;margin:14px 0}
#skinCanvasWrap{min-width:0;display:flex;align-items:center;justify-content:center;padding:15px 5px;background:#101923;border-radius:12px}
#skinCanvas{width:240px;height:240px;max-width:100%;position:relative;overflow:hidden;touch-action:none;background:#050a10;border:1px solid #486070;flex:none}
#skinCanvas.round{border-radius:50%}#skinCanvas.rect{border-radius:8px}
.skinPart{position:absolute;transform:translate(-50%,-50%);white-space:nowrap;cursor:grab;touch-action:none;user-select:none;max-width:95%;overflow:hidden;text-overflow:ellipsis;padding:3px 5px;border:1px dashed transparent;border-radius:4px;font-size:11px;line-height:1.2}
.skinPart.active{border-color:#fff;background:#37607366;cursor:grabbing}
.skinColorPair{display:flex;align-items:center;gap:8px}.skinColorPair input[type=color]{width:56px;min-width:56px;height:40px;padding:3px}.skinColorPair input[type=text]{flex:1;min-width:0}.skinEditorFields{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.skinEditorFields label{margin:0}.skinVisibility{display:flex;align-items:center;gap:8px}.skinVisibility input{width:auto!important}.skinEditorFields input{width:100%;box-sizing:border-box}
#skinLayoutControls{display:none!important}.skinInfo{font-size:.85rem;color:#b1c7d3}.skinEditorActions{display:flex;flex-wrap:wrap;gap:8px}.skinEditorActions button{flex:1}
@media(max-width:700px){.skinEditorGrid{grid-template-columns:1fr}#skinCanvas{margin:auto}}
#freeCanvasWrap{background:#101a24;padding:12px;border-radius:10px;display:flex;justify-content:center;align-items:center;min-height:276px}#freeCanvas{width:240px;height:240px;border:1px solid #4b6474;border-radius:50%;position:relative;background:#000;touch-action:none;overflow:hidden;flex-shrink:0}.freeItem{position:absolute;transform:translate(-50%,-50%);white-space:nowrap;cursor:grab;user-select:none;max-width:235px}.freeItem.selected{outline:1px dashed #6aa8c6;outline-offset:2px}details>summary{cursor:pointer;margin:12px 0}.skinPreviewColumn{display:flex;flex-direction:column;gap:12px;min-width:0}.skinVarHelp{background:#132735;border:1px solid #355263;padding:12px;border-radius:12px}.skinVarHelp p{margin:6px 0 10px}.freeVarChips{display:flex;flex-wrap:wrap;gap:6px}.freeVarChips button{font-size:.78rem;padding:6px 8px;width:auto;border-radius:7px;background:#204558}.freeVarChips button:disabled{opacity:.35;cursor:not-allowed}.skinAddSelect{min-width:130px;flex:1;margin:0}.skinAddSelect select{margin:4px 0 0;padding:9px}.skinSaveFooter{border-top:1px solid #405665;padding:18px 0 4px;margin-top:18px;display:flex;gap:12px;align-items:center;flex-wrap:wrap}.skinSaveButton{background:#168096;font-weight:700;padding:14px 24px;min-width:190px}.skinEditorGrid{grid-template-columns:minmax(300px,1.45fr) minmax(340px,1fr)}.bgRows{display:grid;gap:8px;margin:10px 0}.bgRow{display:grid;grid-template-columns:1fr 1fr 1.2fr auto;gap:8px;align-items:end;padding:8px;border-radius:10px;background:#173548}.bgRow button{width:auto;padding:10px}.inlineFields{display:grid;grid-template-columns:1fr 1fr;gap:10px}.inlineFieldsThree{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px}.bgModeExtra[hidden]{display:none!important} .skinAddSelect{margin:0;flex:1;min-width:135px}.skinAddSelect select{cursor:pointer;background:#174e64;color:#fff;border:1px solid #4c6976;border-radius:10px;padding:11px;min-height:46px;width:100%;margin:0}.skinAddSelect select:hover{background:#216782}.skinAddSelect select:focus{outline:2px solid #43d9df} .skinWorkbench{display:grid;grid-template-columns:minmax(0,1fr) minmax(325px,440px);gap:18px;align-items:start}.skinToolbar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;padding:10px 12px;background:#132938;border:1px solid #365061;border-radius:12px;margin:12px 0}.skinToolbar button{width:auto;padding:9px 13px;flex:none}.skinLayerList{display:grid;gap:5px;margin:8px 0;max-height:230px;overflow:auto}.skinLayerList button{width:100%;padding:9px;text-align:left;background:#152f3e}.skinLayerList button.active{background:#22647b;border-color:#66d9eb}.skinLayerList button{cursor:grab}.skinLayerList button.dragging{opacity:.35}.skinLayerList button.dropTarget{outline:2px dashed #43d9df}.skinProperties{min-width:0}.shapeOnly[hidden],#freeFillControls[hidden]{display:none!important} .skinProperties [hidden]{display:none!important}.skinWorkbench .skinEditorGrid{display:contents}.skinWorkbench .skinPreviewColumn{grid-column:1}.skinWorkbench .skinProperties{grid-column:2;grid-row:1}.skinWorkbench #freeCanvasWrap{min-height:330px}.skinToolbar select{width:auto;min-width:140px;margin:0}.skinCanvasArea{display:flex;justify-content:center;align-items:center;min-height:290px}.skinWorkbench .skinVarHelp{margin-top:12px}@media(max-width:830px){.skinWorkbench{display:block}.skinWorkbench .skinEditorGrid{display:grid;grid-template-columns:1fr}.skinProperties{margin-top:14px}}.skinCompactTop{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:10px;align-items:end}.skinCompactTop label{margin:0}.skinCompactTop select{margin:5px 0 0}.skinCompactTop button{width:auto;min-width:125px;margin-bottom:0}.skinManage,.skinAdvanced{border:1px solid #355263;border-radius:10px;margin:12px 0;background:#152a38}.skinManage summary,.skinAdvanced summary{padding:12px 15px;font-weight:600;cursor:pointer;margin:0}.skinManageBody,.skinAdvancedBody{padding:0 15px 12px}.skinManageBody .skinEditorActions{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:7px}.skinManageBody .skinEditorActions button{min-width:0;padding:10px 5px}.skinManageBody label{margin:5px 0}.skinManageBody input{margin:5px 0}.skinManageBody .note{margin:7px 0}.skinCompactTop+ .note{margin:8px 0}@media(max-width:680px){.skinCompactTop{grid-template-columns:1fr}.skinCompactTop button{width:100%}.skinManageBody .skinEditorActions{grid-template-columns:repeat(2,minmax(0,1fr))}}
/* Player: kompakte Steuerung links, Dateibrowser rechts; auf Mobilgeraeten gestapelt. */
#tab-player .playerLayout{display:grid;grid-template-columns:minmax(0,0.95fr) minmax(0,1.35fr);gap:20px;align-items:start}
#tab-player .playerControls,#tab-player .playerFiles{min-width:0}
#tab-player .playerControls{position:sticky;top:86px}
#tab-player .playerControls #track{margin:12px 0 8px;overflow-wrap:anywhere}
#tab-player .playerControls #trackPath{overflow-wrap:anywhere}
#tab-player .playerControls .seekLine{margin:14px 0 10px}
#tab-player .playerControls .actions{gap:7px;margin:7px 0}
#tab-player .playerControls .actions button{padding:10px 7px}
#tab-player .playerControls label{margin-top:11px}
#tab-player .playerFiles .browserDivider{display:none}
#tab-player .playerFiles label{margin-top:0}
#tab-player .playerFiles #browserInfo{margin:8px 0}
#tab-player .playerFiles #browserList{max-height:min(65vh,740px);overflow:auto}
@media(max-width:900px){#tab-player .playerLayout{display:block}#tab-player .playerControls{position:static}#tab-player .playerFiles{margin-top:20px;padding-top:12px;border-top:1px solid #405665}#tab-player .playerFiles #browserList{max-height:none}}

/* v0.5.200: gruppierte Einstellungen, bestehende Formulare/API bleiben erhalten */
#tab-settings .settingsPanel{max-width:1120px;margin:auto}
#tab-settings .settingsGroup{border:1px solid #3d5868;border-radius:10px;margin:12px 0;background:#192936;overflow:hidden}
#tab-settings .settingsGroup>summary{cursor:pointer;padding:15px 18px;font-weight:650;list-style-position:inside;background:#213646}
#tab-settings .settingsGroup[open]>summary{border-bottom:1px solid #3d5868}
#tab-settings .settingsGroupBody{padding:12px 18px 16px}
#tab-settings .settingsGroupBody>section{padding:0;background:transparent;box-shadow:none;border:0;margin:0}
#tab-settings .settingsGroupBody>section h2{font-size:1.1rem;margin:8px 0}
#tab-settings .settingsGroupBody>section h3{font-size:1rem;margin:8px 0}
#tab-settings .settingsSave{padding:5px 0 8px}
#tab-settings .settingsSave>button{width:auto;min-width:170px}
#tab-settings .sourceFlags{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:7px 16px;padding:10px 14px}
#tab-settings .sourceFlags legend,#tab-settings .sourceFlags .note{grid-column:1/-1}
#tab-settings .sourceFlags label{margin:4px 0;display:flex;align-items:center;gap:8px}
@media(max-width:650px){#tab-settings .sourceFlags{grid-template-columns:1fr}#tab-settings .settingsGroupBody{padding:10px}}
.networkCurrent{background:#132735;border:1px solid #355263;border-radius:10px;padding:12px;margin:0 0 16px}.networkCurrent strong{display:block;margin-bottom:5px}#settingsConnection{font:600 1.1rem/1.5 ui-monospace,SFMono-Regular,Consolas,monospace;overflow-wrap:anywhere;color:#43d9df}
</style></head><body><main><h1>Nerd-Wave</h1><nav class="tabs" aria-label="Bereiche"><button class="active" data-tab="player" onclick="showTab('player')">Player</button><button data-tab="settings" onclick="showTab('settings')">Einstellungen</button><button data-tab="led" onclick="showTab('led')">LED</button><button data-tab="network" onclick="showTab('network')">Netzwerk</button><button data-tab="display" onclick="showTab('display')">Display</button><button data-tab="system" onclick="showTab('system')">System</button></nav><div class="tabPanel active" id="tab-player"><section class="playerLayout"><div class="playerControls"><div hidden aria-hidden="true"><input type="checkbox" id="repeat"><select id="shuffle"><option value="0">Aus</option><option value="1">Ordner</option><option value="2">Rekursiv</option></select></div><div id="state">Verbinde ...</div><h3 id="track">–</h3><div id="trackPath" class="note"></div><div class="seekLine"><input id="seek" aria-label="Wiedergabeposition" type="range" min="0" max="100" value="0" disabled><div class="timeLabels"><span id="elapsed">0:00</span><span id="duration">--:--</span></div><p id="seekNotice" class="seekNotice" role="status" aria-live="polite"></p></div><div class="actions"><button onclick="act('prev')">◀◀</button><button onclick="act('toggle')">▶ / ❚❚</button><button onclick="act('next')">▶▶</button></div><div class="actions"><button type="button" id="repeatBtn" onclick="toggleQuick('repeat')">Repeat: Aus</button><button type="button" id="shuffleBtn" onclick="toggleQuick('shuffle')">Shuffle: Aus</button></div><label>Lautstärke <span id="voltext"></span><input type="range" min="0" max="100" id="volume" onchange="act('volume',this.value)"></label></div><div class="playerFiles"><label>Quelle<select id="source" onchange="act('source',this.value)"><option value="0">NAS</option><option value="1">USB</option><option value="3">SD</option><option value="2">BT</option></select></label><div class="browserDivider"></div><div class="actions"><button type="button" onclick="browseReload()">⟳ Aktualisieren</button></div><div id="breadcrumbs" class="crumbs"></div><label class="note">Titel und Ordner filtern<input id="browserSearch" type="search" placeholder="In diesem Ordner suchen"></label><p id="browserInfo" class="note">Lade Verzeichnis ...</p><div id="browserList" class="browserList"></div></div></section></div><div class="tabPanel" id="tab-settings"><section class="settingsPanel"><h2>Einstellungen</h2><p class="note">Konfiguration nach Bereichen. Geräteeinstellungen werden automatisch gespeichert; Zugang und MQTT besitzen eigene Speichern-Schaltflächen.</p>
<form id="settings">
<details class="settingsGroup" open><summary>NAS und Verbindung</summary><div class="settingsGroupBody"><div class="networkCurrent" role="status" aria-live="polite"><strong>Aktuelle Nerd-Wave-Verbindung</strong><div id="settingsConnection">Verbindung wird ermittelt …</div><div class="note">Dies ist die Geräte-IP, nicht die NAS-Adresse. Aktualisierung alle 2,5 Sekunden.</div></div><label>NAS-IP-Adresse<input name="host" id="host" inputmode="decimal" required></label><label>NAS-API-Port<input type="number" name="port" id="port" min="1" max="65535" required></label><label>Netzwerk<select id="net" name="net"><option value="0">LAN</option><option value="1">WLAN</option></select></label></div></details>
<details class="settingsGroup" open><summary>Gerät und Einschaltverhalten</summary><div class="settingsGroupBody"><label><input type="checkbox" id="autostart" name="autostart" style="width:auto"> Letzten Titel automatisch starten</label><label><input type="checkbox" id="powerSwitch" name="powerSwitch" style="width:auto"> Hardware-Ein/Aus-Schalter (GPIO21) aktivieren</label><p class="note">Standardmäßig aus. Rastschalter zwischen GPIO21 und GND: geschlossen = EIN. Bei AUS werden laufende Kopiervorgaenge zuerst abgeschlossen. Danach erscheint eine Verabschiedung und Deep Sleep wird aktiviert. Die Stromversorgung bleibt angeschlossen.</p></div></details>
<details class="settingsGroup"><summary>Verfügbare Musikquellen</summary><div class="settingsGroupBody"><fieldset class="sourceFlags"><legend>Verfügbare Musikquellen</legend><p class="note">Deaktiviere Quellen, für die kein Modul verbaut ist. Ein Neustart übernimmt die Hardwarekonfiguration.</p><label><input type="checkbox" id="srcNas" name="srcNas" value="1"> NAS / Netzwerk</label><label><input type="checkbox" id="srcUsb" name="srcUsb" value="1"> USB-Host</label><label><input type="checkbox" id="srcSd" name="srcSd" value="1"> SD-Karte</label><label><input type="checkbox" id="srcBt" name="srcBt" value="1"> Bluetooth (separater ESP32-WROOM)</label></fieldset></div></details>
<div class="settingsSave"><button type="submit">Jetzt speichern</button><p class="note">Änderungen werden automatisch gespeichert. Bei ungültigen NAS-Daten bleibt die Eingabe ungespeichert.</p><p id="msg"></p><p class="note">Ein geänderter Netzwerkmodus wird nach einem Neustart aktiv. NAS-Einstellungen gelten für neue Verbindungen; die laufende Wiedergabe wird nicht unterbrochen.</p></div></form>
<details class="settingsGroup"><summary>Webzugang und Passwortschutz</summary><div class="settingsGroupBody"><section><h2>Webzugang</h2><h3>Zugriffsschutz</h3><p id="securityState" class="note">Status wird geladen ...</p><form id="security"><label><input type="checkbox" id="securityEnabled" style="width:auto"> Passwortschutz aktivieren</label><label>Benutzername<input id="securityUsername" autocomplete="username" maxlength="32" placeholder="admin" required></label><label>Neues Passwort (mindestens 8 Zeichen)<input type="password" id="securityPassword" autocomplete="new-password" minlength="8" placeholder="Bei Aktivierung erforderlich"></label><button type="submit">Zugriffsschutz speichern</button></form><p id="securityMsg"></p><p class="note">Ab Werk ohne Passwortschutz. Benutzername und Passwort sind frei konfigurierbar. Der Schutz gilt für die gesamte Weboberfläche und ihre API.</p></section></div></details>
<details class="settingsGroup"><summary>MQTT und Home Assistant</summary><div class="settingsGroupBody"><section><h2>MQTT und Home Assistant</h2><p class="note">MQTT ist optional und standardmäßig aus. Nach dem Verbinden wird der Player automatisch in Home Assistant angemeldet (MQTT Discovery).</p><form id="mqttSettings"><label><input type="checkbox" id="mqttEnabled" style="width:auto"> MQTT aktivieren</label><label>Broker-IP oder Hostname<input id="mqttHost" maxlength="120" placeholder="192.168.0.x"></label><label>Port<input id="mqttPort" type="number" min="1" max="65535" value="1883"></label><label>Benutzername (optional)<input id="mqttUser" autocomplete="username"></label><label>Passwort (leer lassen zum Beibehalten)<input id="mqttPassword" type="password" autocomplete="new-password"></label><label><input type="checkbox" id="mqttClearPass" style="width:auto"> Gespeichertes Passwort entfernen</label><label>Topic-Präfix<input id="mqttPrefix" value="nerdwave" maxlength="56"></label><button type="submit">MQTT speichern</button></form><p id="mqttState" class="note"></p><p class="note">MQTT-Broker bitte im vertrauenswürdigen Heimnetz betreiben. Die Verbindung nutzt unverschlüsseltes MQTT/TCP.</p></section></div></details>
</section></div><div class="tabPanel" id="tab-led"><section><h2>LED-Beleuchtung</h2><p class="note">LED-Stripe einrichten und Effekte anpassen.</p><form id="ledSettings"><h3>LED-Stripe</h3><div id="ledPreview" class="ledPreview" aria-label="Vorschau des gewählten Effekts"></div><p class="note">Die Animation zeigt das Prinzip des Effekts; Musikpegel werden simuliert.</p><label><input type="checkbox" id="ledEnabled" name="ledEnabled" style="width:auto"> LEDs aktiv</label><label>Anzahl<input type="number" id="ledCount" name="ledCount" min="1" max="300"></label><label>LED-Effekt<select id="ledEffect" name="ledEffect"><option value="0">VU-Meter</option><option value="1">Dual-VU</option><option value="2">Pulsieren (Musik)</option><option value="3">Spektrum (3 Bänder)</option><option value="4">Dauerlicht</option><option value="5">Rainbow-Cycle</option><option value="6">Rainbow-Cycle (Musik)</option><option value="7">Rainbow – alle LEDs gemeinsam</option><option value="8">Rainbow – alle LEDs gemeinsam (Musik)</option></select></label><label class="ledOptional" id="ledColorRow">Farbe für Dauerlicht<input type="color" id="ledColor" name="ledColor" value="#00bfff"></label><label class="ledOptional" id="ledSpeedRow">Rainbow-Geschwindigkeit<input type="range" id="ledSpeed" name="ledSpeed" min="1" max="100" value="35"><span id="ledSpeedText">35</span></label><label>Helligkeit<input type="number" id="ledBrightness" name="ledBrightness" min="0" max="100"></label><label>Grundhelligkeit<input type="number" id="ledBaseBrightness" name="ledBaseBrightness" min="0" max="20"></label><label>Empfindlichkeit<input type="number" id="ledSensitivity" name="ledSensitivity" min="1" max="100"></label><button type="submit">Jetzt speichern</button><p class="note">Änderungen wirken sofort und werden nach kurzer Pause gespeichert.</p></form><p id="ledMsg"></p></section></div><div class="tabPanel" id="tab-network"><section><h2>Netzwerkstatus</h2><p id="network"></p><h3>WLAN-Zugangspunkte</h3><p class="note">Mesh-Knoten benennen und bei Bedarf sperren. Gesperrte Zugangspunkte werden auch bei späteren Verbindungen vermieden. Für die Suche stoppt Nerd-Wave die Wiedergabe automatisch.</p><button type="button" onclick="scanAps()">WLAN-Zugangspunkte suchen</button><p id="apInfo" class="note"></p><div id="apList"></div><p class="note">Wenn der derzeit verbundene Knoten gesperrt wird, sucht Nerd-Wave nach einem erlaubten Zugangspunkt. Halte mindestens einen Zugangspunkt frei.</p></section></div><div class="tabPanel" id="tab-display"><section><h2>Skin-Editor</h2><p class="note">Standarddesign oder eigenen Skin auswählen, anwenden und bearbeiten.</p><div class="skinCompactTop"><label>Skin auswählen<select id="skinChoice" onchange="skinChoiceChanged()"><optgroup label="Standardskins"><option value="classic">Classic Cyan</option><option value="amber">Retro Amber</option></optgroup><optgroup label="Eigene Skins" id="skinLibraryOptions"></optgroup><option value="custom">Aktueller eigener Entwurf</option></select></label><button type="button" onclick="skinSelect()">Anwenden</button></div><p id="skinInfo" class="note"></p><details class="skinManage"><summary>Skins verwalten</summary><div class="skinManageBody"><label>Skin-Name<input id="libName" maxlength="32" placeholder="Mein Radiodesign"></label><div class="skinEditorActions"><button type="button" onclick="libSave(false)">Neu speichern</button><button type="button" onclick="libSave(true)">Aktualisieren</button><button type="button" onclick="libDuplicate()">Duplizieren</button><button type="button" onclick="libDelete()">Löschen</button></div><p id="libInfo" class="note"></p></div></details><select id="libSelect" hidden aria-hidden="true"></select><details class="skinAdvanced"><summary>Hintergrund und Design-Einstellungen</summary><div class="skinAdvancedBody"><h3>Display-Hintergrund</h3><p class="note">Solid, Farbverlauf oder abgestufte Bereiche. Änderungen wirken live; "Hintergrund speichern" übernimmt sie dauerhaft.</p><label>Hintergrundmodus<select id="bgMode" onchange="bgModeChanged()"><option value="solid">Vollflächige Farbe</option><option value="gradient">Farbverlauf</option><option value="bands">Abgestufte Bereiche</option></select></label><div id="bgGradientWrap" class="bgModeExtra"><div class="inlineFields"><label>Farbe A<div class="skinColorPair"><input id="bgFirstPicker" type="color" onchange="bgColorSync('bgFirst')"><input id="bgFirst" maxlength="7" oninput="bgEdit()"></div></label><label>Farbe B<div class="skinColorPair"><input id="bgSecondPicker" type="color" onchange="bgColorSync('bgSecond')"><input id="bgSecond" maxlength="7" oninput="bgEdit()"></div></label></div></div><div id="bgDirectionWrap" class="bgModeExtra"><label>Richtung<select id="bgDirection" onchange="bgEdit()"><option value="vertical">Vertikal</option><option value="horizontal">Horizontal</option></select></label></div><div id="bgSolidWrap" class="bgModeExtra"><label>Farbe<div class="skinColorPair"><input id="bgSolidPicker" type="color" onchange="bgColorSync('bgSolid')"><input id="bgSolid" maxlength="7" oninput="bgEdit()"></div></label></div><div id="bgBandsWrap" class="bgModeExtra"><div class="actions"><button type="button" onclick="bgAddBand()">+ Bereich</button><button type="button" onclick="bgNormalizeBands()">Bereiche verteilen</button></div><div id="bgRows" class="bgRows"></div><p class="note">Bereiche gelten je nach Richtung von oben nach unten oder von links nach rechts. Maximal 4 Bereiche.</p></div><div class="actions"><button type="button" onclick="bgSave()">Hintergrund speichern</button></div><p class="note" id="bgMessage"></p></div></details><details class="skinAdvanced"><summary>Import und Export</summary><div class="skinAdvancedBody"><h3>Skin-Template</h3><p class="note">Hintergrund und alle Text-/Symbolelemente gemeinsam exportieren, importieren und auf Nerd-Wave speichern. Ältere reine Feld-Templates bleiben importierbar.</p><div class="actions"><button type="button" onclick="skinBundleExport()">Skin exportieren</button></div><div class="actions"><button type="button" onclick="document.getElementById('skinBundleFile').click()">Skin importieren</button></div><input type="file" id="skinBundleFile" accept=".json,application/json" onchange="skinBundleImport(event)" style="display:none"><p class="note">Import übernimmt Hintergrund und sämtliche Text-/Symbolelemente zunächst als Vorschau. Danach „Skin speichern“ wählen.</p><p id="skinBundleMessage" class="note"></p></div></details><h3>Freie Displayfelder und Ebenen</h3><div class="skinToolbar"><button type="button" onclick="freeAdd()">+ Text</button><select id="toolbarAdd" aria-label="Element hinzufügen" onchange="freeToolbarAdd()"><option value="">+ Element hinzufügen</option><optgroup label="Symbole"><option value="icon:wifi">WLAN</option><option value="icon:repeat">Repeat</option><option value="icon:shuffle">Shuffle</option></optgroup><optgroup label="Anzeigen"><option value="gauge:progress">Fortschritt</option><option value="gauge:volume">Lautstärke</option></optgroup><optgroup label="Formen"><option value="shape:rect">Rechteck</option><option value="shape:ellipse">Ellipse / Kreisausschnitt</option></optgroup></select><button type="button" onclick="freeDelete()">Löschen</button></div><p class="note">Text- und Symbolfelder hinzufügen, ziehen und mit Variablen belegen. Änderungen erscheinen live und werden über „Skin speichern“ am Ende dauerhaft übernommen.</p><div class="skinWorkbench"><div class="skinEditorGrid"><div class="skinPreviewColumn"><div id="freeCanvasWrap" class="skinCanvasArea"><div id="freeCanvas" aria-label="Freies Player-Layout"></div></div><div><strong>Ebenen (unten → oben)</strong><p class="note">Ebenen per Drag-and-drop sortieren.</p><div id="skinLayerList" class="skinLayerList"></div></div><div class="skinVarHelp"><strong>Variablen einfügen</strong><p class="note">Klicken, um den Platzhalter am Ende des ausgewählten Textfelds anzufügen.</p><div id="freeVarChips" class="freeVarChips"></div></div></div><div class="skinProperties"><label>Feld auswählen<select id="freeSelect" onchange="freeSelectChange()"></select></label><label id="freeTextLabel">Text / Platzhalter<input id="freeText" maxlength="120" oninput="freeEdit()" placeholder="%titel%"></label><label id="freeMaxWidthRow">Maximale Textbreite (Pixel)<input id="freeMaxWidth" type="number" min="12" max="239" value="230" oninput="freeEdit()"><span class="note">Lange dynamische Texte werden mit .. gekürzt.</span></label><div class="skinEditorFields"><label>X-Position<input id="freeX" type="number" min="0" max="239" oninput="freeCoordinateChange()"></label><label>Y-Position<input id="freeY" type="number" min="0" max="239" oninput="freeCoordinateChange()"></label></div><div id="shapeOptions" class="shapeOnly" hidden><div class="skinEditorFields"><label>Formbreite<input id="shapeWidth" type="number" min="4" max="239" onchange="freeShapeEdit()"></label><label>Formhöhe<input id="shapeHeight" type="number" min="4" max="239" onchange="freeShapeEdit()"></label></div><div class="skinEditorFields"><label>Randbreite (Pixel)<input id="shapeBorderWidth" type="number" min="0" max="20" onchange="freeShapeEdit()"></label><label>Eckenradius (Pixel)<input id="shapeRadius" type="number" min="0" max="110" onchange="freeShapeEdit()"></label></div><div id="shapeArcOptions" hidden><div class="skinEditorFields"><label>Startwinkel (0–359°)<input id="shapeArcStart" type="number" min="0" max="359" onchange="freeShapeEdit()"></label><label>Bogenumfang (1–360°)<input id="shapeArcSweep" type="number" min="1" max="360" onchange="freeShapeEdit()"></label></div><p class="note">0° oben, 90° rechts. Bei weniger als 360° entsteht ein gefülltes Tortenstück oder ein offener Ellipsenbogen.</p></div><p class="note">Textfarbe = Randfarbe; Hintergrundfarbe = Füllfarbe. Eckenradius 0 ergibt ein normales Rechteck.</p></div><div id="gaugeOptions" hidden><label>Darstellung<select id="gaugeStyle" onchange="freeGaugeEdit()"><option value="linear">Linear</option><option value="segments">Segmentiert</option><option value="ring">Kreis / Teilkreis</option></select></label><div class="skinEditorFields"><label>Breite / Durchmesser<input id="gaugeWidth" type="number" min="12" max="200" onchange="freeGaugeEdit()"></label><label>Dicke (Pixel)<input id="gaugeHeight" type="number" min="2" max="80" onchange="freeGaugeEdit()"></label></div><div id="gaugeAngleRow" class="skinEditorFields"><label>Startwinkel (0–359°)<input id="gaugeStart" type="number" min="0" max="359" oninput="freeGaugeEdit()"></label><label>Bogenumfang (1–360°)<input id="gaugeSweep" type="number" min="1" max="360" oninput="freeGaugeEdit()"></label></div><p class="note" id="gaugeArcHelp">0° = oben, 90° = rechts, 180° = unten. Beispiel: 270° Start / 180° Umfang = Halbkreis.</p></div><label id="freeSizeRow">Größe<select id="freeSize" onchange="freeEdit()"><option value="1">Klein</option><option value="2">Mittel</option><option value="3">Groß</option></select></label><label>Text-/Randfarbe (HEX)<div class="skinColorPair"><input id="freePicker" type="color" onchange="freePickerChange()"><input id="freeHex" maxlength="7" oninput="freeEdit()"></div></label><div id="freeFillControls"><label class="skinVisibility"><input type="checkbox" id="freeBgEnabled" onchange="freeEdit()"> Füllung / Hintergrund anzeigen</label><label>Füllfarbe (HEX)<div class="skinColorPair"><input id="freeBgPicker" type="color" onchange="freeBgPickerChange()"><input id="freeBgHex" maxlength="7" oninput="freeEdit()"></div></label></div><div class="skinEditorActions"><button type="button" onclick="freeCenter('x')">Horizontal zentrieren</button><button type="button" onclick="freeCenter('y')">Vertikal zentrieren</button></div><p class="note" id="freeMessage"></p></div></div></div><div class="skinSaveFooter"><button type="button" class="skinSaveButton" onclick="skinBundleSave()">Skin speichern</button><span class="note">Speichert Hintergrund und alle Displayfelder.</span></div></section></div><div class="tabPanel" id="tab-system"><section><h2>Live-Konsole</h2><p class="note">Firmware-Ausgabe wie im seriellen Monitor. Die letzten 16 KB bleiben im RAM; Neustarts leeren den Puffer.</p><div class="actions"><button type="button" id="logPause" onclick="logToggle()">Pausieren</button><button type="button" onclick="logClear()">Anzeige leeren</button><button type="button" onclick="logCopy()">Kopieren</button></div><p class="note" id="logState">Konsole wird geladen ...</p><pre id="serialLog" style="background:#09131b;color:#b4e4d1;border:1px solid #355065;border-radius:9px;padding:12px;height:330px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;font:12px/1.4 monospace"></pre></section><section><h2>Firmware aktualisieren (OTA)</h2><p id="otaInfo" class="note">Partitionen werden geprüft ...</p><p class="note">Nur die kompilierte ESP32-S3-Firmware (.bin) aus der Arduino IDE verwenden. Die Wiedergabe wird beim Upload automatisch beendet. Wähle in der IDE eine Partitionstabelle mit zwei OTA-App-Slots und behalte diese für spaetere Updates bei. Partitionstabellen lassen sich nicht per OTA ändern.</p><form id="otaForm"><label>Firmware (.bin)<input id="otaFile" type="file" accept=".bin" required></label><button id="otaButton" type="submit">Firmware hochladen</button></form><progress id="otaProgress" max="100" value="0" style="width:100%;display:none"></progress><p id="otaMessage" class="note"></p></section><section><h2>Hinweis zum Zugriff</h2><p class="note">Nerd-Wave ist nur für dein vertrauenswürdiges Heimnetz gedacht. HTTP verschlüsselt keine Zugangsdaten.</p></section></div></main><div id="otaOverlay" role="alertdialog" aria-modal="true" aria-labelledby="otaOverlayTitle" aria-describedby="otaOverlayMessage" hidden><div class="otaDialog"><div class="otaSpinner" aria-hidden="true"></div><h2 id="otaOverlayTitle">Nerd-Wave startet neu</h2><p id="otaOverlayMessage">Firmware erfolgreich übertragen. Verbindung wird wiederhergestellt …</p><button type="button" id="otaOverlayRetry" onclick="retryOtaCheck()" hidden>Erneut prüfen</button></div></div><script>



let bgTimer=null,bgRevision=0;
let bgConfig={mode:'solid',direction:'vertical',first:'#000000',second:'#000000',bands:[{start:0,end:239,color:'#000000'}]};
function bgHexValid(v){return /^#[0-9a-fA-F]{6}$/.test(v||'')}
function bgMsg(t){document.getElementById('bgMessage').textContent=t}
function bgRenderRows(){const wrap=document.getElementById('bgRows');if(!wrap)return;wrap.replaceChildren();bgConfig.bands.forEach((band,idx)=>{const row=document.createElement('div');row.className='bgRow';row.innerHTML=`<label>Von<input type="number" min="0" max="239" value="${band.start}" data-idx="${idx}" data-key="start"></label><label>Bis<input type="number" min="0" max="239" value="${band.end}" data-idx="${idx}" data-key="end"></label><label>Farbe<div class="skinColorPair"><input type="color" value="${band.color}" data-idx="${idx}" data-key="colorPicker"><input type="text" maxlength="7" value="${band.color}" data-idx="${idx}" data-key="color"></div></label><button type="button" data-del="${idx}">Löschen</button>`;wrap.appendChild(row)});wrap.querySelectorAll('input').forEach(el=>el.addEventListener('input',bgBandChanged));wrap.querySelectorAll('button[data-del]').forEach(btn=>btn.addEventListener('click',()=>bgDeleteBand(Number(btn.dataset.del))))}
function bgApplyCanvas(){const canvas=document.getElementById('freeCanvas');if(!canvas)return;let css='#000';if(bgConfig.mode==='gradient'){css=`linear-gradient(${bgConfig.direction==='horizontal'?'to right':'to bottom'}, ${bgConfig.first}, ${bgConfig.second})`}else if(bgConfig.mode==='bands'){const bands=[...bgConfig.bands].sort((a,b)=>a.start-b.start);const stops=[];bands.forEach(b=>{const a=Math.max(0,Math.min(239,Number(b.start)||0))/239*100;const z=Math.max(0,Math.min(239,Number(b.end)||0))/239*100;stops.push(`${b.color} ${a.toFixed(2)}%`,`${b.color} ${z.toFixed(2)}%`)});css=`linear-gradient(${bgConfig.direction==='horizontal'?'to right':'to bottom'}, ${stops.join(',')})`}else{css=bgConfig.first;}canvas.style.background=css}
function bgFill(data){if(!data)data={};const modeNum=Number(data.mode||0);bgConfig.mode=modeNum===1?'gradient':modeNum===2?'bands':'solid';bgConfig.direction=Number(data.direction||0)===1?'horizontal':'vertical';bgConfig.first=data.first||'#000000';bgConfig.second=data.second||bgConfig.first;bgConfig.bands=(data.bands||[]).map(b=>({start:Number(b.start)||0,end:Number(b.end)||0,color:b.color||'#000000'})).slice(0,4);if(!bgConfig.bands.length)bgConfig.bands=[{start:0,end:239,color:bgConfig.first}];document.getElementById('bgMode').value=bgConfig.mode;document.getElementById('bgDirection').value=bgConfig.direction;document.getElementById('bgFirst').value=bgConfig.first;document.getElementById('bgFirstPicker').value=bgConfig.first;document.getElementById('bgSecond').value=bgConfig.second;document.getElementById('bgSecondPicker').value=bgConfig.second;document.getElementById('bgSolid').value=bgConfig.first;document.getElementById('bgSolidPicker').value=bgConfig.first;bgModeChanged(true)}
function bgModeChanged(skipPreview){const mode=document.getElementById('bgMode').value;bgConfig.mode=mode;document.getElementById('bgSolidWrap').hidden=mode!=='solid';document.getElementById('bgGradientWrap').hidden=mode!=='gradient';document.getElementById('bgBandsWrap').hidden=mode!=='bands';document.getElementById('bgDirectionWrap').hidden=mode==='solid';bgRenderRows();bgApplyCanvas();if(!skipPreview)bgEdit()}
function bgColorSync(prefix){const picker=document.getElementById(prefix+'Picker'),hex=document.getElementById(prefix);if(picker&&hex){hex.value=picker.value;bgEdit()}}
function bgBandChanged(ev){const idx=Number(ev.target.dataset.idx),key=ev.target.dataset.key;if(!(idx>=0&&idx<bgConfig.bands.length))return;const band=bgConfig.bands[idx];if(key==='start'||key==='end')band[key]=Math.max(0,Math.min(239,Number(ev.target.value)||0));else if(key==='color'){band.color=ev.target.value;const pick=ev.target.parentElement.querySelector('input[type=color]');if(bgHexValid(band.color)&&pick)pick.value=band.color}else if(key==='colorPicker'){band.color=ev.target.value;const txt=ev.target.parentElement.querySelector('input[type=text]');if(txt)txt.value=band.color}bgEdit()}
function bgAddBand(){if(bgConfig.bands.length>=4){bgMsg('Maximal 4 Bereiche.');return;}const last=bgConfig.bands[bgConfig.bands.length-1]||{start:0,end:239,color:'#000000'};bgConfig.bands.push({start:last.end,end:239,color:last.color});bgRenderRows();bgEdit()}
function bgDeleteBand(idx){if(bgConfig.bands.length<=1){bgMsg('Mindestens ein Bereich bleibt erhalten.');return;}bgConfig.bands.splice(idx,1);bgRenderRows();bgEdit()}
function bgNormalizeBands(){const count=Math.max(1,bgConfig.bands.length);const size=Math.floor(240/count);bgConfig.bands=bgConfig.bands.map((b,i)=>({start:i*size,end:i===count-1?239:((i+1)*size-1),color:b.color||'#000000'}));bgRenderRows();bgEdit()}
function bgPayload(){const mode=document.getElementById('bgMode').value;const dir=document.getElementById('bgDirection').value;const first=mode==='solid'?document.getElementById('bgSolid').value:document.getElementById('bgFirst').value;const second=mode==='gradient'?document.getElementById('bgSecond').value:first;const bands=bgConfig.bands.map(b=>({start:Number(b.start),end:Number(b.end),color:b.color}));return JSON.stringify({mode,direction:dir,first,second,bands})}
function bgEdit(){const mode=document.getElementById('bgMode').value;bgConfig.mode=mode;bgConfig.direction=document.getElementById('bgDirection').value;bgConfig.first=mode==='solid'?document.getElementById('bgSolid').value:document.getElementById('bgFirst').value;bgConfig.second=mode==='gradient'?document.getElementById('bgSecond').value:bgConfig.first;bgConfig.bands=bgConfig.bands.map(b=>({start:Math.max(0,Math.min(239,Number(b.start)||0)),end:Math.max(0,Math.min(239,Number(b.end)||0)),color:b.color||'#000000'}));if(!bgHexValid(bgConfig.first)||!bgHexValid(bgConfig.second)||bgConfig.bands.some(b=>!bgHexValid(b.color)||b.start>b.end)){bgMsg('Bitte gültige Hintergrundwerte eingeben.');return;}bgApplyCanvas();clearTimeout(bgTimer);const token=++bgRevision;bgMsg('Live-Vorschau wird aktualisiert …');bgTimer=setTimeout(async()=>{try{await req('/api/skin/background/preview',{method:'POST',headers:{'Content-Type':'application/json'},body:bgPayload()});if(token===bgRevision)bgMsg('Live-Vorschau aktiv – noch nicht gespeichert.')}catch(e){if(token===bgRevision)bgMsg('Vorschau fehlgeschlagen: '+e.message)}},250)}
async function bgSave(){clearTimeout(bgTimer);++bgRevision;try{await req('/api/skin/background/save',{method:'POST',headers:{'Content-Type':'application/json'},body:bgPayload()});bgApplyCanvas();bgMsg('Hintergrund dauerhaft gespeichert.')}catch(e){bgMsg('Speichern fehlgeschlagen: '+e.message)}}

let freeFields=[],freeSelected='',freeTimer=null,freeRevision=0;
const freeHexValid=v=>/^#[0-9a-fA-F]{6}$/.test(v);
const freeVariables=['%titel%','%quelle%','%quelle_label%','%zeit%','%zeit_aktuell%','%zeit_gesamt%','%status%','%lautstaerke%','%repeat%','%shuffle%','%netzwerk%'];
function freeBuildVariables(){const wrap=document.getElementById('freeVarChips');if(!wrap)return;wrap.replaceChildren(...freeVariables.map(variable=>{const b=document.createElement('button');b.type='button';b.textContent=variable;b.title='An das ausgewählte Textfeld anhängen';b.onclick=()=>freeInsertVariable(variable);return b}));freeVariableButtons()}
function freeVariableButtons(){const f=freeCurrent();for(const btn of document.querySelectorAll('#freeVarChips button'))btn.disabled=!f||f.type!=='text'}
function freeInsertVariable(variable){const f=freeCurrent();if(!f||f.type!=='text')return;f.text=(f.text||'')+variable;document.getElementById('freeText').value=f.text;freeEdit();document.getElementById('freeText').focus()}
let libCurrent='';
async function libRefresh(){try{const r=await req('/api/skin/library');const sel=document.getElementById('libSelect'),group=document.getElementById('skinLibraryOptions'),choice=document.getElementById('skinChoice');const old=choice.value;sel.replaceChildren();group.replaceChildren();for(const x of (r.skins||[])){const o=document.createElement('option');o.value=x.name;o.textContent=x.name;sel.appendChild(o);const v=document.createElement('option');v.value='lib:'+x.name;v.textContent=x.name;group.appendChild(v);}if(libCurrent)sel.value=libCurrent;if(old&&Array.from(choice.options).some(o=>o.value===old))choice.value=old;document.getElementById('libInfo').textContent=(r.skins||[]).length+' eigene Skins gespeichert (maximal 4).';skinChoiceChanged();}catch(e){document.getElementById('libInfo').textContent=e.message}}
function libMessage(msg){document.getElementById('libInfo').textContent=msg}
async function libSave(overwrite){const name=document.getElementById('libName').value.trim();if(!name||name.length>32){libMessage('Bitte einen Skin-Namen (1–32 Zeichen) eingeben.');return;}try{const bundle=skinBundle();skinBundleValidate(bundle);const url='/api/skin/library/save?name='+encodeURIComponent(name)+'&overwrite='+(overwrite?'1':'0');await req(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(bundle)});const applied=await skinBundleSave();if(!applied)throw Error('Skin archiviert, aber Aktivierung fehlgeschlagen');libCurrent=name;await libRefresh();libMessage('Skin „'+name+'“ gespeichert.')}catch(e){libMessage('Speichern fehlgeschlagen: '+e.message)}}
async function libLoad(){const name=document.getElementById('libSelect').value;if(!name)return;try{const data=await req('/api/skin/library/load?name='+encodeURIComponent(name));skinBundleValidate(data);bgFill({mode:data.background.mode==='solid'?0:data.background.mode==='gradient'?1:2,direction:data.background.direction==='horizontal'?1:0,first:data.background.first,second:data.background.second,bands:data.background.bands});freeFields=data.fields.map(x=>({...x,visible:true}));freeSelected=freeFields[0]?.id||'';freeRender();bgEdit();freePreview();libCurrent=name;document.getElementById('libName').value=name;libMessage('Skin „'+name+'“ geladen. Mit „Skin speichern“ dauerhaft anwenden.')}catch(e){libMessage('Laden fehlgeschlagen: '+e.message)}}
function libDuplicate(){const chosen=document.getElementById('skinChoice').value.replace(/^lib:/,'');document.getElementById('libName').value=(chosen||'Mein Skin')+' Kopie';libMessage('Neuen Namen anpassen und „Als neuen Skin speichern“ wählen.');}
async function libDelete(){const name=document.getElementById('libSelect').value;if(!name||!confirm('Skin „'+name+'“ wirklich löschen?'))return;try{await req('/api/skin/library/delete?name='+encodeURIComponent(name),{method:'POST'});libCurrent='';document.getElementById('skinChoice').value='custom';await libRefresh();libMessage('Skin gelöscht. Das aktuell gezeigte Layout bleibt erhalten.')}catch(e){libMessage('Löschen fehlgeschlagen: '+e.message)}}
function freeToolbarAdd(){const menu=document.getElementById('toolbarAdd'),value=menu.value;menu.value='';const [type,kind]=value.split(':');if(type==='icon')freeAddIcon(kind);else if(type==='gauge')freeAddGauge(kind);else if(type==='shape')freeAddShape(kind)}
function freeAddGauge(kind){if(!['progress','volume'].includes(kind))return;if(freeFields.length>=12)return freeMsg('Maximal 12 Elemente.');const id='gauge'+Date.now().toString(36);freeFields.push({id,type:'gauge',text:kind,style:'linear',width:110,height:10,x:120,y:kind==='progress'?175:205,size:1,visible:true,color:kind==='progress'?'#00FFFF':'#FFB000',bgEnabled:true,bgColor:'#202D38',padding:0,arcStart:0,arcSweep:360});freeSelected=id;freeRender();freePreview()}
function freeGaugeEdit(){const f=freeCurrent();if(!f||f.type!=='gauge')return;f.style=document.getElementById('gaugeStyle').value;document.getElementById('gaugeAngleRow').hidden=f.style!=='ring';document.getElementById('gaugeArcHelp').hidden=f.style!=='ring';f.width=Math.min(200,Math.max(12,Number(document.getElementById('gaugeWidth').value)||100));f.height=Math.min(80,Math.max(2,Number(document.getElementById('gaugeHeight').value)||10));f.arcStart=Math.max(0,Math.min(359,Number(document.getElementById('gaugeStart').value)||0));f.arcSweep=Math.max(1,Math.min(360,Number(document.getElementById('gaugeSweep').value)||360));freePaintCanvas();freePreview()}
function freeCurrent(){return freeFields.find(x=>x.id===freeSelected)}
function freeTemplate(){return JSON.stringify({schema:2,fields:freeFields.map(f=>({...f,visible:true}))})}
function freeMsg(msg){document.getElementById('freeMessage').textContent=msg}
async function freeLoad(){try{const obj=await req('/api/skin/fields');freeFields=(obj.fields||[]).filter(f=>f.visible!==false).map(f=>({...f,visible:true}));freeSelected=freeFields[0]?.id||'';freeRender();freeMsg('Felder geladen. Noch nicht geänderte Einstellungen bleiben erhalten.')}catch(e){freeMsg('Laden fehlgeschlagen: '+e.message)}}
function freePreviewText(f){return f.type==='gauge'?'':f.type==='icon'?({wifi:'◉',repeat:'↻',shuffle:'⇄'}[f.text]||f.text):f.text.replaceAll('%quelle_label%','QUELLE: NAS').replaceAll('%quelle%','NAS').replaceAll('%titel%','NERD-WAVE').replaceAll('%status%','SPIELT').replaceAll('%zeit%','01:23 / 04:56').replaceAll('%lautstaerke%','50').replaceAll('%zeit_aktuell%','01:23').replaceAll('%zeit_gesamt%','04:56')}
let freeDragId=null;
let layerDragId=null;
function freeLayerPaint(){
 const target=document.getElementById('skinLayerList');if(!target)return;
 target.replaceChildren();
 freeFields.forEach((f,i)=>{
  const b=document.createElement('button');b.type='button';b.draggable=true;b.dataset.id=f.id;
  b.className=f.id===freeSelected?'active':'';
  b.textContent='☰  '+(i+1)+'. '+(f.type==='shape'?'Form: '+(f.text==='ellipse'?'Ellipse / Ausschnitt':'Rechteck'):f.type==='gauge'?'Anzeige: '+f.text:f.type==='icon'?'Symbol: '+f.text:(f.text||'Text'));
  b.onclick=()=>{freeSelected=f.id;freeRender()};
  b.addEventListener('dragstart',e=>{layerDragId=f.id;e.dataTransfer.effectAllowed='move';e.dataTransfer.setData('text/plain',f.id);b.classList.add('dragging')});
  b.addEventListener('dragover',e=>{e.preventDefault();e.dataTransfer.dropEffect='move';b.classList.add('dropTarget')});
  b.addEventListener('dragleave',()=>b.classList.remove('dropTarget'));
  b.addEventListener('drop',e=>{e.preventDefault();e.stopPropagation();b.classList.remove('dropTarget');const id=layerDragId||e.dataTransfer.getData('text/plain');const from=freeFields.findIndex(x=>x.id===id);const to=freeFields.findIndex(x=>x.id===f.id);if(from>=0&&to>=0&&from!==to){const [moved]=freeFields.splice(from,1);freeFields.splice(to,0,moved);freeSelected=id;freeRender();freePreview()}layerDragId=null});
  b.addEventListener('dragend',()=>{layerDragId=null;b.classList.remove('dragging');target.querySelectorAll('.dropTarget').forEach(el=>el.classList.remove('dropTarget'))});
  target.appendChild(b);
 });
}
function freeMoveLayer(direction){const i=freeFields.findIndex(f=>f.id===freeSelected),j=i+direction;if(i<0||j<0||j>=freeFields.length)return;[freeFields[i],freeFields[j]]=[freeFields[j],freeFields[i]];freeRender();freePreview()}
function freeAddShape(kind){if(!['rect','ellipse'].includes(kind))return;if(freeFields.length>=12)return freeMsg('Maximal 12 Elemente.');const id='shape'+Date.now().toString(36);freeFields.push({id,type:'shape',text:kind,x:120,y:120,width:90,height:60,borderWidth:2,radius:12,arcStart:0,arcSweep:360,size:1,visible:true,color:'#FFB000',bgEnabled:true,bgColor:'#173548',padding:0});freeSelected=id;freeRender();freePreview()}
function freeShapeEdit(){const f=freeCurrent();if(!f||f.type!=='shape')return;const num=(id,low,high,def)=>Math.max(low,Math.min(high,Number(document.getElementById(id).value)||def));f.width=num('shapeWidth',4,239,90);f.height=num('shapeHeight',4,239,60);f.borderWidth=Math.max(0,Math.min(20,Number(document.getElementById('shapeBorderWidth').value)||0));f.radius=Math.max(0,Math.min(110,Number(document.getElementById('shapeRadius').value)||0));f.arcStart=Math.max(0,Math.min(359,Number(document.getElementById('shapeArcStart').value)||0));f.arcSweep=Math.max(1,Math.min(360,Number(document.getElementById('shapeArcSweep').value)||360));freePaintCanvas();freePreview()}
function freePaintCanvas(){const canvas=document.getElementById('freeCanvas');canvas.replaceChildren();bgApplyCanvas();for(const f of freeFields){const div=document.createElement('div');div.className='freeItem'+(f.id===freeSelected?' selected':'');div.dataset.id=f.id;div.style.left=f.x/240*100+'%';div.style.top=f.y/240*100+'%';div.style.color=f.color;div.style.fontSize=(f.size*9+5)+'px';if(f.type==='shape'){div.style.width=(f.width||90)+'px';div.style.height=(f.height||60)+'px';div.style.boxSizing='border-box';div.style.border='none';div.style.borderRadius='0';div.style.background='transparent';div.style.padding='0';
  const w=f.width||90,h=f.height||60,stroke=f.borderWidth||0,rx=w/2,ry=h/2;
  const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox',`0 0 ${w} ${h}`);svg.setAttribute('width',w);svg.setAttribute('height',h);svg.style.display='block';svg.style.pointerEvents='none';
  let part;if(f.text==='ellipse'){
   const begin=Number(f.arcStart??0),sweep=Number(f.arcSweep??360);
   if(sweep>=360){part=document.createElementNS(svg.namespaceURI,'ellipse');part.setAttribute('cx',rx);part.setAttribute('cy',ry);part.setAttribute('rx',Math.max(0,rx-stroke/2));part.setAttribute('ry',Math.max(0,ry-stroke/2))}
   else{part=document.createElementNS(svg.namespaceURI,'path');const pt=a=>[rx+(rx-stroke/2)*Math.cos((a-90)*Math.PI/180),ry+(ry-stroke/2)*Math.sin((a-90)*Math.PI/180)];
    const a=pt(begin),b=pt(begin+sweep);
    part.setAttribute('d',`M ${rx} ${ry} L ${a[0]} ${a[1]} A ${rx-stroke/2} ${ry-stroke/2} 0 ${sweep>180?1:0} 1 ${b[0]} ${b[1]} Z`)}
  }else{part=document.createElementNS(svg.namespaceURI,'rect');part.setAttribute('x',stroke/2);part.setAttribute('y',stroke/2);part.setAttribute('width',Math.max(0,w-stroke));part.setAttribute('height',Math.max(0,h-stroke));part.setAttribute('rx',Math.min(f.radius||0,w/2,h/2))}
  part.setAttribute('fill',f.bgEnabled?f.bgColor:'none');part.setAttribute('stroke',stroke?f.color:'none');part.setAttribute('stroke-width',stroke);svg.appendChild(part);div.appendChild(svg);}if(f.type==='text'){const cap=f.maxTextWidth??230;div.style.maxWidth=cap+'px';div.style.overflow='hidden';div.style.textOverflow='ellipsis';}div.style.opacity='1';if(f.type!=='shape'){div.style.background=f.bgEnabled?f.bgColor:'transparent';div.style.padding=f.bgEnabled?f.padding+'px':'0'}if(f.type==='shape'){}else if(f.type==='gauge'){
 const pct=f.text==='volume'?60:35;const width=f.width||100,height=f.height||10;
 div.style.width=width+'px';div.style.height=(f.style==='ring'?width:height)+'px';div.style.boxSizing='border-box';div.style.border=f.style==='ring'?'none':'1px solid '+f.color;div.style.borderRadius=f.style==='ring'?'50%':'3px';div.style.background=f.style==='ring'?`conic-gradient(from ${f.arcStart||0}deg, ${f.color} 0 ${pct*(f.arcSweep||360)/100}deg, ${f.bgColor||'#303030'} ${pct*(f.arcSweep||360)/100}deg ${f.arcSweep||360}deg, transparent ${f.arcSweep||360}deg)`:f.style==='segments'?`repeating-linear-gradient(90deg, ${f.color} 0 8px, #0000 8px 12px)`:`linear-gradient(90deg, ${f.color} ${pct}%, ${f.bgColor||'#222'} ${pct}%)`;div.style.mask=f.style==='ring'?`radial-gradient(farthest-side, transparent calc(100% - ${Math.min(width/2-1,Math.max(2,Number(f.height)||10))}px), #000 calc(100% - ${Math.min(width/2-1,Math.max(2,Number(f.height)||10))}px + 1px))`:'';div.style.webkitMask=div.style.mask;div.textContent='';
 }else if(f.type==='text'){const raw=freePreviewText(f),lim=Math.max(1,Math.floor((f.maxTextWidth??230)/(6*Math.max(1,f.size))));div.textContent=raw.length>lim?(lim<=2?raw.slice(0,lim):raw.slice(0,lim-2)+'..'):raw;}else div.textContent=freePreviewText(f);canvas.appendChild(div)}}
function freePaintSelection(){freeLayerPaint();for(const el of document.querySelectorAll('#freeCanvas .freeItem'))el.classList.toggle('selected',el.dataset.id===freeSelected)}
function freeRender(){freeLayerPaint();const sel=document.getElementById('freeSelect'),canvas=document.getElementById('freeCanvas');sel.replaceChildren(...freeFields.map(f=>{const o=document.createElement('option');o.value=f.id;o.textContent=f.type==='icon'?'Symbol: '+f.text:f.type==='gauge'?'Anzeige: '+f.text:f.type==='shape'?'Form: '+(f.text==='ellipse'?'Ellipse':'Rechteck'):(f.text||f.id);return o}));sel.value=freeSelected;freePaintCanvas();freeFillForm()}
function freeFillForm(){const f=freeCurrent();freeVariableButtons();if(!f){document.getElementById('freeTextLabel').hidden=true;document.getElementById('freeMaxWidthRow').hidden=true;document.getElementById('freeSizeRow').hidden=true;document.getElementById('shapeOptions').hidden=true;document.getElementById('gaugeOptions').hidden=true;document.getElementById('freeFillControls').hidden=true;return;}document.getElementById('freeText').value=f.text;document.getElementById('freeText').disabled=f.type!=='text';document.getElementById('freeSizeRow').hidden=!(f.type==='text'||f.type==='icon');document.getElementById('freeFillControls').hidden=f.type==='text'||f.type==='icon';document.getElementById('shapeOptions').hidden=f.type!=='shape';if(f.type==='shape'){document.getElementById('shapeWidth').value=f.width||90;document.getElementById('shapeHeight').value=f.height||60;document.getElementById('shapeBorderWidth').value=f.borderWidth??1;document.getElementById('shapeRadius').value=f.radius??8;document.getElementById('shapeArcStart').value=f.arcStart??0;document.getElementById('shapeArcSweep').value=f.arcSweep??360;document.getElementById('shapeArcOptions').hidden=f.text!=='ellipse';document.getElementById('shapeRadius').disabled=f.text==='ellipse';}document.getElementById('gaugeOptions').hidden=f.type!=='gauge';if(f.type==='gauge'){document.getElementById('gaugeStyle').value=f.style||'linear';document.getElementById('gaugeWidth').value=f.width||100;document.getElementById('gaugeHeight').value=f.height||10;document.getElementById('gaugeStart').value=f.arcStart??0;document.getElementById('gaugeSweep').value=f.arcSweep??360;document.getElementById('gaugeAngleRow').hidden=f.style!=='ring';document.getElementById('gaugeArcHelp').hidden=f.style!=='ring';}document.getElementById('freeTextLabel').hidden=f.type!=='text';document.getElementById('freeMaxWidthRow').hidden=f.type!=='text';document.getElementById('freeMaxWidth').value=f.maxTextWidth??230;document.getElementById('freeTextLabel').firstChild.textContent='Text / Platzhalter';document.querySelector('#freeSize option[value="3"]').hidden=f.type==='icon';document.getElementById('freeSize').value=f.size;document.getElementById('freeX').value=f.x;document.getElementById('freeY').value=f.y;document.getElementById('freeHex').value=f.color;document.getElementById('freePicker').value=f.color;document.getElementById('freeBgEnabled').checked=f.bgEnabled;document.getElementById('freeBgHex').value=f.bgColor;document.getElementById('freeBgPicker').value=f.bgColor;}
function freeCoordinateChange(){const f=freeCurrent();if(!f)return;const xe=document.getElementById('freeX'),ye=document.getElementById('freeY');if(xe.value===''||ye.value==='')return;const x=Number(xe.value),y=Number(ye.value);if(!Number.isFinite(x)||!Number.isFinite(y))return;f.x=Math.round(Math.max(0,Math.min(239,x)));f.y=Math.round(Math.max(0,Math.min(239,y)));freePaintCanvas();freePreview()}
const freeCanvas=document.getElementById('freeCanvas');
freeCanvas.addEventListener('pointerdown',e=>{const el=e.target.closest('.freeItem');if(!el)return;const f=freeFields.find(x=>x.id===el.dataset.id);if(!f)return;freeSelected=f.id;document.getElementById('freeSelect').value=f.id;freePaintSelection();freeFillForm();freeDragId=e.pointerId;freeCanvas.setPointerCapture(e.pointerId);e.preventDefault()});
freeCanvas.addEventListener('pointermove',e=>{if(freeDragId!==e.pointerId)return;const f=freeCurrent();if(!f)return;const b=freeCanvas.getBoundingClientRect();f.x=Math.round(Math.max(0,Math.min(239,(e.clientX-b.left)*240/b.width)));f.y=Math.round(Math.max(0,Math.min(239,(e.clientY-b.top)*240/b.height)));const el=[...freeCanvas.children].find(x=>x.dataset.id===f.id);if(el){el.style.left=f.x/240*100+'%';el.style.top=f.y/240*100+'%'}document.getElementById('freeX').value=f.x;document.getElementById('freeY').value=f.y;freePreview()});
function freeEndDrag(){freeDragId=null}
freeCanvas.addEventListener('pointerup',freeEndDrag);freeCanvas.addEventListener('pointercancel',freeEndDrag);freeCanvas.addEventListener('lostpointercapture',freeEndDrag);
function freeSelectChange(){freeSelected=document.getElementById('freeSelect').value;freeRender()}
function freeAdd(){if(freeFields.length>=12)return freeMsg('Maximal 12 Elemente.');const id='field'+Date.now().toString(36);freeFields.push({id,type:'text',text:'Freitext',maxTextWidth:230,x:120,y:120,size:2,visible:true,color:'#FFFFFF',bgEnabled:false,bgColor:'#000000',padding:0});freeSelected=id;freeRender();freePreview()}
function freeAddIcon(kind){if(!['wifi','repeat','shuffle'].includes(kind))return;if(freeFields.length>=12)return freeMsg('Maximal 12 Elemente.');const id='icon'+Date.now().toString(36);freeFields.push({id,type:'icon',text:kind,x:120,y:70,size:1,visible:true,color:'#00FFFF',bgEnabled:false,bgColor:'#000000',padding:0});freeSelected=id;freeRender();freePreview()}
function freeDelete(){freeFields=freeFields.filter(x=>x.id!==freeSelected);freeSelected=freeFields[0]?.id||'';freeRender();freePreview()}
function freeEdit(){const f=freeCurrent();if(!f)return;const color=document.getElementById('freeHex').value,bg=document.getElementById('freeBgHex').value;if(!freeHexValid(color)||!freeHexValid(bg)){freeMsg('Bitte gültige HEX-Farben (#RRGGBB) eingeben.');return;}if(f.type==='text'){f.text=document.getElementById('freeText').value;const width=document.getElementById('freeMaxWidth').value;if(width!=='')f.maxTextWidth=Math.min(239,Math.max(12,Number(width)||230));}if(f.type==='text'||f.type==='icon')f.size=Math.min(f.type==='icon'?2:3,Number(document.getElementById('freeSize').value));f.color=color;f.bgColor=bg;f.visible=true;if(f.type==='shape'||f.type==='gauge')f.bgEnabled=document.getElementById('freeBgEnabled').checked;else {f.bgEnabled=false;f.padding=0;}freePaintCanvas();const option=[...document.getElementById('freeSelect').options].find(o=>o.value===f.id);if(option)option.textContent=f.type==='icon'?'Symbol: '+f.text:f.type==='gauge'?'Anzeige: '+f.text:f.type==='shape'?'Form: '+(f.text==='ellipse'?'Ellipse':'Rechteck'):(f.text||f.id);freePreview()}
function freePickerChange(){document.getElementById('freeHex').value=document.getElementById('freePicker').value;freeEdit()}
function freeBgPickerChange(){document.getElementById('freeBgHex').value=document.getElementById('freeBgPicker').value;freeEdit()}
function freeCenter(axis){const f=freeCurrent();if(!f)return;f[axis]=120;freeRender();freePreview()}
function freePreview(){clearTimeout(freeTimer);const token=++freeRevision;freeMsg('Live-Vorschau wird aktualisiert …');freeTimer=setTimeout(async()=>{try{await req('/api/skin/fields/preview',{method:'POST',headers:{'Content-Type':'application/json'},body:freeTemplate()});if(token===freeRevision)freeMsg('Live-Vorschau aktiv – noch nicht gespeichert.')}catch(e){if(token===freeRevision)freeMsg('Vorschau fehlgeschlagen: '+e.message)}},350)}
async function freeSave(){clearTimeout(freeTimer);++freeRevision;try{await req('/api/skin/fields/save',{method:'POST',headers:{'Content-Type':'application/json'},body:freeTemplate()});freeMsg('Freie Felder dauerhaft gespeichert.')}catch(e){freeMsg('Speichern fehlgeschlagen: '+e.message)}}
function freeExport(){skinBundleExport()}
async function freeImport(ev){const file=ev.target.files[0];if(!file)return;try{if(file.size>8192)throw Error('Datei zu groß');const obj=JSON.parse(await file.text());if(obj.schema!==2||!Array.isArray(obj.fields)||obj.fields.length>16)throw Error('Ungültiges Feld-Template');freeFields=obj.fields;freeSelected=freeFields[0]?.id||'';freeRender();freePreview()}catch(e){freeMsg('Import fehlgeschlagen: '+e.message)}finally{ev.target.value=''}}

async function skinStatus(){try{const x=await req('/api/skin');const e=document.getElementById('skinChoice');e.value=x.active;e.querySelector('option[value="custom"]').disabled=!x.customAvailable;document.getElementById('skinInfo').textContent='Aktiv: '+x.label;bgFill(x.decoration||{});}catch(e){document.getElementById('skinInfo').textContent=e.message}}
function skinChoiceChanged(){const v=document.getElementById('skinChoice').value;const name=v.startsWith('lib:')?v.slice(4):'';if(name){document.getElementById('libSelect').value=name;document.getElementById('libName').value=name;}document.querySelectorAll('.skinManageBody button').forEach(b=>{if(b.textContent==='Löschen'||b.textContent==='Duplizieren'||b.textContent==='Aktualisieren')b.disabled=!name;});}
async function skinSelect(){const v=document.getElementById('skinChoice').value;if(v.startsWith('lib:')){const name=v.slice(4);document.getElementById('libSelect').value=name;await libLoad();const saved=await skinBundleSave();if(!saved){libMessage('Skin geladen, Aktivierung fehlgeschlagen.');return;}try{await req('/api/skin/select',{method:'POST',body:new URLSearchParams({id:'custom'})});libCurrent=name;await skinStatus();document.getElementById('skinChoice').value=v;skinChoiceChanged();libMessage('Skin „'+name+'“ angewendet.');}catch(e){libMessage('Aktivierung fehlgeschlagen: '+e.message)}return;}try{await req('/api/skin/select',{method:'POST',body:new URLSearchParams({id:v})});await skinStatus();freeRender();skinChoiceChanged()}catch(e){document.getElementById('skinInfo').textContent=e.message}}
function skinDownload(){skinBundleExport()}
function skinBundle(){return {schema:3,display:'GC9A01',width:240,height:240,background:JSON.parse(bgPayload()),fields:freeFields.map(f=>({...f,visible:true}))}}
function skinBundleMsg(v){document.getElementById('skinBundleMessage').textContent=v}
function skinBundleExport(){try{const blob=new Blob([JSON.stringify(skinBundle(),null,2)],{type:'application/json'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='Nerd-Wave-Skin-Complete.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1200);skinBundleMsg('Skin-Template exportiert.')}catch(e){skinBundleMsg('Export: '+e.message)}}
function skinBundleValidate(o){if(o.schema!==3||o.display!=='GC9A01'||o.width!==240||o.height!==240)throw Error('Schema 3 für GC9A01 240 × 240 erforderlich');if(!Array.isArray(o.fields)||o.fields.length>12)throw Error('Ungültige Feldliste');const b=o.background;if(!b||!['solid','gradient','bands'].includes(b.mode)||!['vertical','horizontal'].includes(b.direction)||!bgHexValid(b.first)||!bgHexValid(b.second))throw Error('Ungültiger Hintergrund');if(!Array.isArray(b.bands)||b.bands.length<1||b.bands.length>4||b.bands.some(x=>!Number.isInteger(x.start)||!Number.isInteger(x.end)||x.start<0||x.end>239||x.start>x.end||!bgHexValid(x.color)))throw Error('Ungültige Hintergrundbereiche');}
async function skinBundleSave(){clearTimeout(bgTimer);clearTimeout(freeTimer);++bgRevision;++freeRevision;try{skinBundleValidate(skinBundle());await req('/api/skin/background/save',{method:'POST',headers:{'Content-Type':'application/json'},body:bgPayload()});await req('/api/skin/fields/save',{method:'POST',headers:{'Content-Type':'application/json'},body:freeTemplate()});skinBundleMsg('Hintergrund und Felder dauerhaft gespeichert.');return true;}catch(e){skinBundleMsg('Speichern fehlgeschlagen: '+e.message+' (gegebenenfalls nur teilweise gespeichert)');return false;}}
async function skinBundleImport(ev){const file=ev.target.files[0];if(!file)return;try{if(file.size>12000)throw Error('Datei zu groß');const o=JSON.parse(await file.text());if(o.schema===2&&Array.isArray(o.fields)){freeFields=o.fields.filter(f=>f.visible!==false).map(f=>({...f,visible:true}));freeSelected=freeFields[0]?.id||'';freeRender();freePreview();skinBundleMsg('Altes Feld-Template importiert; zuvor ausgeblendete Felder wurden weggelassen. Hintergrund unverändert. Noch nicht gespeichert.');return;}skinBundleValidate(o);bgFill({mode:o.background.mode==='solid'?0:o.background.mode==='gradient'?1:2,direction:o.background.direction==='horizontal'?1:0,first:o.background.first,second:o.background.second,bands:o.background.bands});freeFields=o.fields.filter(f=>f.visible!==false).map(f=>({...f,visible:true}));freeSelected=freeFields[0]?.id||'';freeRender();bgEdit();freePreview();skinBundleMsg('Kompletter Skin importiert und als Vorschau angewendet. Zum Behalten Skin speichern.')}catch(e){skinBundleMsg('Import fehlgeschlagen: '+e.message)}finally{ev.target.value=''}}
let logOffset=0,logPaused=false,logBusy=false;
async function logPoll(){
 if(logPaused||logBusy||!document.getElementById('tab-system').classList.contains('active'))return;
 logBusy=true;
 try{
  const r=await fetch('/api/log?from='+logOffset,{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const d=await r.json(),box=document.getElementById('serialLog');
  const nearBottom=box.scrollTop+box.clientHeight>=box.scrollHeight-35;
  if(d.truncated && logOffset!==0)box.textContent+='\n[Web-Konsole: aeltere Meldungen nicht mehr im Puffer]\n';
  box.textContent+=d.text||'';
  if(box.textContent.length>32000)box.textContent=box.textContent.slice(-24000);
  logOffset=d.next;
  if(nearBottom)box.scrollTop=box.scrollHeight;
  document.getElementById('logState').textContent='Live · Aktualisierung jede Sekunde';
 }catch(e){document.getElementById('logState').textContent='Konsolenabruf: '+e.message}
 finally{logBusy=false;}
}
function logToggle(){logPaused=!logPaused;document.getElementById('logPause').textContent=logPaused?'Fortsetzen':'Pausieren';document.getElementById('logState').textContent=logPaused?'Pausiert':'Live';if(!logPaused)logPoll()}
function logClear(){document.getElementById('serialLog').textContent='';logPoll()}
async function logCopy(){
 const content=document.getElementById('serialLog').textContent;
 const state=document.getElementById('logState');
 if(!content){state.textContent='Konsole ist leer';return;}
 try{
  if(navigator.clipboard && window.isSecureContext){
   await navigator.clipboard.writeText(content);
  }else{
   // HTTP auf lokaler ESP32-IP ist normalerweise kein Secure Context.
   const field=document.createElement('textarea');
   field.value=content;
   field.setAttribute('readonly','');
   field.style.cssText='position:fixed;left:-9999px;top:0;opacity:0';
   document.body.appendChild(field);
   field.focus();field.select();field.setSelectionRange(0,field.value.length);
   const copied=document.execCommand('copy');
   document.body.removeChild(field);
   if(!copied)throw new Error('Browser verweigert Kopieren');
  }
  state.textContent='Konsolentext kopiert';
 }catch(e){
  state.textContent='Kopieren nicht möglich: '+e.message+' (Text manuell markieren)';
 }
}
setInterval(logPoll,1000);
function showTab(name){
 if(settingDirty && (!document.getElementById('host').checkValidity()||!document.getElementById('port').checkValidity()) && !confirm('NAS-Einstellungen sind ungültig und ungespeichert. Tab trotzdem wechseln?'))return;
 document.querySelectorAll('.tabPanel').forEach(el=>el.classList.toggle('active',el.id==='tab-'+name));
 document.querySelectorAll('.tabs button').forEach(el=>el.classList.toggle('active',el.dataset.tab===name));
 if(name==='display'){skinStatus();libRefresh();freeRender();}
 if(name==='settings')mqttStatusRefresh();
 if(name==='player' && !browseBusy && browseItems.length===0)browseReload();
}
async function scanAps(){
 const info=document.getElementById('apInfo'),list=document.getElementById('apList');
 info.textContent='Suche WLAN-Zugangspunkte ...';list.replaceChildren();
 try{let data=await req('/api/wifi/aps');let aps=data.aps||[];
  info.textContent=aps.length+' Zugangspunkte mit derselben SSID gefunden';
  for(let ap of aps){let row=document.createElement('div');row.className='ap';
   let label=document.createElement('span');label.className='apDetails';let renderLabel=()=>label.textContent=(ap.name||ap.bssid)+' · '+ap.bssid+' · Kanal '+ap.channel+' · '+ap.rssi+' dBm'+(ap.current?' (verbunden)':'');renderLabel();let nameInput=document.createElement('input');nameInput.type='text';nameInput.maxLength=40;nameInput.value=ap.name||'';nameInput.placeholder='Name für diesen Mesh-Knoten';nameInput.setAttribute('aria-label','Anzeigename für '+ap.bssid);let nameSave=document.createElement('button');nameSave.textContent='Name speichern';nameSave.onclick=async()=>{try{let body=new URLSearchParams({bssid:ap.bssid,name:nameInput.value});await req('/api/wifi/name',{method:'POST',body});ap.name=nameInput.value.trim();renderLabel();info.textContent='Name gespeichert.';update();}catch(err){info.textContent=err.message}};
   let button=document.createElement('button');button.textContent=ap.blocked?'Freigeben':'Sperren';
   button.onclick=async()=>{if(!ap.blocked && ap.current && !confirm('Aktuellen Zugangspunkt sperren? Die WLAN-Verbindung wird möglicherweise unterbrochen.'))return;
    try{let body=new URLSearchParams({bssid:ap.bssid,action:ap.blocked?'allow':'block'});await req('/api/wifi/block',{method:'POST',body});
      ap.blocked=!ap.blocked;button.textContent=ap.blocked?'Freigeben':'Sperren';info.textContent='Sperrliste gespeichert.';
    }catch(err){info.textContent=err.message}
   };let controls=document.createElement('div');controls.className='apActions';controls.append(nameInput,nameSave,button);row.append(label,controls);list.appendChild(row);
  }
 }catch(err){info.textContent=err.message}
}
async function req(url,opt){let r=await fetch(url,opt);if(!r.ok)throw Error(await r.text());return r.json()}
async function act(cmd,value){try{let b=new URLSearchParams({cmd,value:value??''});await req('/api/control',{method:'POST',body:b});await update()}catch(e){document.getElementById('msg').textContent=e.message}}
let browsePath='/',browseOffset=0,browseTotal=0,browseSource=-1,browseBusy=false,browseItems=[],browseGeneration=0;
function parentPath(p){if(!p||p==='/')return '/';let i=p.lastIndexOf('/');return i<=0?'/':p.slice(0,i)}
async function browseLoad(append=false){
 if(browseBusy)return;
 browseBusy=true;
 const info=document.getElementById('browserInfo'),list=document.getElementById('browserList');
 const source=Number(document.getElementById('source').value),path=browsePath;
 const generation=browseGeneration;
 browseSource=source;
 if(!append){browseOffset=0;browseTotal=0;browseItems=[];list.replaceChildren();list.scrollTop=0;if(path!=='/'){let up=document.createElement('button');up.type='button';up.className='folder';up.textContent='📁 ..';up.onclick=browseUp;list.appendChild(up);}}
 renderCrumbs();
 if(source===2){info.textContent='Bluetooth bietet keinen Dateibrowser.';browseBusy=false;return;}
 info.textContent=append?'Lade weitere Titel ...':'Lade Verzeichnis ...';
 try{
  const offset=browseItems.length;
  const x=await req('/api/browse?source='+source+'&path='+encodeURIComponent(path)+'&offset='+offset);
  if(generation!==browseGeneration || path!==browsePath || source!==Number(document.getElementById('source').value))return;
  const items=x.items||[];
  browseTotal=Number(x.total)||0;
  items.forEach((item,i)=>{
   if(String(item.name||" ").startsWith("."))return; // Versteckte Eintraege nicht anzeigen; API-Indizes bleiben erhalten.
   let b=document.createElement('button');b.type='button';b.className=item.directory?'folder':'';
   b.textContent=(item.directory?'📁 ':'♫ ')+item.name;b.dataset.label=item.name;b.dataset.path=item.path;b.dataset.directory=item.directory?'1':'0';
   const absoluteIndex=offset+i;
   b.onclick=()=>browseOpen(absoluteIndex);
   list.appendChild(b);
  });
  browseItems.push(...items);sortBrowserButtons();filterBrowser();highlightPlaying();
  info.textContent=browseTotal+' Einträge · '+browseItems.length+' geladen';
  if(!items.length && offset<browseTotal)info.textContent+=' · Keine weiteren Daten';
 }catch(e){if(generation===browseGeneration)info.textContent='Browserfehler: '+e.message}
 finally{browseBusy=false}
}
function browseReload(){browseGeneration++;browseLoad()}
function sortBrowserButtons(){
 const list=document.getElementById('browserList');
 const items=[...list.querySelectorAll('button[data-label]')];
 items.sort((a,b)=>Number(b.dataset.directory)-Number(a.dataset.directory)||a.dataset.label.localeCompare(b.dataset.label,'de',{numeric:true,sensitivity:'base'}));
 for(let b of items)list.appendChild(b);
}
function highlightPlaying(){let list=document.getElementById('browserList');list.querySelectorAll('button[data-path]').forEach(b=>b.classList.toggle('current',b.dataset.path===lastStatus.path && b.dataset.directory==='0'))}
function browseUp(){browsePath=parentPath(browsePath);browseGeneration++;browseLoad()}
// Weitere API-Seiten erst abrufen, wenn die Liste ans Ende gescrollt wurde.
document.getElementById('browserList').addEventListener('scroll',e=>{
 const l=e.currentTarget;
 if(!browseBusy && browseItems.length<browseTotal && l.scrollTop+l.clientHeight>=l.scrollHeight-80)browseLoad(true);
});
async function browseOpen(i){let item=browseItems[i];if(!item)return;
 if(item.directory){browsePath=item.path;browseGeneration++;await browseLoad();return}
 try{const source=Number(document.getElementById('source').value);if(source!==browseSource)throw Error('Quelle hat sich geändert. Bitte aktualisieren.');
  let f=new URLSearchParams({source:String(source),path:browsePath,offset:String(Math.floor(i/25)*25),index:String(i%25)});
  await req('/api/browse/play',{method:'POST',body:f});await update();document.getElementById('browserInfo').textContent='Starte: '+item.name;
 }catch(e){document.getElementById('browserInfo').textContent='Titel konnte nicht gestartet werden: '+e.message}
}

let lastStatus={},seekDragging=false,settingDirty=false,savingSettings=false,autoSettingsTimer=null,savedSettingsSnapshot='';
function fmtTime(n){n=Math.max(0,Math.floor(Number(n)||0));return Math.floor(n/60)+':'+String(n%60).padStart(2,'0')}
function renderCrumbs(){const wrap=document.getElementById('breadcrumbs');wrap.replaceChildren();let p='';let b=document.createElement('button');b.type='button';b.textContent='/';b.onclick=()=>gotoFolder('/');wrap.appendChild(b);for(let segment of browsePath.split('/').filter(Boolean)){p+='/'+segment;let name=document.createElement('button');name.type='button';name.textContent=segment;let target=p;name.onclick=()=>gotoFolder(target);wrap.appendChild(name)}}
function gotoFolder(path){browsePath=path;browseGeneration++;browseLoad()}
function filterBrowser(){let q=document.getElementById('browserSearch').value.toLocaleLowerCase();let list=document.getElementById('browserList');for(let button of list.querySelectorAll('button[data-label]'))button.hidden=!button.dataset.label.toLocaleLowerCase().includes(q)}
let browserSearchTimer=null;
async function searchAllPages(){
 if(!document.getElementById('browserSearch').value.trim())return;
 const generation=browseGeneration;
 while(!browseBusy && browseItems.length<browseTotal && generation===browseGeneration && browseItems.length<5000){
  await browseLoad(true);
  if(generation!==browseGeneration)break;
  await new Promise(resolve=>setTimeout(resolve,20));
 }
 filterBrowser();
}
document.getElementById('browserSearch').addEventListener('input',()=>{filterBrowser();clearTimeout(browserSearchTimer);browserSearchTimer=setTimeout(searchAllPages,280)});
const seekBar=document.getElementById('seek');seekBar.addEventListener('pointerdown',()=>seekDragging=true);seekBar.addEventListener('input',()=>{seekDragging=true;document.getElementById('elapsed').textContent=fmtTime(seekBar.value)});seekBar.addEventListener('change',async()=>{const t=Number(seekBar.value);document.getElementById('seekNotice').textContent='Springe zu '+fmtTime(t)+' – bitte warten …';try{await act('seek',String(t))}catch(e){document.getElementById('state').textContent=e.message}finally{seekDragging=false}});
let quickPending=false;
async function toggleQuick(key){
 if(quickPending)return;
 const repeatEl=document.getElementById('repeat'),shuffleEl=document.getElementById('shuffle');
 const oldRepeat=repeatEl.checked,oldShuffle=shuffleEl.value;
 if(key==='repeat')repeatEl.checked=!oldRepeat;
 else if(key==='shuffle')shuffleEl.value=String((Number(oldShuffle)+1)%3);
 else return;
 const repeat=repeatEl.checked,shuffle=Number(shuffleEl.value);
 document.getElementById('repeatBtn').textContent=repeat?'Repeat: Ein':'Repeat: Aus';
 document.getElementById('shuffleBtn').textContent=['Shuffle: Aus','Shuffle: Ordner','Shuffle: Rekursiv'][shuffle];
 quickPending=true;
 try{
  await req('/api/playback-settings',{method:'POST',body:new URLSearchParams({repeat:repeat?'1':'0',shuffle:String(shuffle)})});
  savedSettingsSnapshot=settingsSignature();settingDirty=false;
 }catch(e){
  repeatEl.checked=oldRepeat;shuffleEl.value=oldShuffle;
  document.getElementById('repeatBtn').textContent=oldRepeat?'Repeat: Ein':'Repeat: Aus';
  document.getElementById('shuffleBtn').textContent=['Shuffle: Aus','Shuffle: Ordner','Shuffle: Rekursiv'][Number(oldShuffle)];
  document.getElementById('msg').textContent='Wiedergabeeinstellung fehlgeschlagen: '+e.message;
 }finally{quickPending=false;}
}
const settingForm=document.getElementById('settings');
function settingsSignature(){return JSON.stringify(['host','port','net','autostart','srcNas','srcUsb','srcBt','srcSd','powerSwitch'].map(k=>{let x=document.getElementById(k);return x.type==='checkbox'?x.checked:x.value}))}
function sourceMaskFromForm(){return (document.getElementById('srcNas').checked?1:0)|(document.getElementById('srcUsb').checked?2:0)|(document.getElementById('srcBt').checked?4:0)|(document.getElementById('srcSd').checked?8:0)}
function refreshSourceOptions(mask){const el=document.getElementById('source');for(const opt of el.options)opt.hidden=!(mask&(1<<Number(opt.value)));}

function settingsChanged(){settingDirty=settingsSignature()!==savedSettingsSnapshot;const msg=document.getElementById('msg');if(settingDirty)msg.textContent='Ungespeicherte Änderungen';else if(!savingSettings)msg.textContent='Alle Änderungen gespeichert';}
async function saveSettingsAuto(){if(!settingDirty||savingSettings)return;const host=document.getElementById('host'),port=document.getElementById('port');if(!host.checkValidity()||!port.checkValidity()){document.getElementById('msg').textContent='NAS-Adresse und Port überprüfen – noch nicht gespeichert.';return;}savingSettings=true;let signature=settingsSignature();let form=new URLSearchParams(new FormData(settingForm));if(!sourceMaskFromForm()){document.getElementById('msg').textContent='Mindestens eine Musikquelle aktivieren.';savingSettings=false;return;}form.set('sourceMask',sourceMaskFromForm());form.set('powerSwitch',document.getElementById('powerSwitch').checked?'1':'0');form.set('repeat',document.getElementById('repeat').checked?'1':'0');form.set('shuffle',document.getElementById('shuffle').value);for(let k of ['autostart'])form.set(k,document.getElementById(k).checked?'1':'0');try{await req('/api/settings',{method:'POST',body:form});savedSettingsSnapshot=signature;settingDirty=settingsSignature()!==signature;document.getElementById('msg').textContent=settingDirty?'Weitere Änderungen warten':'Gespeichert';}catch(e){document.getElementById('msg').textContent='Speichern fehlgeschlagen: '+e.message}finally{savingSettings=false;if(settingDirty)queueSettingsSave()}}
function queueSettingsSave(now=false){settingsChanged();clearTimeout(autoSettingsTimer);if(now)saveSettingsAuto();else autoSettingsTimer=setTimeout(saveSettingsAuto,1100)}
settingForm.addEventListener('input',()=>queueSettingsSave());settingForm.addEventListener('change',()=>queueSettingsSave());
window.addEventListener('beforeunload',e=>{if(settingDirty||ledDirty){e.preventDefault();e.returnValue='';}});
let ledDirty=false,ledPersistTimer=null,ledSavedSnapshot='';
function ledSignature(){return JSON.stringify(['ledEnabled','ledCount','ledEffect','ledColor','ledSpeed','ledBrightness','ledBaseBrightness','ledSensitivity'].map(k=>{let x=document.getElementById(k);return x.type==='checkbox'?x.checked:x.value}))}
function renderLedPreview(){const host=document.getElementById('ledPreview');if(!host.children.length){for(let i=0;i<14;i++){let dot=document.createElement('span');dot.className='ledDot';host.appendChild(dot)}}let fx=Number(document.getElementById('ledEffect').value),color=document.getElementById('ledColor').value;for(let i=0;i<host.children.length;i++){const x=host.children[i],n=Date.now()/1000,speed=Number(document.getElementById('ledSpeed').value)/35;let hue=fx===7||fx===8?(n*55*speed)%360:(i*360/14+n*55*speed)%360;let pulse=Math.max(.14,(Math.sin(n*4-i*.5)+1)/2);x.style.color=fx===4?color:(fx>=5?`hsl(${hue} 100% 55%)`:fx===3?['#00c5ff','#ff8b2c','#ae70ff'][i%3]:'#00cfff');x.style.opacity=fx===2||fx===6||fx===8?String(pulse):fx===0||fx===1||fx===3?String(i<(4+Math.floor(4*Math.sin(n*2)))?1:.12):'1'}}
setInterval(renderLedPreview,100);renderLedPreview();
function queueLedPersist(){ledDirty=ledSignature()!==ledSavedSnapshot;document.getElementById('ledMsg').textContent=ledDirty?'Live-Vorschau – Speichern läuft automatisch …':'Gespeichert';clearTimeout(ledPersistTimer);ledPersistTimer=setTimeout(saveLedAuto,1200)}
async function saveLedAuto(){if(!ledDirty)return;const snapshot=ledSignature();let form=new URLSearchParams(new FormData(ledForm));form.set('ledEnabled',document.getElementById('ledEnabled').checked?'1':'0');try{await req('/api/led',{method:'POST',body:form});ledSavedSnapshot=snapshot;ledDirty=ledSignature()!==snapshot;document.getElementById('ledMsg').textContent=ledDirty?'Weitere Änderungen warten':'Gespeichert';}catch(e){document.getElementById('ledMsg').textContent='Speichern fehlgeschlagen: '+e.message}}

let configured=false,lastDeviceStatus=null,otaChecking=false,otaBefore=null;
function setOtaOverlay(message){
 const overlay=document.getElementById('otaOverlay');
 overlay.hidden=false;
 document.body.classList.add('otaLocked');
 document.getElementById('otaOverlayMessage').textContent=message;
 document.getElementById('otaOverlayRetry').hidden=true;
 // Nach erfolgreichem Upload sind sämtliche Bedienelemente hinter dem Modal blockiert.
 document.querySelectorAll('main button,main input,main select').forEach(el=>el.disabled=true);
}
function otaOverlayMessage(message){
 document.getElementById('otaOverlayMessage').textContent=message;
}
function retryOtaCheck(){
 if(otaChecking)return;
 document.getElementById('otaOverlayRetry').hidden=true;
 watchOtaRestart(otaBefore);
}
async function refreshOta(){try{let x=await req('/api/ota/info');let i=document.getElementById('otaInfo');let b=document.getElementById('otaButton');i.textContent=x.available?('OTA bereit · freier App-Slot: '+Math.floor(x.capacity/1024)+' KiB · Partition: '+x.partition):('OTA nicht verfügbar: '+x.reason);b.disabled=!x.available;}catch(e){document.getElementById('otaInfo').textContent=e.message}}
// Nach dem OTA-Upload Neustart anhand der neuen Version oder der zurueckgesetzten Uptime erkennen.
async function watchOtaRestart(before){
 otaChecking=true;
 otaBefore=before;
 setOtaOverlay('Nerd-Wave startet neu. Verbindung wird wiederhergestellt …');
 const message=document.getElementById('otaMessage');
 const button=document.getElementById('otaButton');button.disabled=true;
 const started=Date.now();let offlineSeen=false,successes=0;
 while(Date.now()-started<150000){
  await new Promise(resolve=>setTimeout(resolve,1200));
  try{
   let r=await fetch('/api/status?otaCheck='+Date.now(),{cache:'no-store'});
   if(!r.ok){offlineSeen=true;successes=0;continue}
   let now=await r.json();successes++;
   const newer=before && now.version && now.version!==before.version;
   const restarted=before && Number.isFinite(before.uptimeMs) && Number(now.uptimeMs)+1500<before.uptimeMs;
   if((newer||restarted||offlineSeen) && successes>=2){
    message.textContent='Nerd-Wave ist wieder erreichbar. Weboberfläche wird aktualisiert ...';
    otaOverlayMessage('Nerd-Wave ist wieder erreichbar. Die Weboberfläche wird neu geladen …');
    window.location.replace('/?ota='+Date.now());return;
   }
   message.textContent='Firmware übertragen. Warte auf den Neustart von Nerd-Wave ...';
   otaOverlayMessage('Firmware installiert. Warte auf den Neustart und die Wiederverbindung …');
  }catch(_){offlineSeen=true;successes=0;message.textContent='Nerd-Wave startet neu. Warte auf die Verbindung ...';
   otaOverlayMessage('Nerd-Wave ist vorübergehend nicht erreichbar. Verbindung wird geprüft …');}
 }
 otaChecking=false;
 message.textContent='Neustart noch nicht erkannt. Prüfe die Verbindung erneut.';
 otaOverlayMessage('Nerd-Wave wurde innerhalb von 150 Sekunden nicht bestätigt. Du kannst die Verbindung erneut prüfen.');
 document.getElementById('otaOverlayRetry').hidden=false;
}
document.getElementById('otaForm').onsubmit=e=>{e.preventDefault();let file=document.getElementById('otaFile').files[0];let message=document.getElementById('otaMessage');if(!file)return;if(!file.name.toLowerCase().endsWith('.bin')){message.textContent='Bitte eine .bin-Datei wählen.';return;}if(!confirm('Firmware installieren? Nerd-Wave beendet die Wiedergabe automatisch und startet nach Erfolg neu.'))return;let xhr=new XMLHttpRequest();xhr.open('POST','/api/ota?size='+file.size);xhr.upload.onprogress=ev=>{if(ev.lengthComputable){let p=document.getElementById('otaProgress');p.style.display='block';p.value=Math.round(100*ev.loaded/ev.total)}};xhr.onload=()=>{try{let x=JSON.parse(xhr.responseText);message.textContent=x.message||x.error||(xhr.status===200?'Erfolgreich':'Update fehlgeschlagen');if(x.ok){message.textContent+=' · Neustart wird geprüft ...';watchOtaRestart(lastDeviceStatus)}}catch(err){message.textContent='HTTP '+xhr.status+': '+xhr.responseText}};xhr.onerror=()=>{message.textContent='Verbindung unterbrochen. Gerät prüfen, bevor du erneut hochlädst.'};let form=new FormData();form.append('firmware',file,file.name);message.textContent='Übertragung läuft ...';xhr.send(form)};
async function update(){if(otaChecking)return;try{let x=await req('/api/status');lastDeviceStatus={version:x.version,uptimeMs:Number(x.uptimeMs)};lastStatus=x;const seekEl=document.getElementById('seek');seekEl.disabled=Number(x.source)===2||(!Number(x.duration)&&!x.seeking);seekEl.max=Math.max(1,Number(x.duration)||1);if(!seekDragging){seekEl.value=Math.min(Number(x.elapsed)||0,Number(x.duration)||1);document.getElementById('elapsed').textContent=fmtTime(x.elapsed)}document.getElementById('duration').textContent=x.duration?fmtTime(x.duration):'--:--';document.getElementById('seekNotice').textContent=x.seeking?'Spule zur gewünschten Position – bitte warten …':'';document.getElementById('trackPath').textContent=x.path ? x.path.substring(0,x.path.lastIndexOf('/')) || '/' : ''; if(!quickPending){document.getElementById('repeatBtn').textContent=x.repeat?'Repeat: Ein':'Repeat: Aus';document.getElementById('shuffleBtn').textContent=['Shuffle: Aus','Shuffle: Ordner','Shuffle: Rekursiv'][Number(x.shuffle)]||'Shuffle';}highlightPlaying();document.getElementById('state').textContent=x.seeking?'Suche Titelposition …':(x.playing?'Wiedergabe':(x.paused?'Pause':'Bereit'));document.getElementById('track').textContent=x.title||'Kein Titel';document.getElementById('volume').value=x.volume;document.getElementById('voltext').textContent=x.volume+' %';refreshSourceOptions(Number(x.sourceMask));document.getElementById('source').value=x.source;if(browseSource!==Number(x.source)){browsePath='/';browseGeneration++;browseSource=Number(x.source);if(document.getElementById('tab-player').classList.contains('active'))browseLoad()}document.getElementById('network').textContent=x.transport+' · '+x.ip+(x.bssid?' · '+(x.apName?x.apName+' ('+x.bssid+')':x.bssid):'');document.getElementById('settingsConnection').textContent=(x.ip&&x.ip!=='0.0.0.0')?(x.transport+' · '+x.ip):'Keine Netzwerkverbindung';if(!configured){for(let k of ['host','port','net','shuffle','ledCount','ledEffect','ledBrightness','ledBaseBrightness','ledSensitivity','ledSpeed'])document.getElementById(k).value=x[k];document.getElementById('powerSwitch').checked=!!x.powerSwitch;document.getElementById('ledColor').value=x.ledColor||'#00bfff';syncLedFields();for(let k of ['repeat','autostart','ledEnabled'])document.getElementById(k).checked=x[k];for(const [i,id] of ['srcNas','srcUsb','srcBt','srcSd'].entries())document.getElementById(id).checked=!!(Number(x.sourceMask)&(1<<i));document.getElementById('securityEnabled').checked=!!x.authEnabled;document.getElementById('securityUsername').value=x.authUser||'admin';document.getElementById('securityState').textContent=x.authEnabled?'Passwortschutz ist aktiv':'Passwortschutz ist deaktiviert';document.getElementById('securityState').dataset.enabled=x.authEnabled?'1':'0';configured=true;savedSettingsSnapshot=settingsSignature();ledSavedSnapshot=ledSignature();}}catch(e){document.getElementById('state').textContent=e.message}}
function syncLedFields(){let effect=Number(document.getElementById('ledEffect').value);document.getElementById('ledColorRow').hidden=effect!==4;document.getElementById('ledSpeedRow').hidden=effect!==5&&effect!==6&&effect!==7&&effect!==8;document.getElementById('ledSpeedText').textContent=document.getElementById('ledSpeed').value}
// Änderungen sofort am LED-Stripe zeigen; erst Speichern schreibt in NVS.
let ledPreviewTimer=null;
let ledPreviewBusy=false;
let ledPreviewAgain=false;
async function sendLedPreview(){
 if(ledPreviewBusy){ledPreviewAgain=true;return;}
 ledPreviewBusy=true;
 try{
  const f=new URLSearchParams(new FormData(document.getElementById('ledSettings')));
  f.set('ledEnabled',document.getElementById('ledEnabled').checked?'1':'0');
  await req('/api/led/preview',{method:'POST',body:f});
  document.getElementById('ledMsg').textContent='Vorschau aktiv – zum dauerhaften Übernehmen speichern.';
 }catch(e){document.getElementById('ledMsg').textContent='Vorschau: '+e.message;}
 finally{ledPreviewBusy=false;if(ledPreviewAgain){ledPreviewAgain=false;queueLedPreview();}}
}
function queueLedPreview(){clearTimeout(ledPreviewTimer);ledPreviewTimer=setTimeout(sendLedPreview,180);}
const ledForm=document.getElementById('ledSettings');
ledForm.addEventListener('input',e=>{syncLedFields();queueLedPreview();queueLedPersist();});
ledForm.addEventListener('change',e=>{syncLedFields();queueLedPreview();queueLedPersist();});
syncLedFields();
document.getElementById('ledSettings').onsubmit=e=>{e.preventDefault();clearTimeout(ledPersistTimer);saveLedAuto()};
document.getElementById('settings').onsubmit=e=>{e.preventDefault();clearTimeout(autoSettingsTimer);saveSettingsAuto()};
document.getElementById('security').onsubmit=async e=>{e.preventDefault();let enabled=document.getElementById('securityEnabled').checked;let user=document.getElementById('securityUsername').value.trim();let pass=document.getElementById('securityPassword').value;let previouslyEnabled=document.getElementById('securityState').dataset.enabled==='1';if(enabled && (!user || user.length>32)){document.getElementById('securityMsg').textContent='Benutzername muss 1 bis 32 Zeichen haben.';return;}if(enabled && !previouslyEnabled && pass.length<8){document.getElementById('securityMsg').textContent='Zum Aktivieren mindestens 8 Zeichen eingeben.';return;}try{let f=new URLSearchParams({enabled:enabled?'1':'0',username:user,password:pass});let x=await req('/api/security',{method:'POST',body:f});document.getElementById('securityMsg').textContent=x.message||'Gespeichert';document.getElementById('securityPassword').value='';document.getElementById('securityState').dataset.enabled=enabled?'1':'0';document.getElementById('securityState').textContent=enabled?'Passwortschutz ist aktiv':'Passwortschutz ist deaktiviert';if(enabled)document.getElementById('securityMsg').textContent+=' Bei geänderten Zugangsdaten fordert der Browser eine neue Anmeldung an.';}catch(err){document.getElementById('securityMsg').textContent=err.message}};

async function mqttStatusRefresh(){try{const d=await req('/api/mqtt');document.getElementById('mqttState').textContent=d.connected?'MQTT verbunden':(d.enabled?'MQTT nicht verbunden':'MQTT deaktiviert')}catch(e){document.getElementById('mqttState').textContent=e.message}}
async function mqttLoad(){try{const d=await req('/api/mqtt');document.getElementById('mqttEnabled').checked=!!d.enabled;document.getElementById('mqttHost').value=d.host||'';document.getElementById('mqttPort').value=d.port||1883;document.getElementById('mqttUser').value=d.username||'';document.getElementById('mqttPrefix').value=d.prefix||'nerdwave';document.getElementById('mqttState').textContent=d.connected?'MQTT verbunden':(d.enabled?'MQTT nicht verbunden':'MQTT deaktiviert');}catch(e){document.getElementById('mqttState').textContent=e.message}}
document.getElementById('mqttSettings').onsubmit=async e=>{e.preventDefault();const body=new URLSearchParams({enabled:document.getElementById('mqttEnabled').checked?'1':'0',host:document.getElementById('mqttHost').value.trim(),port:document.getElementById('mqttPort').value,username:document.getElementById('mqttUser').value,password:document.getElementById('mqttPassword').value,clearPassword:document.getElementById('mqttClearPass').checked?'1':'0',prefix:document.getElementById('mqttPrefix').value.trim()});try{await req('/api/mqtt',{method:'POST',body});document.getElementById('mqttPassword').value='';document.getElementById('mqttClearPass').checked=false;document.getElementById('mqttState').textContent='Einstellungen gespeichert · Verbindung wird aufgebaut';}catch(err){document.getElementById('mqttState').textContent='Fehler: '+err.message}};
mqttLoad();setInterval(()=>{if(document.getElementById('tab-settings').classList.contains('active'))mqttStatusRefresh()},4000);
freeBuildVariables();freeLoad();skinStatus();libRefresh();update();refreshOta();setInterval(update,2500);
</script></body></html>)HTML";

bool auth(){if(!authEnabled)return true;if(server.authenticate(username.c_str(),password.c_str()))return true;server.requestAuthentication(BASIC_AUTH,"Nerd-Wave");return false;}
void sendError(int code,const char* msg){server.send(code,"text/plain; charset=utf-8",msg);}
int number(const String& key,int fallback,int low,int high){if(!server.hasArg(key))return fallback;String raw=server.arg(key);for(size_t i=0;i<raw.length();i++)if(!isDigit(raw[i]))return fallback;long n=raw.toInt();return constrain((int)n,low,high);}
void skinLibraryList(){if(!auth())return;server.send(200,"application/json",NerdWaveSkinLibrary::list());}
void skinLibraryLoad(){if(!auth())return;String data,err;if(!NerdWaveSkinLibrary::load(server.arg("name"),data,err)){sendError(404,err.c_str());return;}server.send(200,"application/json",data);}
void skinLibrarySave(){if(!auth())return;String err;if(!NerdWaveSkinLibrary::save(server.arg("name"),server.arg("plain"),server.arg("overwrite")=="1",err)){sendError(400,err.c_str());return;}server.send(200,"application/json","{\"ok\":true}");}
void skinLibraryDelete(){if(!auth())return;String err;if(!NerdWaveSkinLibrary::remove(server.arg("name"),err)){sendError(404,err.c_str());return;}server.send(200,"application/json","{\"ok\":true}");}
void freeFieldsGet(){if(!auth())return;server.send(200,"application/json",NerdWaveFields::json());}
void freeFieldsUpdate(bool save){if(!auth())return;String err;if(!NerdWaveFields::apply(server.arg("plain"),save,err)){sendError(400,err.c_str());return;}c->redraw=true;server.send(200,"application/json","{\"ok\":true}");}
void freeFieldsPreview(){freeFieldsUpdate(false);}
void freeFieldsSave(){freeFieldsUpdate(true);}
void skinGet(){if(!auth())return;server.send(200,"application/json",NerdWaveSkins::json());}
void skinSelect(){if(!auth())return;String id=server.arg("id");if(id=="custom") {
  if(!NerdWaveSkins::selectCustom()){sendError(500,"Eigener Skin konnte nicht aktiviert werden");return;}
 }else if(!NerdWaveSkins::selectBuiltin(id)){sendError(400,"Unbekannter Skin");return;}
 c->redraw=true;server.send(200,"application/json","{\"ok\":true}");}
void skinUpload(){if(!auth())return;String err;String raw=server.arg("plain");if(!NerdWaveSkins::applyJson(raw,err)){sendError(400,err.c_str());return;}c->redraw=true;server.send(200,"application/json","{\"ok\":true}");}
void skinLayoutChange(bool persist){
 if(!auth())return;
 const char* keys[]={"sourceY","iconsY","titleY","timeY","stateY","sourceX","iconsX","titleX","timeX","stateX","sourceFont","titleFont","timeFont","stateFont"};
 const int lows[]={0,0,0,0,0,0,0,0,0,0,1,0,1,1};
 const int highs[]={239,239,239,239,239,239,239,239,239,239,3,3,3,3};
 uint8_t values[14];
 for(int i=0;i<14;i++){
  if(!server.hasArg(keys[i])){sendError(400,"Fehlender Layoutwert");return;}
  const String raw=server.arg(keys[i]);
  if(raw.isEmpty()){sendError(400,"Leerer Layoutwert");return;}
  for(size_t j=0;j<raw.length();j++)if(!isDigit(raw[j])){sendError(400,"Ungültiger Layoutwert");return;}
  const int v=raw.toInt();if(v<lows[i]||v>highs[i]){sendError(400,"Layoutwert außerhalb des gültigen Bereichs");return;}
  values[i]=static_cast<uint8_t>(v);
 }
 NerdWaveSkins::Layout next=NerdWaveSkins::layout();
 next.sourceY=values[0];next.iconsY=values[1];next.titleY=values[2];next.timeY=values[3];next.stateY=values[4];
 next.sourceX=values[5];next.iconsX=values[6];next.titleX=values[7];next.timeX=values[8];next.stateX=values[9];
 next.sourceFont=values[10];next.titleFont=values[11];next.timeFont=values[12];next.stateFont=values[13];
 next.visibleMask=0;
 const char* parts[]={"source","icons","title","time","state"};
 for(int i=0;i<5;i++) {
   String v=String(parts[i])+"Visible";
   if(server.arg(v)=="1")next.visibleMask|=(1<<i);
   String color=server.arg(String(parts[i])+"Color");
   if(color.length()!=7 || color[0]!='#'){sendError(400,"Ungültige Elementfarbe");return;}
   for(int k=1;k<7;k++)if(!isxdigit((unsigned char)color[k])){sendError(400,"Ungültige Elementfarbe");return;}
   uint32_t rgb=strtoul(color.c_str()+1,nullptr,16);
   next.elementColor[i]=uint16_t(((rgb>>19)&31)<<11 | ((rgb>>10)&63)<<5 | ((rgb>>3)&31));
 }
 next.iconSize=static_cast<uint8_t>(number("iconSize",next.iconSize,1,2));
 NerdWaveSkins::Decoration decor=NerdWaveSkins::decoration();
 auto getColor=[&](const String& key,uint16_t& out)->bool{String raw=server.arg(key);if(raw.length()!=7||raw[0]!='#')return false;for(int k=1;k<7;k++)if(!isxdigit((unsigned char)raw[k]))return false;uint32_t rgb=strtoul(raw.c_str()+1,nullptr,16);out=uint16_t(((rgb>>19)&31)<<11|((rgb>>10)&63)<<5|((rgb>>3)&31));return true;};
 if(server.hasArg("bgFirst")){if(!getColor("bgFirst",decor.first)||!getColor("bgSecond",decor.second)){sendError(400,"Ungültige Hintergrundfarbe");return;}decor.mode=number("bgMode",0,0,1);decor.direction=number("bgDirection",0,0,1);}
 for(int i=0;i<5;i++){
  String key=parts[i];if(server.hasArg(key+"BgColor")){
   if(!getColor(key+"BgColor",decor.bgColor[i])){sendError(400,"Ungültige Element-Hintergrundfarbe");return;}
   decor.bgEnabled[i]=server.arg(key+"BgEnabled")=="1";
   decor.bgPadding[i]=number(key+"Padding",0,0,2);
   String txt=server.arg(key+"Text");if(txt.length()>96){sendError(400,"Text zu lang");return;}decor.text[i]=txt;
  }
 }

 String err;
 if(!NerdWaveSkins::setLayout(next,persist,err)){sendError(400,err.c_str());return;}
 if(!NerdWaveSkins::setDecoration(decor,persist,err)){sendError(400,err.c_str());return;}
 c->redraw=true;
 server.send(200,"application/json","{\"ok\":true}");
}
void skinLayoutPreview(){skinLayoutChange(false);}
void skinLayoutSave(){skinLayoutChange(true);}
void skinBackgroundUpdate(bool save){
 if(!auth())return;
 JsonDocument doc;
 String raw=server.arg("plain");
 if(deserializeJson(doc,raw)){sendError(400,"Ungültiges JSON");return;}
 String mode=doc["mode"].as<String>();
 String direction=doc["direction"]|"vertical";
 NerdWaveSkins::Decoration decor=NerdWaveSkins::decoration();
 decor.mode=mode=="gradient"?1:(mode=="bands"?2:0);
 decor.direction=direction=="horizontal"?1:0;
 auto parse565=[&](JsonVariantConst v,uint16_t& out)->bool{ if(!v.is<const char*>()) return false; String c=v.as<String>(); if(c.length()!=7||c[0]!='#') return false; for(int i=1;i<7;i++) if(!isxdigit((unsigned char)c[i])) return false; uint32_t rgb=strtoul(c.c_str()+1,nullptr,16); out=uint16_t(((rgb>>19)&31)<<11 | ((rgb>>10)&63)<<5 | ((rgb>>3)&31)); return true; };
 if(!parse565(doc["first"],decor.first)){sendError(400,"Ungültige Hintergrundfarbe A");return;}
 if(!parse565(doc["second"],decor.second)) decor.second=decor.first;
 JsonArrayConst bands=doc["bands"].as<JsonArrayConst>();
 if(decor.mode==2){
   if(bands.isNull() || bands.size()==0){sendError(400,"Mindestens ein Hintergrund-Bereich erforderlich");return;}
   uint8_t n=0;
   for(JsonObjectConst band : bands){
     if(n>=NerdWaveSkins::Decoration::MAX_BANDS) break;
     int start=band["start"]|0,end=band["end"]|0;
     uint16_t col;
     if(start<0||start>239||end<0||end>239||start>end||!parse565(band["color"],col)){sendError(400,"Ungültiger Hintergrund-Bereich");return;}
     decor.bandStart[n]=uint8_t(start);
     decor.bandEnd[n]=uint8_t(end);
     decor.bandColor[n]=col;
     n++;
   }
   decor.bandCount=n;
 } else {
   decor.bandCount=1; decor.bandStart[0]=0; decor.bandEnd[0]=239; decor.bandColor[0]=decor.first;
 }
 String err;
 if(!NerdWaveSkins::setDecoration(decor,save,err)){sendError(400,err.c_str());return;}
 c->redraw=true;
 server.send(200,"application/json", save?"{\"ok\":true,\"message\":\"Hintergrund gespeichert\"}":"{\"ok\":true,\"message\":\"Vorschau aktiv\"}");
}
void skinBackgroundPreview(){skinBackgroundUpdate(false);}
void skinBackgroundSave(){skinBackgroundUpdate(true);}
void status(){if(!auth())return;JsonDocument doc;doc["version"]=Config::VERSION;doc["uptimeMs"]=millis();doc["authEnabled"]=authEnabled;doc["authUser"]=username;doc["title"]=c->playingName;doc["path"]=c->musicSource==2?String(""):c->selectedPath;doc["elapsed"]=(uint32_t)(c->baseSeconds+(c->sampleRate?c->bytesPlayed/((uint64_t)c->sampleRate*4):0));doc["duration"]=(uint32_t)(c->durationSeconds ? c->durationSeconds : c->seekDisplayDuration);doc["seeking"]=bool(c->seeking);doc["source"]=c->musicSource;doc["sourceMask"]=NerdWaveSources::enabledMask();doc["playing"]=bool(c->playerActive && !c->playbackPaused);doc["paused"]=bool(c->playbackPaused);doc["volume"]=(int)c->volumePercent;doc["host"]=NerdWaveNasConfig::host();doc["port"]=NerdWaveNasConfig::port();doc["net"]=c->networkMode;doc["powerSwitch"]=c->prefs.getBool("pwrSwitch",false);doc["repeat"]=c->repeatAll;doc["shuffle"]=c->shuffleMode;doc["autostart"]=c->autoStartLast;doc["ledEnabled"]=ledEnabled;doc["ledCount"]=ledCount;doc["ledEffect"]=ledEffect;doc["ledBrightness"]=ledBrightness;doc["ledBaseBrightness"]=ledBaseBrightness;doc["ledSensitivity"]=ledSensitivity;doc["ledSpeed"]=ledRainbowSpeed;char colorText[8];snprintf(colorText,sizeof(colorText),"#%06lx",(unsigned long)ledSolidColor);doc["ledColor"]=colorText;const bool wlanActive=WiFi.status()==WL_CONNECTED && WiFi.localIP()!=IPAddress(0,0,0,0);const bool lanActive=NerdWaveNetwork::lanUsable() && Ethernet.localIP()!=IPAddress(0,0,0,0);doc["transport"]=wlanActive?"WLAN":lanActive?"LAN":"Offline";doc["ip"]=wlanActive?WiFi.localIP().toString():lanActive?Ethernet.localIP().toString():String("0.0.0.0");doc["bssid"]=WiFi.status()==WL_CONNECTED?WiFi.BSSIDstr():String("");doc["apName"]=WiFi.status()==WL_CONNECTED?NerdWaveWifiFilter::name(WiFi.BSSIDstr()):String("");String payload;serializeJson(doc,payload);server.send(200,"application/json",payload);}
// Der Webbrowser nutzt exakt dieselbe MusicApi wie der lokale Player.
// Limitierte Seitengroesse und maximale Pfadlaenge begrenzen RAM-Verbrauch.
void browse(){
 if(!auth())return;
 uint8_t source=(uint8_t)number("source",c->musicSource,0,3);
 if(source!=c->musicSource){sendError(409,"Quelle gewechselt. Browser aktualisieren.");return;}
 if(source==2){sendError(409,"Bluetooth hat keinen Dateibrowser");return;}
 String path=server.arg("path");if(path.isEmpty())path="/";
 if(path.length()>300 || !path.startsWith("/") || path.indexOf("..")>=0 || path.indexOf('\\')>=0){sendError(400,"Ungueltiger Ordnerpfad");return;}
 uint16_t offset=(uint16_t)number("offset",0,0,Config::MAX_TOTAL);
 MusicPage page;MusicApi api;
 if(!api.fetchFrom(source,path,offset,page)){sendError(503,api.error().c_str());return;}
 JsonDocument doc;doc["path"]=path;doc["source"]=source;doc["offset"]=page.offset;doc["total"]=page.total;
 JsonArray items=doc["items"].to<JsonArray>();
 for(uint16_t i=0;i<page.count;++i){JsonObject item=items.add<JsonObject>();item["name"]=page.items[i].name;item["path"]=page.items[i].path;item["directory"]=page.items[i].directory;item["size"]=page.items[i].size;}
 String payload;serializeJson(doc,payload);server.send(200,"application/json",payload);
}
void browsePlay(){
 if(!auth())return;
 uint8_t source=(uint8_t)number("source",255,0,255);
 if(source!=c->musicSource || source==2){sendError(409,"Quelle nicht bereit");return;}
 String path=server.arg("path");
 if(path.length()>300 || !path.startsWith("/") || path.indexOf("..")>=0 || path.indexOf('\\')>=0){sendError(400,"Ungueltiger Pfad");return;}
 uint16_t offset=(uint16_t)number("offset",0,0,Config::MAX_TOTAL);
 int index=number("index",-1,0,Config::PAGE_SIZE-1);
 if(index<0){sendError(400,"Titelindex ungueltig");return;}
 MusicPage page;MusicApi api;
 if(!api.fetchFrom(source,path,offset,page)){sendError(503,api.error().c_str());return;}
 if(index>=page.count || page.items[index].directory){sendError(400,"Kein abspielbarer Titel");return;}
 String lower=page.items[index].name;lower.toLowerCase();
 if(!lower.endsWith(".mp3")){sendError(400,"Nur MP3-Dateien abspielbar");return;}
 const MusicItem& song=page.items[index];
 if(!c->playPath || !c->playPath(song.path,song.name)){sendError(409,"Wiedergabestart fehlgeschlagen");return;}
 c->page=1;c->redraw=true;
 server.send(200,"application/json","{\"ok\":true}");
}
void wifiAps(){
 if(!auth())return;
 if(WiFi.status()!=WL_CONNECTED){sendError(409,"WLAN nicht verbunden");return;}
 if(c->playerActive||c->btPlaying){if(!c->stopPlayback || !c->stopPlayback()){sendError(409,"Wiedergabe konnte für den WLAN-Scan nicht beendet werden");return;}}
 int count=WiFi.scanNetworks(false,true);
 JsonDocument doc;doc["current"]=WiFi.BSSIDstr();doc["blocked"]=NerdWaveWifiFilter::entries();
 JsonArray aps=doc["aps"].to<JsonArray>();
 String currentSsid=WiFi.SSID();
 for(int i=0;i<count;i++){
  if(WiFi.SSID(i)!=currentSsid)continue;
  JsonObject ap=aps.add<JsonObject>();
  String b=WiFi.BSSIDstr(i);ap["bssid"]=b;ap["rssi"]=WiFi.RSSI(i);
  ap["channel"]=WiFi.channel(i);ap["blocked"]=NerdWaveWifiFilter::blocked(b);
  ap["current"]=b.equalsIgnoreCase(WiFi.BSSIDstr());ap["name"]=NerdWaveWifiFilter::name(b);
 }
 WiFi.scanDelete();String json;serializeJson(doc,json);server.send(200,"application/json",json);
}
void wifiName(){
 if(!auth())return;
 if(!NerdWaveWifiFilter::setName(server.arg("bssid"),server.arg("name"))){sendError(400,"Ungültige BSSID oder Name zu lang");return;}
 server.send(200,"application/json","{\"ok\":true}");
}
void wifiBlock(){
 if(!auth())return;
 String b=server.arg("bssid");String action=server.arg("action");
 bool ok=(action=="block")?NerdWaveWifiFilter::add(b):
         (action=="allow")?NerdWaveWifiFilter::remove(b):false;
 if(!ok){sendError(400,"BSSID ungueltig oder Sperrliste voll");return;}
 server.send(200,"application/json","{\"ok\":true}");
 // If current access point was just blocked, the main-loop watchdog will
 // reconnect. Delaying allows the browser to receive the response.
}
void otaInfo(){
 if(!auth())return;
 JsonDocument doc;
 const esp_partition_t* slot=otaPartition();
 doc["available"]=slot!=nullptr;
 doc["capacity"]=slot?(uint32_t)slot->size:0;
 doc["partition"]=slot?String(slot->label):String("");
 doc["reason"]=slot?"": "Keine getrennte OTA-App-Partition. In Arduino IDE OTA-Partitionsschema auswaehlen und einmal per USB flashen.";
 String body;serializeJson(doc,body);server.send(200,"application/json",body);
}
void otaUpload(){
 HTTPUpload& upload=server.upload();
 if(upload.status==UPLOAD_FILE_START){
   otaError="";otaSuccess=false;otaActive=false;otaWritten=0;otaExpected=0;
   if(!authEnabled || server.authenticate(username.c_str(),password.c_str())){
     // Browser koennen eine fremde Website nicht fuer Cross-Origin-OTA nutzen.
     // CLI-Clients ohne Origin-Header bleiben fuer Wartungszwecke moeglich.
     String origin=server.header("Origin");
     if(origin.length()){
       String expected=String("http://")+server.hostHeader();
       if(origin!=expected){otaError="Upload von fremder Website abgelehnt";return;}
     }
     const esp_partition_t* slot=otaPartition();
     if(!slot){otaError="Keine zweite OTA-App-Partition vorhanden";return;}
     if(c && c->stopPlayback){
       if(!c->stopPlayback()){
         otaError="Wiedergabe konnte nicht beendet werden";return;
       }
     }
     if(!server.hasArg("size")){otaError="Dateigroesse fehlt";return;}
     String raw=server.arg("size");
     if(raw.isEmpty()){otaError="Ungueltige Dateigroesse";return;}
     for(size_t i=0;i<raw.length();++i)if(!isDigit(raw[i])){otaError="Ungueltige Dateigroesse";return;}
     unsigned long long requested=strtoull(raw.c_str(),nullptr,10);
     if(requested<1024 || requested>slot->size){otaError="Firmware passt nicht in OTA-Partition";return;}
     if(!upload.filename.endsWith(".bin")){otaError="Nur .bin-Firmware akzeptiert";return;}
     otaExpected=(size_t)requested;
     if(!Update.begin(otaExpected,U_FLASH)){otaError=String("Update.begin: ")+Update.errorString();return;}
     otaActive=true;
     NerdWaveLog::output.printf("[OTA] Start, %lu Bytes, Slot %s (%lu Bytes)\n",(unsigned long)otaExpected,slot->label,(unsigned long)slot->size);
   }else otaError="Nicht autorisiert";
 }else if(upload.status==UPLOAD_FILE_WRITE){
   if(!otaActive)return;
   if(otaWritten==0 && (upload.currentSize==0 || upload.buf[0]!=0xE9)){
     otaError="Datei ist kein ESP32-Firmware-Image";Update.abort();otaActive=false;return;
   }
   if(otaWritten+upload.currentSize>otaExpected || Update.write(upload.buf,upload.currentSize)!=upload.currentSize){
     otaError=String("Flash-Schreibfehler: ")+Update.errorString();Update.abort();otaActive=false;return;
   }
   otaWritten+=upload.currentSize;
 }else if(upload.status==UPLOAD_FILE_END){
   if(!otaActive)return;
   if(otaWritten!=otaExpected){otaError="Dateilaenge stimmt nicht ueberein";Update.abort();otaActive=false;return;}
   if(!Update.end(false)){otaError=String("Firmware ungueltig: ")+Update.errorString();otaActive=false;return;}
   otaActive=false;otaSuccess=true;
   NerdWaveLog::output.printf("[OTA] Fertig: %lu Bytes. Neustart folgt.\n",(unsigned long)otaWritten);
 }else if(upload.status==UPLOAD_FILE_ABORTED){
   if(otaActive)Update.abort();otaActive=false;otaError="Upload abgebrochen";
 }
}
void otaFinish(){
 if(!auth())return;
 if(!otaSuccess){String message=otaError.length()?otaError:"Kein erfolgreiches Firmware-Image empfangen";JsonDocument doc;doc["ok"]=false;doc["error"]=message;String body;serializeJson(doc,body);server.send(400,"application/json",body);return;}
 server.sendHeader("Connection","close");
 server.send(200,"application/json","{\"ok\":true,\"message\":\"Firmware geschrieben und geprueft\"}");
 otaSuccess=false;otaRebootPending=true;otaRebootAt=millis()+1200;
}
void control(){if(!auth())return;String cmd=server.arg("cmd");bool ok=true;
 if(cmd=="toggle")ok=c->playPause();
 else if(cmd=="next")ok=c->skip(1);
 else if(cmd=="prev")ok=c->skip(-1);
 else if(cmd=="seek")ok=c->seekTo && c->seekTo((uint32_t)number("value",0,0,86400));
 else if(cmd=="volume")c->setVolume(number("value",(int)c->volumePercent,0,100));
 else if(cmd=="source"){
   const uint8_t target=(uint8_t)number("value",c->musicSource,0,3);
   if(!NerdWaveSources::enabled(target)){sendError(409,"Musikquelle deaktiviert");return;}
   c->requestSource(target);
 }
 else {sendError(400,"Unbekannte Aktion");return;}
 if(!ok){sendError(409,"Aktion nicht moeglich");return;}
 c->redraw=true;server.send(200,"application/json","{\"ok\":true}");}
void logGet(){
 if(!auth())return;
 uint32_t from=0;
 if(server.hasArg("from"))from=(uint32_t)strtoul(server.arg("from").c_str(),nullptr,10);
 uint32_t next=0;bool truncated=false;
 String text=NerdWaveLog::read(from,2048,next,truncated);
 JsonDocument d;d["text"]=text;d["next"]=next;d["truncated"]=truncated;
 String payload;serializeJson(d,payload);
 server.sendHeader("Cache-Control","no-store");
 server.send(200,"application/json",payload);
}
void mqttGet(){if(!auth())return;String payload;NerdWaveMqtt::configJson(payload);server.send(200,"application/json",payload);}
void mqttSave(){if(!auth())return;
 String error;const String password=server.arg("password");
 // A blank password means keep existing password; explicit reset is a separate checkbox.
 String p=password;
 if(p.isEmpty() && server.arg("clearPassword")!="1")p=c->prefs.getString("mqttPass","");
 uint16_t port=(uint16_t)number("port",1883,1,65535);
 if(!NerdWaveMqtt::save(server.arg("enabled")=="1",server.arg("host"),port,server.arg("username"),p,server.arg("prefix"),error)){sendError(400,error.c_str());return;}
 server.send(200,"application/json","{\"ok\":true}");}
void playbackSettings(){
 if(!auth())return;
 if(!server.hasArg("repeat") || !server.hasArg("shuffle")){sendError(400,"Fehlende Wiedergabeeinstellungen");return;}
 const String shuffle=server.arg("shuffle");
 if((server.arg("repeat")!="0" && server.arg("repeat")!="1") ||
    (shuffle!="0" && shuffle!="1" && shuffle!="2")){sendError(400,"Ungueltiger Wert");return;}
 c->repeatAll=server.arg("repeat")=="1";
 c->shuffleMode=(uint8_t)shuffle.toInt();
 c->savePlayback();
 c->redraw=true;
 server.send(200,"application/json","{\"ok\":true}");
}
void settings(){if(!auth())return;
 String host=server.arg("host");int port=number("port",-1,1,65535);IPAddress addr;
 if(!addr.fromString(host)||port<1){sendError(400,"NAS: gueltige IPv4-Adresse und Port erforderlich");return;}
 if(!NerdWaveNasConfig::set(c->prefs,host,(uint16_t)port)){sendError(400,"NAS ungültig");return;}
 c->prefs.putBool("pwrSwitch",server.arg("powerSwitch")=="1");
 c->networkMode=(uint8_t)number("net",c->networkMode,0,1);
 c->prefs.putBool("lanOn",c->networkMode==0);
 NerdWaveSettings::saveNetworkMode(c->prefs,c->networkMode);
 c->networkRestartPending=(c->networkMode!=c->bootNetworkMode);
 if(server.hasArg("sourceMask")){
   const uint8_t m=(uint8_t)number("sourceMask",0,0,15);
   if(!m){sendError(400,"Mindestens eine Musikquelle erforderlich");return;}
   NerdWaveSources::configure(c->prefs,m);
 }
 c->repeatAll=server.arg("repeat")=="1";c->shuffleMode=number("shuffle",c->shuffleMode,0,2);c->autoStartLast=server.arg("autostart")=="1";c->savePlayback();
 c->redraw=true;
 server.send(200,"application/json","{\"ok\":true,\"message\":\"Gespeichert. Netzwerkmodus nach Neustart aktiv.\"}");
}
void applyLedSettings(bool persist){
 if(!auth())return;
 uint16_t count=number("ledCount",ledCount,1,LED_MAX);bool resized=count!=ledCount;ledCount=count;
 ledEnabled=server.arg("ledEnabled")=="1";
 ledEffect=number("ledEffect",ledEffect,0,LED_EFFECT_COUNT-1);
 ledBrightness=number("ledBrightness",ledBrightness,0,100);
 ledBaseBrightness=number("ledBaseBrightness",ledBaseBrightness,0,20);
 ledSensitivity=number("ledSensitivity",ledSensitivity,1,100);
 ledRainbowSpeed=number("ledSpeed",ledRainbowSpeed,1,100);
 String color=server.arg("ledColor");
 if(color.length()==7 && color[0]=='#'){bool valid=true;for(int i=1;i<7;i++)if(!isxdigit((unsigned char)color[i]))valid=false;if(valid)ledSolidColor=strtoul(color.c_str()+1,nullptr,16);}
 if(resized)ledResize();if(persist)ledSave();c->redraw=true;
 server.send(200,"application/json",persist?"{\"ok\":true,\"message\":\"LED-Einstellungen gespeichert.\"}":"{\"ok\":true,\"message\":\"Vorschau aktiv.\"}");
}
void ledSettings(){applyLedSettings(true);}
void ledPreview(){applyLedSettings(false);}
void security(){
 if(!auth())return;
 if(!server.hasArg("enabled")){sendError(400,"enabled fehlt");return;}
 bool enable=server.arg("enabled")=="1";
 if(enable){
   String newUser=server.arg("username");newUser.trim();
   if(newUser.length()<1 || newUser.length()>32 || newUser.indexOf(':')>=0 || newUser.indexOf('\r')>=0 || newUser.indexOf('\n')>=0){sendError(400,"Benutzername ungueltig (1 bis 32 Zeichen, kein Doppelpunkt)");return;}
   String newPass=server.arg("password");
   if(newPass.isEmpty() && authEnabled)newPass=password;
   if(newPass.length()<8 || newPass.length()>128){sendError(400,"Passwort muss 8 bis 128 Zeichen haben");return;}
   // Erst speichern, dann den Schutz aktivieren; keine Kennwoerter im Log oder JSON.
   c->prefs.putString("webPass",newPass);
   c->prefs.putString("webUser",newUser);
   c->prefs.putBool("webAuth",true);
   password=newPass;
   username=newUser;
   authEnabled=true;
   server.send(200,"application/json","{\"ok\":true,\"message\":\"Passwortschutz aktiviert\"}");
 }else{
   c->prefs.putBool("webAuth",false);
   c->prefs.remove("webPass");
   c->prefs.remove("webUser");
   username="admin";
   authEnabled=false;
   password="";
   server.send(200,"application/json","{\"ok\":true,\"message\":\"Passwortschutz deaktiviert\"}");
 }
}
} // namespace
void begin(Context& ctx){c=&ctx;if(started)return;
 // Das Feature ist standardmaessig AUS, auch wenn fruehere Versionen
 // automatisch ein Passwort unter webPass gespeichert haben.
 authEnabled=ctx.prefs.getBool("webAuth",false);
 password=authEnabled?ctx.prefs.getString("webPass",""):String("");
 username=authEnabled?ctx.prefs.getString("webUser","admin"):String("admin");
 if(authEnabled && (password.length()<8 || username.isEmpty() || username.length()>32)){authEnabled=false;password="";ctx.prefs.putBool("webAuth",false);}
 const char* otaHeaders[]={"Origin"};server.collectHeaders(otaHeaders,1);
 server.on("/",HTTP_GET,[]{if(!auth())return;server.send_P(200,"text/html; charset=utf-8",WEB_PAGE);});
 server.on("/api/wifi/aps",HTTP_GET,wifiAps);server.on("/api/wifi/block",HTTP_POST,wifiBlock);server.on("/api/wifi/name",HTTP_POST,wifiName);
 server.on("/api/ota/info",HTTP_GET,otaInfo);server.on("/api/ota",HTTP_POST,otaFinish,otaUpload);
 server.on("/api/browse",HTTP_GET,browse);server.on("/api/browse/play",HTTP_POST,browsePlay);
 server.on("/api/skin",HTTP_GET,skinGet);server.on("/api/skin/library",HTTP_GET,skinLibraryList);server.on("/api/skin/library/load",HTTP_GET,skinLibraryLoad);server.on("/api/skin/library/save",HTTP_POST,skinLibrarySave);server.on("/api/skin/library/delete",HTTP_POST,skinLibraryDelete);server.on("/api/skin/background/preview",HTTP_POST,skinBackgroundPreview);server.on("/api/skin/background/save",HTTP_POST,skinBackgroundSave);server.on("/api/skin/fields",HTTP_GET,freeFieldsGet);server.on("/api/skin/fields/preview",HTTP_POST,freeFieldsPreview);server.on("/api/skin/fields/save",HTTP_POST,freeFieldsSave);server.on("/api/skin/select",HTTP_POST,skinSelect);server.on("/api/skin/upload",HTTP_POST,skinUpload);server.on("/api/skin/layout/preview",HTTP_POST,skinLayoutPreview);server.on("/api/skin/layout/save",HTTP_POST,skinLayoutSave);
 server.on("/api/log",HTTP_GET,logGet);server.on("/api/mqtt",HTTP_GET,mqttGet);server.on("/api/mqtt",HTTP_POST,mqttSave);
 server.on("/api/status",HTTP_GET,status);server.on("/api/control",HTTP_POST,control);server.on("/api/settings",HTTP_POST,settings);server.on("/api/playback-settings",HTTP_POST,playbackSettings);server.on("/api/led",HTTP_POST,ledSettings);server.on("/api/led/preview",HTTP_POST,ledPreview);server.on("/api/security",HTTP_POST,security);
 server.onNotFound([]{if(!auth())return;sendError(404,"Nicht gefunden");});server.begin();started=true;
 NerdWaveLog::output.printf("[WEB] HTTP-Port 80, Passwortschutz: %s (im Web konfigurierbar)\n",authEnabled?"aktiv":"aus");}
void poll(){
 if(started)server.handleClient();
 if(otaRebootPending && (int32_t)(millis()-otaRebootAt)>=0){
   otaRebootPending=false;
   NerdWaveLog::output.println("[OTA] Neustart");Serial.flush();ESP.restart();
 }
}
} // namespace NerdWaveWeb
