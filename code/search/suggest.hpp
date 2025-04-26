#pragma once

#include <string>
#include "trie.hpp"
#include "/home/mufeng/cpp-httplib/httplib.h"

void suggest(const std::string &word, httplib::Response &rsp)
{
    auto results = Trie::instance().starts_with(word);

    std::ostringstream oss;
    oss << R"(<select id="suggestions">)"; // 开始下拉框标签

    for (size_t i = 0; i < results.size(); ++i)
    {
        oss << "<option value=\"" << results[i].first << "\">" << results[i].first << "</option>";
    }

    oss << "</select>"; // 结束下拉框标签

    rsp.set_content(oss.str(), "text/html; charset=utf-8");
}
