//
// Created by 0101 on 2026/4/12.
//

#include "wake_detect_task.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "WAKE_TASK";

wake_detect_task::wake_detect_task()
    : i2s_(nullptr)
      , wake_net_(nullptr)
      , audio_buffer_(nullptr)
      , task_handle_(nullptr)
      , should_stop_(false)
      , detect_count_(0)
      , wake_callback_(nullptr)
      , callback_user_data_(nullptr) {
}

wake_detect_task::~wake_detect_task() {
    stop();
    cleanup();
}

esp_err_t wake_detect_task::init(my_i2s *i2s, my_wake_net *wake_net) {
    if (i2s == nullptr || wake_net == nullptr) {
        ESP_LOGE(TAG, "无效的参数");
        return ESP_ERR_INVALID_ARG;
    }

    i2s_ = i2s;
    wake_net_ = wake_net;

    // 分配音频缓冲区
    int chunk_size = wake_net_->get_chunk_size();
    audio_buffer_ = static_cast<int16_t *>(malloc(chunk_size * sizeof(int16_t)));
    if (audio_buffer_ == nullptr) {
        ESP_LOGE(TAG, "音频缓冲区分配失败");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "初始化完成，音频块大小: %d 采样点", chunk_size);
    return ESP_OK;
}

esp_err_t wake_detect_task::start(UBaseType_t task_priority, uint32_t stack_size) {
    // 检查任务是否已经在运行 task_handle_ 不为空表示任务已经创建并正在执行
    if (task_handle_ != nullptr) {
        ESP_LOGW(TAG, "任务已经在运行");
        return ESP_OK;
    }

    // 检查必要的依赖对象是否已经初始化
    // i2s_: I2S 音频输入接口，用于读取麦克风数据
    // wake_net_: 唤醒词检测引擎，用于识别唤醒词
    // audio_buffer_: 音频数据缓冲区，用于暂存从 I2S 读取的音频采样点
    if (i2s_ == nullptr || wake_net_ == nullptr || audio_buffer_ == nullptr) {
        ESP_LOGE(TAG, "未初始化，请先调用 init()");
        return ESP_ERR_INVALID_STATE;
    }

    // 重置停止标志，确保任务循环能够正常执行
    should_stop_ = false;

    // 创建 FreeRTOS 任务
    // 参数说明：
    //   task_loop      : 任务入口函数（静态方法）
    //   "wake_detect"  : 任务名称（用于调试和监控）
    //   stack_size     : 任务栈大小（单位：字节），默认 20480
    //   this           : 传递给任务函数的参数（当前对象指针）
    //   task_priority  : 任务优先级，数值越大优先级越高，默认 5
    //   &task_handle_  : 任务句柄指针，用于后续管理任务（如删除、挂起等）
    BaseType_t ret = xTaskCreate(
        task_loop,
        "wake_detect",
        stack_size,
        this,
        task_priority,
        &task_handle_
    );

    // 检查任务是否创建成功 pdPASS 表示成功，其他值表示失败（通常是内存不足）
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "检测任务已启动");
    return ESP_OK;
}

esp_err_t wake_detect_task::stop() {
    if (task_handle_ == nullptr) {
        ESP_LOGW(TAG, "任务未在运行");
        return ESP_ERR_INVALID_STATE;
    }

    should_stop_ = true;

    // 等待任务结束
    if (xTaskGetCurrentTaskHandle() != task_handle_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(task_handle_);
    }

    task_handle_ = nullptr;
    ESP_LOGI(TAG, "检测任务已停止");

    return ESP_OK;
}

void wake_detect_task::set_wake_callback(void (*callback)(void *user_data), void *user_data) {
    wake_callback_ = callback;
    callback_user_data_ = user_data;
}

