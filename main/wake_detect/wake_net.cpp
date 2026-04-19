//
// Created by 0101 on 2026/4/12.
//

#include "include/wake_net.h"
#include "esp_log.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include <cstring>

static const char *TAG = "MY_WAKE_NET";

wake_net::wake_net()
    : wake_net_(nullptr)
      , model_data_(nullptr)
      , model_name_(nullptr)
      , audio_chunk_size_(0)
      , is_initialized_(false) {
}

wake_net::~wake_net() {
    cleanup();
}

esp_err_t wake_net::init(const char *model_partition, const char *wake_word, det_mode_t det_mode) {
    if (is_initialized_) {
        ESP_LOGW(TAG, "WakeNet 已经初始化");
        return ESP_OK;
    }

    // 从指定分区加载模型
    ESP_LOGI(TAG, "正在从 '%s' 分区加载模型...", model_partition);
    srmodel_list_t *models = esp_srmodel_init(model_partition);
    if (models == nullptr) {
        ESP_LOGE(TAG, "模型分区 '%s' 加载失败！", model_partition);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "模型分区加载成功");

    // 筛选指定的唤醒词模型
    model_name_ = esp_srmodel_filter(models, ESP_WN_PREFIX, wake_word);
    if (model_name_ == nullptr) {
        ESP_LOGE(TAG, "未找到 '%s' 唤醒词模型！", wake_word);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "找到模型: %s", model_name_);

    // 获取唤醒词句柄
    wake_net_ = esp_wn_handle_from_name(model_name_);
    if (wake_net_ == nullptr) {
        ESP_LOGE(TAG, "获取唤醒词句柄失败！");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "WakeNet 句柄获取成功");

    // 创建模型实例
    model_data_ = wake_net_->create(model_name_, det_mode);
    if (model_data_ == nullptr) {
        ESP_LOGE(TAG, "创建模型实例失败！");
        return ESP_FAIL;
    }

    // 获取音频块大小
    audio_chunk_size_ = wake_net_->get_samp_chunksize(model_data_);

    is_initialized_ = true;
    ESP_LOGI(TAG, "WakeNet 初始化成功，唤醒词: '%s'，音频块大小: %d 采样点 (约 %.1f ms @ 16kHz)",
             wake_word, audio_chunk_size_, (float)audio_chunk_size_ / 16000 * 1000);

    return ESP_OK;
}

int wake_net::detect(const int16_t *audio_data) {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "WakeNet 未初始化");
        return -1;
    }

    if (audio_data == nullptr) {
        ESP_LOGE(TAG, "音频数据为空");
        return -1;
    }

    return wake_net_->detect(model_data_, const_cast<int16_t *>(audio_data));
}

void wake_net::cleanup() {
    if (model_data_ != nullptr && wake_net_ != nullptr) {
        wake_net_->destroy(model_data_);
        model_data_ = nullptr;
    }

    // model_name_ 由 esp_srmodel_filter 分配，不需要手动释放
    model_name_ = nullptr;
    wake_net_ = nullptr;
    is_initialized_ = false;

    ESP_LOGI(TAG, "WakeNet 资源已清理");
}
