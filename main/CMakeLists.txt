idf_component_register(SRCS "main.c" "ota.c"
                    INCLUDE_DIRS "."
                    REQUIRES app_update esp_http_server esp_wifi nvs_flash
                    
                    )