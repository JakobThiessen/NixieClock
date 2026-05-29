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

#define SD_CS           4
#define SD_CS_INP       5
#define DISPLAY_POW_EN  26

#define LED_PIN         33

#define INT_IO_PIN      34
#define WAKE_UP_PIN     35

#define ADC_0_PIN       36  // ADC1
#define ADC_3_PIN       39  // ADC1

#define SPI_FREQUENCY 10000000U

extern QueueHandle_t xQueue_RGB_Config;
extern QueueHandle_t xQueue_WiFi;

typedef struct{
    uint8_t rgbBrigthness;
    bool rgbSwitch;
    uint16_t rgbMode;
}rgbConfigStruct;

typedef struct{
    bool wifiConnected;
    IPAddress ip;
    bool sdCardDetected;
}systemConfigStruct;


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

extern rgbConfigStruct rgbConfig;
extern systemConfigStruct systemConfig;
extern clockConfiguration clockConfig;

#ifdef __cplusplus
}
#endif

#endif /* COMMON_CONFIG_h */