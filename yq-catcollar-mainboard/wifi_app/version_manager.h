#ifndef VERSION_MANAGER_H
#define VERSION_MANAGER_H

#include "sl_status.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Version structure
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint16_t build;
} firmware_version_t;

// Version check result
typedef enum {
    VERSION_CHECK_SUCCESS = 0,
    VERSION_CHECK_ERROR_DNS,
    VERSION_CHECK_ERROR_CONNECTION,
    VERSION_CHECK_ERROR_DOWNLOAD,
    VERSION_CHECK_ERROR_PARSE,
    VERSION_CHECK_ERROR_TIMEOUT
} version_check_error_t;

// Version comparison result
typedef enum {
    VERSION_OLDER = -1,     // Remote version is older
    VERSION_SAME = 0,       // Versions are the same
    VERSION_NEWER = 1       // Remote version is newer
} version_compare_result_t;

// Callback function types
typedef void (*version_check_callback_t)(version_check_error_t error, firmware_version_t remote_version, version_compare_result_t result);

// Function declarations
sl_status_t version_manager_init(void);
sl_status_t version_manager_check_for_updates(version_check_callback_t callback);
sl_status_t version_manager_get_current_version(firmware_version_t *version);
sl_status_t version_manager_get_remote_version(firmware_version_t *version);
version_compare_result_t version_manager_compare_versions(const firmware_version_t *local, const firmware_version_t *remote);
bool version_manager_should_update(void);

// Configuration
#define VERSION_CHECK_URL "https://cat-firmware-bucket-001.s3.us-east-2.amazonaws.com/firmware/version.txt"
#define VERSION_CHECK_HOSTNAME "cat-firmware-bucket-001.s3.us-east-2.amazonaws.com"
#define VERSION_CHECK_RESOURCE "/firmware/version.txt"
#define VERSION_CHECK_TIMEOUT 30000  // 30 seconds

// NOTE: Current implementation uses configured version checking
// To implement real S3 download, modify version_manager_get_remote_version()
// in version_manager.c to download and parse version.txt from S3

#ifdef __cplusplus
}
#endif

#endif // VERSION_MANAGER_H