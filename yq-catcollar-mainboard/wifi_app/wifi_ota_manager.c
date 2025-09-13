/*******************************************************************************
* @file  wifi_ota_manager.c
* @brief Cat Collar Enhanced OTA Manager Implementation with AWS S3 ap-east-1
*******************************************************************************
* # License
* <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* The licensor of this software is Silicon Laboratories Inc. Your use of this
* software is governed by the terms of Silicon Labs Master Software License
* Agreement (MSLA) available at
* www.silabs.com/about-us/legal/master-software-license-agreement. This
* software is distributed to you in Source Code format and is governed by the
* sections of the MSLA applicable to Source Code.
*
******************************************************************************/

#include "wifi_ota_manager.h"
#include "wifi_ota_config.h"
#include "wifi_app.h"
#include "app_log.h"
#include "sl_wifi.h"
#include "sl_net.h"
#include "sl_net_dns.h"
#include "sl_wifi_callback_framework.h"
#include "sl_net_si91x.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// Include AWS certificate for HTTPS
#ifdef OTA_AWS_ENABLE
#include "aws_starfield_ca.pem.h"
#endif

// Static variables
static catcollar_ota_state_t ota_current_state = OTA_STATE_IDLE;
static catcollar_ota_status_t ota_last_status = OTA_STATUS_NO_UPDATE_AVAILABLE;
static volatile bool ota_response_received = false;
static volatile bool ota_operation_aborted = false;
static osTimerId_t ota_timer;
static osSemaphoreId_t ota_semaphore;
static uint32_t ota_bytes_downloaded = 0;
static uint32_t ota_total_bytes = 0;
static uint8_t ota_retry_count = 0;
static catcollar_ota_config_t ota_config;
static catcollar_ota_progress_callback_t ota_progress_callback = NULL;

// Function prototypes
static sl_status_t ota_http_response_callback(sl_wifi_event_t event,
                                            uint16_t *data,
                                            uint32_t data_length,
                                            void *arg);
static sl_status_t ota_load_certificate(void);
static sl_status_t ota_check_version_from_server(catcollar_firmware_version_t *server_version);
static bool ota_compare_versions(catcollar_firmware_version_t *current,
                                catcollar_firmware_version_t *server);
static void ota_timer_callback(void *argument);
static sl_status_t ota_resolve_dns_with_retry(const char *hostname, char *ip_address, size_t ip_size);
static void ota_reset_state(void);
static sl_status_t ota_perform_update_with_retry(void);

// Implementation
sl_status_t catcollar_ota_init(void)
{
    // Initialize with default configuration
    memset(&ota_config, 0, sizeof(ota_config));
    strcpy(ota_config.aws_hostname, OTA_AWS_HOSTNAME);
    strcpy(ota_config.firmware_url, OTA_FIRMWARE_URL);
    strcpy(ota_config.version_check_url, OTA_VERSION_CHECK_URL);
    ota_config.port = OTA_HTTP_PORT;
    ota_config.timeout_ms = OTA_TIMEOUT_MS;
    ota_config.max_retry_count = OTA_MAX_RETRY_COUNT;
    ota_config.certificate_index = OTA_CERTIFICATE_INDEX;
    ota_config.progress_callback = NULL;

    return catcollar_ota_init_with_config(&ota_config);
}

sl_status_t catcollar_ota_init_with_config(catcollar_ota_config_t *config)
{
    sl_status_t status = SL_STATUS_OK;

    if (config == NULL) {
        app_log_error("OTA config parameter is NULL\r\n");
        return SL_STATUS_NULL_POINTER;
    }

    app_log_info("Initializing Enhanced OTA manager for AWS S3 ap-east-1...\r\n");

    // Copy configuration
    memcpy(&ota_config, config, sizeof(catcollar_ota_config_t));

    // Create semaphore for OTA operations
    ota_semaphore = osSemaphoreNew(1, 0, NULL);
    if (ota_semaphore == NULL) {
        app_log_error("Failed to create OTA semaphore\r\n");
        return SL_STATUS_FAIL;
    }

    // Create timer for periodic version checks
    ota_timer = osTimerNew(ota_timer_callback, osTimerPeriodic, NULL, NULL);
    if (ota_timer == NULL) {
        app_log_error("Failed to create OTA timer\r\n");
        return SL_STATUS_FAIL;
    }

#ifdef OTA_LOAD_CERTIFICATE
    // Load SSL certificate for HTTPS connections
    status = ota_load_certificate();
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to load OTA certificate: 0x%lx\r\n", status);
        return status;
    }
