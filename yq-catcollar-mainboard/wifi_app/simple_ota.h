#ifndef SIMPLE_OTA_H
#define SIMPLE_OTA_H

#include "sl_status.h"
#include "wifi_ota_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Simple OTA interface for easy integration

// Start OTA update with default settings
sl_status_t simple_ota_start(void);

// Start OTA update with custom callbacks
sl_status_t simple_ota_start_with_callbacks(wifi_ota_progress_callback_t progress_cb,
                                          wifi_ota_complete_callback_t complete_cb);

// Check if OTA is in progress
bool simple_ota_is_in_progress(void);

// Get current OTA status
wifi_ota_status_t simple_ota_get_status(void);

// Cancel OTA update
sl_status_t simple_ota_cancel(void);

// Default callbacks (can be used if custom callbacks are not needed)
void simple_ota_default_progress_callback(wifi_ota_status_t status, uint32_t progress_percent);
void simple_ota_default_complete_callback(wifi_ota_error_t error);

#ifdef __cplusplus
}
#endif

#endif // SIMPLE_OTA_H