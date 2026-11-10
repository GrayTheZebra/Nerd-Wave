#pragma once

#include <Arduino.h>
#include <EspUsbHost.h>

// USB-Host, FAT32-Mount und Hot-Plug-Status.
// Die Reaktion auf Einstecken/Entfernen (Browser, Player-Stop) bleibt im loopTask.
namespace NerdWaveUsb {
extern EspUsbHostMscFS usbDisk;
extern bool hostStarted;
extern bool previouslyMounted;
extern bool removalPending;
extern bool browserRefreshPending;
extern bool everMounted;

bool startHost();
bool tryMount(bool userRequested = false);
}
