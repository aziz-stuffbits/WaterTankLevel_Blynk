#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "blynk.h"
#include "modbus_master.h"
#include "status_led.h"
#include "tank.h"
#include "wifi.h"

#define APP_TICK_MS 500
#define BLYNK_UPDATE_PERIOD_MS 30000
#define LOW_WATER_TRIGGER_PERCENT 20
#define LOW_WATER_RESET_PERCENT 25
#define FULL_TANK_TRIGGER_PERCENT 98
#define FULL_TANK_RESET_PERCENT 90

static const char *TAG = "APP";
static uint32_t update_counter = 0;

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

static bool low_water_alarm_armed = true;
static bool low_water_logged_active = false;
static bool full_tank_alarm_armed = true;
static bool full_tank_logged_active = false;

static int process_tank_alarms(int tank_level)
{
    if ((tank_level < LOW_WATER_TRIGGER_PERCENT) &&
        low_water_alarm_armed)
    {
        if (!low_water_logged_active)
        {
            ESP_LOGW(TAG, "LOW WATER: level=%d%%", tank_level);
            low_water_logged_active = true;
        }
        return BLYNK_V7_LOW_WATER;
    }

    if ((tank_level < LOW_WATER_TRIGGER_PERCENT) &&
        !low_water_alarm_armed)
    {
        if (!low_water_logged_active)
        {
            ESP_LOGI(TAG, "LOW WATER alarm already active");
            low_water_logged_active = true;
        }
    }
    else if ((tank_level > LOW_WATER_RESET_PERCENT) &&
             !low_water_alarm_armed)
    {
        ESP_LOGI(TAG,
                 "Tank recovered above %d%%, low-water alarm re-armed",
                 LOW_WATER_RESET_PERCENT);
        low_water_alarm_armed = true;
        low_water_logged_active = false;
    }

    if ((tank_level >= FULL_TANK_TRIGGER_PERCENT) &&
        full_tank_alarm_armed)
    {
        if (!full_tank_logged_active)
        {
            ESP_LOGW(TAG, "FULL TANK: level=%d%%", tank_level);
            full_tank_logged_active = true;
        }
        return BLYNK_V7_FULL_TANK;
    }

    if ((tank_level >= FULL_TANK_TRIGGER_PERCENT) &&
        !full_tank_alarm_armed)
    {
        if (!full_tank_logged_active)
        {
            ESP_LOGI(TAG, "FULL TANK alarm already active");
            full_tank_logged_active = true;
        }
        return BLYNK_V7_NONE;
    }

    if ((tank_level < FULL_TANK_RESET_PERCENT) &&
        !full_tank_alarm_armed)
    {
        ESP_LOGI(TAG,
                 "Tank dropped below %d%%, full-tank alarm re-armed",
                 FULL_TANK_RESET_PERCENT);
        full_tank_alarm_armed = true;
        full_tank_logged_active = false;
    }

    return BLYNK_V7_NONE;
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

    blynk_init();

    ESP_ERROR_CHECK(modbus_master_init());

    bool uno_ever_ok = false;
    bool have_last_level = false;
    int last_valid_level = 0;
    int last_valid_volume_l = 0;
    int last_valid_distance_mm = 0;
    TickType_t last_blynk_tick = 0;
    bool blynk_have_sent = false;
    bool last_blynk_ok = true;

    while (1)
    {
        bool wifi_up = wifi_is_connected();
        bool uno_ok = modbus_master_link_ok();
        uint16_t distance_mm = 0;
        uint16_t uno_status = 0;
        tank_reading_t tank = { 0 };
        bool tank_data_valid = false;
        int event_code = BLYNK_V7_NONE;

        if (uno_ok &&
            modbus_master_get_tank(&distance_mm, NULL, NULL, &uno_status))
        {
            uno_ever_ok = true;
            tank_from_distance(distance_mm, &tank);
            tank_data_valid = (uno_status == 0);

            ESP_LOGI(TAG,
                     "dist=%u mm  water=%u mm  level=%u%%  vol=%u L  uno_status=%u",
                     (unsigned)distance_mm,
                     (unsigned)tank.water_mm,
                     (unsigned)tank.level_pct,
                     (unsigned)tank.volume_l,
                     (unsigned)uno_status);

            if (tank_data_valid)
            {
                last_valid_level = (int)tank.level_pct;
                last_valid_volume_l = (int)tank.volume_l;
                last_valid_distance_mm = (int)distance_mm;
                have_last_level = true;
                event_code = process_tank_alarms(last_valid_level);
            }
        }

        apply_status_led(wifi_up, uno_ever_ok, uno_ok);

        TickType_t now = xTaskGetTickCount();
        bool period_elapsed =
            !blynk_have_sent ||
            ((now - last_blynk_tick) >= pdMS_TO_TICKS(BLYNK_UPDATE_PERIOD_MS));
        bool retry_ok = last_blynk_ok || period_elapsed;
        bool first_reading_ready = blynk_have_sent || have_last_level;
        bool alarm_due = (event_code != BLYNK_V7_NONE);

        if (wifi_up && (period_elapsed || alarm_due) && retry_ok &&
            first_reading_ready)
        {
            int rssi = wifi_get_rssi();
            uint32_t uptime_minutes =
                (uint32_t)(esp_timer_get_time() / 1000000ULL / 60ULL);
            int level_to_send =
                have_last_level ? last_valid_level : 0;
            int volume_to_send =
                have_last_level ? last_valid_volume_l : 0;
            int distance_to_send =
                have_last_level ? last_valid_distance_mm : 0;

            update_counter++;
            last_blynk_tick = now;
            blynk_have_sent = true;

            if (blynk_send_status(
                    level_to_send,
                    volume_to_send,
                    distance_to_send,
                    rssi,
                    update_counter,
                    uptime_minutes,
                    event_code) == ESP_OK)
            {
                last_blynk_ok = true;
                if (event_code == BLYNK_V7_LOW_WATER)
                {
                    low_water_alarm_armed = false;
                    low_water_logged_active = false;
                }
                else if (event_code == BLYNK_V7_FULL_TANK)
                {
                    full_tank_alarm_armed = false;
                    full_tank_logged_active = false;
                }
            }
            else
            {
                last_blynk_ok = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_TICK_MS));
    }
}
