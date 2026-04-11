#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2s.h"
#include "esp_log.h"

// ESP-SR
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"

// ================= TAG =================
static const char *TAG = "WAKE";

// ================= I2S =================
#define I2S_WS_PIN   13
#define I2S_SCK_PIN  12
#define I2S_SD_PIN   14

#define I2S_PORT     I2S_NUM_0
#define SAMPLE_RATE  16000

// ================= WakeNet =================
static const esp_wn_iface_t *wakenet = NULL;
static model_iface_data_t *model_data = NULL;
static int16_t *audio_buffer = NULL;
static int audio_chunksize = 0;

// ================= I2S 初始化 =================
esp_err_t i2s_init()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    esp_err_t ret = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_set_pin(I2S_PORT, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 引脚设置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S 初始化完成 (WS:%d, SCK:%d, SD:%d)", I2S_WS_PIN, I2S_SCK_PIN, I2S_SD_PIN);
    return ESP_OK;
}

// ================= WakeNet 初始化 =================
esp_err_t wakenet_init()
{
    // 从 model 分区加载模型
    ESP_LOGI(TAG, "正在从 'model' 分区加载模型...");
    srmodel_list_t *models = esp_srmodel_init("model");
    if (models == NULL) {
        ESP_LOGE(TAG, "模型分区 'model' 加载失败！");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "模型分区加载成功");

    // 筛选 hilexin 唤醒词模型
    char *model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, "hilexin");
    if (model_name == NULL) {
        ESP_LOGE(TAG, "未找到 'hilexin' 唤醒词模型！");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "找到模型: %s", model_name);

    // 获取唤醒词句柄
    wakenet = esp_wn_handle_from_name(model_name);
    ESP_LOGI(TAG, "wakenet ptr: %p", wakenet);
    if (wakenet == NULL) {
        ESP_LOGE(TAG, "获取唤醒词句柄失败！");
        return ESP_FAIL;
    }
    if (wakenet) {
        ESP_LOGI(TAG, "create func ptr: %p", wakenet->create);
        ESP_LOGI(TAG, "detect func ptr: %p", wakenet->detect);
    }

    // 创建模型实例
    model_data = wakenet->create(model_name, DET_MODE_90);
    if (model_data == NULL) {
        ESP_LOGE(TAG, "创建模型实例失败！");
        return ESP_FAIL;
    }

    // 获取音频块大小并分配缓冲区
    audio_chunksize = wakenet->get_samp_chunksize(model_data);
    audio_buffer = (int16_t *)malloc(audio_chunksize * sizeof(int16_t));
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败！");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WakeNet 初始化成功，音频块大小: %d 采样点 (%.1f ms)",
             audio_chunksize, (float)audio_chunksize / SAMPLE_RATE * 1000);
    return ESP_OK;
}

// ================= 语音检测任务 =================
void detect_task(void *param)
{
    size_t bytes_read;
    int detect_count = 0;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始语音唤醒监听...");
    ESP_LOGI(TAG, "唤醒词: 'Hi Lexin' (你好乐鑫)");
    ESP_LOGI(TAG, "========================================");

    while (1) {
        // 读取音频数据
        esp_err_t ret = i2s_read(
            I2S_PORT,
            audio_buffer,
            audio_chunksize * sizeof(int16_t),
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
        int result = wakenet->detect(model_data, audio_buffer);

        if (result > 0) {
            detect_count++;
            ESP_LOGW(TAG, "🎉🎉🎉 检测到唤醒词！(第 %d 次) 🎉🎉🎉", detect_count);

            // 检测到后短暂延迟，避免重复触发
            vTaskDelay(pdMS_TO_TICKS(500));

            // 清空 I2S 缓冲区
            i2s_zero_dma_buffer(I2S_PORT);
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
    if (i2s_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2S 初始化失败，系统停止");
        return;
    }

    // 初始化 WakeNet
    if (wakenet_init() != ESP_OK) {
        ESP_LOGE(TAG, "WakeNet 初始化失败，系统停止");
        return;
    }

    // 创建检测任务
    xTaskCreate(detect_task, "detect_task", 20480, NULL, 5, NULL);
}