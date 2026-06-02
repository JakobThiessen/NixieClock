#ifndef VTASK_WEATHER_H
#define VTASK_WEATHER_H

#include <Adafruit_ST7735.h>

void fetchWeather();
void drawWeatherView(Adafruit_ST7735 &tft);

#endif
