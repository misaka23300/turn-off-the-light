#include "time_mgr.h"

// 日志标签
const char *TIME_MGR_TAG = "time_mgr";

// 静态变量（模块内部状态）
static time_mgr_state_t current_state = TIME_MGR_STATE_UNINITIALIZED; // 当前状态
static const char *ntp_servers[TIME_MGR_NTP_SERVER_COUNT] = TIME_MGR_NTP_SERVERS; // NTP服务器列表

/**
 * @brief 初始化时间管理模块
 * 
 * 设置时区，初始化SNTP客户端
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_init(void)
{
    esp_err_t ret = ESP_OK;
    
    if (current_state != TIME_MGR_STATE_UNINITIALIZED) {
        ESP_LOGW(TIME_MGR_TAG, "时间管理模块已初始化");
        return ESP_OK;
    }
    
    current_state = TIME_MGR_STATE_INITIALIZING;
    
    // 设置时区
    ret = setenv("TZ", TIME_MGR_TIMEZONE, 1);
    if (ret != 0) {
        ESP_LOGE(TIME_MGR_TAG, "设置时区失败");
        current_state = TIME_MGR_STATE_SYNC_FAILED;
        return ESP_FAIL;
    }
    tzset();
    
    ESP_LOGI(TIME_MGR_TAG, "时区设置为: %s", TIME_MGR_TIMEZONE);
    current_state = TIME_MGR_STATE_SYNCING;
    
    return ESP_OK;
}

/**
 * @brief 开始时间同步
 * 
 * 启动SNTP时间同步，等待同步完成
 * 
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_sync(void)
{
    if (current_state != TIME_MGR_STATE_SYNCING) {
        ESP_LOGW(TIME_MGR_TAG, "时间同步未就绪，当前状态: %d", current_state);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TIME_MGR_TAG, "开始时间同步");
    
    // 配置SNTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    for (int i = 0; i < TIME_MGR_NTP_SERVER_COUNT; i++) {
        sntp_setservername(i, (char *)ntp_servers[i]);
        ESP_LOGI(TIME_MGR_TAG, "添加NTP服务器: %s", ntp_servers[i]);
    }
    
    sntp_init();
    
    // 等待时间同步
    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < TIME_MGR_SYNC_RETRY_COUNT) {
        ESP_LOGI(TIME_MGR_TAG, "等待时间同步... (尝试 %d/%d)", retry, TIME_MGR_SYNC_RETRY_COUNT);
        vTaskDelay(pdMS_TO_TICKS(TIME_MGR_SYNC_RETRY_DELAY_MS));
    }
    
    if (retry < TIME_MGR_SYNC_RETRY_COUNT) {
        current_state = TIME_MGR_STATE_SYNCED;
        ESP_LOGI(TIME_MGR_TAG, "时间同步成功");
        time_mgr_print_current_time();
        return ESP_OK;
    } else {
        current_state = TIME_MGR_STATE_SYNC_FAILED;
        ESP_LOGW(TIME_MGR_TAG, "时间同步失败");
        return ESP_FAIL;
    }
}

/**
 * @brief 获取当前时间
 * 
 * 返回当前系统时间
 * 
 * @param[out] timeinfo 输出参数，当前时间结构
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_get_current_time(struct tm *timeinfo)
{
    if (timeinfo == NULL) {
        ESP_LOGE(TIME_MGR_TAG, "输出参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (current_state != TIME_MGR_STATE_SYNCED) {
        ESP_LOGW(TIME_MGR_TAG, "时间未同步，当前状态: %d", current_state);
        return ESP_ERR_INVALID_STATE;
    }
    
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
    
    return ESP_OK;
}

/**
 * @brief 检查是否到达关灯时间
 * 
 * 检查当前时间是否为12:00或8:00（下课时间）
 * 
 * @param[out] is_time_to_turn_off 输出参数，是否到达关灯时间
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_is_time_to_turn_off(bool *is_time_to_turn_off)
{
    if (is_time_to_turn_off == NULL) {
        ESP_LOGE(TIME_MGR_TAG, "输出参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (current_state != TIME_MGR_STATE_SYNCED) {
        ESP_LOGW(TIME_MGR_TAG, "时间未同步，无法检查关灯时间");
        *is_time_to_turn_off = false;
        return ESP_ERR_INVALID_STATE;
    }
    
    struct tm timeinfo;
    esp_err_t ret = time_mgr_get_current_time(&timeinfo);
    if (ret != ESP_OK) {
        *is_time_to_turn_off = false;
        return ret;
    }
    
    // 检查是否为12:00或8:00（下课时间）
    *is_time_to_turn_off = ((timeinfo.tm_hour == TIME_MGR_TURN_OFF_HOUR_1 && timeinfo.tm_min == TIME_MGR_TURN_OFF_MINUTE) ||
                           (timeinfo.tm_hour == TIME_MGR_TURN_OFF_HOUR_2 && timeinfo.tm_min == TIME_MGR_TURN_OFF_MINUTE));
    
    if (*is_time_to_turn_off) {
        ESP_LOGI(TIME_MGR_TAG, "到达关灯时间: %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    }
    
    return ESP_OK;
}

/**
 * @brief 获取时间管理模块状态
 * 
 * @return time_mgr_state_t 当前状态
 */
time_mgr_state_t time_mgr_get_state(void)
{
    return current_state;
}

/**
 * @brief 格式化时间字符串
 * 
 * 将时间结构格式化为字符串
 * 
 * @param timeinfo 时间结构
 * @param[out] buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t 错误码，ESP_OK表示成功
 */
esp_err_t time_mgr_format_time(const struct tm *timeinfo, char *buffer, size_t buffer_size)
{
    if (timeinfo == NULL || buffer == NULL) {
        ESP_LOGE(TIME_MGR_TAG, "参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (buffer_size < 20) {
        ESP_LOGE(TIME_MGR_TAG, "缓冲区大小不足");
        return ESP_ERR_INVALID_SIZE;
    }
    
    snprintf(buffer, buffer_size, "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    return ESP_OK;
}

/**
 * @brief 打印当前时间
 * 
 * 打印格式化后的当前时间到日志
 */
void time_mgr_print_current_time(void)
{
    if (current_state != TIME_MGR_STATE_SYNCED) {
        ESP_LOGW(TIME_MGR_TAG, "时间未同步，无法打印当前时间");
        return;
    }
    
    struct tm timeinfo;
    esp_err_t ret = time_mgr_get_current_time(&timeinfo);
    if (ret != ESP_OK) {
        ESP_LOGE(TIME_MGR_TAG, "获取当前时间失败");
        return;
    }
    
    char time_str[32];
    ret = time_mgr_format_time(&timeinfo, time_str, sizeof(time_str));
    if (ret != ESP_OK) {
        ESP_LOGE(TIME_MGR_TAG, "格式化时间失败");
        return;
    }
    
    ESP_LOGI(TIME_MGR_TAG, "当前时间: %s", time_str);
}