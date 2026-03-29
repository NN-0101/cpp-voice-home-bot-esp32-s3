#include "LEDController.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <iostream>
using namespace std;

LEDController::LEDController(gpio_num_t _pin) : pin(_pin), isOn(false) {
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    off();
    cout << "LED Controller initialized on GPIO " << pin << endl;
}

void LEDController::on() {
    gpio_set_level(pin, 1);
    isOn = true;
    cout << "LED ON" << endl;
}

void LEDController::off() {
    gpio_set_level(pin, 0);
    isOn = false;
    cout << "LED OFF" << endl;
}

void LEDController::toggle() {
    if (isOn) {
        off();
    } else {
        on();
    }
}

bool LEDController::getState() {
    return isOn;
}

// 静态任务函数 - 使用固定延时
static void blinkTask(void* param) {
    LEDController* led = (LEDController*)param;
    while (1) {
        led->toggle();
        vTaskDelay(500 / portTICK_PERIOD_MS);  // 固定500ms延时
    }
}

void LEDController::startBlinking(int delayMs) {
    // 创建FreeRTOS任务，栈大小4096
    xTaskCreate(blinkTask, "blink_task", 4096, this, 5, NULL);
    cout << "Blinking task started with delay: " << delayMs << " ms" << endl;
}