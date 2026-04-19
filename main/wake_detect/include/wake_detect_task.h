//
// Created by 0101 on 2026/4/12.
//

#ifndef CPP_VOICE_HOME_BOT_ESP32_S3_WAKE_DETECT_TASK_H
#define CPP_VOICE_HOME_BOT_ESP32_S3_WAKE_DETECT_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../../i2s_driver/include/i2s_driver.h"
#include "wake_net.h"

class wake_detect_task {
public:
    /**
        * @brief 构造函数
    */
    wake_detect_task();

    /**
     * @brief 析构函数
     */
    ~wake_detect_task();

    // 禁用拷贝构造和赋值
    wake_detect_task(const wake_detect_task &) = delete;

    wake_detect_task &operator=(const wake_detect_task &) = delete;

    /**
     * @brief 初始化检测任务
     * @param i2s I2S 实例指针
     * @param wake_net WakeNet 实例指针
     * @return ESP_OK 成功，其他失败
    */
    esp_err_t init(i2s_driver *i2s, wake_net *wake_net);

    /**
     * @brief 启动检测任务
     * @param task_priority 任务优先级，默认 5
     * @param stack_size 任务栈大小，默认 20480
     * @return ESP_OK 成功，其他失败
    */
    esp_err_t start(UBaseType_t task_priority = 5, uint32_t stack_size = 20480);

    /**
     * @brief 停止检测任务
     * @return ESP_OK 成功，其他失败
    */
    esp_err_t stop();

    /**
     * @brief 检查任务是否正在运行
     * @return true 正在运行，false 已停止
    */
    [[nodiscard]] bool is_running() const { return task_handle_ != nullptr; }

    /**
     * @brief 设置唤醒回调函数
     * @param callback 回调函数指针
     * @param user_data 用户数据指针
    */
    void set_wake_callback(void (*callback)(void *user_data), void *user_data = nullptr);

    /**
     * @brief 获取检测次数
     * @return 检测到的唤醒次数
    */
    [[nodiscard]] int get_detect_count() const { return detect_count_; }

private:
    i2s_driver *i2s_; // I2S 实例
    wake_net *wake_net_; // WakeNet 实例
    int16_t *audio_buffer_; // 音频缓冲区
    TaskHandle_t task_handle_; // 任务句柄
    bool should_stop_; // 停止标志
    int detect_count_; // 检测计数

    // 回调相关
    void (*wake_callback_)(void *); // 唤醒回调函数
    void *callback_user_data_; // 回调用户数据

    /**
     * @brief 任务主循环（静态方法，用于 FreeRTOS 任务创建）
     */
    static void task_loop(void *param);

    /**
     * @brief 实际的任务执行函数
     */
    void run();

    /**
     * @brief 处理唤醒事件
     */
    void on_wake_detected();

    /**
     * @brief 清理资源
     */
    void cleanup();
};


#endif //CPP_VOICE_HOME_BOT_ESP32_S3_WAKE_DETECT_TASK_H
