#pragma once

struct fpos
{
    float x;
    float y;
};

static struct fpos make_fpos(float x, float y)
{
    struct fpos p;
    p.x = x;
    p.y = y;
    return p;
}
