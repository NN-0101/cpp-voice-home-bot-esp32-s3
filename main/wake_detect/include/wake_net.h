//
// Created by 0101 on 2026/4/12.
//

#ifndef CPP_VOICE_HOME_BOT_ESP32_S3_MY_WAKENET_H
#define CPP_VOICE_HOME_BOT_ESP32_S3_MY_WAKENET_H

#include "esp_err.h"
#include "esp_wn_iface.h"

class wake_net {
public:
    /**
     * @brief 构造函数
     */
    wake_net();

    /**
     * @brief 析构函数
     */
    ~wake_net();

    // 禁用拷贝构造和赋值
    wake_net(const wake_net &) = delete;

    wake_net &operator=(const wake_net &) = delete;

    /**
     * @brief 初始化唤醒词引擎
     * @param model_partition 模型分区名称，默认 "model"
     * @param wake_word 唤醒词名称，默认 "hilexin"
     * @param det_mode 检测模式，默认 DET_MODE_90 (90% 准确率)
     * @return ESP_OK 成功，其他失败
    */
    [[nodiscard]] esp_err_t init(const char *model_partition = "model", const char *wake_word = "hilexin",
                                 det_mode_t det_mode = DET_MODE_90);

    /**
     * @brief 检测音频数据中是否包含唤醒词
     * @param audio_data 音频数据缓冲区
     * @return >0 检测到唤醒词，0 未检测到，<0 错误
    */
    [[nodiscard]] int detect(const int16_t *audio_data);

    /**
     * @brief 获取单次检测所需的音频采样点数
     * @return 采样点数量
    */
    [[nodiscard]] int get_chunk_size() const { return audio_chunk_size_; }



private:
    const esp_wn_iface_t *wake_net_; // 唤醒词接口
    model_iface_data_t *model_data_; // 模型实例数据
    char *model_name_; // 模型名称
    int audio_chunk_size_; // 音频块大小（采样点数）
    bool is_initialized_; // 是否已初始化

    /**
     * @brief 清理资源
     */
    void cleanup();
};


#endif //CPP_VOICE_HOME_BOT_ESP32_S3_MY_WAKENET_H
