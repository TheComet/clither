/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef BASE64_H
#define BASE64_H

#include <stdint.h>

#define base64_output_len(len)                                                 \
    ((len) * 4 / 3 + 4 + /* 3-byte blocks to 4-byte  */                        \
     1                   /* nul termination */                                 \
    )
#define base64_input_len(len) ((len) / 4 * 3)

int base64_encode(uint8_t* dst, const uint8_t* src, int len);
int base64_decode(uint8_t* dst, const uint8_t* src, int len);

#endif /* BASE64_H */
