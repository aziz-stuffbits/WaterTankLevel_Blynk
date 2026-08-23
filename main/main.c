#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "blynk.h"
#include "modbus_master.h"
#include "status_led.h"
#include "tank.h"
#include "wifi.h"

#define APP_TICK_MS 500

static const char *TAG = "APP";

static void apply_status_led(bool wifi_up, bool uno_ever_ok, bool uno_ok)
{
    if (!wifi_up)
    {
        status_led_set(STATUS_LED_OFF);
        return;
    }

    if (uno_ok)
    {
        status_led_set(STATUS_LED_BLINK_200);
        return;
    }

    if (uno_ever_ok)
    {
        status_led_set(STATUS_LED_BLINK_1000);
        return;
    }

    status_led_set(STATUS_LED_SOLID);
}

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

    status_led_init();
    status_led_set(STATUS_LED_OFF);

    wifi_init_sta();

    if (wifi_is_connected())
    {
        status_led_set(STATUS_LED_SOLID);
        printf("Wi-Fi Connected Successfully!\n");
    }

    ESP_ERROR_CHECK(modbus_master_init());

    bool uno_ever_ok = false;
    bool have_last_level = false;
    int last_sent_level = -1;
    int last_sent_err = -1;

    while (1)
    {
        bool wifi_up = wifi_is_connected();
        bool uno_ok = modbus_master_link_ok();
        uint16_t distance_mm = 0;
        uint16_t uno_status = 0;
        tank_reading_t tank = { 0 };
        int err_code = BLYNK_ERR_UNO_LOST;

        if (uno_ok &&
            modbus_master_get_tank(&distance_mm, NULL, NULL, &uno_status))
        {
            uno_ever_ok = true;
            tank_from_distance(distance_mm, &tank);
            err_code = (uno_status == 0) ? BLYNK_ERR_OK : BLYNK_ERR_SENSOR;

            ESP_LOGI(TAG,
                     "dist=%u mm  water=%u mm  level=%u%%  vol=%u L  uno_status=%u",
                     (unsigned)distance_mm,
                     (unsigned)tank.water_mm,
                     (unsigned)tank.level_pct,
                     (unsigned)tank.volume_l,
                     (unsigned)uno_status);
        }

        apply_status_led(wifi_up, uno_ever_ok, uno_ok);

        if (wifi_up)
        {
            bool level_changed =
                have_last_level &&
                ((int)tank.level_pct != last_sent_level);
            bool first_ok =
                uno_ok && !have_last_level;
            bool err_changed = (err_code != last_sent_err);

            if (first_ok || (uno_ok && level_changed) || err_changed)
            {
                int level_to_send =
                    uno_ok ? (int)tank.level_pct
                           : (have_last_level ? last_sent_level : 0);

                if (blynk_send_status(level_to_send, err_code) == ESP_OK)
                {
                    last_sent_level = level_to_send;
                    last_sent_err = err_code;
                    if (uno_ok)
                    {
                        have_last_level = true;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_TICK_MS));
    }
}
