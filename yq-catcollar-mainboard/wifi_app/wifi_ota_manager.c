#include "wifi_ota_manager.h"
#include "wifi_ota_config.h"
#include "aws_starfield_ca.pem.h"

#include "sl_wifi.h"
#include "sl_net.h"
#include "sl_net_dns.h"
#include "sl_net_si91x.h"
#include "sl_utility.h"
#include "sl_wifi_callback_framework.h"
#include "firmware_upgradation.h"

#include "app_log.h"
#include <string.h>
#include <stdio.h>

// Static variables
static wifi_ota_status_t current_ota_status = WIFI_OTA_IDLE;
static wifi_ota_progress_callback_t progress_callback = NULL;
static wifi_ota_complete_callback_t complete_callback = NULL;
static volatile bool ota_response = false;
static volatile sl_status_t ota_callback_status = SL_STATUS_OK;

// Server configuration
#ifdef AWS_ENABLE
static char *hostname = AWS_S3_HOSTNAME;
#elif defined(AZURE_ENABLE)
static char *hostname = AZURE_HOSTNAME;
#else
static char *hostname = HTTP_HOSTNAME;
#endif

// Internal function to update status and notify callbacks
static void wifi_ota_update_status(wifi_ota_status_t new_status, uint32_t progress)
{
    current_ota_status = new_status;
    if (progress_callback) {
        progress_callback(new_status, progress);
    }
}

// Internal function to notify completion
static void wifi_ota_notify_complete(wifi_ota_error_t error)
{
    if (error == WIFI_OTA_SUCCESS) {
        current_ota_status = WIFI_OTA_UPDATE_COMPLETE;
    } else {
        current_ota_status = WIFI_OTA_ERROR;
    }

    if (complete_callback) {
        complete_callback(error);
    }
}

sl_status_t wifi_ota_init(void)
{
    app_log_info("WiFi OTA Manager: Initializing\r\n");

    current_ota_status = WIFI_OTA_IDLE;
    progress_callback = NULL;
    complete_callback = NULL;
    ota_response = false;
    ota_callback_status = SL_STATUS_OK;

    return SL_STATUS_OK;
}

sl_status_t wifi_ota_set_progress_callback(wifi_ota_progress_callback_t callback)
{
    progress_callback = callback;
    return SL_STATUS_OK;
}

sl_status_t wifi_ota_set_complete_callback(wifi_ota_complete_callback_t callback)
{
    complete_callback = callback;
    return SL_STATUS_OK;
}

wifi_ota_status_t wifi_ota_get_status(void)
{
    return current_ota_status;
}

sl_status_t wifi_ota_cancel(void)
{
    if (current_ota_status == WIFI_OTA_DOWNLOADING ||
        current_ota_status == WIFI_OTA_UPDATING) {
        current_ota_status = WIFI_OTA_ERROR;
        wifi_ota_notify_complete(WIFI_OTA_ERROR_TIMEOUT);
        return SL_STATUS_OK;
    }

    return SL_STATUS_INVALID_STATE;
}

#if LOAD_CERTIFICATE
sl_status_t wifi_ota_load_certificates(void)
{
    sl_status_t status;
    void *cert = NULL;
    uint32_t cert_length = 0;

#ifdef AWS_ENABLE
    cert = (void *)aws_starfield_ca;
    cert_length = (sizeof(aws_starfield_ca) - 1);
    app_log_info("Loading AWS Starfield CA certificate\r\n");
#elif defined(AZURE_ENABLE)
    // Azure certificate would go here
    app_log_info("Loading Azure certificate\r\n");
    return SL_STATUS_NOT_SUPPORTED;
#else
    // Local server certificate
    app_log_info("Loading local server certificate\r\n");
    return SL_STATUS_NOT_SUPPORTED;
#endif

    // Load SSL CA certificate
    status = sl_net_set_credential(SL_NET_TLS_SERVER_CREDENTIAL_ID(OTA_CERTIFICATE_INDEX),
                                   SL_NET_SIGNING_CERTIFICATE,
                                   cert,
                                   cert_length);
    if (status != SL_STATUS_OK) {
        app_log_error("Loading TLS CA certificate failed, Error Code : 0x%lX\r\n", status);
        return status;
    } else {
        app_log_info("TLS CA certificate loaded successfully at index %d\r\n", OTA_CERTIFICATE_INDEX);
    }

    return status;
}
#endif

