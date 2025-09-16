#include "smart_ota.h"
#include "version_manager.h"
#include "simple_ota.h"
#include "app_log.h"
#include "cmsis_os2.h"

// Static variables
static smart_ota_status_t current_status = SMART_OTA_IDLE;
static smart_ota_status_callback_t status_callback = NULL;
static smart_ota_version_callback_t version_callback = NULL;
static firmware_version_t current_version = {0};
static firmware_version_t remote_version = {0};

// Internal function to update status
static void smart_ota_update_status(smart_ota_status_t new_status, uint32_t progress)
{
    current_status = new_status;
    if (status_callback) {
        status_callback(new_status, progress);
    }
}

// Version check completion callback
void smart_ota_version_check_callback(version_check_error_t error, firmware_version_t remote_ver, version_compare_result_t result)
{
    remote_version = remote_ver;

    if (error != VERSION_CHECK_SUCCESS) {
        app_log_error("Version check failed with error: %d\r\n", error);
        smart_ota_update_status(SMART_OTA_ERROR, 0);
        return;
    }

    // Notify about version comparison result
    if (version_callback) {
        version_callback(current_version, remote_version, result);
    }

    switch (result) {
        case VERSION_NEWER:
            app_log_info("New firmware version available - starting OTA update\r\n");
            smart_ota_update_status(SMART_OTA_UPDATE_AVAILABLE, 100);

            // Start OTA update
            osDelay(1000);  // Brief delay before starting update
            sl_status_t ota_status = simple_ota_start();
            if (ota_status == SL_STATUS_OK) {
                smart_ota_update_status(SMART_OTA_DOWNLOADING, 0);
            } else {
                app_log_error("Failed to start OTA update: 0x%lx\r\n", ota_status);
                smart_ota_update_status(SMART_OTA_ERROR, 0);
            }
            break;

        case VERSION_SAME:
            app_log_info("Firmware is up to date - no update needed\r\n");
            smart_ota_update_status(SMART_OTA_NO_UPDATE_NEEDED, 100);
            break;

        case VERSION_OLDER:
            app_log_info("Remote firmware is older - no update needed\r\n");
            smart_ota_update_status(SMART_OTA_NO_UPDATE_NEEDED, 100);
            break;
    }
}

// OTA progress callback
void smart_ota_progress_callback(wifi_ota_status_t ota_status, uint32_t progress_percent)
{
    switch (ota_status) {
        case WIFI_OTA_DOWNLOADING:
            smart_ota_update_status(SMART_OTA_DOWNLOADING, progress_percent);
            break;
        case WIFI_OTA_UPDATING:
            smart_ota_update_status(SMART_OTA_UPDATING, progress_percent);
            break;
        case WIFI_OTA_UPDATE_COMPLETE:
            smart_ota_update_status(SMART_OTA_COMPLETE, 100);
            break;
        case WIFI_OTA_ERROR:
            smart_ota_update_status(SMART_OTA_ERROR, progress_percent);
            break;
        default:
            break;
    }
}

// OTA completion callback
void smart_ota_complete_callback(wifi_ota_error_t error)
{
    if (error == WIFI_OTA_SUCCESS) {
        app_log_info("Smart OTA update completed successfully!\r\n");
        smart_ota_update_status(SMART_OTA_COMPLETE, 100);
    } else {
        app_log_error("Smart OTA update failed with error: %d\r\n", error);
        smart_ota_update_status(SMART_OTA_ERROR, 0);
    }
}

sl_status_t smart_ota_init(void)
{
    sl_status_t status;

    app_log_info("Smart OTA: Initializing\r\n");

    // Initialize version manager
    status = version_manager_init();
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to initialize version manager: 0x%lx\r\n", status);
        return status;
    }

    // Get current version
    status = version_manager_get_current_version(&current_version);
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to get current version: 0x%lx\r\n", status);
        return status;
    }

    // Set default callbacks if none provided
    if (!status_callback) {
        status_callback = smart_ota_default_status_callback;
    }
    if (!version_callback) {
        version_callback = smart_ota_default_version_callback;
    }

    current_status = SMART_OTA_IDLE;
    app_log_info("Smart OTA initialized successfully\r\n");

    return SL_STATUS_OK;
}

