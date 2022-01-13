#include "math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "mqtt_client.h"

esp_adc_cal_characteristics_t adc_cal; // Estrutura que contem as informacoes para calibracao

// Conexão do termistor
const int pinTermistorPiscina = 34;
const int pinTermistorSolar = 35;

// Parâmetros do termistor
const double beta = 3872.0;
const double r0 = 10000.0;
const double t0 = 273.0 + 25.0;
const double vcc = 3.3;
const double R = 10000.0;

double rx;

/* Mqtt */
#define POOL_TEPERATURE_TOPIC "pool-temperature/state"
esp_mqtt_client_handle_t mqttClient;

/* Taks Handler */
TimerHandle_t xTimerTaskLoadTemperature;

static const char *TAG_TEMPERATURE = "Smart Pool - Temperature";

void callBackTimerLoadTemperature(TimerHandle_t xTimer);

void temperature_init(esp_mqtt_client_handle_t mqttClientParam)
{

    mqttClient = mqttClientParam;

    rx = r0 * exp(-beta / t0);

    adc_bits_width_t adc_bits_width = ADC_WIDTH_BIT_10;
    adc_atten_t adc_atten = ADC_ATTEN_DB_11;
    adc1_config_width(adc_bits_width);                    // Configura a resolucao
    adc1_config_channel_atten(ADC1_CHANNEL_6, adc_atten); // Configura a atenuacao

    esp_adc_cal_value_t adc_type = esp_adc_cal_characterize(ADC_UNIT_1, adc_atten, adc_bits_width, 1100, &adc_cal); // Inicializa a estrutura de calibracao

    if (adc_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        ESP_LOGI(TAG_TEMPERATURE, "Vref eFuse encontrado: %umV", adc_cal.vref);
    }
    else if (adc_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        ESP_LOGI(TAG_TEMPERATURE, "Two Point eFuse encontrado");
    }
    else
    {
        ESP_LOGW(TAG_TEMPERATURE, "Nada encontrado, utilizando Vref padrao: %umV", adc_cal.vref);
    }

    xTimerTaskLoadTemperature = xTimerCreate("TIMER_LOAD_TEMPERATURE", pdMS_TO_TICKS(3000), pdTRUE, 0, callBackTimerLoadTemperature);
    if (xTimerTaskLoadTemperature != NULL)
    {
        xTimerStart(xTimerTaskLoadTemperature, 0);
    }
}

double getPoolTemperature()
{

    /*
        Obtem a leitura RAW do ADC para depois ser utilizada pela API de calibracao

        Media simples de 100 leituras intervaladas com 30us
    */
    uint32_t voltage = 0;
    for (int i = 0; i < 100; i++)
    {
        voltage += adc1_get_raw(ADC1_CHANNEL_6); // Obtem o valor RAW do ADC
        ets_delay_us(30);
    }

    double v = (double)esp_adc_cal_raw_to_voltage(voltage / 100, &adc_cal) / 1000; // Converte e calibra o valor lido (RAW) para V
    ESP_LOGI(TAG_TEMPERATURE, "Read mV: %0.03f", v);                               // Mostra a leitura calibrada no Serial Monitor

    // double v = ((vcc * voltage) / (100 * 4096.0)) * 1.05;
    // ESP_LOGI(TAG_TEMPERATURE, "Read mV sem calibracao: %0.03f", v); // Mostra a leitura calibrada no Serial Monitor

    // Determina a resistência do termistor
    double rt = (vcc * R) / v - R;
    ESP_LOGI(TAG_TEMPERATURE, "RT: %0.05f", rt);

    // Calcula a temperatura
    double t = (beta / log(rt / rx)) - 273.0;
    ESP_LOGI(TAG_TEMPERATURE, "T: %0.03f", t);

    return t;
}

void callBackTimerLoadTemperature(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG_TEMPERATURE, "==========================================");

    double temperature = getPoolTemperature();
    ESP_LOGI(TAG_TEMPERATURE, "Temperature: %0.03f", temperature);

    char s[10];
    sprintf(s, "%0.01f", temperature);
    int msg_id = esp_mqtt_client_publish(mqttClient, POOL_TEPERATURE_TOPIC, s, 0, 1, 0);
    ESP_LOGI(TAG_TEMPERATURE, "sent publish successful, msg_id=%d", msg_id);

    ESP_LOGI(TAG_TEMPERATURE, "==========================================\n");
}
