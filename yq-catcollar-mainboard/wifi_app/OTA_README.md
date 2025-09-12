# Cat Collar WiFi HTTPS OTA Implementation

## Overview

This implementation provides Over-The-Air (OTA) firmware update functionality for the Cat Collar device using WiFi HTTPS protocol. The device automatically checks for firmware updates from AWS S3 after establishing WiFi connection and can download and install updates automatically.

## Features

- ✅ **Automatic Version Checking**: Checks for new firmware versions after WiFi connection
- ✅ **HTTPS Support**: Secure firmware downloads from AWS S3 using SSL/TLS
- ✅ **Periodic Checks**: Configurable periodic version checks (default: 24 hours)
- ✅ **Version Comparison**: Intelligent version comparison (major.minor.patch.build)
- ✅ **Automatic Updates**: Optional automatic firmware installation
- ✅ **Error Handling**: Comprehensive error handling and status reporting
- ✅ **Retry Mechanism**: Configurable retry attempts for failed downloads

## Architecture

### Files Structure
```
wifi_app/
├── wifi_ota_config.h       # OTA configuration and constants
├── wifi_ota_manager.h      # OTA manager API declarations
├── wifi_ota_manager.c      # OTA manager implementation
├── wifi_app.h              # Updated with OTA includes
├── wifi_app.c              # Updated with OTA integration
└── OTA_README.md           # This documentation
```

### Key Components

1. **OTA Configuration** (`wifi_ota_config.h`)
   - Firmware version definitions
   - AWS S3 server configuration
   - OTA behavior settings

2. **OTA Manager** (`wifi_ota_manager.c/.h`)
   - Core OTA functionality
   - Version checking and comparison
   - Firmware download and installation
   - Status and state management

3. **WiFi App Integration** (`wifi_app.c`)
   - OTA initialization after WiFi connection
   - Automatic update triggers

## Configuration

### AWS S3 Setup

1. **Create S3 Bucket**: Set up an AWS S3 bucket for firmware storage
2. **Configure Hostname**: Update `OTA_AWS_HOSTNAME` in `wifi_ota_config.h`
3. **Set Firmware Path**: Configure `OTA_FIRMWARE_URL` for firmware file location
4. **Version File**: Optionally create `OTA_VERSION_CHECK_URL` for version metadata

### Current Configuration (wifi_ota_config.h)
```c
#define OTA_AWS_HOSTNAME               "catcollar-firmware.s3.ap-southeast-1.amazonaws.com"
#define OTA_FIRMWARE_URL               "firmware/catcollar_firmware.rps"
#define OTA_VERSION_CHECK_URL          "firmware/version.json"
```

### Firmware Version
```c
#define CATCOLLAR_FIRMWARE_VERSION_MAJOR    1
#define CATCOLLAR_FIRMWARE_VERSION_MINOR    0
#define CATCOLLAR_FIRMWARE_VERSION_PATCH    0
#define CATCOLLAR_FIRMWARE_VERSION_BUILD    1
```

## Usage

### Automatic Operation

The OTA functionality is automatically integrated into the WiFi connection flow:

1. Device boots and connects to WiFi
2. After successful IP configuration, OTA manager initializes
3. System performs initial version check against AWS S3
4. If newer version is available, download begins automatically
5. Firmware is installed and device reboots with new version

### Manual Control

You can also manually control OTA operations:

```c
// Initialize OTA manager
sl_status_t status = catcollar_ota_init();

// Check for updates
catcollar_ota_check_for_updates();

// Get current state and status
catcollar_ota_state_t state = catcollar_ota_get_state();
catcollar_ota_status_t status = catcollar_ota_get_status();

// Manually start update if available
if (status == OTA_STATUS_UPDATE_AVAILABLE) {
    catcollar_ota_start_update();
}

// Start/stop periodic checking
catcollar_ota_start_periodic_check();
catcollar_ota_stop_periodic_check();
```

## OTA States

| State | Description |
|-------|-------------|
| `OTA_STATE_IDLE` | OTA manager is idle |
| `OTA_STATE_CHECKING_VERSION` | Checking server for new version |
| `OTA_STATE_DOWNLOADING` | Downloading firmware from server |
| `OTA_STATE_VERIFYING` | Verifying downloaded firmware |
| `OTA_STATE_INSTALLING` | Installing firmware update |
| `OTA_STATE_REBOOTING` | Rebooting to apply update |
| `OTA_STATE_SUCCESS` | Update completed successfully |
| `OTA_STATE_FAILED` | Update failed |

