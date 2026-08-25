#pragma once

#include <stdbool.h>
#include <stdint.h>

void wifi_init_sta(void);
bool wifi_is_connected(void);
int wifi_get_rssi(void);