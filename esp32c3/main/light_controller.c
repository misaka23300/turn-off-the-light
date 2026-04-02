#include "light_controller.h"

// 日志标签
const char *LIGHT_CONTROLLER_TAG = "light_controller";

// 默认配置
const light_controller_config_t LIGHT_CONTROLLER_CONFIG_DEFAULT = {
    .sound_threshold = SOUND_THRESHOLD,
    .silence_duration_ms = SOUND_SILENCE_DURATION,  // 1分钟静音
    .pir_detect_threshold = PIR_DETECT_THRESHOLD,
    .turn_off_hour_1 = TIME_MGR_TURN_OFF_HOUR_1,    // 中午12点
    .turn_off_hour_2 = TIME_MGR_TURN_OFF_HOUR_2,    // 晚上8点
    .turn_off_minute = TIME_MGR_TURN_OFF_MINUTE,
    .main_loop_delay_ms = 10000,                    // 10秒检查一次
    .enable_sound_monitor_task = true
};

// 静态变量（模块内部状态）
static light_controller_state_t current_state = LIGHT_CONTROLLER_STATE_UNINITIALIZED; // 当前系统状态
static light_state_t current_light_state = LIGHT_STATE_UNKNOWN; // 当前灯光状态
static light_controller_config_t current_config = LIGHT_CONTROLLER_CONFIG_DEFAULT; // 当前配置
static TaskHandle_t sound_monitor_task_handle = NULL; // 声音监测任务句柄
static int sound_silence_counter = 0; // 静音计数器
static bool motion_detected = false; // 人体检测状态

// 静态函数声明
static esp_err_t initialize_modules(void);
static esp_err_t cleanup_modules(void);
static void sound_monitor_task(void *pvParameters);

/**
 * @brief 初始化自动关灯系统
 * 
 * 配置系统参数，初始化各功能模块
 * 
 * @param config 配置参数，NULL表示使用默认配置
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_init(const light_controller_config_t *config)
{
    if (current_state != LIGHT_CONTROLLER_STATE_UNINITIALIZED) {
        ESP_LOGW(LIGHT_CONTROLLER_TAG, "自动关灯系统已初始化");
        return ESP_OK;
    }
    
    current_state = LIGHT_CONTROLLER_STATE_INITIALIZING;
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "开始初始化自动关灯系统");
    
    // 使用配置参数
    if (config != NULL) {
        current_config = *config;
        ESP_LOGI(LIGHT_CONTROLLER_TAG, "使用自定义配置");
    } else {
        ESP_LOGI(LIGHT_CONTROLLER_TAG, "使用默认配置");
    }
    
    // 初始化各模块
    esp_err_t ret = initialize_modules();
    if (ret != ESP_OK) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "模块初始化失败: %s", esp_err_to_name(ret));
        current_state = LIGHT_CONTROLLER_STATE_ERROR;
        return ret;
    }
    
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "自动关灯系统初始化完成");
    return ESP_OK;
}

/**
 * @brief 启动自动关灯系统
 * 
 * 启动声音监测任务，设置初始状态为开灯
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_start(void)
{
    if (current_state != LIGHT_CONTROLLER_STATE_INITIALIZING) {
        ESP_LOGW(LIGHT_CONTROLLER_TAG, "系统未就绪，当前状态: %d", current_state);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "启动自动关灯系统");
    
    // 启动声音监测任务
    if (current_config.enable_sound_monitor_task) {
        BaseType_t ret = xTaskCreate(sound_monitor_task, "sound_monitor", 4096, NULL, 5, &sound_monitor_task_handle);
        if (ret != pdPASS) {
            ESP_LOGW(LIGHT_CONTROLLER_TAG, "创建声音监测任务失败，将继续运行");
        } else {
            ESP_LOGI(LIGHT_CONTROLLER_TAG, "声音监测任务已启动");
        }
    }
    
    // 初始状态：开灯
    esp_err_t ret = light_controller_turn_on();
    if (ret != ESP_OK) {
        ESP_LOGW(LIGHT_CONTROLLER_TAG, "初始开灯失败: %s", esp_err_to_name(ret));
    }
    
    current_state = LIGHT_CONTROLLER_STATE_RUNNING;
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "自动关灯系统已启动");
    
    return ESP_OK;
}

/**
 * @brief 停止自动关灯系统
 * 
 * 停止声音监测任务，清理各模块
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_stop(void)
{
    if (current_state != LIGHT_CONTROLLER_STATE_RUNNING) {
        ESP_LOGW(LIGHT_CONTROLLER_TAG, "系统未运行，当前状态: %d", current_state);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "停止自动关灯系统");
    
    // 停止声音监测任务
    if (sound_monitor_task_handle != NULL) {
        vTaskDelete(sound_monitor_task_handle);
        sound_monitor_task_handle = NULL;
        ESP_LOGI(LIGHT_CONTROLLER_TAG, "声音监测任务已停止");
    }
    
    // 清理各模块
    esp_err_t ret = cleanup_modules();
    if (ret != ESP_OK) {
        ESP_LOGW(LIGHT_CONTROLLER_TAG, "模块清理失败: %s", esp_err_to_name(ret));
    }
    
    current_state = LIGHT_CONTROLLER_STATE_STOPPED;
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "自动关灯系统已停止");
    
    return ESP_OK;
}

/**
 * @brief 主控制循环
 * 
 * 检查是否需要关灯或开灯，执行相应操作
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_main_loop(void)
{
    if (current_state != LIGHT_CONTROLLER_STATE_RUNNING) {
        ESP_LOGW(LIGHT_CONTROLLER_TAG, "系统未运行，当前状态: %d", current_state);
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = ESP_OK;
    
    // 检查是否需要关灯
    bool need_turn_off = false;
    ret = light_controller_check_turn_off(&need_turn_off);
    if (ret == ESP_OK && need_turn_off) {
        ret = light_controller_turn_off();
        if (ret != ESP_OK) {
            ESP_LOGE(LIGHT_CONTROLLER_TAG, "关灯失败: %s", esp_err_to_name(ret));
        }
    }
    
    // 检查是否需要开灯
    bool need_turn_on = false;
    ret = light_controller_check_turn_on(&need_turn_on);
    if (ret == ESP_OK && need_turn_on) {
        ret = light_controller_turn_on();
        if (ret != ESP_OK) {
            ESP_LOGE(LIGHT_CONTROLLER_TAG, "开灯失败: %s", esp_err_to_name(ret));
        }
    }
    
    return ESP_OK;
}

/**
 * @brief 获取系统状态
 * 
 * @return light_controller_state_t 当前系统状态
 */
