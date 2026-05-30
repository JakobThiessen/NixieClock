
#include <stdint.h>
#include <Arduino.h>
#include "vTask_WebServer.h"
#include <common.h>

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Esp.h>
#include <SDFat.h>
#include "WebPage.h"
#include "credentials.h"

extern SdFs sdCard;
extern SemaphoreHandle_t xSpiMutex;

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
void HandleSdList();
void HandleSdFile();
void HandleSetAnim();
void applyNTPConfig();

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
    prefs.end();
    Serial.println("--> Prefs loaded");

    // Push persisted RGB config into the queue so the RGB task starts with correct values
    xQueueSend(xQueue_RGB_Config, &rgbConfig, pdMS_TO_TICKS(200));

    connectWiFi();

    // Apply NTP config after WiFi is connected (needs DNS)
    applyNTPConfig();
    Serial.println("--> NTP configured");

    server.on("/", SendWebsite);
    server.on("/xml", SendXML);
    server.on("/UPDATE_SLIDER", UpdateSlider);
    server.on("/BUTTON_0", ProcessButton_0);
    server.on("/SET_NTP", HandleSetNTP);
    server.on("/SET_RGB", HandleSetRGB);
    server.on("/SET_DISPLAY", HandleSetDisplay);
    server.on("/SD_LIST", HandleSdList);
    server.on("/SD_FILE", HandleSdFile);
    server.on("/SET_ANIM", HandleSetAnim);
    server.begin();
    Serial.print("--> Init WebServer OK: "); Serial.println(millis());

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
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int brightness = 0;
bool LED0 = false, SomeOutput = false;

void UpdateSlider()
{
    String t_state = server.arg("VALUE");
    brightness = t_state.toInt();
    Serial.print("UpdateSlider ");
    Serial.println(brightness);
    
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
    Serial.print("Button 0 ");
    Serial.println(LED0);
    rgbConfig.rgbSwitch = LED0;
    server.send(200, "text/plain", "");
    xQueueSend(xQueue_RGB_Config, &rgbConfig, 10);  // Send queue
}

void SendWebsite()
{
    Serial.printf("sending web page (free heap: %u)\n", ESP.getFreeHeap());
    // Stream directly from flash – avoids ~20KB heap allocation
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
        "<DPINT>%u</DPINT>"
        "<DPBR>%u</DPBR>"
        "<SLIDE>%u</SLIDE>"
        "<CAL>%u</CAL>"
        "<BG>%u</BG>"
        "<ANIMM>%u</ANIMM>"
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
        (unsigned)systemConfig.displayIntervalSec,
        (unsigned)systemConfig.displayBrightness,
        (unsigned)systemConfig.slideshowEnabled,
        (unsigned)systemConfig.calendarEnabled,
        (unsigned)systemConfig.backgroundEnabled,
        (unsigned)systemConfig.bootAnimMode
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
    Serial.printf("--> RGB: mode=%u R=%u G=%u B=%u br=%u\n",
        rgbConfig.rgbMode, rgbConfig.rgbR, rgbConfig.rgbG,
        rgbConfig.rgbB, rgbConfig.rgbBrigthness);
    xQueueSend(xQueue_RGB_Config, &rgbConfig, 10);
    prefs.begin("nixie", false);
    prefs.putUInt("rgb_mode", rgbConfig.rgbMode);
    prefs.putUChar("rgb_r",   rgbConfig.rgbR);
    prefs.putUChar("rgb_g",   rgbConfig.rgbG);
    prefs.putUChar("rgb_b",   rgbConfig.rgbB);
    prefs.putUChar("rgb_br",  rgbConfig.rgbBrigthness);
    prefs.end();
    server.send(200, "text/plain", "OK");
}

