#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ota.h"

static const char *TAG = "main";

void app_main(void)
{
    // Initialize NVS storage partition
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Booting initialization setup...");

    // Boot local Wi-Fi Hotspot configuration
    ota_wifi_init_softap();

    // Start local web node backend system
    ota_web_server_init();

    while (1) {
               
        printf("OTA 1 Running!\n");

        printf("Running App... Free heap size: %ld bytes\n", esp_get_free_heap_size());
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}