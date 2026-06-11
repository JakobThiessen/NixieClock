/**************************** */
/*** NIXIE CLOCK          *** */
/**************************** */

#include <SPI.h>
#include <SDFat.h>
#include <Esp.h>
#include <Fonts/FreeMonoBoldOblique24pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>

#include "pca9685.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789

#include "draw7Numbers.h"
/*
#include "0.h"
#include "1.h"
#include "2.h"
#include "3.h"
#include "4.h"
#include "5.h"
#include "3_neon.h"
#include "7_neon.h"
*/
#include "weather_icon_set.h"
#include "background_1.h"
#include "birthday.h"
#include "img_advent.h"
#include "icon\sd_card.h"
#include "icon\WLAN.h"

#include <Wire.h>
#include <common.h>
#include <Preferences.h>
#include <vTask_rgb.h>
#include "vTask_pca9538.h"
#include <vTask_uart.h>
#include "vTask_WebServer.h"
#include "bootAnim.h"
#include "vTask_weather.h"
#include <JPEGDEC.h>         // JPEG decode (JPEGDEC by Larry Bank)

/**************************** */
/*** DEFINES              *** */
/**************************** */


/**************************** */
/*** LOCAL VARIABLES      *** */
/**************************** */

//SPIClass sdSPI = SPIClass(VSPI);
SdFs sdCard;

// Software-SPI für SD-Karte (umgeht ESP32 SPI-Peripherie-Probleme durch Buffer)
SoftSpiDriver<TFT_MISO, TFT_MOSI, TFT_CLK> softSpi;

SdFile file;
SdFile root_1;

/*
TFT_eSPI tft = TFT_eSPI(160, 128);               // Invoke custom library
TFT_eSPI tft_1 = TFT_eSPI(135, 240);               // Invoke custom library
TFT_eSPI tft_2 = TFT_eSPI(135, 240);               // Invoke custom library
TFT_eSPI tft_3 = TFT_eSPI(135, 240);               // Invoke custom library
TFT_eSPI tft_4 = TFT_eSPI(135, 240);               // Invoke custom library
*/

// For 1.44" and 1.8" TFT with ST7735 use: 128x160
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS_4, TFT_DC_2, TFT_RST);
// For 1.14", 1.3", 1.54", 1.69", and 2.0" TFT with ST7789:    TFT_MOSI, TFT_CLK,
Adafruit_ST7789 tft_1 = Adafruit_ST7789(TFT_CS_0, TFT_DC, TFT_RST);
Adafruit_ST7789 tft_2 = Adafruit_ST7789(TFT_CS_1, TFT_DC, TFT_RST);
Adafruit_ST7789 tft_3 = Adafruit_ST7789(TFT_CS_2, TFT_DC, TFT_RST);
Adafruit_ST7789 tft_4 = Adafruit_ST7789(TFT_CS_3, TFT_DC, TFT_RST);

rgbConfigStruct rgbConfig;
systemConfigStruct systemConfig;
clockConfiguration clockConfig;

SemaphoreHandle_t xSpiMutex = NULL;  // shared SPI bus mutex
SemaphoreHandle_t xI2cMutex = NULL;  // shared I2C (Wire) bus mutex
volatile bool bootInitDone = false;
weatherDataStruct weatherData = {};

// SD background image buffer – heap-allocated on first loadBgImage() call.
// Kept as nullptr until a valid image is loaded; freed memory only needed in display task.
uint16_t* bgImageData = nullptr;  // nullptr until first successful loadBgImage()
uint16_t  bgImageW    = 0;
uint16_t  bgImageH    = 0;

// SD directory cache – populated at boot, served by WebServer without SPI access
SdCacheEntry sdFileCache[SD_CACHE_MAX];
int sdFileCacheCount = 0;

