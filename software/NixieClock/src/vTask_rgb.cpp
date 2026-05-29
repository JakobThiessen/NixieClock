#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <stdint.h>
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
    uint8_t wait = 30;

    strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();            // Turn OFF all pixels ASAP
    strip.setBrightness(10); // Set BRIGHTNESS to about 1/5 (max = 255)
    for(int i = 0; i < LED_COUNT; i++)
    {
        strip.setPixelColor(i, 0x00FF0000); //  Update delay time
    }

    static rgbConfigStruct rgbConfigRec;
    static bool status = false;
    static bool trigger = false;

    for (;;)
    {
        if (xQueue_RGB_Config != NULL)
        {                          
            status = xQueueReceive(xQueue_RGB_Config, &rgbConfigRec, pdMS_TO_TICKS(20) );
        }
        else
        {
            Serial.print("--> xQueue_RGB_Config is NULL");
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        if (status)
        {
            if(rgbConfigRec.rgbSwitch)
            {
                strip.setBrightness(rgbConfigRec.rgbBrigthness); // Set BRIGHTNESS to about 1/5 (max = 255)
            }
            else
            {
                strip.setBrightness(0);
            }
        }
        /*
        //Serial.print("Task RGB running: "); Serial.println( millis());
        if (pixelInterval != wait)
            pixelInterval = wait;
        for (uint16_t i = 0; i < pixelNumber; i++)
        {
            strip.setPixelColor(i, Wheel((i + pixelCycle) & 255)); //  Update delay time
        }
        strip.show(); //  Update strip to match
        pixelCycle++; //  Advance current cycle
        if (pixelCycle >= 256)
            pixelCycle = 0; //  Loop the cycle back to the begining
        */


        for (uint16_t i = 0; i < LED_COUNT; i++)
        {
            uint32_t color_1 = 0xFF0000;
            uint32_t color_2 = 0x00FF00;
            uint32_t color;

            if(i % 2 == 0)
            {
                color = trigger ? color_1 : color_2;
            }
            else
            {
                color = trigger ? color_2 : color_1;
            }

            strip.setPixelColor(i, color); //  Update delay time
        }
        strip.show(); //  Update strip to match
        trigger = !trigger;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
