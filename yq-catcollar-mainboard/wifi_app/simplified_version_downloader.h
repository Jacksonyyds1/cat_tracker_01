/**
 * @file simplified_version_downloader.h
 * @brief 基于现有wifi_ota_manager代码的简化版本下载器头文件
 * @author YQ05165
 * @date 2025-09-16
 */

#ifndef SIMPLIFIED_VERSION_DOWNLOADER_H
#define SIMPLIFIED_VERSION_DOWNLOADER_H

#include "sl_status.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 下载版本信息（主要接口）
 * 
 * 这个函数复用现有OTA管理器中的DNS解析和网络连接逻辑，
 * 用于下载S3上的version.txt文件内容。
 * 
 * @param version_buffer 版本信息输出缓冲区
 * @param buffer_size 缓冲区大小 (建议至少64字节)
 * @return sl_status_t 
 *         - SL_STATUS_OK: 下载成功
 *         - SL_STATUS_INVALID_PARAMETER: 参数无效
 *         - SL_STATUS_FAIL: 下载失败
 * 
 * @note 此函数复用了wifi_ota_manager.c中的DNS解析逻辑
 * @note 自动适配AWS_ENABLE/AZURE_ENABLE配置，或使用默认S3配置
 * 
 * @example
 * ```c
 * char version[64];
 * sl_status_t status = simplified_download_version(version, sizeof(version));
 * if (status == SL_STATUS_OK) {
 *     printf("Remote version: %s\n", version);
 * }
 * ```
 */
sl_status_t simplified_download_version(char *version_buffer, size_t buffer_size);

/**
 * @brief 验证版本字符串格式
 * 
 * 检查版本字符串是否符合 "major.minor.patch.build" 格式，
 * 并验证各个版本号是否在有效范围内。
 * 
 * @param version_str 版本字符串
 * @return true 格式正确且在有效范围内
 * @return false 格式错误或超出范围
 * 
 * @example
 * ```c
 * if (simplified_validate_version("1.2.3.456")) {
 *     // 版本格式正确
 * }
 * ```
 */
bool simplified_validate_version(const char *version_str);

/**
 * @brief 测试版本下载功能
 * 
 * 执行完整的版本下载测试，包括：
 * - DNS解析测试
 * - 网络连接测试 
 * - 版本信息下载
 * - 格式验证
 * 
 * @return sl_status_t 
 *         - SL_STATUS_OK: 测试成功
 *         - SL_STATUS_FAIL: 测试失败
 * 
 * @note 此函数主要用于调试和验证网络配置
 * @note 可以通过Shell命令或调试界面调用
 */
sl_status_t simplified_test_version_download(void);

/**
 * @brief OTA系统集成接口
 * 
 * 专门为现有OTA系统提供的版本获取接口，
 * 可以直接被version_manager.c调用。
 * 
 * @param version_buffer 版本信息输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return sl_status_t 状态码
 * 
 * @note 这个接口的命名和参数与现有OTA代码风格保持一致
 */
sl_status_t get_remote_version_for_ota(char *version_buffer, size_t buffer_size);

// ========== 内部函数声明（仅供内部使用） ==========
// Static functions are not declared in header files - they are internal to the .c file

#ifdef __cplusplus
}
#endif

#endif /* SIMPLIFIED_VERSION_DOWNLOADER_H */