#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <future>

#include "assistance.hpp"
#include "FixedThreadPool.hpp"

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
                mysql_set_character_set(mysql_, "utf8mb4");
                std::cerr << "mysql connect success\n";
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
        std::string sql = "INSERT INTO user(name, password) VALUES(?, ?)";

        // 预处理 SQL 语句
        MYSQL_STMT *stmt = mysql_stmt_init(mysql_);
        if (!stmt)
        {
            std::cerr << "mysql_stmt_init failed" << std::endl;
            return false;
        }

        // 准备 SQL 语句
        if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0)
        {
            std::cerr << "mysql_stmt_prepare failed: " << mysql_error(mysql_) << std::endl;
            mysql_stmt_close(stmt);
            return false;
        }

        // 绑定参数
        MYSQL_BIND bind[2];
        memset(bind, 0, sizeof(bind));

        // 设置第一个参数（name）
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (char *)name.c_str();
        bind[0].buffer_length = name.length();

        // 设置第二个参数（password）
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (char *)password.c_str();
        bind[1].buffer_length = password.length();

        if (mysql_stmt_bind_param(stmt, bind) != 0)
        {
            std::cerr << "mysql_stmt_bind_param failed: " << mysql_error(mysql_) << std::endl;
            mysql_stmt_close(stmt);
            return false;
        }

        // 执行查询
        if (mysql_stmt_execute(stmt) != 0)
        {
            std::cerr << "mysql_stmt_execute failed: " << mysql_error(mysql_) << std::endl;
            mysql_stmt_close(stmt);
            return false;
        }

        // 关闭语句
        mysql_stmt_close(stmt);
        return true;
    }

    void read_user_information(std::unordered_map<std::string, std::string> &users)
    {
        std::string sql = "SELECT name, password FROM user";

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
                          ns_helper::escape_string(mysql_, title) + "','" + ns_helper::escape_string(mysql_, content) + "','" + ns_helper::escape_string(mysql_, url) + "')";
        if (mysql_query(mysql_, sql.c_str()) == 0)
            return true;
        Log::getInstance()(ERROR, "%s", mysql_error(mysql_));
        return false;
    }

    void read_source_information(std::vector<ns_helper::docInfo_index> &index)
    {
        std::string sql = "select title, content, url,id from source";
        if (mysql_query(mysql_, sql.c_str()) == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)))
                {
                    ns_helper::docInfo_index doc;
                    doc.title_ = row[0] ? row[0] : "";
                    doc.content_ = row[1] ? row[1] : "";
                    doc.url_ = row[2] ? row[2] : "";
                    doc.doc_id_ = std::stoi(row[3]);
                    index.emplace_back(std::move(doc));
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

    bool has_forward_index_data(const std::string &table)
    {
        std::string sql = "SELECT COUNT(*) FROM " + table;
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
            int doc_id = item.doc_id_;
            const std::string &title = item.title_;
            const std::string &content = item.content_;
            const std::string &url = item.url_;

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
        const int MAX_BATCH = 100; // 每100条一批

        size_t num_threads = std::min(index.size(), (size_t)std::thread::hardware_concurrency());
        size_t index_per_thread = (index.size() + num_threads - 1) / num_threads;

        std::vector<std::future<void>> tasks;
        auto it = index.begin();

        for (size_t i = 0; i < num_threads && it != index.end(); ++i)
        {
            auto start_it = it;
            size_t count = 0;
            while (it != index.end() && count < index_per_thread)
            {
                ++it;
                ++count;
            }
            auto end_it = it;

            auto task = FixedThreadPool::get_instance().submit([this, start_it, end_it]()
                                                               {
            std::string sql_prefix = "INSERT INTO inverted_index_table (word, doc_id, weight, url) VALUES ";
            std::string sql_values;
            int batch_size = 0;

            for (auto itr = start_it; itr != end_it; ++itr)
            {
                const std::string &word = itr->first;
                if (word.size() > 255)
                {
                    continue; // 跳过太长的单词
                }

                for (const auto &info : itr->second)
                {
                    std::string escaped_word = ns_helper::escape_string(mysql_, word);
                    std::string escaped_url = ns_helper::escape_string(mysql_, info.url_);

                    sql_values += "('" + escaped_word + "', " + std::to_string(info.doc_id_) + ", " +
                                std::to_string(info.weight_) + ", '" + escaped_url + "'),";
                    ++batch_size;

                    if (batch_size >= MAX_BATCH)
                    {
                        sql_values.pop_back(); // 去掉最后一个逗号
                        std::string sql = sql_prefix + sql_values;
                        if (mysql_query(mysql_, sql.c_str()) != 0)
                        {
                            std::cerr << "Warning: Failed to insert inverted index: " << mysql_error(mysql_)
                                    << " [SQL]: " << sql << std::endl;
                        }
                        sql_values.clear();
                        batch_size = 0;
                    }
                }
            }

            if (!sql_values.empty())
            {
                sql_values.pop_back();
                std::string sql = sql_prefix + sql_values;
                mysql_query(mysql_, sql.c_str());
            } });

            tasks.push_back(std::move(task));
        }

        for (auto &task : tasks)
        {
            task.get();
        }

        return true;
    }

private:
    index_table() = default;
    ~index_table() = default;
    index_table(const index_table &) = delete;
    index_table &operator=(const index_table &) = delete;
};