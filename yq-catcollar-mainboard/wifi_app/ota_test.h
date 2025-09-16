#ifndef OTA_TEST_H
#define OTA_TEST_H

#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// Test OTA with current WiFi connection
sl_status_t ota_test_with_net_init(void);

// Test OTA with standalone initialization (similar to wifi_http_otaf_soc)
sl_status_t ota_test_standalone(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_TEST_H