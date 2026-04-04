#include "VoiceInput.h"
#include <math.h>
#include "driver/i2s.h"
#include "esp_log.h"

static const char *TAG = "MIC";

#define I2S_WS_PIN   13
#define I2S_SCK_PIN  12
#define I2S_SD_PIN   14

#define I2S_PORT     I2S_NUM_0
#define SAMPLE_RATE  44100
#define BUFFER_SIZE  512

void VoiceInput::setup()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_PORT, &pin_config));

    ESP_LOGI(TAG, "I2S 初始化完成");
}

float VoiceInput::calculateRMS(int16_t* samples, int count)
{
    long long sum = 0;
    for (int i = 0; i < count; i++) {
        sum += (long long)samples[i] * samples[i];
    }
    return sqrt((float)sum / count);
}

int16_t VoiceInput::calculateMax(int16_t* samples, int count)
{
    int16_t maxVal = 0;
    for (int i = 0; i < count; i++) {
        int16_t v = abs(samples[i]);
        if (v > maxVal) maxVal = v;
    }
    return maxVal;
}