light_controller_state_t light_controller_get_state(void)
{
    return current_state;
}

/**
 * @brief 获取灯光状态
 * 
 * @return light_state_t 当前灯光状态
 */
light_state_t light_controller_get_light_state(void)
{
    return current_light_state;
}

/**
 * @brief 打开灯光
 * 
 * 控制舵机转到开灯角度
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_turn_on(void)
{
    esp_err_t ret = servo_turn_on();
    if (ret != ESP_OK) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "舵机开灯失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    current_light_state = LIGHT_STATE_ON;
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "灯光已打开");
    
    return ESP_OK;
}

/**
 * @brief 关闭灯光
 * 
 * 控制舵机转到关灯角度
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_turn_off(void)
{
    esp_err_t ret = servo_turn_off();
    if (ret != ESP_OK) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "舵机关灯失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    current_light_state = LIGHT_STATE_OFF;
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "灯光已关闭");
    
    return ESP_OK;
}

/**
 * @brief 检查是否需要关灯
 * 
 * 检查下课时间和声音/人体检测状态
 * 
 * @param[out] need_turn_off 输出参数，是否需要关灯
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_check_turn_off(bool *need_turn_off)
{
    if (need_turn_off == NULL) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "输出参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    *need_turn_off = false;
    
    // 检查时间关灯（下课时间：12点和8点）
    bool time_to_turn_off = false;
    esp_err_t ret = time_mgr_is_time_to_turn_off(&time_to_turn_off);
    if (ret == ESP_OK && time_to_turn_off) {
        // 下课时间检测是否有人
        bool motion = pir_get_status();
        if (!motion) {
            *need_turn_off = true;
            ESP_LOGI(LIGHT_CONTROLLER_TAG, "下课时间无人，执行关灯");
        } else {
            ESP_LOGI(LIGHT_CONTROLLER_TAG, "下课时间有人，保持开灯");
        }
        return ESP_OK;
    }
    
    // 检查声音关灯（仅作为辅助，不推荐作为主要检测方法）
    // 由于人体检测模块无法覆盖全教室，可能导致误关灯
    if (sound_silence_counter * 1000 >= current_config.silence_duration_ms && !motion_detected) {
        *need_turn_off = true;
        ESP_LOGI(LIGHT_CONTROLLER_TAG, "静音且无人，执行关灯");
        sound_silence_counter = 0; // 重置计数器
    }
    
    return ESP_OK;
}

/**
 * @brief 检查是否需要开灯
 * 
 * 检查人体感应状态
 * 
 * @param[out] need_turn_on 输出参数，是否需要开灯
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t light_controller_check_turn_on(bool *need_turn_on)
{
    if (need_turn_on == NULL) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "输出参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    *need_turn_on = false;
    
    // 检查人体感应
    bool motion = pir_get_status();
    motion_detected = motion;
    
    if (motion && current_light_state != LIGHT_STATE_ON) {
        *need_turn_on = true;
        ESP_LOGI(LIGHT_CONTROLLER_TAG, "人体感应开灯条件满足");
    }
    
    return ESP_OK;
}

/**
 * @brief 打印系统状态
 * 
 * 打印当前系统状态、灯光状态、人体感应状态等信息
 */
