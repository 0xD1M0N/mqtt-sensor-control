#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dht.h"
#include "esp_log.h"

#define DHT_PIN GPIO_NUM_5 // Пін, до якого підключено DHT11
#define SENSOR_TYPE DHT_TYPE_DHT11 // Тип сенсора

static const char *TAG = "DHT11_APP";

void dht_task(void *pvParameter)
{
    while (1)
    {
        float temperature = 0.0;
        float humidity = 0.0;

        esp_err_t result = dht_read_float_data(SENSOR_TYPE, DHT_PIN, &humidity, &temperature);

        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);
        }
        else
        {
            ESP_LOGE(TAG, "Error reading data from DHT sensor: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Очікування 2 секунди між вимірюваннями
    }
}

void app_main(void)
{
    // Ініціалізація GPIO
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    // Запуск задачі для зчитування даних із сенсора
    xTaskCreate(dht_task, "dht_task", 2048, NULL, 5, NULL);
}
