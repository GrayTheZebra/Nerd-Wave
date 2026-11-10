#include "NerdWaveLog.h"
#include "NerdWaveUsb.h"

namespace NerdWaveUsb {
static EspUsbHost usbHost;
EspUsbHostMscFS usbDisk;
bool hostStarted = false;
bool previouslyMounted = false;
bool removalPending = false;
bool browserRefreshPending = false;
bool everMounted = false;
static uint32_t lastMountAt = 0;

bool startHost() {
  if(hostStarted) return true;
  hostStarted=usbHost.begin();
  NerdWaveLog::output.printf("[USB] Host %s\n",hostStarted?"aktiv":"FEHLER");
  return hostStarted;
}

bool tryMount(bool userRequested) {
  if(!hostStarted || usbDisk.mounted()) return usbDisk.mounted();
  // Keine Dateisystem-Initialisierung ohne erkannten USB-Massenspeicher.
  // usbDisk.begin() kann sonst den Hauptloop blockieren (Encoder, Power, Web/OTA).
  // Die USB-Enumeration laeuft im Hintergrund; mscReady() ist nur eine Statusabfrage.
  if(!usbHost.mscReady()) return false;
  // Wenn ein Laufwerk gemeldet wird, Mountversuche bei Fehlern begrenzen.
  if(!userRequested && (uint32_t)(millis()-lastMountAt)<5000) return false;
  lastMountAt=millis();
  bool ok=usbDisk.begin(usbHost,"/usb");
  NerdWaveLog::output.printf("[USB] FAT32 %s\n",ok?"eingebunden":"nicht bereit");
  return ok;
}
}
