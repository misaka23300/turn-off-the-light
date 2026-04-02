#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 日志标签
const char *SOUND_TAG = "sound";

// 静态变量（模块内部状态）
static uint32_t sound_threshold = 1000;       // 声音检测阈值
static uint32_t silence_duration = 3000;      // 静音持续时间（毫秒）
static bool monitor_task_running = false;     // 监测任务运行状态
static i2s_chan_handle_t i2s_rx_chan = NULL; // I2S 接收通道句柄

// 任务函数声明
static void sound_monitor_task(void *pvParameters);

/**
 * @brief 初始化声音检测模块
 * 
 * 配置I2S接口和麦克风，设置音频采样
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_init(void)
{
    esp_err_t ret = ESP_OK;
    
    // 配置 I2S 通道
    i2s_chan_config_t chan_cfg = {
        .id = 0, // I2S 通道 ID
        .role = I2S_ROLE_MASTER, // 主机模式
        .dma_desc_num = 8, // DMA 描述符数量
        .dma_frame_num = 64, // 每个描述符的帧数
        .auto_clear = false, // 自动清除
    };
    
    // 创建 I2S 接收通道
    ret = i2s_new_channel(&chan_cfg, NULL, &i2s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(SOUND_TAG, "I2S通道创建失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置 I2S 标准模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 44100,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT
        },
        .gpio_cfg = {
            .mclk = -1, // 不需要主时钟
            .bclk = 9,  // SCK (BCLK) GPIO
            .ws = 8,    // WS GPIO
            .dout = -1, // 不需要输出
            .din = 10   // SD (数据输入) GPIO
        }
    };
    
    // 配置接收通道
    ret = i2s_channel_init_std_mode(i2s_rx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(SOUND_TAG, "I2S通道配置失败: %s", esp_err_to_name(ret));
        i2s_del_channel(i2s_rx_chan);
        i2s_rx_chan = NULL;
        return ret;
    }
    
    // 启用 I2S 接收通道
    ret = i2s_channel_enable(i2s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(SOUND_TAG, "I2S通道启用失败: %s", esp_err_to_name(ret));
        i2s_del_channel(i2s_rx_chan);
        i2s_rx_chan = NULL;
        return ret;
    }

    ESP_LOGI(SOUND_TAG, "I2S麦克风初始化完成");
    ESP_LOGI(SOUND_TAG, "采样率: 44100 Hz, 阈值: %d, 静音时长: %d ms", 
             sound_threshold, silence_duration);
    
    return ESP_OK;
}

/**
 * @brief 检测是否有声音
 * 
 * 读取音频数据并计算平均幅度，判断是否超过阈值
 * 
 * @param[out] sound_detected 输出参数，是否检测到声音
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_detect(bool *sound_detected)
{
    if (sound_detected == NULL) {
        ESP_LOGE(SOUND_TAG, "输出参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (i2s_rx_chan == NULL) {
        ESP_LOGE(SOUND_TAG, "I2S通道未初始化");
        *sound_detected = false;
        return ESP_ERR_INVALID_STATE;
    }
    
    int16_t audio_buffer[64];
    size_t bytes_read;
    esp_err_t ret = ESP_OK;
    
    // 读取音频数据
    ret = i2s_channel_read(i2s_rx_chan, audio_buffer, sizeof(audio_buffer), &bytes_read, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(SOUND_TAG, "I2S读取失败: %s", esp_err_to_name(ret));
        *sound_detected = false;
        return ret;
    }
    
    // 计算平均幅度
    int32_t sum = 0;
    int sample_count = bytes_read / sizeof(int16_t);
    
    if (sample_count == 0) {
        ESP_LOGW(SOUND_TAG, "未读取到音频数据");
        *sound_detected = false;
        return ESP_OK;
    }
    
    for (int i = 0; i < sample_count; i++) {
        sum += abs(audio_buffer[i]);
    }
    
    int avg = sum / sample_count;
    *sound_detected = (avg > sound_threshold);
    
    ESP_LOGD(SOUND_TAG, "音频平均幅度: %d, 阈值: %d, 检测到声音: %s", 
             avg, sound_threshold, *sound_detected ? "是" : "否");
    
    return ESP_OK;
}

/**
 * @brief 获取声音检测状态（简化接口）
 * 
 * 内部调用sound_detect，返回检测结果
 * 
 * @return bool true表示检测到声音，false表示静音
 */
