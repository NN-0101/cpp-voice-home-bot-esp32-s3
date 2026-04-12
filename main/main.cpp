#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

// 自定义类
#include "i2s/my_i2s.h"
#include "wakenet/my_wake_net.h"

// ================= TAG =================
static const char *TAG = "WAKE";

// ================= 全局对象 =================
static my_i2s i2s_mic;
static my_wake_net wakenet;
static int16_t *audio_buffer = nullptr;

// ================= 语音检测任务 =================
void detect_task(void *param)
{
    size_t bytes_read;
    int detect_count = 0;
    int chunk_size = wakenet.get_chunk_size();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始语音唤醒监听...");
    ESP_LOGI(TAG, "唤醒词: 'Hi Lexin' (你好乐鑫)");
    ESP_LOGI(TAG, "音频块大小: %d 采样点", chunk_size);
    ESP_LOGI(TAG, "========================================");

    while (1) {
        // 读取音频数据
        esp_err_t ret = i2s_mic.read(
            audio_buffer,
            chunk_size * sizeof(int16_t),
            &bytes_read,
            portMAX_DELAY
        );

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2S 读取警告: %s", esp_err_to_name(ret));
            continue;
        }

        if (bytes_read == 0) {
            continue;
        }

        // 唤醒词检测
        int result = wakenet.detect(audio_buffer);

        if (result > 0) {
            detect_count++;
            ESP_LOGW(TAG, "🎉🎉🎉 检测到唤醒词！(第 %d 次) 🎉🎉🎉", detect_count);

            // 检测到后短暂延迟，避免重复触发
            vTaskDelay(pdMS_TO_TICKS(500));

            // 清空 I2S 缓冲区
            i2s_mic.zero_dma_buffer();
        }

        // 喂狗
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ================= 主函数 =================
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

    // 分配音频缓冲区
    int chunk_size = wakenet.get_chunk_size();
    audio_buffer = (int16_t *)malloc(chunk_size * sizeof(int16_t));
    if (audio_buffer == nullptr) {
        ESP_LOGE(TAG, "音频缓冲区分配失败");
        return;
    }

    // 创建检测任务
    xTaskCreate(detect_task, "detect_task", 20480, NULL, 5, NULL);
}