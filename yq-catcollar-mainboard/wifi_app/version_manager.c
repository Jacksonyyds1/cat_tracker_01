#include "version_manager.h"
#include "wifi_ota_config.h"
#include "common.h"
#include "simplified_version_downloader.h"
#include "sl_wifi.h"
#include "app_log.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Static variables
static firmware_version_t current_version = {0};
static firmware_version_t remote_version = {0};
static volatile bool version_check_complete = false;
static volatile version_check_error_t version_check_result = VERSION_CHECK_SUCCESS;
static version_check_callback_t version_callback = NULL;

// Global variables for version management
static char version_buffer[64] = {0};

sl_status_t version_manager_init(void)
{
    app_log_info("Version Manager: Initializing\r\n");

    // Get current firmware version
    sl_status_t status = version_manager_get_current_version(&current_version);
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to get current firmware version: 0x%lx\r\n", status);
        return status;
    }

    app_log_info("Current firmware version: %d.%d.%d.%d\r\n",
                 current_version.major, current_version.minor,
                 current_version.patch, current_version.build);

    return SL_STATUS_OK;
}

sl_status_t version_manager_get_current_version(firmware_version_t *version)
{
    if (!version) {
        return SL_STATUS_NULL_POINTER;
    }

    // 使用真实的应用程序版本，而不是WiFi固件版本
    version->major = CATCOLLAR_APPLICATION_MAJOR_VERSION;
    version->minor = CATCOLLAR_APPLICATION_MINOR_VERSION;
    version->patch = CATCOLLAR_APPLICATION_PATCH_VERSION;
    
    // 对于build号，可以使用编译时间或其他标识
    #ifdef BUILD_NUMBER
        version->build = BUILD_NUMBER;
    #else
        // 使用简单的日期计算作为build号
        version->build = CATCOLLAR_FW_VERSION;
    #endif

    // Update static current version
    current_version = *version;

    app_log_info("Current application version: %d.%d.%d.%d\r\n",
                 version->major, version->minor, version->patch, version->build);

    return SL_STATUS_OK;
}

// Parse version string "1.1.1" into firmware_version_t
static sl_status_t parse_version_string(const char *version_str, firmware_version_t *version)
{
    if (!version_str || !version) {
        return SL_STATUS_NULL_POINTER;
    }

    // Clean the version string (remove whitespace)
    char clean_version[32];
    strncpy(clean_version, version_str, sizeof(clean_version) - 1);
    clean_version[sizeof(clean_version) - 1] = '\0';

    // Remove trailing whitespace/newlines
    int len = strlen(clean_version);
    while (len > 0 && (clean_version[len-1] == '\r' || clean_version[len-1] == '\n' ||
                       clean_version[len-1] == ' ' || clean_version[len-1] == '\t')) {
        clean_version[--len] = '\0';
    }

    // Support 3-part version format (major.minor.patch)
    int major, minor, patch;
    int parsed = sscanf(clean_version, "%d.%d.%d", &major, &minor, &patch);

    if (parsed != 3) {
        app_log_error("Invalid version format: '%s' (expected major.minor.patch, got %d fields)\r\n", clean_version, parsed);
        return SL_STATUS_INVALID_PARAMETER;
    }

    version->major = (uint8_t)major;
    version->minor = (uint8_t)minor;
    version->patch = (uint8_t)patch;
    version->build = 0;  // Default build number for 3-part versions

    return SL_STATUS_OK;
}

sl_status_t version_manager_get_remote_version(firmware_version_t *version)
{
    if (!version) {
        return SL_STATUS_NULL_POINTER;
    }

    sl_status_t status;
    app_log_info("Getting remote version...\r\n");

    // Initialize version buffer
    memset(version_buffer, 0, sizeof(version_buffer));

    // 选择实现方式：
    // 1. 真实的版本下载（推荐用于生产环境）
    // 2. 配置的版本检查（用于开发/测试）
    
    #ifdef USE_REAL_S3_VERSION_CHECK
        // 使用简化的下载器，复用现有OTA代码的网络功能
        app_log_info("Using simplified version download (based on existing OTA code)\r\n");
        status = get_remote_version_for_ota(version_buffer, sizeof(version_buffer));
        if (status != SL_STATUS_OK) {
            app_log_error("Failed to download version: 0x%lx\r\n", status);
            return status;
        }
        
        // 验证下载的版本格式
        if (!simplified_validate_version(version_buffer)) {
            app_log_error("Invalid version format downloaded\r\n");
            return SL_STATUS_INVALID_PARAMETER;
        }
    #else
        // 配置的版本检查（保留原有逻辑作为后备）
        app_log_info("Using configured version check (define USE_REAL_S3_VERSION_CHECK for real download)\r\n");
        
        // IMPORTANT: Update this version string each time you upload new firmware to S3
        // This should match the version of the firmware file in S3
        //
        // 现在这个版本号应该根据您的应用版本来设置
        // 如果当前应用版本是 1.0.2，那么S3上的新版本应该是更高的版本
        strcpy(version_buffer, "1.0.3.001");  // 设置一个比当前版本更高的版本号用于测试OTA
    #endif

    app_log_info("Version data: %s\r\n", version_buffer);

    // Parse the version string
    status = parse_version_string(version_buffer, version);
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to parse version string\r\n");
        return status;
    }

    app_log_info("Remote version: %d.%d.%d.%d\r\n",
                 version->major, version->minor, version->patch, version->build);

    // Update static remote version
    remote_version = *version;

    return SL_STATUS_OK;
}

version_compare_result_t version_manager_compare_versions(const firmware_version_t *local, const firmware_version_t *remote)
{
    if (!local || !remote) {
        return VERSION_SAME;
    }

    // Compare major version
    if (remote->major > local->major) return VERSION_NEWER;
    if (remote->major < local->major) return VERSION_OLDER;

    // Compare minor version
    if (remote->minor > local->minor) return VERSION_NEWER;
    if (remote->minor < local->minor) return VERSION_OLDER;

    // Compare patch version
    if (remote->patch > local->patch) return VERSION_NEWER;
    if (remote->patch < local->patch) return VERSION_OLDER;

    // Compare build version
    if (remote->build > local->build) return VERSION_NEWER;
    if (remote->build < local->build) return VERSION_OLDER;

    return VERSION_SAME;
}

sl_status_t version_manager_check_for_updates(version_check_callback_t callback)
{
    sl_status_t status;
    firmware_version_t remote_ver = {0};

    version_callback = callback;

    app_log_info("Checking for firmware updates...\r\n");

    // Get remote version
    status = version_manager_get_remote_version(&remote_ver);
    if (status != SL_STATUS_OK) {
        if (callback) {
            callback(VERSION_CHECK_ERROR_DOWNLOAD, remote_ver, VERSION_SAME);
        }
        return status;
    }

    // Compare versions
    version_compare_result_t result = version_manager_compare_versions(&current_version, &remote_ver);

    app_log_info("Version comparison result: ");
    switch (result) {
        case VERSION_NEWER:
            app_log_append("Remote version is newer - update available\r\n");
            break;
        case VERSION_SAME:
            app_log_append("Versions are the same - no update needed\r\n");
            break;
        case VERSION_OLDER:
            app_log_append("Remote version is older - no update needed\r\n");
            break;
    }

    if (callback) {
        callback(VERSION_CHECK_SUCCESS, remote_ver, result);
    }

    return SL_STATUS_OK;
}

bool version_manager_should_update(void)
{
    version_compare_result_t result = version_manager_compare_versions(&current_version, &remote_version);
    return (result == VERSION_NEWER);
}