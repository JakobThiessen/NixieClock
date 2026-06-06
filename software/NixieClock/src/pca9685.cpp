#include <Arduino.h>
#include <Wire.h>
#include "pca9685.h"
#include "common.h"   // xI2cMutex, I2C_SDA_PIN, I2C_SCL_PIN

// ─── Dynamic I2C address (found during begin()) ───────────────────────────────
static uint8_t s_addr = 0xFF;

// ─── Mutex helpers (NULL-safe: begin() runs before tasks start) ──────────────
static inline void i2c_take() { if (xI2cMutex) xSemaphoreTake(xI2cMutex, portMAX_DELAY); }
static inline void i2c_give() { if (xI2cMutex) xSemaphoreGive(xI2cMutex); }

// ─── Bus recovery ─────────────────────────────────────────────────────────────
// Bit-bangs 9 SCL pulses to free a stuck SDA, generates a STOP condition,
// then re-inits the Wire peripheral at 100 kHz.
// Must be called while holding the I2C mutex (or from single-threaded setup).
static void _bus_recover()
{
    Serial.println("[PCA9685] I2C bus recovery...");
    pinMode(I2C_SCL_PIN, OUTPUT);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH); delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);  delayMicroseconds(5);
    }
    // Generate STOP: SDA low → high while SCL is high
    pinMode(I2C_SDA_PIN, OUTPUT);
    digitalWrite(I2C_SDA_PIN, LOW);
    digitalWrite(I2C_SCL_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(I2C_SDA_PIN, HIGH); delayMicroseconds(5);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);
    delayMicroseconds(200);
}

// ─── Single-register write with 3-attempt retry ───────────────────────────────
static uint8_t _writereg(uint8_t reg, uint8_t val)
{
    for (int attempt = 1; attempt <= 3; attempt++) {
        Wire.beginTransmission(s_addr);
        Wire.write(reg);
        Wire.write(val);
        uint8_t err = Wire.endTransmission();
        if (err == 0) return 0;
        Serial.printf("[PCA9685] WRITE ERR reg=0x%02X val=0x%02X err=%d (attempt %d/3)\n",
                      reg, val, err, attempt);
        _bus_recover();
    }
    return 1;
}

// ─── 4-byte channel write using auto-increment (AI bit must be set in MODE1) ──
// base_reg = 0x06 + ch*4  OR  PCA9685_ALL_ON_L (0xFA) for broadcast.
static uint8_t _write_ch_regs(uint8_t base_reg,
                               uint8_t on_l, uint8_t on_h,
                               uint8_t off_l, uint8_t off_h)
{
    for (int attempt = 1; attempt <= 3; attempt++) {
        Wire.beginTransmission(s_addr);
        Wire.write(base_reg);
        Wire.write(on_l);
        Wire.write(on_h);
        Wire.write(off_l);
        Wire.write(off_h);
        uint8_t err = Wire.endTransmission();
        if (err == 0) return 0;
        Serial.printf("[PCA9685] WRITE ERR ch_base=0x%02X err=%d (attempt %d/3)\n",
                      base_reg, err, attempt);
        _bus_recover();
    }
    return 1;
}

// ─── Single-register read ─────────────────────────────────────────────────────
static uint8_t _readreg(uint8_t reg, uint8_t& out)
{
    Wire.beginTransmission(s_addr);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission(false);   // repeated START
    if (err) { Serial.printf("[PCA9685] READ ERR reg=0x%02X err=%d\n", reg, err); return err; }
    Wire.requestFrom(s_addr, (uint8_t)1);
    out = Wire.available() ? Wire.read() : 0xFF;
    return 0;
}

// ─── Convert 12-bit brightness to PCA9685 ON/OFF register bytes ───────────────
static void _brightness_to_regs(uint16_t val,
                                 uint8_t& on_l, uint8_t& on_h,
                                 uint8_t& off_l, uint8_t& off_h)
{
    on_l = 0x00;
    if (val == 0) {
        // FULL_OFF: OFF_H bit 4 = 1, output always off
        on_h = 0x00; off_l = 0x00; off_h = PCA9685_FULL_OFF;
    } else if (val >= 4095) {
        // FULL_ON: ON_H bit 4 = 1, output always on
        on_h = PCA9685_FULL_ON; off_l = 0x00; off_h = 0x00;
    } else {
        // PWM: ON at count 0, OFF at count val
        on_h  = 0x00;
        off_l = val & 0xFF;
        off_h = (val >> 8) & 0x0F;
    }
}

