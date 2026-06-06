
#include <stdint.h>
#include <Arduino.h>
#include <FreeRTOS.h>

#include "vTask_uart.h"

char ptrTaskList[250];

void tCodeUart(void *pvParameters)
{
    Serial.print("Task UART running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}