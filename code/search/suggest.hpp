#pragma once

#include <string>
#include "trie.hpp"
#include "/home/mufeng/cpp-httplib/httplib.h"

void suggest(const std::string &word, httplib::Response &rsp)
{
    auto results = Trie::instance().starts_with(word);

    std::ostringstream oss;
    oss << R"(<ul id="suggestions-list">)"; // 开始列表标签

    for (size_t i = 0; i < results.size(); ++i)
    {
        oss << "<li class=\"suggestion-item\" data-suggestion=\"" << results[i].first << "\">"
            << results[i].first << "</li>";
    }

    oss << "</ul>"; // 结束列表标签

    rsp.set_content(oss.str(), "text/html; charset=utf-8");
}
