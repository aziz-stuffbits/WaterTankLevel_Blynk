#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "blynk.h"
#include "modbus_master.h"
#include "wifi.h"

#define BLYNK_SEND_PERIOD_MS 10000

static const char *TAG = "APP";

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

    ESP_ERROR_CHECK(modbus_master_init());

    while (1)
    {
        uint16_t level_pct = 0;

        if (modbus_master_get_tank(NULL, &level_pct, NULL, NULL))
        {
            if (level_pct > 100)
            {
                level_pct = 100;
            }

            blynk_send_tank_level((int)level_pct);
        }
        else
        {
            ESP_LOGW(TAG, "No UNO reading yet, skip Blynk");
        }

        vTaskDelay(pdMS_TO_TICKS(BLYNK_SEND_PERIOD_MS));
    }
}
