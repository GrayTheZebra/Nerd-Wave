# Nerd-Wave Skins v0.5.143

- Classic Cyan und Retro Amber (gedämpfte Amber-Playertexte und farblich passende Status-Icons).
- Das bestehende JSON-Schema 1 bleibt kompatibel; optionaler `layout`-Block erlaubt die Y-Positionen der Quellenzeile (`sourceY`: 19–39), Statussymbole (`iconsY`: 56–80), Titelmitte (`titleY`: 106–135), Zeit (`timeY`: 143–161) und Playerstatus (`stateY`: 170–194).
- Im Web-Tab Display lassen sich die Layoutwerte bearbeiten und als JSON-Template herunterladen. Anschließend die Datei bei Bedarf in einem Texteditor anpassen und wieder hochladen. Farben sind weiterhin `#RRGGBB`.
- Player, Browser, Menüs und Symbolfarben verwenden die aktuelle Skinpalette. Das feste Original bleibt als Fallback eingebaut. Die NVS-Auswahl wird gespeichert.
- Größenlimit 3072 Byte. Unbekannte oder ungültige Layoutwerte werden abgelehnt.
- Noch nicht enthalten: ZIP-Bildpakete, frei positionierbare X-Koordinaten, Dateisystem-Skinbibliothek und WYSIWYG-Editor. Dazu ist ein abgesicherter Dateisystem- und Ressourcenpfad erforderlich; diese Funktionen werden nicht vorgetäuscht.
- Firmware noch nicht auf ESP32 kompiliert oder getestet. GPIOs und tft.setRotation(0) bleiben unverändert.

## Erweiterte Einstellungen (v0.5.145)

Für die fünf Playerbereiche gibt es jetzt X- und Y-Positionen. X bezeichnet die horizontale Mitte; Werte sind aus Sicherheitsgründen begrenzt. Außerdem können Quelle, Titel, Zeit und Status ihre Schriftgröße ändern (Titel: 0 = automatisch). Änderungen erscheinen sofort als Vorschau; erst „Layout speichern“ legt sie im NVS ab. Alte JSON-Skins und NVS-Layouts mit fünf Y-Positionen bleiben kompatibel.
