#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>

struct TrieNode
{
    std::unordered_map<char, TrieNode *> children; // 每个字符对应一个子节点
    bool is_end = false;
    int freq = 0; // 用于排序
};

class Trie
{
public:
    static Trie &instance()
    {
        static Trie instance; // 局部静态变量线程安全
        return instance;
    }

    void insert(const std::string &word, int freq = 1)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        TrieNode *node = root;
        // 逐字符构建 Trie 路径
        for (char c : word)
        {
            if (!node->children.count(c))
            {
                node->children[c] = new TrieNode();
            }
            node = node->children[c];
        }
        // 构建完设置结束标志
        node->is_end = true;
        // 累加词频
        node->freq += freq;
    }

private:
    TrieNode *root;
    std::mutex mutex_;

    Trie() { root = new TrieNode(); }
    Trie(const Trie &) = delete;
    Trie &operator=(const Trie &) = delete;

    void dfs(TrieNode *node, std::string &path, std::vector<std::pair<std::string, int>> &results)
    {
        if (node->is_end)
        {
            results.emplace_back(path, node->freq);
        }
        for (const auto &[ch, child] : node->children)
        {
            path.push_back(ch);
            dfs(child, path, results);
            path.pop_back();
        }
    }
    std::vector<std::pair<std::string, int>> starts_with(const std::string &prefix)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        TrieNode *node = root;
        for (char c : prefix)
        {
            if (!node->children.count(c))
                return {};
            node = node->children[c];
        }

        std::vector<std::pair<std::string, int>> results;
        std::string path = prefix;
        dfs(node, path, results);

        std::sort(results.begin(), results.end(), [](const auto &a, const auto &b)
                  { return a.second > b.second; });
        return results;
    }
};