// Recursive SD scan; call once after sdCard.begin() succeeds, before display init
void scanSdCache(const char* dirPath) {
    FsFile dir = sdCard.open(dirPath);
    if (!dir || !dir.isDirectory()) { dir.close(); return; }
    FsFile entry;
    char name[64];
    while (entry.openNext(&dir, O_RDONLY)) {
        entry.getName(name, sizeof(name));
        if (name[0] != '.' && sdFileCacheCount < SD_CACHE_MAX) {
            SdCacheEntry& e = sdFileCache[sdFileCacheCount++];
            // Build full path
            strlcpy(e.path, dirPath, sizeof(e.path));
            if (e.path[strlen(e.path)-1] != '/') strlcat(e.path, "/", sizeof(e.path));
            strlcat(e.path, name, sizeof(e.path));
            strlcpy(e.name, name, sizeof(e.name));
            e.isDir = entry.isDirectory();
            e.size  = e.isDir ? 0 : (uint32_t)entry.fileSize();
            if (e.isDir) scanSdCache(e.path);  // recurse
        }
        entry.close();
    }
    dir.close();
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared decode state for JPEG callback (set before decode, cleared after)
// ─────────────────────────────────────────────────────────────────────────────
static uint16_t* s_decBuf = nullptr;
static uint16_t  s_decW   = 0;
static uint16_t  s_decH   = 0;

// JPEG MCU block callback – copies decoded block into s_bgImageBuf.
// Full bounds checking: negative x/y from JPEGDEC are valid for right-edge MCUs
// and MUST be handled; writing before the buffer start causes a hard crash.
static int jpegBlockCb(JPEGDRAW* pDraw)
{
    if (!s_decBuf || s_decW == 0 || s_decH == 0) return 0;

    for (int row = 0; row < pDraw->iHeight; row++) {
        int dy = pDraw->y + row;
        if (dy < 0)              continue;   // block starts above frame
        if (dy >= (int)s_decH)   break;      // past bottom of frame

        // Compute source pixel range within the MCU row
        int srcX0 = 0;                       // first source column to copy
        int dstX  = pDraw->x;               // first destination column
        if (dstX < 0) { srcX0 = -dstX; dstX = 0; }  // clip left
        int cpW = pDraw->iWidth - srcX0;    // pixels remaining after left-clip
        if (dstX + cpW > (int)s_decW) cpW = (int)s_decW - dstX;  // clip right
        if (cpW <= 0) continue;

        // Safety assertion: destination range must be within buffer
        uint32_t dstOff = (uint32_t)dy * s_decW + dstX;
        if (dstOff + (uint32_t)cpW > (uint32_t)BG_IMAGE_MAX_W * BG_IMAGE_MAX_H) {
            Serial.printf("[BGIMG] JPEG cb OOB dy=%d dstX=%d cpW=%d\n", dy, dstX, cpW);
            return 0;  // abort decode
        }

        memcpy(&s_decBuf[dstOff],
               &pDraw->pPixels[(uint32_t)row * pDraw->iWidth + srcX0],
               (size_t)cpW * 2u);
    }
    return 1;
}

// JPEGDEC file-based I/O callbacks – stream directly from SD without a large file buffer.
// s_jpegFsFile points to the already-open FsFile managed by loadBgImage().
static FsFile* s_jpegFsFile = nullptr;
static void*   jpegOpenCb(const char* fn, int32_t* pSize) {
    (void)fn; *pSize = (int32_t)s_jpegFsFile->size(); return (void*)s_jpegFsFile;
}
static void    jpegCloseCb(void* p)  { (void)p; /* closed externally */ }
static int32_t jpegReadCb(JPEGFILE* p, uint8_t* buf, int32_t len) {
    (void)p; return (int32_t)s_jpegFsFile->read(buf, len);
}
static int32_t jpegSeekCb(JPEGFILE* p, int32_t pos) {
    (void)p; return s_jpegFsFile->seek(pos) ? pos : -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadBgImage() – loads BMP (24-bit) or JPEG from SD into a heap RGB565 buffer.
// Images larger than 160×128 are cropped to the top-left corner.
// Thread-safe: acquires xSpiMutex. For JPEG the bus is released after file read;
// decoding happens from a RAM copy so the display task can run concurrently.
// ─────────────────────────────────────────────────────────────────────────────
bool loadBgImage()
{
    bgImageW = 0; bgImageH = 0;

    if (!systemConfig.backgroundEnabled) {
        Serial.println("[BGIMG] background disabled");
        return false;
    }

    if (!systemConfig.sdCardDetected || systemConfig.bgImagePath[0] == '\0') {
        Serial.println("[BGIMG] no path configured");
        return false;
    }

    // Determine format from extension
    const char* ext = strrchr(systemConfig.bgImagePath, '.');
    bool isBmp  = ext && (strcasecmp(ext, ".bmp")  == 0);
    bool isJpeg = ext && (strcasecmp(ext, ".jpg")  == 0 || strcasecmp(ext, ".jpeg") == 0);
    if (!isBmp && !isJpeg) {
        Serial.println("[BGIMG] unsupported format (need .bmp or .jpg)");
        return false;
    }

    // Check file size from RAM cache – no SPI needed
    uint32_t fileSize = 0; bool found = false;
    for (int i = 0; i < sdFileCacheCount; i++) {
        if (strcmp(sdFileCache[i].path, systemConfig.bgImagePath) == 0) {
            fileSize = sdFileCache[i].size; found = true; break;
        }
    }
    if (!found) { Serial.printf("[BGIMG] not in SD cache: %s\n", systemConfig.bgImagePath); return false; }
    if (fileSize > BG_IMAGE_MAX_FILE_SIZE) {
        Serial.printf("[BGIMG] too large: %u bytes (max %u)\n", (unsigned)fileSize, (unsigned)BG_IMAGE_MAX_FILE_SIZE);
        return false;
    }

    // Puffer muss bereits in setup() alloziert worden sein
    if (!bgImageData) {
        Serial.println("[BGIMG] kein Pixelbuffer (pre-alloc fehlgeschlagen)");
        return false;
    }

    // Acquire SPI bus + SD reinit (immer, da SPI.begin für Displays den SD-Zustand zerstört)
    if (!sdAcquireBus()) {
        Serial.println("[BGIMG] sdAcquireBus failed"); return false;
    }

    auto releaseBus = [&]() { sdReleaseBus(); };

    // Open file – nach sdAcquireBus() ist sdCard garantiert frisch initialisiert
    FsFile f = sdCard.open(systemConfig.bgImagePath, O_RDONLY);
    if (!f) {
        Serial.println("[BGIMG] open failed");
        releaseBus(); return false;
    }

    // ── JPEG: stream decode directly from SD – SPI bus kept held during decode ──
    // Using file-based JPEGDEC callbacks avoids a large heap allocation for the file data.
    // Only needs ~40 KB for the RGB565 output buffer (160×128×2 bytes).
    if (isJpeg) {
        Serial.printf("[BGIMG] free heap before JPEG decode: %u bytes\n", (unsigned)ESP.getFreeHeap());
        JPEGDEC jpeg;
        s_jpegFsFile = &f;
        if (!jpeg.open(systemConfig.bgImagePath, jpegOpenCb, jpegCloseCb, jpegReadCb, jpegSeekCb, jpegBlockCb)) {
            Serial.println("[BGIMG] JPEG open failed");
            s_jpegFsFile = nullptr; f.close(); releaseBus(); return false;
        }
        jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
        uint16_t jpW = (uint16_t)jpeg.getWidth();
        uint16_t jpH = (uint16_t)jpeg.getHeight();
        uint16_t outW = jpW < BG_IMAGE_MAX_W ? jpW : BG_IMAGE_MAX_W;
        uint16_t outH = jpH < BG_IMAGE_MAX_H ? jpH : BG_IMAGE_MAX_H;
        // Use pre-allocated buffer – no large malloc at runtime
        memset(bgImageData, 0, (uint32_t)BG_IMAGE_MAX_W * BG_IMAGE_MAX_H * 2u);
        s_decBuf = bgImageData; s_decW = outW; s_decH = outH;
        jpeg.decode(0, 0, 0);
        jpeg.close();
        s_decBuf = nullptr; s_jpegFsFile = nullptr;
        f.close();
        releaseBus();
        bgImageW = outW; bgImageH = outH;
        Serial.printf("[BGIMG] JPEG loaded %dx%d (src %dx%d)\n", outW, outH, jpW, jpH);
        return true;
    }

    // ── BMP (24-bit uncompressed): decode row-by-row from SD, crop if needed ─
    uint8_t hdr[54];
    if (f.read(hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        Serial.println("[BGIMG] invalid BMP header");
        f.close(); releaseBus(); return false;
    }
    uint32_t dataOffset = (uint32_t)hdr[10] | ((uint32_t)hdr[11]<<8) | ((uint32_t)hdr[12]<<16) | ((uint32_t)hdr[13]<<24);
    int32_t  imgW       = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19]<<8) | ((uint32_t)hdr[20]<<16) | ((uint32_t)hdr[21]<<24));
    int32_t  imgH       = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23]<<8) | ((uint32_t)hdr[24]<<16) | ((uint32_t)hdr[25]<<24));
    uint16_t bpp        = (uint16_t)hdr[28] | ((uint16_t)hdr[29]<<8);
    uint32_t comp       = (uint32_t)hdr[30] | ((uint32_t)hdr[31]<<8) | ((uint32_t)hdr[32]<<16) | ((uint32_t)hdr[33]<<24);
    bool topDown = (imgH < 0); if (topDown) imgH = -imgH;
    if (bpp != 24)  { Serial.printf("[BGIMG] BMP bpp=%u (need 24)\n", bpp); f.close(); releaseBus(); return false; }
    if (comp != 0)  { Serial.println("[BGIMG] compressed BMP not supported");  f.close(); releaseBus(); return false; }
    if (imgW <= 0 || imgH <= 0) { Serial.println("[BGIMG] invalid BMP dimensions"); f.close(); releaseBus(); return false; }
    // Crop to display area
    int32_t drawW = imgW < BG_IMAGE_MAX_W ? imgW : BG_IMAGE_MAX_W;
    int32_t drawH = imgH < BG_IMAGE_MAX_H ? imgH : BG_IMAGE_MAX_H;
    if (imgW != drawW || imgH != drawH)
        Serial.printf("[BGIMG] BMP %dx%d -> crop %dx%d\n", (int)imgW, (int)imgH, (int)drawW, (int)drawH);

    uint32_t rowBytes = ((uint32_t)imgW * 3u + 3u) & ~3u;
    uint8_t* rowBuf = (uint8_t*)malloc(rowBytes);
    if (!rowBuf) {
        Serial.println("[BGIMG] BMP rowBuf malloc failed");
        f.close(); releaseBus(); return false;
    }
    // Use pre-allocated buffer – no large malloc needed
    uint16_t* imgBuf = bgImageData;
    bool ok = true;
    for (int32_t row = 0; row < drawH && ok; row++) {
        int32_t  srcRow  = topDown ? row : (imgH - 1 - row);
        uint32_t seekPos = dataOffset + (uint32_t)srcRow * rowBytes;
        if (!f.seek(seekPos) || f.read(rowBuf, rowBytes) != (int)rowBytes) { ok = false; break; }
        for (int32_t col = 0; col < drawW; col++) {
            uint8_t B = rowBuf[col * 3 + 0];
            uint8_t G = rowBuf[col * 3 + 1];
            uint8_t R = rowBuf[col * 3 + 2];
            imgBuf[row * drawW + col] = ((uint16_t)(R & 0xF8u) << 8) | ((uint16_t)(G & 0xFCu) << 3) | (B >> 3);
        }
    }
    free(rowBuf);
    if (ok) {
        bgImageW = (uint16_t)drawW; bgImageH = (uint16_t)drawH;
        Serial.printf("[BGIMG] BMP loaded %dx%d (%u bytes RAM)\n",
                      (int)drawW, (int)drawH, (unsigned)((uint32_t)drawW * (uint32_t)drawH * 2u));
    } else {
        Serial.println("[BGIMG] BMP pixel read error");
    }
    f.close();
    releaseBus();
    return ok;
}

