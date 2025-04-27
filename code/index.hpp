// 建立正排和倒排索引,并提供查找接口
#pragma once

#include <utility>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "assistance.hpp"
#include "mysql.hpp"
#include "FixedThreadPool.hpp"

namespace fs = boost::filesystem;

class Index
{
    std::vector<ns_helper::docInfo_index> pos_index_;                       // 正排索引
    std::unordered_map<std::string, ns_helper::inverted_zipper> inv_index_; // 倒排索引
    static Index instance_;

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
        return &instance_;
    }
    void init()
    {
        bool pos_flag = false, inv_flag = false;

        create_positive_index(pos_flag);
        create_inverted_index(inv_flag);
        Persistence(pos_flag, inv_flag);
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
    bool search_inverted_index(const std::string &target, ns_helper::inverted_zipper &iz)
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
    const std::unordered_map<std::string, ns_helper::inverted_zipper> &get_inv_index()
    {
        return inv_index_;
    }

private:
    void create_positive_index(bool &pos_flag)
    {
        if (!index_table::instance().has_forward_index_data("forward_index_table"))
        {
            pos_flag = true;
            Log::getInstance()(INFO, "create positive_index");
            auto task = FixedThreadPool::get_instance().submit([this]
                                                               { source_table::instance().read_source_information(pos_index_); });

            task.get();
        }
        else
        {
            Log::getInstance()(INFO, "load positive_index from MySQL");
            auto task = FixedThreadPool::get_instance().submit([this]
                                                               { index_table::instance().load_positive(pos_index_); });
            task.get();
        }
        Log::getInstance()(INFO, "正排索引建立完成 %d", pos_index_.size());
    }
    void create_inverted_index(bool &inv_flag)
    {
        if (!index_table::instance().has_forward_index_data("inverted_index_table"))
        {
            inv_flag = true;
            Log::getInstance()(INFO, "create inverted_index");

            // 逐个处理正排索引，生成倒排索引
            for (size_t i = 0; i < pos_index_.size(); ++i)
            {
                help_create_inverted_index(pos_index_[i]); // 处理每一个文档
            }
        }
        else
        {
            Log::getInstance()(INFO, "load inverted_index from MySQL");

            auto task = FixedThreadPool::get_instance().submit([this]
                                                               { index_table::instance().load_inverted(inv_index_); });
            task.get();
        }

        Log::getInstance()(INFO, "倒排索引建立完成 %lu", inv_index_.size());
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
            ns_helper::word_info t;
            t.doc_id_ = doc.doc_id_;
            t.url_ = doc.url_;
            t.word_ = it.first;
            t.weight_ = (it.second).title_cnt_ * title_count + (it.second).content_cnt_ * content_count;
            inv_index_[t.word_].push_back(t); // 插入的是小写单词
        }
    }
    void Persistence(bool pos_flag, bool inv_flag)
    {
        if (pos_flag)
        {
            auto task = FixedThreadPool::get_instance().submit([this]
                                                               { index_table::instance().save_positive(pos_index_); 
            Log::getInstance()(INFO, "正排索引持久化完成"); });
        }
        if (inv_flag)
        {
            auto task = FixedThreadPool::get_instance().submit([this]
                                                               { index_table::instance().save_inverted(inv_index_);
            Log::getInstance()(INFO, "倒排索引持久化完成"); });
        }
    }
};

Index Index::instance_;