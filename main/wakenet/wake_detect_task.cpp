//
// Created by 0101 on 2026/4/12.
//

#include "wake_detect_task.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "WAKE_TASK";

wake_detect_task::wake_detect_task()
    : i2s_(nullptr)
      , wake_net_(nullptr)
      , audio_buffer_(nullptr)
      , task_handle_(nullptr)
      , should_stop_(false)
      , detect_count_(0)
      , wake_callback_(nullptr)
      , callback_user_data_(nullptr) {
}

wake_detect_task::~wake_detect_task() {
    stop();
    cleanup();
}

esp_err_t wake_detect_task::init(my_i2s *i2s, my_wake_net *wake_net) {
    if (i2s == nullptr || wake_net == nullptr) {
        ESP_LOGE(TAG, "无效的参数");
        return ESP_ERR_INVALID_ARG;
    }

    i2s_ = i2s;
    wake_net_ = wake_net;

    // 分配音频缓冲区
    int chunk_size = wake_net_->get_chunk_size();
    audio_buffer_ = static_cast<int16_t *>(malloc(chunk_size * sizeof(int16_t)));
    if (audio_buffer_ == nullptr) {
        ESP_LOGE(TAG, "音频缓冲区分配失败");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "初始化完成，音频块大小: %d 采样点", chunk_size);
    return ESP_OK;
}

esp_err_t wake_detect_task::start(UBaseType_t task_priority, uint32_t stack_size) {
    if (task_handle_ != nullptr) {
        ESP_LOGW(TAG, "任务已经在运行");
        return ESP_OK;
    }

    if (i2s_ == nullptr || wake_net_ == nullptr || audio_buffer_ == nullptr) {
        ESP_LOGE(TAG, "未初始化，请先调用 init()");
        return ESP_ERR_INVALID_STATE;
    }

    should_stop_ = false;

    BaseType_t ret = xTaskCreate(
        task_loop,
        "wake_detect",
        stack_size,
        this,
        task_priority,
        &task_handle_
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "检测任务已启动");
    return ESP_OK;
}

esp_err_t wake_detect_task::stop() {
    if (task_handle_ == nullptr) {
        ESP_LOGW(TAG, "任务未在运行");
        return ESP_ERR_INVALID_STATE;
    }

    should_stop_ = true;

    // 等待任务结束
    if (xTaskGetCurrentTaskHandle() != task_handle_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(task_handle_);
    }

    task_handle_ = nullptr;
    ESP_LOGI(TAG, "检测任务已停止");

    return ESP_OK;
}

void wake_detect_task::set_wake_callback(void (*callback)(void *user_data), void *user_data) {
    wake_callback_ = callback;
    callback_user_data_ = user_data;
}

void wake_detect_task::task_loop(void *param) {
    auto self = static_cast<wake_detect_task *>(param);
    self->run();
}

void wake_detect_task::run() {
    size_t bytes_read;
    int chunk_size = wake_net_->get_chunk_size();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始语音唤醒监听...");
    ESP_LOGI(TAG, "唤醒词: 'Hi Lexin' (你好乐鑫)");
    ESP_LOGI(TAG, "音频块大小: %d 采样点", chunk_size);
    ESP_LOGI(TAG, "========================================");

    while (!should_stop_) {
        // 读取音频数据
        esp_err_t ret = i2s_->read(
            audio_buffer_,
            chunk_size * sizeof(int16_t),
            &bytes_read,
            pdMS_TO_TICKS(100) // 100ms 超时
        );

        if (ret != ESP_OK) {
            if (ret != ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "I2S 读取警告: %s", esp_err_to_name(ret));
            }
            continue;
        }

        if (bytes_read == 0) {
            continue;
        }

        // 唤醒词检测
        int result = wake_net_->detect(audio_buffer_);

        if (result > 0) {
            on_wake_detected();
        }

        // 短暂延时，避免占用过多 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 任务结束前清理
    vTaskDelete(nullptr);
}

void wake_detect_task::on_wake_detected() {
    detect_count_++;
    ESP_LOGW(TAG, "🎉🎉🎉 检测到唤醒词！(第 %d 次) 🎉🎉🎉", detect_count_);

    // 清空 I2S 缓冲区，避免处理旧数据
    i2s_->zero_dma_buffer();

    // 调用用户回调
    if (wake_callback_ != nullptr) {
        wake_callback_(callback_user_data_);
    }

    // 短暂延迟，避免重复触发
    vTaskDelay(pdMS_TO_TICKS(500));
}

void wake_detect_task::cleanup() {
    if (audio_buffer_ != nullptr) {
        free(audio_buffer_);
        audio_buffer_ = nullptr;
    }
}
