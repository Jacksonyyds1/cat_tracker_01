#ifndef WIFI_OTA_CONFIG_H
#define WIFI_OTA_CONFIG_H

#include "sl_wifi_types.h"

// OTA Configuration Defines
#define AWS_ENABLE          // Enable AWS S3 OTA
// #define AZURE_ENABLE     // Enable Azure Blob OTA
// #define LOCAL_SERVER     // Enable Local HTTP server OTA

// Type of FW update
#define M4_FW_UPDATE       0
#define TA_FW_UPDATE       1
#define COMBINED_FW_UPDATE 2

// Set FW update type
#define FW_UPDATE_TYPE TA_FW_UPDATE

// Load certificate to device flash
#define LOAD_CERTIFICATE 1

// HTTP/HTTPS flags
#define BIT(a) ((uint32_t)1U << a)
#define HTTPV6 BIT(3)
#define HTTPS_SUPPORT BIT(0)
#define HTTP_POST_DATA BIT(5)
#define HTTP_V_1_1 BIT(6)
#define HTTP_USER_DEFINED_CONTENT_TYPE BIT(7)

// Certificate index for OTA (avoid conflict with wifi_http_event.h)
#ifndef OTA_CERTIFICATE_INDEX
#define OTA_CERTIFICATE_INDEX 0
#endif

// Timeout settings for OTA (avoid conflict with wifi_http_event.h)
#ifndef OTA_DNS_TIMEOUT
#define OTA_DNS_TIMEOUT         20000
#endif
#ifndef OTA_MAX_DNS_RETRY_COUNT
#define OTA_MAX_DNS_RETRY_COUNT 5
#endif
#define OTAF_TIMEOUT        600000

#ifdef AWS_ENABLE
// AWS S3 Configuration
#define FLAGS HTTPS_SUPPORT
#define HTTP_PORT 443

#if (FW_UPDATE_TYPE == TA_FW_UPDATE)
#define HTTP_URL "firmware/yq-catcollar-mainboard.rps"
#else
#define HTTP_URL "firmware/yq-catcollar-mainboard.bin"
#endif

// AWS S3 bucket configuration
#define AWS_S3_BUCKET_NAME "cat-firmware-bucket-001"
#define AWS_S3_REGION "us-east-2"
#define AWS_S3_HOSTNAME AWS_S3_BUCKET_NAME ".s3." AWS_S3_REGION ".amazonaws.com"

#define HTTP_EXTENDED_HEADER NULL
#define USERNAME ""
#define PASSWORD ""
#define SERVER_NAME "AWS S3 Server"

#elif defined(AZURE_ENABLE)
// Azure Blob Configuration
#define FLAGS HTTPS_SUPPORT
#define HTTP_PORT 443
#define HTTP_URL "rps/firmware.rps"
#define AZURE_HOSTNAME "si917updates.blob.core.windows.net"
#define HTTP_EXTENDED_HEADER NULL
#define USERNAME ""
#define PASSWORD ""
#define SERVER_NAME "AZURE Server"

#else
// Local HTTP Server Configuration
#define FLAGS 0
#define HTTP_PORT 80
#define HTTP_SERVER_IP_ADDRESS "192.168.0.100"

#if (FW_UPDATE_TYPE == TA_FW_UPDATE)
#define HTTP_URL "rps/firmware.rps"
#else
#define HTTP_URL "isp.bin"
#endif

#define HTTP_HOSTNAME "192.168.0.100"
#define HTTP_EXTENDED_HEADER NULL
#define USERNAME "admin"
#define PASSWORD "admin"
#define SERVER_NAME "Local Apache Server"
#endif

// OTA status definitions
typedef enum {
    WIFI_OTA_IDLE = 0,
    WIFI_OTA_INITIALIZING,
    WIFI_OTA_DOWNLOADING,
    WIFI_OTA_DOWNLOAD_COMPLETE,
    WIFI_OTA_UPDATING,
    WIFI_OTA_UPDATE_COMPLETE,
    WIFI_OTA_ERROR
} wifi_ota_status_t;

// OTA error codes
typedef enum {
    WIFI_OTA_SUCCESS = 0,
    WIFI_OTA_ERROR_DNS_RESOLUTION,
    WIFI_OTA_ERROR_CONNECTION_FAILED,
    WIFI_OTA_ERROR_DOWNLOAD_FAILED,
    WIFI_OTA_ERROR_UPDATE_FAILED,
    WIFI_OTA_ERROR_CERTIFICATE_LOAD_FAILED,
    WIFI_OTA_ERROR_TIMEOUT
} wifi_ota_error_t;

// OTA callback function type
typedef void (*wifi_ota_progress_callback_t)(wifi_ota_status_t status, uint32_t progress_percent);
typedef void (*wifi_ota_complete_callback_t)(wifi_ota_error_t error);

#endif // WIFI_OTA_CONFIG_H