void HandleSetDisplay()
{
    systemConfig.displayIntervalSec = (uint16_t)server.arg("INTERVAL").toInt();
    systemConfig.displayBrightness  = (uint16_t)server.arg("BRIGHT").toInt();
    if (server.hasArg("SLIDE")) systemConfig.slideshowEnabled  = server.arg("SLIDE").toInt() != 0;
    if (server.hasArg("CAL"))   systemConfig.calendarEnabled   = server.arg("CAL").toInt()   != 0;
    if (server.hasArg("BG"))    systemConfig.backgroundEnabled = server.arg("BG").toInt()    != 0;
    prefs.begin("nixie", false);
    prefs.putUShort("dp_int",  systemConfig.displayIntervalSec);
    prefs.putUShort("dp_br",   systemConfig.displayBrightness);
    prefs.putUChar("dp_slide", systemConfig.slideshowEnabled  ? 1 : 0);
    prefs.putUChar("dp_cal",   systemConfig.calendarEnabled   ? 1 : 0);
    prefs.putUChar("dp_bg",    systemConfig.backgroundEnabled ? 1 : 0);
    prefs.end();
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
    Serial.printf("[SD_LIST] path='%s' sdDetected=%d\n", path.c_str(), (int)systemConfig.sdCardDetected);

    if (!systemConfig.sdCardDetected) {
        server.send(503, "application/json", "[]");
        return;
    }

    FsFile dir;
    if (xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY) != pdTRUE) {
        server.send(503, "application/json", "[]");
        return;
    }

    // Diagnose: ls() direkt über SdFat
    Serial.printf("[SD_LIST] sdCard.ls():\n");
    sdCard.ls(path.c_str(), LS_DATE | LS_SIZE);

    bool opened = dir.open(&sdCard, path.c_str(), O_RDONLY);
    Serial.printf("[SD_LIST] dir.open=%d isDir=%d\n", (int)opened, opened ? (int)dir.isDirectory() : -1);

    if (!opened || !dir.isDirectory()) {
        if (dir) dir.close();
        xSemaphoreGiveRecursive(xSpiMutex);
        server.send(404, "application/json", "[]");
        return;
    }
    dir.rewindDirectory();

    String json = "[";
    bool first = true;
    FsFile entry;
    char name[256];
    int count = 0;

    while (entry.openNext(&dir, O_RDONLY)) {
        entry.getName(name, sizeof(name));
        Serial.printf("[SD_LIST] entry: '%s' dir=%d\n", name, (int)entry.isDirectory());
        if (name[0] == '.') { entry.close(); continue; }

        if (!first) json += ',';
        first = false;
        count++;

        json += "{\"name\":\"";
        escapeJson(name, json);
        json += "\",\"type\":\"";
        json += entry.isDirectory() ? "dir" : "file";
        json += '"';
        if (!entry.isDirectory()) {
            json += ",\"size\":";
            json += String((uint32_t)entry.fileSize());
        }
        json += '}';
        entry.close();
    }
    dir.close();
    xSemaphoreGiveRecursive(xSpiMutex);

    json += ']';
    Serial.printf("[SD_LIST] found %d entries, json len=%d\n", count, json.length());
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

    FsFile f;
    xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);
    bool opened = f.open(&sdCard, path.c_str(), O_RDONLY);

    if (!opened || f.isDirectory()) {
        if (f) f.close();
        xSemaphoreGiveRecursive(xSpiMutex);
        server.send(404, "text/plain", "Not found");
        return;
    }

    uint32_t fileSize = (uint32_t)f.fileSize();

    // Read entire file into a heap buffer while holding the SPI mutex,
    // then release mutex and stream over WiFi – avoids interleaved SPI+WiFi.
    uint8_t* fileData = (uint8_t*)malloc(fileSize);
    if (!fileData) {
        f.close();
        xSemaphoreGive(xSpiMutex);
        server.send(503, "text/plain", "Out of memory");
        return;
    }
    f.read(fileData, fileSize);
    f.close();
    xSemaphoreGiveRecursive(xSpiMutex);

    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(fileSize);
    server.send(200, ct, "");

    WiFiClient client = server.client();
    uint32_t sent = 0;
    while (sent < fileSize && client.connected()) {
        uint32_t chunk = fileSize - sent;
        if (chunk > 1024) chunk = 1024;
        client.write(fileData + sent, chunk);
        sent += chunk;
    }
    free(fileData);
}

void HandleSetAnim()
{
    if (server.hasArg("MODE")) {
        systemConfig.bootAnimMode = (uint8_t)server.arg("MODE").toInt();
        prefs.begin("nixie", false);
        prefs.putUChar("anim_m", systemConfig.bootAnimMode);
        prefs.end();
    }
    server.send(200, "text/plain", "OK");
}
