# Nixie Clock – ESP32 Display Controller

Multi-display clock firmware for an ESP32-based Nixie-style clock with five TFT displays, a WS2812 LED strip, SD card, and a browser-based configuration interface.

---

## Inhaltsverzeichnis

- [Hardware](#hardware)
- [Pin-Belegung](#pin-belegung)
- [Software-Voraussetzungen](#software-voraussetzungen)
- [Projekt einrichten](#projekt-einrichten)
- [WLAN-Zugangsdaten (credentials.h)](#wlan-zugangsdaten-credentialsh)
- [Bauen & Flashen](#bauen--flashen)
- [Web-Interface](#web-interface)
- [HTTP-API](#http-api)
- [NVS-Einstellungen (Persistenz)](#nvs-einstellungen-persistenz)
- [NeoPixel-Modi](#neopixel-modi)
- [FreeRTOS-Tasks](#freertos-tasks)
- [Projektstruktur](#projektstruktur)

---

## Hardware

| Komponente | Beschreibung |
|---|---|
| **Mikrocontroller** | ESP32 (Dual-Core, 240 MHz, 4 MB Flash) |
| **Display 0** | Adafruit ST7735 – 1,8" TFT, 160×128 px (Info-/Status-Display) |
| **Display 1–4** | Adafruit ST7789 – 1,14" TFT, 240×135 px (Ziffern-Displays) |
| **LED-Strip** | WS2812B NeoPixel, 32 LEDs, an GPIO 33 |
| **Display-Backlight** | PCA9685 PWM-Controller (I²C, Adresse `0x41`), Kanäle 0–4 |
| **SD-Karte** | SPI, FAT32/exFAT (via SdFat-Adafruit-Fork) |

---

## Pin-Belegung

### SPI-Bus (geteilt von allen Displays und SD-Karte)

| Signal | GPIO |
|---|---|
| MOSI | 23 |
| MISO | 19 |
| CLK | 18 |

### Display-Steuerleitungen

| Signal | GPIO | Beschreibung |
|---|---|---|
| TFT_DC | 25 | Data/Command – Display 1–4 (ST7789) |
| TFT_DC_2 | 17 | Data/Command – Display 0 (ST7735) |
| TFT_RESET | 15 | Gemeinsamer Hardware-Reset |
| DISPLAY_POW_EN | 26 | Versorgungsspannung der Displays (HIGH = Ein) |

### Chip-Select-Leitungen

| Signal | GPIO | Display |
|---|---|---|
| TFT_CS_0 | 27 | ST7789 Display 1 (Stunde Zehner) |
| TFT_CS_1 | 16 | ST7789 Display 2 (Stunde Einer) |
| TFT_CS_2 | 32 | ST7789 Display 3 (Minute Zehner) |
| TFT_CS_3 | 13 | ST7789 Display 4 (Minute Einer) |
| TFT_CS_4 | 14 | ST7735 Display 0 (Info-Display) |
| SD_CS | 5 | SD-Karten-Modul (Chip Select) |
| SD_BUF_OE | 4 | 74LVC1G125 Buffer /OE (CLK+MOSI zu SD) |

### Weitere GPIOs

| Signal | GPIO | Beschreibung |
|---|---|---|
| LED_PIN | 33 | WS2812B NeoPixel Datenleitung |
| INT_IO_PIN | 34 | Externer Interrupt (Eingang) |
| WAKE_UP_PIN | 35 | Wake-up-Eingang |
| ADC_0_PIN | 36 | ADC1 Kanal 0 |
| ADC_3_PIN | 39 | ADC1 Kanal 3 |

---

## Software-Voraussetzungen

- [PlatformIO](https://platformio.org/) (VS Code Extension oder CLI)
- ESP32 Arduino Framework (wird automatisch von PlatformIO geladen)

### Bibliotheken (automatisch via `platformio.ini`)

| Bibliothek | Version | Zweck |
|---|---|---|
| Adafruit GFX Library | ^1.11.10 | Grafik-Grundfunktionen |
| Adafruit ST7735 and ST7789 Library | ^1.10.4 | TFT-Treiber |
| Bonezegei_PCA9685 | ^1.0.0 | Backlight-PWM |
| Adafruit NeoPixel | ^1.12.3 | WS2812B LED-Strip |
| SdFat – Adafruit Fork | ^2.2.3 | SD-Karte |
| WebServer | (ESP32 built-in) | HTTP-Server |
| Preferences | (ESP32 built-in) | NVS-Persistenz |
| WiFi | (ESP32 built-in) | WLAN-Verbindung |

---

## Projekt einrichten

```bash
git clone <repo-url>
cd software/NixieClock

# WLAN-Zugangsdaten anlegen (wird NICHT committed)
cp src/credentials.h.example src/credentials.h
# credentials.h mit echten Daten befüllen (siehe nächster Abschnitt)
```

---

## WLAN-Zugangsdaten (credentials.h)

Die Datei `src/credentials.h` enthält SSID und Passwort und ist in `.gitignore` eingetragen – sie wird **nie** ins Repository hochgeladen.

### Schritt 1 – Datei anlegen

```bash
cp src/credentials.h.example src/credentials.h
```

### Schritt 2 – Zugangsdaten eintragen

```c
// src/credentials.h
#pragma once

#define AP_SSID "Mein-WLAN"
#define AP_PASS "MeinPasswort123"
```

> **Hinweis:** `credentials.h.example` ist die versionierte Vorlage mit Platzhaltern. Diese Datei nicht mit echten Daten befüllen.

### Warum so?

```
src/credentials.h          ← echte Daten, in .gitignore eingetragen
src/credentials.h.example  ← Vorlage, wird committed
.gitignore                 ← enthält: src/credentials.h
```

---

## Bauen & Flashen

```bash
# Nur bauen
platformio run

# Bauen + auf ESP32 flashen
platformio run --target upload

# Seriellen Monitor öffnen (115200 Baud)
platformio device monitor --baud 115200
```

Oder über die PlatformIO-Buttons in VS Code (Build / Upload / Monitor).

---

## Web-Interface

Nach dem Start verbindet sich der ESP32 per WLAN. Die IP-Adresse wird im seriellen Monitor und auf dem Info-Display angezeigt.

**Aufruf im Browser:**
```
http://<IP-Adresse>
```

> **Wichtig:** Immer `http://` verwenden – kein `https://`. Browser versuchen sonst automatisch HTTPS, das nicht unterstützt wird.

Das Interface hat drei Tabs:

### Tab: System
- Freier Heap-Speicher
- WLAN-Signalstärke (RSSI)
- Betriebszeit (Uptime)
- SD-Karten-Status
- IP-Adresse
- Firmware-Version / Build-Datum
- CPU-Takt / Chip-Modell
- NTP-Server konfigurieren
- UTC-Offset einstellen

### Tab: NeoPixel
- Betriebsmodus wählen (Aus / Statisch / Regenbogen / Atmen / Farbwechsel)
- Farbe per Farbwähler
- Helligkeit (0–255)

### Tab: Display
- Anzeigedauer je Bild (Sekunden)
- Display-Helligkeit (0–4095, über PCA9685)
- **Anzeige-Optionen:**
  - Bilderwechsel (Dia-Show) An/Aus
  - Kalender An/Aus
  - Hintergrundbild An/Aus

Alle Einstellungen werden beim Klick auf **Speichern** dauerhaft im NVS gespeichert und beim nächsten Neustart automatisch wiederhergestellt.

---

## HTTP-API

Alle Endpunkte akzeptieren `GET`-Anfragen.

### `GET /xml`

Liefert den aktuellen Systemstatus als XML (wird vom Web-Interface alle 2 Sekunden abgefragt).

```xml
<?xml version='1.0'?>
<Data>
  <HEAP>123456</HEAP>       <!-- Freier Heap in Bytes -->
  <RSSI>-65</RSSI>          <!-- WLAN-Signalstärke dBm -->
  <UPTIME>3600</UPTIME>     <!-- Laufzeit in Sekunden -->
  <SD>1</SD>                <!-- SD-Karte: 1=OK, 0=Fehlt -->
  <IP>192.168.1.100</IP>
  <NTP>de.pool.ntp.org</NTP>
  <UTC>1</UTC>
  <VER>3.0.0</VER>
  <BUILD>May 29 2026 12:00:00</BUILD>
  <CPU>240</CPU>
  <CHIP>ESP32-D0WDQ6</CHIP>
  <RGBM>1</RGBM>            <!-- NeoPixel-Modus 0–4 -->
  <DPINT>5</DPINT>          <!-- Anzeigedauer je Bild (s) -->
  <DPBR>4095</DPBR>         <!-- Display-Helligkeit 0–4095 -->
  <SLIDE>0</SLIDE>          <!-- Bilderwechsel: 1=An -->
  <CAL>1</CAL>              <!-- Kalender: 1=An -->
  <BG>0</BG>                <!-- Hintergrundbild: 1=An -->
</Data>
```

---

### `GET /SET_NTP`

NTP-Server und Zeitzone konfigurieren.

| Parameter | Typ | Beschreibung |
|---|---|---|
| `SERVER` | string | Hostname des NTP-Servers (z. B. `de.pool.ntp.org`) |
| `UTC` | int | UTC-Offset in Stunden (z. B. `1` für MEZ) |

**Beispiel:**
```
GET /SET_NTP?SERVER=de.pool.ntp.org&UTC=1
```

---

### `GET /SET_RGB`

NeoPixel-LED-Strip konfigurieren.

| Parameter | Typ | Wertebereich | Beschreibung |
|---|---|---|---|
| `MODE` | int | 0–4 | Betriebsmodus (siehe NeoPixel-Modi) |
| `R` | int | 0–255 | Rotanteil |
| `G` | int | 0–255 | Grünanteil |
| `B` | int | 0–255 | Blauanteil |
| `BRIGHT` | int | 0–255 | Gesamthelligkeit |

**Beispiel:**
```
GET /SET_RGB?MODE=1&R=255&G=102&B=0&BRIGHT=128
```

---

### `GET /SET_DISPLAY`

Display-Parameter konfigurieren.

| Parameter | Typ | Wertebereich | Beschreibung |
|---|---|---|---|
| `INTERVAL` | int | 1–60 | Anzeigedauer je Bild in Sekunden |
| `BRIGHT` | int | 0–4095 | Backlight-Helligkeit (PCA9685) |
| `SLIDE` | int | 0 / 1 | Bilderwechsel aktivieren |
| `CAL` | int | 0 / 1 | Kalender aktivieren |
| `BG` | int | 0 / 1 | Hintergrundbild aktivieren |

**Beispiel:**
```
GET /SET_DISPLAY?INTERVAL=10&BRIGHT=2048&SLIDE=0&CAL=1&BG=0
```

---

## NVS-Einstellungen (Persistenz)

Alle Einstellungen werden im ESP32-NVS (Non-Volatile Storage) unter dem Namespace `nixie` gespeichert.

| NVS-Key | Typ | Beschreibung | Standard |
|---|---|---|---|
| `ntp` | String | NTP-Server-Hostname | `de.pool.ntp.org` |
| `utc` | Int | UTC-Offset (Stunden) | `1` |
| `rgb_mode` | UInt | NeoPixel-Modus (0–4) | `0` (Aus) |
| `rgb_r` | UChar | Rotanteil (0–255) | `255` |
| `rgb_g` | UChar | Grünanteil (0–255) | `102` |
| `rgb_b` | UChar | Blauanteil (0–255) | `0` |
| `rgb_br` | UChar | LED-Helligkeit (0–255) | `128` |
| `dp_int` | UShort | Anzeigedauer je Bild (s) | `5` |
| `dp_br` | UShort | Display-Helligkeit (0–4095) | `4095` |
| `dp_slide` | UChar | Bilderwechsel (0/1) | `0` |
| `dp_cal` | UChar | Kalender (0/1) | `1` |
| `dp_bg` | UChar | Hintergrundbild (0/1) | `0` |

NVS zurücksetzen (alle gespeicherten Werte löschen):
```cpp
// In Arduino-Code:
Preferences prefs;
prefs.begin("nixie", false);
prefs.clear();
prefs.end();
```

Oder über das ESP32-Flash-Erase-Tool:
```bash
platformio run --target erase
```

---

## NeoPixel-Modi

| Modus | Name | Beschreibung |
|---|---|---|
| `0` | Aus | Alle LEDs aus (Helligkeit 0) |
| `1` | Statisch | Alle LEDs in der gewählten Farbe |
| `2` | Regenbogen | Laufender Regenbogen-Effekt über alle 32 LEDs |
| `3` | Atmen | Pulsierende Helligkeit (Sinuskurve) in der gewählten Farbe |
| `4` | Farbwechsel | Sequenzieller colorWipe-Effekt |

---

## FreeRTOS-Tasks

| Task | Funktion | Kern | Priorität | Stack |
|---|---|---|---|---|
| `tCodeServerTask` | WiFi-Verbindung + HTTP-WebServer | — | 3 | 80 000 B |
| `tCodeRGB` | NeoPixel-LED-Steuerung | — | 3 | 5 000 B |
| `tCodeUart` | UART-Kommunikation | — | 2 | 5 000 B |
| `tCodeDisplay` | Display-Ausgabe (Uhr, Kalender, Status) | — | 1 | 80 000 B |

**Startsequenz `tCodeServerTask`:**
1. Preferences laden (NVS → globale Structs)
2. Geladene RGB-Konfiguration in Queue senden (damit RGB-Task sofort startet)
3. WiFi verbinden
4. NTP konfigurieren (`configTzTime`)
5. HTTP-Routen registrieren + `server.begin()`
6. Loop: `server.handleClient()` alle 10 ms

**Kommunikation zwischen Tasks:**

```
WebServer-Task ──xQueue_RGB_Config──► RGB-Task
                (rgbConfigStruct)

Alle Tasks lesen systemConfig direkt (shared global, race-condition-arm aber
akzeptabel bei den gegebenen Zugriffsmustern).
```

---

## Projektstruktur

```
NixieClock/
├── platformio.ini              # Build-Konfiguration, Bibliotheksabhängigkeiten
├── .gitignore                  # .pio, credentials.h u. a.
├── README.md                   # Diese Datei
├── src/
│   ├── main.cpp                # setup(), loop(), tCodeDisplay-Task, Draw-Funktionen
│   ├── common.h                # Pin-Defines, Structs, Externs (geteilt von allen Tasks)
│   ├── credentials.h           # ⚠ NICHT in Git – echte WLAN-Daten (siehe .gitignore)
│   ├── credentials.h.example   # Vorlage für credentials.h (wird committed)
│   ├── vTask_WebServer.cpp/.h  # WiFi, HTTP-Server, Preferences
│   ├── vTask_rgb.cpp/.h        # NeoPixel WS2812B Steuerung
│   ├── vTask_uart.cpp/.h       # UART-Task
│   ├── WebPage.h               # HTML/CSS/JS der Web-UI (PROGMEM)
│   ├── webPage.html            # Quelldatei der Web-UI (zur Entwicklung)
│   ├── draw7Number.cpp         # 7-Segment-Zifferzeichnung für TFT
│   ├── draw7Numbers.h
│   ├── background_1.h          # Hintergrundbilder (RGB565-Arrays)
│   ├── birthday.h
│   ├── img_advent.h
│   ├── weather_icon_set.h
│   ├── weathericons.h
│   ├── 0.h – 5.h              # Ziffern-Grafiken
│   ├── 3_neon.h, 7_neon.h    # Neon-Stil Ziffern
│   └── icon/
│       ├── sd_card.h           # SD-Karten-Icon (32×32)
│       └── WLAN.h              # WLAN-Icon (32×32)
├── lib/                        # Lokale Bibliotheken (leer, via platformio.ini)
├── include/                    # Globale Header (leer)
└── test/                       # Unit-Tests (leer)
```

---

## Standard-Einstellungen beim ersten Start

| Einstellung | Standardwert | Änderbar via |
|---|---|---|
| NTP-Server | `de.pool.ntp.org` | Web-Interface → System |
| UTC-Offset | `+1` (MEZ/MESZ) | Web-Interface → System |
| NeoPixel | Aus | Web-Interface → NeoPixel |
| Bilderwechsel | Aus | Web-Interface → Display |
| Kalender | **An** | Web-Interface → Display |
| Hintergrundbild | Aus | Web-Interface → Display |
| Display-Helligkeit | 100 % (4095) | Web-Interface → Display |
| Anzeigedauer | 5 Sekunden | Web-Interface → Display |
