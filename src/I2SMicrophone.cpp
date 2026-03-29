#include "I2SMicrophone.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "I2SMicrophone";

I2SMicrophone::I2SMicrophone(int bck_pin, int ws_pin, int din_pin, int sample_rate)
    : rx_handle(nullptr), sample_rate(sample_rate), bck_pin(bck_pin), ws_pin(ws_pin), din_pin(din_pin) {
}

bool I2SMicrophone::init() {
    // 配置 I2S 通道
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear_after_cb = true;
    chan_cfg.auto_clear_before_cb = false;

    esp_err_t ret = i2s_new_channel(&chan_cfg, nullptr, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel");
        return false;
    }

    // 配置 I2S 标准模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)16, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)bck_pin,
            .ws = (gpio_num_t)ws_pin,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)din_pin,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        },
    };

    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S standard mode");
        return false;
    }

    // 启用通道
    ret = i2s_channel_enable(rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel");
        return false;
    }

    ESP_LOGI(TAG, "I2S microphone initialized (BCK=%d, WS=%d, DIN=%d, rate=%d)",
             bck_pin, ws_pin, din_pin, sample_rate);
    return true;
}

int I2SMicrophone::read(int16_t* buffer, int samples) {
    if (!rx_handle) {
        ESP_LOGE(TAG, "I2S not initialized");
        return -1;
    }

    size_t bytes_read = 0;
    esp_err_t err = i2s_channel_read(rx_handle, buffer, samples * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S read error: %d", err);
        return -1;
    }
    return bytes_read / sizeof(int16_t);
}

void I2SMicrophone::deinit() {
    if (rx_handle) {
        i2s_channel_disable(rx_handle);
        i2s_del_channel(rx_handle);
        rx_handle = nullptr;
    }
}