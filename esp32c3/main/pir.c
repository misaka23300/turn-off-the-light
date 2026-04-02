#include "pir.h"

// 日志标签
const char *PIR_TAG = "pir";

// 静态变量（模块内部状态）
static uint8_t detect_count = 0; // 检测计数器，用于去抖

/**
 * @brief 初始化人体感应模块
 * 
 * 配置GPIO输入引脚，设置人体感应检测
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t pir_init(void)
{
    esp_err_t ret = ESP_OK;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(PIR_TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(PIR_TAG, "人体感应初始化完成，引脚 %d", PIR_PIN);
    return ESP_OK;
}

/**
 * @brief 检测人体活动
 * 
 * 读取GPIO电平并应用去抖算法，检测是否有人
 * 
 * @param[out] motion_detected 输出参数，是否检测到人体活动
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t pir_detect(bool *motion_detected)
{
    if (motion_detected == NULL) {
        ESP_LOGE(PIR_TAG, "输出参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    int level = gpio_get_level(PIR_PIN);
    
    if (level == 1) {
        // 检测到高电平（可能有人）
        if (detect_count < PIR_DETECT_THRESHOLD) {
            detect_count++;
            ESP_LOGD(PIR_TAG, "检测到高电平，计数: %d", detect_count);
        }
    } else {
        // 低电平（无人）
        if (detect_count > 0) {
            detect_count--;
            ESP_LOGD(PIR_TAG, "低电平，计数: %d", detect_count);
        }
    }
    
    // 连续检测到阈值次数才认为有人（去抖）
    *motion_detected = (detect_count >= PIR_DETECT_THRESHOLD);
    
    if (*motion_detected) {
        ESP_LOGD(PIR_TAG, "确认检测到人体活动");
    }
    
    return ESP_OK;
}

/**
 * @brief 获取人体感应状态（简化接口）
 * 
 * 内部调用pir_detect，返回检测结果
 * 
 * @return bool true表示检测到人体活动，false表示无活动
 */
bool pir_get_status(void)
{
    bool motion_detected = false;
    esp_err_t ret = pir_detect(&motion_detected);
    
    if (ret != ESP_OK) {
        ESP_LOGW(PIR_TAG, "获取状态失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    return motion_detected;
}

/**
 * @brief 重置人体感应检测计数
 * 
 * 清除去抖计数器，用于手动重置状态
 */
void pir_reset_detection(void)
{
    detect_count = 0;
    ESP_LOGI(PIR_TAG, "人体感应检测计数已重置");
}