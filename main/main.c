#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps_module.h"
#include "esp_log.h"
void some_other_task(void *pvParameters)
{
    while (1) {
        // Получаем доступ к глобальным переменным через семафор
        if (xSemaphoreTake(gps_data_mutex, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("GPS_DATA", "%.6f, %.6f", latitude, longitude); 
            // Освобождаем семафор
            xSemaphoreGive(gps_data_mutex);
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Периодический вывод данных
    }
}
void app_main(void)
{
    // Запуск задачи для GPS в отдельном потоке
    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);

    // Создание другой задачи для работы с координатами
    xTaskCreate(some_other_task, "some_other_task", 2048, NULL, 5, NULL);
}