void light_controller_print_status(void)
{
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "=== 系统状态 ===");
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "系统状态: %d", current_state);
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "灯光状态: %d", current_light_state);
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "人体感应: %s", motion_detected ? "检测到" : "未检测到");
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "静音计数: %d 秒", sound_silence_counter);
    
    // 时间状态
    time_mgr_state_t time_state = time_mgr_get_state();
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "时间状态: %d", time_state);
    
    if (time_state == TIME_MGR_STATE_SYNCED) {
        struct tm timeinfo;
        if (time_mgr_get_current_time(&timeinfo) == ESP_OK) {
            char time_str[32];
            time_mgr_format_time(&timeinfo, time_str, sizeof(time_str));
            ESP_LOGI(LIGHT_CONTROLLER_TAG, "当前时间: %s", time_str);
        }
    }
    
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "=== 状态结束 ===");
}

/**
 * @brief 初始化各功能模块
 * 
 * 初始化舵机、人体感应、声音检测和时间管理模块
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
static esp_err_t initialize_modules(void)
{
    esp_err_t ret = ESP_OK;
    
    // 初始化舵机模块
    ret = servo_init();
    if (ret != ESP_OK) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "舵机模块初始化失败");
        return ret;
    }
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "舵机模块初始化成功");
    
    // 初始化人体感应模块
    ret = pir_init();
    if (ret != ESP_OK) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "人体感应模块初始化失败");
        return ret;
    }
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "人体感应模块初始化成功");
    
    // 初始化声音检测模块
    ret = sound_init();
    if (ret != ESP_OK) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "声音检测模块初始化失败");
        return ret;
    }
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "声音检测模块初始化成功");
    
    // 初始化时间管理模块
    ret = time_mgr_init();
    if (ret != ESP_OK) {
        ESP_LOGE(LIGHT_CONTROLLER_TAG, "时间管理模块初始化失败");
        return ret;
    }
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "时间管理模块初始化成功");
    
    // 开始时间同步
    ret = time_mgr_sync();
    if (ret != ESP_OK) {
        ESP_LOGW(LIGHT_CONTROLLER_TAG, "时间同步失败，系统将继续运行");
        // 不返回错误，允许系统在没有时间同步的情况下运行
    }
    
    return ESP_OK;
}

/**
 * @brief 清理各功能模块
 * 
 * 执行必要的清理操作
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
static esp_err_t cleanup_modules(void)
{
    // 注意：大多数模块不需要显式清理
    // 这里可以添加必要的清理操作
    
    return ESP_OK;
}

/**
 * @brief 声音监测任务
 * 
 * 每1分钟检测一次声音，更新静音计数器
 * 
 * @param pvParameters 任务参数
 */
static void sound_monitor_task(void *pvParameters)
{
    ESP_LOGI(LIGHT_CONTROLLER_TAG, "声音监测任务开始（每1分钟检测一次）");
    
    while (1) {
        bool sound_detected = sound_get_status();
        
        if (!sound_detected) {
            sound_silence_counter++;
            ESP_LOGD(LIGHT_CONTROLLER_TAG, "未检测到声音，静音计数: %d", sound_silence_counter);
            
            // 当检测到静音时，立即检测是否有人
            if (sound_silence_counter >= 1) { // 1分钟静音
                bool motion = pir_get_status();
                motion_detected = motion;
                if (!motion) {
                    ESP_LOGD(LIGHT_CONTROLLER_TAG, "静音且无人，准备关灯");
                }
            }
        } else {
            sound_silence_counter = 0;
            ESP_LOGD(LIGHT_CONTROLLER_TAG, "检测到声音");
        }
        
        vTaskDelay(pdMS_TO_TICKS(60000)); // 每1分钟检测一次
    }
    
    // 任务不应返回
    vTaskDelete(NULL);
}