TaskHandle_t taskRGB_LED;
TaskHandle_t task_Uart;
TaskHandle_t task_Display;
TaskHandle_t serverTask;

QueueHandle_t  xQueue_RGB_Config = xQueueCreate( 10, sizeof(rgbConfigStruct));
QueueHandle_t  xQueue_WiFi = xQueueCreate( 1, sizeof(systemConfig));

void tCodeDisplay(void *pvParameters);

/**************************** */
/*** LOCAL FUNCTIONS      *** */
/**************************** */

void handle_cs(uint8_t spiDeviceNumber, uint8_t level)
{
    // Take SPI mutex when starting a transaction (CS→LOW).
    // portMAX_DELAY: display task MUST wait until SD/WebServer releases bus.
    // Ignoring the return value with a short timeout was the root cause of SPI corruption.
    if (level == LOW && xSpiMutex) {
        xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);
    }

    switch(spiDeviceNumber)
    {
        case 0:
            digitalWrite(TFT_CS_4, level);
            break;

        case 1:
            digitalWrite(TFT_CS_0, level);
            break;
            
        case 2:
            digitalWrite(TFT_CS_1, level);
            break;
            
        case 3:
            digitalWrite(TFT_CS_2, level);
             break;
            
        case 4:   
            digitalWrite(TFT_CS_3, level);
            break;

        case 5:   
            if (level == LOW) {
                digitalWrite(SD_BUF_OE, LOW);  // Buffer aktiv BEVOR CS selektiert
                delayMicroseconds(1);
            }
            digitalWrite(SD_CS, level);
            if (level == HIGH) {
                delayMicroseconds(1);
                digitalWrite(SD_BUF_OE, HIGH); // Buffer aus NACH CS deselektiert
            }
            break;

        default:
            break;
    }

    if (level == HIGH && xSpiMutex) {
        xSemaphoreGiveRecursive(xSpiMutex);
    }
}

void displayRESET(uint8_t pin)
{
    digitalWrite(pin, LOW);
    delay(50);
    digitalWrite(pin, HIGH);
    delay(10);
}

int getDaysInMonth(int year, int month)
{
    if (month == 2)
    {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        {
            return 29; // February in a leap year
        }
        else
        {
            return 28; // February in a non-leap year
        }
    }
    else if (month == 4 || month == 6 || month == 9 || month == 11)
    {
        return 30; // April, June, September, and November
    }
    else
    {
        return 31; // All other months
    }
}

uint16_t color_sand = tft.color565(240,230,140);

void houtGlas_init()
{
    handle_cs(0, LOW);
    tft.fillScreen(ST7735_BLACK);    //   ST7735_BLACK
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.fillRect(0, 100, 240, 240, color_sand); // full sand in glass 1
    handle_cs(0, HIGH);
}

/*
void hourGlass_Write()
{
    tft1.fillRect(0, 100, 240, 240, color); // full sand in glass 1
    tft2.fillScreen(COLOR0);                // no sand in glass 2

    tft1.setCursor(104, 10);
    tft1.setTextColor(COLOR4);
    tft1.setTextSize(4);
    if (hour < 10)
    {
        tft1.print("0");
    }
    tft1.println(hour); // show hour

    // animated sand track
    for (int i = 0; i < 250; i += 10)
    {
        tft2.fillRect(118, 0, fontSize, i, color);
        delay(10);
    }

    tft1.fillCircle(50, fontSize0, 14, WHITE1);
    tft2.fillCircle(50, 45, 14, WHITE2); // add reflections

    // trickling sand
    m = map(minute, 0, fontSize9, 0, 141);
    tft1.fillTriangle(0, m * 0.9 + 68, 240, m * 0.9 + 68, 120, m * 1.2 + 90, COLOR0); // upper glass

    n = map(m, 0, 141, 250, 75);
    tft2.fillTriangle(0, n * 0.55 + 105, 240, n * 0.55 + 105, 120, n, color); // lower glass
    tft2.fillTriangle(108, n + 7, 132, n + 7, 120, n - 6, color);             // sand track end

    delay(10);

    tft2.fillRect(118, 0, fontSize, n - 4, COLOR00); // delete sand track
}
*/

