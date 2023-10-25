/*
 * MIT License
 *
 * Copyright (c) 2023 LumenRadio AB
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

#include "mira.h"
#include "mtk_broadcast.h"
#include "mtk_broadcast_worker.h"

#ifndef MTK_BROADCAST_NUM_UNIQUE_BROADCASTS
#define MTK_BROADCAST_NUM_CTX    4
#else
#define MTK_BROADCAST_NUM_CTX MTK_BROADCAST_NUM_UNIQUE_BROADCASTS
#endif

static mtk_int_broadcast_worker_t broadcast_ctx[
    MTK_BROADCAST_NUM_CTX];
static int broadcast_num_ctx = 0;

mtk_broadcast_status_t mtk_broadcast_init(
    mira_net_address_t *broadcast_addr,
    uint16_t broadcast_udp_port)
{
    if (mtk_int_broadcast_worker_init_net(broadcast_addr, broadcast_udp_port) != 0) {
        return MTK_BROADCAST_ERROR_INTERNAL;
    } else {
        return MTK_BROADCAST_SUCCESS;
    }
}

mtk_broadcast_status_t mtk_broadcast_register(
    uint32_t data_id,
    void *data,
    mira_size_t size,
    mtk_broadcast_callback_t update_handler,
    void *storage)
{
    mtk_int_broadcast_worker_t *ctx;
    if (broadcast_num_ctx >= MTK_BROADCAST_NUM_CTX) {
        return MTK_BROADCAST_ERROR_NO_MEMORY;
    }

    ctx = &broadcast_ctx[broadcast_num_ctx];
    broadcast_num_ctx++;

    int status = mtk_int_broadcast_worker_register(
        ctx,
        data_id,
        data,
        size,
        update_handler,
        storage);
    
    if (status != 0) {
        return MTK_BROADCAST_ERROR_INTERNAL;
    } else {
        return MTK_BROADCAST_SUCCESS;
    }
}

int mtk_broadcast_update(
    uint32_t data_id,
    void *data,
    mira_size_t size)
{
    int i;
    mtk_int_broadcast_worker_t *ctx;

    for (i = 0; i < broadcast_num_ctx; i++) {
        ctx = &broadcast_ctx[i];
        if (ctx->id == data_id) {
            int status = mtk_int_broadcast_worker_update(ctx, data, size);
            if (status != 0) {
                return MTK_BROADCAST_ERROR_INTERNAL;
            } else {
                return MTK_BROADCAST_SUCCESS;
            }
        }
    }

    return MTK_BROADCAST_ERROR_NOT_INITIALIZED;
}

int mtk_broadcast_pause(
    uint32_t data_id)
{
    int i;
    mtk_int_broadcast_worker_t *ctx;

    for (i = 0; i < broadcast_num_ctx; i++) {
        ctx = &broadcast_ctx[i];
        if (ctx->id == data_id) {
            int status = mtk_int_broadcast_worker_pause(ctx);
            if (status != 0) {
                return MTK_BROADCAST_ERROR_INTERNAL;
            } else {
                return MTK_BROADCAST_SUCCESS;
            }
        }
    }

    return MTK_BROADCAST_ERROR_NOT_INITIALIZED;
}

int mtk_broadcast_resume(
    uint32_t data_id)
{
    int i;
    mtk_int_broadcast_worker_t *ctx;

    for (i = 0; i < broadcast_num_ctx; i++) {
        ctx = &broadcast_ctx[i];
        if (ctx->id == data_id) {
            int status = mtk_int_broadcast_worker_resume(ctx);
            if (status != 0) {
                return MTK_BROADCAST_ERROR_INTERNAL;
            } else {
                return MTK_BROADCAST_SUCCESS;
            }
        }
    }

    return MTK_BROADCAST_ERROR_NOT_INITIALIZED;
}
