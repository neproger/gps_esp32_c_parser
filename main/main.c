#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps_module.h"
#include "esp_log.h"
void some_other_task(void *pvParameters)
{
    // Переменные для хранения предыдущих значений координат
    float prev_latitude = 0.0f;
    float prev_longitude = 0.0f;

    while (1) {
        // Получаем доступ к глобальным переменным через семафор
        if (xSemaphoreTake(gps_data_mutex, portMAX_DELAY) == pdTRUE) {
            // Проверяем, изменились ли координаты
            if (latitude != prev_latitude || longitude != prev_longitude) {
                ESP_LOGI("GPS_DATA", "%.6f, %.6f", latitude, longitude);
                
                // Обновляем предыдущие значения
                prev_latitude = latitude;
                prev_longitude = longitude;
            }
            
            // Освобождаем семафор
            xSemaphoreGive(gps_data_mutex);
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Периодическая проверка данных
    }
}
void app_main(void)
{
    // Запуск задачи для GPS в отдельном потоке
    xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);

    // Создание другой задачи для работы с координатами
    xTaskCreate(some_other_task, "some_other_task", 2048, NULL, 5, NULL);
}
