idf_component_register(
    SRCS
        "main.c"
        "wifi.c"
        "blynk.c"
        "modbus_master.c"
        "tank.c"
        "status_led.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_http_client
        esp-tls
        mbedtls
        driver
        espressif__esp-modbus
)
