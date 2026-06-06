
#include <stdint.h>
#include <Arduino.h>
#include "vTask_WebServer.h"
#include <common.h>

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Esp.h>
#include <SDFat.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "pca9685.h"
#include "WebPage.h"
#include "credentials.h"

extern SdCacheEntry sdFileCache[];
extern int sdFileCacheCount;
extern SdFs sdCard;
extern SoftSpiDriver<TFT_MISO, TFT_MOSI, TFT_CLK> softSpi;
extern SemaphoreHandle_t xSpiMutex;
extern volatile bool bootInitDone;
extern void scanSdCache(const char* dirPath);   // defined in main.cpp
extern weatherDataStruct weatherData;

char XML[4096];
char buf[64];
Preferences prefs;

IPAddress Actual_IP;
IPAddress PageIP(192, 168, 178, 137);
IPAddress gateway(192, 168, 178, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip;
WebServer server(80);

void UpdateSlider();
void ProcessButton_0();
void SendXML();
void SendWebsite();
void HandleSetNTP();
void HandleSetRGB();
void HandleSetDisplay();
void HandleSetWeather();
void HandleSdList();
void HandleSdFile();
void HandleSdDelete();
void HandleSdUpload();
void HandleSdUploadFile();
void HandleSetAnim();
void HandleSdRescan();
void applyNTPConfig();
bool saveConfigToSD();  // returns true on success

void tCodeServerTask(void *pvParameters)
{
    Serial.print("Task WebServer running on core ");
    Serial.println(xPortGetCoreID());

    const uint32_t connectTimeoutMs = 15000; // 15 Sekunden Timeout pro Verbindungsversuch

    auto connectWiFi = [&]() {
        Serial.print("Connecting to: ");
        Serial.println(AP_SSID);
        WiFi.config(ip, gateway, subnet);
        WiFi.setHostname("NixieClock"); // sets DHCP hostname
        WiFi.begin(AP_SSID, AP_PASS);

        uint32_t startMs = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - startMs >= connectTimeoutMs)
            {
                Serial.println("\nWiFi Timeout - neuer Versuch...");
                WiFi.disconnect();
                vTaskDelay(pdMS_TO_TICKS(1000));
                WiFi.begin(AP_SSID, AP_PASS);
                startMs = millis();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
        }

        systemConfig.wifiConnected = true;
        systemConfig.ip = WiFi.localIP();
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.print("IP-Address of NixieClock: ");
        Serial.println(WiFi.localIP());
    };

    // Load persistent settings from NVS and apply
    // Open read-write (false) so the namespace is created on first boot
    prefs.begin("nixie", false);
    if (prefs.isKey("ntp")) {
        String ntpStr = prefs.getString("ntp");
        strncpy(systemConfig.ntpServer, ntpStr.c_str(), 63);
        systemConfig.ntpServer[63] = '\0';
    }
    if (prefs.isKey("utc"))      systemConfig.utcOffset          = (int8_t)prefs.getInt("utc");
    if (prefs.isKey("rgb_mode")) rgbConfig.rgbMode               = (uint16_t)prefs.getUInt("rgb_mode");
    if (prefs.isKey("rgb_r"))    rgbConfig.rgbR                  = prefs.getUChar("rgb_r");
    if (prefs.isKey("rgb_g"))    rgbConfig.rgbG                  = prefs.getUChar("rgb_g");
    if (prefs.isKey("rgb_b"))    rgbConfig.rgbB                  = prefs.getUChar("rgb_b");
    if (prefs.isKey("rgb_br"))   rgbConfig.rgbBrigthness         = prefs.getUChar("rgb_br");
    if (prefs.isKey("dp_int"))   systemConfig.displayIntervalSec = prefs.getUShort("dp_int");
    if (prefs.isKey("dp_br"))    systemConfig.displayBrightness  = prefs.getUShort("dp_br");
    if (prefs.isKey("dp_slide")) systemConfig.slideshowEnabled   = prefs.getUChar("dp_slide") != 0;
    if (prefs.isKey("dp_cal"))   systemConfig.calendarEnabled    = prefs.getUChar("dp_cal")   != 0;
    if (prefs.isKey("dp_bg"))    systemConfig.backgroundEnabled  = prefs.getUChar("dp_bg")    != 0;
    if (prefs.isKey("anim_m"))   systemConfig.bootAnimMode       = prefs.getUChar("anim_m");
    if (prefs.isKey("wt_en"))    systemConfig.weatherEnabled     = prefs.getUChar("wt_en") != 0;
    if (prefs.isKey("wt_city")) { String _c = prefs.getString("wt_city"); strncpy(systemConfig.weatherCity, _c.c_str(), 31); systemConfig.weatherCity[31] = '\0'; }
    if (prefs.isKey("wt_lat"))   systemConfig.weatherLat         = prefs.getFloat("wt_lat");
    if (prefs.isKey("wt_lon"))   systemConfig.weatherLon         = prefs.getFloat("wt_lon");
    if (prefs.isKey("cl_fg_r"))  systemConfig.clockFgR           = prefs.getUChar("cl_fg_r");
    if (prefs.isKey("cl_fg_g"))  systemConfig.clockFgG           = prefs.getUChar("cl_fg_g");
    if (prefs.isKey("cl_fg_b"))  systemConfig.clockFgB           = prefs.getUChar("cl_fg_b");
    if (prefs.isKey("cl_bg_r"))  systemConfig.clockBgR           = prefs.getUChar("cl_bg_r");
    if (prefs.isKey("cl_bg_g"))  systemConfig.clockBgG           = prefs.getUChar("cl_bg_g");
    if (prefs.isKey("cl_bg_b"))  systemConfig.clockBgB           = prefs.getUChar("cl_bg_b");
    prefs.end();
    Serial.println("--> Prefs loaded");

    // Apply persisted brightness to PCA9685 immediately after prefs are loaded
    Serial.printf("[DISPLAY] restoring brightness from NVS: %u\n", systemConfig.displayBrightness);
    pca9685_setAllChannels(systemConfig.displayBrightness);

    // Push persisted RGB config into the queue so the RGB task starts with correct values
    xQueueSend(xQueue_RGB_Config, &rgbConfig, pdMS_TO_TICKS(200));

    connectWiFi();

    // Apply NTP config after WiFi is connected (needs DNS)
    applyNTPConfig();
    Serial.println("--> NTP configured");
    bootInitDone = true;  // Animation kann jetzt enden

    server.on("/", SendWebsite);
    server.on("/xml", SendXML);
    server.on("/UPDATE_SLIDER", UpdateSlider);
    server.on("/BUTTON_0", ProcessButton_0);
    server.on("/SET_NTP", HandleSetNTP);
    server.on("/SET_RGB", HandleSetRGB);
    server.on("/SET_DISPLAY", HandleSetDisplay);
    server.on("/SD_LIST", HandleSdList);
    server.on("/SD_RESCAN", HandleSdRescan);
    server.on("/SD_FILE", HandleSdFile);
    server.on("/SD_DELETE", HandleSdDelete);
    server.on("/SD_UPLOAD", HTTP_POST, HandleSdUpload, HandleSdUploadFile);
    server.on("/SET_ANIM", HandleSetAnim);
    server.on("/SET_WEATHER", HandleSetWeather);
    server.begin();
    Serial.print("--> Init WebServer OK: "); Serial.println(millis());

    bool pendingSettingsSave = true;  // write settings.json on the first loop iteration

    for (;;)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            systemConfig.wifiConnected = false;
            Serial.println("WiFi Verbindung verloren - reconnect...");
            WiFi.disconnect();
            vTaskDelay(pdMS_TO_TICKS(1000));
            connectWiFi();
        }

        server.handleClient();

        // Deferred first-boot settings write: runs after the display task has had
        // time to release the SPI bus (typically within the first few loop ticks).
        if (pendingSettingsSave && systemConfig.sdCardDetected) {
            if (saveConfigToSD()) pendingSettingsSave = false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int brightness = 0;
bool LED0 = false, SomeOutput = false;

void UpdateSlider()
{
    String t_state = server.arg("VALUE");
    brightness = t_state.toInt();
    
    // Map brightness to LED brightness
    int ledBrightness = map(brightness, 0, 255, 0, 255);
    
    rgbConfig.rgbBrigthness = ledBrightness;

    // Respond with the updated RPM value
    server.send(200, "text/plain", String(brightness));
    xQueueSend(xQueue_RGB_Config, &rgbConfig, 10);  // Send queue
}

void ProcessButton_0()
{
    LED0 = !LED0;
    //digitalWrite(PIN_LED, LED0);
    rgbConfig.rgbSwitch = LED0;
    server.send(200, "text/plain", "");
    xQueueSend(xQueue_RGB_Config, &rgbConfig, 10);  // Send queue
}

void SendWebsite()
{
    server.setContentLength(strlen_P(PAGE_MAIN));
    server.send_P(200, PSTR("text/html"), PAGE_MAIN);
}

void applyNTPConfig()
{
    char tzStr[64];
    switch (systemConfig.utcOffset) {
        case 1:  strcpy(tzStr, "CET-1CEST,M3.5.0,M10.5.0/3");   break;
        case 2:  strcpy(tzStr, "EET-2EEST,M3.5.0/3,M10.5.0/4"); break;
        default:
            if (systemConfig.utcOffset >= 0)
                snprintf(tzStr, sizeof(tzStr), "UTC-%d", systemConfig.utcOffset);
            else
                snprintf(tzStr, sizeof(tzStr), "UTC+%d", -systemConfig.utcOffset);
            break;
    }
    configTzTime(tzStr, systemConfig.ntpServer);
}

void SendXML()
{
    char ipStr[24];
    WiFi.localIP().toString().toCharArray(ipStr, sizeof(ipStr));

    snprintf(XML, sizeof(XML),
        "<?xml version='1.0'?><Data>"
        "<HEAP>%u</HEAP>"
        "<RSSI>%d</RSSI>"
        "<UPTIME>%lu</UPTIME>"
        "<SD>%d</SD>"
        "<IP>%s</IP>"
        "<NTP>%s</NTP>"
        "<UTC>%d</UTC>"
        "<VER>%s</VER>"
        "<BUILD>%s %s</BUILD>"
        "<CPU>%u</CPU>"
        "<CHIP>%s</CHIP>"
        "<RGBM>%u</RGBM>"
        "<RGBR>%u</RGBR>"
        "<RGBG>%u</RGBG>"
        "<RGBB>%u</RGBB>"
        "<RGBBR>%u</RGBBR>"
        "<DPINT>%u</DPINT>"
        "<DPBR>%u</DPBR>"
        "<SLIDE>%u</SLIDE>"
        "<CAL>%u</CAL>"
        "<BG>%u</BG>"
        "<ANIMM>%u</ANIMM>"
        "<WEN>%u</WEN>"
        "<WCITY>%s</WCITY>"
        "<WLAT>%.4f</WLAT>"
        "<WLON>%.4f</WLON>"
        "</Data>",
        (unsigned)ESP.getFreeHeap(),
        WiFi.RSSI(),
        millis() / 1000UL,
        (int)systemConfig.sdCardDetected,
        ipStr,
        systemConfig.ntpServer,
        (int)systemConfig.utcOffset,
        FW_VERSION,
        FW_BUILD_DATE, FW_BUILD_TIME,
        (unsigned)ESP.getCpuFreqMHz(),
        ESP.getChipModel(),
        (unsigned)rgbConfig.rgbMode,
        (unsigned)rgbConfig.rgbR,
        (unsigned)rgbConfig.rgbG,
        (unsigned)rgbConfig.rgbB,
        (unsigned)rgbConfig.rgbBrigthness,
        (unsigned)systemConfig.displayIntervalSec,
        (unsigned)systemConfig.displayBrightness,
        (unsigned)systemConfig.slideshowEnabled,
        (unsigned)systemConfig.calendarEnabled,
        (unsigned)systemConfig.backgroundEnabled,
        (unsigned)systemConfig.bootAnimMode,
        (unsigned)systemConfig.weatherEnabled,
        systemConfig.weatherCity,
        systemConfig.weatherLat,
        systemConfig.weatherLon
    );
    server.send(200, "text/xml", XML);
}

void HandleSetNTP()
{
    if (server.hasArg("SERVER")) {
        String srv = server.arg("SERVER");
        strncpy(systemConfig.ntpServer, srv.c_str(), 63);
        systemConfig.ntpServer[63] = '\0';
    }
    if (server.hasArg("UTC")) {
        systemConfig.utcOffset = (int8_t)server.arg("UTC").toInt();
    }
    applyNTPConfig();
    prefs.begin("nixie", false);
    prefs.putString("ntp", systemConfig.ntpServer);
    prefs.putInt("utc", systemConfig.utcOffset);
    prefs.end();
    saveConfigToSD();
    server.send(200, "text/plain", "OK");
}

void HandleSetRGB()
{
    rgbConfig.rgbMode       = (uint16_t)server.arg("MODE").toInt();
    rgbConfig.rgbR          = (uint8_t)server.arg("R").toInt();
    rgbConfig.rgbG          = (uint8_t)server.arg("G").toInt();
    rgbConfig.rgbB          = (uint8_t)server.arg("B").toInt();
    rgbConfig.rgbBrigthness = (uint8_t)server.arg("BRIGHT").toInt();
    rgbConfig.rgbSwitch     = (rgbConfig.rgbMode != 0);
    xQueueSend(xQueue_RGB_Config, &rgbConfig, 10);
    prefs.begin("nixie", false);
    prefs.putUInt("rgb_mode", rgbConfig.rgbMode);
    prefs.putUChar("rgb_r",   rgbConfig.rgbR);
    prefs.putUChar("rgb_g",   rgbConfig.rgbG);
    prefs.putUChar("rgb_b",   rgbConfig.rgbB);
    prefs.putUChar("rgb_br",  rgbConfig.rgbBrigthness);
    prefs.end();
    saveConfigToSD();
    server.send(200, "text/plain", "OK");
}

void HandleSetWeather()
{
    if (server.hasArg("EN"))   systemConfig.weatherEnabled = server.arg("EN").toInt() != 0;
    if (server.hasArg("CITY")) { String _c = server.arg("CITY"); strncpy(systemConfig.weatherCity, _c.c_str(), 31); systemConfig.weatherCity[31] = '\0'; }
    if (server.hasArg("LAT"))  systemConfig.weatherLat = server.arg("LAT").toFloat();
    if (server.hasArg("LON"))  systemConfig.weatherLon = server.arg("LON").toFloat();
    // Reset cache so next fetch uses new settings
    weatherData.valid = false;
    weatherData.lastFetchMs = 0;
    prefs.begin("nixie", false);
    prefs.putUChar("wt_en",   systemConfig.weatherEnabled ? 1 : 0);
    prefs.putString("wt_city", systemConfig.weatherCity);
    prefs.putFloat("wt_lat",  systemConfig.weatherLat);
    prefs.putFloat("wt_lon",  systemConfig.weatherLon);
    prefs.end();
    saveConfigToSD();
    server.send(200, "text/plain", "OK");
}

void HandleSetDisplay()
{
    if (server.hasArg("INTERVAL")) systemConfig.displayIntervalSec = (uint16_t)server.arg("INTERVAL").toInt();
    if (server.hasArg("BRIGHT"))   systemConfig.displayBrightness  = (uint16_t)server.arg("BRIGHT").toInt();
    if (server.hasArg("SLIDE"))    systemConfig.slideshowEnabled   = server.arg("SLIDE").toInt() != 0;
    if (server.hasArg("CAL"))      systemConfig.calendarEnabled    = server.arg("CAL").toInt()   != 0;
    if (server.hasArg("BG"))       systemConfig.backgroundEnabled  = server.arg("BG").toInt()    != 0;

    // Clock foreground/background colour settings
    if (server.hasArg("CFR")) systemConfig.clockFgR = (uint8_t)server.arg("CFR").toInt();
    if (server.hasArg("CFG")) systemConfig.clockFgG = (uint8_t)server.arg("CFG").toInt();
    if (server.hasArg("CFB")) systemConfig.clockFgB = (uint8_t)server.arg("CFB").toInt();
    if (server.hasArg("CBR")) systemConfig.clockBgR = (uint8_t)server.arg("CBR").toInt();
    if (server.hasArg("CBG")) systemConfig.clockBgG = (uint8_t)server.arg("CBG").toInt();
    if (server.hasArg("CBB")) systemConfig.clockBgB = (uint8_t)server.arg("CBB").toInt();

    // Apply brightness to hardware immediately
    if (server.hasArg("BRIGHT")) {
        pca9685_setAllChannels(systemConfig.displayBrightness);
    }

    prefs.begin("nixie", false);
    prefs.putUShort("dp_int",   systemConfig.displayIntervalSec);
    prefs.putUShort("dp_br",    systemConfig.displayBrightness);
    prefs.putUChar("dp_slide",  systemConfig.slideshowEnabled  ? 1 : 0);
    prefs.putUChar("dp_cal",    systemConfig.calendarEnabled   ? 1 : 0);
    prefs.putUChar("dp_bg",     systemConfig.backgroundEnabled ? 1 : 0);
    prefs.putUChar("cl_fg_r",   systemConfig.clockFgR);
    prefs.putUChar("cl_fg_g",   systemConfig.clockFgG);
    prefs.putUChar("cl_fg_b",   systemConfig.clockFgB);
    prefs.putUChar("cl_bg_r",   systemConfig.clockBgR);
    prefs.putUChar("cl_bg_g",   systemConfig.clockBgG);
    prefs.putUChar("cl_bg_b",   systemConfig.clockBgB);
    prefs.end();
    saveConfigToSD();
    server.send(200, "text/plain", "OK");
}

// ---- SD card browser ----

static void escapeJson(const char* src, String& out)
{
    while (*src) {
        char c = *src++;
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
}

void HandleSdList()
{
    String path = server.hasArg("path") ? server.arg("path") : "/";
    if (path.length() == 0) path = "/";
    // Normalize: remove trailing slash except root
    while (path.length() > 1 && path.endsWith("/")) path.remove(path.length() - 1);

    if (!systemConfig.sdCardDetected || sdFileCacheCount == 0) {
        server.send(503, "application/json", "[]");
        return;
    }

    // Serve listing from RAM cache – no SPI access required
    String json = "[";
    bool first = true;
    int count = 0;

    for (int i = 0; i < sdFileCacheCount; i++) {
        const SdCacheEntry& e = sdFileCache[i];

        // Determine parent path of this entry
        const char* p = strrchr(e.path, '/');
        String parent = (p && p != e.path) ? String(e.path).substring(0, p - e.path) : "/";

        if (parent != path) continue;

        if (!first) json += ',';
        first = false;
        count++;

        json += "{\"name\":\"";
        escapeJson(e.name, json);
        json += "\",\"path\":\"";
        escapeJson(e.path, json);
        json += "\",\"type\":\"";
        json += e.isDir ? "dir" : "file";
        json += '"';
        if (!e.isDir) {
            json += ",\"size\":";
            json += String(e.size);
        }
        json += '}';
    }
    json += ']';
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

void HandleSdFile()
{
    if (!systemConfig.sdCardDetected || !server.hasArg("path")) {
        server.send(404, "text/plain", "Not found");
        return;
    }

    String path = server.arg("path");

    String ct = "application/octet-stream";
    String lp = path; lp.toLowerCase();
    if      (lp.endsWith(".jpg") || lp.endsWith(".jpeg")) ct = "image/jpeg";
    else if (lp.endsWith(".png"))  ct = "image/png";
    else if (lp.endsWith(".bmp"))  ct = "image/bmp";
    else if (lp.endsWith(".gif"))  ct = "image/gif";
    else if (lp.endsWith(".txt"))  ct = "text/plain";

    // sdCard was initialized before display-init and kept valid via SHARED_SPI.
    // Protect with mutex so display task doesn't use SPI concurrently.
    if (xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY) != pdTRUE) {
        server.send(503, "text/plain", "Busy");
        return;
    }

    // Deselect ALL display CS pins explicitly before SD access
    digitalWrite(TFT_CS_0, HIGH);
    digitalWrite(TFT_CS_1, HIGH);
    digitalWrite(TFT_CS_2, HIGH);
    digitalWrite(TFT_CS_3, HIGH);
    digitalWrite(TFT_CS_4, HIGH);

    // SPI-Peripherie freigeben damit SoftSpiDriver GPIO-Zugriff hat
    SPI.end();

    // Buffer aktivieren fuer SD-Zugriff
    digitalWrite(SD_BUF_OE, LOW);
    delayMicroseconds(1);

    FsFile f = sdCard.open(path.c_str(), O_RDONLY);

    // If open fails, aggressive reinit: full SPI reset + slow clock + long delays
    if (!f) {
        Serial.println("[SD_FILE] open failed, aggressive reinit...");

        // 1) Kill SdFat state completely
        sdCard.end();

        // 2) Set pins as GPIO
        pinMode(TFT_CLK, OUTPUT);
        pinMode(TFT_MOSI, OUTPUT);
        pinMode(TFT_MISO, INPUT_PULLUP);
        pinMode(SD_CS, OUTPUT);
        digitalWrite(SD_CS, HIGH);
        digitalWrite(TFT_CLK, LOW);
        digitalWrite(TFT_MOSI, HIGH);
        delay(100);

        // 3) Re-init SD card via SoftSpi
        if (sdCard.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi))) {
            f = sdCard.open(path.c_str(), O_RDONLY);
            Serial.printf("[SD_FILE] retry open=%d\n", (int)(bool)f);
        } else {
            Serial.printf("[SD_FILE] reinit failed err=0x%02X data=0x%02X\n",
                          (int)sdCard.sdErrorCode(), (int)sdCard.sdErrorData());
        }
    }

    if (!f || f.isDirectory()) {
        if (f) f.close();
        digitalWrite(SD_BUF_OE, HIGH);  // Buffer aus
        SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, 2);  // SPI restore
        xSemaphoreGiveRecursive(xSpiMutex);
        server.send(404, "text/plain", "Not found");
        return;
    }

    uint32_t fileSize = (uint32_t)f.fileSize();

    // Chunk-Buffer: klein genug um immer in Heap zu passen
    const size_t CHUNK = 2048;
    uint8_t* buf = (uint8_t*)malloc(CHUNK);
    if (!buf) {
        f.close();
        digitalWrite(SD_BUF_OE, HIGH);
        SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, 2);
        xSemaphoreGiveRecursive(xSpiMutex);
        server.send(503, "text/plain", "Out of memory");
        return;
    }

    server.sendHeader("Cache-Control", "max-age=60");
    server.setContentLength(fileSize);
    server.send(200, ct, "");

    WiFiClient client = server.client();
    uint32_t remaining = fileSize;
    while (remaining > 0 && client.connected()) {
        size_t toRead = (remaining < CHUNK) ? (size_t)remaining : CHUNK;
        int rd = f.read(buf, toRead);
        if (rd <= 0) break;
        client.write(buf, rd);
        remaining -= rd;
    }
    free(buf);
    f.close();
    digitalWrite(SD_BUF_OE, HIGH);  // Buffer aus - SD geschuetzt
    SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, 2);  // SPI wieder fuer Displays
    xSemaphoreGiveRecursive(xSpiMutex);
}