void drawSystem()
{
    handle_cs(0, LOW);
    tft.fillScreen(ST77XX_BLACK);
    handle_cs(0, HIGH);

    //WLAN and IP
    if (systemConfig.backgroundEnabled)
    {
        handle_cs(0, LOW);
        tft.drawRGBBitmap(0, 0, background_1, 160, 128);
        handle_cs(0, HIGH);
    }

    if(systemConfig.sdCardDetected)
    {
        handle_cs(0, LOW);
        tft.drawRGBBitmap(110, 10, image_sd_card, 32, 32);
        handle_cs(0, HIGH);
    }
    else
    {
        handle_cs(0, LOW);
        tft.drawRGBBitmap(110, 10, image_no_sd_card, 32, 32);
        handle_cs(0, HIGH);
    }

    if(systemConfig.wifiConnected == true)
    {
        handle_cs(0, LOW);
        tft.drawRGBBitmap(15, 10, image_wlan, 32, 32);
        handle_cs(0, HIGH);
    }
    else
    {
        handle_cs(0, LOW);
        tft.drawRGBBitmap(15, 10, image_no_wlan, 32, 32);
        handle_cs(0, HIGH);
    }

    handle_cs(0, LOW);
    tft.setTextSize(1);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.setTextColor(systemConfig.backgroundEnabled ? ST7735_BLACK : ST77XX_WHITE);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.setCursor(15, 50);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.print(systemConfig.ip.toString());
    handle_cs(0, HIGH);

    Serial.printf(" --> Wifi Connection: %d  IP: ", systemConfig.wifiConnected);
    Serial.println(systemConfig.ip);
}

void drawWeather(int16_t tempActual)
{

    tft.fillScreen(ST77XX_BLACK);
    tft.drawRGBBitmap(0, 0, background_1, 160, 128);

    tft.setTextSize(2);
    tft.setTextColor(ST7735_BLACK);
    tft.setCursor(15, 20);
    char temperature[10];
    sprintf(temperature, "%d%cC", tempActual, (char)247);
    tft.print(temperature);
    
    tft.setCursor(15, 40);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_BLACK);
    char text[30];
    sprintf(text, "Espel 17%c/7%c", (char)247,(char)247 );
    tft.print(text);
    tft.drawRGBBitmap(15+32*2+10*2, 20,      cloudy, 32, 32);

    // Second field

    tft.drawRGBBitmap(15,           64 + 10, chancetstorms, 32, 32);
    tft.drawRGBBitmap(15+32*1+10*1, 64 + 10, clear, 32, 32);
    tft.drawRGBBitmap(15+32*2+10*2, 64 + 10, nt_snow, 32, 32);

}

