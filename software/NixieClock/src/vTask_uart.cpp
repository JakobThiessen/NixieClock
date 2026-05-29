
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
        Serial.print("Task UART running: "); Serial.println( millis());
        /*
        vTaskList(ptrTaskList);
        Serial.println(F("**********************************"));
        Serial.println(F("Task  State   Prio    Stack    Num")); 
        Serial.println(F("**********************************"));
        Serial.print(ptrTaskList);
        Serial.println(F("**********************************"));
        */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}