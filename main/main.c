#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "blynk.h"
#include "wifi.h"


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

    int tank_level = 10;

    while (1)
    {
        blynk_send_tank_level(tank_level);

        tank_level += 10;

        if (tank_level > 100)
        {
            tank_level = 10;
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
