#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2s/my_i2s.h"
#include "wakenet/my_wake_net.h"
#include "wakenet/wake_detect_task.h"

static const char *TAG = "MAIN";

// 全局对象
static my_i2s i2s_mic;
static my_wake_net wakenet;
static wake_detect_task wake_task;

// 唤醒回调函数
void on_wake_up(void* user_data) {
    ESP_LOGI(TAG, "用户自定义唤醒处理逻辑...");

    // 这里可以添加你的业务逻辑：
    // - 点亮 LED
    // - 播放提示音
    // - 启动录音
    // - 发送网络请求
    // - 启动语音识别等

    // 示例：获取检测次数
    int count = wake_task.get_detect_count();
    ESP_LOGI(TAG, "已唤醒 %d 次", count);

    // 这里可以根据需求做更多操作
    switch (count % 3) {
        case 0:
            ESP_LOGI(TAG, "执行操作 A");
            break;
        case 1:
            ESP_LOGI(TAG, "执行操作 B");
            break;
        case 2:
            ESP_LOGI(TAG, "执行操作 C");
            break;
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "===== 语音唤醒系统启动 =====");
    ESP_LOGI(TAG, "芯片: ESP32-S3");

    // 初始化 I2S
    if (i2s_mic.init(16000, 13, 12, 14) != ESP_OK) {
        ESP_LOGE(TAG, "I2S 初始化失败，系统停止");
        return;
    }

    // 初始化 WakeNet
    if (wakenet.init("model", "hilexin", DET_MODE_90) != ESP_OK) {
        ESP_LOGE(TAG, "WakeNet 初始化失败，系统停止");
        return;
    }

    // 初始化检测任务
    if (wake_task.init(&i2s_mic, &wakenet) != ESP_OK) {
        ESP_LOGE(TAG, "检测任务初始化失败");
        return;
    }

    // 设置唤醒回调
    wake_task.set_wake_callback(on_wake_up, nullptr);

    // 启动检测任务
    if (wake_task.start() != ESP_OK) {
        ESP_LOGE(TAG, "启动检测任务失败");
        return;
    }

    ESP_LOGI(TAG, "系统初始化完成，等待唤醒...");

    // 主循环可以做其他事情
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}