#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "ota.h"


static const char *TAG = "OTA_SYSTEM";

#define AP_SSID      "ESP32S3_OTA_AP"
#define AP_PASSWORD  "12345678"
#define MAX_STA_CONN 4
#define SCRATCH_BUFSIZE 1024

/* HTTP POST Handler for firmware upload */
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char buf[SCRATCH_BUFSIZE];
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    
    ESP_LOGI(TAG, "Starting OTA update reception...");

    // 1. Identify the next partition to write to
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partition missing");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
             update_partition->subtype, update_partition->address);

    // 2. Prepare the flash for writing
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Init OTA Flash Failed");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    int received;

    // 3. Loop to receive data stream in chunks over HTTP
    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Retry if timeout occurred
            }
            ESP_LOGE(TAG, "File reception failed or interrupted.");
            esp_ota_end(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Stream Interrupted");
            return ESP_FAIL;
        }

        // Write the received chunk directly into the flash segment
        err = esp_ota_write(update_handle, (const void *)buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            esp_ota_end(update_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Write Error");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    ESP_LOGI(TAG, "Total binary received: %d bytes. Validating...", req->content_len);

    // 4. Close the write handle and validate signature/checksums
    if (esp_ota_end(update_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed! Image might be corrupted.");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image Validation Failed");
        return ESP_FAIL;
    }

    // 5. Update the boot target selection flag
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Boot Target Update Failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA Upgrade Successful! Rebooting system in 2 seconds...");
    
    // Send dynamic response back to client
    const char *resp_str = "<html><body><h1>OTA Update Successful!</h1><p>Device is rebooting...</p></body></html>";
    httpd_resp_send(req, resp_str, strlen(resp_str));

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}

/* CORS Handling for options pre-flight (Ensures seamless cross-origin updates from local storage) */
static esp_err_t ota_options_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void ota_web_server_init(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;

    // URI structure configurations
    httpd_uri_t ota_upload_uri = {
        .uri      = "/update",
        .method   = HTTP_POST,
        .handler  = ota_post_handler,
        .user_ctx = NULL
    };

    httpd_uri_t ota_options_uri = {
        .uri      = "/update",
        .method   = HTTP_OPTIONS,
        .handler  = ota_options_handler,
        .user_ctx = NULL
    };

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ota_upload_uri);
        httpd_register_uri_handler(server, &ota_options_uri);
    }
}

void ota_wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1,
            .password = AP_PASSWORD,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP complete. SSID: %s Password: %s", AP_SSID, AP_PASSWORD);
}