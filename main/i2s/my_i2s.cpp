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
        // 工作模式: 主模式 + 接收模式 (ESP32 作为主机，从麦克风接收数据)
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),

        // 采样率: 16000 Hz (语音识别常用采样率)
        .sample_rate = sample_rate_,

        // 采样位宽: 16 位 (每个采样点用 16 bit 表示)
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,

        // 声道格式: 仅左声道 (麦克风通常是单声道)
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,

        // 通信格式: 标准 I2S 格式
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,

        // 中断分配标志: 优先级 1 (中断优先级)
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,

        // DMA 描述符数量: 8 个 (DMA 缓冲区个数，越多越不容易丢数据，但占内存)
        .dma_desc_num = 8,

        // DMA 帧数量: 512 个采样点 (单次 DMA 传输的数据量)
        .dma_frame_num = 512,

        // 是否使用 APLL: 否 (APLL 是 ESP32 的高精度时钟源，一般不需要)
        .use_apll = false,

        // 发送描述符自动清除: 是 (发生欠载时自动清除，避免产生噪声)
        .tx_desc_auto_clear = true,

        // 固定 MCLK 输出: 0 (0 表示不固定，由系统自动计算)
        .fixed_mclk = 0,

        // MCLK 倍率: 256 倍 (主时钟 = 采样率 × 256)
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,

        // 每个通道的总位数: 默认 (默认等于 bits_per_sample，即 16 位)
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,

        // 通道掩码: 单声道 (选择启用哪些通道)
        .chan_mask = I2S_CHANNEL_MONO,

        // 总通道数: 1 个
        .total_chan = 1,

        // 左对齐模式: 否 (数据在 WS 信号后延迟一个时钟开始)
        .left_align = false,

        // 大端模式: 否 (使用小端字节序)
        .big_edin = false,

        // MSB 优先: 否 (高位在前，标准 I2S 格式)
        .bit_order_msb = false,

        // 跳过掩码: 否 (不跳过任何通道的数据)
        .skip_msk = false,
    };

    // 引脚配置
    i2s_pin_config_t pin_config = {
        // 主时钟引脚: 不使用
        .mck_io_num = I2S_PIN_NO_CHANGE,

        // 位时钟引脚: 接麦克风的 SCK (串行时钟，控制数据传输时序)
        .bck_io_num = sck_pin_,

        // 字选引脚: 接麦克风的 WS (左右声道选择，也作为帧同步信号)
        .ws_io_num = ws_pin_,

        // 数据输出引脚: 不使用 (这是 ESP32 发送数据的引脚，麦克风只需要接收)
        .data_out_num = I2S_PIN_NO_CHANGE,

        // 数据输入引脚: 接麦克风的 SD (串行数据，麦克风采集的音频数据)
        .data_in_num = sd_pin_
    };

    // 安装 I2S 驱动
    esp_err_t ret = i2s_driver_install(port_, &i2s_config, 0, nullptr);
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
