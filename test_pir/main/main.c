#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 日志标签
static const char *TAG = "test_pir";

// PIR 配置
#define PIR_GPIO          7   // 人体检测模块信号引脚
#define PIR_DETECT_THRESHOLD 3 // 检测阈值（去抖次数）

static uint8_t detect_count = 0;

/**
 * @brief 初始化 PIR 人体检测模块
 */
static esp_err_t pir_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "PIR初始化完成，引脚: GPIO%d", PIR_GPIO);
    return ESP_OK;
}

/**
 * @brief 检测人体活动（带去抖）
 */
static bool pir_detect(void)
{
    int level = gpio_get_level(PIR_GPIO);
    
    if (level == 1) {
        // 检测到高电平
        if (detect_count < PIR_DETECT_THRESHOLD) {
            detect_count++;
        }
    } else {
        // 低电平
        if (detect_count > 0) {
            detect_count--;
        }
    }
    
    return (detect_count >= PIR_DETECT_THRESHOLD);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== 人体检测测试 ===");
    
    esp_err_t ret = pir_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PIR初始化失败");
        return;
    }
    
    ESP_LOGI(TAG, "开始测试，每秒输出一次检测状态...");
    ESP_LOGI(TAG, "检测阈值: %d", PIR_DETECT_THRESHOLD);
    
    bool last_state = false;
    int detection_count = 0;
    
    while (1) {
        bool motion = pir_detect();
        int raw_level = gpio_get_level(PIR_GPIO);
        
        // 状态变化时输出
        if (motion != last_state) {
            if (motion) {
                ESP_LOGI(TAG, "【有人进入】检测到人体活动！");
                detection_count++;
            } else {
                ESP_LOGI(TAG, "【无人】人体活动消失");
            }
            last_state = motion;
        }
        
        // 每秒输出一次状态
        static int counter = 0;
        if (++counter >= 10) {
            const char *status = motion ? "有人" : "无人";
            const char *raw = raw_level ? "高电平" : "低电平";
            ESP_LOGI(TAG, "状态: %s (原始: %s), 检测次数: %d, 去抖计数: %d", 
                     status, raw, detection_count, detect_count);
            counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
