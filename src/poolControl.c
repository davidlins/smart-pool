#include "stdbool.h"
#include "time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "mqtt_client.h"
#include "temperature.h"

static const char *TAG_CONTROLL = "Smart Pool - Controll";

#define LIGADO 0
#define DESLIGADO 1

//  Reles GPIO_NUM
#define releBombaFiltro GPIO_NUM_23
#define releBombaHidro GPIO_NUM_22
#define releOzonio GPIO_NUM_21

#define GPIO_OUTPUT_PIN_SEL ((1ULL << releBombaFiltro) | (1ULL << releBombaHidro) | (1ULL << releOzonio))

#define botaoBombaFiltroManual GPIO_NUM_25
#define botaoBombaFiltroAutomatico GPIO_NUM_26

#define botaoBombaHidroManual GPIO_NUM_13
#define botaoBombaHidroAutomatico GPIO_NUM_14

#define GPIO_INPUT_PIN_SEL ((1ULL << botaoBombaFiltroManual) | (1ULL << botaoBombaFiltroAutomatico) | (1ULL << botaoBombaHidroManual) | (1ULL << botaoBombaHidroAutomatico))

bool remoteFilter = false;
bool remoteHidro = false;

int startFilter = 0;
int filteringTime = 0;

/* MQTT */

#define POOL_TOPIC "pool-status/state"

esp_mqtt_client_handle_t mqttClient;

/* tasks */
TimerHandle_t xTimerRemoteFilter;
TimerHandle_t xTimerRemoteHidro;

void callBackRemoteFilter(TimerHandle_t xTimer);
void callBackRemoteHidro(TimerHandle_t xTimer);

int getHours()
{

  time_t now = 0;
  time(&now);
  struct tm timeinfo = {0};
  localtime_r(&now, &timeinfo);
  return timeinfo.tm_hour;
}

int controleBombaFiltro()
{
  int status = 0;
  if (gpio_get_level(botaoBombaFiltroManual) == LIGADO)
  {
    status = 1;
  }
  else if (gpio_get_level(botaoBombaFiltroAutomatico) == LIGADO)
  {
    status = 2;
  }

  return status;
}

int controleBombaHidro()
{
  int status = 0;
  if (gpio_get_level(botaoBombaHidroManual) == LIGADO)
  {
    status = 1;
  }
  else if (gpio_get_level(botaoBombaHidroAutomatico) == LIGADO)
  {
    status = 2;
  }

  return status;
}

bool bombaFiltroLigada()
{
  return gpio_get_level(releBombaFiltro) == LIGADO;
}

bool bombaHidroLigada()
{
  return gpio_get_level(releBombaHidro) == LIGADO;
}

bool ozonioLigado()
{
  return gpio_get_level(releOzonio) == LIGADO;
}

void checkBomba()
{
  uint8_t releStatus = DESLIGADO;
  int status = controleBombaFiltro();
  if (status > 0)
  {
    if (status == 1)
    {
      releStatus = LIGADO;
    }
    else
    {
      if (remoteFilter)
      {
        releStatus = LIGADO;
      }
      else
      {
        int horas = getHours();
        releStatus = (horas >= startFilter && horas < (startFilter + filteringTime)) ? LIGADO : DESLIGADO;
      }
    }
  }
  gpio_set_level(releBombaFiltro, releStatus);
}

void checkHidro()
{
  uint8_t releStatus = DESLIGADO;
  int status = controleBombaHidro();
  if (status > 0)
  {
    if (status == 1 || remoteHidro)
    {
      releStatus = LIGADO;
    }
  }

  gpio_set_level(releBombaHidro, releStatus);
}

void checkOzonio()
{
  gpio_set_level(releOzonio, (bombaFiltroLigada()) ? LIGADO : DESLIGADO);
}

