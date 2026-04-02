#pragma once

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"

// 人体感应引脚定义
#define PIR_PIN GPIO_NUM_7

// 检测阈值（去抖次数）
#define PIR_DETECT_THRESHOLD 3

// 日志标签
extern const char *PIR_TAG;

/**
 * @brief 初始化人体感应模块
 * 
 * 配置GPIO输入引脚，设置人体感应检测
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t pir_init(void);

/**
 * @brief 检测人体活动
 * 
 * 读取GPIO电平并应用去抖算法，检测是否有人
 * 
 * @param[out] motion_detected 输出参数，是否检测到人体活动
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t pir_detect(bool *motion_detected);

/**
 * @brief 获取人体感应状态（简化接口）
 * 
 * 内部调用pir_detect，返回检测结果
 * 
 * @return bool true表示检测到人体活动，false表示无活动
 */
bool pir_get_status(void);

/**
 * @brief 重置人体感应检测计数
 * 
 * 清除去抖计数器，用于手动重置状态
 */
void pir_reset_detection(void);