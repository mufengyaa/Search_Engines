#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <mysql/mysql.h>

#include "assistance.hpp"

class my_mysql
{
protected:
    static MYSQL *mysql_; // 保持全局唯一的数据库连接
    static std::mutex connect_mutex;

public:
    my_mysql()
    {
        connect();
    }

    virtual ~my_mysql() {}

protected:
    static void connect()
    {
        if (mysql_ == nullptr)
        {
            std::lock_guard<std::mutex> lock(connect_mutex); // 自动加锁，保证同一时刻只有一个线程初始化连接
            if (mysql_ == nullptr)
            {
                mysql_ = mysql_init(nullptr);
                mysql_ = mysql_real_connect(mysql_, "101.126.142.54", "mufeng", "599348181", "conn", 3306, nullptr, 0);
                if (mysql_ == nullptr)
                {
                    std::cerr << "connect failed\n";
                    exit(1);
                }
                mysql_set_character_set(mysql_, "utf8");
            }
        }
    }
};
MYSQL *my_mysql::mysql_ = nullptr;
std::mutex my_mysql::connect_mutex;

// ---------- advertising_table 单例 ----------
class advertising_table : public my_mysql
{
public:
    static advertising_table &instance()
    {
        // C++11 保证了函数内部静态变量初始化的线程安全，无需加锁
        // static 局部变量只初始化一次,只有初次调用才会创建对象,后续返回的都是同一个
        static advertising_table instance_;
        return instance_;
    }

    void read_advertising_information(std::unordered_map<std::string, float> &data)
    {
        std::string sql = "select url,fee from ad";
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)))
                {
                    std::string fee = row[1];
                    data[row[0]] = std::stof(fee);
                }
                mysql_free_result(res);
            }
            else
                std::cerr << "mysql_store_result failed\n";
        }
        else
            std::cerr << mysql_error(mysql_) << std::endl;
    }

private:
    advertising_table() = default;
    ~advertising_table() = default;
    advertising_table(const advertising_table &) = delete;
    advertising_table &operator=(const advertising_table &) = delete;
};

// ---------- user_table 单例 ----------
class user_table : public my_mysql
{
public:
    static user_table &instance()
    {
        static user_table instance_;
        return instance_;
    }

    bool write_user_information(const std::string &name, const std::string &password)
    {
        std::string sql = "insert into user(name,password) values('" + name + "','" + password + "')";
        if (mysql_query(mysql_, sql.c_str()) == 0)
            return true;
        lg(ERROR, "%s", mysql_error(mysql_));
        return false;
    }

    void read_user_information(std::unordered_map<std::string, std::string> &users)
    {
        std::string sql = "select name,password from user";
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)))
                    users[row[0]] = row[1];
                mysql_free_result(res);
            }
            else
                std::cerr << "mysql_store_result failed\n";
        }
        else
            std::cerr << mysql_error(mysql_) << std::endl;
    }

private:
    user_table() = default;
    ~user_table() = default;
    user_table(const user_table &) = delete;
    user_table &operator=(const user_table &) = delete;
};

// ---------- source_table 单例 ----------
class source_table : public my_mysql
{
public:
    static source_table &instance()
    {
        static source_table instance_;
        return instance_;
    }

    bool write_source_information(const std::string &title, const std::string &content, const std::string &url)
    {
        std::string sql = "insert into source(title, content, url) values('" +
                          escape_string(title) + "','" + escape_string(content) + "','" + escape_string(url) + "')";
        if (mysql_query(mysql_, sql.c_str()) == 0)
            return true;
        lg(ERROR, "%s", mysql_error(mysql_));
        return false;
    }

    void read_source_information(std::vector<ns_helper::doc_info> &sources)
    {
        std::string sql = "select title, content, url from source";
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)))
                {
                    ns_helper::doc_info doc;
                    doc.title_ = row[0] ? row[0] : "";
                    doc.content_ = row[1] ? row[1] : "";
                    doc.url_ = row[2] ? row[2] : "";
                    sources.emplace_back(std::move(doc));
                }
                mysql_free_result(res);
            }
            else
                std::cerr << "mysql_store_result failed\n";
        }
        else
            std::cerr << mysql_error(mysql_) << std::endl;
    }

