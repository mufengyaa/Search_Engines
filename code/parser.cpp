// 对网页信息去标签和数据清洗,生成一份新文件
//(没法处理特大文件,9w字节以上)? 但感觉有些超了的好像也没事,不知道为什么会Segmentation fault (core dumped)
#include <utility>
#include <boost/filesystem.hpp>
#include <regex>

#include "assistance.hpp"

namespace fs = boost::filesystem;

//static int count = 1;

class Parser
{
    std::vector<ns_helper::doc_info> docs_;

public:
    Parser() {}
    ~Parser() {}
    bool parser(const std::string &path, const std::string &output_path)
    {
        std::ofstream out(output_path, std::ios_base::out | std::ios_base::binary);
        if (!out.is_open())
        {
            lg(ERROR, "file: %s open failed", path.c_str());
            return false;
        }

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
                    // 边解析边写入文件
                    if (analysis(data, document, file->path().string()))
                    {
                        out.write(document.title_.c_str(), document.title_.size());
                        out << delimiter;
                        out.write(document.content_.c_str(), document.content_.size());
                        out << delimiter;
                        out.write(document.url_.c_str(), document.url_.size());
                        out << '\n';
                    }
                }
            }
        }
        else
        {
            return false;
        }

        out.close();
        lg(DEBUG, "write success");

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

        //std::cout << "已完成文档" << count++ << std::endl;

        return true;

        // // 提取content,也就是去掉标签
        // enum state
        // {
        //     LABLE,
        //     CONTENT
        // };
        // enum state s = LABLE;
        // for (char c : data)
        // {
        //     switch (s)
        //     {
        //     case LABLE:
        //         if (c == '>')
        //         {
        //             s = CONTENT;
        //         }
        //         break;
        //     case CONTENT:
        //         if (c == '<')
        //         {
        //             s = LABLE;
        //         }
        //         else
        //         {
        //             // 我们不想保留原始文件中的\n,因为我们想用\n作为html解析之后文本的分隔符
        //             if (c == '\n')
        //             {
        //                 c = ' ';
        //             }
        //             (result.content_).push_back(c);
        //         }
        //         break;
        //     default:
        //         break;
        //     }
        // }
        // std::cout << "content:";
        // std::cout << result.content_ << std::endl;
    }
};

void work()
{
    Parser p;
    if (!p.parser(source_file_path, target_path))
    {
        lg(ERROR, "parser falied");
    }
}
int main()
{
    work();
    return 0;
}