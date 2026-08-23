#include "tank.h"

#define TANK_USABLE_MM \
    (TANK_OVERFLOW_WATER_MM - TANK_OUTLET_WATER_MM)

void tank_from_distance(uint16_t distance_mm, tank_reading_t *out)
{
    uint16_t water_mm = 0;

    if (distance_mm < TANK_OVERALL_HEIGHT_MM)
    {
        water_mm = (uint16_t)(TANK_OVERALL_HEIGHT_MM - distance_mm);
    }

    if (water_mm > TANK_OVERFLOW_WATER_MM)
    {
        water_mm = TANK_OVERFLOW_WATER_MM;
    }

    uint16_t level_pct = 0;
    uint16_t volume_l = 0;

    if (water_mm > TANK_OUTLET_WATER_MM)
    {
        uint16_t usable_mm = (uint16_t)(water_mm - TANK_OUTLET_WATER_MM);
        level_pct = (uint16_t)(((uint32_t)usable_mm * 100UL) / TANK_USABLE_MM);
        volume_l = (uint16_t)(((uint32_t)usable_mm * TANK_CAPACITY_L) /
                              TANK_USABLE_MM);
        if (level_pct > 100)
        {
            level_pct = 100;
        }
        if (volume_l > TANK_CAPACITY_L)
        {
            volume_l = TANK_CAPACITY_L;
        }
    }

    out->water_mm = water_mm;
    out->level_pct = level_pct;
    out->volume_l = volume_l;
}
