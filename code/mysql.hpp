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
                          ns_helper::escape_string(title) + "','" + ns_helper::escape_string(content) + "','" + ns_helper::escape_string(url) + "')";
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

    // 正排索引
    void load_positive(std::vector<ns_helper::docInfo_index> &index)
    {
        std::string sql = "SELECT doc_id, title, content, url FROM forward_index_table";
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)))
                {
                    ns_helper::docInfo_index doc;
                    doc.doc_id_ = std::stoi(row[0]);
                    doc.title_ = row[1] ? row[1] : "";
                    doc.content_ = row[2] ? row[2] : "";
                    doc.url_ = row[3] ? row[3] : "";
                    index.emplace_back(std::move(doc));
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
    bool save_positive(const std::vector<ns_helper::docInfo_index> &index)
    {
        for (const auto &item : index)
        {
            int doc_id = std::get<0>(item);
            const std::string &title = std::get<1>(item);
            const std::string &content = std::get<2>(item);
            const std::string &url = std::get<3>(item);

            std::string escaped_title = ns_helper::escape_string(mysql_, title);
            std::string escaped_content = ns_helper::escape_string(mysql_, content);
            std::string escaped_url = ns_helper::escape_string(mysql_, url);

            std::string sql = "INSERT INTO forward_index_table (doc_id, title, content, url) VALUES (" +
                              std::to_string(doc_id) + ", '" + escaped_title + "', '" +
                              escaped_content + "', '" + escaped_url + "')";

            if (mysql_query(mysql_, sql.c_str()) != 0)
            {
                std::cerr << "Failed to insert forward index: " << mysql_error(mysql_) << std::endl;
                return false;
            }
        }
        return true;
    }

    // 倒排索引
    void load_inverted(std::unordered_map<std::string, ns_helper::inverted_zipper> &index)
    {
        std::string sql = "SELECT word, doc_id, weight, url FROM inverted_index_table";
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
                    int weight = std::stoi(row[2]);
                    std::string url = row[3] ? row[3] : "";

                    ns_helper::word_info info{word, doc_id, weight, url};
                    index[word].push_back(std::move(info));
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
    bool save_inverted(const std::unordered_map<std::string, ns_helper::inverted_zipper> &index)
    {
        for (const auto &entry : index)
        {
            const std::string &word = entry.first;
            const auto &infos = entry.second;

            for (const auto &info : infos)
            {
                std::string escaped_word = ns_helper::escape_string(mysql_, word);
                std::string escaped_url = ns_helper::escape_string(mysql_, info.url_);
                std::string sql = "INSERT INTO inverted_index_table (word, doc_id, weight, url) VALUES ('" +
                                  escaped_word + "', " + std::to_string(info.doc_id_) + ", " +
                                  std::to_string(info.weight_) + ", '" + escaped_url + "')";

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

    bool has_forward_index_data(const std::string &table)
    {
        std::string sql = "SELECT COUNT(*) FROM" + table;
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0])
                {
                    int count = std::stoi(row[0]);
                    mysql_free_result(res);
                    return count > 0; // 有数据就返回 true
                }
                mysql_free_result(res);
            }
        }
        else
        {
            std::cerr << "MySQL query failed: " << mysql_error(mysql_) << std::endl;
        }
        return false;
    }
};