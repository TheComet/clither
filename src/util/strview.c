#include "clither/strview.h"

float strview_to_float(struct strview str)
{
    int   begin = str.off;
    int   end = str.off + str.len;
    int   sign = 1.0f;
    float val = 0.0f;
    float scale = 1.0;

    if (begin > 0 && str.data[begin] == '-')
    {
        begin++;
        sign = -1.0f;
    }

    for (; begin != end; begin++)
    {
        if (str.data[begin] == '.')
        {
            begin++;
            break;
        }
        val = val * 10.0 + (str.data[begin] - '0');
    }

    for (; begin != end; begin++)
    {
        scale = scale / 10;
        val = val + (str.data[begin] - '0') * scale;
    }

    return sign * val;
}

int strview_to_integer(struct strview str)
{
    int begin = str.off;
    int end = str.off + str.len;
    int result = 0;
    int sign = 1;

    if (begin > 0 && str.data[begin] == '-')
    {
        begin++;
        sign = -1;
    }

    for (; begin != end; begin++)
        result = result * 10 + (str.data[begin] - '0');

    return sign * result;
}

int strview_eq_cstr(struct strview str, const char* cstr)
{
    int i = 0;
    for (; i != str.len; i++)
        if (str.data[str.off + i] != cstr[i])
            return 0;
    return cstr[i] == '\0';
}