private:
    source_table() = default;
    ~source_table() = default;
    source_table(const source_table &) = delete;
    source_table &operator=(const source_table &) = delete;

    std::string escape_string(const std::string &input)
    {
        char *buffer = new char[input.size() * 2 + 1];
        mysql_real_escape_string(mysql_, buffer, input.c_str(), input.size());
        std::string result(buffer);
        delete[] buffer;
        return result;
    }
};

// ---------- index_table 单例 ----------
class index_table : public my_mysql
{
public:
    static index_table &instance()
    {
        static index_table instance_;
        return instance_;
    }

    // 从 MySQL 加载正排索引数据
    void load_positive(std::vector<std::tuple<int, std::string>> &index)
    {
        std::string sql = "SELECT doc_id, content FROM forward_index_table"; // 假设表名为 forward_index_table
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)))
                {
                    int doc_id = std::stoi(row[0]);
                    std::string content = row[1] ? row[1] : "";
                    index.push_back(std::make_tuple(doc_id, content));
                }
                mysql_free_result(res);
            }
            else
            {
                std::cerr << "mysql_store_result failed\n";
            }
        }
        else
        {
            std::cerr << mysql_error(mysql_) << std::endl;
        }
    }

    // 保存正排索引数据到 MySQL
    bool save_forward_index_to_mysql(const std::vector<std::tuple<int, std::string>> &index)
    {
        for (const auto &item : index)
        {
            int doc_id = std::get<0>(item);
            const std::string &content = std::get<1>(item);

            std::string sql = "INSERT INTO forward_index_table (doc_id, content) VALUES (" +
                              std::to_string(doc_id) + ",'" + escape_string(content) + "')";

            if (mysql_query(mysql_, sql.c_str()) != 0)
            {
                std::cerr << "Failed to insert forward index: " << mysql_error(mysql_) << std::endl;
                return false;
            }
        }
        return true;
    }

        // 从 MySQL 加载倒排索引数据
    void load_inverted_index_from_mysql(std::unordered_map<std::string, std::vector<int>> &index)
    {
        std::string sql = "SELECT word, doc_id FROM inverted_index_table"; // 假设表名为 inverted_index_table
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)))
                {
                    std::string word = row[0] ? row[0] : "";
                    int doc_id = std::stoi(row[1]);
                    index[word].push_back(doc_id);
                }
                mysql_free_result(res);
            }
            else
            {
                std::cerr << "mysql_store_result failed\n";
            }
        }
        else
        {
            std::cerr << mysql_error(mysql_) << std::endl;
        }
    }

    // 保存倒排索引数据到 MySQL
    bool save_inverted_index_to_mysql(const std::unordered_map<std::string, std::vector<int>> &index)
    {
        for (const auto &entry : index)
        {
            const std::string &word = entry.first;
            const std::vector<int> &doc_ids = entry.second;

            for (int doc_id : doc_ids)
            {
                std::string sql = "INSERT INTO inverted_index_table (word, doc_id) VALUES ('" +
                                  escape_string(word) + "', " + std::to_string(doc_id) + ")";

                if (mysql_query(mysql_, sql.c_str()) != 0)
                {
                    std::cerr << "Failed to insert inverted index: " << mysql_error(mysql_) << std::endl;
                    return false;
                }
            }
        }
        return true;
    }

private:
    index_table() = default;
    ~index_table() = default;
    index_table(const index_table &) = delete;
    index_table &operator=(const index_table &) = delete;

    // 转义字符串，防止 SQL 注入
    std::string escape_string(const std::string &input)
    {
        char *buffer = new char[input.size() * 2 + 1];
        mysql_real_escape_string(mysql_, buffer, input.c_str(), input.size());
        std::string result(buffer);
        delete[] buffer;
        return result;
    }
};