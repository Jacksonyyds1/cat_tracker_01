#include "simple_ota.h"
#include "wifi_ota_manager.h"
#include "app_log.h"

// Default progress callback implementation
void simple_ota_default_progress_callback(wifi_ota_status_t status, uint32_t progress_percent)
{
    switch (status) {
        case WIFI_OTA_IDLE:
            app_log_info("OTA Status: Idle\r\n");
            break;
        case WIFI_OTA_INITIALIZING:
            app_log_info("OTA Status: Initializing (%ld%%)\r\n", progress_percent);
            break;
        case WIFI_OTA_DOWNLOADING:
            app_log_info("OTA Status: Downloading firmware (%ld%%)\r\n", progress_percent);
            break;
        case WIFI_OTA_DOWNLOAD_COMPLETE:
            app_log_info("OTA Status: Download complete (%ld%%)\r\n", progress_percent);
            break;
        case WIFI_OTA_UPDATING:
            app_log_info("OTA Status: Updating firmware (%ld%%)\r\n", progress_percent);
            break;
        case WIFI_OTA_UPDATE_COMPLETE:
            app_log_info("OTA Status: Update complete (%ld%%)\r\n", progress_percent);
            break;
        case WIFI_OTA_ERROR:
            app_log_error("OTA Status: Error (%ld%%)\r\n", progress_percent);
            break;
        default:
            app_log_warning("OTA Status: Unknown status %d (%ld%%)\r\n", status, progress_percent);
            break;
    }
}

// Default completion callback implementation
void simple_ota_default_complete_callback(wifi_ota_error_t error)
{
    switch (error) {
        case WIFI_OTA_SUCCESS:
            app_log_info("OTA Update completed successfully!\r\n");
            app_log_info("System will restart to apply the new firmware...\r\n");
            break;
        case WIFI_OTA_ERROR_DNS_RESOLUTION:
            app_log_error("OTA Update failed: DNS resolution error\r\n");
            break;
        case WIFI_OTA_ERROR_CONNECTION_FAILED:
            app_log_error("OTA Update failed: Connection failed\r\n");
            break;
        case WIFI_OTA_ERROR_DOWNLOAD_FAILED:
            app_log_error("OTA Update failed: Download failed\r\n");
            break;
        case WIFI_OTA_ERROR_UPDATE_FAILED:
            app_log_error("OTA Update failed: Update failed\r\n");
            break;
        case WIFI_OTA_ERROR_CERTIFICATE_LOAD_FAILED:
            app_log_error("OTA Update failed: Certificate load failed\r\n");
            break;
        case WIFI_OTA_ERROR_TIMEOUT:
            app_log_error("OTA Update failed: Timeout\r\n");
            break;
        default:
            app_log_error("OTA Update failed: Unknown error %d\r\n", error);
            break;
    }
}

sl_status_t simple_ota_start(void)
{
    return simple_ota_start_with_callbacks(simple_ota_default_progress_callback,
                                         simple_ota_default_complete_callback);
}

sl_status_t simple_ota_start_with_callbacks(wifi_ota_progress_callback_t progress_cb,
                                          wifi_ota_complete_callback_t complete_cb)
{
    sl_status_t status;

    // Initialize OTA manager
    status = wifi_ota_init();
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to initialize OTA manager: 0x%lX\r\n", status);
        return status;
    }

    // Set callbacks
    if (progress_cb) {
        status = wifi_ota_set_progress_callback(progress_cb);
        if (status != SL_STATUS_OK) {
            app_log_error("Failed to set progress callback: 0x%lX\r\n", status);
            return status;
        }
    }

    if (complete_cb) {
        status = wifi_ota_set_complete_callback(complete_cb);
        if (status != SL_STATUS_OK) {
            app_log_error("Failed to set complete callback: 0x%lX\r\n", status);
            return status;
        }
    }

    // Start OTA update
    status = wifi_ota_start_update();
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to start OTA update: 0x%lX\r\n", status);
        return status;
    }

    app_log_info("OTA update started successfully\r\n");
    return SL_STATUS_OK;
}

bool simple_ota_is_in_progress(void)
{
    wifi_ota_status_t status = wifi_ota_get_status();
    return (status == WIFI_OTA_INITIALIZING ||
            status == WIFI_OTA_DOWNLOADING ||
            status == WIFI_OTA_UPDATING);
}

wifi_ota_status_t simple_ota_get_status(void)
{
    return wifi_ota_get_status();
}

sl_status_t simple_ota_cancel(void)
{
    sl_status_t status = wifi_ota_cancel();
    if (status == SL_STATUS_OK) {
        app_log_info("OTA update cancelled\r\n");
    } else if (status == SL_STATUS_INVALID_STATE) {
        app_log_warning("No OTA update in progress to cancel\r\n");
    } else {
        app_log_error("Failed to cancel OTA update: 0x%lX\r\n", status);
    }
    return status;
}