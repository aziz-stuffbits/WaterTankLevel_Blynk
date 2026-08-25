#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls_errors.h"
#include "mqtt_client.h"

#include "blynk.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef BLYNK_TOKEN
#define BLYNK_TOKEN "YOUR_BLYNK_AUTH_TOKEN"
#endif

#define BLYNK_MQTT_URI "mqtts://blynk.cloud:8883"
#define BLYNK_MQTT_KEEPALIVE_S 45
#define BLYNK_MQTT_CONNECT_WAIT_MS 10000

#define BLYNK_STATUS_URL \
    "https://blynk.cloud/external/api/update?token=%s&v0=%d&v1=%d&v2=%d&v3=%d&v4=%s&v5=%lu&v6=%lu&v7=%d"

static const char *TAG = "BLYNK";

static esp_mqtt_client_handle_t s_mqtt;
static volatile bool s_mqtt_connected;
static char s_mqtt_uri[96] = BLYNK_MQTT_URI;

static bool token_is_set(void)
{
    return (BLYNK_TOKEN[0] != '\0') &&
           (strcmp(BLYNK_TOKEN, "YOUR_BLYNK_AUTH_TOKEN") != 0);
}

static bool mqtt_topic_eq(const esp_mqtt_event_handle_t event, const char *topic)
{
    int n = (int)strlen(topic);

    return (event->topic != NULL) &&
           (event->topic_len == n) &&
           (memcmp(event->topic, topic, (size_t)n) == 0);
}

static void apply_mqtt_redirect(esp_mqtt_client_handle_t client,
                                const char *data,
                                int data_len)
{
    if ((data == NULL) || (data_len <= 0) ||
        (data_len >= (int)sizeof(s_mqtt_uri)))
    {
        ESP_LOGE(TAG, "Invalid MQTT redirect");
        return;
    }

    memcpy(s_mqtt_uri, data, (size_t)data_len);
    s_mqtt_uri[data_len] = '\0';

    while ((data_len > 0) &&
           ((s_mqtt_uri[data_len - 1] == '\n') ||
            (s_mqtt_uri[data_len - 1] == '\r') ||
            (s_mqtt_uri[data_len - 1] == ' ')))
    {
        data_len--;
        s_mqtt_uri[data_len] = '\0';
    }

    ESP_LOGI(TAG, "MQTT redirect to %s", s_mqtt_uri);
    s_mqtt_connected = false;
    esp_mqtt_client_set_uri(client, s_mqtt_uri);
    esp_mqtt_client_reconnect(client);
}

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        s_mqtt_connected = true;
        esp_mqtt_client_subscribe(event->client, "downlink/#", 0);
        ESP_LOGI(TAG, "MQTT connected (device Online)");
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
        break;

    case MQTT_EVENT_DATA:
        if (mqtt_topic_eq(event, "downlink/redirect"))
        {
            apply_mqtt_redirect(event->client, event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

bool blynk_mqtt_connected(void)
{
    return s_mqtt_connected;
}

void blynk_init(void)
{
    if (!token_is_set())
    {
        ESP_LOGE(TAG, "Blynk Auth Token is not set");
        return;
    }

    if (s_mqtt != NULL)
    {
        return;
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = s_mqtt_uri,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = "device",
        .credentials.authentication.password = BLYNK_TOKEN,
        .session.keepalive = BLYNK_MQTT_KEEPALIVE_S,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .buffer.size = 1024,
    };

    s_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt == NULL)
    {
        ESP_LOGE(TAG, "MQTT client init failed");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt, MQTT_EVENT_ANY, mqtt_event_handler, NULL);

    if (esp_mqtt_client_start(s_mqtt) != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client start failed");
        return;
    }

    int waited_ms = 0;
    while (!s_mqtt_connected && (waited_ms < BLYNK_MQTT_CONNECT_WAIT_MS))
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        waited_ms += 100;
    }

    if (!s_mqtt_connected)
    {
        ESP_LOGW(TAG, "MQTT not up yet; HTTP updates will still run");
    }
}

