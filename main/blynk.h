#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define BLYNK_ERR_OK 0
#define BLYNK_ERR_UNO_LOST 1
#define BLYNK_ERR_SENSOR 2

#define BLYNK_V7_NONE 0
#define BLYNK_V7_LOW_WATER 1
#define BLYNK_V7_FULL_TANK 2

void blynk_init(void);
bool blynk_mqtt_connected(void);

esp_err_t blynk_send_status(int level,
                            int error_code,
                            int volume_l,
                            int distance_mm,
                            int rssi,
                            uint32_t update_counter,
                            uint32_t uptime_minutes,
                            int event_code);
