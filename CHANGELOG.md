# Changelog

## v1.0.1 – IR-Direktquellen und LED-Grundhelligkeit

- Vier neue lernbare IR-Befehle fuer direkte Quellenwahl NAS, USB, SD und BT; alte IR-Belegungen bleiben erhalten.
- Beim Pausieren/Stoppen werden eingeschaltete LEDs in allen Effektmodi mit eingestellter Grundhelligkeit dargestellt, statt in musikabhaengigen Modi komplett auszugehen.
- Quellen-IDs, Display-Renderer, Audiofunktionen und Netzwerkkonfiguration unveraendert.

## v1.0.0 – Release-Kandidat
- Aus dem erfolgreich getesteten Projektstand v0.5.231 vorbereitet.
- Einheitliche Versionskonstante in `Config.h` auf `v1.0.0` gesetzt.
- Haupt-README ergänzt: Build, Hardwarepins, Einschränkungen und Release-Checks.
- Veraltete, vorcompilierte `build/`-Artefakte aus dem ZIP entfernt, damit keine alte Firmware als v1.0.0 ausgegeben wird.
- Kein Eingriff in Player, Audio, Renderer, Netzwerk, USB/SD oder Shutdown.

## v0.5.231 – Referenzstand
- Im Display-Dateibrowser bei Rückkehr vom Player den gewählten Titel fokussieren.
- Vorherige Korrekturen der v0.5-Serie bleiben Bestandteil des Quellprojekts.
