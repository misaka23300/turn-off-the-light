#include <stdio.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 日志标签
static const char *TAG = "test_servo";

// 舵机配置
#define SERVO_GPIO        6   // 舵机PWM引脚
#define SERVO_LEDC_TIMER  LEDC_TIMER_0
#define SERVO_LEDC_MODE   LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_CHANNEL LEDC_CHANNEL_0
#define SERVO_LEDC_DUTY_RES LEDC_TIMER_13_BIT // 13位分辨率
#define SERVO_LEDC_FREQUENCY 50 // 50Hz舵机标准频率
#define SERVO_MIN_PULSE_WIDTH 500  // 0度对应脉冲宽度（微秒）
#define SERVO_MAX_PULSE_WIDTH 2500 // 180度对应脉冲宽度（微秒）

/**
 * @brief 初始化舵机
 */
static esp_err_t servo_init(void)
{
    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_LEDC_MODE,
        .timer_num        = SERVO_LEDC_TIMER,
        .duty_resolution  = SERVO_LEDC_DUTY_RES,
        .freq_hz          = SERVO_LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC定时器配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置LEDC通道
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = SERVO_LEDC_CHANNEL,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_GPIO,
        .duty           = 0,
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC通道配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "舵机初始化完成，引脚: GPIO%d", SERVO_GPIO);
    return ESP_OK;
}

/**
 * @brief 设置舵机角度
 */
static esp_err_t servo_set_angle(int angle)
{
    // 限制角度范围
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    // 将角度转换为脉冲宽度
    uint32_t pulse_width = SERVO_MIN_PULSE_WIDTH + 
                          (angle * (SERVO_MAX_PULSE_WIDTH - SERVO_MIN_PULSE_WIDTH) / 180);
    
    // 将脉冲宽度转换为占空比
    uint32_t duty = (pulse_width * (1 << SERVO_LEDC_DUTY_RES)) / 20000;
    
    esp_err_t ret = ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置占空比失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "更新占空比失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "舵机角度: %d°, 脉冲宽度: %d us", angle, pulse_width);
    return ESP_OK;
}

/**
 * @brief 测试舵机转动
 */
static void servo_test_sequence(void)
{
    ESP_LOGI(TAG, "=== 开始舵机测试序列 ===");
    
    // 测试1: 0度
    ESP_LOGI(TAG, "【测试1】转到 0度");
    servo_set_angle(0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试2: 45度
    ESP_LOGI(TAG, "【测试2】转到 45度");
    servo_set_angle(45);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试3: 90度
    ESP_LOGI(TAG, "【测试3】转到 90度");
    servo_set_angle(90);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试4: 135度
    ESP_LOGI(TAG, "【测试4】转到 135度");
    servo_set_angle(135);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试5: 180度
    ESP_LOGI(TAG, "【测试5】转到 180度");
    servo_set_angle(180);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 测试6: 回到90度
    ESP_LOGI(TAG, "【测试6】回到 90度");
    servo_set_angle(90);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "=== 测试序列完成 ===");
}

/**
 * @brief 连续扫描测试
 */
static void servo_scan_test(void)
{
    ESP_LOGI(TAG, "=== 开始连续扫描测试 ===");
    
    for (int i = 0; i <= 180; i += 10) {
        servo_set_angle(i);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    for (int i = 180; i >= 0; i -= 10) {
        servo_set_angle(i);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    ESP_LOGI(TAG, "=== 扫描测试完成 ===");
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== 舵机转动测试 ===");
    
    esp_err_t ret = servo_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "舵机初始化失败");
        return;
    }
    
    // 初始位置
    ESP_LOGI(TAG, "设置初始位置: 90度");
    servo_set_angle(90);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    int test_cycle = 0;
    
    while (1) {
        test_cycle++;
        ESP_LOGI(TAG, "\n========== 测试循环 %d ==========", test_cycle);
        
        // 执行测试序列
        servo_test_sequence();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 执行扫描测试
        servo_scan_test();
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
