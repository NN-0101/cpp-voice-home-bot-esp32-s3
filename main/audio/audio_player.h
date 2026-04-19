//
// Created by 0101 on 2026/4/19.
//
#ifndef CPP_VOICE_HOME_BOT_ESP32_S3_AUDIO_PLAYER_H
#define CPP_VOICE_HOME_BOT_ESP32_S3_AUDIO_PLAYER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "../i2s_driver/i2s_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAV 文件头结构体（标准 PCM WAV 格式）
 *
 * WAV 文件格式说明：
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 偏移量 │ 字段名           │ 大小  │ 说明                    │
 * ├─────────────────────────────────────────────────────────────┤
 * │ 0x00   │ chunk_id         │ 4字节 │ "RIFF" 标识             │
 * │ 0x04   │ chunk_size       │ 4字节 │ 文件总大小-8            │
 * │ 0x08   │ format           │ 4字节 │ "WAVE" 标识             │
 * │ 0x0C   │ subchunk1_id     │ 4字节 │ "fmt " 标识(注意有空格) │
 * │ 0x10   │ subchunk1_size   │ 4字节 │ fmt块大小(PCM为16)      │
 * │ 0x14   │ audio_format     │ 2字节 │ 音频格式(1=PCM)         │
 * │ 0x16   │ num_channels     │ 2字节 │ 声道数(1=单声道,2=立体声)│
 * │ 0x18   │ sample_rate      │ 4字节 │ 采样率(Hz)              │
 * │ 0x1C   │ byte_rate        │ 4字节 │ 字节率 = 采样率×位深×声道/8│
 * │ 0x20   │ block_align      │ 2字节 │ 数据块对齐              │
 * │ 0x22   │ bits_per_sample  │ 2字节 │ 位深(8/16/24/32)        │
 * │ 0x24   │ subchunk2_id     │ 4字节 │ "data" 标识             │
 * │ 0x28   │ subchunk2_size   │ 4字节 │ PCM数据大小(字节)       │
 * └─────────────────────────────────────────────────────────────┘
 */
#pragma pack(push, 1)  // 禁用结构体字节对齐，确保内存布局与文件格式一致
typedef struct {
    char     chunk_id[4];        // "RIFF" - 资源交换文件格式标识
    uint32_t chunk_size;         // 文件大小 - 8（从下一个字段开始计算）
    char     format[4];          // "WAVE" - 波形文件标识
    char     subchunk1_id[4];    // "fmt " - 格式子块标识（注意末尾空格）
    uint32_t subchunk1_size;     // fmt 块大小（16 for PCM）
    uint16_t audio_format;       // 音频格式：1 = PCM（脉冲编码调制）
    uint16_t num_channels;       // 声道数：1=单声道，2=立体声
    uint32_t sample_rate;        // 采样率：每秒采样次数（Hz）
    uint32_t byte_rate;          // 字节率：每秒数据量 = sample_rate * num_channels * bits_per_sample / 8
    uint16_t block_align;        // 块对齐：每次采样帧的字节数 = num_channels * bits_per_sample / 8
    uint16_t bits_per_sample;    // 位深：每个采样点的位数（8/16/24/32）
    char     subchunk2_id[4];    // "data" - 数据块标识
    uint32_t subchunk2_size;     // 数据大小：PCM音频数据的字节数
} wav_header_t;
#pragma pack(pop)  // 恢复默认对齐方式

#ifdef __cplusplus
}
#endif

/**
 * @brief 音频播放器类（单例模式，用于播放嵌入式 WAV 文件）
 *
 * 【设计模式说明】
 * 本类采用单例模式（Singleton Pattern），原因如下：
 * 1. 硬件资源唯一性：I2S 音频输出硬件只有一套，多个播放器实例会导致资源冲突
 * 2. 全局状态管理：播放状态（is_playing_）需要全局统一管理
 * 3. 资源节约：避免重复初始化 I2S 驱动和音量表
 *
 * 【使用示例】
 * @code
 * // 初始化（通常在系统启动时执行一次）
 * audio_player::get_instance().init(16000, 15, 16, 7);
 *
 * // 设置音量（0-100%）
 * audio_player::get_instance().set_volume(80);
 *
 * // 同步播放（阻塞当前任务直到播放完成）
 * audio_player::get_instance().play(wav_data, wav_size);
 *
 * // 异步播放（立即返回，后台播放）
 * audio_player::get_instance().play_async(wav_data, wav_size);
 * @endcode
 */
class audio_player {
public:
    /**
     * @brief 获取单例实例（线程安全的懒汉模式）
     *
     * C++11 标准保证静态局部变量初始化的线程安全性。
     * 首次调用时创建实例，后续调用返回同一实例的引用。
     *
     * @return 音频播放器单例引用
     */
    static audio_player& get_instance();

    // 禁用拷贝构造和赋值操作，确保单例的唯一性
    audio_player(const audio_player&) = delete;
    audio_player& operator=(const audio_player&) = delete;

