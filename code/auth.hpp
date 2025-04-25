#pragma once

#include <unordered_map>
#include <string>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <random>
#include <sstream>
#include <iomanip>
#include "mysql.hpp"
#include <sodium.h>

enum user_status
{
    SUCCESS,
    EXIST,
    WRONG,
    FAILED
};

std::string hash_password(const std::string &password)
{
    unsigned char hash[crypto_pwhash_STRBYTES];
    std::string hashed_password;
    // 调用 libsodium 的 Argon2 算法进行哈希
    if (crypto_pwhash_str(
            reinterpret_cast<char *>(hash), password.c_str(),
            password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        std::cerr << "Error: hash generation failed" << std::endl;
        return password;
    }

    hashed_password.assign(reinterpret_cast<char *>(hash), crypto_pwhash_STRBYTES);
    return hashed_password;
}
bool check_password(const std::string &password, const std::string &hashed_password)
{
    // 调用 libsodium 的验证函数来验证密码
    if (crypto_pwhash_str_verify(
            reinterpret_cast<const char *>(hashed_password.c_str()),
            password.c_str(), password.size()) != 0)
    {
        std::cerr << "Error: password verification failed" << std::endl;
        return false;
    }
    return true;
}

class auth_manager
{
public:
    static auth_manager &instance()
    {
        static auth_manager instance_;
        return instance_;
    }

    int login(const std::string &username, const std::string &password, std::string &session_id)
    {
        // 检查用户是否存在
        if (users_.find(username) == users_.end())
        {
            return user_status::FAILED;
        }
        // 比较两个哈希值
        bool ret = check_password(password, users_[username]);
        if (ret == false)
        {
            return user_status::WRONG;
        }

        // 生成会话 ID
        session_id = generate_session_id(username);
        sessions_[session_id] = username;
        return user_status::SUCCESS;
    }

    int regist(const std::string &username, const std::string &password)
    {
        // 防止用户已存在
        if (users_.find(username) != users_.end())
        {
            return user_status::EXIST;
        }

        // 生成
        std::string password_hash = hash_password(password);

        // 存储哈希密码到数据库
        if (!user_table::instance().write_user_information(username, password_hash))
        {
            return user_status::FAILED;
        }

        users_[username] = password_hash; // 更新本地用户表
        return user_status::SUCCESS;
    }

    bool validate_session(const std::string &session_id)
    {
        if (sessions_.find(session_id) != sessions_.end())
        {
            return true;
        }
    }

private:
    auth_manager()
    {
        load_user_data();
    }
    ~auth_manager() = default;
    auth_manager(const auth_manager &) = delete;
    auth_manager &operator=(const auth_manager &) = delete;

    void load_user_data()
    {
        // 从数据库加载用户信息
        user_table::instance().read_user_information(users_);
    }

    std::string generate_session_id(const std::string &username)
    {
        // 获取当前时间戳（纳秒级）
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

        // 随机字节生成器
        unsigned char random_bytes[16]; // 16字节的随机数
        if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1)
        {
            std::cerr << "Error generating random bytes for session ID" << std::endl;
            return "";
        }

        // 将随机字节转为十六进制字符串
        std::stringstream hex_stream;
        for (int i = 0; i < sizeof(random_bytes); ++i)
        {
            hex_stream << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(random_bytes[i]);
        }
        std::string random_str = hex_stream.str();

        // 生成带时间戳和用户名的 session ID
        std::string session_id = std::to_string(timestamp) + "_" + random_str + "_" + username;
        return session_id;
    }

    std::unordered_map<std::string, std::string> users_;    // 存储用户名与密码哈希
    std::unordered_map<std::string, std::string> sessions_; // 存储会话 ID 与用户名的映射
};
