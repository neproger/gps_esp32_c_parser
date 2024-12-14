#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Глобальные переменные для широты и долготы
extern float latitude;
extern float longitude;

// Семафор для синхронизации доступа
extern SemaphoreHandle_t gps_data_mutex;

// Прототипы функций
void gps_task(void *pvParameters);

#endif // GPS_MODULE_H
