#include "vTask_weather.h"
#include <common.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "weather_icon_set.h"

// Background image buffer defined in main.cpp
extern uint16_t* bgImageData;
extern uint16_t  bgImageW;
extern uint16_t  bgImageH;

// Draw RGB565 bitmap with color 0x0000 treated as transparent
static void drawTransparentBitmap(Adafruit_ST7735 &tft, int16_t x, int16_t y,
                                  const uint16_t *bitmap, int16_t w, int16_t h) {
    for (int16_t row = 0; row < h; row++) {
        for (int16_t col = 0; col < w; col++) {
            uint16_t pixel = pgm_read_word(&bitmap[row * w + col]);
            if (pixel != 0x0000) {
                tft.drawPixel(x + col, y + row, pixel);
            }
        }
    }
}

// WMO Weather Code to icon mapping
static const uint16_t* getWeatherIcon(uint8_t code) {
    if (code == 0)                          return clear;
    if (code <= 3)                          return cloudy;
    if (code == 45 || code == 48)           return fog;
    if (code >= 51 && code <= 57)           return chancerain;
    if (code >= 61 && code <= 67)           return chancerain;
    if (code >= 71 && code <= 77)           return chancesnow;
    if (code >= 80 && code <= 82)           return chancerain;
    if (code >= 85 && code <= 86)           return chancesnow;
    if (code >= 95)                         return chancetstorms;
    return cloudy;
}

static const char* getWeatherText(uint8_t code) {
    if (code == 0)                          return "Klar";
    if (code == 1)                          return "Heiter";
    if (code == 2)                          return "Bewolkt";
    if (code == 3)                          return "Bedeckt";
    if (code == 45 || code == 48)           return "Nebel";
    if (code >= 51 && code <= 55)           return "Nieselregen";
    if (code >= 56 && code <= 57)           return "Gefrierender Niesel";
    if (code >= 61 && code <= 65)           return "Regen";
    if (code >= 66 && code <= 67)           return "Gefrierender Regen";
    if (code >= 71 && code <= 75)           return "Schnee";
    if (code == 77)                         return "Schneegriesel";
    if (code >= 80 && code <= 82)           return "Regenschauer";
    if (code >= 85 && code <= 86)           return "Schneeschauer";
    if (code >= 95)                         return "Gewitter";
    return "Unbekannt";
}

void fetchWeather() {
    if (!systemConfig.weatherEnabled) return;
    if (WiFi.status() != WL_CONNECTED) return;

    // Rate limit: only fetch every 15 minutes
    if (weatherData.valid && (millis() - weatherData.lastFetchMs < 900000UL)) return;

    char url[192];
    snprintf(url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code",
        systemConfig.weatherLat, systemConfig.weatherLon);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            weatherData.temperature = doc["current"]["temperature_2m"] | 0.0f;
            weatherData.weatherCode = doc["current"]["weather_code"] | 0;
            weatherData.lastFetchMs = millis();
            weatherData.valid = true;
            Serial.printf("[WEATHER] %.1f°C, code=%u (%s)\n",
                weatherData.temperature, weatherData.weatherCode,
                getWeatherText(weatherData.weatherCode));
        } else {
            Serial.printf("[WEATHER] JSON parse error: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[WEATHER] HTTP error: %d\n", httpCode);
    }
    http.end();
}

void drawWeatherView(Adafruit_ST7735 &tft) {
    if (!weatherData.valid) return;

    // Colour palette – adapts to background brightness set by bgTextDark
    uint16_t colPrimary = systemConfig.bgTextDark ? (uint16_t)ST77XX_BLACK : (uint16_t)ST77XX_WHITE;
    uint16_t colSecond  = systemConfig.bgTextDark ? (uint16_t)0x4208       : (uint16_t)0x7BEF;  // medium grey
    uint16_t colAccent  = systemConfig.bgTextDark ? (uint16_t)0x0010       : (uint16_t)0x07FF;  // cyan
    uint16_t colMuted   = systemConfig.bgTextDark ? (uint16_t)0x8410       : (uint16_t)0x4208;  // dark grey

    // --- Background: SD image or dark-blue gradient ---
    if (bgImageData && bgImageW > 0 && bgImageH > 0) {
        tft.drawRGBBitmap(0, 0, (uint16_t*)bgImageData, (int16_t)bgImageW, (int16_t)bgImageH);
        if (bgImageH < 128)
            tft.fillRect(0, bgImageH, 160, 128 - bgImageH, ST77XX_BLACK);
    } else {
        for (int y = 0; y < 128; y++) {
            // Gradient from dark navy (top) to slightly lighter blue (bottom)
            uint8_t r = 5 + y / 16;
            uint8_t g = 10 + y / 8;
            uint8_t b = 30 + y / 4;
            uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            tft.drawFastHLine(0, y, 160, color);
        }
    }

    // --- Accent bar at top ---
    tft.fillRect(0, 0, 160, 3, 0x04FF); // cyan accent line

    // --- City name centered at top ---
    tft.setTextSize(1);
    tft.setTextColor(colPrimary);
    int16_t x1, y1;
    uint16_t tw, th;
    tft.getTextBounds(systemConfig.weatherCity, 0, 0, &x1, &y1, &tw, &th);
    tft.setCursor((160 - tw) / 2, 8);
    tft.print(systemConfig.weatherCity);

    // --- Weather icon (32x32) ---
    const uint16_t* icon = getWeatherIcon(weatherData.weatherCode);
    drawTransparentBitmap(tft, 12, 28, icon, 32, 32);

    // --- Temperature large ---
    tft.setTextSize(3);
    tft.setTextColor(colPrimary);
    char tempStr[10];
    int tempInt = (int)(weatherData.temperature + 0.5f);
    if (weatherData.temperature < 0 && tempInt == 0) tempInt = 0; // avoid "-0"
    snprintf(tempStr, sizeof(tempStr), "%d", tempInt);
    tft.setCursor(54, 30);
    tft.print(tempStr);

    // Degree symbol + C in smaller size next to temp
    int curX = tft.getCursorX();
    tft.setTextSize(2);
    tft.setCursor(curX + 2, 30);
    tft.print((char)247);
    tft.print("C");

    // --- Weather description ---
    tft.setTextSize(1);
    tft.setTextColor(colAccent);
    const char* desc = getWeatherText(weatherData.weatherCode);
    tft.getTextBounds(desc, 0, 0, &x1, &y1, &tw, &th);
    tft.setCursor((160 - tw) / 2, 68);
    tft.print(desc);

    // --- Separator line ---
    tft.drawFastHLine(20, 82, 120, 0x2124); // subtle dark gray line

    // --- Min/Max or feels like section (placeholder: show "Aktuell") ---
    tft.setTextSize(1);
    tft.setTextColor(colSecond);
    tft.setCursor(12, 90);
    tft.print("Aktuell");
    tft.setTextColor(colPrimary);
    tft.setCursor(70, 90);
    char tempLine[16];
    snprintf(tempLine, sizeof(tempLine), "%d%cC", tempInt, (char)247);
    tft.print(tempLine);

    // --- Last update time at bottom ---
    tft.setTextColor(colMuted);
    tft.setCursor(12, 115);
    unsigned long ago = (millis() - weatherData.lastFetchMs) / 60000UL;
    if (ago < 1)
        tft.print("Gerade aktualisiert");
    else {
        char ageStr[24];
        snprintf(ageStr, sizeof(ageStr), "Vor %lu Min.", ago);
        tft.print(ageStr);
    }

    // --- Bottom accent bar ---
    tft.fillRect(0, 125, 160, 3, 0x04FF);
}
