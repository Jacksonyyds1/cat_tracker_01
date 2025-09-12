#ifndef WIFI_OTA_MANAGER_H
#define WIFI_OTA_MANAGER_H

#include "wifi_ota_config.h"
#include "sl_status.h"

/**
 * @brief Initialize the OTA manager
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_init(void);

/**
 * @brief Check for available firmware updates from AWS
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_check_for_updates(void);

/**
 * @brief Start the OTA firmware update process
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_start_update(void);

/**
 * @brief Get the current OTA state
 * @return Current OTA state
 */
catcollar_ota_state_t catcollar_ota_get_state(void);

/**
 * @brief Get the last OTA status
 * @return Last OTA status
 */
catcollar_ota_status_t catcollar_ota_get_status(void);

/**
 * @brief Get the current firmware version
 * @return Current firmware version structure
 */
catcollar_firmware_version_t catcollar_get_current_version(void);

/**
 * @brief Start periodic OTA version checking (every 24 hours)
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_start_periodic_check(void);

/**
 * @brief Stop periodic OTA version checking
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_stop_periodic_check(void);

#endif // WIFI_OTA_MANAGER_H