## OTA Status Codes

| Status | Description |
|--------|-------------|
| `OTA_STATUS_NO_UPDATE_AVAILABLE` | Firmware is up to date |
| `OTA_STATUS_UPDATE_AVAILABLE` | New firmware version available |
| `OTA_STATUS_DOWNLOAD_SUCCESS` | Firmware downloaded successfully |
| `OTA_STATUS_DOWNLOAD_FAILED` | Firmware download failed |
| `OTA_STATUS_VERIFY_FAILED` | Firmware verification failed |
| `OTA_STATUS_INSTALL_FAILED` | Firmware installation failed |
| `OTA_STATUS_NETWORK_ERROR` | Network connectivity issue |

## AWS S3 Firmware Hosting

### Directory Structure
```
s3://catcollar-firmware/
├── firmware/
│   ├── catcollar_firmware.rps    # Main firmware file
│   └── version.json              # Version metadata (optional)
└── certificates/
    └── aws_starfield_ca.pem      # SSL certificate
```

### Version Metadata Example (version.json)
```json
{
  "version": {
    "major": 1,
    "minor": 0,
    "patch": 1,
    "build": 2
  },
  "release_date": "2024-12-11",
  "changelog": [
    "Bug fixes for sensor calibration",
    "Improved battery management",
    "Enhanced WiFi connectivity"
  ]
}
```

## Security

- **HTTPS/TLS**: All communications use HTTPS with SSL/TLS encryption
- **Certificate Validation**: AWS Starfield CA certificate is used for validation
- **Secure Storage**: Firmware is stored securely in AWS S3
- **Version Integrity**: Version comparison prevents downgrades

## Troubleshooting

### Common Issues

1. **WiFi Not Connected**
   - Error: `OTA_STATUS_NETWORK_ERROR`
   - Solution: Ensure device is connected to WiFi before OTA operations

2. **DNS Resolution Failed**
   - Error: Download timeout or network error
   - Solution: Check AWS hostname configuration and network connectivity

3. **Certificate Issues**
   - Error: HTTPS connection failed
   - Solution: Verify AWS Starfield CA certificate is properly loaded

4. **Firmware Download Timeout**
   - Error: `OTA_STATUS_DOWNLOAD_FAILED`
   - Solution: Check network stability and increase `OTA_TIMEOUT_MS`

### Debug Information

Enable debug logging to troubleshoot issues:

```c
// Check current firmware version
catcollar_firmware_version_t version = catcollar_get_current_version();
app_log_info("Current version: %d.%d.%d.%d\r\n", 
             version.major, version.minor, version.patch, version.build);

// Monitor OTA state
catcollar_ota_state_t state = catcollar_ota_get_state();
app_log_info("OTA State: %d\r\n", state);
```

## Performance Considerations

- **Memory Usage**: OTA operations require sufficient heap memory for firmware buffers
- **Network Bandwidth**: Large firmware files may take time to download over slower connections
- **Power Management**: Ensure adequate power during OTA operations to prevent corruption
- **Timing**: OTA checks are performed after WiFi connection to ensure network availability

## Future Enhancements

1. **Delta Updates**: Implement differential firmware updates to reduce download size
2. **Rollback Support**: Add ability to rollback to previous firmware version
3. **Progress Reporting**: Add detailed download progress callbacks
4. **Custom Scheduling**: Allow user-defined update schedules
5. **Multi-Server Support**: Support multiple firmware servers for redundancy

## Integration with Main Application

The OTA functionality is seamlessly integrated into the existing WiFi application:

- Initialized automatically after WiFi connection
- Runs periodic checks in background
- Provides status updates through logging
- Handles errors gracefully without affecting main application

## Testing

To test the OTA functionality:

1. **Setup Test Environment**:
   - Configure AWS S3 bucket with test firmware
   - Update configuration with test server details

2. **Version Testing**:
   - Build firmware with current version (e.g., 1.0.0.1)
   - Upload newer version to S3 (e.g., 1.0.0.2)
   - Power on device and monitor logs

3. **Network Testing**:
   - Test with different WiFi networks
   - Simulate network interruptions
   - Verify retry mechanisms

4. **Error Testing**:
   - Test with invalid server URLs
   - Test with corrupted firmware files
   - Test with certificate issues

## Conclusion

This OTA implementation provides a robust, secure, and automatic firmware update system for the Cat Collar device. It leverages the existing WiFi infrastructure and integrates seamlessly with the application architecture while providing comprehensive error handling and status reporting.