
#ifndef __POOL_CONTROL_H__
#define __POOL_CONTROL_H__

void pool_controll_init(esp_mqtt_client_handle_t mqttClientParam);
void startRemoteFilter(int periodMs);
void startRemoteHidro(int periodMs);

#endif