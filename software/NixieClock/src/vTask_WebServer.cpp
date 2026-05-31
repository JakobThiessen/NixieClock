
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
#include "WebPage.h"
#include "credentials.h"

extern SdCacheEntry sdFileCache[];
extern int sdFileCacheCount;
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
    if (prefs.isKey("cl_fg_r"))  systemConfig.clockFgR           = prefs.getUChar("cl_fg_r");
    if (prefs.isKey("cl_fg_g"))  systemConfig.clockFgG           = prefs.getUChar("cl_fg_g");
    if (prefs.isKey("cl_fg_b"))  systemConfig.clockFgB           = prefs.getUChar("cl_fg_b");
    if (prefs.isKey("cl_bg_r"))  systemConfig.clockBgR           = prefs.getUChar("cl_bg_r");
    if (prefs.isKey("cl_bg_g"))  systemConfig.clockBgG           = prefs.getUChar("cl_bg_g");
    if (prefs.isKey("cl_bg_b"))  systemConfig.clockBgB           = prefs.getUChar("cl_bg_b");
    prefs.end();
    Serial.println("--> Prefs loaded");

    // Push persisted RGB config into the queue so the RGB task starts with correct values
    xQueueSend(xQueue_RGB_Config, &rgbConfig, pdMS_TO_TICKS(200));

    connectWiFi();

    // Apply NTP config after WiFi is connected (needs DNS)
    applyNTPConfig();
    Serial.println("--> NTP configured");

    // Signalisiere anderen Tasks, dass Initialisierung abgeschlossen ist
    bootInitDone = true;
    Serial.println("--> bootInitDone = true");

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
        "<CLFR>%u</CLFR>"
        "<CLFG>%u</CLFG>"
        "<CLFB>%u</CLFB>"
        "<CLBR>%u</CLBR>"
        "<CLBG>%u</CLBG>"
        "<CLBB>%u</CLBB>"
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
        (unsigned)systemConfig.clockFgR,
        (unsigned)systemConfig.clockFgG,
        (unsigned)systemConfig.clockFgB,
        (unsigned)systemConfig.clockBgR,
        (unsigned)systemConfig.clockBgG,
        (unsigned)systemConfig.clockBgB
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
    if (server.hasArg("INTERVAL")) systemConfig.displayIntervalSec = (uint16_t)server.arg("INTERVAL").toInt();
    if (server.hasArg("BRIGHT"))   systemConfig.displayBrightness  = (uint16_t)server.arg("BRIGHT").toInt();
    if (server.hasArg("SLIDE"))    systemConfig.slideshowEnabled   = server.arg("SLIDE").toInt() != 0;
    if (server.hasArg("CAL"))      systemConfig.calendarEnabled    = server.arg("CAL").toInt()   != 0;
    if (server.hasArg("BG"))       systemConfig.backgroundEnabled  = server.arg("BG").toInt()    != 0;
    if (server.hasArg("CFR"))      systemConfig.clockFgR           = (uint8_t)server.arg("CFR").toInt();
    if (server.hasArg("CFG"))      systemConfig.clockFgG           = (uint8_t)server.arg("CFG").toInt();
    if (server.hasArg("CFB"))      systemConfig.clockFgB           = (uint8_t)server.arg("CFB").toInt();
    if (server.hasArg("CBR"))      systemConfig.clockBgR           = (uint8_t)server.arg("CBR").toInt();
    if (server.hasArg("CBG"))      systemConfig.clockBgG           = (uint8_t)server.arg("CBG").toInt();
    if (server.hasArg("CBB"))      systemConfig.clockBgB           = (uint8_t)server.arg("CBB").toInt();
    prefs.begin("nixie", false);
    prefs.putUShort("dp_int",  systemConfig.displayIntervalSec);
    prefs.putUShort("dp_br",   systemConfig.displayBrightness);
    prefs.putUChar("dp_slide", systemConfig.slideshowEnabled  ? 1 : 0);
    prefs.putUChar("dp_cal",   systemConfig.calendarEnabled   ? 1 : 0);
    prefs.putUChar("dp_bg",    systemConfig.backgroundEnabled ? 1 : 0);
    prefs.putUChar("cl_fg_r",  systemConfig.clockFgR);
    prefs.putUChar("cl_fg_g",  systemConfig.clockFgG);
    prefs.putUChar("cl_fg_b",  systemConfig.clockFgB);
    prefs.putUChar("cl_bg_r",  systemConfig.clockBgR);
    prefs.putUChar("cl_bg_g",  systemConfig.clockBgG);
    prefs.putUChar("cl_bg_b",  systemConfig.clockBgB);
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
    // Normalize: remove trailing slash except root
    while (path.length() > 1 && path.endsWith("/")) path.remove(path.length() - 1);

    Serial.printf("[SD_LIST] path='%s' cacheCount=%d\n", path.c_str(), sdFileCacheCount);

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
    Serial.printf("[SD_LIST] found %d entries from cache\n", count);
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
    Serial.printf("[SD_FILE] %s\n", path.c_str());

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

    // Diagnostic: check if card is still reachable at raw level
    Serial.printf("[SD_FILE] card type=%d sectors=%lu\n",
                  (int)sdCard.card()->type(), (unsigned long)sdCard.card()->sectorCount());

    // Deselect ALL display CS pins explicitly before SD access
    digitalWrite(TFT_CS_0, HIGH);
    digitalWrite(TFT_CS_1, HIGH);
    digitalWrite(TFT_CS_2, HIGH);
    digitalWrite(TFT_CS_3, HIGH);
    digitalWrite(TFT_CS_4, HIGH);

    // Send clock pulses with SD_CS HIGH to sync card
    digitalWrite(SD_CS, HIGH);
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    for (int i = 0; i < 10; i++) SPI.transfer(0xFF);
    SPI.endTransaction();

    FsFile f = sdCard.open(path.c_str(), O_RDONLY);

    // If open fails, aggressive reinit: full SPI reset + slow clock + long delays
    if (!f) {
        Serial.println("[SD_FILE] open failed, aggressive reinit...");

        // 1) Kill SdFat state completely
        sdCard.end();  // calls SPI.end() internally

        // 2) Disconnect all SPI pins from peripheral, set as GPIO
        pinMode(TFT_CLK, OUTPUT);
        pinMode(TFT_MOSI, OUTPUT);
        pinMode(TFT_MISO, INPUT_PULLUP);
        pinMode(SD_CS, OUTPUT);
        digitalWrite(SD_CS, HIGH);
        digitalWrite(TFT_CLK, LOW);
        digitalWrite(TFT_MOSI, HIGH);
        delay(100);  // let card see stable lines

        // 3) Re-init SPI peripheral
        SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI);

        // 4) Card power-on sequence: >74 clock pulses at <=400kHz with CS HIGH
        digitalWrite(SD_CS, HIGH);
        SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
        for (int i = 0; i < 20; i++) SPI.transfer(0xFF);  // 160 clocks
        SPI.endTransaction();
        delay(50);

        // 5) Re-init SD card
        if (sdCard.begin(SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(1), &SPI))) {
            f = sdCard.open(path.c_str(), O_RDONLY);
            Serial.printf("[SD_FILE] retry open=%d\n", (int)(bool)f);
        } else {
            Serial.printf("[SD_FILE] reinit failed err=0x%02X data=0x%02X\n",
                          (int)sdCard.sdErrorCode(), (int)sdCard.sdErrorData());
        }
    }

    Serial.printf("[SD_FILE] open=%d\n", (int)(bool)f);

    if (!f || f.isDirectory()) {
        if (f) f.close();
        xSemaphoreGiveRecursive(xSpiMutex);
        server.send(404, "text/plain", "Not found");
        return;
    }

    uint32_t fileSize = (uint32_t)f.fileSize();
    uint8_t* fileData = (uint8_t*)malloc(fileSize);
    if (!fileData) {
        f.close();
        xSemaphoreGiveRecursive(xSpiMutex);
        server.send(503, "text/plain", "Out of memory");
        return;
    }
    f.read(fileData, fileSize);
    f.close();
    xSemaphoreGiveRecursive(xSpiMutex);

    Serial.printf("[SD_FILE] sending %u bytes\n", fileSize);
    server.sendHeader("Cache-Control", "max-age=60");
    server.setContentLength(fileSize);
    server.send(200, ct, "");

    WiFiClient client = server.client();
    uint32_t sent = 0;
    while (sent < fileSize && client.connected()) {
        uint32_t chunk = fileSize - sent;
        if (chunk > 1460) chunk = 1460;  // TCP MSS
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
