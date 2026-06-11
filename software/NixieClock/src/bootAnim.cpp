/*
 * bootAnim.cpp
 * Boot-Animationen für Nixie Clock
 *
 * Displays:
 *   tft   (Index 0) – ST7735  160×128  Rotation 1  (kleines Display, Systemanzeige)
 *   tft_1 (Index 1) – ST7789  135×240  Rotation 2  (große Uhren-Displays)
 *   tft_2 (Index 2) – ST7789  135×240  Rotation 2
 *   tft_3 (Index 3) – ST7789  135×240  Rotation 2
 *   tft_4 (Index 4) – ST7789  135×240  Rotation 2
 *
 * CS-Pin-Mapping (identisch mit handle_cs in main.cpp):
 *   dispCS(0) → TFT_CS_4 → tft
 *   dispCS(1) → TFT_CS_0 → tft_1
 *   dispCS(2) → TFT_CS_1 → tft_2
 *   dispCS(3) → TFT_CS_2 → tft_3
 *   dispCS(4) → TFT_CS_3 → tft_4
 */

#include "bootAnim.h"
#include <Arduino.h>
#include <common.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_ST7789.h>
#include "draw7Numbers.h"

// ── externe Objekte aus main.cpp ───────────────────────────────────────────
extern Adafruit_ST7735  tft;
extern Adafruit_ST7789  tft_1, tft_2, tft_3, tft_4;
extern systemConfigStruct systemConfig;
extern volatile bool bootInitDone;

// ── Hilfsfunktionen ────────────────────────────────────────────────────────

// CS-Pins: Index entspricht handle_cs()-Nummerierung in main.cpp
static const uint8_t kCS[5] = {TFT_CS_4, TFT_CS_0, TFT_CS_1, TFT_CS_2, TFT_CS_3};
static void dispCS(uint8_t d, uint8_t lvl) { if (d < 5) digitalWrite(kCS[d], lvl); }

// Liefert Pointer auf einen der 4 großen Displays (d = 0..3)
static Adafruit_ST7789* bigDisp(uint8_t d)
{
    switch (d) {
        case 0: return &tft_1;
        case 1: return &tft_2;
        case 2: return &tft_3;
        default: return &tft_4;
    }
}

// RGB888 → RGB565
static inline uint16_t c565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

// HSV (h: 0-255, s=255, v=255) → RGB565
static uint16_t hsvColor(uint8_t h)
{
    uint8_t r, g, b;
    uint8_t region = h / 43;
    uint8_t rem    = (h - region * 43u) * 6u;
    uint8_t t = rem, q = 255u - rem;
    switch (region) {
        case 0: r=255; g=t;   b=0;   break;
        case 1: r=q;   g=255; b=0;   break;
        case 2: r=0;   g=255; b=t;   break;
        case 3: r=0;   g=q;   b=255; break;
        case 4: r=t;   g=0;   b=255; break;
        default:r=255; g=0;   b=q;   break;
    }
    return c565(r, g, b);
}

