#ifndef SMART_OTA_H
#define SMART_OTA_H

#include "sl_status.h"
#include "version_manager.h"
#include "wifi_ota_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Smart OTA status
typedef enum {
    SMART_OTA_IDLE = 0,
    SMART_OTA_CHECKING_VERSION,
    SMART_OTA_NO_UPDATE_NEEDED,
    SMART_OTA_UPDATE_AVAILABLE,
    SMART_OTA_DOWNLOADING,
    SMART_OTA_UPDATING,
    SMART_OTA_COMPLETE,
    SMART_OTA_ERROR
} smart_ota_status_t;

// Smart OTA callback types
typedef void (*smart_ota_status_callback_t)(smart_ota_status_t status, uint32_t progress_percent);
typedef void (*smart_ota_version_callback_t)(firmware_version_t current, firmware_version_t remote, version_compare_result_t result);

// Function declarations
sl_status_t smart_ota_init(void);
sl_status_t smart_ota_check_and_update(void);
sl_status_t smart_ota_set_status_callback(smart_ota_status_callback_t callback);
sl_status_t smart_ota_set_version_callback(smart_ota_version_callback_t callback);
smart_ota_status_t smart_ota_get_status(void);
sl_status_t smart_ota_force_update(void);  // Force update regardless of version

// Default callbacks
void smart_ota_default_status_callback(smart_ota_status_t status, uint32_t progress_percent);
void smart_ota_default_version_callback(firmware_version_t current, firmware_version_t remote, version_compare_result_t result);

#ifdef __cplusplus
}
#endif

#endif // SMART_OTA_H