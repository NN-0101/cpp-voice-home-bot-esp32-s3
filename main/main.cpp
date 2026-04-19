// main.cpp
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2s_driver/include/i2s_driver.h"
#include "wake_detect/include/wake_net.h"
#include "wake_detect/include/wake_detect_task.h"
#include "audio/include/audio_player.h"

static const char *TAG = "MAIN";

// 全局对象
static i2s_driver i2s_mic;
static wake_net wakenet;
static wake_detect_task wake_task;

// 声明嵌入式 WAV 文件 - 使用 EMBED_TXTFILES 时的符号名称格式
extern "C" {
    extern const uint8_t hello1_wav_start[] asm("_binary_hello1_wav_start");
    extern const uint8_t hello1_wav_end[] asm("_binary_hello1_wav_end");
}

// 唤醒回调函数
void on_wake_up(void* user_data) {
    ESP_LOGI(TAG, "🎤 Wake word detected!");

    // 计算嵌入式文件大小
    size_t wav_size = hello1_wav_end - hello1_wav_start;

    // 播放提示音（异步，不阻塞唤醒检测）
    esp_err_t ret = audio_player::get_instance().play_async(hello1_wav_start, wav_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "播放提示音失败: %s", esp_err_to_name(ret));
    }

    int count = wake_task.get_detect_count();
    ESP_LOGI(TAG, "Total wake-ups: %d", count);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "===== Voice Wake-up System Starting =====");
    ESP_LOGI(TAG, "Chip: ESP32-S3");

    // 1. 初始化麦克风 I2S（I2S_NUM_0）
    if (i2s_mic.init(i2s_driver::Direction::RX_ONLY, 16000, 13, 12, 14) != ESP_OK) {
        ESP_LOGE(TAG, "I2S mic init failed, system halted");
        return;
    }

    // 2. 初始化扬声器 I2S（I2S_NUM_1，使用新引脚）
    if (audio_player::get_instance().init(16000, 15, 16, 7) != ESP_OK) {
        ESP_LOGE(TAG, "Audio player init failed, system halted");
        return;
    }

    // 3. 初始化 WakeNet
    if (wakenet.init("model", "hilexin", DET_MODE_90) != ESP_OK) {
        ESP_LOGE(TAG, "WakeNet init failed, system halted");
        return;
    }

    // 4. 初始化检测任务
    if (wake_task.init(&i2s_mic, &wakenet) != ESP_OK) {
        ESP_LOGE(TAG, "Detection task init failed");
        return;
    }

    // 5. 设置唤醒回调
    wake_task.set_wake_callback(on_wake_up, nullptr);

    // 6. 启动检测任务
    if (wake_task.start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start detection task");
        return;
    }

    ESP_LOGI(TAG, "System initialized. Waiting for wake word 'Hi Lexin'...");
    ESP_LOGI(TAG, "==========================================");

    // 删除主任务，释放资源
    vTaskDelete(nullptr);
}