void HandleSetAnim()
{
    if (server.hasArg("MODE")) {
        systemConfig.bootAnimMode = (uint8_t)server.arg("MODE").toInt();
        prefs.begin("nixie", false);
        prefs.putUChar("anim_m", systemConfig.bootAnimMode);
        prefs.end();
        saveConfigToSD();
    }
    server.send(200, "text/plain", "OK");
}

// ═══════════════════════════════════════════════════════════════════════════
// SD helper: acquire / release SPI bus for SD access
// ═══════════════════════════════════════════════════════════════════════════

static bool sdAcquireBus()
{
    // Use a generous timeout: display task holds SPI for up to ~100ms per frame
    if (xSemaphoreTakeRecursive(xSpiMutex, pdMS_TO_TICKS(8000)) != pdTRUE) return false;
    digitalWrite(TFT_CS_0, HIGH);
    digitalWrite(TFT_CS_1, HIGH);
    digitalWrite(TFT_CS_2, HIGH);
    digitalWrite(TFT_CS_3, HIGH);
    digitalWrite(TFT_CS_4, HIGH);
    SPI.end();
    digitalWrite(SD_BUF_OE, LOW);
    delayMicroseconds(2);
    return true;
}

static void sdReleaseBus()
{
    digitalWrite(SD_BUF_OE, HIGH);
    SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, 2);
    xSemaphoreGiveRecursive(xSpiMutex);
}

