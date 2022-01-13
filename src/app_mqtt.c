#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"

/* MQTT */
#define MQTT_BROKER_URL CONFIG_MQTT_BROKER_URL
#define MQTT_BROKER_USER CONFIG_MQTT_BROKER_USER
#define MQTT_BROKER_PASSWORD CONFIG_MQTT_BROKER_PASSWORD

esp_mqtt_client_handle_t app_mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER_URL,
        .username = MQTT_BROKER_USER,
        .password = MQTT_BROKER_PASSWORD,
        .disable_auto_reconnect = false};

    esp_mqtt_client_handle_t mqttClient = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_start(mqttClient);
    return  mqttClient;   
}
