#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "servo.h"
#include "pir.h"
#include "sound.h"
#include "time_mgr.h"

// 日志标签
extern const char *LIGHT_CONTROLLER_TAG;

// 系统状态
typedef enum {
    LIGHT_CONTROLLER_STATE_UNINITIALIZED = 0,  // 未初始化
    LIGHT_CONTROLLER_STATE_INITIALIZING,       // 初始化中
    LIGHT_CONTROLLER_STATE_RUNNING,            // 运行中
    LIGHT_CONTROLLER_STATE_ERROR,              // 错误状态
    LIGHT_CONTROLLER_STATE_STOPPED             // 已停止
} light_controller_state_t;

// 灯光状态
typedef enum {
    LIGHT_STATE_OFF = 0,   // 关灯
    LIGHT_STATE_ON,        // 开灯
    LIGHT_STATE_UNKNOWN    // 未知状态
} light_state_t;

// 系统配置
typedef struct {
    // 声音检测配置
    uint32_t sound_threshold;          // 声音检测阈值
    uint32_t silence_duration_ms;      // 静音持续时间（毫秒）
    
    // 人体感应配置
    uint8_t pir_detect_threshold;      // 人体感应去抖阈值
    
    // 时间配置
    uint8_t turn_off_hour_1;           // 第一个关灯时间（小时）
    uint8_t turn_off_hour_2;           // 第二个关灯时间（小时）
    uint8_t turn_off_minute;           // 关灯时间（分钟）
    
    // 系统配置
    uint32_t main_loop_delay_ms;       // 主循环延迟（毫秒）
    bool enable_sound_monitor_task;    // 是否启用声音监测任务
} light_controller_config_t;

// 默认配置
extern const light_controller_config_t LIGHT_CONTROLLER_CONFIG_DEFAULT;

/**
 * @brief 初始化自动关灯系统
 * 
 * 初始化所有模块，配置系统参数
 * 
 * @param config 系统配置（可为NULL，使用默认配置）
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_init(const light_controller_config_t *config);

/**
 * @brief 启动自动关灯系统
 * 
 * 启动所有任务，开始系统运行
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_start(void);

/**
 * @brief 停止自动关灯系统
 * 
 * 停止所有任务，清理资源
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_stop(void);

/**
 * @brief 主控制循环
 * 
 * 执行主要控制逻辑，协调各模块
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_main_loop(void);

/**
 * @brief 获取系统状态
 * 
 * @return light_controller_state_t 当前系统状态
 */
light_controller_state_t light_controller_get_state(void);

/**
 * @brief 获取灯光状态
 * 
 * @return light_state_t 当前灯光状态
 */
light_state_t light_controller_get_light_state(void);

/**
 * @brief 手动打开灯光
 * 
 * 忽略所有自动控制逻辑，强制打开灯光
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_turn_on(void);

/**
 * @brief 手动关闭灯光
 * 
 * 忽略所有自动控制逻辑，强制关闭灯光
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_turn_off(void);

/**
 * @brief 检查是否需要关灯
 * 
 * 根据时间、声音和人体感应判断是否需要关灯
 * 
 * @param[out] need_turn_off 输出参数，是否需要关灯
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_check_turn_off(bool *need_turn_off);

/**
 * @brief 检查是否需要开灯
 * 
 * 根据人体感应判断是否需要开灯
 * 
 * @param[out] need_turn_on 输出参数，是否需要开灯
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_check_turn_on(bool *need_turn_on);

/**
 * @brief 打印系统状态
 * 
 * 打印所有模块的当前状态到日志
 */
void light_controller_print_status(void);