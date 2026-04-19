//
// Created by 0101 on 2026/4/19.
//

// audio/audio_player.cpp
#include "include/audio_player.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include <cstring>
#include <cmath>

static const char* TAG = "AUDIO_PLAYER";  ///< 日志标签

/**
 * @brief 异步播放任务参数结构体
 *
 * 用于在创建播放任务时传递 WAV 数据信息。
 * 任务执行完成后会自动释放此结构体内存。
 */
struct play_param_t {
    const uint8_t* wav_data;  ///< WAV 文件数据指针（只读）
    size_t wav_size;          ///< WAV 文件大小（字节）
};

// ============================================================================
// 单例模式实现
// ============================================================================

audio_player& audio_player::get_instance() {
    static audio_player instance;  // C++11 保证线程安全的懒汉单例
    return instance;
}

// ============================================================================
// 构造与析构
// ============================================================================

audio_player::audio_player()
    : i2s_tx_(I2S_NUM_1)           // 使用 I2S1 作为音频输出（I2S0 用于麦克风输入）
    , is_initialized_(false)        // 初始未初始化
    , is_playing_(false)            // 初始未播放
    , volume_(70)                   // 默认音量 70%
    , volume_table_(nullptr) {      // 音量表待分配
    // 分配音量查询表内存（101 个条目：0% 到 100%）
    volume_table_ = static_cast<int16_t*>(malloc(101 * sizeof(int16_t)));
    if (volume_table_) {
        build_volume_table();       // 预计算音量系数
    } else {
        ESP_LOGE(TAG, "音量表内存分配失败！");
    }
}

audio_player::~audio_player() {
    deinit();                       // 释放 I2S 资源
    if (volume_table_) {
        free(volume_table_);        // 释放音量表内存
        volume_table_ = nullptr;
    }
}

// ============================================================================
// 音量控制（私有方法）
// ============================================================================

void audio_player::build_volume_table() {
    /**
     * 构建音量查询表（平方律曲线）
     *
     * 【为什么使用平方律曲线？】
     * 人耳对音量的感知是对数关系，而非线性关系。
     * 使用平方律曲线（factor = (volume/100)²）可以让人耳感受到的音量变化更加线性。
     *
     * 【Q15 定点格式说明】
     * 系数以 Q15 格式存储：实际值 = 存储值 / 32768
     * 例如：100% 音量 → factor = 1.0 → 存储值 = 32767
     *       50% 音量 → factor = 0.25 → 存储值 = 8192
     *
     * 【运行时使用】
     * apply_volume() 中使用定点乘法：
     * result = (sample * table[volume]) >> 15
     * 等价于 result = sample * factor
     */
    for (int i = 0; i <= 100; i++) {
        float factor = static_cast<float>(i) / 100.0f;  // 0.0 ~ 1.0
        factor = factor * factor;                        // 平方律
        volume_table_[i] = static_cast<int16_t>(factor * 32767.0f);  // 转为 Q15 格式
    }
}

void audio_player::apply_volume(int16_t* buffer, size_t sample_count) {
    // 100% 音量时无需处理，直接返回
    if (volume_ == 100) return;

    int16_t factor = volume_table_[volume_];  // 查表获取系数

    /**
     * 定点乘法实现音量调节
     *
     * 公式：output = (input × factor) >> 15
     *
     * 为什么用 int32_t 中间变量？
     * - int16_t × int16_t 可能溢出（最大 32767 × 32767 ≈ 1e9 > 65535）
     * - 先用 int32_t 保存乘积，再右移 15 位，最后截断回 int16_t
     */
    for (size_t i = 0; i < sample_count; i++) {
        buffer[i] = static_cast<int16_t>((static_cast<int32_t>(buffer[i]) * factor) >> 15);
    }
}

// ============================================================================
// 初始化和反初始化
// ============================================================================

