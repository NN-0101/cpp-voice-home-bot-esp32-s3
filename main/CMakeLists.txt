# ============================================================================
# 组件注册 - 定义 main 组件的编译规则
# ============================================================================
#
# idf_component_register 是 ESP-IDF 构建系统的核心宏，用于向构建系统描述一个组件的：
#   - 有哪些源文件需要编译
#   - 头文件从哪里查找
#   - 依赖哪些其他组件
#
# 参数详解：
# ============================================================================
#
# 📌 SRCS (Sources)
#    - 作用：指定需要编译的源文件列表
#    - 类型：字符串列表，多个文件用空格分隔
#    - 示例：SRCS "main.cpp" "utils.cpp" "audio.cpp"
#    - 说明：CMake 会自动根据文件扩展名选择编译器 (.c → gcc, .cpp → g++)
#
# 📌 INCLUDE_DIRS (Include Directories)
#    - 作用：指定头文件的搜索路径
#    - 类型：字符串列表，多个路径用空格分隔
#    - 示例：INCLUDE_DIRS "." "include" "../common/include"
#    - 说明：这些路径会被添加到编译器的 -I 参数中，用于解析 #include 指令
#
# 📌 REQUIRES (Public Requirements)
#    - 作用：声明本组件的公开依赖（其他依赖本组件的组件也会继承这些依赖）
#    - 类型：字符串列表，多个组件名用空格分隔
#    - 示例：REQUIRES esp-sr esp-timer
#    - 说明：本组件和依赖本组件的其他组件都能访问 REQUIRES 中声明的组件
#
# 📌 PRIV_REQUIRES (Private Requirements)
#    - 作用：声明本组件的私有依赖（仅本组件内部使用，不对外暴露）
#    - 类型：字符串列表，多个组件名用空格分隔
#    - 示例：PRIV_REQUIRES driver freertos
#    - 说明：这些依赖仅在编译本组件时可用，不会传递给依赖本组件的其他组件
#
# 📌 LDFRAGMENTS (Linker Fragment Files)
#    - 作用：指定链接脚本片段文件，用于控制代码/数据的段放置
#    - 类型：字符串列表
#    - 示例：LDFRAGMENTS "linker.lf"
#
# 📌 EMBED_FILES / EMBED_TXTFILES
#    - 作用：将文件嵌入到固件中，运行时可通过文件系统 API 读取
#    - 类型：字符串列表
#    - 示例：EMBED_FILES "config.json" "logo.bin"
#    - 说明：常用于嵌入网页、配置文件、模型数据等只读资源
#
# 📌 REQUIRED_IDF_TARGETS
#    - 作用：限制本组件仅支持特定的芯片型号
#    - 类型：字符串列表
#    - 示例：REQUIRED_IDF_TARGETS esp32s3 esp32
#    - 说明：若用户使用不支持的芯片，构建时会报错
#
# 📌 WHOLE_ARCHIVE
#    - 作用：强制链接静态库中的所有符号（即使未被直接引用）
#    - 类型：布尔值 (TRUE/FALSE)
#    - 示例：WHOLE_ARCHIVE TRUE
#    - 说明：适用于包含自注册机制的库，防止符号被优化丢弃
#
# ============================================================================

idf_component_register(
        SRCS
            "main.cpp"                    # 编译主程序源文件
            "i2s_driver/i2s_driver.cpp"
            "wake_detect/wake_net.cpp"
            "wake_detect/wake_detect_task.cpp"
            "audio/audio_player.cpp"
        INCLUDE_DIRS "."                   # 当前目录加入头文件搜索路径
        EMBED_TXTFILES
            "assets/hello1.wav"
        # REQUIRES esp-sr                  # (可选) 公开依赖 ESP-SR 语音识别库
        # PRIV_REQUIRES driver freertos    # (可选) 私有依赖 I2S 驱动和 FreeRTOS
)