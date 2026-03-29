#ifndef LEDCONTROLLER_H
#define LEDCONTROLLER_H

#include "driver/gpio.h"

class LEDController {
private:
    gpio_num_t pin;
    bool isOn;
    int blinkDelay;  // 保存闪烁延时

public:
    // 构造函数
    LEDController(gpio_num_t _pin);

    // 打开LED
    void on();

    // 关闭LED
    void off();

    // 切换状态
    void toggle();

    // 获取状态
    bool getState();

    // 获取闪烁延时（供任务函数使用）
    int getBlinkDelay() const { return blinkDelay; }
    
    // 启动闪烁任务
    void startBlinking(int delayMs);
};

#endif // LEDCONTROLLER_H