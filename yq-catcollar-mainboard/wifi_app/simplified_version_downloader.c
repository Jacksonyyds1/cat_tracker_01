/**
 * @file simplified_version_downloader.c
 * @brief 基于现有wifi_ota_manager代码的简化版本下载器
 * @author YQ05165
 * @date 2025-09-16
 */

#include "simplified_version_downloader.h"
#include "wifi_ota_config.h"
#include "sl_wifi.h"
#include "sl_net.h"
#include "sl_net_dns.h"
#include "sl_si91x_socket_utility.h"
#include "sl_si91x_socket_constants.h"
#include "socket.h"
#include "app_log.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 复用现有OTA配置中的参数
#ifdef AWS_ENABLE
    #define VERSION_HOSTNAME        AWS_S3_HOSTNAME
    #define VERSION_RESOURCE        "/firmware/version.txt"
    #define VERSION_PORT            443
    #define VERSION_USE_HTTPS       1
#elif defined(AZURE_ENABLE)
    #define VERSION_HOSTNAME        AZURE_HOSTNAME
    #define VERSION_RESOURCE        "/version.txt"
    #define VERSION_PORT            443
    #define VERSION_USE_HTTPS       1
#else
    // 使用S3配置作为默认
    #define VERSION_HOSTNAME        "cat-firmware-bucket-001.s3.us-east-2.amazonaws.com"
    #define VERSION_RESOURCE        "/firmware/version.txt"
    #define VERSION_PORT            443
    #define VERSION_USE_HTTPS       1
#endif

// 复用OTA管理器中的DNS解析函数思路
static sl_status_t simplified_dns_resolve(const char *hostname, char *ip_buffer)
{
    sl_status_t status;
    sl_ip_address_t dns_query_rsp = {0};
    uint32_t server_address;
    int32_t dns_retry_count = OTA_MAX_DNS_RETRY_COUNT;

    app_log_info("Resolving hostname: %s\r\n", hostname);

    do {
        status = sl_net_dns_resolve_hostname(hostname,
                                           OTA_DNS_TIMEOUT,
                                           SL_NET_DNS_TYPE_IPV4,
                                           &dns_query_rsp);
        dns_retry_count--;
        if (status != SL_STATUS_OK && dns_retry_count > 0) {
            app_log_info("DNS retry, attempts remaining: %d\r\n", dns_retry_count);
            osDelay(1000);
        }
    } while ((dns_retry_count > 0) && (status != SL_STATUS_OK));

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

// 使用Silicon Labs的HTTP OTA接口下载小文件（版本信息）
static sl_status_t download_small_file_via_http_otaf(const char *hostname, 
                                                   const char *resource,
                                                   char *version_buffer, 
                                                   size_t buffer_size)
{
    sl_status_t status;
    char server_ip[16];

    // DNS解析（复用OTA的DNS解析逻辑）
    status = simplified_dns_resolve(hostname, server_ip);
    if (status != SL_STATUS_OK) {
        return status;
    }

    // 注意：这里我们不能直接用sl_si91x_http_otaf下载文本文件，因为它是专门用于固件下载的
    // 我们需要使用标准的socket方法
    return download_version_via_socket(server_ip, VERSION_PORT, hostname, resource, 
                                     version_buffer, buffer_size);
}

// 使用socket方式下载版本信息（参考OTA中的网络连接部分）
static sl_status_t download_version_via_socket(const char *server_ip,
                                             uint16_t port,
                                             const char *hostname,
                                             const char *resource,
                                             char *version_buffer,
                                             size_t buffer_size)
{
    int32_t client_socket = -1;
    struct sockaddr_in server_addr = {0};
    char http_request[512];
    char http_response[1024];
    ssize_t bytes_sent, bytes_received;
    sl_status_t status = SL_STATUS_OK;

    app_log_info("Downloading version info from %s:%d%s\r\n", server_ip, port, resource);

    // 创建socket
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0) {
        app_log_error("Socket creation failed\r\n");
        return SL_STATUS_FAIL;
    }

    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // 将IP地址字符串转换为网络地址
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        app_log_error("Invalid IP address: %s\r\n", server_ip);
        close(client_socket);
        return SL_STATUS_INVALID_PARAMETER;
    }

    // 连接到服务器
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        app_log_error("Connection to server failed\r\n");
        close(client_socket);
        return SL_STATUS_FAIL;
    }

    app_log_info("Connected to server successfully\r\n");

    // 构建HTTP请求（复用OTA中的HTTP请求格式）
    int request_len = snprintf(http_request, sizeof(http_request),
                              "GET %s HTTP/1.1\r\n"
                              "Host: %s\r\n"
                              "User-Agent: CatCollar-OTA/1.0\r\n"
                              "Connection: close\r\n"
                              "Cache-Control: no-cache\r\n"
                              "\r\n",
                              resource, hostname);

    // 发送HTTP请求
    bytes_sent = send(client_socket, http_request, request_len, 0);
    if (bytes_sent != request_len) {
        app_log_error("Failed to send HTTP request\r\n");
        status = SL_STATUS_FAIL;
        goto cleanup;
    }

    app_log_info("HTTP request sent successfully\r\n");

    // 接收HTTP响应
    memset(http_response, 0, sizeof(http_response));
    bytes_received = recv(client_socket, http_response, sizeof(http_response) - 1, 0);
    if (bytes_received <= 0) {
        app_log_error("Failed to receive HTTP response\r\n");
        status = SL_STATUS_FAIL;
        goto cleanup;
    }

    app_log_info("Received %d bytes from server\r\n", bytes_received);

    // 解析HTTP响应
    status = parse_version_from_http_response(http_response, bytes_received, 
                                            version_buffer, buffer_size);

