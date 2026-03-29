#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "LEDController.h"

using namespace std;

// 定义LED引脚
#define LED_GPIO_PIN  GPIO_NUM_2

// 主函数 - 保持简洁
extern "C" void app_main() {
    cout << "=================================" << endl;
    cout << "ESP32-S3 C++ LED Test Starting..." << endl;
    cout << "=================================" << endl;

    // 创建LED控制器对象
    LEDController led(LED_GPIO_PIN);

    // 启动闪烁（参数：延时毫秒）
    led.startBlinking(500);

    cout << "Main task running, LED blinking in background..." << endl;

    // 主循环可以空闲或做其他事情
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // 可以在这里添加其他功能
        // cout << "Main loop heartbeat..." << endl;  // 可选的心跳打印
    }
}