#ifndef WIFI_OTA_CONFIG_H
#define WIFI_OTA_CONFIG_H

#include <stdbool.h>
#include "sl_status.h"
#include "sl_net.h"
#include "sl_wifi.h"
#include "firmware_upgradation.h"

// OTA Configuration
#define CATCOLLAR_FIRMWARE_VERSION_MAJOR    1
#define CATCOLLAR_FIRMWARE_VERSION_MINOR    0
#define CATCOLLAR_FIRMWARE_VERSION_PATCH    0
#define CATCOLLAR_FIRMWARE_VERSION_BUILD    1

#define CATCOLLAR_FIRMWARE_VERSION_STRING "1.0.0.1"

// AWS Configuration for OTA - Updated to ap-east-1 region
#define OTA_AWS_ENABLE                  1
#define OTA_FLAGS                      (BIT(0))  // HTTPS_SUPPORT
#define OTA_HTTP_PORT                  443
#define OTA_AWS_HOSTNAME               "catcollar-firmware.s3.ap-east-1.amazonaws.com"
#define OTA_FIRMWARE_URL               "firmware/catcollar_firmware.rps"
#define OTA_VERSION_CHECK_URL          "firmware/version.json"

// Enhanced OTA Settings for improved reliability
#define OTA_TIMEOUT_MS                 900000    // 15 minutes (increased timeout)
#define OTA_MAX_RETRY_COUNT           5         // Increased retry count
#define OTA_VERSION_CHECK_INTERVAL_MS  (24 * 60 * 60 * 1000)  // 24 hours
#define OTA_DNS_TIMEOUT_MS            20000     // 20 seconds for DNS resolution
#define OTA_MAX_DNS_RETRY_COUNT       5         // Maximum DNS retry attempts

// Certificate configuration
#define OTA_LOAD_CERTIFICATE          1
#define OTA_CERTIFICATE_INDEX         0

// Firmware update type (from reference implementation)
#define M4_FW_UPDATE                  0
#define TA_FW_UPDATE                  1
#define COMBINED_FW_UPDATE            2
#define OTA_FW_UPDATE_TYPE           TA_FW_UPDATE

// Bit manipulation macro
#ifndef BIT
#define BIT(a) ((uint32_t)1U << a)
#endif

// HTTPS flags - Enhanced for better AWS S3 compatibility
#define HTTPS_SUPPORT                 BIT(0)
#define HTTP_V_1_1                    BIT(6)
#define HTTPV6                        BIT(3)
#define HTTP_USER_DEFINED_CONTENT_TYPE BIT(7)

// Enhanced OTA flags for AWS S3
#define OTA_ENHANCED_FLAGS            (HTTPS_SUPPORT | HTTP_V_1_1)

// OTA States
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_INITIALIZING,
    OTA_STATE_RESOLVING_DNS,
    OTA_STATE_CHECKING_VERSION,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_INSTALLING,
    OTA_STATE_REBOOTING,
    OTA_STATE_SUCCESS,
    OTA_STATE_FAILED
} catcollar_ota_state_t;

// OTA Status
typedef enum {
    OTA_STATUS_NO_UPDATE_AVAILABLE = 0,
    OTA_STATUS_UPDATE_AVAILABLE,
    OTA_STATUS_DOWNLOAD_IN_PROGRESS,
    OTA_STATUS_DOWNLOAD_SUCCESS,
    OTA_STATUS_DOWNLOAD_FAILED,
    OTA_STATUS_VERIFY_FAILED,
    OTA_STATUS_INSTALL_FAILED,
    OTA_STATUS_NETWORK_ERROR,
    OTA_STATUS_DNS_RESOLUTION_FAILED,
    OTA_STATUS_CERTIFICATE_ERROR,
    OTA_STATUS_TIMEOUT_ERROR
} catcollar_ota_status_t;

// Version structure
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint16_t build;
} catcollar_firmware_version_t;

// OTA Progress callback type
typedef void (*catcollar_ota_progress_callback_t)(uint32_t bytes_downloaded, uint32_t total_bytes);

// OTA Configuration structure
typedef struct {
    char aws_hostname[128];
    char firmware_url[256];
    char version_check_url[256];
    uint16_t port;
    uint32_t timeout_ms;
    uint8_t max_retry_count;
    uint8_t certificate_index;
    catcollar_ota_progress_callback_t progress_callback;
} catcollar_ota_config_t;

// Function declarations
sl_status_t catcollar_ota_init(void);
sl_status_t catcollar_ota_init_with_config(catcollar_ota_config_t *config);
sl_status_t catcollar_ota_check_for_updates(void);
sl_status_t catcollar_ota_start_update(void);
sl_status_t catcollar_ota_start_update_with_retry(uint8_t max_retries);
catcollar_ota_state_t catcollar_ota_get_state(void);
catcollar_ota_status_t catcollar_ota_get_status(void);
catcollar_firmware_version_t catcollar_get_current_version(void);
sl_status_t catcollar_ota_start_periodic_check(void);
sl_status_t catcollar_ota_stop_periodic_check(void);
sl_status_t catcollar_ota_abort(void);
uint8_t catcollar_ota_get_progress_percentage(void);

#endif // WIFI_OTA_CONFIG_H