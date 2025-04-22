#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <mysql/mysql.h>

#include "assistance.hpp"

// 提供插入/读取广告信息的功能
class my_mysql
{
protected:
    MYSQL *mysql_;

public:
    my_mysql()
    {
        connect();
    }
    ~my_mysql()
    {
        mysql_close(mysql_);
    }

private:
    void connect()
    {
        mysql_ = mysql_init(nullptr);
        mysql_ = mysql_real_connect(mysql_, "101.126.142.54", "mufeng", "599348181", "conn", 3306, nullptr, 0);
        if (nullptr == mysql_)
        {
            std::cout << "connect failed\n";
            exit(1);
        }
        mysql_set_character_set(mysql_, "utf8");
    }
};

class advertising_table : public my_mysql
{
public:
    void read_advertising_information(std::unordered_map<std::string, float> &data)
    {
        std::string sql;
        sql = "select url,fee from ad";
        int ret = mysql_query(mysql_, sql.c_str());
        if (ret == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res == nullptr)
            {
                std::cout << "mysql_store_result failed\n";
            }
            else
            {
                int row_num = mysql_num_rows(res);
                int field_num = mysql_num_fields(res);
                // // 打印列名
                // MYSQL_FIELD *field;
                // while ((field = mysql_fetch_field(res)) != NULL)
                // {
                //     std::cout << field->name << " ";
                // }
                // std::cout << std::endl;

                // 拿出表数据
                for (int i = 0; i < row_num; ++i)
                {
                    MYSQL_ROW row = mysql_fetch_row(res);
                    // 转换广告费类型
                    std::string fee = row[1];
                    data[row[0]] = std::stof(fee);
                }
            }
        }
        else
        {
            std::cout << mysql_error(mysql_) << std::endl;
        }
    }
};

class user_table : public my_mysql
{
public:
    bool write_user_information(const std::string &name, const std::string &password)
    {
        std::string sql = "insert into user(name,password) values('" + name + "','" + password + "')";
        int ret = mysql_query(mysql_, sql.c_str());
        if (ret == 0)
        {
            return true;
        }
        else
        {
            lg(ERROR, "%s", mysql_error(mysql_));
            return false;
        }
    }
    void read_user_information(std::unordered_map<std::string, std::string>& users)
    {
        std::string sql;
        sql = "select name,password from user";
        int ret = mysql_query(mysql_, sql.c_str());
        if (ret == 0)
        {
            MYSQL_RES *res = mysql_store_result(mysql_);
            if (res == nullptr)
            {
                std::cout << "mysql_store_result failed\n";
            }
            else
            {
                int row_num = mysql_num_rows(res);
                int field_num = mysql_num_fields(res);

                // 拿出表数据
                for (int i = 0; i < row_num; ++i)
                {
                    MYSQL_ROW row = mysql_fetch_row(res);
                    users[row[0]] = row[1];
                }
            }
        }
        else
        {
            std::cout << mysql_error(mysql_) << std::endl;
        }
    }
};