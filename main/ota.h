#ifndef OTA_H
#define OTA_H

/**
 * @brief Initializes the ESP32-S3 as a SoftAP (Access Point).
 */
void ota_wifi_init_softap(void);

/**
 * @brief Starts the web server to listen for incoming firmware POST requests.
 */
void ota_web_server_init(void);

#endif // OTA_H