// ═══════════════════════════════════════════════════════════════════════════
// Animation 1: Matrix-Regen  (alle 5 Displays)
// ═══════════════════════════════════════════════════════════════════════════
static void animMatrix(uint32_t durationMs)
{
    // Großes Display-Layout (dispCS 1-4): 135×240
    const int BIG_COLS  = 22;  // 135/6
    const int BIG_ROWS  = 30;  // 240/8
    // Kleines Display (dispCS 0): 160×128
    const int SML_COLS  = 26;  // 160/6
    const int SML_ROWS  = 16;  // 128/8
    const int TRAIL     = 13;
    const int SML_TRAIL =  8;

    int16_t headBig[4][BIG_COLS];
    int16_t headSml[SML_COLS];

    // Init große Displays
    for (int d = 0; d < 4; d++) {
        dispCS(d + 1, LOW);
        bigDisp(d)->setFont(NULL);
        bigDisp(d)->setTextSize(1);
        bigDisp(d)->fillScreen(0x0000);
        dispCS(d + 1, HIGH);
        for (int c = 0; c < BIG_COLS; c++)
            headBig[d][c] = -(int16_t)random(0, BIG_ROWS + 4);
    }
    // Init kleines Display
    dispCS(0, LOW);
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.fillScreen(0x0000);
    dispCS(0, HIGH);
    for (int c = 0; c < SML_COLS; c++)
        headSml[c] = -(int16_t)random(0, SML_ROWS + 4);

    uint32_t start = millis();
    while (millis() - start < durationMs && (millis() - start < 10000 || !bootInitDone)) {
        // --- Große Displays ---
        for (int d = 0; d < 4; d++) {
            Adafruit_ST7789* t = bigDisp(d);
            dispCS(d + 1, LOW);
            for (int c = 0; c < BIG_COLS; c++) {
                int hy = headBig[d][c];
                if (hy >= 0 && hy < BIG_ROWS) {
                    t->setTextColor(c565(210, 255, 210), 0x0000);
                    t->setCursor(c * 6, hy * 8);
                    t->print((char)(0x21 + random(0, 93)));
                }
                if (hy - 1 >= 0 && hy - 1 < BIG_ROWS) {
                    t->setTextColor(c565(0, 230, 50), 0x0000);
                    t->setCursor(c * 6, (hy - 1) * 8);
                    t->print((char)(0x21 + random(0, 93)));
                }
                if (hy - 4 >= 0 && hy - 4 < BIG_ROWS && (random(0, 3) == 0)) {
                    t->setTextColor(c565(0, 140, 20), 0x0000);
                    t->setCursor(c * 6, (hy - 4) * 8);
                    t->print((char)(0x21 + random(0, 93)));
                }
                int tail = hy - TRAIL;
                if (tail >= 0 && tail < BIG_ROWS)
                    t->fillRect(c * 6, tail * 8, 6, 8, 0x0000);
                headBig[d][c]++;
                if (headBig[d][c] >= BIG_ROWS + TRAIL)
                    headBig[d][c] = -(int16_t)random(2, BIG_ROWS / 2);
            }
            dispCS(d + 1, HIGH);
        }
        // --- Kleines Display ---
        dispCS(0, LOW);
        for (int c = 0; c < SML_COLS; c++) {
            int hy = headSml[c];
            if (hy >= 0 && hy < SML_ROWS) {
                tft.setTextColor(c565(210, 255, 210), 0x0000);
                tft.setCursor(c * 6, hy * 8);
                tft.print((char)(0x21 + random(0, 93)));
            }
            if (hy - 1 >= 0 && hy - 1 < SML_ROWS) {
                tft.setTextColor(c565(0, 230, 50), 0x0000);
                tft.setCursor(c * 6, (hy - 1) * 8);
                tft.print((char)(0x21 + random(0, 93)));
            }
            if (hy - 3 >= 0 && hy - 3 < SML_ROWS && (random(0, 3) == 0)) {
                tft.setTextColor(c565(0, 140, 20), 0x0000);
                tft.setCursor(c * 6, (hy - 3) * 8);
                tft.print((char)(0x21 + random(0, 93)));
            }
            int tail = hy - SML_TRAIL;
            if (tail >= 0 && tail < SML_ROWS)
                tft.fillRect(c * 6, tail * 8, 6, 8, 0x0000);
            headSml[c]++;
            if (headSml[c] >= SML_ROWS + SML_TRAIL)
                headSml[c] = -(int16_t)random(2, SML_ROWS / 2);
        }
        dispCS(0, HIGH);

        // Mutex freigeben während vTaskDelay damit andere Tasks (SD/WebServer) SPI nutzen können
        xSemaphoreGiveRecursive(xSpiMutex);
        vTaskDelay(pdMS_TO_TICKS(60));
        xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);
    }

    // Aufräumen
    for (int d = 0; d < 4; d++) {
        dispCS(d + 1, LOW);
        bigDisp(d)->fillScreen(0x0000);
        dispCS(d + 1, HIGH);
    }
    dispCS(0, LOW);
    tft.fillScreen(0x0000);
    dispCS(0, HIGH);
}

