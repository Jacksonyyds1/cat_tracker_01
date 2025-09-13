/*******************************************************************************
* @file  wifi_ota_integration_example.c
* @brief Cat Collar OTA Integration Example - AWS S3 ap-east-1
*******************************************************************************
* Example showing how to integrate the enhanced WiFi OTA manager into the
* cat collar application for firmware updates from AWS S3 ap-east-1 region.
******************************************************************************/

#include "wifi_ota_manager.h"
#include "wifi_app.h"
#include "app_log.h"
#include "cmsis_os2.h"

// Example OTA integration functions
static void ota_progress_callback_example(uint32_t bytes_downloaded, uint32_t total_bytes);
static void ota_status_check_task(void *argument);

// Example task attributes
static const osThreadAttr_t ota_task_attributes = {
    .name = "ota_task",
    .stack_size = 2048,
    .priority = osPriorityNormal,
};

/**
 * @brief Initialize OTA manager during application startup
 */
sl_status_t catcollar_ota_app_init(void)
{
    sl_status_t status = SL_STATUS_OK;

    app_log_info("=== Cat Collar OTA Integration Example ===\r\n");
    app_log_info("AWS S3 Region: ap-east-1\r\n");
    app_log_info("HTTPS enabled with Starfield CA certificate\r\n");

    // Initialize OTA manager with default configuration
    status = catcollar_ota_init();
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to initialize OTA manager: 0x%lx\r\n", status);
        return status;
    }

    // Set progress callback for download monitoring
    catcollar_ota_set_progress_callback(ota_progress_callback_example);

    // Start periodic OTA checks (every 24 hours)
    status = catcollar_ota_start_periodic_check();
    if (status != SL_STATUS_OK) {
        app_log_warning("Failed to start periodic OTA checks: 0x%lx\r\n", status);
    }

    app_log_info("OTA manager initialized successfully\r\n");

    return status;
}

/**
 * @brief Manual OTA check example (can be triggered by button press, shell command, etc.)
 */
sl_status_t catcollar_ota_manual_check_example(void)
{
    sl_status_t status = SL_STATUS_OK;
    catcollar_ota_state_t current_state;
    catcollar_ota_status_t current_status;

    app_log_info("=== Manual OTA Check Started ===\r\n");

    // Check current state - don't start if already in progress
    current_state = catcollar_ota_get_state();
    if (current_state != OTA_STATE_IDLE) {
        app_log_warning("OTA operation already in progress: %s\r\n",
                       catcollar_ota_state_to_string(current_state));
        return SL_STATUS_BUSY;
    }

    // Check for updates
    status = catcollar_ota_check_for_updates();
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to check for updates: 0x%lx\r\n", status);
        return status;
    }

    // Get the result
    current_status = catcollar_ota_get_status();
    app_log_info("OTA Status: %s\r\n", catcollar_ota_status_to_string(current_status));

    if (current_status == OTA_STATUS_UPDATE_AVAILABLE) {
        app_log_info("New firmware available! Use catcollar_ota_manual_update_example() to start download\r\n");
    } else {
        app_log_info("Firmware is up to date\r\n");
    }

    return status;
}

/**
 * @brief Manual OTA update example (start download and installation)
 */
sl_status_t catcollar_ota_manual_update_example(void)
{
    sl_status_t status = SL_STATUS_OK;
    catcollar_ota_state_t current_state;

    app_log_info("=== Manual OTA Update Started ===\r\n");

    // Check current state
    current_state = catcollar_ota_get_state();
    if (current_state != OTA_STATE_IDLE) {
        app_log_warning("OTA operation already in progress: %s\r\n",
                       catcollar_ota_state_to_string(current_state));
        return SL_STATUS_BUSY;
    }

    // Start update with retry (max 3 attempts)
    app_log_info("Starting firmware download from AWS S3 ap-east-1...\r\n");
    status = catcollar_ota_start_update_with_retry(3);

    if (status == SL_STATUS_OK) {
        app_log_info("OTA update completed successfully - device will reboot\r\n");
    } else {
        app_log_error("OTA update failed: 0x%lx\r\n", status);
        app_log_info("Final Status: %s\r\n",
                    catcollar_ota_status_to_string(catcollar_ota_get_status()));
    }

    return status;
}

/**
 * @brief Complete OTA check and update example (check + update if available)
 */
sl_status_t catcollar_ota_check_and_update_example(void)
{
    sl_status_t status = SL_STATUS_OK;

    app_log_info("=== Automatic OTA Check and Update ===\r\n");

    // This function will check for updates and automatically start download if available
    status = catcollar_ota_check_and_update();

    if (status == SL_STATUS_OK) {
        catcollar_ota_status_t final_status = catcollar_ota_get_status();
        if (final_status == OTA_STATUS_DOWNLOAD_SUCCESS) {
            app_log_info("Firmware update completed - device will reboot\r\n");
        } else {
            app_log_info("No update available or update completed\r\n");
        }
    } else {
        app_log_error("OTA check and update failed: 0x%lx\r\n", status);
    }

    return status;
}

/**
 * @brief OTA status monitoring task example
 */
