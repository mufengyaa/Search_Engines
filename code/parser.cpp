// 对网页信息去标签和数据清洗,生成一份新文件
//(没法处理特大文件,9w字节以上)? 但感觉有些超了的好像也没事,不知道为什么会Segmentation fault (core dumped)
#include <utility>
#include <boost/filesystem.hpp>
#include <regex>

#include "assistance.hpp"
#include "mysql.hpp"

namespace fs = boost::filesystem;

class Parser
{
    std::vector<ns_helper::doc_info> docs_;

public:
    Parser() {}
    ~Parser() {}
    bool parser(const std::string &path)
    {
        fs::path directory(path);
        if (fs::exists(directory) && fs::is_directory(directory))
        {
            fs::recursive_directory_iterator begin(directory), end;
            for (fs::recursive_directory_iterator file = begin; file != end; ++file)
            {
                if (fs::is_regular_file(*file) && file->path().extension() == ".html")
                {
                    // 读取文件
                    std::string data;
                    ns_helper::read_file(file->path().string(), data);
                    ns_helper::doc_info document;

                    // 写入数据库
                    if (analysis(data, document, file->path().string()))
                    {
                        bool ret = source_table::instance().write_source_information(document.title_, document.content_, document.url_);
                        if (!ret)
                        {
                            Log::getInstance()(ERROR, "file: %s write db failed", path.c_str());
                        }
                    }
                }
            }
        }
        else
        {
            return false;
        }
        Log::getInstance()(DEBUG, "write success");

        return true;
    }

private:
    bool analysis(const std::string &data, ns_helper::doc_info &result, const std::string &path)
    {
        // 使用正则表达式提取<title>标签内容
        std::regex title_regex("<title>(.*?)</title>", std::regex::icase);
        std::smatch title_match;
        if (std::regex_search(data, title_match, title_regex))
        {
            result.title_ = title_match[1].str();
        }
        else
        {
            return false;
        }

        // 构建url
        std::string head = "https://www.boost.org/doc/libs/1_86_0/";
        std::string t = source_file_path;
        std::string tail = path.substr(t.size());
        result.url_ = head + tail;

        std::cout << "url:" << result.url_ << std::endl;

        // 洗标签
        std::regex css_js_regex("<(script|style)[^>]*?>(?:.|\\n)*?</\\1>|<[^>]+>", std::regex::icase);
        result.content_ = std::regex_replace(data, css_js_regex, "");

        // 移除多余的空白字符和空行
        std::regex whitespace_regex("\\s+");
        result.content_ = std::regex_replace(result.content_, whitespace_regex, " ");

        // 去掉内容开头和结尾的多余空白
        result.content_ = std::regex_replace(result.content_, std::regex("^\\s+|\\s+$"), "");

        return true;
    }
};

void work()
{
    Parser p;
    if (!p.parser(source_file_path))
    {
        Log::getInstance()(ERROR, "parser falied");
    }
}
int main()
{
    work();
    return 0;
}