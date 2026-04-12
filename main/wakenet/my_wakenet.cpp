//
// Created by 0101 on 2026/4/12.
//

#include "my_wakenet.h"
#include "esp_log.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include <cstring>

static const char *TAG = "MY_WAKENET";

my_wakenet::my_wakenet()
    : wakenet_(nullptr)
      , model_data_(nullptr)
      , model_name_(nullptr)
      , audio_chunksize_(0)
      , is_initialized_(false) {
}

my_wakenet::~my_wakenet() {
    cleanup();
}

esp_err_t my_wakenet::init(const char *model_partition, const char *wake_word, det_mode_t det_mode) {
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
    wakenet_ = esp_wn_handle_from_name(model_name_);
    if (wakenet_ == nullptr) {
        ESP_LOGE(TAG, "获取唤醒词句柄失败！");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "WakeNet 句柄获取成功");

    // 创建模型实例
    model_data_ = wakenet_->create(model_name_, det_mode);
    if (model_data_ == nullptr) {
        ESP_LOGE(TAG, "创建模型实例失败！");
        return ESP_FAIL;
    }

    // 获取音频块大小
    audio_chunksize_ = wakenet_->get_samp_chunksize(model_data_);

    is_initialized_ = true;
    ESP_LOGI(TAG, "WakeNet 初始化成功，唤醒词: '%s'，音频块大小: %d 采样点 (约 %.1f ms @ 16kHz)",
             wake_word, audio_chunksize_, (float)audio_chunksize_ / 16000 * 1000);

    return ESP_OK;
}

int my_wakenet::detect(const int16_t *audio_data) {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "WakeNet 未初始化");
        return -1;
    }

    if (audio_data == nullptr) {
        ESP_LOGE(TAG, "音频数据为空");
        return -1;
    }

    return wakenet_->detect(model_data_, const_cast<int16_t *>(audio_data));
}

esp_err_t my_wakenet::destroy() {
    if (!is_initialized_) {
        return ESP_OK;
    }

    cleanup();
    return ESP_OK;
}

void my_wakenet::cleanup() {
    if (model_data_ != nullptr && wakenet_ != nullptr) {
        wakenet_->destroy(model_data_);
        model_data_ = nullptr;
    }

    // model_name_ 由 esp_srmodel_filter 分配，不需要手动释放
    model_name_ = nullptr;
    wakenet_ = nullptr;
    is_initialized_ = false;

    ESP_LOGI(TAG, "WakeNet 资源已清理");
}
