#pragma once

#include "freertos/FreeRTOS.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

// 声音检测配置
#define SOUND_THRESHOLD          1000    // 声音检测阈值
#define SOUND_SILENCE_DURATION   60000   // 静音持续时间（毫秒）- 1分钟

// 日志标签
extern const char *SOUND_TAG;

/**
 * @brief 初始化声音检测模块
 * 
 * 配置I2S接口和麦克风，设置音频采样
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_init(void);

/**
 * @brief 检测是否有声音
 * 
 * 读取音频数据并计算平均幅度，判断是否超过阈值
 * 
 * @param[out] sound_detected 输出参数，是否检测到声音
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_detect(bool *sound_detected);

/**
 * @brief 获取声音检测状态（简化接口）
 * 
 * 内部调用sound_detect，返回检测结果
 * 
 * @return bool true表示检测到声音，false表示静音
 */
bool sound_get_status(void);

/**
 * @brief 开始声音监测任务
 * 
 * 创建FreeRTOS任务，持续监测声音状态
 * 
 * @param[out] task_handle 输出参数，任务句柄
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_start_monitor_task(TaskHandle_t *task_handle);

/**
 * @brief 停止声音监测任务
 * 
 * 删除FreeRTOS任务，停止声音监测
 * 
 * @param task_handle 任务句柄
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_stop_monitor_task(TaskHandle_t task_handle);

/**
 * @brief 设置声音检测阈值
 * 
 * 动态调整声音检测灵敏度
 * 
 * @param threshold 新的阈值
 */
void sound_set_threshold(uint32_t threshold);

/**
 * @brief 设置静音持续时间
 * 
 * 动态调整静音检测时间
 * 
 * @param duration_ms 新的静音持续时间（毫秒）
 */
void sound_set_silence_duration(uint32_t duration_ms);