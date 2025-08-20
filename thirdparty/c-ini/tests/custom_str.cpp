#include "custom_str.h"
#include <string>

extern "C" {

int custom_str_init(struct str** str)
{
    *str = reinterpret_cast<struct str*>(new std::string);
    return 0;
}

void custom_str_deinit(struct str* str)
{
    delete reinterpret_cast<std::string*>(str);
}

int custom_str_set(struct str** str, const char* data, int len)
{
    auto* s = reinterpret_cast<std::string*>(*str);
    *s = std::string(data, len);
    return 0;
}

const char* custom_str_data(const struct str* str)
{
    const auto* s = reinterpret_cast<const std::string*>(str);
    return s->data();
}

int custom_str_len(const struct str* str)
{
    const auto* s = reinterpret_cast<const std::string*>(str);
    return s->size();
}
}
