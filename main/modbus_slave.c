#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbcontroller.h"

#include "modbus_slave.h"

#define TANK_MB_UART_PORT UART_NUM_2
#define TANK_MB_UART_TX_PIN 17
#define TANK_MB_UART_RX_PIN 16
#define TANK_MB_UART_DE_RE_PIN 21

#define MB_SLAVE_ID 1
#define MB_BAUD 9600

#define REG_DISTANCE_MM 0
#define REG_LEVEL_PCT 1
#define REG_VOLUME_L 2
#define REG_SENSOR_STATUS 3
#define REG_RESERVED 4
#define REG_TANK_HEIGHT_MM 10
#define REG_TANK_CAPACITY_L 11
#define REG_COUNT 12

#define TANK_HEIGHT_MM_DEFAULT 1500
#define TANK_CAPACITY_L_DEFAULT 1000

static const char *TAG = "MB_SLAVE";

static uint16_t holding_regs[REG_COUNT];

void modbus_slave_set_tank(uint16_t distance_mm,
                           uint16_t level_pct,
                           uint16_t volume_l,
                           uint16_t status)
{
    holding_regs[REG_DISTANCE_MM] = distance_mm;
    holding_regs[REG_LEVEL_PCT] = level_pct;
    holding_regs[REG_VOLUME_L] = volume_l;
    holding_regs[REG_SENSOR_STATUS] = status;
}

static void mb_event_task(void *arg)
{
    (void)arg;

    const mb_event_group_t mask =
        MB_EVENT_HOLDING_REG_RD | MB_EVENT_HOLDING_REG_WR;

    while (1)
    {
        (void)mbc_slave_check_event(mask);

        mb_param_info_t info;
        if (mbc_slave_get_param_info(&info, 10) != ESP_OK)
        {
            continue;
        }

        ESP_LOGI(TAG, "Holding %s offset=%u size=%u",
                 (info.type & MB_EVENT_HOLDING_REG_WR) ? "WRITE" : "READ",
                 (unsigned)info.mb_offset,
                 (unsigned)info.size);
    }
}

esp_err_t modbus_slave_init(void)
{
    holding_regs[REG_TANK_HEIGHT_MM] = TANK_HEIGHT_MM_DEFAULT;
    holding_regs[REG_TANK_CAPACITY_L] = TANK_CAPACITY_L_DEFAULT;

    void *mbc_slave_handler = NULL;
    esp_err_t err = mbc_slave_init(MB_PORT_SERIAL_SLAVE, &mbc_slave_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mbc_slave_init failed: %s", esp_err_to_name(err));
        return err;
    }

    mb_communication_info_t comm = { 0 };
    comm.mode = MB_MODE_RTU;
    comm.slave_addr = MB_SLAVE_ID;
    comm.port = TANK_MB_UART_PORT;
    comm.baudrate = MB_BAUD;
    comm.parity = MB_PARITY_NONE;
    err = mbc_slave_setup(&comm);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mbc_slave_setup failed: %s", esp_err_to_name(err));
        return err;
    }

    mb_register_area_descriptor_t holding = {
        .type = MB_PARAM_HOLDING,
        .start_offset = 0,
        .address = (void *)holding_regs,
        .size = sizeof(holding_regs),
    };
    err = mbc_slave_set_descriptor(holding);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mbc_slave_set_descriptor failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mbc_slave_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "mbc_slave_start failed: %s", esp_err_to_name(err));
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

    /* Idle = receive: RTS/sw_rts=1 drives GPIO21 LOW (DE off, RE on). */
    ESP_ERROR_CHECK(uart_set_rts(TANK_MB_UART_PORT, 1));

    if (xTaskCreate(mb_event_task, "mb_evt", 3072, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "event task failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "RTU slave ID %d on UART2 TX=%d RX=%d DE/RE=%d @ %d 8N1",
             MB_SLAVE_ID,
             TANK_MB_UART_TX_PIN,
             TANK_MB_UART_RX_PIN,
             TANK_MB_UART_DE_RE_PIN,
             MB_BAUD);

    return ESP_OK;
}
