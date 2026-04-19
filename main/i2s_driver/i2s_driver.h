//
// Created by 0101 on 2026/4/12.
//

#ifndef CPP_VOICE_HOME_BOT_ESP32_S3_MY_I2S_H
#define CPP_VOICE_HOME_BOT_ESP32_S3_MY_I2S_H

#include "esp_err.h"
#include "driver/i2s.h"

class i2s_driver {
public:
    /**
     * @brief I2S 工作方向枚举
     */
    enum class Direction {
        RX_ONLY,   // 仅接收模式（用于麦克风输入）
        TX_ONLY,   // 仅发送模式（用于扬声器输出）
    };

    /**
     * @brief 构造函数
     * @param port I2S 端口号，默认 I2S_NUM_0
     */
    explicit i2s_driver(i2s_port_t port = I2S_NUM_0);

    /**
     * @brief 析构函数
     */
    ~i2s_driver();

    // 禁用拷贝构造和赋值
    i2s_driver(const i2s_driver &) = delete;
    i2s_driver &operator=(const i2s_driver &) = delete;

    /**
     * @brief 初始化 I2S
     * @param dir 工作方向（RX_ONLY 或 TX_ONLY）
     * @param sample_rate 采样率，默认 16000 Hz
     * @param ws_pin WS/LRC 引脚（字选/左右声道时钟）
     * @param sck_pin SCK/BCLK 引脚（位时钟）
     * @param sd_pin SD/DIN 引脚（数据引脚）
     * @return ESP_OK 成功，其他失败
     */
    esp_err_t init(Direction dir,
                   uint32_t sample_rate = 16000,
                   int ws_pin = 13,
                   int sck_pin = 12,
                   int sd_pin = 14);

    /**
     * @brief 读取音频数据（仅 RX 模式有效）
     * @param buffer 数据缓冲区
     * @param size 要读取的字节数
     * @param bytes_read 实际读取的字节数（可为 nullptr）
     * @param timeout_ms 超时时间（毫秒），0 表示不等待
     * @return ESP_OK 成功，其他失败
     */
    esp_err_t read(void *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms);

    /**
     * @brief 写入音频数据（仅 TX 模式有效）
     * @param buffer 数据缓冲区
     * @param size 要写入的字节数
     * @param bytes_written 实际写入的字节数（可为 nullptr）
     * @param timeout_ms 超时时间（毫秒），0 表示不等待
     * @return ESP_OK 成功，其他失败
     */
    esp_err_t write(const void *buffer, size_t size, size_t *bytes_written, uint32_t timeout_ms);

    /**
     * @brief 清空 DMA 缓冲区（仅 RX 模式有效）
     * @return ESP_OK 成功，其他失败
     */
    esp_err_t zero_dma_buffer();

    /**
     * @brief 销毁/反初始化 I2S
     * @return ESP_OK 成功，其他失败
     */
    esp_err_t destroy();

    // === Getters ===
    [[nodiscard]] i2s_port_t get_port() const { return port_; }
    [[nodiscard]] uint32_t get_sample_rate() const { return sample_rate_; }
    [[nodiscard]] Direction get_direction() const { return direction_; }
    [[nodiscard]] bool is_initialized() const { return is_initialized_; }
    [[nodiscard]] int get_ws_pin() const { return ws_pin_; }
    [[nodiscard]] int get_sck_pin() const { return sck_pin_; }
    [[nodiscard]] int get_sd_pin() const { return sd_pin_; }

private:
    i2s_port_t port_;           // I2S 端口号
    Direction direction_;       // 工作方向
    uint32_t sample_rate_;      // 采样率
    int ws_pin_;                // WS/LRC 引脚
    int sck_pin_;               // SCK/BCLK 引脚
    int sd_pin_;                // SD/DIN 引脚
    bool is_initialized_;       // 是否已初始化
};

#endif //CPP_VOICE_HOME_BOT_ESP32_S3_MY_I2S_H