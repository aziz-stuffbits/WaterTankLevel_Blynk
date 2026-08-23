#include <stdio.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls_errors.h"

#include "blynk.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef BLYNK_TOKEN
#define BLYNK_TOKEN "YOUR_BLYNK_AUTH_TOKEN"
#endif

#define BLYNK_UPDATE_URL \
    "https://blynk.cloud/external/api/update?token=%s&v0=%d"

#define BLYNK_STATUS_URL \
    "https://blynk.cloud/external/api/update?token=%s&v0=%d&v1=%d"

static const char *TAG = "BLYNK";

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

esp_err_t blynk_send_tank_level(int level)
{
    if ((level < 0) || (level > 100))
    {
        ESP_LOGE(TAG, "Invalid tank level %d", level);
        return ESP_ERR_INVALID_ARG;
    }

    if ((BLYNK_TOKEN[0] == '\0') ||
        (strcmp(BLYNK_TOKEN, "YOUR_BLYNK_AUTH_TOKEN") == 0))
    {
        ESP_LOGE(TAG,
                 "Blynk Auth Token is not set");
        return ESP_ERR_INVALID_ARG;
    }

    char url[192];
    int written = snprintf(
        url,
        sizeof(url),
        BLYNK_UPDATE_URL,
        BLYNK_TOKEN,
        level
    );

    if ((written < 0) || (written >= (int)sizeof(url)))
    {
        ESP_LOGE(TAG, "Failed to build Blynk URL");
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Sending tank level %d%%", level);

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
    esp_http_client_cleanup(client);

    if (status != 200)
    {
        ESP_LOGE(TAG,
                 "Tank level %d%% rejected, HTTP status = %d",
                 level,
                 status);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "Tank level %d%% sent, HTTP status = %d",
             level,
             status);

    return ESP_OK;
}

esp_err_t blynk_send_status(int level, int error_code)
{
    if ((level < 0) || (level > 100))
    {
        ESP_LOGE(TAG, "Invalid tank level %d", level);
        return ESP_ERR_INVALID_ARG;
    }

    if ((BLYNK_TOKEN[0] == '\0') ||
        (strcmp(BLYNK_TOKEN, "YOUR_BLYNK_AUTH_TOKEN") == 0))
    {
        ESP_LOGE(TAG, "Blynk Auth Token is not set");
        return ESP_ERR_INVALID_ARG;
    }

    char url[192];
    int written = snprintf(
        url,
        sizeof(url),
        BLYNK_STATUS_URL,
        BLYNK_TOKEN,
        level,
        error_code
    );

    if ((written < 0) || (written >= (int)sizeof(url)))
    {
        ESP_LOGE(TAG, "Failed to build Blynk URL");
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Sending level %d%%  err=%d", level, error_code);

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
    esp_http_client_cleanup(client);

    if (status != 200)
    {
        ESP_LOGE(TAG,
                 "Status update rejected, HTTP status = %d",
                 status);
        return ESP_FAIL;
    }

    return ESP_OK;
}
