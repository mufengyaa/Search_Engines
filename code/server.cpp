#include "/home/mufeng/cpp-httplib/httplib.h"
#include <pthread.h>

#include "search/search_engine.hpp"
#include "mysql.hpp"
#include "search/suggest.hpp"
#include "auth.hpp"

#define root_path "../wwwroot"

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

    httplib::Server svr;
    svr.set_base_dir(root_path);

    // 读取用户表
    user_table user_tb;
    std::unordered_map<std::string, std::string> users;
    user_tb.read_user_information(users);

    // // 用户注册
    // svr.Post("/register", [&user_tb, &users](const httplib::Request &req, httplib::Response &rsp)
    //          {
    //     Json::Value json_body;
    //     Json::CharReaderBuilder reader;
    //     std::istringstream s(req.body);
    //     std::string errs;

    //     if (!Json::parseFromStream(reader, s, &json_body, &errs)) {
    //         rsp.status = 400;  // 请求格式错误
    //         rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
    //         return;
    //     }

    //     std::string username = json_body["username"].asString();
    //     std::string password = json_body["password"].asString();

    //    int ret= register_user(user_tb,users,username, password);
    //    if(ret==user_status::EXIST){
    //         rsp.set_content("账号已存在", "text/plain; charset=utf-8");
    //    }
    //    else if(ret==user_status::FAILED){
    //         rsp.set_content("注册失败,请联系开发者", "text/plain; charset=utf-8");
    //    }
    //    else{
    //         rsp.set_content("注册成功", "text/plain; charset=utf-8");
    //    } });

    // // 用户登录
    // svr.Post("/login", [&user_tb, &users](const httplib::Request &req, httplib::Response &rsp)
    //          {
    //     Json::Value json_body;
    //     Json::CharReaderBuilder reader;
    //     std::istringstream s(req.body);
    //     std::string errs;

    //     if (!Json::parseFromStream(reader, s, &json_body, &errs)) {
    //         rsp.status = 400;  // 请求格式错误
    //         rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
    //         return;
    //     }

    //     std::string username = json_body["username"].asString();
    //     std::string password = json_body["password"].asString();

    //     std::string session_id;
    //    int ret= login(user_tb,users,username, password,session_id);

    //     //分类
    //     if (ret==user_status::SUCCESS) {
    //         rsp.set_content("登录成功, 会话ID: " + session_id, "text/plain; charset=utf-8");
    //     }
    //     else if(ret==user_status::WRONG){
    //         rsp.set_content("账号或密码输入错误", "text/plain; charset=utf-8");
    //     }
    //     else {
    //         rsp.status = 401;
    //         rsp.set_content("登录失败", "text/plain; charset=utf-8");
    //     } });

    // 用户注册
    svr.Post("/register", [](const httplib::Request &req, httplib::Response &rsp)
             {
            Json::Value json_body;
            Json::CharReaderBuilder reader;
            std::istringstream s(req.body);
            std::string errs;
    
            if (!Json::parseFromStream(reader, s, &json_body, &errs)) {
                rsp.status = 400;
                rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
                return;
            }
    
            std::string username = json_body["username"].asString();
            std::string password = json_body["password"].asString();
    
            int ret = auth_manager.register_user(username, password);
            if (ret == user_status::EXIST) {
                rsp.set_content("账号已存在", "text/plain; charset=utf-8");
            } else if (ret == user_status::FAILED) {
                rsp.set_content("注册失败，请联系管理员", "text/plain; charset=utf-8");
            } else {
                rsp.set_content("注册成功", "text/plain; charset=utf-8");
            } });

    // 用户登录
    svr.Post("/login", [](const httplib::Request &req, httplib::Response &rsp)
             {
            Json::Value json_body;
            Json::CharReaderBuilder reader;
            std::istringstream s(req.body);
            std::string errs;
    
            if (!Json::parseFromStream(reader, s, &json_body, &errs)) {
                rsp.status = 400;
                rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
                return;
            }
    
            std::string username = json_body["username"].asString();
            std::string password = json_body["password"].asString();
            std::string session_id;
    
            int ret = auth_manager.login(username, password, session_id);
            if (ret == user_status::SUCCESS) {
                rsp.set_content("登录成功, 会话ID: " + session_id, "text/plain; charset=utf-8");
            } else if (ret == user_status::WRONG) {
                rsp.set_content("账号或密码错误", "text/plain; charset=utf-8");
            } else {
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

        // 判断请求是联想 / 正式搜索
        if (req.has_param("search") && req.get_param_value("search") == "true")
        {
            search(session_id, word, s, rsp); // 带有search字段的才是正式搜索
        }
        else
        {
            suggest(word, s, rsp); // 联想建议
        }

        std::cout << "Starting server on port 8080..." << std::endl;
        svr.listen("0.0.0.0", 8080);
        std::cout << "Server is listening..." << std::endl;
        return 0;
}