void pollControllTask(void *pvParameters)
{

  while (true)
  {
    ESP_LOGI(TAG_CONTROLL, "====================================================== ");

    checkBomba();
    checkHidro();
    checkOzonio();

    char *statusFiltro = (bombaFiltroLigada()) ? "ON" : "OFF";
    char *statusHidro = (bombaHidroLigada()) ? "ON" : "OFF";
    char *statusOzonio = (ozonioLigado()) ? "ON" : "OFF";

    char *statusControleBombaFiltro = "OFF";
    int statusControle = controleBombaFiltro();
    if (statusControle > 0)
    {
      statusControleBombaFiltro = (statusControle == 1) ? "Manual" : "Auto";
    }

    char *statusControleBombaHidro = "OFF";
    statusControle = controleBombaHidro();
    if (statusControle > 0)
    {
      statusControleBombaHidro = (statusControle == 1) ? "Manual" : "Auto";
    }

    ESP_LOGI(TAG_CONTROLL, "Filtro Ligado:  %s", statusFiltro);
    ESP_LOGI(TAG_CONTROLL, "Hidro Ligado:  %s", statusHidro);
    ESP_LOGI(TAG_CONTROLL, "Ozonio Ligado:  %s", statusOzonio);
    ESP_LOGI(TAG_CONTROLL, "Filtro Command:  %s", statusControleBombaFiltro);
    ESP_LOGI(TAG_CONTROLL, "Hidro Command:  %s", statusControleBombaHidro);

    char message[150];
    sprintf(message, "{\"poolFilter\": \"%s\",\"poolFilterCommand\": \"%s\",\"poolHidro\": \"%s\",\"poolHidroCommand\":\"%s\", \"poolOzonio\":\"%s\"}", statusFiltro, statusControleBombaFiltro, statusHidro, statusControleBombaHidro, statusOzonio);
    int msg_id = esp_mqtt_client_publish(mqttClient, POOL_TOPIC, message, 0, 1, 0);
    ESP_LOGI(TAG_CONTROLL, "sent publish successful, msg_id=%d", msg_id);

    ESP_LOGI(TAG_CONTROLL, "======================================================\n");

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void pool_controll_init(esp_mqtt_client_handle_t mqttClientParam)
{

  mqttClient = mqttClientParam;

  ESP_LOGI(TAG_CONTROLL, "Init Temperature");
  temperature_init(mqttClient);

  gpio_config_t io_conf;
  io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
  io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config(&io_conf);

  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

  gpio_set_level(releBombaFiltro, DESLIGADO);
  gpio_set_level(releBombaHidro, DESLIGADO);
  gpio_set_level(releOzonio, DESLIGADO);

  ESP_LOGI(TAG_CONTROLL, "startFilter: %d", startFilter);
  ESP_LOGI(TAG_CONTROLL, "filteringTime: %d", filteringTime);
  ESP_LOGI(TAG_CONTROLL, ">>>>>>>>>>>>>>>>>>>");

  startFilter = CONFIG_FILTER_POOL_START_HOUR;
  filteringTime = CONFIG_FILTERING_POOL_TIME;

  ESP_LOGI(TAG_CONTROLL, "startFilter: %d", startFilter);
  ESP_LOGI(TAG_CONTROLL, "filteringTime: %d", filteringTime);

  xTaskCreate(pollControllTask, "pollControllTask", configMINIMAL_STACK_SIZE + 10240, NULL, 5, NULL);
}

void callBackRemoteFilter(TimerHandle_t xTimer)
{
  ESP_LOGI(TAG_CONTROLL, "callBackRemoteFilter");
  remoteFilter = false;
}

void startRemoteFilter(int periodMs)
{

  ESP_LOGI(TAG_CONTROLL, "Called startRemoteFilter");

  if (xTimerRemoteFilter != NULL)
  {
    xTimerStop(xTimerRemoteFilter, 0);
    xTimerDelete(xTimerRemoteFilter, 0);
    xTimerRemoteFilter = NULL;
  }

  xTimerRemoteFilter = xTimerCreate("remoteFilterTimer", pdMS_TO_TICKS(periodMs), pdFALSE, 0, callBackRemoteFilter);
  if (xTimerRemoteFilter != NULL)
  {
    remoteFilter = true;
    xTimerStart(xTimerRemoteFilter, 0);
  }
}

void callBackRemoteHidro(TimerHandle_t xTimer)
{
  ESP_LOGI(TAG_CONTROLL, "callBackRemoteHidro");
  remoteHidro = false;
}

void startRemoteHidro(int periodMs)
{

  ESP_LOGI(TAG_CONTROLL, "Called startRemoteHidro");

  if (xTimerRemoteFilter != NULL)
  {
    xTimerStop(xTimerRemoteHidro, 0);
    xTimerDelete(xTimerRemoteHidro, 0);
    xTimerRemoteHidro = NULL;
  }

  xTimerRemoteHidro = xTimerCreate("remoteHidroTime", pdMS_TO_TICKS(periodMs), pdFALSE, 0, callBackRemoteHidro);
  if (xTimerRemoteHidro != NULL)
  {
    remoteHidro = true;
    xTimerStart(xTimerRemoteHidro, 0);
  }
}

