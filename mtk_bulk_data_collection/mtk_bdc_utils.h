/*
 * MIT License
 *
 * Copyright (c) 2024 LumenRadio AB
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef MTK_BDC_UTILS_H
#define MTK_BDC_UTILS_H

#define MLP_CON_(x, y) x##y
#define MLP_CON(x, y) MLP_CON_(x, y)

/* Check return values from functions and print possible errors. */
#define RUN_CHECK(f)                                          \
    do {                                                      \
        int MLP_CON(ret_, __LINE__) = f;                      \
        if (MLP_CON(ret_, __LINE__) < 0) {                    \
            P_ERR("[%d]: " #f "\n", MLP_CON(ret_, __LINE__)); \
        }                                                     \
    } while (0)

/* Check return values from mira functions and print possible errors. */
#define MIRA_RUN_CHECK(f)                                     \
    do {                                                      \
        mira_status_t MLP_CON(ret_, __LINE__) = f;            \
        if (MLP_CON(ret_, __LINE__) != MIRA_SUCCESS) {        \
            P_ERR("[%d]: " #f "\n", MLP_CON(ret_, __LINE__)); \
        }                                                     \
    } while (0)

/* Define DEBUG_LEVEL before including this header file, default is no print. */
#if !defined(DEBUG_LEVEL)
#define DEBUG_LEVEL 0
#endif

#if DEBUG_LEVEL > 0
#include <stdio.h>
#endif

/* Use P_ERR for messages that are not expected to happen in normal behavior. */
#if DEBUG_LEVEL >= 1
#define P_ERR(...) printf("ERROR " __VA_ARGS__)
#else
#define P_ERR(...)
#endif

/* Use P_DEBUG for help messages to analyze expected behavior. */
#if DEBUG_LEVEL >= 2
#define P_DEBUG(...) printf(__VA_ARGS__)
#else
#define P_DEBUG(...)
#endif

/* Store a variable to buffer, in little endian. The type of the variable
 * determines the width of the write, use types from stdint.h. */
#define LITTLE_ENDIAN_STORE(buffer, v)         \
    do {                                       \
        for (int i = 0; i < sizeof(v); i++) {  \
            buffer[i] = (v >> (i * 8)) & 0xff; \
        }                                      \
    } while (0)

/* Load a variable from buffer, read in little endian. The type of the variable
 * determines the width of the read. use types from stdint.h. Up to 64 bits
 * supported. */
#define LITTLE_ENDIAN_LOAD(p, buffer)             \
    do {                                          \
        *p = 0;                                   \
        for (int i = 0; i < sizeof(*p); i++) {    \
            *p |= (uint64_t)buffer[i] << (i * 8); \
        }                                         \
    } while (0)

#endif