esp_err_t audio_player::init(uint32_t sample_rate, int bclk_pin, int lrc_pin, int din_pin) {
    // 防止重复初始化
    if (is_initialized_) {
        ESP_LOGW(TAG, "音频播放器已经初始化，跳过重复初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "正在初始化 I2S 音频输出...");
    ESP_LOGI(TAG, "  采样率: %lu Hz", sample_rate);
    ESP_LOGI(TAG, "  位时钟引脚(BCLK): GPIO %d", bclk_pin);
    ESP_LOGI(TAG, "  左右时钟引脚(LRC): GPIO %d", lrc_pin);
    ESP_LOGI(TAG, "  数据输出引脚(DIN): GPIO %d", din_pin);

    /**
     * 初始化 I2S 为 TX_ONLY 模式（仅发送）
     *
     * 参数顺序说明：
     * - Direction::TX_ONLY : 发送模式（扬声器）
     * - sample_rate        : 采样率
     * - lrc_pin            : WS 引脚（字选/左右声道时钟）
     * - bclk_pin           : SCK 引脚（位时钟）
     * - din_pin            : SD 引脚（数据线）
     */
    esp_err_t ret = i2s_tx_.init(i2s_driver::Direction::TX_ONLY,
                                  sample_rate,
                                  lrc_pin,    // ws_pin
                                  bclk_pin,   // sck_pin
                                  din_pin);   // sd_pin
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 发送模式初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    is_initialized_ = true;
    ESP_LOGI(TAG, "音频播放器初始化成功");
    return ESP_OK;
}

esp_err_t audio_player::deinit() {
    if (is_initialized_) {
        i2s_tx_.destroy();          // 卸载 I2S 驱动
        is_initialized_ = false;
        ESP_LOGI(TAG, "音频播放器已反初始化");
    }
    return ESP_OK;
}

// ============================================================================
// WAV 文件解析（静态方法）
// ============================================================================

esp_err_t audio_player::parse_wav_header(const uint8_t* data, size_t size,
                                          wav_header_t* header, const uint8_t** pcm_data) {
    // WAV 文件最小大小：44 字节（标准头部）
    if (size < 44) {
        ESP_LOGE(TAG, "文件太小（%zu 字节），无法解析 WAV 头部", size);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "正在解析 WAV 文件，大小: %zu 字节", size);

    /**
     * 搜索 "data" 标识符定位 PCM 数据块
     *
     * 为什么要搜索而不是直接读取固定偏移？
     * - 某些 WAV 文件在 fmt 块和 data 块之间可能包含额外信息块（如 fact、list 等）
     * - 搜索 "data" 可以适应各种变体格式
     */
    const uint8_t* ptr = data;
    const uint8_t* end = data + size - 8;  // 至少需要 4 字节标识 + 4 字节大小
    bool found = false;

    while (ptr < end) {
        if (ptr[0] == 'd' && ptr[1] == 'a' && ptr[2] == 't' && ptr[3] == 'a') {
            uint32_t data_size = *(uint32_t*)(ptr + 4);  // 读取数据块大小
            *pcm_data = ptr + 8;                          // PCM 数据从标识后第 8 字节开始
            header->subchunk2_size = data_size;

            ESP_LOGI(TAG, "在偏移量 %d 处找到 'data' 标识，数据大小: %lu 字节",
                     (int)(ptr - data), (unsigned long)data_size);
            found = true;
            break;
        }
        ptr++;
    }

    // 兜底策略：如果没找到 "data"，假设文件是标准的 44 字节头部
    if (!found) {
        ESP_LOGW(TAG, "未找到 'data' 标识，假定为标准 44 字节头部");
        *pcm_data = data + 44;
        header->subchunk2_size = size - 44;
    }

    // 设置默认音频参数（本项目使用固定的 16kHz 单声道 16bit 格式）
    header->sample_rate = 16000;
    header->num_channels = 1;
    header->bits_per_sample = 16;
    header->audio_format = 1;  // PCM 格式

    // 边界检查：确保 PCM 数据不超出文件范围
    size_t pcm_offset = *pcm_data - data;
    if (pcm_offset + header->subchunk2_size > size) {
        header->subchunk2_size = size - pcm_offset;
        ESP_LOGW(TAG, "调整 PCM 数据大小为 %lu 字节", (unsigned long)header->subchunk2_size);
    }

    ESP_LOGI(TAG, "PCM 数据位置: 偏移量=%d 字节, 大小=%lu 字节",
             (int)pcm_offset, (unsigned long)header->subchunk2_size);

    return ESP_OK;
}

// ============================================================================
// 同步播放
// ============================================================================

esp_err_t audio_player::play(const uint8_t* wav_data, size_t wav_size) {
    // 状态检查
    if (!is_initialized_) {
        ESP_LOGE(TAG, "音频播放器未初始化，无法播放");
        return ESP_ERR_INVALID_STATE;
    }

    if (is_playing_) {
        ESP_LOGW(TAG, "正在播放中，请等待当前播放完成");
        return ESP_ERR_INVALID_STATE;
    }

    // 解析 WAV 文件头
    wav_header_t header;
    const uint8_t* pcm_data = nullptr;

    esp_err_t ret = parse_wav_header(wav_data, wav_size, &header, &pcm_data);
    if (ret != ESP_OK) {
        return ret;
    }

    // 验证 PCM 数据边界
    size_t pcm_offset = pcm_data - wav_data;
    if (pcm_offset + header.subchunk2_size > wav_size) {
        ESP_LOGE(TAG, "PCM 数据超出文件边界");
        return ESP_ERR_INVALID_SIZE;
    }

    /**
     * 分配播放缓冲区
     *
     * 缓冲区大小计算：
     * - 每次发送 1024 个采样点
     * - 每个采样点占 bits_per_sample/8 字节
     * - 16 位音频：1024 × 2 = 2048 字节
     */
    const size_t chunk_samples = 1024;
    size_t buffer_size = chunk_samples * (header.bits_per_sample / 8);
    auto* buffer = static_cast<uint8_t*>(malloc(buffer_size));
    if (!buffer) {
        ESP_LOGE(TAG, "分配播放缓冲区失败（需要 %zu 字节）", buffer_size);
        return ESP_ERR_NO_MEM;
    }

    is_playing_ = true;
    size_t bytes_written;
    const uint8_t* data_ptr = pcm_data;
    size_t remaining = header.subchunk2_size;

    ESP_LOGI(TAG, "开始播放 WAV 音频（PCM 数据: %zu 字节）...", (size_t)header.subchunk2_size);

    /**
     * 分块发送音频数据到 I2S
     *
     * 为什么分块发送？
     * - I2S DMA 缓冲区有限，一次性发送大量数据可能阻塞
     * - 分块发送可以在每个块之间应用音量调节
     * - 便于控制播放进度和错误处理
     */
    while (remaining > 0) {
        size_t chunk = (remaining > buffer_size) ? buffer_size : remaining;
        memcpy(buffer, data_ptr, chunk);

        // 对 16 位音频应用音量调节
        if (header.bits_per_sample == 16) {
            apply_volume(reinterpret_cast<int16_t*>(buffer), chunk / 2);
        }

        // 写入 I2S，等待直到 DMA 有空闲空间
        ret = i2s_tx_.write(buffer, chunk, &bytes_written, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S 写入失败: %s", esp_err_to_name(ret));
            break;
        }

        data_ptr += chunk;
        remaining -= chunk;
    }

    // 播放完成后短暂延时，确保最后的数据被完整发送
    vTaskDelay(pdMS_TO_TICKS(50));

    free(buffer);
    is_playing_ = false;

    ESP_LOGI(TAG, "播放完成");
    return ret;
}

// ============================================================================
// 异步播放
// ============================================================================

void audio_player::play_task(void* param) {
    /**
     * 异步播放任务入口函数
     *
     * 这是一个静态桥接函数，用于将 FreeRTOS 任务连接到 C++ 成员函数。
     *
     * 执行流程：
     * 1. 从 param 恢复 play_param_t 结构
     * 2. 调用单例的同步播放方法
     * 3. 释放参数内存
     * 4. 删除自身任务
     *
     * @param param play_param_t 结构指针
     */
    auto* p = static_cast<play_param_t*>(param);

    esp_err_t ret = get_instance().play(p->wav_data, p->wav_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "异步播放失败: %s", esp_err_to_name(ret));
    }

    delete p;                   // 释放参数内存
    vTaskDelete(nullptr);       // 删除当前任务
}

esp_err_t audio_player::play_async(const uint8_t* wav_data, size_t wav_size) { // NOLINT
    // 状态检查
    if (!is_initialized_) {
        ESP_LOGE(TAG, "音频播放器未初始化，无法异步播放");
        return ESP_ERR_INVALID_STATE;
    }

    if (is_playing_) {
        ESP_LOGW(TAG, "正在播放中，忽略本次请求");
        return ESP_ERR_INVALID_STATE;
    }

    // 创建参数结构（任务完成后会自动释放）
    auto* param = new play_param_t{wav_data, wav_size};

    /**
     * 创建播放任务
     *
     * 任务参数说明：
     * - play_task      : 任务入口函数
     * - "audio_play"   : 任务名称（用于调试）
     * - 4096           : 任务栈大小（字节）
     * - param          : 传递给任务的参数
     * - 5              : 任务优先级（数值越大优先级越高）
     * - nullptr        : 任务句柄（不需要保存）
     */
    BaseType_t ret = xTaskCreate(play_task, "audio_play", 4096, param, 5, nullptr);
    if (ret != pdPASS) {
        delete param;
        ESP_LOGE(TAG, "创建播放任务失败（内存不足？）");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "异步播放任务已创建");
    return ESP_OK;
}

// ============================================================================
// 音量设置
// ============================================================================

void audio_player::set_volume(uint8_t volume) {
    // 钳位到 0-100 范围
    volume_ = (volume > 100) ? 100 : volume;
    ESP_LOGI(TAG, "音量设置为 %d%%", volume_);
}