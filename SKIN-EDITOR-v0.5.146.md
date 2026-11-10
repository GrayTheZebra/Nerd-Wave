# Nerd-Wave Skin-Editor v0.5.146

Der Web-Tab **Display** bietet eine verschiebbare Vorschau. Für das GC9A01 (rund, 240 x 240) werden die Elemente Quelle, Symbole, Titel, Zeit und Status unmittelbar auf dem physischen Display verschoben. **Layout speichern** schreibt die Positionen ins NVS; die bisherigen sicheren Positionsgrenzen bleiben bestehen.

Weitere Vorlagen: rechteckig 240x240, Hochformat 240x280, Querformat 320x240, 320x170 und 480x320. Diese Vorlagen sind **reine Browserentwürfe** und lassen sich als `Nerd-Wave-Design-*.json` exportieren oder wieder importieren. Die Entwürfe ändern weder TFT_eSPI-Konfiguration noch GPIOs, Treiber, Rotation oder die vorhandene Firmwareauflösung. Für die tatsächliche Ausgabe auf anderen Displays wird später eine separate Treiberanbindung benötigt.

Das bisherige Skin-JSON mit `schema:1` ist von dem neuen Designexport getrennt. Das Skin-JSON für das Gerät bleibt auf GC9A01 240x240 beschränkt, um ungültige Hardwareprofile zu vermeiden. Die Encoder-, Audio-, Netzwerk- und Kopierlogik bleibt unverändert.
