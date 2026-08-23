#pragma once

typedef enum
{
    STATUS_LED_OFF = 0,
    STATUS_LED_SOLID,
    STATUS_LED_BLINK_200,
    STATUS_LED_BLINK_1000,
} status_led_mode_t;

void status_led_init(void);
void status_led_set(status_led_mode_t mode);
