
// display
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"
#include "colors.h"


#define TFT1_DC 15
#define TFT1_CS 2
#define TFT2_DC 13
#define TFT2_CS 12
//SCL  -->  18
//SDA  -->  23

// Hardware SPI 
Adafruit_GC9A01A tft1(TFT1_CS, TFT1_DC); //upper glass
Adafruit_GC9A01A tft2(TFT2_CS, TFT2_DC); //lower glass

// RTC clock
#include <RTClib.h>
RTC_DS3231 rtc;


//int startx = 120, starty = 120; //center of display
int x, y, minute, hour;
int m, n; double h;
double color = SAND1; // set sand color


void setup() {
  Serial.begin(115200);
  delay(2000);
 
  // display setup
  tft1.begin();  tft2.begin();
  tft1.setRotation(2); tft2.setRotation(0);
  tft1.fillScreen(COLOR0); tft2.fillScreen(COLOR00);


  // setup RTC module
  if (! rtc.begin()) {
    Serial.println("Can't find RTC");
    Serial.flush();
    while (1);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  tft1.fillScreen(COLOR00);  // clear
  tft1.fillRect(0, 100, 240, 240, color); // full sand in glass 1
  tft2.fillScreen(COLOR0);  // no sand in glass 2

}
 
void loop() {
  
  // get time
  DateTime now = rtc.now();
  Serial.print(now.hour(), DEC); Serial.print(':');
  Serial.print(now.minute(), DEC); Serial.print(':');
  Serial.println(now.second(), DEC);
  
  minute = now.minute();
  hour = now.hour();
  Serial.print(hour);Serial.print(" : ");Serial.println(minute);

 minutes();
 delay(1000);

}


void minutes() {
    
  //tft1.fillScreen(COLOR00);       // clear
  tft1.fillRect(0, 100, 240, 240, color); // full sand in glass 1
  tft2.fillScreen(COLOR0);        // no sand in glass 2

  tft1.setCursor(104, 10);
  tft1.setTextColor(COLOR4);
  tft1.setTextSize(4);
  if (hour<10){
    tft1.print("0");
  }
  tft1.println(hour); // show hour

  //animated sand track
   for (int i = 0; i<250; i+=10) {
    tft2.fillRect(118, 0, 5, i, color);
    delay(10);
   }


  tft1.fillCircle(50, 50, 14, WHITE1); tft2.fillCircle(50, 45, 14, WHITE2); // add reflections
  
  // trickling sand
   m = map(minute, 0, 59, 0, 141);
     tft1.fillTriangle(0, m*0.9+68, 240, m*0.9+68, 120, m*1.2+90, COLOR0);     //upper glass

     n= map(m, 0, 141, 250, 75);
     tft2.fillTriangle(0, n*0.55+105, 240, n*0.55+105, 120, n, color);  //lower glass
     tft2.fillTriangle(108, n+7, 132, n+7, 120, n-6, color);  //sand track end

     delay(10);
   
   tft2.fillRect(118, 0, 5, n-4, COLOR00); //delete sand track
}



 
/*
int           n, i, i2,
                cx = tft1.width()  / 2 - 1,
                cy = tft1.height() / 2 - 1;
  tft1.drawLine(x1, y1, x2, y2, color);
  tft1.fillScreen(GC9A01A_BLACK);
  tft1.drawFastHLine(0, y, w, color1);
  tft1.drawFastVLine(x, 0, h, color2);
  tft1.drawRect(cx-i2, cy-i2, i, i, color);
  tft1.fillRect(cx-i2, cy-i2, i, i, color1);
  tft1.drawRect(cx-i2, cy-i2, i, i, color2);
  tft1.fillCircle(x, y, radius, color);
  tft1.drawCircle(x, y, radius, color);
  tft1.drawTriangle(
      cx    , cy - i, // peak
      cx - i, cy + i, // bottom left
      cx + i, cy + i, // bottom right
      tft1.color565(i, i, i));
  tft1.fillTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
      tft1.color565(0, i*10, i*10));
    t += micros() - start;
  tft1.drawTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
      tft1.color565(i*10, i*10, 0));
  tft1.drawRoundRect(cx-i2, cy-i2, i, i, i/8, tft1.color565(i, 0, 0));
  tft1.fillRoundRect(cx-i2, cy-i2, i, i, i/8, tft1.color565(0, i, 0));
  tft1.setCursor(120, 30);
  tft1.setTextColor(GC9A01A_RED);    
  tft1.setTextSize(3);
  tft1.println("Text");
  tft1.setRotation(rotation);   //1-4
  tft1.fillArc(0,0, 140,0, 0, hourangle, color);
 */


 /*
 // Old color definitions
GC9A01A_BLACK 0x0000       ///<   0,   0,   0
GC9A01A_WHITE 0xFFFF       ///< 255, 255, 255
GC9A01A_LIGHTGREY 0xC618   ///< 198, 195, 198
GC9A01A_DARKGREY 0x7BEF    ///< 123, 125, 123
GC9A01A_PINK 0xFC18        ///< 255, 130, 198
GC9A01A_PURPLE 0x780F      ///< 123,   0, 123
GC9A01A_MAGENTA 0xF81F     ///< 255,   0, 255
*/

 