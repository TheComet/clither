#pragma once

struct strspan
{
    int off, len;
};

static inline struct strspan strspan(int off, int len)
{
    struct strspan span;
    span.off = off;
    span.len = len;
    return span;
}
