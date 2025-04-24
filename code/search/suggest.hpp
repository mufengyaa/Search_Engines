#pragma once

#include <string>
#include "trie.hpp"
#include "/home/mufeng/cpp-httplib/httplib.h"

void suggest(const std::string &word, httplib::Response &rsp)
{
    auto results = Trie::instance().starts_with(word);

    std::ostringstream oss;
    oss << R"([)"; // 这里直接开始一个数组

    for (size_t i = 0; i < results.size(); ++i)
    {
        oss << "{\"word\": \"" << results[i].first << "\"}";
        if (i < results.size() - 1)
            oss << ","; // 如果不是最后一个元素，添加逗号
    }

    oss << "]"; // 结束数组

    rsp.set_content(oss.str(), "application/json; charset=utf-8");
}
