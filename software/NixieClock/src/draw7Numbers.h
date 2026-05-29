#ifndef DRAW_7_NUMBERS_h
#define DRAW_7_NUMBERS_h

#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789

#ifdef __cplusplus
extern "C" {
#endif

/*
long n - The number to be displayed
int xLoc = The x location of the upper left corner of the number
int yLoc = The y location of the upper left corner of the number
int cS = The size of the number.
fC is the foreground color of the number
bC is the background color of the number (prevents having to clear previous space)
nD is the number of digit spaces to occupy (must include space for minus sign for numbers < 0).

// Example to draw the number 37 in four directions in four corners of the display
draw7Number(37,10,10,4, ILI9341_WHITE , ILI9341_BLACK,2);       //LEFT2RIGHT
draw7Number90(37,10,310,4, ILI9341_WHITE , ILI9341_BLACK,2);    //DOWN2UP
draw7Number180(37,230,310,4, ILI9341_WHITE , ILI9341_BLACK,2);  //RIGHT2LEFT
draw7Number270(37,230,10,4, ILI9341_WHITE , ILI9341_BLACK,2);   //UP2DOWN
*/

void draw7Number(long n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC, char nD, Adafruit_ST77xx *tft);
void draw7Number90(long n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC, char nD, Adafruit_ST77xx *tft);
void draw7Number180(long n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC, char nD, Adafruit_ST77xx *tft);
void draw7Number270(long n, unsigned int xLoc, unsigned int yLoc, char cS, unsigned int fC, unsigned int bC, char nD, Adafruit_ST77xx *tft);

#ifdef __cplusplus
}
#endif

#endif // endif DRAW_7_NUMBERS_h