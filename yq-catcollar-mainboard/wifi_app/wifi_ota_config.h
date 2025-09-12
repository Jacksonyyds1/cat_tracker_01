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

// AWS Configuration for OTA
#define OTA_AWS_ENABLE                  1
#define OTA_FLAGS                      (BIT(0))  // HTTPS_SUPPORT
#define OTA_HTTP_PORT                  443
#define OTA_AWS_HOSTNAME               "catcollar-firmware.s3.ap-southeast-1.amazonaws.com"
#define OTA_FIRMWARE_URL               "firmware/catcollar_firmware.rps"
#define OTA_VERSION_CHECK_URL          "firmware/version.json"

// OTA Settings
#define OTA_TIMEOUT_MS                 600000    // 10 minutes
#define OTA_MAX_RETRY_COUNT           3
#define OTA_VERSION_CHECK_INTERVAL_MS  (24 * 60 * 60 * 1000)  // 24 hours

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

// HTTPS flags
#define HTTPS_SUPPORT                 BIT(0)
#define HTTP_V_1_1                    BIT(6)
#define HTTPV6                        BIT(3)

// OTA States
typedef enum {
    OTA_STATE_IDLE = 0,
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
    OTA_STATUS_DOWNLOAD_SUCCESS,
    OTA_STATUS_DOWNLOAD_FAILED,
    OTA_STATUS_VERIFY_FAILED,
    OTA_STATUS_INSTALL_FAILED,
    OTA_STATUS_NETWORK_ERROR
} catcollar_ota_status_t;

// Version structure
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint16_t build;
} catcollar_firmware_version_t;

// Function declarations
sl_status_t catcollar_ota_init(void);
sl_status_t catcollar_ota_check_for_updates(void);
sl_status_t catcollar_ota_start_update(void);
catcollar_ota_state_t catcollar_ota_get_state(void);
catcollar_ota_status_t catcollar_ota_get_status(void);
catcollar_firmware_version_t catcollar_get_current_version(void);

#endif // WIFI_OTA_CONFIG_H