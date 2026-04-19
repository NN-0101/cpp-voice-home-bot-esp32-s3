//
// Created by 0101 on 2026/4/12.
//

#include "include/i2s_driver.h"
#include "esp_log.h"
#include "esp_check.h"
#include <cstring>

static const char* TAG = "MY_I2S";

i2s_driver::i2s_driver(i2s_port_t port)
    : port_(port)
    , direction_(Direction::RX_ONLY)
    , sample_rate_(16000)
    , ws_pin_(13)
    , sck_pin_(12)
    , sd_pin_(14)
    , is_initialized_(false) {
}

i2s_driver::~i2s_driver() {
    if (is_initialized_) {
        destroy();
    }
}

esp_err_t i2s_driver::init(Direction dir, uint32_t sample_rate, int ws_pin, int sck_pin, int sd_pin) {
    if (is_initialized_) {
        ESP_LOGW(TAG, "[I2S%d] Already initialized", port_);
        return ESP_OK;
    }

    // 保存配置
    direction_ = dir;
    sample_rate_ = sample_rate;
    ws_pin_ = ws_pin;
    sck_pin_ = sck_pin;
    sd_pin_ = sd_pin;

    // 根据方向设置 I2S 模式
    i2s_mode_t mode;
    if (direction_ == Direction::RX_ONLY) {
        mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
    } else {  // Direction::TX_ONLY
        mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    }

    // I2S 配置（通用部分）
    i2s_config_t i2s_config = {
        .mode = mode,
        .sample_rate = sample_rate_,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
        .chan_mask = I2S_CHANNEL_MONO,
        .total_chan = 1,
        .left_align = false,
        .big_edin = false,
        .bit_order_msb = false,
        .skip_msk = false,
    };

    // 引脚配置（根据方向决定 data_in 还是 data_out）
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = sck_pin_,
        .ws_io_num = ws_pin_,
        .data_out_num = (direction_ == Direction::TX_ONLY) ? sd_pin_ : I2S_PIN_NO_CHANGE,
        .data_in_num = (direction_ == Direction::RX_ONLY) ? sd_pin_ : I2S_PIN_NO_CHANGE,
    };

    // 安装 I2S 驱动
    esp_err_t ret = i2s_driver_install(port_, &i2s_config, 0, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[I2S%d] Driver install failed: %s", port_, esp_err_to_name(ret));
        return ret;
    }

    // 设置 I2S 引脚
    ret = i2s_set_pin(port_, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[I2S%d] Pin set failed: %s", port_, esp_err_to_name(ret));
        i2s_driver_uninstall(port_);
        return ret;
    }

    // 如果是 RX 模式，启动后清空一次 DMA 缓冲区
    if (direction_ == Direction::RX_ONLY) {
        i2s_zero_dma_buffer(port_);
    }

    is_initialized_ = true;
    const char* dir_str = (direction_ == Direction::RX_ONLY) ? "RX" : "TX";
    ESP_LOGI(TAG, "[I2S%d] %s initialized - WS:%d, SCK:%d, SD:%d, Rate:%lu Hz",
             port_, dir_str, ws_pin_, sck_pin_, sd_pin_, sample_rate_);

    return ESP_OK;
}

esp_err_t i2s_driver::read(void *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "[I2S%d] Not initialized", port_);
        return ESP_ERR_INVALID_STATE;
    }

    if (direction_ != Direction::RX_ONLY) {
        ESP_LOGE(TAG, "[I2S%d] Not configured for RX", port_);
        return ESP_ERR_INVALID_STATE;
    }

    if (buffer == nullptr) {
        ESP_LOGE(TAG, "[I2S%d] Buffer is null", port_);
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return i2s_read(port_, buffer, size, bytes_read, ticks);
}

esp_err_t i2s_driver::write(const void *buffer, size_t size, size_t *bytes_written, uint32_t timeout_ms) {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "[I2S%d] Not initialized", port_);
        return ESP_ERR_INVALID_STATE;
    }

    if (direction_ != Direction::TX_ONLY) {
        ESP_LOGE(TAG, "[I2S%d] Not configured for TX", port_);
        return ESP_ERR_INVALID_STATE;
    }

    if (buffer == nullptr) {
        ESP_LOGE(TAG, "[I2S%d] Buffer is null", port_);
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return i2s_write(port_, buffer, size, bytes_written, ticks);
}

esp_err_t i2s_driver::zero_dma_buffer() {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "[I2S%d] Not initialized", port_);
        return ESP_ERR_INVALID_STATE;
    }

    if (direction_ != Direction::RX_ONLY) {
        ESP_LOGW(TAG, "[I2S%d] zero_dma_buffer only valid for RX mode", port_);
        return ESP_ERR_INVALID_STATE;
    }

    return i2s_zero_dma_buffer(port_);
}

esp_err_t i2s_driver::destroy() {
    if (!is_initialized_) {
        return ESP_OK;
    }

    esp_err_t ret = i2s_driver_uninstall(port_);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "[I2S%d] Destroyed", port_);
        is_initialized_ = false;
    } else {
        ESP_LOGE(TAG, "[I2S%d] Destroy failed: %s", port_, esp_err_to_name(ret));
    }

    return ret;
}