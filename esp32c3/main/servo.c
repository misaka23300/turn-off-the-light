#include "servo.h"

// 日志标签
const char *SERVO_TAG = "servo";

/**
 * @brief 初始化舵机模块
 * 
 * 配置LEDC定时器和通道，设置PWM输出用于控制舵机
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_init(void)
{
    esp_err_t ret = ESP_OK;
    
    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_LEDC_MODE,
        .timer_num        = SERVO_LEDC_TIMER,
        .duty_resolution  = SERVO_LEDC_DUTY_RES,
        .freq_hz          = SERVO_LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(SERVO_TAG, "LEDC定时器配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置LEDC通道
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = SERVO_LEDC_CHANNEL,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(SERVO_TAG, "LEDC通道配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(SERVO_TAG, "舵机PWM初始化完成，引脚 %d，频率 %d Hz", SERVO_PIN, SERVO_LEDC_FREQUENCY);
    return ESP_OK;
}

/**
 * @brief 设置舵机角度
 * 
 * 将角度转换为PWM脉冲宽度并输出，控制舵机转动到指定角度
 * 
 * @param angle 角度值（0-180度）
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_set_angle(int angle)
{
    esp_err_t ret = ESP_OK;
    
    // 限制角度范围
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // 将角度转换为脉冲宽度（微秒）
    uint32_t pulse_width = SERVO_MIN_PULSE_WIDTH + (angle * (SERVO_MAX_PULSE_WIDTH - SERVO_MIN_PULSE_WIDTH) / 180);
    
    // 将脉冲宽度转换为占空比
    // 占空比 = (脉冲宽度 / 周期) * (2^分辨率)
    // 周期 = 1 / 频率 = 1 / 50Hz = 20ms = 20000微秒
    uint32_t duty = (pulse_width * (1 << SERVO_LEDC_DUTY_RES)) / 20000;
    
    // 设置占空比
    ret = ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(SERVO_TAG, "设置占空比失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(SERVO_TAG, "更新占空比失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(SERVO_TAG, "设置舵机角度: %d°，脉冲宽度: %d us，占空比: %lu", angle, pulse_width, duty);
    return ESP_OK;
}

/**
 * @brief 打开灯光（舵机转到开灯角度）
 * 
 * 控制舵机转动到预设的开灯角度
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_turn_on(void)
{
    ESP_LOGI(SERVO_TAG, "打开灯光");
    return servo_set_angle(SERVO_ANGLE_OPEN);
}

/**
 * @brief 关闭灯光（舵机转到关灯角度）
 * 
 * 控制舵机转动到预设的关灯角度
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t servo_turn_off(void)
{
    ESP_LOGI(SERVO_TAG, "关闭灯光");
    return servo_set_angle(SERVO_ANGLE_CLOSE);
}