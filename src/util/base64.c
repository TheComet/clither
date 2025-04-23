/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "clither/util/base64.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
int base64_encode(uint8_t* dst, const uint8_t* src, int len)
{
    int i;
    int o = 0;

    for (i = 0; len - i >= 3; i += 3)
    {
        dst[o++] = base64_table[src[i] >> 2];
        dst[o++] = base64_table[((src[i] & 0x03) << 4) | (src[i + 1] >> 4)];
        dst[o++] = base64_table[((src[i + 1] & 0x0f) << 2) | (src[i + 2] >> 6)];
        dst[o++] = base64_table[src[i + 2] & 0x3f];
    }

    if (len - i)
    {
        dst[o++] = base64_table[src[i] >> 2];
        if (len - i == 1)
        {
            dst[o++] = base64_table[(src[i] & 0x03) << 4];
            dst[o++] = '=';
        }
        else
        {
            dst[o++] = base64_table[((src[i] & 0x03) << 4) | (src[i + 1] >> 4)];
            dst[o++] = base64_table[(src[i + 1] & 0x0f) << 2];
        }
        dst[o++] = '=';
    }

    dst[o] = '\0';
    return o;
}

/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
int base64_decode(uint8_t* dst, const uint8_t* src, int len)
{
    uint8_t dtable[256], block[4], tmp;
    int     i, o, count;
    int     pad = 0;

    memset(dtable, 0x80, 256);
    for (i = 0; i < (int)sizeof(base64_table) - 1; i++)
        dtable[base64_table[i]] = (uint8_t)i;
    dtable['='] = 0;

    count = 0;
    for (i = 0; i < len; i++)
    {
        if (dtable[src[i]] != 0x80)
            count++;
    }

    if (count == 0 || count % 4)
        return -1;

    count = 0;
    o = 0;
    for (i = 0; i < len; i++)
    {
        tmp = dtable[src[i]];
        if (tmp == 0x80)
            continue;

        if (src[i] == '=')
            pad++;
        block[count] = tmp;
        count++;
        if (count == 4)
        {
            dst[o++] = (block[0] << 2) | (block[1] >> 4);
            dst[o++] = (block[1] << 4) | (block[2] >> 2);
            dst[o++] = (block[2] << 6) | block[3];
            count = 0;
            if (pad)
            {
                if (pad == 1)
                    o--;
                else if (pad == 2)
                    o -= 2;
                else
                    return -1; /* Invalid padding */
                break;
            }
        }
    }

    return 0;
}
