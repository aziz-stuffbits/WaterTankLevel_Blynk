#pragma once

#include "esp_err.h"

#define BLYNK_ERR_OK 0
#define BLYNK_ERR_UNO_LOST 1
#define BLYNK_ERR_SENSOR 2

esp_err_t blynk_send_tank_level(int level);
esp_err_t blynk_send_status(int level, int error_code);