void wake_detect_task::task_loop(void *param) {
    /**
     * FreeRTOS 任务入口函数（静态桥接函数）
     *
     * 【为什么需要这个函数？】
     * FreeRTOS 是用 C 语言编写的，它不理解 C++ 的类和对象。
     * 创建任务时，FreeRTOS 只能接受：
     *   1. 普通的函数指针（不能是类的成员函数）
     *   2. 一个 void* 类型的参数
     *
     * 【为什么不能用成员函数直接作为任务函数？】
     * C++ 的成员函数在编译后实际上有一个隐藏的 this 参数：
     *   - 你写的代码：void run();
     *   - 编译器看到的：void run(wake_detect_task* this);
     * 因为函数签名不匹配，所以无法直接传递给 FreeRTOS。
     *
     * 【解决方案：静态成员函数作为桥梁】
     * 静态成员函数没有隐含的 this 指针，函数签名与普通函数相同，
     * 可以被 FreeRTOS 接受。它充当了一个"桥接器"的角色，
     * 把 FreeRTOS 的 C 风格调用转换回 C++ 的面向对象调用。
     *
     * 【param 参数的作用】
     * 在 xTaskCreate 创建任务时，我们把 this（当前对象的地址）
     * 作为 void* 参数传给了 FreeRTOS。FreeRTOS 保存这个地址，
     * 并在启动任务时原封不动地传回给这个函数。
     *
     * 【数据流示意图】
     * start() 中: xTaskCreate(..., this, ...)  → this = 0x3FCE1234
     *                  ↓
     * FreeRTOS 内核保存参数
     *                  ↓
     * task_loop(0x3FCE1234) 被调用
     *                  ↓
     * static_cast 恢复对象指针
     *                  ↓
     * self->run() 调用真正的成员函数
     *
     * @param param 对象指针，由 xTaskCreate 的 pvParameters 参数传入
     */

    // 将万能指针 void* 转换回原来的对象指针类型
    // static_cast 是 C++ 的类型转换操作符，在编译时进行类型检查，比 C 风格转换更安全
    // 这里的 param 就是之前在 xTaskCreate 中传入的 this 指针
    auto self = static_cast<wake_detect_task *>(param);

    // 调用对象的实际成员函数 run() 来执行任务逻辑
    // 此时 self 指针会被编译器作为隐含的 this 参数传递给 run()
    // 所以在 run() 函数内部可以正常访问所有成员变量（如 i2s_、wake_net_ 等）
    self->run();

    // 当 run() 函数返回时（should_stop_ 标志被设置为 true，循环退出），
    // 这个静态函数也会返回，FreeRTOS 会自动清理任务占用的栈空间等资源
}

void wake_detect_task::run() {
    /**
     * 实际的任务执行函数
     *
     * 【与 task_loop 的关系】
     * task_loop 是静态桥接函数，run 是真正干活的成员函数。
     * 这种设计模式在嵌入式 C++ 中非常常见，称为 "Static Bridge Pattern"（静态桥接模式）。
     *
     * 【this 指针的来源】
     * 在这个成员函数内部，this 指针指向的是在 task_loop 中通过 static_cast 恢复的对象。
     * 因为 this 指针是有效的，所以可以：
     *   - 访问成员变量：i2s_、wake_net_、audio_buffer_ 等
     *   - 调用其他成员函数：on_wake_detected() 等
     *
     * 【任务生命周期】
     * 1. start() 创建任务 → FreeRTOS 分配栈空间
     * 2. task_loop() 被调用 → 恢复对象指针
     * 3. run() 被调用 → 进入无限循环处理音频
     * 4. 循环中不断检测唤醒词
     * 5. stop() 设置 should_stop_ = true
     * 6. 循环退出 → run() 返回 → task_loop() 返回
     * 7. vTaskDelete() 删除任务 → FreeRTOS 回收栈空间
     */
    size_t bytes_read;
    int chunk_size = wake_net_->get_chunk_size();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始语音唤醒监听...");
    ESP_LOGI(TAG, "唤醒词: 'Hi Lexin' (你好乐鑫)");
    ESP_LOGI(TAG, "音频块大小: %d 采样点", chunk_size);
    ESP_LOGI(TAG, "========================================");

    while (!should_stop_) {
        // 读取音频数据
        esp_err_t ret = i2s_->read(
            audio_buffer_,
            chunk_size * sizeof(int16_t),
            &bytes_read,
            100 // 直接传毫秒，内部会转换为 Tick
        );

        if (ret != ESP_OK) {
            if (ret != ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "I2S 读取警告: %s", esp_err_to_name(ret));
            }
            continue;
        }

        if (bytes_read == 0) {
            continue;
        }

        // 唤醒词检测
        int result = wake_net_->detect(audio_buffer_);

        if (result > 0) {
            on_wake_detected();
        }

        // 短暂延时，避免占用过多 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 任务结束前清理
    vTaskDelete(nullptr);
}

void wake_detect_task::on_wake_detected() {
    detect_count_++;
    ESP_LOGW(TAG, "🎉🎉🎉 检测到唤醒词！(第 %d 次) 🎉🎉🎉", detect_count_);

    // 清空 I2S 缓冲区，避免处理旧数据
    i2s_->zero_dma_buffer();

    // 调用用户回调
    if (wake_callback_ != nullptr) {
        wake_callback_(callback_user_data_);
    }

    // 短暂延迟，避免重复触发
    vTaskDelay(pdMS_TO_TICKS(500));
}

void wake_detect_task::cleanup() {
    if (audio_buffer_ != nullptr) {
        free(audio_buffer_);
        audio_buffer_ = nullptr;
    }
}
