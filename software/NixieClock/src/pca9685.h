#pragma once
#include <stdint.h>

// ─── PCA9685 16-channel 12-bit PWM LED driver ─────────────────────────────────
// I2C address auto-detected in pca9685_begin() by scanning 0x40-0x7F.
// Default (A0-A5 all low) = 0x40.
//
// Key difference vs PCA9635:
//   Each channel has 4 registers: LED_ON_L, LED_ON_H, LED_OFF_L, LED_OFF_H
//   PWM value is 12-bit (0-4095), not 8-bit.
//   No LEDOUT control registers — outputs enabled by default.

// ─── Register addresses ───────────────────────────────────────────────────────
#define PCA9685_MODE1       0x00
#define PCA9685_MODE2       0x01
#define PCA9685_LED0_ON_L   0x06   // Channel 0 base; channel n base = 0x06 + n*4
#define PCA9685_ALL_ON_L    0xFA   // Broadcast: sets all 16 channels at once
#define PCA9685_ALL_ON_H    0xFB
#define PCA9685_ALL_OFF_L   0xFC
#define PCA9685_ALL_OFF_H   0xFD
#define PCA9685_PRE_SCALE   0xFE

// ─── MODE1 bit masks ──────────────────────────────────────────────────────────
#define PCA9685_MODE1_RESTART 0x80  // bit 7: restart enabled
#define PCA9685_MODE1_AI      0x20  // bit 5: auto-increment register pointer
#define PCA9685_MODE1_SLEEP   0x10  // bit 4: 1=low power (osc off). POR default = 1!
#define PCA9685_MODE1_ALLCALL 0x01  // bit 0: respond to ALLCALL address (default = 1)

// ─── MODE2 bit masks ──────────────────────────────────────────────────────────
#define PCA9685_MODE2_INVRT   0x10  // bit 4: invert output logic
#define PCA9685_MODE2_OCH     0x08  // bit 3: update outputs on ACK (else on STOP)
#define PCA9685_MODE2_OUTDRV  0x04  // bit 2: totem-pole (1) vs open-drain (0)

// ─── OFF_H / ON_H special bits ────────────────────────────────────────────────
#define PCA9685_FULL_ON  0x10  // Set in LED_ON_H  to force output always-ON
#define PCA9685_FULL_OFF 0x10  // Set in LED_OFF_H to force output always-OFF

// ─── Public API ───────────────────────────────────────────────────────────────

// Initialize: scan I2C bus, wake chip, configure MODE2, set all channels full.
// Must be called from single-threaded context (before FreeRTOS tasks start).
// Returns true if device found and configured.
bool pca9685_begin();

// Set one channel (0-15) to a 12-bit PWM value (0 = off, 4095 = full on).
void pca9685_setChannel(uint8_t ch, uint16_t val_4095);

// Set ALL 16 channels simultaneously via broadcast registers (0xFA-0xFD).
// brightness_4095: 0 = off, 4095 = full on.
// Thread-safe: takes/gives xI2cMutex.
void pca9685_setAllChannels(uint16_t brightness_4095);

// Debug: print MODE1, MODE2 and first 5 channel ON/OFF values to Serial.
void pca9685_dumpRegs();
