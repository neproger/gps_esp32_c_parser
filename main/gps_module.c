#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "gps_module.h"

#define EX_UART_NUM UART_NUM_1
#define BUF_SIZE (1024)

// Глобальные переменные для широты и долготы
float latitude = 0.0f;
float longitude = 0.0f;

// Семафор для синхронизации доступа
SemaphoreHandle_t gps_data_mutex;

// Структура для хранения данных из строки GPGGA
typedef struct
{
    char time[11];
    char latitude[20];
    char lat_dir[3];
    char longitude[20];
    char lon_dir[3];
    int fix_quality;
    int num_satellites;
    float hdop;
    float altitude;
    float geoid_height;
} GPGGA_Data;

// Функция для форматированного вывода времени
void print_time(const char *time_str)
{
    int hours, minutes, seconds;

    if (strlen(time_str) >= 6)
    {
        // Извлекаем часы, минуты и секунды
        hours = (time_str[0] - '0') * 10 + (time_str[1] - '0');
        minutes = (time_str[2] - '0') * 10 + (time_str[3] - '0');
        seconds = (time_str[4] - '0') * 10 + (time_str[5] - '0');

        // Выводим в формате ЧЧ : ММ : СС
        printf("Time: %02d:%02d:%02d\n", hours, minutes, seconds);
    }
    else
    {
        printf("Invalid time format\n");
    }
}
// Функция для парсинга строки GPGGA
int parse_gpgga(const char *gpgga_str, GPGGA_Data *data)
{
    if (!gpgga_str || !data)
        return -1; // Проверка на корректность входных данных

    char gpgga_copy[200];
    strncpy(gpgga_copy, gpgga_str, sizeof(gpgga_copy) - 1);
    gpgga_copy[sizeof(gpgga_copy) - 1] = '\0'; // Защита от переполнения строки

    char *token = strtok(gpgga_copy, ",");

    // Проверка на правильный тип сообщения
    if (!token || strcmp(token, "$GPGGA") != 0)
    {
        return -2; // Неверный формат строки
    }

    // Парсинг остальных данных
    token = strtok(NULL, ","); // UTC Time
    if (token)
        strncpy(data->time, token, sizeof(data->time) - 1);

    token = strtok(NULL, ","); // Latitude
    if (token && strlen(token) > 0)
        strncpy(data->latitude, token, sizeof(data->latitude) - 1);

    token = strtok(NULL, ","); // Latitude direction (N/S)
    if (token && strlen(token) > 0)
        strncpy(data->lat_dir, token, sizeof(data->lat_dir) - 1);

    token = strtok(NULL, ","); // Longitude
    if (token && strlen(token) > 0)
        strncpy(data->longitude, token, sizeof(data->longitude) - 1);

    token = strtok(NULL, ","); // Longitude direction (E/W)
    if (token && strlen(token) > 0)
        strncpy(data->lon_dir, token, sizeof(data->lon_dir) - 1);

    token = strtok(NULL, ","); // Fix quality
    data->fix_quality = token ? atoi(token) : 0;

    token = strtok(NULL, ","); // Number of satellites
    data->num_satellites = token ? atoi(token) : 0;

    token = strtok(NULL, ","); // HDOP
    data->hdop = token ? atof(token) : 0.0f;

    token = strtok(NULL, ","); // Altitude
    data->altitude = token ? atof(token) : 0.0f;

    token = strtok(NULL, ","); // Altitude units (ignored)

    token = strtok(NULL, ","); // Geoid height
    data->geoid_height = token ? atof(token) : 0.0f;

    return 0; // Успешный парсинг
}

// Функция для преобразования координат в десятичный формат
void convert_to_decimal(const char *coord, float *decimal)
{
    int degrees = (int)(atof(coord) / 100);
    float minutes = atof(coord) - (degrees * 100);
    *decimal = degrees + (minutes / 60.0f);
}

// Задача для работы с GPS
void gps_task(void *pvParameters)
{
    // Инициализация семафора
    gps_data_mutex = xSemaphoreCreateMutex();

    if (gps_data_mutex == NULL)
    {
        // Не удалось создать семафор, обработать ошибку
        printf("Failed to create semaphore!\n");
        vTaskDelete(NULL);
        return;
    }

    // Настройка UART
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_driver_install(EX_UART_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    uart_set_pin(EX_UART_NUM, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    uint8_t data[BUF_SIZE];
    while (1)
    {
        int len = uart_read_bytes(EX_UART_NUM, data, BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            data[len] = '\0'; // Завершение строки
            char *line = strtok((char *)data, "\r\n");
            while (line)
            {
                if (strstr(line, "$GPGGA") != NULL)
                {
                    GPGGA_Data parsed_data;
                    parse_gpgga(line, &parsed_data);
                    printf("________________\n");
                    if (parsed_data.fix_quality > 0)
                    {
                        // Расчет высоты над уровнем моря
                        float sea_level_altitude = parsed_data.altitude - parsed_data.geoid_height;

                        // Преобразование широты и долготы в десятичный формат
                        float new_latitude, new_longitude;
                        convert_to_decimal(parsed_data.latitude, &new_latitude);
                        convert_to_decimal(parsed_data.longitude, &new_longitude);

                        // Корректируем знак широты и долготы
                        if (strcmp(parsed_data.lat_dir, "S") == 0)
                        {
                            new_latitude = -new_latitude;
                        }
                        if (strcmp(parsed_data.lon_dir, "W") == 0)
                        {
                            new_longitude = -new_longitude;
                        }
                        // Форматированный вывод данных
                        print_time(parsed_data.time);
                        printf("Latitude: %.6f° %s\n", new_latitude, parsed_data.lat_dir);
                        printf("Longitude: %.6f° %s\n", new_longitude, parsed_data.lon_dir);
                        printf("Fix Quality: %d\n", parsed_data.fix_quality);
                        printf("Satellites: %d\n", parsed_data.num_satellites);
                        printf("HDOP: %.2f\n", parsed_data.hdop);
                        printf("Sea Level Altitude: %.1f m\n", sea_level_altitude);

                        if (xSemaphoreTake(gps_data_mutex, portMAX_DELAY) == pdTRUE)
                        {
                            latitude = new_latitude;
                            longitude = new_longitude;

                            // Освобождаем семафор после обновления
                            xSemaphoreGive(gps_data_mutex);
                        }
                    }
                    else
                    {
                        printf("HDOP: %.2f\n", parsed_data.hdop);
                    }
                }
                line = strtok(NULL, "\r\n");
            }
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
