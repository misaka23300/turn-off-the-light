#pragma once

#include <time.h>
#include "freertos/FreeRTOS.h"
#include "esp_sntp.h"
#include "esp_log.h"

// 时区配置
#define TIME_MGR_TIMEZONE "UTC+8"  // 中国标准时间

// NTP服务器配置
#define TIME_MGR_NTP_SERVER_COUNT 3
#define TIME_MGR_NTP_SERVERS {"pool.ntp.org", "ntp1.aliyun.com", "ntp2.aliyun.com"}

// 时间同步配置
#define TIME_MGR_SYNC_RETRY_COUNT 10
#define TIME_MGR_SYNC_RETRY_DELAY_MS 2000

// 关灯时间配置
#define TIME_MGR_TURN_OFF_HOUR_1 12  // 12:00
#define TIME_MGR_TURN_OFF_HOUR_2 8   // 8:00
#define TIME_MGR_TURN_OFF_MINUTE 0

// 日志标签
extern const char *TIME_MGR_TAG;

/**
 * @brief 时间管理模块状态
 */
typedef enum {
    TIME_MGR_STATE_UNINITIALIZED = 0,  // 未初始化
    TIME_MGR_STATE_INITIALIZING,       // 初始化中
    TIME_MGR_STATE_SYNCING,            // 时间同步中
    TIME_MGR_STATE_SYNCED,             // 时间已同步
    TIME_MGR_STATE_SYNC_FAILED         // 时间同步失败
} time_mgr_state_t;

/**
 * @brief 初始化时间管理模块
 * 
 * 设置时区，初始化SNTP客户端
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_init(void);

/**
 * @brief 开始时间同步
 * 
 * 启动SNTP时间同步，等待同步完成
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_sync(void);

/**
 * @brief 获取当前时间
 * 
 * 返回当前系统时间
 * 
 * @param[out] timeinfo 输出参数，当前时间结构
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_get_current_time(struct tm *timeinfo);

/**
 * @brief 检查是否到达关灯时间
 * 
 * 检查当前时间是否为12:00或8:00
 * 
 * @param[out] is_time_to_turn_off 输出参数，是否到达关灯时间
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_is_time_to_turn_off(bool *is_time_to_turn_off);

/**
 * @brief 获取时间管理模块状态
 * 
 * @return time_mgr_state_t 当前状态
 */
time_mgr_state_t time_mgr_get_state(void);

/**
 * @brief 格式化时间字符串
 * 
 * 将时间结构格式化为字符串
 * 
 * @param timeinfo 时间结构
 * @param[out] buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_format_time(const struct tm *timeinfo, char *buffer, size_t buffer_size);

/**
 * @brief 打印当前时间
 * 
 * 打印格式化后的当前时间到日志
 */
void time_mgr_print_current_time(void);