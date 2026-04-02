#include <stdio.h>
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 日志标签
static const char *TAG = "test_mic";

// I2S 配置
#define I2S_BCLK_GPIO     9   // SCK
#define I2S_WS_GPIO       8   // WS
#define I2S_DIN_GPIO      10  // SD
#define SOUND_THRESHOLD   1000 // 声音检测阈值

static i2s_chan_handle_t i2s_rx_chan = NULL;

/**
 * @brief 初始化 I2S 麦克风
 */
static esp_err_t mic_init(void)
{
    esp_err_t ret = ESP_OK;
    
    // 配置 I2S 通道
    i2s_chan_config_t chan_cfg = {
        .id = 0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 64,
        .auto_clear = false,
    };
    
    ret = i2s_new_channel(&chan_cfg, NULL, &i2s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S通道创建失败: %s", esp_err_to_name(ret));
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
            .mclk = -1,
            .bclk = I2S_BCLK_GPIO,
            .ws = I2S_WS_GPIO,
            .dout = -1,
            .din = I2S_DIN_GPIO
        }
    };
    
    ret = i2s_channel_init_std_mode(i2s_rx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S通道配置失败: %s", esp_err_to_name(ret));
        i2s_del_channel(i2s_rx_chan);
        return ret;
    }
    
    ret = i2s_channel_enable(i2s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S通道启用失败: %s", esp_err_to_name(ret));
        i2s_del_channel(i2s_rx_chan);
        return ret;
    }
    
    ESP_LOGI(TAG, "麦克风初始化完成");
    ESP_LOGI(TAG, "BCLK(SCK): GPIO%d, WS: GPIO%d, DIN(SD): GPIO%d", 
             I2S_BCLK_GPIO, I2S_WS_GPIO, I2S_DIN_GPIO);
    
    return ESP_OK;
}

/**
 * @brief 读取麦克风响度
 */
static int read_mic_loudness(void)
{
    int16_t audio_buffer[64];
    size_t bytes_read;
    esp_err_t ret;
    
    ret = i2s_channel_read(i2s_rx_chan, audio_buffer, sizeof(audio_buffer), &bytes_read, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S读取失败: %s", esp_err_to_name(ret));
        return -1;
    }
    
    int sample_count = bytes_read / sizeof(int16_t);
    if (sample_count == 0) {
        return 0;
    }
    
    int32_t sum = 0;
    for (int i = 0; i < sample_count; i++) {
        sum += abs(audio_buffer[i]);
    }
    
    return sum / sample_count;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== 麦克风响度测试 ===");
    
    esp_err_t ret = mic_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "麦克风初始化失败");
        return;
    }
    
    ESP_LOGI(TAG, "开始测试，每秒输出一次响度...");
    ESP_LOGI(TAG, "阈值: %d", SOUND_THRESHOLD);
    
    while (1) {
        int loudness = read_mic_loudness();
        
        if (loudness >= 0) {
            // 显示响度条
            char bar[51];
            int bar_len = (loudness > 5000) ? 50 : (loudness / 100);
            if (bar_len < 0) bar_len = 0;
            if (bar_len > 50) bar_len = 50;
            
            for (int i = 0; i < 50; i++) {
                bar[i] = (i < bar_len) ? '#' : '-';
            }
            bar[50] = '\0';
            
            const char *status = (loudness > SOUND_THRESHOLD) ? "有声音" : "静音";
            ESP_LOGI(TAG, "响度: %5d [%s] %s", loudness, bar, status);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