sl_status_t wifi_ota_dns_resolve(char *hostname, char *ip_buffer)
{
    sl_status_t status;
    sl_ip_address_t dns_query_rsp = { 0 };
    uint32_t server_address;
    int32_t dns_retry_count = OTA_MAX_DNS_RETRY_COUNT;

    app_log_info("Resolving hostname: %s\r\n", hostname);

    do {
        status = sl_net_dns_resolve_hostname((const char *)hostname,
                                           OTA_DNS_TIMEOUT,
                                           SL_NET_DNS_TYPE_IPV4,
                                           &dns_query_rsp);
        dns_retry_count--;
    } while ((dns_retry_count != 0) && (status != SL_STATUS_OK));

    if (status != SL_STATUS_OK) {
        app_log_error("DNS resolution failed, Error 0x%lX\r\n", status);
        return status;
    }

    server_address = dns_query_rsp.ip.v4.value;
    sprintf(ip_buffer,
            "%ld.%ld.%ld.%ld",
            server_address & 0x000000ff,
            (server_address & 0x0000ff00) >> 8,
            (server_address & 0x00ff0000) >> 16,
            (server_address & 0xff000000) >> 24);

    app_log_info("Resolved IP address: %s\r\n", ip_buffer);
    return SL_STATUS_OK;
}

sl_status_t wifi_ota_fw_update_response_handler(sl_wifi_event_t event,
                                               uint16_t *data,
                                               uint32_t data_length,
                                               void *arg)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(data_length);
    UNUSED_PARAMETER(arg);

    if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
        ota_response = false;
        ota_callback_status = SL_STATUS_FAIL;
        app_log_error("OTA firmware update failed\r\n");
        wifi_ota_notify_complete(WIFI_OTA_ERROR_UPDATE_FAILED);
        return SL_STATUS_FAIL;
    }

    ota_response = true;
    ota_callback_status = SL_STATUS_OK;
    app_log_info("OTA firmware update completed successfully\r\n");
    wifi_ota_notify_complete(WIFI_OTA_SUCCESS);
    return SL_STATUS_OK;
}

