#ifndef WIFI_OTA_MANAGER_H
#define WIFI_OTA_MANAGER_H

#include "wifi_ota_config.h"
#include "sl_status.h"

/**
 * @brief Initialize the OTA manager with default configuration
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_init(void);

/**
 * @brief Initialize the OTA manager with custom configuration
 * @param config Pointer to custom OTA configuration
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_init_with_config(catcollar_ota_config_t *config);

/**
 * @brief Check for available firmware updates from AWS S3 ap-east-1
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_check_for_updates(void);

/**
 * @brief Start the OTA firmware update process
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_start_update(void);

/**
 * @brief Start the OTA firmware update process with retry mechanism
 * @param max_retries Maximum number of retry attempts
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_start_update_with_retry(uint8_t max_retries);

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

/**
 * @brief Abort ongoing OTA operation
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_abort(void);

/**
 * @brief Get the current download progress percentage (0-100)
 * @return Progress percentage
 */
uint8_t catcollar_ota_get_progress_percentage(void);

/**
 * @brief Set progress callback for OTA download monitoring
 * @param callback Function pointer to progress callback
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_set_progress_callback(catcollar_ota_progress_callback_t callback);

/**
 * @brief Get readable string representation of OTA state
 * @param state OTA state value
 * @return String representation of the state
 */
const char* catcollar_ota_state_to_string(catcollar_ota_state_t state);

/**
 * @brief Get readable string representation of OTA status
 * @param status OTA status value
 * @return String representation of the status
 */
const char* catcollar_ota_status_to_string(catcollar_ota_status_t status);

/**
 * @brief Force check for updates and download if available (for manual trigger)
 * @return SL_STATUS_OK on success, error code otherwise
 */
sl_status_t catcollar_ota_check_and_update(void);

#endif // WIFI_OTA_MANAGER_H