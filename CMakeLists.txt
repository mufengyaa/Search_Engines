cmake_minimum_required(VERSION 3.10)
project(Search_Engines VERSION 2.0)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找头文件路径（可选）
include_directories(${PROJECT_SOURCE_DIR}/code)

# 查找 OpenSSL
find_package(OpenSSL REQUIRED)

# 添加 bcrypt 源文件路径
set(BCRYPT_SOURCES
    ${PROJECT_SOURCE_DIR}/code/bcrypt/crypt_blowfish.c
    ${PROJECT_SOURCE_DIR}/code/bcrypt/crypt_gensalt.c
    ${PROJECT_SOURCE_DIR}/code/bcrypt/crypt_blowfish.h
    ${PROJECT_SOURCE_DIR}/code/bcrypt/crypt_gensalt.h
    ${PROJECT_SOURCE_DIR}/code/bcrypt/crypt.h
)
# 手动指定 libsodium 的 include 和 lib 目录
include_directories(/path/to/libsodium/include)
link_directories(/path/to/libsodium/lib)

# 添加可执行文件
add_executable(parser ./code/parser.cpp)
add_executable(server ./code/server.cpp)

# 手动链接 libsodium
target_link_libraries(server
    sodium
    boost_system
    boost_filesystem
    jsoncpp
    mysqlclient
    pthread
    OpenSSL::Crypto
)


# parser 用到 MySQL
target_link_libraries(parser
    boost_system
    boost_filesystem
    mysqlclient
    pthread
)