    /**
     * @brief 初始化音频播放器
     *
     * 配置并启动 I2S 音频输出接口，准备播放音频。
     * 注意：多次调用不会重复初始化，会直接返回 ESP_OK。
     *
     * @param sample_rate 采样率（Hz），默认 16000（适合语音）
     * @param bclk_pin   位时钟引脚（BCLK/SCK），默认 GPIO 15
     * @param lrc_pin    左右声道时钟引脚（LRC/WS），默认 GPIO 16
     * @param din_pin    数据输出引脚（DIN/SD），默认 GPIO 7
     * @return ESP_OK 成功，其他值表示失败
     */
    [[nodiscard]] esp_err_t init(uint32_t sample_rate = 16000,
                                  int bclk_pin = 15,
                                  int lrc_pin = 16,
                                  int din_pin = 7);

    /**
     * @brief 同步播放 WAV 音频（阻塞调用）
     *
     * 在当前任务中播放音频，直到播放完成或出错才返回。
     * 适用于不需要同时处理其他任务的场景。
     *
     * 注意：播放期间当前任务会被阻塞，不建议在关键任务中调用。
     *
     * @param wav_data WAV 文件数据指针（完整文件，包含头部）
     * @param wav_size WAV 文件大小（字节）
     * @return ESP_OK 播放成功，其他值表示失败
     */
    [[nodiscard]] esp_err_t play(const uint8_t* wav_data, size_t wav_size);

    /**
     * @brief 异步播放 WAV 音频（非阻塞调用）
     *
     * 创建独立任务在后台播放音频，函数立即返回。
     * 适用于需要同时执行其他任务的场景（如播放提示音时继续检测唤醒词）。
     *
     * 注意：
     * - 同时只能有一个异步播放任务，若已有播放任务则返回错误
     * - 播放任务栈大小为 4096 字节，优先级为 5
     *
     * @param wav_data WAV 文件数据指针（完整文件，包含头部）
     * @param wav_size WAV 文件大小（字节）
     * @return ESP_OK 播放任务创建成功，其他值表示失败
     */
    [[nodiscard]] esp_err_t play_async(const uint8_t* wav_data, size_t wav_size);

    /**
     * @brief 检查是否正在播放
     * @return true 正在播放，false 空闲
     */
    [[nodiscard]] bool is_playing() const { return is_playing_; }

    /**
     * @brief 设置播放音量
     *
     * 使用平方律曲线实现人耳感知的线性音量变化。
     * 音量表在构造函数中预计算，运行时只需查表。
     *
     * @param volume 音量百分比（0-100），超出范围会被钳位
     */
    void set_volume(uint8_t volume);

    /**
     * @brief 反初始化音频播放器
     *
     * 释放 I2S 资源，停止所有播放。
     * 注意：不会释放音量表内存（析构时释放）
     *
     * @return ESP_OK 成功
     */
    esp_err_t deinit();

private:
    /**
     * @brief 私有构造函数（单例模式）
     *
     * 初始化成员变量并分配音量查询表内存。
     * 音量表大小：101 个 int16_t 值（0% 到 100%）
     */
    audio_player();

    /**
     * @brief 析构函数
     *
     * 反初始化 I2S 并释放音量表内存
     */
    ~audio_player();

    // === 硬件抽象层 ===
    i2s_driver i2s_tx_;           ///< I2S 发送驱动器（用于音频输出）

    // === 状态标志 ===
    bool is_initialized_;         ///< 是否已完成初始化
    bool is_playing_;             ///< 是否正在播放（用于互斥）

    // === 音量控制 ===
    uint8_t volume_;              ///< 当前音量（0-100%）
    int16_t* volume_table_;       ///< 音量查询表（预计算的放大系数）

    /**
     * @brief 构建音量查询表
     *
     * 使用平方律曲线：factor = (volume/100)²
     * 预计算 0%-100% 的音量系数，格式为 Q15 定点数。
     * 运行时直接查表，避免浮点运算。
     */
    void build_volume_table();

    /**
     * @brief 对音频缓冲区应用音量调节
     *
     * 将缓冲区中的 16 位采样数据乘以音量系数。
     * 使用定点运算：result = (sample * factor) >> 15
     *
     * @param buffer       音频数据缓冲区（int16_t 格式）
     * @param sample_count 采样点数量（不是字节数）
     */
    void apply_volume(int16_t* buffer, size_t sample_count);

    /**
     * @brief 解析 WAV 文件头，定位 PCM 数据
     *
     * 支持两种格式：
     * 1. 标准 WAV 格式：搜索 "data" 标识符定位数据块
     * 2. 无头格式：假设前 44 字节为标准头部（兜底策略）
     *
     * @param data       WAV 文件数据指针
     * @param size       文件总大小
     * @param header     输出：解析出的头部信息
     * @param pcm_data   输出：PCM 数据起始位置指针
     * @return ESP_OK 成功，其他值表示失败
     */
    static esp_err_t parse_wav_header(const uint8_t* data, size_t size,
                               wav_header_t* header, const uint8_t** pcm_data);

    /**
     * @brief 异步播放任务的入口函数（静态桥接函数）
     *
     * FreeRTOS 要求任务函数必须是普通函数或静态成员函数。
     * 此函数作为桥接器，将 C 风格调用转换为 C++ 成员函数调用。
     *
     * @param param 指向 play_param_t 结构的指针（包含 wav_data 和 wav_size）
     */
    static void play_task(void* param);
};

#endif // CPP_VOICE_HOME_BOT_ESP32_S3_AUDIO_PLAYER_H