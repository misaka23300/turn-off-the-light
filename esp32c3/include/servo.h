#pragma once

#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

// 舵机引脚定义
#define SERVO_PIN GPIO_NUM_6

// 舵机角度定义
#define SERVO_ANGLE_OPEN 90   // 开灯角度
#define SERVO_ANGLE_CLOSE 0   // 关灯角度

// LEDC PWM配置
#define SERVO_LEDC_TIMER              LEDC_TIMER_0
#define SERVO_LEDC_MODE               LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_CHANNEL            LEDC_CHANNEL_0
#define SERVO_LEDC_DUTY_RES           LEDC_TIMER_13_BIT // 13位分辨率（0-8191）
#define SERVO_LEDC_FREQUENCY          50                // 50Hz舵机标准频率
#define SERVO_MIN_PULSE_WIDTH         500               // 0度对应脉冲宽度（微秒）
#define SERVO_MAX_PULSE_WIDTH         2500              // 180度对应脉冲宽度（微秒）

// 日志标签
extern const char *SERVO_TAG;

/**
 * @brief 初始化舵机模块
 * 
 * 配置LEDC定时器和通道，设置PWM输出
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_init(void);

/**
 * @brief 设置舵机角度
 * 
 * 将角度转换为PWM脉冲宽度并输出
 * 
 * @param angle 角度值（0-180度）
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_set_angle(int angle);

/**
 * @brief 打开灯光（舵机转到开灯角度）
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_turn_on(void);

/**
 * @brief 关闭灯光（舵机转到关灯角度）
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_turn_off(void);