#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <mysql/mysql.h>

#include "../build/inc/cppjieba/Jieba.hpp"
#include "../build/inc/cppjieba/limonp/StringUtil.hpp"
#include "../build/inc/cppjieba/limonp/Logging.hpp"
#include "Log.hpp"

#define source_file_path "/home/mufeng/boost_1_86_0/"
#define delimiter '\3'
#define doc_id_t long

namespace ns_helper
{
    // 数据结构
    struct doc_info
    {
        std::string title_;
        std::string content_;
        std::string url_;
    };
    class docInfo_index : public ns_helper::doc_info
    {
    public:
        doc_id_t doc_id_;
        docInfo_index() {}
        docInfo_index(const ns_helper::doc_info &doc)
        {
            title_ = doc.title_;
            content_ = doc.content_;
            url_ = doc.url_;
        }
    };
    struct word_info
    {
        std::string word_;
        doc_id_t doc_id_;
        int weight_; // 这个词在文档中的权重
        std::string url_;
    };
    using inverted_zipper = std::vector<word_info>;

    // 支持的任务类型
    inline constexpr const char *TASK_TYPE_BUILD_INDEX = "build_index";
    inline constexpr const char *TASK_TYPE_PERSIST_INDEX = "persist_index";
    inline constexpr const char *TASK_TYPE_SEARCH = "search";
    inline constexpr const char *TASK_TYPE_AUTOCOMPLETE = "autocomplete";

    void read_file(const std::string &path, std::string &data)
    {
        std::ifstream in(path, std::ios_base::in);
        if (!in.is_open())
        {
            Log::getInstance()(ERROR, "file: %s open failed", path.c_str());
        }
        std::string line;
        while (std::getline(in, line))
        {
            data += line;
        }
        in.close();
    }
    // 转义字符串，防止 SQL 注入
    std::string escape_string(MYSQL *mysql_, const std::string &input)
    {
        char *buffer = new char[input.size() * 2 + 1];
        mysql_real_escape_string(mysql_, buffer, input.c_str(), input.size());
        std::string result(buffer);
        delete[] buffer;
        return result;
    }

    // 分词
    const char *const DICT_PATH = "/home/mufeng/c++/Search_Engines/build/dict/jieba.dict.utf8";
    const char *const HMM_PATH = "/home/mufeng/c++/Search_Engines/build/dict/hmm_model.utf8";
    const char *const USER_DICT_PATH = "/home/mufeng/c++/Search_Engines/build/dict/user.dict.utf8";
    const char *const IDF_PATH = "/home/mufeng/c++/Search_Engines/build/dict/idf.utf8";
    const char *const STOP_WORD_PATH = "/home/mufeng/c++/Search_Engines/build/dict/stop_words.utf8";
    class jieba_util
    {
    private:
        cppjieba::Jieba jieba_;
        std::unordered_map<std::string, bool> stop_words_; // bool是辅助参数,没啥用

    private:
        jieba_util() : jieba_(DICT_PATH, HMM_PATH, USER_DICT_PATH, IDF_PATH, STOP_WORD_PATH)
        {
            init();
        }
        jieba_util(const jieba_util &) = delete;
        jieba_util &operator=(const jieba_util &) = delete;

        static jieba_util *instance_;

    public:
        void init()
        {
            std::ifstream in(STOP_WORD_PATH);
            if (!in.is_open())
            {
                Log::getInstance()(ERROR, "file: stop_words.utf8 open failed");
            }
            std::string line;
            while (std::getline(in, line))
            {
                stop_words_[line] = true;
            }
        }
        static jieba_util *get_instance()
        {
            static std::mutex mtx;
            if (nullptr == instance_) // 防止已有对象后,还要进来加锁
            {
                mtx.lock();
                if (nullptr == instance_) // 防止多线程下重复创建对象
                {
                    instance_ = new jieba_util();
                    Log::getInstance()(DEBUG, "JiebaUtil success");
                }
                mtx.unlock();
            }
            return instance_;
        }
        void cut_helper(const std::string &src, std::vector<std::string> &out)
        {
            jieba_.CutForSearch(src, out);
            for (auto word = out.begin(); word != out.end();)
            {
                auto pos = stop_words_.find(*word);
                if (pos != stop_words_.end())
                {
                    // 当前词就是暂停词
                    word = out.erase(word);
                }
                else
                {
                    ++word;
                }
            }
        }

    public:
        static void CutString(const std::string &src, std::vector<std::string> &out)
        {
            ns_helper::jieba_util::get_instance()->cut_helper(src, out);
        }
    };

    jieba_util *jieba_util::instance_ = nullptr;
};