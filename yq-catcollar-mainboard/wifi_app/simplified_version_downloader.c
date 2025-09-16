/**
 * @file simplified_version_downloader.c
 * @brief 基于现有wifi_ota_manager代码的简化版本下载器实现
 * @author YQ05165
 * @date 2025-09-16
 */

#include "simplified_version_downloader.h"
#include "wifi_ota_config.h"
#include "app_log.h"
#include "sl_wifi.h"
#include "sl_net.h"
#include "sl_net_dns.h"
#include "sl_http_client.h"
#include "version_manager.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Configuration for version download
#define VERSION_HOSTNAME "cat-firmware-bucket-001.s3.us-east-2.amazonaws.com"
#define VERSION_RESOURCE "/firmware/version.txt"
#define VERSION_PORT 80  // HTTP (avoid HTTPS certificate issues)
#define MAX_DNS_RETRY_COUNT 5
#define DNS_TIMEOUT 10000
#define HTTP_TIMEOUT 30000
#define VERSION_BUFFER_SIZE 256

// Static variables for HTTP response handling
static char downloaded_version[64] = {0};
static bool version_download_complete = false;
static bool version_download_success = false;

// DNS resolution function (based on wifi_http_event.c)
static sl_status_t resolve_version_server_ip(const char *hostname, uint8_t *server_ip)
{
    sl_status_t status;
    sl_ip_address_t dns_query_rsp = {0};
    uint32_t server_address;
    int32_t dns_retry_count = MAX_DNS_RETRY_COUNT;

    app_log_info("Resolving hostname: %s\r\n", hostname);

    do {
        status = sl_net_dns_resolve_hostname((const char *)hostname,
                                           DNS_TIMEOUT,
                                           SL_NET_DNS_TYPE_IPV4,
                                           &dns_query_rsp);
        dns_retry_count--;
    } while ((dns_retry_count != 0) && (status != SL_STATUS_OK));

    if (status != SL_STATUS_OK) {
        app_log_error("DNS resolution failed for %s: 0x%lx\r\n", hostname, status);
        return status;
    }

    server_address = dns_query_rsp.ip.v4.value;
    sprintf((char *)server_ip,
            "%ld.%ld.%ld.%ld",
            server_address & 0x000000ff,
            (server_address & 0x0000ff00) >> 8,
            (server_address & 0x00ff0000) >> 16,
            (server_address & 0xff000000) >> 24);

    app_log_info("Resolved IP address: %s\r\n", (char *)server_ip);
    return SL_STATUS_OK;
}

// HTTP GET response callback (based on wifi_http_event.c)
static sl_status_t version_get_response_callback(const sl_http_client_t *client,
                                               sl_http_client_event_t event,
                                               void *data,
                                               void *request_context)
{
    UNUSED_PARAMETER(client);
    UNUSED_PARAMETER(event);
    UNUSED_PARAMETER(request_context);

    sl_http_client_response_t *get_response = (sl_http_client_response_t *)data;

    app_log_debug("HTTP GET Response - Status: 0x%X, Code: %u, Data Length: %u\r\n",
                  get_response->status, get_response->http_response_code, get_response->data_length);

    if (get_response->status != SL_STATUS_OK) {
        app_log_error("HTTP GET failed with status: 0x%X\r\n", get_response->status);
        version_download_success = false;
        version_download_complete = true;
        return SL_STATUS_FAIL;
    }

    if (get_response->http_response_code != 200) {
        app_log_error("HTTP response code: %u\r\n", get_response->http_response_code);
        version_download_success = false;
        version_download_complete = true;
        return SL_STATUS_FAIL;
    }

    // Copy response data
    if (get_response->data_buffer && get_response->data_length > 0) {
        size_t copy_len = (get_response->data_length < sizeof(downloaded_version) - 1) ?
                         get_response->data_length : sizeof(downloaded_version) - 1;
        memcpy(downloaded_version, get_response->data_buffer, copy_len);
        downloaded_version[copy_len] = '\0';

        app_log_info("Downloaded version data: '%s'\r\n", downloaded_version);
    }

    if (get_response->end_of_data) {
        version_download_success = true;
        version_download_complete = true;
        app_log_info("Version download completed successfully\r\n");
    }

    return SL_STATUS_OK;
}

sl_status_t simplified_download_version(char *version_buffer, size_t buffer_size)
{
    if (!version_buffer || buffer_size == 0) {
        return SL_STATUS_INVALID_PARAMETER;
    }

    app_log_info("Starting real version download from S3...\r\n");

    // Reset state variables
    memset(downloaded_version, 0, sizeof(downloaded_version));
    version_download_complete = false;
    version_download_success = false;

    sl_status_t status;
    uint8_t server_ip[16] = {0};

    // Step 1: DNS resolution
    status = resolve_version_server_ip(VERSION_HOSTNAME, server_ip);
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to resolve server IP\r\n");
        return status;
    }

    // Step 2: Setup HTTP client credentials (required before client init)
    sl_http_client_credentials_t empty_cred = {0};
    status = sl_net_set_credential(SL_NET_HTTP_CLIENT_CREDENTIAL_ID(0),
                                   SL_NET_HTTP_CLIENT_CREDENTIAL,
                                   &empty_cred,
                                   sizeof(empty_cred));
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to set HTTP client credentials: 0x%lx\r\n", status);
        return status;
    }

    // Step 3: Setup HTTP client configuration
    sl_http_client_t client_handle = 0;
    sl_http_client_configuration_t client_config = {0};
    sl_http_client_request_t client_request = {0};

    // Configure HTTP client
    client_config.network_interface = SL_NET_WIFI_CLIENT_INTERFACE;
    client_config.ip_version = SL_IPV4;  // IPv4
    client_config.http_version = SL_HTTP_V_1_1;  // HTTP 1.1
