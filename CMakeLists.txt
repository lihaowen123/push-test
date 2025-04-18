cmake_minimum_required(VERSION 3.10)
project(HelloWorld)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)

# 生成可执行文件
add_executable(hello_world main.cpp)