// Re-initialize the SD card after it loses state due to SPI bus switches.
// Must be called while the bus is already acquired (SD_BUF_OE LOW, mutex held).
static bool sdReinit()
{
    Serial.println("[SD] reinit...");
    sdCard.end();
    // Manual pin reset so SoftSpiDriver can take over cleanly
    pinMode(TFT_CLK,  OUTPUT);       digitalWrite(TFT_CLK,  LOW);
    pinMode(TFT_MOSI, OUTPUT);       digitalWrite(TFT_MOSI, HIGH);
    pinMode(TFT_MISO, INPUT_PULLUP);
    pinMode(SD_CS,    OUTPUT);       digitalWrite(SD_CS,    HIGH);
    delay(100);
    bool ok = sdCard.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi));
    Serial.printf("[SD] reinit %s (err=0x%02X)\n", ok ? "OK" : "FAIL",
                  ok ? 0 : (int)sdCard.sdErrorCode());
    return ok;
}

// ─── SD_RESCAN: rebuild the RAM file cache from the actual SD card ────────────
// Called by the refresh button. Full recursive scan so any file written since
// boot (settings.json, uploads) is visible immediately.
void HandleSdRescan()
{
    if (!systemConfig.sdCardDetected) {
        server.send(503, "text/plain", "No SD");
        return;
    }
    if (!sdAcquireBus()) {
        server.send(503, "text/plain", "Busy");
        return;
    }
    // Reinit first so a stale SD state doesn't yield an empty scan
    sdReinit();
    sdFileCacheCount = 0;
    scanSdCache("/");
    int found = sdFileCacheCount;
    sdReleaseBus();
    Serial.printf("[SD_RESCAN] done, %d entries\n", found);
    server.send(200, "text/plain", String(found));
}

