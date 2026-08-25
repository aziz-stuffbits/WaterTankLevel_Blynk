#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbcontroller.h"

#include "modbus_master.h"

#define TANK_MB_UART_PORT UART_NUM_2
#define TANK_MB_UART_TX_PIN 17
#define TANK_MB_UART_RX_PIN 16
#define TANK_MB_UART_DE_RE_PIN 21

#define MB_SLAVE_ID 1
#define MB_BAUD 9600
#define MB_FC_READ_HOLDING 3

#define REG_DISTANCE_MM 0
#define REG_LEVEL_PCT 1
#define REG_VOLUME_L 2
#define REG_SENSOR_STATUS 3
#define REG_TANK_HEIGHT_MM 10
#define REG_TANK_CAPACITY_L 11
#define REG_COUNT 12

#define POLL_PERIOD_MS 2000

static const char *TAG = "MB_MASTER";

static uint16_t last_regs[REG_COUNT];
static bool last_ok = false;

bool modbus_master_get_tank(uint16_t *distance_mm,
                            uint16_t *level_pct,
                            uint16_t *volume_l,
                            uint16_t *status)
{
    if (!last_ok)
    {
        return false;
    }

    if (distance_mm)
    {
        *distance_mm = last_regs[REG_DISTANCE_MM];
    }
    if (level_pct)
    {
        *level_pct = last_regs[REG_LEVEL_PCT];
    }
    if (volume_l)
    {
        *volume_l = last_regs[REG_VOLUME_L];
    }
    if (status)
    {
        *status = last_regs[REG_SENSOR_STATUS];
    }
    return true;
}

bool modbus_master_link_ok(void)
{
    return last_ok;
}

static esp_err_t poll_uno(void)
{
    uint16_t regs[REG_COUNT] = { 0 };
    mb_param_request_t req = {
        .slave_addr = MB_SLAVE_ID,
        .command = MB_FC_READ_HOLDING,
        .reg_start = 0,
        .reg_size = REG_COUNT,
    };

    esp_err_t err = mbc_master_send_request(&req, regs);
    if (err != ESP_OK)
    {
        last_ok = false;
        ESP_LOGE(TAG, "poll slave %d failed: %s",
                 MB_SLAVE_ID, esp_err_to_name(err));
        return err;
    }

    for (int i = 0; i < REG_COUNT; i++)
    {
        last_regs[i] = regs[i];
    }
    last_ok = true;

    return ESP_OK;
}

static void poll_task(void *arg)
{
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(500));

    while (1)
    {
        (void)poll_uno();
        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}

esp_err_t modbus_master_init(void)
{
    void *mbc_master_handler = NULL;
    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &mbc_master_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mbc_master_init failed: %s", esp_err_to_name(err));
        return err;
    }

    mb_communication_info_t comm = { 0 };
    comm.mode = MB_MODE_RTU;
    comm.port = TANK_MB_UART_PORT;
    comm.baudrate = MB_BAUD;
    comm.parity = MB_PARITY_NONE;
    err = mbc_master_setup(&comm);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mbc_master_setup failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mbc_master_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mbc_master_start failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(
        TANK_MB_UART_PORT,
        TANK_MB_UART_TX_PIN,
        TANK_MB_UART_RX_PIN,
        TANK_MB_UART_DE_RE_PIN,
        UART_PIN_NO_CHANGE
    );
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_mode(TANK_MB_UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(uart_set_rts(TANK_MB_UART_PORT, 1));

    if (xTaskCreate(poll_task, "mb_poll", 4096, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "poll task failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "RTU master UART2 TX=%d RX=%d DE/RE=%d @ %d 8N1, polling slave %d",
             TANK_MB_UART_TX_PIN,
             TANK_MB_UART_RX_PIN,
             TANK_MB_UART_DE_RE_PIN,
             MB_BAUD,
             MB_SLAVE_ID);

    return ESP_OK;
}
