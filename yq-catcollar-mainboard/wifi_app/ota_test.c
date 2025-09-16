#include "simple_ota.h"
#include "wifi_ota_config.h"
#include "sl_net.h"
#include "sl_wifi.h"
#include "app_log.h"
#include "wifi_app.h"
#include "cmsis_os2.h"

// Test function to run OTA with proper initialization
sl_status_t ota_test_with_net_init(void)
{
    sl_status_t status;

    // Check if WiFi is already connected
    if (catcollar_wifi_connection_get_state() != CATCOLLAR_WIFI_CONNECTED) {
        app_log_error("WiFi not connected, cannot start OTA test\r\n");
        return SL_STATUS_NOT_READY;
    }

    app_log_info("=== Starting OTA Test with Network Layer ===\r\n");

    // Start OTA update
    status = simple_ota_start();
    if (status == SL_STATUS_OK) {
        app_log_info("OTA test started successfully\r\n");
    } else {
        app_log_error("Failed to start OTA test: 0x%lx\r\n", status);
    }

    return status;
}

// Alternative test function that mimics wifi_http_otaf_soc initialization
sl_status_t ota_test_standalone(void)
{
    sl_status_t status;

    app_log_info("=== Starting Standalone OTA Test ===\r\n");

    // WiFi device configuration optimized for OTA (similar to wifi_http_otaf_soc)
    static const sl_wifi_device_configuration_t ota_wifi_config = {
        .boot_option = LOAD_NWP_FW,
        .mac_address = NULL,
        .band        = SL_SI91X_WIFI_BAND_2_4GHZ,
        .region_code = US,
        .boot_config = {
            .oper_mode              = SL_SI91X_CLIENT_MODE,
            .coex_mode              = SL_SI91X_WLAN_ONLY_MODE,
            .feature_bit_map        = (SL_SI91X_FEAT_SECURITY_PSK | SL_SI91X_FEAT_AGGREGATION),
            .tcp_ip_feature_bit_map = (SL_SI91X_TCP_IP_FEAT_DHCPV4_CLIENT | SL_SI91X_TCP_IP_FEAT_HTTP_CLIENT
                                     | SL_SI91X_TCP_IP_FEAT_EXTENSION_VALID | SL_SI91X_TCP_IP_FEAT_SSL
                                     | SL_SI91X_TCP_IP_FEAT_DNS_CLIENT),
            .custom_feature_bit_map = SL_SI91X_CUSTOM_FEAT_EXTENTION_VALID,
            .ext_custom_feature_bit_map = (SL_SI91X_EXT_FEAT_XTAL_CLK | SL_SI91X_EXT_FEAT_UART_SEL_FOR_DEBUG_PRINTS | MEMORY_CONFIG
#if defined(SLI_SI917) || defined(SLI_SI915)
                                         | SL_SI91X_EXT_FEAT_FRONT_END_SWITCH_PINS_ULP_GPIO_4_5_0
#endif
                                         ),
            .bt_feature_bit_map = 0,
            .ext_tcp_ip_feature_bit_map = (SL_SI91X_EXT_FEAT_HTTP_OTAF_SUPPORT | SL_SI91X_EXT_TCP_IP_SSL_16K_RECORD
                                         | SL_SI91X_CONFIG_FEAT_EXTENTION_VALID),
            .ble_feature_bit_map     = 0,
            .ble_ext_feature_bit_map = 0,
            .config_feature_bit_map  = 0
        }
    };

    // Initialize with network layer (like wifi_http_otaf_soc)
    status = sl_net_init(SL_NET_WIFI_CLIENT_INTERFACE, &ota_wifi_config, NULL, NULL);
    if (status != SL_STATUS_OK) {
        app_log_error("OTA WiFi initialization failed: 0x%lX\r\n", status);
        return status;
    }
    app_log_info("OTA WiFi initialization successful\r\n");

    // Connect to test access point
    wifi_connect_test();

    if (catcollar_wifi_connection_get_state() == CATCOLLAR_WIFI_CONNECTED) {
        app_log_info("WiFi connected, starting OTA...\r\n");
        osDelay(2000); // Wait for connection to stabilize

        status = simple_ota_start();
        if (status == SL_STATUS_OK) {
            app_log_info("Standalone OTA test started successfully\r\n");
        } else {
            app_log_error("Failed to start standalone OTA test: 0x%lx\r\n", status);
        }
    } else {
        app_log_error("WiFi connection failed, cannot start OTA\r\n");
        status = SL_STATUS_NOT_READY;
    }

    return status;
}