#endif

    ota_reset_state();

    app_log_info("Enhanced OTA manager initialized successfully\r\n");
    app_log_info("AWS S3 Hostname: %s\r\n", ota_config.aws_hostname);
    app_log_info("Firmware URL: %s\r\n", ota_config.firmware_url);

    return status;
}

sl_status_t catcollar_ota_check_for_updates(void)
{
    sl_status_t status = SL_STATUS_OK;
    catcollar_firmware_version_t server_version;
    catcollar_firmware_version_t current_version;

    // Check if WiFi is connected
    if (catcollar_wifi_connection_get_state() != CATCOLLAR_WIFI_CONNECTED) {
        app_log_error("WiFi not connected, cannot check for updates\r\n");
        ota_last_status = OTA_STATUS_NETWORK_ERROR;
        return SL_STATUS_FAIL;
    }

    if (ota_current_state != OTA_STATE_IDLE) {
        app_log_warning("OTA operation already in progress\r\n");
        return SL_STATUS_BUSY;
    }

    ota_current_state = OTA_STATE_CHECKING_VERSION;
    app_log_info("Checking for firmware updates from AWS S3 ap-east-1...\r\n");

    // Get current firmware version
    current_version = catcollar_get_current_version();
    app_log_info("Current firmware version: %d.%d.%d.%d\r\n",
                current_version.major, current_version.minor,
                current_version.patch, current_version.build);

    // Check server version
    status = ota_check_version_from_server(&server_version);
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to check server version: 0x%lx\r\n", status);
        ota_current_state = OTA_STATE_FAILED;
        ota_last_status = OTA_STATUS_NETWORK_ERROR;
        return status;
    }

    app_log_info("Server firmware version: %d.%d.%d.%d\r\n",
                server_version.major, server_version.minor,
                server_version.patch, server_version.build);

    // Compare versions
    if (ota_compare_versions(&current_version, &server_version)) {
        app_log_info("New firmware version available!\r\n");
        ota_current_state = OTA_STATE_IDLE;
        ota_last_status = OTA_STATUS_UPDATE_AVAILABLE;
    } else {
        app_log_info("Firmware is up to date\r\n");
        ota_current_state = OTA_STATE_IDLE;
        ota_last_status = OTA_STATUS_NO_UPDATE_AVAILABLE;
    }

    return SL_STATUS_OK;
}

sl_status_t catcollar_ota_start_update(void)
{
    return catcollar_ota_start_update_with_retry(ota_config.max_retry_count);
}

sl_status_t catcollar_ota_start_update_with_retry(uint8_t max_retries)
{
    sl_status_t status = SL_STATUS_OK;

    // Check if WiFi is connected
    if (catcollar_wifi_connection_get_state() != CATCOLLAR_WIFI_CONNECTED) {
        app_log_error("WiFi not connected, cannot start update\r\n");
        ota_last_status = OTA_STATUS_NETWORK_ERROR;
        return SL_STATUS_FAIL;
    }

    if (ota_current_state != OTA_STATE_IDLE) {
        app_log_warning("OTA operation already in progress\r\n");
        return SL_STATUS_BUSY;
    }

    ota_retry_count = 0;
    ota_operation_aborted = false;
    ota_config.max_retry_count = max_retries;

    app_log_info("Starting Enhanced OTA firmware update from AWS S3 ap-east-1...\r\n");

    status = ota_perform_update_with_retry();

    return status;
}

sl_status_t catcollar_ota_check_and_update(void)
{
    sl_status_t status = SL_STATUS_OK;

    // First check for updates
    status = catcollar_ota_check_for_updates();
    if (status != SL_STATUS_OK) {
        return status;
    }

    // If update is available, start the update
    if (ota_last_status == OTA_STATUS_UPDATE_AVAILABLE) {
        app_log_info("Update available, starting download...\r\n");
        status = catcollar_ota_start_update();
    } else {
        app_log_info("No update available\r\n");
    }

    return status;
}

catcollar_ota_state_t catcollar_ota_get_state(void)
{
    return ota_current_state;
}

catcollar_ota_status_t catcollar_ota_get_status(void)
{
    return ota_last_status;
}

catcollar_firmware_version_t catcollar_get_current_version(void)
{
    catcollar_firmware_version_t version;
    version.major = CATCOLLAR_FIRMWARE_VERSION_MAJOR;
    version.minor = CATCOLLAR_FIRMWARE_VERSION_MINOR;
    version.patch = CATCOLLAR_FIRMWARE_VERSION_PATCH;
    version.build = CATCOLLAR_FIRMWARE_VERSION_BUILD;
    return version;
}