// ═══════════════════════════════════════════════════════════════════════════
// Animation 2: Nixie Slot-Machine + Boot-Status  (alle 5 Displays)
// ═══════════════════════════════════════════════════════════════════════════
static void animSlot(uint32_t durationMs)
{
    // Alle 5 Displays schwarz
    dispCS(0, LOW); tft.fillScreen(0x0000); dispCS(0, HIGH);
    for (int d = 0; d < 4; d++) {
        dispCS(d + 1, LOW); bigDisp(d)->fillScreen(0x0000); dispCS(d + 1, HIGH);
    }

    uint8_t  digit[4]  = {0, 0, 0, 0};
    uint32_t lastDraw  = 0;
    uint32_t lastSmall = 0;
    uint32_t start     = millis();

    // Hintergrund kleines Display: Logo
    dispCS(0, LOW);
    tft.setFont(NULL);
    tft.setTextSize(2);
    tft.setTextColor(c565(0, 180, 255));
    tft.setCursor(16, 6); tft.print("NIXIE");
    tft.setCursor(16, 28); tft.print("CLOCK");
    tft.drawFastHLine(8, 52, 144, c565(40, 60, 80));
    dispCS(0, HIGH);

    while (millis() - start < durationMs && (millis() - start < 10000 || !bootInitDone)) {
        uint32_t now     = millis();
        uint32_t elapsed = now - start;

        uint32_t frameMs;
        if      (elapsed < durationMs * 50 / 100) frameMs = 80;
        else if (elapsed < durationMs * 75 / 100) frameMs = 150;
        else                                       frameMs = 300;

        // Große Displays: Ziffern rollen
        if (now - lastDraw >= frameMs) {
            lastDraw = now;
            for (int d = 0; d < 4; d++) {
                digit[d] = (digit[d] + 1) % 10;
                dispCS(d + 1, LOW);
                bigDisp(d)->fillRect(0, 0, 135, 155, 0x0000);
                draw7Number(digit[d], 12, 10, 10,
                            c565(255, 130, 0), 0x0000, 1, bigDisp(d));
                dispCS(d + 1, HIGH);
            }
        }

        // Kleines Display: Status
        if (now - lastSmall >= 400) {
            lastSmall = now;
            dispCS(0, LOW);
            tft.setTextSize(1);
            // WiFi
            tft.setCursor(8, 60); tft.setTextColor(c565(150, 150, 150)); tft.print("WiFi: ");
            if (systemConfig.wifiConnected) { tft.setTextColor(c565(0, 255, 80));  tft.print("OK     "); }
            else                            { tft.setTextColor(c565(255, 120, 0)); tft.print("...    "); }
            // SD-Karte
            tft.setCursor(8, 74); tft.setTextColor(c565(150, 150, 150)); tft.print("SD:   ");
            if (systemConfig.sdCardDetected) { tft.setTextColor(c565(0, 255, 80));  tft.print("OK     "); }
            else                             { tft.setTextColor(c565(255, 80, 80)); tft.print("Fehlt  "); }
            // Fortschrittsbalken
            uint16_t barW = (uint16_t)((uint32_t)(now - start) * 144 / durationMs);
            tft.fillRect(8,  90, 144, 5, c565(30, 30, 30));
            tft.fillRect(8,  90, barW, 5, c565(0, 180, 255));
            dispCS(0, HIGH);
        }

        // Mutex freigeben während vTaskDelay damit andere Tasks (SD/WebServer) SPI nutzen können
        xSemaphoreGiveRecursive(xSpiMutex);
        vTaskDelay(pdMS_TO_TICKS(20));
        xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);
    }

    // Aufräumen
    for (int d = 0; d < 4; d++) {
        dispCS(d + 1, LOW); bigDisp(d)->fillScreen(0x0000); dispCS(d + 1, HIGH);
    }
    dispCS(0, LOW); tft.fillScreen(0x0000); dispCS(0, HIGH);
}