sl_status_t smart_ota_check_and_update(void)
{
    sl_status_t status;

    if (current_status != SMART_OTA_IDLE && current_status != SMART_OTA_NO_UPDATE_NEEDED &&
        current_status != SMART_OTA_COMPLETE && current_status != SMART_OTA_ERROR) {
        app_log_warning("Smart OTA operation already in progress\r\n");
        return SL_STATUS_ALREADY_EXISTS;
    }

    app_log_info("Smart OTA: Starting check and update process\r\n");
    smart_ota_update_status(SMART_OTA_CHECKING_VERSION, 0);

    // Start version check
    status = version_manager_check_for_updates(smart_ota_version_check_callback);
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to start version check: 0x%lx\r\n", status);
        smart_ota_update_status(SMART_OTA_ERROR, 0);
        return status;
    }

    return SL_STATUS_OK;
}

sl_status_t smart_ota_force_update(void)
{
    app_log_info("Smart OTA: Starting forced update\r\n");
    smart_ota_update_status(SMART_OTA_UPDATE_AVAILABLE, 0);

    sl_status_t status = simple_ota_start_with_callbacks(smart_ota_progress_callback, smart_ota_complete_callback);
    if (status == SL_STATUS_OK) {
        smart_ota_update_status(SMART_OTA_DOWNLOADING, 0);
    } else {
        app_log_error("Failed to start forced OTA update: 0x%lx\r\n", status);
        smart_ota_update_status(SMART_OTA_ERROR, 0);
    }

    return status;
}

sl_status_t smart_ota_set_status_callback(smart_ota_status_callback_t callback)
{
    status_callback = callback;
    return SL_STATUS_OK;
}

sl_status_t smart_ota_set_version_callback(smart_ota_version_callback_t callback)
{
    version_callback = callback;
    return SL_STATUS_OK;
}

smart_ota_status_t smart_ota_get_status(void)
{
    return current_status;
}

// Default status callback implementation
void smart_ota_default_status_callback(smart_ota_status_t status, uint32_t progress_percent)
{
    switch (status) {
        case SMART_OTA_IDLE:
            app_log_info("Smart OTA Status: Idle\r\n");
            break;
        case SMART_OTA_CHECKING_VERSION:
            app_log_info("Smart OTA Status: Checking version (%ld%%)\r\n", progress_percent);
            break;
        case SMART_OTA_NO_UPDATE_NEEDED:
            app_log_info("Smart OTA Status: No update needed (%ld%%)\r\n", progress_percent);
            break;
        case SMART_OTA_UPDATE_AVAILABLE:
            app_log_info("Smart OTA Status: Update available (%ld%%)\r\n", progress_percent);
            break;
        case SMART_OTA_DOWNLOADING:
            app_log_info("Smart OTA Status: Downloading firmware (%ld%%)\r\n", progress_percent);
            break;
        case SMART_OTA_UPDATING:
            app_log_info("Smart OTA Status: Updating firmware (%ld%%)\r\n", progress_percent);
            break;
        case SMART_OTA_COMPLETE:
            app_log_info("Smart OTA Status: Update complete (%ld%%)\r\n", progress_percent);
            break;
        case SMART_OTA_ERROR:
            app_log_error("Smart OTA Status: Error (%ld%%)\r\n", progress_percent);
            break;
        default:
            app_log_warning("Smart OTA Status: Unknown status %d (%ld%%)\r\n", status, progress_percent);
            break;
    }
}

// Default version callback implementation
void smart_ota_default_version_callback(firmware_version_t current, firmware_version_t remote, version_compare_result_t result)
{
    app_log_info("=== Firmware Version Check ===\r\n");
    app_log_info("Current version: %d.%d.%d.%d\r\n", current.major, current.minor, current.patch, current.build);
    app_log_info("Remote version:  %d.%d.%d.%d\r\n", remote.major, remote.minor, remote.patch, remote.build);

    switch (result) {
        case VERSION_NEWER:
            app_log_info("Result: Update available - remote version is newer\r\n");
            break;
        case VERSION_SAME:
            app_log_info("Result: No update needed - versions are the same\r\n");
            break;
        case VERSION_OLDER:
            app_log_info("Result: No update needed - remote version is older\r\n");
            break;
    }
    app_log_info("==============================\r\n");
}