sl_status_t catcollar_ota_start_periodic_check(void)
{
    // Start timer for periodic version checks (24 hours)
    osStatus_t status = osTimerStart(ota_timer, OTA_VERSION_CHECK_INTERVAL_MS);
    if (status != osOK) {
        app_log_error("Failed to start OTA periodic timer\r\n");
        return SL_STATUS_FAIL;
    }

    app_log_info("Started periodic OTA checks (every 24 hours)\r\n");
    return SL_STATUS_OK;
}

sl_status_t catcollar_ota_stop_periodic_check(void)
{
    osStatus_t status = osTimerStop(ota_timer);
    if (status != osOK) {
        app_log_error("Failed to stop OTA periodic timer\r\n");
        return SL_STATUS_FAIL;
    }

    app_log_info("Stopped periodic OTA checks\r\n");
    return SL_STATUS_OK;
}

sl_status_t catcollar_ota_abort(void)
{
    app_log_warning("Aborting OTA operation...\r\n");
    ota_operation_aborted = true;
    ota_current_state = OTA_STATE_FAILED;
    ota_last_status = OTA_STATUS_DOWNLOAD_FAILED;

    // Release semaphore if waiting
    osSemaphoreRelease(ota_semaphore);

    return SL_STATUS_OK;
}

uint8_t catcollar_ota_get_progress_percentage(void)
{
    if (ota_total_bytes == 0) {
        return 0;
    }

    uint8_t percentage = (uint8_t)((ota_bytes_downloaded * 100) / ota_total_bytes);
    return (percentage > 100) ? 100 : percentage;
}

sl_status_t catcollar_ota_set_progress_callback(catcollar_ota_progress_callback_t callback)
{
    ota_progress_callback = callback;
    return SL_STATUS_OK;
}