static void log_request_failure(esp_err_t err)
{
    if (err == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME)
    {
        ESP_LOGE(TAG, "DNS failure: cannot resolve blynk.cloud");
        return;
    }

    if (err == ESP_ERR_HTTP_CONNECT ||
        err == ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST)
    {
        ESP_LOGE(TAG,
                 "HTTP connection failure: %s",
                 esp_err_to_name(err));
        return;
    }

    if ((err >= ESP_ERR_ESP_TLS_BASE) &&
        (err < (ESP_ERR_ESP_TLS_BASE + 0x100)))
    {
        ESP_LOGE(TAG,
                 "TLS/SSL failure: %s",
                 esp_err_to_name(err));
        return;
    }

    ESP_LOGE(TAG,
             "Blynk request failure: %s",
             esp_err_to_name(err));
}

static esp_err_t blynk_http_get(const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);

    if (client == NULL)
    {
        ESP_LOGE(TAG, "HTTP client init failed");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK)
    {
        log_request_failure(err);
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    char body[160];
    int n = esp_http_client_read_response(client, body, sizeof(body) - 1);

    if (n < 0)
    {
        n = 0;
    }
    body[n] = '\0';

    esp_http_client_cleanup(client);

    if (status != 200)
    {
        ESP_LOGE(TAG,
                 "Blynk HTTP status = %d body=%s",
                 status,
                 (n > 0) ? body : "(empty)");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t blynk_send_status(int level,
                            int error_code,
                            int volume_l,
                            int distance_mm,
                            int rssi,
                            uint32_t update_counter,
                            uint32_t uptime_minutes,
                            int event_code)
{
    if ((level < 0) || (level > 100))
    {
        ESP_LOGE(TAG, "Invalid tank level %d", level);
        return ESP_ERR_INVALID_ARG;
    }

    if (!token_is_set())
    {
        ESP_LOGE(TAG, "Blynk Auth Token is not set");
        return ESP_ERR_INVALID_ARG;
    }

    int rssi_send = rssi;
    if (rssi_send < -127)
    {
        rssi_send = -127;
    }
    if (rssi_send > 0)
    {
        rssi_send = 0;
    }

    char rssi_q[16];
    if (rssi_send < 0)
    {
        snprintf(rssi_q, sizeof(rssi_q), "%%2D%u", (unsigned)(-rssi_send));
    }
    else
    {
        snprintf(rssi_q, sizeof(rssi_q), "%d", rssi_send);
    }

    char url[384];
    int written = snprintf(
        url,
        sizeof(url),
        BLYNK_STATUS_URL,
        BLYNK_TOKEN,
        level,
        error_code,
        volume_l,
        distance_mm,
        rssi_q,
        (unsigned long)update_counter,
        (unsigned long)uptime_minutes,
        event_code
    );

    if ((written < 0) || (written >= (int)sizeof(url)))
    {
        ESP_LOGE(TAG, "Failed to build Blynk URL");
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG,
             "Level=%d%%, Vol=%d L, Dist=%d mm, RSSI=%d dBm, Counter=%lu, Uptime=%lu min, V7=%d MQTT=%s",
             level,
             volume_l,
             distance_mm,
             rssi,
             (unsigned long)update_counter,
             (unsigned long)uptime_minutes,
             event_code,
             s_mqtt_connected ? "online" : "offline");

    esp_err_t err = blynk_http_get(url);
    if (err == ESP_OK)
    {
        if (event_code == BLYNK_V7_LOW_WATER)
        {
            ESP_LOGI(TAG, "low_water event sent");
        }
        else if (event_code == BLYNK_V7_FULL_TANK)
        {
            ESP_LOGI(TAG, "full_tank event sent");
        }
    }

    return err;
}
