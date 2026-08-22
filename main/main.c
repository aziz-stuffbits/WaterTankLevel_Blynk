#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "blynk.h"
#include "modbus_slave.h"
#include "wifi.h"

#define TANK_HEIGHT_MM_DEFAULT 1500
#define TANK_CAPACITY_L_DEFAULT 1000

void app_main(void)
{
    printf(
        "Water Tank Monitor Starting!\n"
    );

    esp_err_t ret =
        nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ret =
            nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    printf(
        "Wi-Fi Connected Successfully!\n"
    );

    ESP_ERROR_CHECK(modbus_slave_init());

    int tank_level = 10;

    while (1)
    {
        uint16_t water_mm =
            (uint16_t)(((uint32_t)tank_level *
                        TANK_HEIGHT_MM_DEFAULT) / 100);
        uint16_t distance_mm =
            TANK_HEIGHT_MM_DEFAULT - water_mm;
        uint16_t volume_l =
            (uint16_t)(((uint32_t)tank_level *
                        TANK_CAPACITY_L_DEFAULT) / 100);

        modbus_slave_set_tank(
            distance_mm,
            (uint16_t)tank_level,
            volume_l,
            0
        );

        // blynk_send_tank_level(tank_level);

        tank_level += 10;

        if (tank_level > 100)
        {
            tank_level = 10;
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
