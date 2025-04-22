cmake_minimum_required(VERSION 3.10)  # 指定 CMake 的最低版本
project(Search_Engines VERSION 2.0)         # 定义项目名称和版本

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 添加可执行文件
add_executable(parser parser.cpp)
add_executable(server server.cpp)

target_link_libraries(parser boost_system boost_filesystem)
target_link_libraries(server boost_system boost_filesystem jsoncpp pthread mysqlclient)
