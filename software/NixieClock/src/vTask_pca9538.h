#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ─── PCA9538A I2C address ────────────────────────────────────────────────────
// A0 = 0 (GND), A1 = 0 (GND)  →  7-bit address 0x70
#define PCA9538_ADDR        0x70

// ─── Register map ────────────────────────────────────────────────────────────
#define PCA9538_REG_IN      0x00   // Input port          (read-only)
#define PCA9538_REG_OUT     0x01   // Output port         (read/write)
#define PCA9538_REG_POL     0x02   // Polarity inversion  (read/write)
#define PCA9538_REG_CFG     0x03   // Config: 1=input, 0=output

// ─── I2C pins (match schematic: SDA=IO21, SCL=IO22) ─────────────────────────
#define PCA9538_SDA_PIN     21
#define PCA9538_SCL_PIN     22

// ─── Button assignments (active LOW – external 10k pull-up R24 via INT line) ─
// INT of PCA9538A → INT_IO_PIN (GPIO34) already defined in common.h
#define PCA9538_BTN_DIM     0   // IO0 → display dimmer
#define PCA9538_BTN_BRT     1   // IO1 → display brighter
#define PCA9538_BRIGHT_STEP 256 // step size for 12-bit PCA9685 value (0–4095)

// ─── Public API ──────────────────────────────────────────────────────────────
// Call once during setup() after Wire.begin()
void pca9538_begin();

// FreeRTOS task entry – handles INT-driven button reading and brightness control
void vTask_PCA9538(void* pvParameters);
