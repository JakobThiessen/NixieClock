#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

#include "vTask_pca9538.h"
#include "pca9685.h"
#include "common.h"

// ─── Externs (defined in main.cpp / vTask_WebServer.cpp) ────────────────────
extern systemConfigStruct systemConfig;
extern void               saveConfigToSD();

// ─── Private helpers ─────────────────────────────────────────────────────────
static TaskHandle_t s_taskHandle = NULL;

static void IRAM_ATTR pca9538_isr()
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_taskHandle, &woken);
    if (woken) portYIELD_FROM_ISR();
}

static bool pca9538_write(uint8_t reg, uint8_t val)
{
    xSemaphoreTake(xI2cMutex, portMAX_DELAY);
    Wire.beginTransmission(PCA9538_ADDR);
    Wire.write(reg);
    Wire.write(val);
    bool ok = (Wire.endTransmission() == 0);
    xSemaphoreGive(xI2cMutex);
    return ok;
}

static bool pca9538_read(uint8_t reg, uint8_t& val)
{
    xSemaphoreTake(xI2cMutex, portMAX_DELAY);
    Wire.beginTransmission(PCA9538_ADDR);
    Wire.write(reg);
    bool ok = (Wire.endTransmission(false) == 0);
    if (ok) {
        Wire.requestFrom((uint8_t)PCA9538_ADDR, (uint8_t)1);
        ok = Wire.available() != 0;
        if (ok) val = Wire.read();
    }
    xSemaphoreGive(xI2cMutex);
    return ok;
}

// Apply systemConfig.displayBrightness to PCA9685 (all 16 channels via broadcast)
static void applyBrightness()
{
    pca9685_setAllChannels(systemConfig.displayBrightness);
}

// ─── Public: init chip ───────────────────────────────────────────────────────
void pca9538_begin()
{
    // Called from task context – mutex already exists
    xSemaphoreTake(xI2cMutex, portMAX_DELAY);
    Wire.beginTransmission(PCA9538_ADDR);
    bool present = (Wire.endTransmission() == 0);
    xSemaphoreGive(xI2cMutex);
    if (!present) {
        Serial.println("[PCA9538] WARNING: chip not found on I2C bus!");
        return;
    }
    // IO0 + IO1 = inputs (buttons active-LOW), IO2-IO7 = outputs
    pca9538_write(PCA9538_REG_OUT, 0xFF);
    pca9538_write(PCA9538_REG_POL, 0x00);
    pca9538_write(PCA9538_REG_CFG, 0x03);
    Serial.printf("[PCA9538] init OK, addr=0x%02X\n", PCA9538_ADDR);
}

// ─── Public: FreeRTOS task ───────────────────────────────────────────────────
void vTask_PCA9538(void* pvParameters)
{
    s_taskHandle = xTaskGetCurrentTaskHandle();

    pca9538_begin();

    // INT pin: input-only GPIO, external pull-up via R24 (10 kΩ to +3.3 V)
    pinMode(INT_IO_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(INT_IO_PIN), pca9538_isr, FALLING);

    uint8_t lastIn  = 0xFF;   // all de-pressed (HIGH)
    uint8_t cur     = 0xFF;

    Serial.println("[PCA9538] task running");

    for (;;)
    {
        // Block until INT fires or 200 ms polling fallback
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));

        if (!pca9538_read(PCA9538_REG_IN, cur)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Debounce: wait 50 ms and re-read
        vTaskDelay(pdMS_TO_TICKS(50));
        uint8_t stable;
        if (!pca9538_read(PCA9538_REG_IN, stable) || stable != cur) continue;

        uint8_t changed = lastIn ^ cur;

        // IO0 (bit 0) falling edge → DIM
        if ((changed & (1u << PCA9538_BTN_DIM)) && !(cur & (1u << PCA9538_BTN_DIM)))
        {
            Serial.println("[PCA9538] BTN_DIM pressed");
            uint16_t& br = systemConfig.displayBrightness;
            br = (br >= PCA9538_BRIGHT_STEP) ? br - PCA9538_BRIGHT_STEP : 0;

            applyBrightness();

            Preferences prefs;
            prefs.begin("nixie", false);
            prefs.putUShort("dp_br", br);
            prefs.end();

            saveConfigToSD();
            Serial.printf("[PCA9538] brightness → %u\n", br);
        }

        // IO1 (bit 1) falling edge → BRIGHT
        if ((changed & (1u << PCA9538_BTN_BRT)) && !(cur & (1u << PCA9538_BTN_BRT)))
        {
            Serial.println("[PCA9538] BTN_BRT pressed");
            uint16_t& br = systemConfig.displayBrightness;
            br = (br <= (0xFFF - PCA9538_BRIGHT_STEP)) ? br + PCA9538_BRIGHT_STEP : 0xFFF;

            applyBrightness();

            Preferences prefs;
            prefs.begin("nixie", false);
            prefs.putUShort("dp_br", br);
            prefs.end();

            saveConfigToSD();
            Serial.printf("[PCA9538] brightness → %u\n", br);
        }

        lastIn = cur;
    }
}
