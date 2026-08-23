#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t modbus_master_init(void);

bool modbus_master_get_tank(uint16_t *distance_mm,
                            uint16_t *level_pct,
                            uint16_t *volume_l,
                            uint16_t *status);
bool modbus_master_link_ok(void);
