#ifndef COMMON_CONFIG_h
#define COMMON_CONFIG_h

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************** */
/*** DEFINES              *** */
/**************************** */

#define CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS 1
#define CONFIG_FREERTOS_USE_TRACE_FACILITY 1

#define TFT_MOSI        23
#define TFT_MISO        19
#define TFT_CLK         18
#define TFT_RST         -1 // set to -1 and connect to Arduino RESET pin
#define TFT_DC          25
#define TFT_DC_2        17
#define TFT_RESET       15

#define TFT_CS_0        27
#define TFT_CS_1        16
#define TFT_CS_2        32
#define TFT_CS_3        13
#define TFT_CS_4        14

#define SD_CS           5
#define SD_BUF_OE       4   // /OE der 74LVC1G125 Tristate-Buffer (LOW=aktiv)
#define DISPLAY_POW_EN  26

#define LED_PIN         33

#define INT_IO_PIN      34
#define WAKE_UP_PIN     35

#define ADC_0_PIN       36  // ADC1
#define ADC_3_PIN       39  // ADC1

#define SPI_FREQUENCY 16000000U

// ─── I2C bus (PCA9538A button expander + PCA9685 backlight) ──────────────────
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22

#define FW_VERSION "4.8.5"
#define FW_BUILD_DATE __DATE__
#define FW_BUILD_TIME __TIME__

#define SD_CACHE_MAX 128

typedef struct {
    char path[128];
    char name[64];
    bool isDir;
    uint32_t size;
} SdCacheEntry;

extern SemaphoreHandle_t xSpiMutex;
extern SemaphoreHandle_t xI2cMutex;   // protects Wire (I2C) bus shared between PCA9685 + PCA9538A
extern QueueHandle_t xQueue_RGB_Config;
extern QueueHandle_t xQueue_WiFi;

typedef struct{
    uint8_t rgbBrigthness;
    bool rgbSwitch;
    uint16_t rgbMode;
    uint8_t rgbR;
    uint8_t rgbG;
    uint8_t rgbB;
}rgbConfigStruct;

typedef struct{
    bool wifiConnected;
    IPAddress ip;
    bool sdCardDetected;
    uint16_t displayBrightness;
    uint16_t displayIntervalSec;
    char ntpServer[64];
    int8_t utcOffset;
    bool slideshowEnabled;
    bool calendarEnabled;
    bool backgroundEnabled;
    uint8_t bootAnimMode;
    uint8_t clockFgR;
    uint8_t clockFgG;
    uint8_t clockFgB;
    uint8_t clockBgR;
    uint8_t clockBgG;
    uint8_t clockBgB;
    bool weatherEnabled;
    char weatherCity[32];
    float weatherLat;
    float weatherLon;
}systemConfigStruct;

typedef struct{
    float temperature;
    uint8_t weatherCode;
    unsigned long lastFetchMs;
    bool valid;
}weatherDataStruct;


typedef struct{
    uint8_t dayList[30];
    uint8_t monthList[30];
}calendarPublicHolidays;

typedef struct{
    uint16_t displayBrightness;

    int8_t clockUTC;
    bool clockAnimation;
    char* clockAnimatinoPicturePath;
    bool clockDigitNumber;
    bool clockDigitPicture;
    char* clockDigitPicturePath;
    bool clockDigitNumberUnicolored;
    uint16_t clockDigitNumberColor;
    bool clockDigitMulticolored;
    uint16_t* clockDigitMulticoredList;
    char* clockBackground;

    //Variables for Calendar
    bool calendarUseBackgroundPicture;
    char* calendarBackgroudPath;
    uint16_t calendarBackGroundColor;
    bool calendarAnimation;
    char* calendarAnimatinoPicturePath;

    bool caldarUseGooleCalender;
    char* calendarGoogleAPI_KEY;

    struct calendarPublicHolidays;
}clockConfiguration;

extern volatile bool bootInitDone;
extern rgbConfigStruct rgbConfig;
extern systemConfigStruct systemConfig;
extern clockConfiguration clockConfig;
extern weatherDataStruct weatherData;

#ifdef __cplusplus
}
#endif

// C++ only: saves all current settings as JSON to /sys/config.json on the SD card.
// Thread-safe: takes xSpiMutex internally. No-op when SD is not present.
#ifdef __cplusplus
void saveConfigToSD();
#endif

#endif /* COMMON_CONFIG_h */