// ─── Public ──────────────────────────────────────────────────────────────────

bool pca9685_begin()
{
    // Scan 0x40-0x7F (all 64 possible PCA9685 addresses)
    Serial.println("[PCA9685] Scanning I2C 0x40-0x7F...");
    s_addr = 0xFF;
    for (uint8_t a = 0x40; a <= 0x7F; a++) {
        Wire.beginTransmission(a);
        uint8_t e = Wire.endTransmission();
        if (e == 0) {
            Serial.printf("[I2C] 0x%02X ACK <--\n", a);
            if (s_addr == 0xFF) s_addr = a;
        }
    }
    if (s_addr == 0xFF) {
        Serial.println("[PCA9685] ERROR: no device found in 0x40-0x7F!");
        return false;
    }
    Serial.printf("[PCA9685] Using 0x%02X\n", s_addr);

    // MODE1: wake up (clear SLEEP bit), enable auto-increment, disable ALLCALL
    // POR default is 0x11 (SLEEP=1, ALLCALL=1) — must clear SLEEP to use oscillator
    _writereg(PCA9685_MODE1, PCA9685_MODE1_AI);  // = 0x20
    delay(1);   // >= 500 µs for oscillator stabilisation after wakeup

    // MODE2: totem-pole output drivers, update on STOP
    _writereg(PCA9685_MODE2, PCA9685_MODE2_OUTDRV);  // = 0x04

    // All channels full brightness via ALLCALL broadcast registers
    _writereg(PCA9685_ALL_ON_L,  0x00);
    _writereg(PCA9685_ALL_ON_H,  PCA9685_FULL_ON);   // FULL_ON bit → always on
    _writereg(PCA9685_ALL_OFF_L, 0x00);
    _writereg(PCA9685_ALL_OFF_H, 0x00);
    Serial.println("[PCA9685] init OK");
    return true;
}

void pca9685_setChannel(uint8_t ch, uint16_t val_4095)
{
    if (s_addr == 0xFF || ch > 15) return;
    uint8_t on_l, on_h, off_l, off_h;
    _brightness_to_regs(val_4095, on_l, on_h, off_l, off_h);

    i2c_take();
    _write_ch_regs(0x06 + ch * 4, on_l, on_h, off_l, off_h);
    i2c_give();
}

void pca9685_setAllChannels(uint16_t brightness_4095)
{
    if (s_addr == 0xFF) { Serial.println("[PCA9685] not init!"); return; }
    uint8_t on_l, on_h, off_l, off_h;
    _brightness_to_regs(brightness_4095, on_l, on_h, off_l, off_h);

    i2c_take();
    // One 4-byte write to ALL_LED broadcast registers — sets all 16 channels at once
    _write_ch_regs(PCA9685_ALL_ON_L, on_l, on_h, off_l, off_h);
    i2c_give();
}

void pca9685_dumpRegs()
{
    if (s_addr == 0xFF) return;
    i2c_take();
    uint8_t m1 = 0, m2 = 0;
    _readreg(PCA9685_MODE1, m1);
    _readreg(PCA9685_MODE2, m2);
    Serial.printf("[PCA9685] MODE1=0x%02X  MODE2=0x%02X\n", m1, m2);
    for (uint8_t ch = 0; ch < 5; ch++) {
        uint8_t on_l=0, on_h=0, off_l=0, off_h=0;
        uint8_t base = 0x06 + ch * 4;
        _readreg(base,     on_l);
        _readreg(base + 1, on_h);
        _readreg(base + 2, off_l);
        _readreg(base + 3, off_h);
        uint16_t on_cnt  = ((uint16_t)(on_h  & 0x0F) << 8) | on_l;
        uint16_t off_cnt = ((uint16_t)(off_h & 0x0F) << 8) | off_l;
        bool full_on  = (on_h  & PCA9685_FULL_ON)  != 0;
        bool full_off = (off_h & PCA9685_FULL_OFF) != 0;
        Serial.printf("[PCA9685]   ch%u: ON=%04X OFF=%04X%s%s\n", ch, on_cnt, off_cnt,
                      full_on  ? " [FULL_ON]"  : "",
                      full_off ? " [FULL_OFF]" : "");
    }
    i2c_give();
}
