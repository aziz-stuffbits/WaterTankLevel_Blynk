#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define BLYNK_V7_NONE 0
#define BLYNK_V7_LOW_WATER 1
#define BLYNK_V7_FULL_TANK 2

/* MQTT topics use Datastream Name, not the pin. Must match the Blynk template. */
#define BLYNK_DS_LEVEL "Tank Level"
#define BLYNK_DS_VOLUME "Volume"
#define BLYNK_DS_DISTANCE "Distance"
#define BLYNK_DS_RSSI "Wi-Fi RSSI"
#define BLYNK_DS_COUNTER "Update Counter"
#define BLYNK_DS_UPTIME "Device Uptime"
#define BLYNK_DS_ALARM "Alarm"

void blynk_init(void);
bool blynk_mqtt_connected(void);

esp_err_t blynk_send_status(int level,
                            int volume_l,
                            int distance_mm,
                            int rssi,
                            uint32_t update_counter,
                            uint32_t uptime_minutes,
                            int event_code);