#if 0  // Disable HTTPS for now to avoid certificate issues
    client_config.https_enable = true;  // Enable HTTPS
    client_config.tls_version = SL_TLS_V_1_2;  // TLS 1.2
    client_config.certificate_index = SL_HTTPS_CLIENT_CERTIFICATE_INDEX_1;  // Use certificate index 1
#endif

    // Initialize HTTP client
    status = sl_http_client_init(&client_config, &client_handle);
    if (status != SL_STATUS_OK) {
        app_log_error("HTTP client init failed: 0x%lx\r\n", status);
        return status;
    }

    // Configure GET request
    client_request.ip_address = server_ip;
    client_request.host_name = (uint8_t *)VERSION_HOSTNAME;
    client_request.port = VERSION_PORT;
    client_request.resource = (uint8_t *)VERSION_RESOURCE;
    client_request.http_method_type = SL_HTTP_GET;
    client_request.extended_header = NULL;

    // Initialize request callback
    status = sl_http_client_request_init(&client_request, version_get_response_callback, "Version Download");
    if (status != SL_STATUS_OK) {
        app_log_error("HTTP request init failed: 0x%lx\r\n", status);
        sl_http_client_deinit(&client_handle);
        return status;
    }

    // Send GET request
    app_log_info("Sending HTTP GET request to %s:%d%s\r\n", VERSION_HOSTNAME, VERSION_PORT, VERSION_RESOURCE);
    app_log_info("Client config: interface=%d, ip_version=%d, http_version=%d\r\n",
                 client_config.network_interface, client_config.ip_version, client_config.http_version);

    status = sl_http_client_send_request(&client_handle, &client_request);
    if (status != SL_STATUS_OK && status != SL_STATUS_IN_PROGRESS) {
        app_log_error("HTTP GET request failed: 0x%lx\r\n", status);
        sl_http_client_deinit(&client_handle);
        return status;
    }

    // Wait for response
    uint32_t timeout_count = 0;
    const uint32_t max_timeout = HTTP_TIMEOUT / 100;  // 100ms intervals

    while (!version_download_complete && timeout_count < max_timeout) {
        osDelay(100);
        timeout_count++;
    }

    // Cleanup
    sl_http_client_deinit(&client_handle);

    // Check result
    if (!version_download_complete) {
        app_log_error("Version download timeout\r\n");
        return SL_STATUS_TIMEOUT;
    }

    if (!version_download_success) {
        app_log_error("Version download failed\r\n");
        return SL_STATUS_FAIL;
    }

    // Copy result to output buffer
    strncpy(version_buffer, downloaded_version, buffer_size - 1);
    version_buffer[buffer_size - 1] = '\0';

    // Clean up the version string (remove whitespace)
    int len = strlen(version_buffer);
    while (len > 0 && (version_buffer[len-1] == '\r' || version_buffer[len-1] == '\n' ||
                       version_buffer[len-1] == ' ' || version_buffer[len-1] == '\t')) {
        version_buffer[--len] = '\0';
    }

    app_log_info("Successfully downloaded version: %s\r\n", version_buffer);
    return SL_STATUS_OK;
}

bool simplified_validate_version(const char *version_str)
{
    if (!version_str) {
        return false;
    }

    // Check basic format: should be like "1.1.1" (3 parts)
    int major, minor, patch;
    int parsed = sscanf(version_str, "%d.%d.%d", &major, &minor, &patch);

    if (parsed != 3) {
        app_log_error("Invalid version format: expected 3 numbers separated by dots (major.minor.patch)\r\n");
        return false;
    }

    // Check ranges (assuming reasonable limits)
    if (major < 0 || major > 255 ||
        minor < 0 || minor > 255 ||
        patch < 0 || patch > 255) {
        app_log_error("Version numbers out of valid range\r\n");
        return false;
    }

    app_log_info("Valid 3-part version: %d.%d.%d\r\n", major, minor, patch);
    return true;
}

sl_status_t simplified_test_version_download(void)
{
    char version_buffer[64];
    sl_status_t status;

    app_log_info("=== Testing Real Version Download ===\r\n");

    status = simplified_download_version(version_buffer, sizeof(version_buffer));
    if (status != SL_STATUS_OK) {
        app_log_error("Version download test failed: 0x%lx\r\n", status);
        return status;
    }

    if (!simplified_validate_version(version_buffer)) {
        app_log_error("Downloaded version failed validation\r\n");
        return SL_STATUS_FAIL;
    }

    app_log_info("Real version download test passed: %s\r\n", version_buffer);
    return SL_STATUS_OK;
}

sl_status_t get_remote_version_for_ota(char *version_buffer, size_t buffer_size)
{
    // This is the interface called by version_manager.c
    return simplified_download_version(version_buffer, buffer_size);
}