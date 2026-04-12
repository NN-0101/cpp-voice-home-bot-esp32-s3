//
// Created by 0101 on 2026/4/12.
//

#include "my_i2s.h"
#include "esp_log.h"

static const char* TAG = "MY_I2S";

my_i2s::my_i2s()
    : port_(I2S_NUM_0)
      , sample_rate_(1600)
      , ws_pin_(13)
      , sck_pin_(12)
      , sd_pin_(14)
      , is_initialized_(false) {
}

my_i2s::~my_i2s() {
    if (is_initialized_) {
        destroy();
    }
}

esp_err_t my_i2s::init(uint32_t sample_rate, int ws_pin, int sck_pin, int sd_pin) {
    // 保存配置
    sample_rate_ = sample_rate;
    ws_pin_ = ws_pin;
    sck_pin_ = sck_pin;
    sd_pin_ = sd_pin;

    // I2S 配置
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = sample_rate_,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
    };

    // 引脚配置
    i2s_pin_config_t pin_config = {
        .bck_io_num = sck_pin_,
        .ws_io_num = ws_pin_,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = sd_pin_
    };

    // 安装 I2S 驱动
    esp_err_t ret = i2s_driver_install(port_, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置 I2S 引脚
    ret = i2s_set_pin(port_, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 引脚设置失败: %s", esp_err_to_name(ret));
        i2s_driver_uninstall(port_);
        return ret;
    }

    is_initialized_ = true;
    ESP_LOGI(TAG, "I2S 初始化完成 (WS:%d, SCK:%d, SD:%d, 采样率:%lu Hz)",
             ws_pin_, sck_pin_, sd_pin_, sample_rate_);

    return ESP_OK;
}

esp_err_t my_i2s::read(void *buffer, size_t size, size_t *bytes_read, uint32_t timeout) {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "I2S 未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    return i2s_read(port_, buffer, size, bytes_read, timeout);
}

esp_err_t my_i2s::zero_dma_buffer() {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "I2S 未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    return i2s_zero_dma_buffer(port_);
}

esp_err_t my_i2s::destroy() {
    if (!is_initialized_) {
        return ESP_OK;
    }

    esp_err_t ret = i2s_driver_uninstall(port_);
    if (ret == ESP_OK) {
        is_initialized_ = false;
        ESP_LOGI(TAG, "I2S 反初始化完成");
    } else {
        ESP_LOGE(TAG, "I2S 反初始化失败: %s", esp_err_to_name(ret));
    }

    return ret;
}
