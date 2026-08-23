#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "status_led.h"

/* ESP32-WROOM DevKit onboard LED (usually blue). */
#define STATUS_LED_GPIO 2

static volatile status_led_mode_t s_mode = STATUS_LED_OFF;

static void led_task(void *arg)
{
    (void)arg;
    bool on = false;

    while (1)
    {
        status_led_mode_t mode = s_mode;

        switch (mode)
        {
        case STATUS_LED_SOLID:
            gpio_set_level(STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            break;

        case STATUS_LED_BLINK_200:
            on = !on;
            gpio_set_level(STATUS_LED_GPIO, on ? 1 : 0);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case STATUS_LED_BLINK_1000:
            on = !on;
            gpio_set_level(STATUS_LED_GPIO, on ? 1 : 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;

        case STATUS_LED_OFF:
        default:
            gpio_set_level(STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            break;
        }
    }
}

void status_led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(STATUS_LED_GPIO, 0);

    xTaskCreate(led_task, "status_led", 2048, NULL, 3, NULL);
}

void status_led_set(status_led_mode_t mode)
{
    s_mode = mode;
}
