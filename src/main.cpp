#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "I2SMicrophone.h"
#include "esp_log.h"

using namespace std;

// 根据你的引脚配置
#define I2S_BCK_PIN  12   // SCK
#define I2S_WS_PIN   13   // WS
#define I2S_DIN_PIN  14   // SD

extern "C" void app_main() {
    cout << "=================================" << endl;
    cout << "I2S Microphone Test" << endl;
    cout << "=================================" << endl;

    // 关闭默认日志输出，避免干扰
    esp_log_level_set("*", ESP_LOG_WARN);

    I2SMicrophone mic(I2S_BCK_PIN, I2S_WS_PIN, I2S_DIN_PIN, 16000);
    if (!mic.init()) {
        cout << "Mic init failed!" << endl;
        return;
    }

    cout << "Mic initialized successfully!" << endl;

    const int SAMPLES = 512;
    int16_t* buffer = new int16_t[SAMPLES];

    int silent_count = 0;

    while (1) {
        int samples_read = mic.read(buffer, SAMPLES);
        if (samples_read > 0) {
            // 计算最大幅度
            int16_t max_val = 0;
            int32_t sum = 0;
            for (int i = 0; i < samples_read; i++) {
                int16_t abs_val = abs(buffer[i]);
                if (abs_val > max_val) max_val = abs_val;
                sum += abs_val;
            }
            int16_t avg_val = sum / samples_read;

            // 检测声音
            if (max_val > 1000) {
                cout << "LOUD! max=" << max_val << " avg=" << avg_val << endl;
                silent_count = 0;
            } else if (max_val > 300) {
                cout << "Sound: max=" << max_val << " avg=" << avg_val << endl;
                silent_count = 0;
            } else {
                silent_count++;
                if (silent_count % 100 == 0) {
                    cout << "Silent... (max=" << max_val << ")" << endl;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    delete[] buffer;
    mic.deinit();
}