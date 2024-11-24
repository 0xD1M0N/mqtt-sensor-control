#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dht.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define DHT_PIN GPIO_NUM_5
#define SENSOR_TYPE DHT_TYPE_DHT11

#define WIFI_SSID ""
#define WIFI_PASS ""

static const char *DHT_TAG = "DHT11";
static const char *WIFI_TAG = "WIFI";


void wifi_init_sta(void)
{
    esp_log_level_set(WIFI_TAG, ESP_LOG_INFO);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "Connecting to WiFi");
}


void dht_task(void *pvParameter)
{
    while (1)
    {
        float temperature = 0.0;
        float humidity = 0.0;

        esp_err_t result = dht_read_float_data(SENSOR_TYPE, DHT_PIN, &humidity, &temperature);

        if (result == ESP_OK)
        {
            ESP_LOGI(DHT_TAG, "Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);
        }
        else
        {
            ESP_LOGE(DHT_TAG, "Error reading data from DHT sensor: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    wifi_init_sta();
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    xTaskCreate(dht_task, "dht_task", 2048, NULL, 5, NULL);
}
