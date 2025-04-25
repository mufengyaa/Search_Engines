// 建立正排和倒排索引,并提供查找接口
#pragma once

#include <utility>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "assistance.hpp"
#include "mysql.hpp"

namespace fs = boost::filesystem;

class Index
{
    std::vector<ns_helper::docInfo_index> pos_index_;                       // 正排索引
    std::unordered_map<std::string, ns_helper::inverted_zipper> inv_index_; // 倒排索引

    Index()
    {
        init();
    }
    Index(const Index &) = delete;
    Index &operator=(const Index &) = delete;
    ~Index() {}

public:
    static Index *get_instance()
    {
        static Index instance_;
        return &instance_;
    }
    void Index::init()
    {
        // 正排索引构建
        if (!index_table::instance().has_forward_index_data("forward_index_table"))
        {
            // 通知线程去创建
            create_positive_index();
            lg(INFO, "create positive_index success");
            // 通知其他线程去持久化
        }
        else
        {
            // 通知线程去读取
            index_table::instance().load_positive(pos_index_);
            lg(INFO, "load index from MySQL");
        }

        // 倒排索引构建
        if (!index_table::instance().has_forward_index_data("inverted_index_table"))
        {
            // 通知线程去创建
            create_inverted_index();
            lg(INFO, "create inverted_index success");
        }
        else
        {
            index_table::instance().load_inverted(inv_index_);
            lg(INFO, "load index from MySQL");
        }
    }

    bool search_positive_index(const doc_id_t id, ns_helper::docInfo_index &doc)
    {
        if (id >= pos_index_.size())
        {
            return false;
        }
        doc = pos_index_[id];
        return true;
    }
    bool search_inverted_index(const std::string &target, inverted_zipper &iz)
    {
        auto ret = inv_index_.find(target);
        if (ret == inv_index_.end())
        {
            return false;
        }
        else
        {
            iz = (ret->second);
        }
        return true;
    }

private:
    void create_positive_index()
    {
        std::vector<ns_helper::doc_info> sources;
        source_table::instance().read_source_information(sources);
        for (auto &doc : sources)
        {
            // 拿到一个文档,进行解析
            ns_helper::docInfo_index di(std::move(doc));
            di.doc_id_ = pos_index_.size();
            // 解析完成后,插入到索引中
            pos_index_.push_back(std::move(di));
        }
    }
    void create_inverted_index() // 以文档为单位
    {
        for (const auto &it : pos_index_)
        {
            help_create_inverted_index(it);
        }
    }
    void help_create_inverted_index(const ns_helper::docInfo_index &doc)
    {
        struct word_cnt
        {
            int title_cnt_;
            int content_cnt_;
            word_cnt() : title_cnt_(0), content_cnt_(0) {}
            ~word_cnt() {}
        };
        std::unordered_map<std::string, word_cnt> cnt_map;

        // 统计每个词在所属文档中的相关性
        std::vector<std::string> content_words;
        ns_helper::jieba_util::CutString(doc.content_, content_words);

        for (auto it : content_words)
        {
            // 为了实现匹配时忽略大小写,将所有单词转换为小写
            boost::to_lower(it);
            ++cnt_map[it].content_cnt_;
        }

        std::vector<std::string> title_words;
        ns_helper::jieba_util::CutString(doc.title_, title_words);
        for (auto it : title_words)
        {
            boost::to_lower(it);
            ++cnt_map[it].title_cnt_;
        }

// 计算权值
#define title_count 10
#define content_count 1
        for (const auto &it : cnt_map)
        {
            word_info t;
            t.doc_id_ = doc.doc_id_;
            t.url_ = doc.url_;
            t.word_ = it.first;
            t.weight_ = (it.second).title_cnt_ * title_count + (it.second).content_cnt_ * content_count;
            inv_index_[t.word_].push_back(t); // 插入的是小写单词
        }
    }
};