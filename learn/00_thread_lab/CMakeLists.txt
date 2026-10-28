cmake_minimum_required(VERSION 3.20)
project(thread_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(thread_lab src/main.cpp)
target_include_directories(thread_lab PRIVATE include)

if(MSVC)
    # /W4        打开常用警告（数据竞争、未使用变量、类型转换都会提示）
    # /utf-8     源码和控制台都按 UTF-8 处理，中文才不会乱码
    # /permissive- 关闭 MSVC 的宽松模式，更接近标准
    target_compile_options(thread_lab PRIVATE /W4 /utf-8 /permissive- /EHsc)
endif()

find_package(Threads REQUIRED)
target_link_libraries(thread_lab PRIVATE Threads::Threads)

# 说明：
#   Release 配置下计时才有意义（Debug 版的 STL 带大量检查，慢 10 倍以上）
#   推荐命令：
#       cmake -B build -A x64
#       cmake --build build --config Release
#       build\Release\thread_lab.exe
