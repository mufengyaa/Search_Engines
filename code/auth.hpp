#pragma once

#include <unordered_map>
#include <string>
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>
#include "mysql.hpp"

enum user_status
{
    SUCCESS,
    EXIST,
    WRONG,
    FAILED
};

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
        std::string stored_password_hash = users_[username];
        std::string salt = stored_password_hash.substr(0, 16); // 设置盐是哈希值的前 16 位
        std::string input_password_hash = hash_password(password, salt);
        if (input_password_hash != stored_password_hash)
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
        std::string salt = generate_salt();
        std::string password_hash = hash_password(password, salt);

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

    std::string hash_password(const std::string &password, const std::string &salt)
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        // 定义和初始化 SHA256 哈希算法的上下文结构体，在计算哈希值的过程中记录中间状态
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        // 添加盐值和密码到哈希过程中
        SHA256_Update(&sha256, salt.c_str(), salt.size());
        SHA256_Update(&sha256, password.c_str(), password.size());
        // 计算
        SHA256_Final(hash, &sha256);

        // 将哈希值转换为十六进制字符串的流
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return salt + ss.str(); // 加盐哈希值
    }

    void load_user_data()
    {
        // 从数据库加载用户信息
        user_table::instance().read_user_information(users_);
    }

    std::unordered_map<std::string, std::string> users_;    // 存储用户名与密码哈希
    std::unordered_map<std::string, std::string> sessions_; // 存储会话 ID 与用户名的映射
};
