#include "/home/mufeng/cpp-httplib/httplib.h"
#include <pthread.h>

#include "searcher.hpp"
#include "mysql.hpp"

#define root_path "../wwwroot"

enum user_status
{
    SUCCESS,
    EXIST,
    WRONG,
    FAILED
};

std::unordered_map<std::string, std::string> sessions; // session_id -> username

// 登录函数
int login(std::unordered_map<std::string, std::string> &users, const std::string &name, const std::string &password, std::string &session_id)
{
    // 先更新下用户表
    user_table::instance().read_user_information(users);
    // 比对账号密码
    if (users.count(name))
    {
        if (users.find(name) == users.end() || (users[name] != password))
        {
            return user_status::WRONG;
        }
        // 生成session id
        session_id = name + "_session"; // 简化的会话 ID
        sessions[session_id] = name;
        return user_status::SUCCESS;
    }
    return user_status::FAILED;
}

// 注册函数
int register_user(std::unordered_map<std::string, std::string> &users, const std::string &name, const std::string &password)
{
    // 判断用户表中是否有这个账号
    if (users.find(name) != users.end())
    {
        return user_status::EXIST;
    }
    // 添加到数据库中
    if (!user_table::instance().write_user_information(name, password))
    {
        return user_status::FAILED;
    }
    return user_status::SUCCESS;
}

// 搜索功能
void search(const std::string &session_id, const std::string &word, Searcher &s, httplib::Response &rsp)
{
    if (sessions.count(session_id) == 0)
    {
        rsp.status = 401; // 未登录
        rsp.set_content("未登录，请先登录。", "text/plain; charset=utf-8");
    }
    else
    {
        std::string json_string;
        s.search(word, &json_string);
        rsp.set_content(json_string, "application/json");
    }
}

int main()
{
    Searcher s;
    ns_helper::jieba_util::get_instance()->init();

    std::cout << "init success\n";
    httplib::Server svr;
    svr.set_base_dir(root_path);

    // 读取用户表
    user_table user_tb;
    std::unordered_map<std::string, std::string> users;
    user_tb.read_user_information(users);

    // 用户注册
    svr.Post("/register", [&user_tb, &users](const httplib::Request &req, httplib::Response &rsp)
             {
        Json::Value json_body;
        Json::CharReaderBuilder reader;
        std::istringstream s(req.body);
        std::string errs;

        if (!Json::parseFromStream(reader, s, &json_body, &errs)) {
            rsp.status = 400;  // 请求格式错误
            rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
            return;
        }

        std::string username = json_body["username"].asString();
        std::string password = json_body["password"].asString();

       int ret= register_user(user_tb,users,username, password);
       if(ret==user_status::EXIST){
            rsp.set_content("账号已存在", "text/plain; charset=utf-8");
       }
       else if(ret==user_status::FAILED){
            rsp.set_content("注册失败,请联系开发者", "text/plain; charset=utf-8");
       }
       else{
            rsp.set_content("注册成功", "text/plain; charset=utf-8");
       } });

    // 用户登录
    svr.Post("/login", [&user_tb, &users](const httplib::Request &req, httplib::Response &rsp)
             {
        Json::Value json_body;
        Json::CharReaderBuilder reader;
        std::istringstream s(req.body);
        std::string errs;

        if (!Json::parseFromStream(reader, s, &json_body, &errs)) {
            rsp.status = 400;  // 请求格式错误
            rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
            return;
        }

        std::string username = json_body["username"].asString();
        std::string password = json_body["password"].asString();

        std::string session_id;
       int ret= login(user_tb,users,username, password,session_id);

        //分类
        if (ret==user_status::SUCCESS) {
            rsp.set_content("登录成功, 会话ID: " + session_id, "text/plain; charset=utf-8");
        } 
        else if(ret==user_status::WRONG){
            rsp.set_content("账号或密码输入错误", "text/plain; charset=utf-8");
        }
        else {
            rsp.status = 401;
            rsp.set_content("登录失败", "text/plain; charset=utf-8");
        } });

    // 搜索
    svr.Get("/s", [&s](const httplib::Request &req, httplib::Response &rsp)
            {
                if (!req.has_param("word"))
                {
                    rsp.set_content("必须要有搜索关键字", "text/plain; charset=utf-8");
                    return;
                }

                std::string word = req.get_param_value("word");
                std::string session_id = req.get_param_value("session_id"); // 获取会话 ID

                std::cout << "用户在搜索：" << word << std::endl;
                search(session_id, word, s, rsp); // 调用搜索函数
            });

    std::cout << "Starting server on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);
    std::cout << "Server is listening..." << std::endl;
    return 0;
}