// ═══════════════════════════════════════════════════════════════════════════
// saveConfigToSD – write all settings as /settings.json on the SD card
// ═══════════════════════════════════════════════════════════════════════════

// Returns true if the file was written successfully.
bool saveConfigToSD()
{
    if (!systemConfig.sdCardDetected) return false;
    if (!sdAcquireBus()) {
        Serial.println("[CFG] saveConfigToSD: mutex timeout");
        return false;
    }

    // Delete any existing file first.
    // SdFat2: O_CREAT|O_TRUNC on a non-existent file can fail with O_WRONLY
    // on some SD cards/firmware versions. Remove + O_RDWR|O_CREAT is bulletproof.
    sdCard.remove("/settings.json");

    FsFile f = sdCard.open("/settings.json", O_RDWR | O_CREAT);
    if (!f) {
        Serial.println("[CFG] open failed – reinit SD and retry");
        if (sdReinit()) {
            sdCard.remove("/settings.json");
            f = sdCard.open("/settings.json", O_RDWR | O_CREAT);
        }
    }
    if (!f) {
        Serial.println("[CFG] saveConfigToSD: cannot open file");
        sdReleaseBus();
        return false;
    }

    // Build JSON – ArduinoJson v6
    StaticJsonDocument<1024> doc;

    JsonObject sys = doc.createNestedObject("system");
    sys["ntp"]       = systemConfig.ntpServer;
    sys["utc"]       = systemConfig.utcOffset;
    sys["dp_br"]     = systemConfig.displayBrightness;
    sys["dp_int"]    = systemConfig.displayIntervalSec;
    sys["dp_slide"]  = systemConfig.slideshowEnabled;
    sys["dp_cal"]    = systemConfig.calendarEnabled;
    sys["dp_bg"]     = systemConfig.backgroundEnabled;
    sys["anim_m"]    = systemConfig.bootAnimMode;
    sys["wt_en"]     = systemConfig.weatherEnabled;
    sys["wt_city"]   = systemConfig.weatherCity;
    sys["wt_lat"]    = serialized(String(systemConfig.weatherLat, 4));
    sys["wt_lon"]    = serialized(String(systemConfig.weatherLon, 4));
    JsonArray clFg = sys.createNestedArray("cl_fg");
    clFg.add(systemConfig.clockFgR);
    clFg.add(systemConfig.clockFgG);
    clFg.add(systemConfig.clockFgB);
    JsonArray clBg = sys.createNestedArray("cl_bg");
    clBg.add(systemConfig.clockBgR);
    clBg.add(systemConfig.clockBgG);
    clBg.add(systemConfig.clockBgB);

    JsonObject rgb = doc.createNestedObject("rgb");
    rgb["mode"] = rgbConfig.rgbMode;
    rgb["r"]    = rgbConfig.rgbR;
    rgb["g"]    = rgbConfig.rgbG;
    rgb["b"]    = rgbConfig.rgbB;
    rgb["br"]   = rgbConfig.rgbBrigthness;

    // Write via a small char buffer to stay heap-friendly
    char jsonBuf[1100];
    size_t len = serializeJsonPretty(doc, jsonBuf, sizeof(jsonBuf));
    f.write((const uint8_t*)jsonBuf, len);
    f.close();

    sdReleaseBus();

    // Keep the RAM file cache in sync so the SD browser shows the file.
    // Look for an existing entry; add one if not found.
    {
        const char* SETTINGS_PATH = "/settings.json";
        const char* SETTINGS_NAME = "settings.json";
        bool found = false;
        for (int i = 0; i < sdFileCacheCount; i++) {
            if (strcmp(sdFileCache[i].path, SETTINGS_PATH) == 0) {
                sdFileCache[i].size = (uint32_t)len;
                found = true;
                break;
            }
        }
        if (!found && sdFileCacheCount < SD_CACHE_MAX) {
            SdCacheEntry& e = sdFileCache[sdFileCacheCount++];
            strncpy(e.path, SETTINGS_PATH, sizeof(e.path) - 1);
            e.path[sizeof(e.path) - 1] = '\0';
            strncpy(e.name, SETTINGS_NAME, sizeof(e.name) - 1);
            e.name[sizeof(e.name) - 1] = '\0';
            e.isDir = false;
            e.size  = (uint32_t)len;
        }
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// HandleSdDelete – DELETE /SD_DELETE?path=/pict/img.jpg
// ═══════════════════════════════════════════════════════════════════════════

void HandleSdDelete()
{
    if (!systemConfig.sdCardDetected || !server.hasArg("path")) {
        server.send(400, "text/plain", "Bad request");
        return;
    }
    String path = server.arg("path");

    if (!sdAcquireBus()) {
        server.send(503, "text/plain", "Busy");
        return;
    }

    bool ok = sdCard.remove(path.c_str());

    // If remove failed the SD is likely in an error state after a bus switch.
    // Reinit and try once more.
    if (!ok) {
        Serial.println("[SD_DELETE] remove failed – reinit SD and retry");
        if (sdReinit()) {
            ok = sdCard.remove(path.c_str());
        }
    }

    if (ok) {
        // Remove from RAM cache
        for (int i = 0; i < sdFileCacheCount; i++) {
            if (strcmp(sdFileCache[i].path, path.c_str()) == 0) {
                for (int j = i; j < sdFileCacheCount - 1; j++) {
                    sdFileCache[j] = sdFileCache[j + 1];
                }
                sdFileCacheCount--;
                break;
            }
        }
    }

    sdReleaseBus();
    if (!ok) Serial.printf("[SD_DELETE] FAIL: %s\n", path.c_str());
    server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Delete failed");
}

// ═══════════════════════════════════════════════════════════════════════════
// HandleSdUpload – POST /SD_UPLOAD  (multipart form-data)
//   Form fields:  file=<binary>, dir=<target directory>
// ═══════════════════════════════════════════════════════════════════════════

static FsFile   s_uploadFile;
static String   s_uploadPath;
static bool     s_uploadOk    = false;
static bool     s_busAcquired = false;
static uint32_t s_uploadWritten = 0;   // bytes successfully written so far

void HandleSdUpload()
{
    if (s_uploadOk) {
        // Update file size in cache entry that HandleSdUploadFile already added
        for (int i = 0; i < sdFileCacheCount; i++) {
            if (strcmp(sdFileCache[i].path, s_uploadPath.c_str()) == 0) {
                // size was added at UPLOAD_FILE_END, nothing more to do
                break;
            }
        }
        server.send(200, "text/plain", "OK");
    } else {
        server.send(500, "text/plain", "Upload failed");
    }
    s_uploadOk    = false;
    s_busAcquired = false;
}

void HandleSdUploadFile()
{
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        // Determine target directory from query arg
        String dir = server.hasArg("dir") ? server.arg("dir") : String("/");
        if (!dir.endsWith("/")) dir += '/';
        s_uploadPath  = dir + upload.filename;
        s_uploadOk    = false;
        s_busAcquired = false;

        Serial.printf("[SD_UPLOAD] start: %s\n", s_uploadPath.c_str());

        if (!sdAcquireBus()) {
            Serial.println("[SD_UPLOAD] sdAcquireBus failed");
            return;  // s_busAcquired stays false; WRITE/END will skip safely
        }
        s_busAcquired = true;

        sdCard.remove(s_uploadPath.c_str());
        s_uploadFile = sdCard.open(s_uploadPath.c_str(), O_RDWR | O_CREAT);
        if (!s_uploadFile) {
            Serial.println("[SD_UPLOAD] open failed – reinit SD and retry");
            if (sdReinit()) {
                sdCard.remove(s_uploadPath.c_str());
                s_uploadFile = sdCard.open(s_uploadPath.c_str(), O_RDWR | O_CREAT);
            }
        }
        if (!s_uploadFile) {
            Serial.println("[SD_UPLOAD] open failed");
            sdReleaseBus();
            s_busAcquired = false;
            return;
        }
        s_uploadOk      = true;
        s_uploadWritten = 0;

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Bus is still held from START
        if (s_uploadFile && s_uploadOk) {
            size_t written = s_uploadFile.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize) {
                Serial.printf("[SD_UPLOAD] write ERROR: wanted %u got %u (total so far %u)\n",
                              upload.currentSize, (unsigned)written, s_uploadWritten);
                s_uploadOk = false;   // mark as failed; END will still close + release bus
            } else {
                s_uploadWritten += written;
                // Sync every 32 KB so SdFat2 flushes its internal buffer to the card.
                // Without this, large files (>~64 KB) can fail silently.
                if (s_uploadWritten % (32 * 1024) < upload.currentSize) {
                    s_uploadFile.sync();
                }
            }
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        if (s_uploadFile) {
            s_uploadFile.sync();   // flush remaining data before close
            s_uploadFile.close();
            if (s_uploadOk) {
                // Add/update cache entry
                bool found = false;
                for (int i = 0; i < sdFileCacheCount; i++) {
                    if (strcmp(sdFileCache[i].path, s_uploadPath.c_str()) == 0) {
                        sdFileCache[i].size = upload.totalSize;
                        found = true; break;
                    }
                }
                if (!found && sdFileCacheCount < SD_CACHE_MAX) {
                    SdCacheEntry& e = sdFileCache[sdFileCacheCount++];
                    strlcpy(e.path, s_uploadPath.c_str(), sizeof(e.path));
                    const char* slash = strrchr(e.path, '/');
                    strlcpy(e.name, slash ? slash + 1 : e.path, sizeof(e.name));
                    e.isDir = false;
                    e.size  = upload.totalSize;
                }
                Serial.printf("[SD_UPLOAD] done: %u bytes written\n", s_uploadWritten);
            } else {
                // Write failed partway through – remove the incomplete file
                sdCard.remove(s_uploadPath.c_str());
                Serial.println("[SD_UPLOAD] FAILED – incomplete file removed");
            }
        }
        if (s_busAcquired) {
            sdReleaseBus();
            s_busAcquired = false;
        }

    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (s_uploadFile) { s_uploadFile.close(); }
        if (s_busAcquired) { sdReleaseBus(); s_busAcquired = false; }
        s_uploadOk = false;
        Serial.println("[SD_UPLOAD] aborted");
    }
}