bool sound_get_status(void)
{
    bool sound_detected = false;
    esp_err_t ret = sound_detect(&sound_detected);
    
    if (ret != ESP_OK) {
        ESP_LOGW(SOUND_TAG, "获取状态失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    return sound_detected;
}

/**
 * @brief 开始声音监测任务
 * 
 * 创建FreeRTOS任务，持续监测声音状态
 * 
 * @param[out] task_handle 输出参数，任务句柄
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_start_monitor_task(TaskHandle_t *task_handle)
{
    if (task_handle == NULL) {
        ESP_LOGE(SOUND_TAG, "任务句柄参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (monitor_task_running) {
        ESP_LOGW(SOUND_TAG, "声音监测任务已在运行");
        return ESP_OK;
    }
    
    BaseType_t ret = xTaskCreate(sound_monitor_task, "sound_monitor", 4096, NULL, 5, task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(SOUND_TAG, "创建声音监测任务失败");
        return ESP_FAIL;
    }
    
    monitor_task_running = true;
    ESP_LOGI(SOUND_TAG, "声音监测任务已启动");
    
    return ESP_OK;
}

/**
 * @brief 停止声音监测任务
 * 
 * 删除FreeRTOS任务，停止声音监测
 * 
 * @param task_handle 任务句柄
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t sound_stop_monitor_task(TaskHandle_t task_handle)
{
    if (!monitor_task_running) {
        ESP_LOGW(SOUND_TAG, "声音监测任务未运行");
        return ESP_OK;
    }
    
    if (task_handle == NULL) {
        ESP_LOGE(SOUND_TAG, "任务句柄无效");
        return ESP_ERR_INVALID_ARG;
    }
    
    vTaskDelete(task_handle);
    monitor_task_running = false;
    
    ESP_LOGI(SOUND_TAG, "声音监测任务已停止");
    
    return ESP_OK;
}

/**
 * @brief 设置声音检测阈值
 * 
 * 动态调整声音检测灵敏度
 * 
 * @param threshold 新的阈值
 */
void sound_set_threshold(uint32_t threshold)
{
    sound_threshold = threshold;
    ESP_LOGI(SOUND_TAG, "声音检测阈值已更新为: %d", threshold);
}

/**
 * @brief 设置静音持续时间
 * 
 * 动态调整静音检测时间
 * 
 * @param duration_ms 新的静音持续时间（毫秒）
 */
void sound_set_silence_duration(uint32_t duration_ms)
{
    silence_duration = duration_ms;
    ESP_LOGI(SOUND_TAG, "静音持续时间已更新为: %d ms", duration_ms);
}

/**
 * @brief 声音监测任务
 * 
 * 持续监测声音状态，更新静音计数器
 * 
 * @param pvParameters 任务参数
 */
static void sound_monitor_task(void *pvParameters)
{
    int silence_counter = 0;
    
    ESP_LOGI(SOUND_TAG, "声音监测任务开始运行");
    
    while (1) {
        bool sound_detected = false;
        esp_err_t ret = sound_detect(&sound_detected);
        
        if (ret != ESP_OK) {
            ESP_LOGW(SOUND_TAG, "声音检测失败，等待重试");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        if (!sound_detected) {
            silence_counter++;
            ESP_LOGD(SOUND_TAG, "未检测到声音，静音计数: %d", silence_counter);
        } else {
            silence_counter = 0;
            ESP_LOGD(SOUND_TAG, "检测到声音");
        }
        
        // 注意：这里的关灯逻辑由主控制器处理
        // 此任务只负责监测声音状态
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // 任务不应返回
    vTaskDelete(NULL);
}