const char* catcollar_ota_state_to_string(catcollar_ota_state_t state)
{
    switch (state) {
        case OTA_STATE_IDLE: return "IDLE";
        case OTA_STATE_INITIALIZING: return "INITIALIZING";
        case OTA_STATE_RESOLVING_DNS: return "RESOLVING_DNS";
        case OTA_STATE_CHECKING_VERSION: return "CHECKING_VERSION";
        case OTA_STATE_DOWNLOADING: return "DOWNLOADING";
        case OTA_STATE_VERIFYING: return "VERIFYING";
        case OTA_STATE_INSTALLING: return "INSTALLING";
        case OTA_STATE_REBOOTING: return "REBOOTING";
        case OTA_STATE_SUCCESS: return "SUCCESS";
        case OTA_STATE_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

const char* catcollar_ota_status_to_string(catcollar_ota_status_t status)
{
    switch (status) {
        case OTA_STATUS_NO_UPDATE_AVAILABLE: return "NO_UPDATE_AVAILABLE";
        case OTA_STATUS_UPDATE_AVAILABLE: return "UPDATE_AVAILABLE";
        case OTA_STATUS_DOWNLOAD_IN_PROGRESS: return "DOWNLOAD_IN_PROGRESS";
        case OTA_STATUS_DOWNLOAD_SUCCESS: return "DOWNLOAD_SUCCESS";
        case OTA_STATUS_DOWNLOAD_FAILED: return "DOWNLOAD_FAILED";
        case OTA_STATUS_VERIFY_FAILED: return "VERIFY_FAILED";
        case OTA_STATUS_INSTALL_FAILED: return "INSTALL_FAILED";
        case OTA_STATUS_NETWORK_ERROR: return "NETWORK_ERROR";
        case OTA_STATUS_DNS_RESOLUTION_FAILED: return "DNS_RESOLUTION_FAILED";
        case OTA_STATUS_CERTIFICATE_ERROR: return "CERTIFICATE_ERROR";
        case OTA_STATUS_TIMEOUT_ERROR: return "TIMEOUT_ERROR";
        default: return "UNKNOWN";
    }
}

// Private functions
static sl_status_t ota_perform_update_with_retry(void)
{
    sl_status_t status = SL_STATUS_OK;
    char server_ip[16] = {0};

    for (ota_retry_count = 0; ota_retry_count <= ota_config.max_retry_count; ota_retry_count++) {
        if (ota_operation_aborted) {
            app_log_warning("OTA operation aborted by user\r\n");
            return SL_STATUS_ABORT;
        }

        if (ota_retry_count > 0) {
            app_log_info("OTA retry attempt %d/%d\r\n", ota_retry_count, ota_config.max_retry_count);
            osDelay(5000); // Wait 5 seconds between retries
        }

        ota_current_state = OTA_STATE_INITIALIZING;
        ota_response_received = false;
        ota_bytes_downloaded = 0;
        ota_total_bytes = 0;

        // DNS resolution with retry
        ota_current_state = OTA_STATE_RESOLVING_DNS;
        status = ota_resolve_dns_with_retry(ota_config.aws_hostname, server_ip, sizeof(server_ip));
        if (status != SL_STATUS_OK) {
            app_log_error("DNS resolution failed after retries: 0x%lx\r\n", status);
            ota_last_status = OTA_STATUS_DNS_RESOLUTION_FAILED;
            continue;
        }

        app_log_info("Resolved AWS S3 IP: %s\r\n", server_ip);

        ota_current_state = OTA_STATE_DOWNLOADING;
        ota_last_status = OTA_STATUS_DOWNLOAD_IN_PROGRESS;

        // Setup HTTP OTAF parameters with enhanced flags
        sl_si91x_http_otaf_params_t http_params = { 0 };

        uint16_t flags = OTA_ENHANCED_FLAGS;
        if (ota_config.certificate_index == 1) {
            flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_1;
        } else if (ota_config.certificate_index == 2) {
            flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_2;
        }

        http_params.flags = flags;
        http_params.ip_address = (uint8_t *)server_ip;
        http_params.port = (uint16_t)ota_config.port;
        http_params.resource = (uint8_t *)ota_config.firmware_url;
        http_params.host_name = (uint8_t *)ota_config.aws_hostname;
        http_params.extended_header = NULL;
        http_params.user_name = (uint8_t *)"";
        http_params.password = (uint8_t *)"";

        // Set callback for HTTP response
        sl_wifi_set_callback(SL_WIFI_HTTP_OTA_FW_UPDATE_EVENTS,
                            (sl_wifi_callback_function_t)&ota_http_response_callback,
                            NULL);

        // Start HTTP OTAF
        app_log_info("Downloading firmware from AWS S3 ap-east-1...\r\n");
        status = sl_si91x_http_otaf_v2(&http_params);

        if (status == SL_STATUS_IN_PROGRESS) {
            // Wait for download completion
            osStatus_t sem_status = osSemaphoreAcquire(ota_semaphore, ota_config.timeout_ms);
            if (sem_status == osOK && ota_response_received && !ota_operation_aborted) {
                app_log_info("Firmware download completed successfully\r\n");
                ota_current_state = OTA_STATE_INSTALLING;
                ota_last_status = OTA_STATUS_DOWNLOAD_SUCCESS;

                app_log_info("Installing firmware update...\r\n");
                ota_current_state = OTA_STATE_REBOOTING;
                app_log_info("Rebooting to apply firmware update...\r\n");

                // For TA firmware update, the device will automatically reboot
                // For M4 firmware update, manual reboot is required
#if (OTA_FW_UPDATE_TYPE == M4_FW_UPDATE)
                osDelay(2000);
                NVIC_SystemReset();
#endif

                ota_current_state = OTA_STATE_SUCCESS;
                return SL_STATUS_OK;

            } else {
                if (ota_operation_aborted) {
                    app_log_warning("Firmware download aborted by user\r\n");
                    return SL_STATUS_ABORT;
                } else {
                    app_log_error("Firmware download timeout or failed on attempt %d\r\n", ota_retry_count + 1);
                    ota_last_status = (sem_status == osErrorTimeout) ?
                                     OTA_STATUS_TIMEOUT_ERROR : OTA_STATUS_DOWNLOAD_FAILED;
                }
            }
        } else {
            app_log_error("Failed to start firmware download: 0x%lx\r\n", status);
            ota_last_status = OTA_STATUS_DOWNLOAD_FAILED;
        }
    }

    // All retries exhausted
    app_log_error("OTA update failed after %d attempts\r\n", ota_config.max_retry_count + 1);
    ota_current_state = OTA_STATE_FAILED;
    return SL_STATUS_FAIL;
}

static sl_status_t ota_resolve_dns_with_retry(const char *hostname, char *ip_address, size_t ip_size)
{
    sl_status_t status = SL_STATUS_OK;
    sl_ip_address_t dns_query_rsp = { 0 };
    int dns_retry_count = 0;

    app_log_info("Resolving DNS for hostname: %s\r\n", hostname);

    do {
        status = sl_net_dns_resolve_hostname(hostname, OTA_DNS_TIMEOUT_MS,
                                           SL_NET_DNS_TYPE_IPV4, &dns_query_rsp);

        if (status == SL_STATUS_OK) {
            snprintf(ip_address, ip_size, "%d.%d.%d.%d",
                     dns_query_rsp.ip.v4.bytes[0], dns_query_rsp.ip.v4.bytes[1],
                     dns_query_rsp.ip.v4.bytes[2], dns_query_rsp.ip.v4.bytes[3]);
            app_log_info("DNS resolution successful: %s -> %s\r\n", hostname, ip_address);
            return SL_STATUS_OK;
        }

        dns_retry_count++;
        app_log_warning("DNS resolution attempt %d failed: 0x%lx\r\n", dns_retry_count, status);

        if (dns_retry_count < OTA_MAX_DNS_RETRY_COUNT) {
            osDelay(2000); // Wait 2 seconds between DNS retries
        }

    } while (dns_retry_count < OTA_MAX_DNS_RETRY_COUNT && !ota_operation_aborted);

    app_log_error("DNS resolution failed after %d attempts\r\n", OTA_MAX_DNS_RETRY_COUNT);
    return status;
}

static sl_status_t ota_load_certificate(void)
{
#ifdef OTA_AWS_ENABLE
    app_log_info("Loading AWS Starfield CA certificate for ap-east-1...\r\n");

    sl_status_t status = sl_net_set_credential(SL_NET_TLS_SERVER_CREDENTIAL_ID(ota_config.certificate_index),
                                             SL_NET_SIGNING_CERTIFICATE,
                                             aws_starfield_ca,
                                             sizeof(aws_starfield_ca) - 1);

    if (status != SL_STATUS_OK) {
        app_log_error("Failed to set TLS certificate: 0x%lx\r\n", status);
        ota_last_status = OTA_STATUS_CERTIFICATE_ERROR;
        return status;
    }

    app_log_info("TLS certificate loaded successfully at index %d\r\n", ota_config.certificate_index);
    return SL_STATUS_OK;
#else
    app_log_info("Certificate loading disabled\r\n");
    return SL_STATUS_OK;
#endif
}

static sl_status_t ota_check_version_from_server(catcollar_firmware_version_t *server_version)
{
    // For now, simulate server version check
    // In a real implementation, you would make an HTTP GET request to ota_config.version_check_url
    // and parse the JSON response to get the version information

    app_log_info("Checking version from: %s%s\r\n", ota_config.aws_hostname, ota_config.version_check_url);

    // Simulated server response - newer version available
    server_version->major = CATCOLLAR_FIRMWARE_VERSION_MAJOR;
    server_version->minor = CATCOLLAR_FIRMWARE_VERSION_MINOR;
    server_version->patch = CATCOLLAR_FIRMWARE_VERSION_PATCH;
    server_version->build = CATCOLLAR_FIRMWARE_VERSION_BUILD + 1; // Simulate newer build

    return SL_STATUS_OK;
}

static bool ota_compare_versions(catcollar_firmware_version_t *current,
                                catcollar_firmware_version_t *server)
{
    // Return true if server version is newer than current version
    if (server->major > current->major) return true;
    if (server->major < current->major) return false;

    if (server->minor > current->minor) return true;
    if (server->minor < current->minor) return false;

    if (server->patch > current->patch) return true;
    if (server->patch < current->patch) return false;

    if (server->build > current->build) return true;

    return false; // Server version is same or older
}

static sl_status_t ota_http_response_callback(sl_wifi_event_t event,
                                            uint16_t *data,
                                            uint32_t data_length,
                                            void *arg)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(data_length);
    UNUSED_PARAMETER(arg);

    if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
        app_log_error("OTA HTTP response failed, event: 0x%lx\r\n", event);
        ota_response_received = false;
        ota_current_state = OTA_STATE_FAILED;
        ota_last_status = OTA_STATUS_DOWNLOAD_FAILED;
    } else {
        app_log_info("OTA HTTP response received successfully\r\n");
        ota_response_received = true;

        // Update progress if callback is set
        if (ota_progress_callback) {
            ota_progress_callback(ota_bytes_downloaded, ota_total_bytes);
        }
    }

    osSemaphoreRelease(ota_semaphore);
    return SL_STATUS_OK;
}

static void ota_timer_callback(void *argument)
{
    UNUSED_PARAMETER(argument);

    app_log_info("Periodic OTA check triggered\r\n");

    // Check for updates automatically
    catcollar_ota_check_for_updates();

    // Log the result for manual decision
    if (ota_last_status == OTA_STATUS_UPDATE_AVAILABLE) {
        app_log_info("OTA update available - manual trigger required\r\n");
    }
}

static void ota_reset_state(void)
{
    ota_current_state = OTA_STATE_IDLE;
    ota_last_status = OTA_STATUS_NO_UPDATE_AVAILABLE;
    ota_response_received = false;
    ota_operation_aborted = false;
    ota_bytes_downloaded = 0;
    ota_total_bytes = 0;
    ota_retry_count = 0;
}