// audio/audio_player.h
#ifndef CPP_VOICE_HOME_BOT_ESP32_S3_AUDIO_PLAYER_H
#define CPP_VOICE_HOME_BOT_ESP32_S3_AUDIO_PLAYER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "i2s/my_i2s.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAV 文件头结构体（标准 PCM WAV 格式）
 */
#pragma pack(push, 1)
typedef struct {
    char     chunk_id[4];        // "RIFF"
    uint32_t chunk_size;         // 文件大小 - 8
    char     format[4];          // "WAVE"
    char     subchunk1_id[4];    // "fmt "
    uint32_t subchunk1_size;     // fmt 块大小（16 for PCM）
    uint16_t audio_format;       // 1 = PCM
    uint16_t num_channels;       // 声道数
    uint32_t sample_rate;        // 采样率
    uint32_t byte_rate;          // 字节率
    uint16_t block_align;        // 块对齐
    uint16_t bits_per_sample;    // 位深（8/16/24/32）
    char     subchunk2_id[4];    // "data"
    uint32_t subchunk2_size;     // 数据大小
} wav_header_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

/**
 * @brief 音频播放器类（单例模式，用于播放嵌入式 WAV 文件）
 */
class audio_player {
public:
    static audio_player& get_instance();

    audio_player(const audio_player&) = delete;
    audio_player& operator=(const audio_player&) = delete;

    [[nodiscard]] esp_err_t init(uint32_t sample_rate = 16000,
                                  int bclk_pin = 15,
                                  int lrc_pin = 16,
                                  int din_pin = 7);

    [[nodiscard]] esp_err_t play(const uint8_t* wav_data, size_t wav_size);
    [[nodiscard]] esp_err_t play_async(const uint8_t* wav_data, size_t wav_size);

    [[nodiscard]] bool is_playing() const { return is_playing_; }
    void set_volume(uint8_t volume);
    esp_err_t deinit();

private:
    audio_player();
    ~audio_player();

    my_i2s i2s_tx_;
    bool is_initialized_;
    bool is_playing_;
    uint8_t volume_;
    int16_t* volume_table_;

    void build_volume_table();
    void apply_volume(int16_t* buffer, size_t sample_count);
    esp_err_t parse_wav_header(const uint8_t* data, size_t size,
                               wav_header_t* header, const uint8_t** pcm_data);
    static void play_task(void* param);
};

#endif // CPP_VOICE_HOME_BOT_ESP32_S3_AUDIO_PLAYER_H