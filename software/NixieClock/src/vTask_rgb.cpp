#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <stdint.h>
#include <math.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "vTask_rgb.h"
#include "common.h"

#define LED_COUNT 32    // Number of LEDs in strip

// NeoPixel
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


int pixelInterval = 50; // Pixel Interval (ms)
bool patternComplete = false;
uint16_t pixelNumber = LED_COUNT; // Total Number of Pixels
int pixelCycle = 0;               // Pattern Pixel Cycle

void colorWipe(uint32_t color, int wait)
{
    static uint16_t current_pixel = 0;
    pixelInterval = wait;                        //  Update delay time
    strip.setPixelColor(current_pixel++, color); //  Set pixel's color (in RAM)
    strip.show();                                //  Update strip to match
    if (current_pixel >= pixelNumber)
    { //  Loop the pattern from the first LED
        current_pixel = 0;
        patternComplete = true;
    }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos)
{
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85)
    {
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170)
    {
        WheelPos -= 85;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void tCodeRGB(void *pvParameters)
{
    strip.begin();
    strip.show();            // all off
    strip.setBrightness(10);

    static rgbConfigStruct rgbConfigRec;
    rgbConfigRec.rgbMode       = 0;
    rgbConfigRec.rgbSwitch     = false;
    rgbConfigRec.rgbBrigthness = 128;
    rgbConfigRec.rgbR          = 255;
    rgbConfigRec.rgbG          = 102;
    rgbConfigRec.rgbB          = 0;

    static float breathPhase = 0.0f;

    for (;;)
    {
        if (xQueue_RGB_Config != NULL)
        {
            (void)xQueueReceive(xQueue_RGB_Config, &rgbConfigRec, pdMS_TO_TICKS(10));
        }

        switch (rgbConfigRec.rgbMode)
        {
            case 0: // Aus
                strip.setBrightness(0);
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case 1: // Statische Farbe
                strip.setBrightness(rgbConfigRec.rgbBrigthness);
                for (int i = 0; i < LED_COUNT; i++)
                    strip.setPixelColor(i, strip.Color(rgbConfigRec.rgbR, rgbConfigRec.rgbG, rgbConfigRec.rgbB));
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case 2: // Regenbogen
                strip.setBrightness(rgbConfigRec.rgbBrigthness);
                for (uint16_t i = 0; i < LED_COUNT; i++)
                    strip.setPixelColor(i, Wheel((i + pixelCycle) & 255));
                strip.show();
                if (++pixelCycle >= 256) pixelCycle = 0;
                vTaskDelay(pdMS_TO_TICKS(30));
                break;

            case 3: // Atmen (Breathing)
            {
                breathPhase += 0.04f;
                if (breathPhase > 6.2832f) breathPhase -= 6.2832f;
                uint8_t bright = (uint8_t)((sinf(breathPhase) + 1.0f) * 0.5f * rgbConfigRec.rgbBrigthness);
                strip.setBrightness(bright);
                for (int i = 0; i < LED_COUNT; i++)
                    strip.setPixelColor(i, strip.Color(rgbConfigRec.rgbR, rgbConfigRec.rgbG, rgbConfigRec.rgbB));
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            }

            case 4: // Farbwechsel (Color Wipe)
                strip.setBrightness(rgbConfigRec.rgbBrigthness);
                colorWipe(strip.Color(rgbConfigRec.rgbR, rgbConfigRec.rgbG, rgbConfigRec.rgbB), 50);
                vTaskDelay(pdMS_TO_TICKS(pixelInterval));
                break;

            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}
