#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#include "app_wifi.h"
#include "app_sntp.h"
#include "app_mqtt.h"
#include "app_webserver.h"
#include "poolControl.h"

static const char *TAG = "Smart Pool";


void app_main()
{
    ESP_LOGI(TAG, "Initialize NVS");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Init wifi in sta mode");
    wifi_init_sta();

    ESP_LOGI(TAG, "Init SNTP");
    app_sntp_init();

    ESP_LOGI(TAG, "Init Mqtt");
    esp_mqtt_client_handle_t mqttClient = app_mqtt_start();

    ESP_LOGI(TAG, "Init Pool Controll");
    pool_controll_init(mqttClient);

    ESP_LOGI(TAG, "Init Webserver");
    app_webserver_start();

    
}