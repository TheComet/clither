#pragma once

static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float unlerp(float a, float b, float t)
{
    return (t - a) / (b - a);
}
