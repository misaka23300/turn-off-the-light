#include "light_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 日志标签
static const char *TAG = "main";

// 主控制任务
static void main_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "主控制任务启动");

    // 初始化自动关灯系统（使用默认配置）
    esp_err_t ret = light_controller_init(NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "自动关灯系统初始化失败: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    // 启动自动关灯系统
    ret = light_controller_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "自动关灯系统启动失败: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    // 打印初始状态
    light_controller_print_status();

    int status_counter = 0;

    while (1)
    {
        // 执行主控制逻辑
        ret = light_controller_main_loop();
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "主控制循环执行失败: %s", esp_err_to_name(ret));

            // 尝试恢复系统
            if (light_controller_get_state() != LIGHT_CONTROLLER_STATE_RUNNING)
            {
                ESP_LOGW(TAG, "系统状态异常，尝试重启");
                light_controller_stop();
                vTaskDelay(pdMS_TO_TICKS(2000));
                light_controller_init(NULL);
                light_controller_start();
            }
        }

        // 定期打印状态（每30秒）
        if (++status_counter >= 30)
        {
            light_controller_print_status();
            status_counter = 0;
        }

        // 控制循环频率（每10秒检查一次）
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C3自动关灯系统启动");
    ESP_LOGI(TAG, "使用模块化设计，ESP错误处理");

    // 创建主控制任务
    BaseType_t ret = xTaskCreate(
        main_control_task,
        "main_control",
        4096,
        NULL,
        5,
        NULL);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "创建主控制任务失败");
        return;
    }

    ESP_LOGI(TAG, "系统启动完成，主控制任务已创建");
}