#pragma once

#include <stdint.h>

/* Physical tank. Sensor looks down from the lid. */
#define TANK_OVERALL_HEIGHT_MM 1300
#define TANK_OVERFLOW_WATER_MM 1100
#define TANK_OUTLET_WATER_MM 150
#define TANK_CAPACITY_L 1000

typedef struct
{
    uint16_t water_mm;
    uint16_t level_pct;
    uint16_t volume_l;
} tank_reading_t;

/*
 * distance_mm: sensor to water surface (from the UNO).
 * 0% / 0 L at the lowest outlet (150 mm).
 * 100% / 1000 L at the overflow (1100 mm).
 */
void tank_from_distance(uint16_t distance_mm, tank_reading_t *out);
