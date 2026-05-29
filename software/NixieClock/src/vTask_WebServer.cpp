
#include <stdint.h>
#include <Arduino.h>
#include "vTask_WebServer.h"
#include <common.h>

#include <WiFi.h>
#include <WebServer.h>
#include "WebPage.h"
/*
#define AP_SSID "NixieClock"
#define AP_PASS "12345678"
*/

#define AP_SSID "TH - WLAN"
#define AP_PASS "JakobThiessen@1980"

char XML[2048];
char buf[32];

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

    connectWiFi();

    server.on("/", SendWebsite);
    server.on("/xml", SendXML);
    server.on("/UPDATE_SLIDER", UpdateSlider);
    server.on("/BUTTON_0", ProcessButton_0);
    server.begin();
    Serial.print("--> Init WebServer OK: "); Serial.println( millis());

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
    Serial.println("sending web page");
    server.send(200, "text/html", PAGE_MAIN);
}

void SendXML()
{
    strcpy(XML, "<?xml version = '1.0'?>\n<Data>\n");
    if (LED0)
    {
        strcat(XML, "<LED>1</LED>\n");
    }
    else
    {
        strcat(XML, "<LED>0</LED>\n");
    }

    strcat(XML, "</Data>\n");

    server.send(200, "text/xml", XML);
}
