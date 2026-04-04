#include <stdio.h>
#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "VoiceInput.h"

// TAG
static const char *TAG = "MIC";

// I2S 引脚
#define I2S_WS_PIN   13
#define I2S_SCK_PIN  12
#define I2S_SD_PIN   14

#define I2S_PORT     I2S_NUM_0
#define SAMPLE_RATE  44100
#define BUFFER_SIZE  512

#define SOUND_THRESHOLD 200.0f

static int16_t sampleBuffer[BUFFER_SIZE];
static VoiceInput mic;

// ======================= 主任务 =======================
void mic_task(void *param)
{
    size_t bytesRead;

    ESP_LOGI(TAG, "开始监听声音...");

    while (1)
    {
        esp_err_t result = i2s_read(I2S_PORT,
                                   sampleBuffer,
                                   sizeof(sampleBuffer),
                                   &bytesRead,
                                   portMAX_DELAY);

        if (result == ESP_OK && bytesRead > 0)
        {
            int samplesRead = bytesRead / sizeof(int16_t);

            float rms = mic.calculateRMS(sampleBuffer, samplesRead);
            int16_t maxAmp = mic.calculateMax(sampleBuffer, samplesRead);

            if (rms > SOUND_THRESHOLD)
            {
                ESP_LOGI(TAG, "🔊 检测到声音 RMS=%.2f 峰值=%d", rms, maxAmp);
            }
            else
            {
                ESP_LOGI(TAG, "⏺ 静默 RMS=%.2f", rms);
            }
        }
        else
        {
            ESP_LOGE(TAG, "I2S 读取失败");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ======================= 入口 =======================
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "I2S 麦克风测试启动");

    mic.setup();

    xTaskCreate(mic_task, "mic_task", 4096, NULL, 5, NULL);
}