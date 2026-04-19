// audio/audio_player.cpp
#include "audio_player.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include <cstring>
#include <cmath>

static const char* TAG = "AUDIO_PLAYER";

struct play_param_t {
    const uint8_t* wav_data;
    size_t wav_size;
};

audio_player& audio_player::get_instance() {
    static audio_player instance;
    return instance;
}

audio_player::audio_player()
    : tx_port_(I2S_NUM_1)
    , is_initialized_(false)
    , is_playing_(false)
    , volume_(70)
    , volume_table_(nullptr) {
    volume_table_ = static_cast<int16_t*>(malloc(101 * sizeof(int16_t)));
    if (volume_table_) {
        build_volume_table();
    }
}

audio_player::~audio_player() {
    deinit();
    if (volume_table_) {
        free(volume_table_);
        volume_table_ = nullptr;
    }
}

void audio_player::build_volume_table() {
    for (int i = 0; i <= 100; i++) {
        float factor = i / 100.0f;
        factor = factor * factor;
        volume_table_[i] = static_cast<int16_t>(factor * 32767);
    }
}

void audio_player::apply_volume(int16_t* buffer, size_t sample_count) {
    if (volume_ == 100) return;
    int16_t factor = volume_table_[volume_];
    for (size_t i = 0; i < sample_count; i++) {
        buffer[i] = static_cast<int16_t>((static_cast<int32_t>(buffer[i]) * factor) >> 15);
    }
}

esp_err_t audio_player::init(uint32_t sample_rate, int bclk_pin, int lrc_pin, int din_pin) {
    if (is_initialized_) {
        ESP_LOGW(TAG, "Audio player already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2S audio output (legacy driver)...");
    ESP_LOGI(TAG, "  Sample rate: %lu Hz", sample_rate);
    ESP_LOGI(TAG, "  BCLK: GPIO %d, LRC: GPIO %d, DIN: GPIO %d", bclk_pin, lrc_pin, din_pin);

    // I2S 配置（旧版驱动）
    i2s_config_t i2s_config = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
    };

    // 引脚配置
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = bclk_pin,
        .ws_io_num = lrc_pin,
        .data_out_num = din_pin,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };

    // 安装 I2S 驱动
    esp_err_t ret = i2s_driver_install(tx_port_, &i2s_config, 0, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置引脚
    ret = i2s_set_pin(tx_port_, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S pin set failed: %s", esp_err_to_name(ret));
        i2s_driver_uninstall(tx_port_);
        return ret;
    }

    is_initialized_ = true;
    ESP_LOGI(TAG, "Audio player initialized successfully");
    return ESP_OK;
}

esp_err_t audio_player::parse_wav_header(const uint8_t* data, size_t size,
                                          wav_header_t* header, const uint8_t** pcm_data) {
    if (size < 44) {
        ESP_LOGE(TAG, "File too small: %zu bytes", size);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Parsing WAV file, size: %zu bytes", size);

    // 直接搜索 "data" 字符串
    const uint8_t* ptr = data;
    const uint8_t* end = data + size - 8;
    bool found = false;

    while (ptr < end) {
        if (ptr[0] == 'd' && ptr[1] == 'a' && ptr[2] == 't' && ptr[3] == 'a') {
            uint32_t data_size = *(uint32_t*)(ptr + 4);
            *pcm_data = ptr + 8;
            header->subchunk2_size = data_size;

            ESP_LOGI(TAG, "Found 'data' at offset %d, size: %lu",
                     (int)(ptr - data), (unsigned long)data_size);
            found = true;
            break;
        }
        ptr++;
    }

    // 如果没找到 "data"，假设前 44 字节是头部
    if (!found) {
        ESP_LOGW(TAG, "No 'data' marker found, assuming 44-byte header");
        *pcm_data = data + 44;
        header->subchunk2_size = size - 44;
    }

    // 设置默认参数
    header->sample_rate = 16000;
    header->num_channels = 1;
    header->bits_per_sample = 16;
    header->audio_format = 1;

    // 边界检查
    size_t pcm_offset = *pcm_data - data;
    if (pcm_offset + header->subchunk2_size > size) {
        header->subchunk2_size = size - pcm_offset;
        ESP_LOGW(TAG, "Adjusting PCM size to %lu", (unsigned long)header->subchunk2_size);
    }

    ESP_LOGI(TAG, "PCM data: offset=%d, size=%lu",
             (int)pcm_offset, (unsigned long)header->subchunk2_size);

    return ESP_OK;
}

esp_err_t audio_player::play(const uint8_t* wav_data, size_t wav_size) {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "Audio player not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (is_playing_) {
        ESP_LOGW(TAG, "Already playing");
        return ESP_ERR_INVALID_STATE;
    }

    wav_header_t header;
    const uint8_t* pcm_data = nullptr;

    esp_err_t ret = parse_wav_header(wav_data, wav_size, &header, &pcm_data);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t pcm_offset = pcm_data - wav_data;
    if (pcm_offset + header.subchunk2_size > wav_size) {
        ESP_LOGE(TAG, "PCM data exceeds file bounds");
        return ESP_ERR_INVALID_SIZE;
    }

    const size_t chunk_samples = 1024;
    size_t buffer_size = chunk_samples * (header.bits_per_sample / 8);
    uint8_t* buffer = static_cast<uint8_t*>(malloc(buffer_size));
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }

    is_playing_ = true;
    size_t bytes_written;
    const uint8_t* data_ptr = pcm_data;
    size_t remaining = header.subchunk2_size;

    ESP_LOGI(TAG, "Playing WAV (%zu bytes PCM)...", (size_t)header.subchunk2_size);

    while (remaining > 0) {
        size_t chunk = (remaining > buffer_size) ? buffer_size : remaining;
        memcpy(buffer, data_ptr, chunk);

        if (header.bits_per_sample == 16) {
            apply_volume(reinterpret_cast<int16_t*>(buffer), chunk / 2);
        }

        ret = i2s_write(tx_port_, buffer, chunk, &bytes_written, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
            break;
        }

        data_ptr += chunk;
        remaining -= chunk;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    free(buffer);
    is_playing_ = false;

    ESP_LOGI(TAG, "Playback finished");
    return ret;
}

void audio_player::play_task(void* param) {
    play_param_t* p = static_cast<play_param_t*>(param);
    esp_err_t ret = audio_player::get_instance().play(p->wav_data, p->wav_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Async playback failed: %s", esp_err_to_name(ret));
    }
    delete p;
    vTaskDelete(nullptr);
}

esp_err_t audio_player::play_async(const uint8_t* wav_data, size_t wav_size) {
    if (!is_initialized_) {
        ESP_LOGE(TAG, "Audio player not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (is_playing_) {
        ESP_LOGW(TAG, "Already playing, ignoring request");
        return ESP_ERR_INVALID_STATE;
    }

    play_param_t* param = new play_param_t{wav_data, wav_size};
    if (!param) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ret = xTaskCreate(play_task, "audio_play", 4096, param, 5, nullptr);
    if (ret != pdPASS) {
        delete param;
        ESP_LOGE(TAG, "Failed to create play task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void audio_player::set_volume(uint8_t volume) {
    volume_ = (volume > 100) ? 100 : volume;
    ESP_LOGI(TAG, "Volume set to %d%%", volume_);
}

esp_err_t audio_player::deinit() {
    if (is_initialized_) {
        i2s_driver_uninstall(tx_port_);
        is_initialized_ = false;
        ESP_LOGI(TAG, "Audio player deinitialized");
    }
    return ESP_OK;
}