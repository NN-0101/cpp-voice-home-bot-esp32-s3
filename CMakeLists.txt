# ============================================================================
# ESP-IDF 项目根 CMake 配置
# 这是每个 ESP-IDF 项目的必需入口文件
# ============================================================================

# 1. CMake 最低版本要求
cmake_minimum_required(VERSION 3.16)      # ESP-IDF v5.x 要求 CMake ≥ 3.16

# 2. 引入 ESP-IDF 构建系统
# 这行代码是 ESP-IDF 项目的核心，它加载了所有编译规则、工具链配置和组件系统
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 3. 项目声明
# 定义项目名称，这个名字会用于最终生成的固件文件名
project(cpp-voice-home-bot-esp32-s3)