// ═══════════════════════════════════════════════════════════════════════════
// Animation 3: Regenbogen-Farbwellen  (alle 5 Displays)
// ═══════════════════════════════════════════════════════════════════════════
static void animColorWave(uint32_t durationMs)
{
    const int BAND_H   = 12;
    const int BIG_BANDS = (240 + BAND_H - 1) / BAND_H;   // 20
    const int SML_BANDS = (128 + BAND_H - 1) / BAND_H;   // 11
    const int BIG_W    = 135;
    const int SML_W    = 160;

    uint8_t  baseHue = 0;
    uint32_t start   = millis();

    // Init
    for (int d = 0; d < 4; d++) {
        dispCS(d + 1, LOW); bigDisp(d)->fillScreen(0x0000); dispCS(d + 1, HIGH);
    }
    dispCS(0, LOW); tft.fillScreen(0x0000); dispCS(0, HIGH);

    while (millis() - start < durationMs && (millis() - start < 10000 || !bootInitDone)) {
        // Große Displays
        for (int d = 0; d < 4; d++) {
            Adafruit_ST7789* t = bigDisp(d);
            dispCS(d + 1, LOW);
            for (int b = 0; b < BIG_BANDS; b++) {
                uint8_t hue = baseHue + (uint8_t)(d * 50) + (uint8_t)(b * 12);
                t->fillRect(0, b * BAND_H, BIG_W, BAND_H, hsvColor(hue));
            }
            dispCS(d + 1, HIGH);
        }
        // Kleines Display – eigener Farbversatz
        dispCS(0, LOW);
        for (int b = 0; b < SML_BANDS; b++) {
            uint8_t hue = baseHue + (uint8_t)(200) + (uint8_t)(b * 22);
            tft.fillRect(0, b * BAND_H, SML_W, BAND_H, hsvColor(hue));
        }
        dispCS(0, HIGH);

        baseHue += 6;
        // Mutex freigeben während vTaskDelay damit andere Tasks (SD/WebServer) SPI nutzen können
        xSemaphoreGiveRecursive(xSpiMutex);
        vTaskDelay(pdMS_TO_TICKS(80));
        xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);
    }

    // Fade-Out
    for (int step = 8; step >= 0; step--) {
        for (int d = 0; d < 4; d++) {
            Adafruit_ST7789* t = bigDisp(d);
            dispCS(d + 1, LOW);
            for (int b = 0; b < BIG_BANDS; b++) {
                uint8_t hue = baseHue + (uint8_t)(d * 50) + (uint8_t)(b * 12);
                uint16_t col = hsvColor(hue);
                uint8_t r  = ((col >> 11) & 0x1F) * step / 8u;
                uint8_t g  = ((col >> 5)  & 0x3F) * step / 8u;
                uint8_t bl = (col & 0x1F)          * step / 8u;
                t->fillRect(0, b * BAND_H, BIG_W, BAND_H,
                            ((uint16_t)r << 11) | ((uint16_t)g << 5) | bl);
            }
            dispCS(d + 1, HIGH);
        }
        dispCS(0, LOW);
        for (int b = 0; b < SML_BANDS; b++) {
            uint8_t hue = baseHue + (uint8_t)(200) + (uint8_t)(b * 22);
            uint16_t col = hsvColor(hue);
            uint8_t r  = ((col >> 11) & 0x1F) * step / 8u;
            uint8_t g  = ((col >> 5)  & 0x3F) * step / 8u;
            uint8_t bl = (col & 0x1F)          * step / 8u;
            tft.fillRect(0, b * BAND_H, SML_W, BAND_H,
                         ((uint16_t)r << 11) | ((uint16_t)g << 5) | bl);
        }
        dispCS(0, HIGH);
        baseHue += 3;
        // Mutex freigeben während Fade-Out-Pause damit WebServer SD nutzen kann
        xSemaphoreGiveRecursive(xSpiMutex);
        vTaskDelay(pdMS_TO_TICKS(60));
        xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);
    }

    for (int d = 0; d < 4; d++) {
        dispCS(d + 1, LOW); bigDisp(d)->fillScreen(0x0000); dispCS(d + 1, HIGH);
    }
    dispCS(0, LOW); tft.fillScreen(0x0000); dispCS(0, HIGH);
}

// ═══════════════════════════════════════════════════════════════════════════
// Öffentlicher Einstiegspunkt
// ═══════════════════════════════════════════════════════════════════════════
void runBootAnimation(uint8_t mode, uint32_t durationMs)
{
    if (!xSpiMutex) return;
    xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);
    switch (mode) {
        case ANIM_MATRIX:    animMatrix(durationMs);    break;
        case ANIM_SLOT:      animSlot(durationMs);      break;
        case ANIM_COLORWAVE: animColorWave(durationMs); break;
        default: break;
    }
    xSemaphoreGiveRecursive(xSpiMutex);
}