void drawCalendar(int currentDay, int month, int year, uint8_t shortMonthWithYear)
{
    const char *days[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    const char *strg_month[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};
    char stgMonth[20];

    // Adapt text colours to background brightness
    uint16_t colorFont   = systemConfig.bgTextDark ? (uint16_t)ST77XX_BLACK : (uint16_t)ST77XX_WHITE;
    uint16_t colorHeader = systemConfig.bgTextDark ? (uint16_t)0x0010       : (uint16_t)ST77XX_CYAN;
    uint16_t colorMarker = ST77XX_RED;
    uint16_t colorLine   = systemConfig.bgTextDark ? (uint16_t)0x8430       : (uint16_t)ST77XX_BLUE;
    uint8_t fontsize = 1;

    int disp_w = 160;
    //int disp_h = 128;

    int startPosX = 20;
    int startPosY = 30;

    handle_cs(0, LOW);
    tft.fillScreen(ST77XX_BLACK);
    handle_cs(0, HIGH);

    if (bgImageData && bgImageW > 0 && bgImageH > 0) {
        // SD background image (RGB565 in RAM) – use explicit cast to select the non-PROGMEM overload
        handle_cs(0, LOW);
        tft.drawRGBBitmap(0, 0, (uint16_t*)bgImageData, (int16_t)bgImageW, (int16_t)bgImageH);
        handle_cs(0, HIGH);
    }
    // kein Bild geladen (Haken gesetzt aber kein Pfad, oder Ladefehler) → schwarzer Hintergrund bleibt

    handle_cs(0, LOW);
    tft.setTextColor(colorHeader);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.setTextSize(2);
    handle_cs(0, HIGH);

    int pos_x, pos_y;
    int16_t x1, y1;
    uint16_t w, h;

    // Calculate the day of the week for the first day of the month
    struct tm tm = {0};
    tm.tm_mday = 1;
    tm.tm_mon = month - 1;           // Months are 0-based in tm structure
    tm.tm_year = year - 1900;        // Years are years since 1900 in tm structure
    mktime(&tm);                     // Update the tm structure with the correct values
    int firstDayOfWeek = tm.tm_wday; // Get the day of the week

    int day = 1;
    String str_day = (String)day;
    int days_of_month = getDaysInMonth(year, month);

    // Draw --> Monthname
    if(shortMonthWithYear)
    {
        char tempStrg[4];
        strncpy(tempStrg, strg_month[month-1], 3);
        tempStrg[3] = '\0';
        sprintf(stgMonth, "%s. %d", tempStrg, year);
    }
    else
    {
        sprintf(stgMonth, "%s", strg_month[month-1]);
    }
    
    tft.getTextBounds(stgMonth, 0, 0, &x1, &y1, &w, &h);
    pos_x = (disp_w - w) / 2;
    pos_y = (startPosY - h) / 2;
    
    handle_cs(0, LOW);
    tft.setCursor(pos_x, pos_y);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.print(stgMonth);
    handle_cs(0, HIGH);

    // Draw --> Headline
    handle_cs(0, LOW);
    tft.setTextSize(fontsize);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.setTextColor(colorFont);
    handle_cs(0, HIGH);

    pos_x = startPosX;
    pos_y = startPosY;
    int offset = 10;
    int firstEndPosX = 0;

    for (int i = 0; i < 7; i++)
    {
        tft.getTextBounds(days[i], 0, 0, &x1, &y1, &w, &h);
        tft.setCursor(pos_x, pos_y);
        tft.print(days[i]);
    	if (i == 0)
        {
            offset = 10;
            firstEndPosX = pos_x + w;
        }
        else
        {
            offset = 5;
        }
        
        pos_x = pos_x + w + offset;
    }

    // Draw Line 
    int linePosY = startPosY + 10;

    tft.drawLine(startPosX, linePosY, pos_x, linePosY, colorLine);
    tft.drawLine(firstEndPosX+5, startPosY, firstEndPosX+5, startPosX + 100, colorLine);

    // Print Days of the Month
    pos_y = linePosY + 10 ;

    // Skip the days before the first day of the month
    day = (firstDayOfWeek - 1) * -1;

    pos_x = startPosX;
    for(int i = 0; i < firstDayOfWeek; i++)
    {
        tft.getTextBounds(days[i], 0, 0, &x1, &y1, &w, &h);
        if(i == 0)
        {
            offset = 10; 
        }
        else
        {
            offset = 5;
        }

        pos_x = pos_x + w + offset;
    }

    while (day <= days_of_month)
    {
        for (int i = 0; i < 7; i++)
        {
            if ((day >= 1) && (day <= days_of_month))
            {
                tft.getTextBounds(days[i], 0, 0, &x1, &y1, &w, &h);

                tft.setCursor(pos_x, pos_y);
                if(i == 0)
                {
                    offset = 10; 
                }
                else
                {
                    offset = 5;
                }
                pos_x = pos_x + w + offset;

                // highlight today with filled red circle
                if (day == currentDay)
                {
                    int cur_x = tft.getCursorX();
                    int cur_y = tft.getCursorY();

                    tft.fillRoundRect(cur_x -2, cur_y - 2, w+4, h+4, 2, colorMarker);
                    tft.setTextColor(colorFont);
                    if (day > 0)
                        tft.print(day);
                }
                else
                {
                    if (day > 0)
                        tft.print(day);
                }
            }
            day++;
        }
        pos_x = startPosX;
        pos_y += 16;
    }

}

/*
* pict 0 birhtday
* pict 1 19th birthday
* pict 2 19th birthday_1
*/
void drawBirthday(uint8_t pict)
{
    (void)pict;
    handle_cs(0, LOW);
    tft.fillScreen(ST77XX_BLACK);
    handle_cs(0, HIGH);

    switch (pict)
    {
        case 0:
            tft.drawRGBBitmap(0, 0, birthday_1, 160, 114);
            break;
        case 1:
            tft.drawRGBBitmap(0, 0, birthday_19th_1, 160, 90);
            break;
        case 2:
            tft.drawRGBBitmap(0, 0, birthday_19th_2, 160, 104);
            break;
        default:
            break;
    } 
    


}

void drawPowerSupply(uint16_t voltage_mV, uint16_t current_mA)
{
    char str_voltage[6];
    char str_current[6];
    sprintf(str_voltage, "%05d", voltage_mV);
    sprintf(str_current, "%05d", current_mA);
    tft.fillScreen(ST77XX_BLACK);
    int i = 0;

    int16_t x_0 = 4+50;
    int16_t y_0 = 52;
    int16_t x_1 = 4+50+3;
    int16_t y_1 = 48;
    int16_t x_2 = 4+50 + 6;
    int16_t y_2 = 52;

    do
    {
        draw7Number(str_voltage[i] - '0', 10 + i * 25, 10, 2, ST77XX_GREEN, 0x18c3, 1, &tft);
        i++;
    } while (i < 5);
    tft.fillTriangle(x_0, y_0, x_1, y_1, x_2, y_2, ST77XX_GREEN);
    tft.drawChar(135, 10+20 , 'V', ST77XX_GREEN, ST7735_BLACK, 3);
    
    x_0 = 4+50;
    y_0 = 70+42;
    x_1 = 4+50+3;
    y_1 = 70+38;
    x_2 = 4+50 + 6;
    y_2 = 70+42;
    i = 1;
    do
    {
        draw7Number(str_current[i] - '0', 10 + i * 25, 70, 2, ST77XX_RED, 0x18c3, 1, &tft);
        i++;
    } while (i < 5);
    tft.fillTriangle(x_0, y_0, x_1, y_1, x_2, y_2, ST77XX_RED);
    tft.drawChar(135, 70 + 20 , 'A', ST77XX_RED, ST7735_BLACK, 3);

}

void drawAdvent()
{
    handle_cs(0, LOW);
    tft.fillScreen(ST77XX_BLACK);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.drawRGBBitmap(0, 0, bg_advent_2, 227, 128);
    handle_cs(0, HIGH);
}

struct tm timeinfo;
int getLocTime(int *day, int *month, int *year, uint8_t *hour, uint8_t *min, uint8_t *sec)
{
    if (!getLocalTime(&timeinfo))
    {
        return -1;
    }
    *day = timeinfo.tm_mday;
    *month = timeinfo.tm_mon;
    *year = timeinfo.tm_year;

    *hour = timeinfo.tm_hour;
    *min = timeinfo.tm_min;
    *sec = timeinfo.tm_sec;

    // Print time every 10 seconds
    static uint8_t lastPrintedSec = 0xFF;
    if (timeinfo.tm_sec != lastPrintedSec && timeinfo.tm_sec % 10 == 0) {
        lastPrintedSec = timeinfo.tm_sec;
        printf("%02d:%02d:%02d %02d.%02d.%04d\n",
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
            timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900);
    }

    return 0;
}

volatile static uint8_t old_hour = 0;
volatile static uint8_t old_min = 0;
volatile static uint8_t old_sec = 0;

void drawTime(uint8_t hour, uint8_t min, uint8_t sec, int timestatus)
{
    static int8_t old_timestatus = -1;

    uint8_t digit_1 = hour / 10;
    uint8_t digit_2 = hour % 10;
    uint8_t digit_3 = min  / 10;
    uint8_t digit_4 = min  % 10;

    uint8_t old_digit_1 = old_hour / 10;
    uint8_t old_digit_2 = old_hour % 10;
    uint8_t old_digit_3 = old_min  / 10;
    uint8_t old_digit_4 = old_min  % 10;

    // Force full redraw when timestatus changes (e.g. NTP sync acquired/lost)
    bool force = (old_timestatus != timestatus);

    if (timestatus == 0)
    {
        if (force || digit_1 != old_digit_1) {
            tft_1.fillScreen(ST77XX_BLACK);
            draw7Number(digit_1, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_1);
        }
        if (force || digit_2 != old_digit_2) {
            tft_2.fillScreen(ST77XX_BLACK);
            draw7Number(digit_2, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_2);
        }
        if (force || digit_3 != old_digit_3) {
            tft_3.fillScreen(ST77XX_BLACK);
            draw7Number(digit_3, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_3);
        }
        if (force || digit_4 != old_digit_4) {
            tft_4.fillScreen(ST77XX_BLACK);
            draw7Number(digit_4, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_4);
        }
    }
    else
    {
        // No valid time – show 0000, but only when transitioning into error state
        if (force) {
            tft_1.fillScreen(ST77XX_BLACK);
            tft_2.fillScreen(ST77XX_BLACK);
            tft_3.fillScreen(ST77XX_BLACK);
            tft_4.fillScreen(ST77XX_BLACK);
            draw7Number(0, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_1);
            draw7Number(0, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_2);
            draw7Number(0, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_3);
            draw7Number(0, 20, 20, 10, ST77XX_MAGENTA, 0x18c3, 1, &tft_4);
        }
    }

    old_hour       = hour;
    old_min        = min;
    old_sec        = sec;
    old_timestatus = timestatus;
}

void drawAdventsKerzen()
{
    handle_cs(1, LOW);   
    tft_1.fillScreen(ST77XX_BLACK);
    handle_cs(1, HIGH);

    handle_cs(1, LOW);   
    tft_1.drawRGBBitmap(0, 0, bg_advent_kerze, 135, 210);
    handle_cs(1, HIGH);

    handle_cs(2, LOW);   
    tft_2.fillScreen(ST77XX_BLACK);
    handle_cs(2, HIGH);

    handle_cs(2, LOW);   
    tft_2.drawRGBBitmap(0, 0, bg_advent_kerze, 135, 210);
    handle_cs(2, HIGH);

    handle_cs(3, LOW);   
    tft_3.fillScreen(ST77XX_BLACK);
    handle_cs(3, HIGH);

    handle_cs(3, LOW);   
    tft_3.drawRGBBitmap(0, 0, bg_advent_kerze, 135, 210);
    handle_cs(3, HIGH);

    handle_cs(4, LOW);   
    tft_4.fillScreen(ST77XX_BLACK);
    handle_cs(4, HIGH);

    handle_cs(4, LOW);   
    tft_4.drawRGBBitmap(0, 0, bg_advent_kerze, 135, 210);
    handle_cs(4, HIGH);
}

/**************************** */
/*** SETUP                *** */
/**************************** */
void setup(void)
{
    Serial.begin(115200);

    // Beide großen Puffer als allererstes allozieren – Heap ist jetzt maximal defragmentiert.
    // Spätere Inits (SPI, Adafruit, SD, WiFi) und Task-Stacks fragmentieren den Heap;
    // danach sind 40–64 KB am Stück nicht mehr garantiert verfügbar.
    bgImageData = (uint16_t*)malloc((size_t)BG_IMAGE_MAX_W * BG_IMAGE_MAX_H * sizeof(uint16_t));
    Serial.printf("[BGIMG] pre-alloc %s (free heap: %u)\n",
                  bgImageData ? "OK" : "FAIL",
                  (unsigned)ESP.getFreeHeap());


    Serial.printf("***** ESP32 Info ****\n\r");
    Serial.printf("** getChipCore:      %d\n\r", ESP.getChipCores() );
    Serial.printf("** getChipModel:     %s\n\r", ESP.getChipModel() );
    Serial.printf("** getChipRevision:  %d\n\r", ESP.getChipRevision() );
    Serial.printf("** getCpuFreqMHz:    %d\n\r", ESP.getCpuFreqMHz() );
    Serial.printf("** getFlashChipSize: %d\n\r", ESP.getFlashChipSize() );

    Serial.printf("** getFlashChipSize: %s\n\r", ESP.getSdkVersion() );
    Serial.printf("** getFlashChipSize: %d\n\r", ESP.getHeapSize() );
    Serial.printf("******************************\n\r");

    pinMode(TFT_MISO, INPUT_PULLUP);
    pinMode(TFT_MOSI, OUTPUT);
    digitalWrite(TFT_MOSI, HIGH);
    pinMode(TFT_CLK, OUTPUT);
    digitalWrite(TFT_CLK, HIGH);

    pinMode(DISPLAY_POW_EN, OUTPUT);
    digitalWrite(DISPLAY_POW_EN, LOW);

    pinMode(TFT_CS_0, OUTPUT);
    digitalWrite(TFT_CS_0, HIGH);

    pinMode(TFT_CS_1, OUTPUT);
    digitalWrite(TFT_CS_1, HIGH);

    pinMode(TFT_CS_2, OUTPUT);
    digitalWrite(TFT_CS_2, HIGH);

    pinMode(TFT_CS_3, OUTPUT);
    digitalWrite(TFT_CS_3, HIGH);
    
    pinMode(TFT_CS_4, OUTPUT);
    digitalWrite(TFT_CS_4, HIGH); // hab von Low auf High geändert

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    pinMode(SD_BUF_OE, OUTPUT);
    digitalWrite(SD_BUF_OE, HIGH);  // Buffer AUS waehrend Display-Init (SD soll keinen Traffic sehen)

    // === DIAGNOSE-TEST: Displays ZUERST, dann SD ===
    // So testen wir ob der HW-Tristate-Buffer auf CLK+MOSI funktioniert.
    // WICHTIG: 4. Parameter = SS-Pin. Ohne Angabe benutzt VSPI GPIO5 als Hardware-SS!
    // Das verhindert dass digitalWrite(SD_CS) funktioniert.
    // Fix: GPIO 2 als dummy Hardware-SS verwenden (ist frei, nur onboard LED auf manchen Boards)
    SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, 2);  // SS=GPIO2 (dummy, nicht GPIO5!)
    pinMode(TFT_MISO, INPUT_PULLUP);  // Pull-Up auf MISO - sonst floatet die Leitung auf 0
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    digitalWrite(DISPLAY_POW_EN, HIGH);
    delay(50);
    pinMode(TFT_RESET, OUTPUT);
    displayRESET(TFT_RESET);

    // I2C bus – shared by PCA9685 (backlight) and PCA9538A (button expander)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);   // 100 kHz – reliable on this PCB; 400kHz causes bus lockup

    /* DISPLAY HINTERGRUNDBELUCHTUNG – PCA9685 16-ch 12-bit PWM driver */
    if (!pca9685_begin())
    {
        Serial.println("Error PCA9685 not connected!");
    }


    /* DISPLAY INIT */

    // Use this initializer if using a 1.8" TFT screen:
    handle_cs(0, LOW);
    tft.initR(INITR_BLACKTAB);                     //
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.setRotation(1);
    handle_cs(0, HIGH);

    handle_cs(0, LOW);
    tft.fillScreen(ST77XX_BLACK);
    handle_cs(0, HIGH);

    // OR use this initializer (uncomment) if using a 1.14" 240x135 TFT:
    handle_cs(1, LOW); 
    tft_1.init(135, 240); // Init ST7789 240x135
    handle_cs(1, HIGH);

    handle_cs(1, LOW);    
    tft_1.setRotation(2);
    handle_cs(1, HIGH);

    handle_cs(1, LOW);
    tft_1.fillScreen(ST77XX_BLACK);
    handle_cs(1, HIGH);

    handle_cs(2, LOW);
    tft_2.init(135, 240); // Init ST7789 240x135
    handle_cs(2, HIGH);

    handle_cs(2, LOW); 
    tft_2.setRotation(2);
    handle_cs(2, HIGH);

    handle_cs(2, LOW);
    tft_2.fillScreen(ST77XX_BLACK);
    handle_cs(2, HIGH);

    handle_cs(3, LOW);
    tft_3.init(135, 240); // Init ST7789 240x135
    handle_cs(3, HIGH);

    handle_cs(3, LOW);
    tft_3.setRotation(2);
    handle_cs(3, HIGH);

    handle_cs(3, LOW); 
    tft_3.fillScreen(ST77XX_BLACK);
    handle_cs(3, HIGH);

    handle_cs(4, LOW);
    tft_4.init(135, 240); // Init ST7789 240x135
     handle_cs(4, HIGH);

    handle_cs(4, LOW);
    tft_4.setRotation(2);
    handle_cs(4, HIGH);

    handle_cs(4, LOW);
    tft_4.fillScreen(ST77XX_BLACK);
    handle_cs(4, HIGH);

    tft_1.setFont(&FreeMonoBoldOblique24pt7b);
    tft_2.setFont(&FreeMonoBoldOblique24pt7b);
    tft_3.setFont(&FreeMonoBoldOblique24pt7b);
    tft_4.setFont(&FreeMonoBoldOblique24pt7b);


    // === SD-KARTE INIT (nach Display-Init, Tristate-Buffer schuetzt SD) ===
    digitalWrite(SD_BUF_OE, LOW);   // Buffer AN
    delay(10);
    SPI.end();                       // SPI-Peripherie freigeben fuer SoftSpiDriver
    pinMode(TFT_MISO, INPUT_PULLUP);

    for (int attempt = 1; attempt <= 3 && !systemConfig.sdCardDetected; attempt++) {
        Serial.printf("[SD] Versuch %d/3... ", attempt);
        systemConfig.sdCardDetected = sdCard.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi));
        if (!systemConfig.sdCardDetected) {
            Serial.printf("err=0x%02X\n", (int)sdCard.sdErrorCode());
            delay(300);
        } else {
            Serial.println("OK");
        }
    }

    if (systemConfig.sdCardDetected) {
        sdFileCacheCount = 0;
        scanSdCache("/");
        Serial.printf("[SD] Cache: %d Eintraege\n", sdFileCacheCount);
    } else {
        Serial.println("[SD] FEHLGESCHLAGEN");
    }

    SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, 2);  // SPI wieder fuer Displays
    digitalWrite(SD_BUF_OE, HIGH);               // Buffer AUS

    // Initialize systemConfig defaults (Preferences loaded later in WebServer task)
    strncpy(systemConfig.ntpServer, "de.pool.ntp.org", sizeof(systemConfig.ntpServer) - 1);
    systemConfig.ntpServer[sizeof(systemConfig.ntpServer) - 1] = '\0';
    systemConfig.utcOffset          = 1;
    systemConfig.displayIntervalSec = 5;
    systemConfig.displayBrightness  = 0xFFF;
    systemConfig.slideshowEnabled   = false;
    systemConfig.calendarEnabled    = true;
    systemConfig.backgroundEnabled  = false;
    systemConfig.bootAnimMode       = 0;  // default: keine Animation
    systemConfig.bgImagePath[0]     = '\0';
    systemConfig.bgTextDark         = false;

    // RGB defaults – overwritten by Preferences at boot if keys exist
    rgbConfig.rgbMode       = 0;      // Aus
    rgbConfig.rgbSwitch     = false;
    rgbConfig.rgbBrigthness = 128;
    rgbConfig.rgbR          = 255;
    rgbConfig.rgbG          = 102;
    rgbConfig.rgbB          = 0;

    // Initial NTP config with defaults (WebServer task will reload from Preferences)
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", systemConfig.ntpServer);
    //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    /*
        while ( !getLocalTime( &timeinfo, 0 ) )  // wait for NTP to sync
        vTaskDelay( 10 / portTICK_PERIOD_MS );
    */

    if( xQueue_RGB_Config == NULL )
    {
        // Fehler Erkennung
        while(1)
        {
            Serial.println("Error during queue creation: xQueue_RGB_Config");
            delay(500);
        }
    }

    if( xQueue_WiFi == NULL )
    {
        // Fehler Erkennung
        while(1)
        {
            Serial.println("Error during queue creation: xQueue_WiFi");
            delay(500);
        }
        
    }

    xSpiMutex = xSemaphoreCreateRecursiveMutex();
    xI2cMutex = xSemaphoreCreateMutex();
    if (!xI2cMutex) Serial.println("ERROR: xI2cMutex creation failed!");

    // Display-Task ZUERST erstellen: sichert 40 KB zusammenhängenden Heap bevor der
    // WebServer-Task (80 KB) den Speicher fragmentiert.
    Serial.printf("[MEM] free heap vor Display-Task: %u, largest block: %u\n",
                   (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    BaseType_t dispResult = xTaskCreate(
        tCodeDisplay,   /* Task function. */
        "TaskDisp",     /* name of task. */
        40000,          /* Stack size (bytes): needs ~8 KB for JPEGDEC struct on stack */
        NULL,           /* parameter of the task */
        1,              /* priority of the task */
        &task_Display   /* Task handle to keep track of created task */
    );
    if (dispResult != pdPASS) Serial.printf("ERROR: xTaskCreate Display FEHLGESCHLAGEN! (free=%u, maxAlloc=%u)\n",
                                            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    BaseType_t srvResult = xTaskCreatePinnedToCore(
        tCodeServerTask, /* Task function. */
        "serverTask",   /* name of task. */
        60000,          /* Stack size: 60 KB */
        NULL,           /* parameter of the task */
        3,              /* priority of the task */
        &serverTask,    /* Task handle */
        0               /* Core 0: WiFi/lwIP laeuft ebenfalls auf Core 0 → weniger inter-core overhead */
    );
    if (srvResult != pdPASS) Serial.printf("ERROR: xTaskCreate WebServer FEHLGESCHLAGEN! (free=%u, maxAlloc=%u)\n",
                                           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());              

    xTaskCreate(
        tCodeRGB,       /* Task function. */
        "TaskRGB",      /* name of task. */
        5000,           /* Stack size of task */
        NULL,           /* parameter of the task */
        3,              /* priority of the task */
        &taskRGB_LED    /* Task handle to keep track of created task */
    );
    
    xTaskCreate(
        tCodeUart,      /* Task function. */
        "TaskUart",     /* name of task. */
        5000,          /* Stack size of task */
        NULL,           /* parameter of the task */
        2,              /* priority of the task */
        &task_Uart      /* Task handle to keep track of created task */
    );

    xTaskCreate(
        vTask_PCA9538,  /* Task function. */
        "TaskPCA9538",  /* name of task. */
        8192,           /* Stack size (bytes): increased from 4096 – Wire I2C needs headroom */
        NULL,           /* parameter of the task */
        2,              /* priority of the task */
        NULL            /* Task handle */
    );
}

/**************************** */
/*** PROGRAMM             *** */
/**************************** */

// file: 0.h --> w: 126   h: 160
// file: 1.h --> w: 103   h: 160
// file: 2.h --> w: 112   h: 160
// file: 3.h --> w: 105   h: 160
// file: 3_neon.h --> w: 104   h: 160
// file: 4.h --> w: 123   h: 160
// file: 5.h --> w: 109   h: 160
// file: 7_neon.h --> w: 128   h: 144

void loop()
{

}

int8_t state_tft_info = 0;

void tCodeDisplay(void *pvParameters)
{
    Serial.printf("Task Display running on core %02d\n", xPortGetCoreID());
    int day = 1, month = 1, year = 124;
    uint8_t hour = 0, min = 0, sec = 0;
    state_tft_info = 0;

    // Boot-Animation: Modus aus NVS lesen (vor WebServer-Task, eigener Prefs-Block)
    {
        Preferences animPrefs;
        animPrefs.begin("nixie", true);  // read-only
        if (animPrefs.isKey("anim_m"))
            systemConfig.bootAnimMode = animPrefs.getUChar("anim_m");
        animPrefs.end();
    }

    // Animation sofort starten – mindestens 10 s, warten bis bootInitDone (max. 60 s)
    if (systemConfig.bootAnimMode != 0) {
        runBootAnimation(systemConfig.bootAnimMode, 60000);
    } else {
        // Kein Anim-Modus: trotzdem 10 s warten oder bis Init fertig
        unsigned long waitStart = millis();
        while (!bootInitDone || (millis() - waitStart < 10000))
            vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Load SD background image now that NVS settings have been applied by WebServer task
    loadBgImage();

    // Direkt in den Hauptloop – kein Delay, kein System-Overlay nach Animation

    // Anzeigestate und Dirty-Flags
    bool showWeather = false;
    unsigned long lastInfoSwitch = millis();
    unsigned long bgRetryMs = 0;  // Zeitstempel letzter loadBgImage()-Versuch

    // "Zuletzt gezeichnet"-Werte zum Dirty-Check
    uint8_t  drawn_hour = 0xFF, drawn_min = 0xFF;
    int      drawn_day = -1, drawn_month = -1, drawn_year = -1;
    bool     drawn_showWeather = false;
    unsigned long drawn_weatherFetch = 0;  // lastFetchMs beim letzten Zeichnen
    int      drawn_timestatus = -1;

    for (;;)
    {
        int timestatus = getLocTime(&day, &month, &year, &hour, &min, &sec);

        fetchWeather();

        // Wechsel zwischen Kalender und Wetter
        uint32_t infoMs = (uint32_t)max((int)systemConfig.displayIntervalSec, 10) * 1000UL;
        bool switchNow = (millis() - lastInfoSwitch >= infoMs);
        if (switchNow) {
            if (systemConfig.weatherEnabled && weatherData.valid)
                showWeather = !showWeather;
            else
                showWeather = false;
            lastInfoSwitch = millis();
        }

        // Dirty-Flags
        bool clockDirty = (hour != drawn_hour || min != drawn_min || timestatus != drawn_timestatus);
        bool infoDirty  = switchNow
                       || (showWeather != drawn_showWeather)
                       || (showWeather && weatherData.lastFetchMs != drawn_weatherFetch)
                       || (!showWeather && (day != drawn_day || month != drawn_month || year != drawn_year));

        if (clockDirty || infoDirty) {
            xSemaphoreTakeRecursive(xSpiMutex, portMAX_DELAY);

            if (clockDirty) {
                drawTime(hour, min, sec, timestatus);
                drawn_hour      = hour;
                drawn_min       = min;
                drawn_timestatus = timestatus;
            }

            if (infoDirty && timestatus == 0) {
                if (showWeather && systemConfig.weatherEnabled && weatherData.valid) {
                    handle_cs(0, LOW);
                    drawWeatherView(tft);
                    handle_cs(0, HIGH);
                    drawn_weatherFetch = weatherData.lastFetchMs;
                } else if (systemConfig.calendarEnabled) {
                    drawCalendar(day, month + 1, year + 1900, 1);
                    drawn_day   = day;
                    drawn_month = month;
                    drawn_year  = year;
                }
                drawn_showWeather = showWeather;
            }

            xSemaphoreGiveRecursive(xSpiMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        // Hintergrundbild nachladen falls beim Startup fehlgeschlagen
        if (systemConfig.backgroundEnabled && systemConfig.sdCardDetected
            && bgImageW == 0 && systemConfig.bgImagePath[0] != '\0') {
            unsigned long now = millis();
            if (bgRetryMs == 0 || (now - bgRetryMs) >= 30000UL) {
                bgRetryMs = now;
                Serial.println("[BGIMG] Retry...");
                loadBgImage();
            }
        }
    }
}