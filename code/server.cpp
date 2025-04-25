#include "/home/mufeng/cpp-httplib/httplib.h"
#include <pthread.h>

#include "search/search_engine.hpp"
#include "mysql.hpp"
#include "search/suggest.hpp"
#include "auth.hpp"

#define root_path "../wwwroot"

// 101.126.142.54:8080

// 用户注册
void handle_register(const httplib::Request &req, httplib::Response &rsp)
{
    Json::Value json_body;
    Json::CharReaderBuilder reader;
    std::istringstream s(req.body);
    std::string errs;

    if (!Json::parseFromStream(reader, s, &json_body, &errs))
    {
        rsp.status = 400;
        rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
        return;
    }

    std::string username = json_body["username"].asString();
    std::string password = json_body["password"].asString();

    int ret = auth_manager::instance().register_user(username, password);
    if (ret == user_status::EXIST)
    {
        rsp.set_content("账号已存在", "text/plain; charset=utf-8");
    }
    else if (ret == user_status::FAILED)
    {
        rsp.set_content("注册失败，请联系管理员", "text/plain; charset=utf-8");
    }
    else
    {
        rsp.set_content("注册成功", "text/plain; charset=utf-8");
    }
}

// 用户登录
void handle_login(const httplib::Request &req, httplib::Response &rsp)
{
    Json::Value json_body;
    Json::CharReaderBuilder reader;
    std::istringstream s(req.body);
    std::string errs;

    if (!Json::parseFromStream(reader, s, &json_body, &errs))
    {
        rsp.status = 400;
        rsp.set_content("请求格式错误", "text/plain; charset=utf-8");
        return;
    }

    std::string username = json_body["username"].asString();
    std::string password = json_body["password"].asString();
    std::string session_id;

    int ret = auth_manager::instance().login(username, password, session_id);
    if (ret == user_status::SUCCESS)
    {
        rsp.set_content("登录成功, 会话ID: " + session_id, "text/plain; charset=utf-8");
    }
    else if (ret == user_status::WRONG)
    {
        rsp.set_content("账号或密码错误", "text/plain; charset=utf-8");
    }
    else
    {
        rsp.status = 401;
        rsp.set_content("登录失败", "text/plain; charset=utf-8");
    }
}

// 搜索
// 搜索请求处理
void handle_search(const httplib::Request &req, httplib::Response &rsp)
{
    // 检查是否有搜索关键字
    if (!req.has_param("word"))
    {
        rsp.status = 400; // Bad Request
        rsp.set_content("必须要有搜索关键字", "text/plain; charset=utf-8");
        return;
    }

    std::string word = req.get_param_value("word");
    std::string session_id = req.get_param_value("session_id"); // 获取会话 ID

    // 登录后才能进行后续操作
    if (auth_manager::instance().validate_session(session_id))
    {
        std::cout << "用户在搜索：" << word << std::endl;

        // 判断请求是联想 / 正式搜索
        if (req.has_param("search") && req.get_param_value("search") == "true")
        {
            std::string json_string;

            Searcher::instance().search(word, &json_string);
            rsp.set_content(json_string, "application/json");
        }
        else
        {
            suggest(word, rsp); // 提供联想建议
        }
    }
    else
    {
        rsp.status = 401; // 未登录
        rsp.set_content("未登录，请先登录", "text/plain; charset=utf-8");
    }
}

int main()
{
    ns_helper::jieba_util::get_instance()->init();

    // 读取用户表
    std::unordered_map<std::string, std::string> users;
    user_table::instance().read_user_information(users);

    // 初始化服务器
    httplib::Server svr;
    svr.set_base_dir(root_path);

    svr.Post("/register", handle_register);
    svr.Post("/login", handle_login);
    svr.Get("/s", [](const httplib::Request &req, httplib::Response &rsp)
            { handle_search(req, rsp); });

    std::cout << "Starting server on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);
    std::cout << "Server is listening..." << std::endl;
    return 0;
}