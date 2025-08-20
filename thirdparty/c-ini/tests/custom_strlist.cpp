#include "custom_strlist.h"
#include <string>
#include <vector>

using list = std::vector<std::string>;

extern "C" {

int custom_strlist_init(struct strlist** l)
{
    *l = reinterpret_cast<struct strlist*>(new list);
    return 0;
}

void custom_strlist_deinit(struct strlist* l)
{
    delete reinterpret_cast<list*>(l);
}

int custom_strlist_add(struct strlist** l, const char* data, int len)
{
    auto* cpp_l = reinterpret_cast<list*>(*l);
    cpp_l->push_back(std::string(data, len));
    return 0;
}

void custom_strlist_clear(struct strlist* l)
{
    auto* cpp_l = reinterpret_cast<list*>(l);
    cpp_l->clear();
}
}
