#pragma once

#include <unordered_map>
#include <string>
#include <openssl/sha.h>
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

std::string generateHash(const std::string &password)
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

bool validatePassword(const std::string &password, const std::string &hashed_password)
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
        session_id = generate_salt() + "_" + username;
        sessions_[session_id] = username;
        return user_status::SUCCESS;
    }

    int register_user(const std::string &username, const std::string &password)
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
        return sessions_.find(session_id) != sessions_.end();
    }

private:
    auth_manager()
    {
        load_user_data();
    }
    ~auth_manager() = default;
    auth_manager(const auth_manager &) = delete;
    auth_manager &operator=(const auth_manager &) = delete;

    std::string generate_salt()
    {
        // 随机数生成器
        std::random_device rd;
        // 0-255 表示一个char的bit数
        std::uniform_int_distribution<int> dist(0, 255);
        std::string salt;
        // 随机抽取16个bit
        for (int i = 0; i < 16; ++i)
        {
            salt += static_cast<char>(dist(rd));
        }
        return salt;
    }

    std::string hash_password(const std::string &password)
    {
        return generateHash(password); // 自动加盐 + 哈希
    }
    bool check_password(const std::string &password, const std::string &hashed)
    {
        return validatePassword(password, hashed);
    }

    void load_user_data()
    {
        // 从数据库加载用户信息
        user_table::instance().read_user_information(users_);
    }

    std::unordered_map<std::string, std::string> users_;    // 存储用户名与密码哈希
    std::unordered_map<std::string, std::string> sessions_; // 存储会话 ID 与用户名的映射
};
