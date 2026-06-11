#ifndef vTask_WEB_SERVER_h
#define vTask_WEB_SERVER_h

#ifdef __cplusplus
extern "C" {
#endif

void tCodeServerTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// SD-Bus Hilfsfunktionen – für main.cpp (loadBgImage) und WebServer-Task
// sdAcquireBus(): Mutex nehmen, SPI.end(), SD_BUF_OE=LOW, SD reinit. Gibt false zurück bei Fehler.
// sdReleaseBus(): SD_BUF_OE=HIGH, SPI.begin(), Mutex freigeben.
bool sdAcquireBus();
void sdReleaseBus();
#endif

#endif