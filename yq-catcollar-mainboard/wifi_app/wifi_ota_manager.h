#ifndef WIFI_OTA_MANAGER_H
#define WIFI_OTA_MANAGER_H

#include "sl_status.h"
#include "sl_wifi_types.h"
#include "wifi_ota_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations
sl_status_t wifi_ota_init(void);
sl_status_t wifi_ota_start_update(void);
sl_status_t wifi_ota_set_progress_callback(wifi_ota_progress_callback_t callback);
sl_status_t wifi_ota_set_complete_callback(wifi_ota_complete_callback_t callback);
wifi_ota_status_t wifi_ota_get_status(void);
sl_status_t wifi_ota_cancel(void);

// Internal functions
sl_status_t wifi_ota_load_certificates(void);
sl_status_t wifi_ota_dns_resolve(char *hostname, char *ip_buffer);
sl_status_t wifi_ota_download_firmware(void);

// Callback handlers
sl_status_t wifi_ota_fw_update_response_handler(sl_wifi_event_t event,
                                               uint16_t *data,
                                               uint32_t data_length,
                                               void *arg);

#ifdef __cplusplus
}
#endif

#endif // WIFI_OTA_MANAGER_H