cleanup:
    close(client_socket);
    return status;
}

// 解析HTTP响应提取版本信息
static sl_status_t parse_version_from_http_response(const char *response, 
                                                  size_t response_len,
                                                  char *version_buffer, 
                                                  size_t buffer_size)
{
    // 检查HTTP状态码
    if (strncmp(response, "HTTP/1.1 200", 12) != 0 && 
        strncmp(response, "HTTP/1.0 200", 12) != 0) {
        app_log_error("HTTP request failed - non-200 status\r\n");
        
        // 提取状态行用于调试
        const char *line_end = strstr(response, "\r\n");
        if (line_end) {
            char status_line[128];
            size_t line_len = (line_end - response) < 127 ? (line_end - response) : 127;
            strncpy(status_line, response, line_len);
            status_line[line_len] = '\0';
            app_log_error("HTTP Status: %s\r\n", status_line);
        }
        return SL_STATUS_FAIL;
    }

    // 查找HTTP头部结束标记
    const char *body_start = strstr(response, "\r\n\r\n");
    if (!body_start) {
        app_log_error("Invalid HTTP response format\r\n");
        return SL_STATUS_INVALID_PARAMETER;
    }
    
    body_start += 4; // 跳过 "\r\n\r\n"

    // 提取版本信息到第一个换行符
    const char *version_start = body_start;
    const char *version_end = strstr(version_start, "\r\n");
    if (!version_end) {
        version_end = strstr(version_start, "\n");
    }
    if (!version_end) {
        // 没有换行符，使用剩余所有内容
        version_end = response + response_len;
    }

    size_t version_len = version_end - version_start;
    if (version_len == 0 || version_len >= buffer_size) {
        app_log_error("Invalid version data length: %zu\r\n", version_len);
        return SL_STATUS_INVALID_PARAMETER;
    }

    // 复制并清理版本字符串
    strncpy(version_buffer, version_start, version_len);
    version_buffer[version_len] = '\0';

    // 移除前后空格和特殊字符
    char *start = version_buffer;
    char *end = version_buffer + strlen(version_buffer) - 1;

    // 移除前面的空格
    while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')) {
        start++;
    }

    // 移除后面的空格
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }

    // 如果start不是version_buffer的开始，需要移动字符串
    if (start != version_buffer) {
        memmove(version_buffer, start, strlen(start) + 1);
    }

    if (strlen(version_buffer) == 0) {
        app_log_error("Empty version string after cleanup\r\n");
        return SL_STATUS_FAIL;
    }

    app_log_info("Extracted version: '%s'\r\n", version_buffer);
    return SL_STATUS_OK;
}

// 主要的版本下载接口
sl_status_t simplified_download_version(char *version_buffer, size_t buffer_size)
{
    if (!version_buffer || buffer_size < 16) {
        return SL_STATUS_INVALID_PARAMETER;
    }

    app_log_info("=== Starting Simplified Version Download ===\r\n");
    app_log_info("Target: %s%s\r\n", VERSION_HOSTNAME, VERSION_RESOURCE);

    sl_status_t status = download_small_file_via_http_otaf(VERSION_HOSTNAME,
                                                         VERSION_RESOURCE,
                                                         version_buffer,
                                                         buffer_size);

    if (status == SL_STATUS_OK) {
        app_log_info("Version download successful: %s\r\n", version_buffer);
    } else {
        app_log_error("Version download failed: 0x%lx\r\n", status);
    }

    return status;
}

// 验证版本格式（复用之前的实现）
bool simplified_validate_version(const char *version_str)
{
    if (!version_str || strlen(version_str) == 0) {
        return false;
    }

    int major, minor, patch, build;
    int parsed = sscanf(version_str, "%d.%d.%d.%d", &major, &minor, &patch, &build);
    
    if (parsed != 4) {
        app_log_error("Invalid version format: %s (expected: major.minor.patch.build)\r\n", version_str);
        return false;
    }

    // 检查版本号范围
    if (major < 0 || minor < 0 || patch < 0 || build < 0 ||
        major > 255 || minor > 255 || patch > 255 || build > 65535) {
        app_log_error("Version numbers out of range: %s\r\n", version_str);
        return false;
    }

    return true;
}

// 测试函数
sl_status_t simplified_test_version_download(void)
{
    char version_buffer[64];
    
    app_log_info("=== Testing Simplified Version Download ===\r\n");
    
    sl_status_t status = simplified_download_version(version_buffer, sizeof(version_buffer));
    
    if (status == SL_STATUS_OK) {
        app_log_info("Download test successful\r\n");
        app_log_info("Downloaded version: %s\r\n", version_buffer);
        
        if (simplified_validate_version(version_buffer)) {
            app_log_info("Version format validation passed\r\n");
            return SL_STATUS_OK;
        } else {
            app_log_error("Version format validation failed\r\n");
            return SL_STATUS_FAIL;
        }
    } else {
        app_log_error("Download test failed: 0x%lx\r\n", status);
        return status;
    }
}

// 与现有OTA系统的集成接口
sl_status_t get_remote_version_for_ota(char *version_buffer, size_t buffer_size)
{
    // 这个函数可以直接被version_manager.c中的version_manager_get_remote_version调用
    return simplified_download_version(version_buffer, buffer_size);
}