sl_status_t wifi_ota_download_firmware(void)
{
    sl_status_t status;
    char server_ip[16];
    uint16_t flags = FLAGS;

#if (FW_UPDATE_TYPE == TA_FW_UPDATE)
    // Print current firmware version (like wifi_http_otaf_soc does)
    sl_wifi_firmware_version_t version = { 0 };
    status = sl_wifi_get_firmware_version(&version);
    if (status == SL_STATUS_OK) {
        app_log_info("Current firmware version: Chip ID: %d, ROM ID: %d, Major: %d, Minor: %d\r\n",
                     version.chip_id, version.rom_id, version.major, version.minor);
    } else {
        app_log_warning("Failed to get firmware version: 0x%lx\r\n", status);
    }
#endif

    // Set certificate index flags
    if (OTA_CERTIFICATE_INDEX == 1) {
        flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_1;
    } else if (OTA_CERTIFICATE_INDEX == 2) {
        flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_2;
    }

    wifi_ota_update_status(WIFI_OTA_DOWNLOADING, 0);

#if defined(AWS_ENABLE) || defined(AZURE_ENABLE)
    // Resolve DNS for cloud servers
    status = wifi_ota_dns_resolve(hostname, server_ip);
    if (status != SL_STATUS_OK) {
        wifi_ota_notify_complete(WIFI_OTA_ERROR_DNS_RESOLUTION);
        return status;
    }

#ifdef AWS_ENABLE
    app_log_info("Resolved AWS S3 Bucket IP address = %s\r\n", server_ip);
#elif defined(AZURE_ENABLE)
    app_log_info("Resolved AZURE Blob Storage IP address = %s\r\n", server_ip);
#endif

#else
    // Use static IP for local server
    strcpy(server_ip, HTTP_SERVER_IP_ADDRESS);
    app_log_info("%s IP Address : %s\r\n", SERVER_NAME, server_ip);
#endif

    app_log_info("Firmware download from %s is in progress...\r\n", SERVER_NAME);
    wifi_ota_update_status(WIFI_OTA_DOWNLOADING, 10);

    // Set up OTAF parameters
    sl_si91x_http_otaf_params_t http_params = { 0 };
    http_params.flags           = flags;
    http_params.ip_address      = (uint8_t *)server_ip;
    http_params.port            = (uint16_t)HTTP_PORT;
    http_params.resource        = (uint8_t *)HTTP_URL;
    http_params.host_name       = (uint8_t *)hostname;
    http_params.extended_header = (uint8_t *)HTTP_EXTENDED_HEADER;
    http_params.user_name       = (uint8_t *)USERNAME;
    http_params.password        = (uint8_t *)PASSWORD;

    wifi_ota_update_status(WIFI_OTA_DOWNLOADING, 25);

    // Register callback for OTA events
    sl_wifi_set_callback(SL_WIFI_HTTP_OTA_FW_UPDATE_EVENTS,
                         (sl_wifi_callback_function_t)&wifi_ota_fw_update_response_handler,
                         NULL);

    wifi_ota_update_status(WIFI_OTA_DOWNLOADING, 50);

    app_log_info("Starting HTTP OTAF with parameters:\r\n");
    app_log_info("  Flags: 0x%x\r\n", http_params.flags);
    app_log_info("  IP: %s\r\n", http_params.ip_address);
    app_log_info("  Port: %d\r\n", http_params.port);
    app_log_info("  Resource: %s\r\n", http_params.resource);
    app_log_info("  Hostname: %s\r\n", http_params.host_name);

    // Start OTAF firmware upgrade
    status = sl_si91x_http_otaf_v2(&http_params);

    app_log_info("sl_si91x_http_otaf_v2 returned with status: 0x%lX\r\n", status);

    if (status != SL_STATUS_OK) {
        app_log_error("Firmware update failed with status = 0x%lX\r\n", status);
        wifi_ota_notify_complete(WIFI_OTA_ERROR_DOWNLOAD_FAILED);
        return status;
    } else {
        app_log_info("Completed firmware download from %s\r\n", SERVER_NAME);
        app_log_info("Updating the firmware...\r\n");
        wifi_ota_update_status(WIFI_OTA_UPDATING, 75);
    }

    return SL_STATUS_OK;
}

sl_status_t wifi_ota_start_update(void)
{
    sl_status_t status;

    if (current_ota_status != WIFI_OTA_IDLE) {
        app_log_error("OTA update already in progress or completed\r\n");
        return SL_STATUS_INVALID_STATE;
    }

    app_log_info("Starting WiFi OTA update...\r\n");
    wifi_ota_update_status(WIFI_OTA_INITIALIZING, 0);

#if LOAD_CERTIFICATE
    if (FLAGS & HTTPS_SUPPORT) {
        status = wifi_ota_load_certificates();
        if (status != SL_STATUS_OK) {
            app_log_error("Certificate loading failed: 0x%lX\r\n", status);
            wifi_ota_notify_complete(WIFI_OTA_ERROR_CERTIFICATE_LOAD_FAILED);
            return status;
        }
    }
#endif

    wifi_ota_update_status(WIFI_OTA_INITIALIZING, 5);

    // Start firmware download and update
    status = wifi_ota_download_firmware();
    if (status != SL_STATUS_OK) {
        return status;
    }

    return SL_STATUS_OK;
}