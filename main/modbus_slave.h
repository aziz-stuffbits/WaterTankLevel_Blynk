#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t modbus_slave_init(void);
void modbus_slave_set_tank(uint16_t distance_mm,
                           uint16_t level_pct,
                           uint16_t volume_l,
                           uint16_t status);
