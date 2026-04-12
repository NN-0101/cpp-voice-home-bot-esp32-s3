//
// Created by 0101 on 2026/4/12.
//

#ifndef CPP_VOICE_HOME_BOT_ESP32_S3_MY_I2S_H
#define CPP_VOICE_HOME_BOT_ESP32_S3_MY_I2S_H

#include "esp_err.h"
#include "driver/i2s.h"

class my_i2s {
public:
    /**
     * 构造函数
     */
    my_i2s();

    /**
     * 析构函数
     */
    ~my_i2s();

    /**
     * @brief 初始化 I2S
     * @param sample_rate 采样率，默认 16000
     * @param ws_pin WS 引脚
     * @param sck_pin SCK 引脚
     * @param sd_pin SD 引脚
     * @return ESP_OK 成功，其他失败
    */
    esp_err_t init(uint32_t sample_rate = 16000, int ws_pin = 13, int sck_pin = 12, int sd_pin = 14);

    /**
     * @brief 读取音频数据
     * @param buffer 数据缓冲区
     * @param size 读取字节数
     * @param bytes_read 实际读取字节数
     * @param timeout 超时时间
     * @return ESP_OK 成功，其他失败
    */
    esp_err_t read(void *buffer, size_t size, size_t *bytes_read, uint32_t timeout);

    /**
     * @brief 清空 DMA 缓冲区
     * @return ESP_OK 成功，其他失败
    */
    esp_err_t zero_dma_buffer();

    /**
     * @brief 销毁、反初始化 I2S
     * @return ESP_OK 成功，其他失败
    */
    esp_err_t destroy();

    // 获取采样率
    [[nodiscard]] uint32_t get_sample_rate() const { return sample_rate_; }

    // 获取 I2S 端口号
    [[nodiscard]] i2s_port_t get_port() const { return port_; }

private:
    i2s_port_t port_; // I2S 端口号
    uint32_t sample_rate_; // 采样率
    int ws_pin_; // WS 引脚
    int sck_pin_; // SCK 引脚
    int sd_pin_; // SD 引脚
    bool is_initialized_; // 是否已初始化

    // 禁用拷贝构造和赋值
    my_i2s(const my_i2s &) = delete;

    my_i2s &operator=(const my_i2s &) = delete;
};


#endif //CPP_VOICE_HOME_BOT_ESP32_S3_MY_I2S_H
