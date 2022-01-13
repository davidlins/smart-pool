
#ifndef __TEMPERATURE_H__
#define __TEMPERATURE_H__

void temperature_init(esp_mqtt_client_handle_t mqttClientParam);
double getPoolTemperature();


#endif
