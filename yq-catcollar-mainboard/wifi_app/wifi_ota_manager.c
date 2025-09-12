/*******************************************************************************
* @file  wifi_ota_manager.c
* @brief Cat Collar OTA Manager Implementation
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
static osTimerId_t ota_timer;
static osSemaphoreId_t ota_semaphore;

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

// Implementation
sl_status_t catcollar_ota_init(void)
{
    sl_status_t status = SL_STATUS_OK;
    
    app_log_info("Initializing OTA manager...\r\n");
    
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
    
    ota_current_state = OTA_STATE_IDLE;
    ota_last_status = OTA_STATUS_NO_UPDATE_AVAILABLE;
    
    app_log_info("OTA manager initialized successfully\r\n");
    
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
    
    ota_current_state = OTA_STATE_CHECKING_VERSION;
    app_log_info("Checking for firmware updates...\r\n");
    
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
    sl_status_t status = SL_STATUS_OK;
    char server_ip[16] = {0};
    
    // Check if WiFi is connected
    if (catcollar_wifi_connection_get_state() != CATCOLLAR_WIFI_CONNECTED) {
        app_log_error("WiFi not connected, cannot start update\r\n");
        ota_last_status = OTA_STATUS_NETWORK_ERROR;
        return SL_STATUS_FAIL;
    }
    
    ota_current_state = OTA_STATE_DOWNLOADING;
    ota_response_received = false;
    
    app_log_info("Starting OTA firmware update from AWS...\r\n");
    app_log_info("Hostname: %s\r\n", OTA_AWS_HOSTNAME);
    app_log_info("Firmware URL: %s\r\n", OTA_FIRMWARE_URL);
    
    // DNS resolution for AWS hostname
    sl_ip_address_t dns_query_rsp = { 0 };
    sl_net_dns_resolve_hostname(OTA_AWS_HOSTNAME, OTA_TIMEOUT_MS, 
                               SL_NET_DNS_TYPE_IPV4, &dns_query_rsp);
    
    snprintf(server_ip, sizeof(server_ip), "%d.%d.%d.%d",
             dns_query_rsp.ip.v4.bytes[0], dns_query_rsp.ip.v4.bytes[1],
             dns_query_rsp.ip.v4.bytes[2], dns_query_rsp.ip.v4.bytes[3]);
    
    app_log_info("Resolved AWS IP: %s\r\n", server_ip);
    
    // Setup HTTP OTAF parameters
    sl_si91x_http_otaf_params_t http_params = { 0 };
    http_params.flags = (uint16_t)OTA_FLAGS;
    http_params.ip_address = (uint8_t *)server_ip;
    http_params.port = (uint16_t)OTA_HTTP_PORT;
    http_params.resource = (uint8_t *)OTA_FIRMWARE_URL;
    http_params.host_name = (uint8_t *)OTA_AWS_HOSTNAME;
    http_params.extended_header = NULL;
    http_params.user_name = (uint8_t *)"";
    http_params.password = (uint8_t *)"";
    
    // Set callback for HTTP response  
    sl_wifi_set_callback(SL_WIFI_HTTP_OTA_FW_UPDATE_EVENTS, 
                        (sl_wifi_callback_function_t)&ota_http_response_callback, 
                        NULL);
    
    // Start HTTP OTAF
    app_log_info("Downloading firmware from AWS S3...\r\n");
    status = sl_si91x_http_otaf_v2(&http_params);
    
    if (status == SL_STATUS_IN_PROGRESS) {
        // Wait for download completion
        osStatus_t sem_status = osSemaphoreAcquire(ota_semaphore, OTA_TIMEOUT_MS);
        if (sem_status == osOK && ota_response_received) {
            app_log_info("Firmware download completed successfully\r\n");
            ota_current_state = OTA_STATE_INSTALLING;
            ota_last_status = OTA_STATUS_DOWNLOAD_SUCCESS;
            
            app_log_info("Installing firmware update...\r\n");
            ota_current_state = OTA_STATE_REBOOTING;
            app_log_info("Rebooting to apply firmware update...\r\n");
            
            // For TA firmware update, the device will automatically reboot
            // For M4 firmware update, manual reboot is required
#if (OTA_FW_UPDATE_TYPE == M4_FW_UPDATE)
            osDelay(1000);
            NVIC_SystemReset();
#endif
            
            ota_current_state = OTA_STATE_SUCCESS;
            status = SL_STATUS_OK;
        } else {
            app_log_error("Firmware download timeout or failed\r\n");
            ota_current_state = OTA_STATE_FAILED;
            ota_last_status = OTA_STATUS_DOWNLOAD_FAILED;
            status = SL_STATUS_TIMEOUT;
        }
    } else if (status != SL_STATUS_OK) {
        app_log_error("Failed to start firmware download: 0x%lx\r\n", status);
        ota_current_state = OTA_STATE_FAILED;
        ota_last_status = OTA_STATUS_DOWNLOAD_FAILED;
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

// Private functions
static sl_status_t ota_load_certificate(void)
{
#ifdef OTA_AWS_ENABLE
    app_log_info("Loading AWS Starfield CA certificate...\r\n");
    
    sl_status_t status = sl_net_set_credential(SL_NET_TLS_SERVER_CREDENTIAL_ID(OTA_CERTIFICATE_INDEX),
                                             SL_NET_SIGNING_CERTIFICATE, 
                                             aws_starfield_ca, 
                                             sizeof(aws_starfield_ca) - 1);
    
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to set TLS certificate: 0x%lx\r\n", status);
        return status;
    }
    
    app_log_info("TLS certificate loaded successfully\r\n");
    return SL_STATUS_OK;
#else
    return SL_STATUS_OK;
#endif
}

static sl_status_t ota_check_version_from_server(catcollar_firmware_version_t *server_version)
{
    // For now, simulate server version check
    // In a real implementation, you would make an HTTP GET request to OTA_VERSION_CHECK_URL
    // and parse the JSON response to get the version information
    
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
        app_log_error("OTA HTTP response failed\r\n");
        ota_response_received = false;
        ota_current_state = OTA_STATE_FAILED;
        ota_last_status = OTA_STATUS_DOWNLOAD_FAILED;
    } else {
        app_log_info("OTA HTTP response received successfully\r\n");
        ota_response_received = true;
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
    
    // If update is available, you could optionally auto-start the update
    // or just log the availability for manual triggering
    if (ota_last_status == OTA_STATUS_UPDATE_AVAILABLE) {
        app_log_info("OTA update available - manual trigger required\r\n");
    }
}
