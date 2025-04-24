cmake_minimum_required(VERSION 3.10)
project(Search_Engines VERSION 2.0)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找头文件路径（可选）
include_directories(${PROJECT_SOURCE_DIR}/code)

# 查找 OpenSSL
find_package(OpenSSL REQUIRED)

# 添加可执行文件
add_executable(parser ./code/parser.cpp)
add_executable(server ./code/server.cpp)

# parser 用到 MySQL
target_link_libraries(parser
    boost_system
    boost_filesystem
    mysqlclient
    pthread
)

# server 用到 jsoncpp + mysql + openssl
target_link_libraries(server
    boost_system
    boost_filesystem
    jsoncpp
    mysqlclient
    pthread
    OpenSSL::Crypto
)
