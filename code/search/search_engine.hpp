#include <algorithm>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <mutex>

#include <jsoncpp/json/json.h>
#include "../mysql.hpp"
#include "../assistance.hpp"
#include "../index.hpp"

class Searcher
{
    Index *index_ = nullptr;
    Searcher()
    {
        index_ = Index::get_instance();
    }
    ~Searcher() {}
    Searcher(const Searcher &) = delete;
    Searcher &operator=(const Searcher &) = delete;

public:
    static Searcher &instance()
    {
        static Searcher instance_;
        return instance_;
    }

    void search(const std::string &data, std::string *json)
    {

        // 进行分词
        std::vector<std::string> words;
        ns_helper::jieba_util::CutString(data, words);

        struct words_info
        {
            std::vector<std::string> words_; // 多个分词可以在某文档中查找到
            doc_id_t doc_id_ = 0;
            int weight_ = 0; // 这个词在文档中的权重
            bool is_ad_ = false;
        };
        std::unordered_map<doc_id_t, words_info> Non_duplicate_map;

        // 引入广告功能
        std::unordered_map<std::string, float> ads;
        advertising_table::instance().read_advertising_information(ads);

        // 得到不重复的文档集合
        for (auto word : words)
        {
            boost::to_lower(word);
            ns_helper::inverted_zipper zipper;
            index_->search_inverted_index(word, zipper);
            for (auto &it : zipper)
            {
                Non_duplicate_map[it.doc_id_].doc_id_ = it.doc_id_;
                std::string url = it.url_;
                Non_duplicate_map[it.doc_id_].weight_ += it.weight_;

                const float alpha = 0.1f;
                if (ads.find(url) != ads.end())
                {
                    Non_duplicate_map[it.doc_id_].is_ad_ = true;
                    Non_duplicate_map[it.doc_id_].weight_ += (int)(ads[url] * alpha);
                }

                Non_duplicate_map[it.doc_id_].words_.push_back(std::move(it.word_));
            }
        }

        // 将文档集合转换类型(转成vector方便排序)
        std::vector<words_info> doc_map;
        for (auto &it : Non_duplicate_map)
        {
            doc_map.push_back(std::move(it.second));
        }

        // 按相关性排序
        std::sort(doc_map.begin(), doc_map.end(),
                  [](const words_info &x, const words_info &y)
                  { return x.weight_ > y.weight_; });

        // 查询正排索引,并构建json串
        Json::Value root;
        // 添加常驻置顶
        Json::Value top;
        top["title"] = "我的博客";
        top["desc"] = "这是本人的博客,欢迎大家浏览";
        top["url"] = "https://blog.csdn.net/m0_74206992?type=blog";
        root.append(top);

        for (const auto &it : doc_map)
        {
            ns_helper::docInfo_index doc;
            if (!index_->search_positive_index(it.doc_id_, doc))
            {
                continue;
            }

            Json::Value item;
            std::string title = doc.title_;
            if (it.is_ad_ == true)
            {
                title += "[广告]"; // 这里加的空格会被json忽略
            }
            // title += ",weight=" + std::to_string(it.weight_);  //Debug
            item["title"] = std::move(title);
            item["desc"] = get_desc(doc.content_, it.words_[0]);
            item["url"] = doc.url_;

            root.append(item);
        }
        Json::FastWriter writer;
        // Json::StyledWriter writer;
        *json = writer.write(root);
    }

private:
    std::string get_desc(const std::string &content, const std::string &word)
    {
#define left_count 50
#define right_count 80
        // ssize_t pos = content.find(word);
        // 要实现大小写匹配,查找时要将原文也转换为小写
        auto iter = std::search(content.begin(), content.end(), word.begin(), word.end(), [](int x, int y)
                                { return (std::tolower(x) == std::tolower(y)); });
        if (iter == content.end())
        {
            lg(ERROR, "No keywords found");
            return "";
        }
        int pos = std::distance(content.begin(), iter);

        // 找范围
        ssize_t left = 0, right = content.size() - 1;
        if (pos > left_count + left)
        {
            left = pos - left_count;
        }
        if (pos + word.size() + right_count < right)
        {
            right = pos + word.size() + right_count;
        }

        if (left >= right)
        {
            lg(ERROR, "Subscript error found");
            return "";
        }
        return content.substr(left, right - left);
    }
};