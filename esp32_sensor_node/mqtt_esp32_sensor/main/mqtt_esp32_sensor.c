#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dht.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_ping.h"

#define STA_SSID ""
#define STA_PASSWORD ""

#define DHT_PIN GPIO_NUM_5
#define SENSOR_TYPE DHT_TYPE_DHT11

static const char *TAG = "ESP32_APP";
static const char *DHT_TAG = "DHT11";
static const char *WIFI_TAG = "WiFi";
static const char *MQTT_TAG = "MQTT";

static esp_mqtt_client_handle_t mqtt_client = NULL;

static void wifi_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(WIFI_TAG, "WiFi STA started, connecting to %s", STA_SSID);
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(WIFI_TAG, "Connected to WiFi: %s", STA_SSID);
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGE(WIFI_TAG, "Disconnected from WiFi, reconnecting...");
                esp_wifi_connect();
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
                break;
            }
            default:
                break;
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGE(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(MQTT_TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(MQTT_TAG, "Transport error: esp-tls error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(MQTT_TAG, "Socket errno: %d", event->error_handle->esp_transport_sock_errno);
            }
            break;

        default:
            ESP_LOGI(MQTT_TAG, "Other MQTT event id: %d", event->event_id);
            break;
    }
}

void dht_task(void *pvParameter) {
    while (1) {
        float temperature = 0.0;
        float humidity = 0.0;
        esp_err_t result = dht_read_float_data(SENSOR_TYPE, DHT_PIN, &humidity, &temperature);

        if (result == ESP_OK) {
            ESP_LOGI(DHT_TAG, "Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);

            char message[128];
            snprintf(message, sizeof(message), "{\"temperature\":%.1f,\"humidity\":%.1f}", temperature, humidity);

            if (mqtt_client != NULL) {
                esp_mqtt_client_publish(mqtt_client, "esp32/sensor", message, 0, 1, 0);
                ESP_LOGI(MQTT_TAG, "Published: %s", message);
            }
        } else {
            ESP_LOGE(DHT_TAG, "Error reading data from DHT sensor: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "NVS initialized");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t sta_config = {
        .sta = {
            .ssid = STA_SSID,
            .password = STA_PASSWORD
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(WIFI_TAG, "WiFi initialized");

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "",
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
    ESP_LOGI(MQTT_TAG, "MQTT client started");

    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    xTaskCreate(dht_task, "dht_task", 4096, NULL, 5, NULL);
}