sl_status_t catcollar_ota_start_monitoring_task(void)
{
    osThreadId_t thread_id = osThreadNew(ota_status_check_task, NULL, &ota_task_attributes);

    if (thread_id == NULL) {
        app_log_error("Failed to create OTA monitoring task\r\n");
        return SL_STATUS_FAIL;
    }

    app_log_info("OTA monitoring task started\r\n");
    return SL_STATUS_OK;
}

/**
 * @brief Get current firmware version example
 */
void catcollar_ota_print_version_info(void)
{
    catcollar_firmware_version_t version = catcollar_get_current_version();

    app_log_info("=== Current Firmware Information ===\r\n");
    app_log_info("Version: %d.%d.%d.%d\r\n",
                version.major, version.minor, version.patch, version.build);
    app_log_info("Version String: %s\r\n", CATCOLLAR_FIRMWARE_VERSION_STRING);
    app_log_info("AWS S3 Hostname: %s\r\n", OTA_AWS_HOSTNAME);
    app_log_info("Firmware URL: %s\r\n", OTA_FIRMWARE_URL);
}

/**
 * @brief Abort OTA operation example (for emergency stop)
 */
sl_status_t catcollar_ota_abort_example(void)
{
    sl_status_t status = SL_STATUS_OK;
    catcollar_ota_state_t current_state = catcollar_ota_get_state();

    if (current_state == OTA_STATE_IDLE) {
        app_log_info("No OTA operation to abort\r\n");
        return SL_STATUS_OK;
    }

    app_log_warning("Aborting OTA operation (current state: %s)\r\n",
                   catcollar_ota_state_to_string(current_state));

    status = catcollar_ota_abort();

    if (status == SL_STATUS_OK) {
        app_log_info("OTA operation aborted successfully\r\n");
    } else {
        app_log_error("Failed to abort OTA operation: 0x%lx\r\n", status);
    }

    return status;
}

// Private functions implementation

/**
 * @brief Progress callback for OTA download monitoring
 */
static void ota_progress_callback_example(uint32_t bytes_downloaded, uint32_t total_bytes)
{
    if (total_bytes > 0) {
        uint8_t percentage = (uint8_t)((bytes_downloaded * 100) / total_bytes);
        app_log_info("OTA Progress: %d%% (%lu/%lu bytes)\r\n",
                    percentage, bytes_downloaded, total_bytes);
    } else {
        app_log_info("OTA Progress: %lu bytes downloaded\r\n", bytes_downloaded);
    }
}

/**
 * @brief Background task for OTA status monitoring
 */
static void ota_status_check_task(void *argument)
{
    UNUSED_PARAMETER(argument);

    catcollar_ota_state_t last_state = OTA_STATE_IDLE;
    catcollar_ota_status_t last_status = OTA_STATUS_NO_UPDATE_AVAILABLE;

    app_log_info("OTA monitoring task started\r\n");

    while (1) {
        catcollar_ota_state_t current_state = catcollar_ota_get_state();
        catcollar_ota_status_t current_status = catcollar_ota_get_status();

        // Log state changes
        if (current_state != last_state) {
            app_log_info("OTA State changed: %s -> %s\r\n",
                        catcollar_ota_state_to_string(last_state),
                        catcollar_ota_state_to_string(current_state));
            last_state = current_state;
        }

        // Log status changes
        if (current_status != last_status) {
            app_log_info("OTA Status changed: %s -> %s\r\n",
                        catcollar_ota_status_to_string(last_status),
                        catcollar_ota_status_to_string(current_status));
            last_status = current_status;
        }

        // Show progress during download
        if (current_state == OTA_STATE_DOWNLOADING) {
            uint8_t progress = catcollar_ota_get_progress_percentage();
            static uint8_t last_progress = 0;

            if (progress != last_progress && progress % 10 == 0) {
                app_log_info("Download progress: %d%%\r\n", progress);
                last_progress = progress;
            }
        }

        // Sleep for 5 seconds
        osDelay(5000);
    }
}

// Usage Instructions (commented examples for documentation):

/*
 * === Integration Usage Examples ===
 *
 * 1. Initialize OTA during app startup:
 *    catcollar_ota_app_init();
 *
 * 2. Manual check for updates (e.g., from shell command):
 *    catcollar_ota_manual_check_example();
 *
 * 3. Manual update if available (e.g., from shell command):
 *    catcollar_ota_manual_update_example();
 *
 * 4. Automatic check and update:
 *    catcollar_ota_check_and_update_example();
 *
 * 5. Start background monitoring:
 *    catcollar_ota_start_monitoring_task();
 *
 * 6. Print version information:
 *    catcollar_ota_print_version_info();
 *
 * 7. Emergency abort:
 *    catcollar_ota_abort_example();
 *
 * === Shell Command Integration Example ===
 *
 * // Add these to your shell command list:
 * SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
 *                  ota_check, catcollar_ota_manual_check_example, Check for OTA updates);
 *
 * SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
 *                  ota_update, catcollar_ota_manual_update_example, Start OTA update);
 *
 * SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
 *                  ota_version, catcollar_ota_print_version_info, Show firmware version);
 *
 * SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
 *                  ota_abort, catcollar_ota_abort_example, Abort OTA operation);
 */