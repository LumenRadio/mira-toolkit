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
#include "mtk_broadcast_worker.h"
#include "mtk_trickle_timer.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define MTK_INT_BROADCAST_TRICKLE_IMIN (CLOCK_SECOND / 8)
#define MTK_INT_BROADCAST_TRICKLE_IMAX 6
#define MTK_INT_BROADCAST_TRICKLE_K 3

#define MTK_INT_BROADCAST_VERSION_INCREMENT 0x10000

#define DEBUG 1

#if DEBUG
#define P_INFO_DS_W(...) printf(__VA_ARGS__)
#define P_DEBUG_DS_W(...) printf(__VA_ARGS__)
#else
#define P_INFO_DS_W(...)
#define P_DEBUG_DS_W(...)
#endif

/*
 * Send to link-local all-nodes multicast [ff02:3f00:1]
 */
static mira_net_address_t broadcast_dest_addr = { .u8 = { 0 } };
static uint16_t broadcast_udp_port = 0;

static mtk_int_broadcast_worker_t* broadcast_ctx_base = NULL;

static int broadcast_net_initialized = 0;
static mira_net_udp_connection_t* udp_connection;

static void broadcast_udp_callback(mira_net_udp_connection_t* connection,
                                   const void* data,
                                   uint16_t data_len,
                                   const mira_net_udp_callback_metadata_t* metadata,
                                   void* storage)
{
    uint32_t id;
    uint32_t version;
    mtk_int_broadcast_worker_t* ctx;
    const uint8_t* data_u8 = (uint8_t*)data;

    if (data_len < 8) {
        P_DEBUG_DS_W("UDP input: short packet\n");
        return;
    }

    id = ((uint32_t)data_u8[0]) << 0 | ((uint32_t)data_u8[1]) << 8 | ((uint32_t)data_u8[2]) << 16 |
         ((uint32_t)data_u8[3]) << 24;
    version = ((uint32_t)data_u8[4]) << 0 | ((uint32_t)data_u8[5]) << 8 |
              ((uint32_t)data_u8[6]) << 16 | ((uint32_t)data_u8[7]) << 24;

    ctx = broadcast_ctx_base;
    while (ctx != NULL && ctx->id != id) {
        ctx = ctx->next;
    }

    if (ctx == NULL) {
        /*
         * If there is a packet from an unknown id, it doesn't really mean that
         * there is a problem, just that this node doesn't listen for that state
         * id.
         */
        P_DEBUG_DS_W("%08lx @ %9lu: UDP input from unknown id, discard\n", id, version);
        return;
    }

    if (ctx->timer.i_cur == MTK_TRICKLE_TIMER_IS_STOPPED) {
        P_DEBUG_DS_W("%08lx @ %9lu: UDP input to paused id, ignore\n", id, version);
        return;
    }

    if ((int32_t)(version - ctx->version) > 0) {
        /* If there version is newer, update and register inconsistency */
        P_DEBUG_DS_W(
          "%08lx @ %9lu: UDP input of newer version (old = %lu)\n", ctx->id, version, ctx->version);

        ctx->version = version;
        ctx->size = data_len - 8;

        memcpy(ctx->data, data + 8, ctx->size);
        mtk_trickle_timer_inconsistency(&ctx->timer);

        /* Updated version, call handler */
        ctx->update_handler(ctx->id, ctx->data, ctx->size, metadata, ctx->storage);
    } else if (version != ctx->version) {
        P_DEBUG_DS_W(
          "%08lx @ %9lu: UDP input of older version (old = %lu)\n", ctx->id, ctx->version, version);
        /* If there version is older, keep and register inconsistency */
        mtk_trickle_timer_inconsistency(&ctx->timer);
    } else {
        P_DEBUG_DS_W("%08lx @ %9lu: UDP input of same version\n", ctx->id, ctx->version);
        /* If the versions are the same, register consistency */
        mtk_trickle_timer_consistency(&ctx->timer);
    }
}

static void broadcast_trickle_callback(void* ptr, uint8_t supress)
{
    mtk_int_broadcast_worker_t* ctx = (mtk_int_broadcast_worker_t*)ptr;

    uint8_t buf[256];

    if (ctx->version == 0) {
        P_DEBUG_DS_W("%08lx @ %9lu: Trickle tick - uninitialized, skip\n", ctx->id, ctx->version);
        return;
    }

    P_DEBUG_DS_W("%08lx @ %9lu: Trickle tick - sending\n", ctx->id, ctx->version);

    buf[0] = (ctx->id >> 0) & 0xff;
    buf[1] = (ctx->id >> 8) & 0xff;
    buf[2] = (ctx->id >> 16) & 0xff;
    buf[3] = (ctx->id >> 24) & 0xff;
    buf[4] = (ctx->version >> 0) & 0xff;
    buf[5] = (ctx->version >> 8) & 0xff;
    buf[6] = (ctx->version >> 16) & 0xff;
    buf[7] = (ctx->version >> 24) & 0xff;

    memcpy(buf + 8, ctx->data, ctx->size);

    /* Don't send if we are not joined to the network */
    if (mira_net_get_state() != MIRA_NET_STATE_NOT_ASSOCIATED) {
        if (mira_net_udp_send_to(
              udp_connection, &broadcast_dest_addr, broadcast_udp_port, buf, 8 + ctx->size) !=
            MIRA_SUCCESS) {
            P_INFO_DS_W("%s: mira_net_udp_send() fail\n", __func__);
        }
    }
}

int mtk_int_broadcast_worker_init_net(mira_net_address_t* broadcast_addr, uint16_t broadcast_port)
{
    if (broadcast_net_initialized) {
        return 0;
    }

    mira_net_toolkit_copy_address(&broadcast_dest_addr, broadcast_addr);
    broadcast_udp_port = broadcast_port;

    udp_connection = mira_net_udp_bind_address(&broadcast_dest_addr,
                                               NULL,
                                               broadcast_udp_port,
                                               broadcast_udp_port,
                                               broadcast_udp_callback,
                                               NULL);
    if (udp_connection == NULL) {
        P_DEBUG_DS_W("ERROR: %s: mira_net_udp_bind_address()\n", __func__);
        return -1;
    }

    if (mira_net_udp_multicast_group_join(udp_connection, &broadcast_dest_addr) != MIRA_SUCCESS) {
        P_DEBUG_DS_W("ERROR: %s: mira_net_udp_multicast_group_join()\n", __func__);
        mira_net_udp_close(udp_connection);
        return -1;
    }

    broadcast_net_initialized = 1;

    P_DEBUG_DS_W("Initialized broadcast worker\n");
    return 0;
}

int mtk_int_broadcast_worker_register(mtk_int_broadcast_worker_t* ctx,
                                      uint32_t id,
                                      void* data,
                                      uint32_t size,
                                      mtk_int_broadcast_worker_callback_t update_handler,
                                      void* storage)
{
    if (ctx == NULL) {
        P_INFO_DS_W("ERROR: %s: Got null pointer\n", __func__);
        return -1;
    }

    if (!broadcast_net_initialized) {
        P_INFO_DS_W("ERROR: Broadcast worker not initialized\n");
        return -1;
    }

    ctx->id = id;
    ctx->version = 0;

    ctx->data = data;
    ctx->size = size;

    ctx->update_handler = update_handler;
    ctx->storage = storage;

    mtk_trickle_timer_config(&ctx->timer,
                             MTK_INT_BROADCAST_TRICKLE_IMIN,
                             MTK_INT_BROADCAST_TRICKLE_IMAX,
                             MTK_INT_BROADCAST_TRICKLE_K);

    mtk_trickle_timer_set(&ctx->timer, broadcast_trickle_callback, ctx);

    /* Add to list */
    ctx->next = broadcast_ctx_base;
    broadcast_ctx_base = ctx;

    P_DEBUG_DS_W("%08lx @ %9lu: Register\n", ctx->id, ctx->version);
    return 0;
}

int mtk_int_broadcast_worker_update(mtk_int_broadcast_worker_t* ctx, void* data, uint32_t size)
{
    if (ctx == NULL) {
        P_INFO_DS_W("ERROR: %s: Got null pointer\n", __func__);
        return -1;
    }

    memcpy(ctx->data, data, size);
    ctx->size = size;

    /*
     * Increment state. Make sure it increments, and reduce risk of collisions
     * (randomize)
     */
    ctx->version += MTK_INT_BROADCAST_VERSION_INCREMENT;
    ctx->version += mira_random_generate() % MTK_INT_BROADCAST_VERSION_INCREMENT;
    if (ctx->version == 0) {
        /* Keep version == 0 as uninitialized/no propagataion */
        ctx->version = 1;
    }
    P_DEBUG_DS_W("%08lx @ %9lu: Local update\n", ctx->id, ctx->version);
    mtk_trickle_timer_reset_event(&ctx->timer);

    return 0;
}

int mtk_int_broadcast_worker_pause(mtk_int_broadcast_worker_t* ctx)
{
    if (ctx == NULL || ctx->id == 0) {
        P_INFO_DS_W("Can not pause uninitialized broadcast\n");
        return -1;
    }

    mtk_trickle_timer_stop(&ctx->timer);
    P_DEBUG_DS_W("%08lx @ %9lu: Paused\n", ctx->id, ctx->version);
    return 0;
}

int mtk_int_broadcast_worker_resume(mtk_int_broadcast_worker_t* ctx)
{
    if (ctx == NULL || ctx->id == 0) {
        P_INFO_DS_W("Can not resume uninitialized broadcast\n");
        return -1;
    }
    if (ctx->timer.i_cur != MTK_TRICKLE_TIMER_IS_STOPPED) {
        P_INFO_DS_W("Can not resume a broadcast that is not paused\n");
        return -1;
    }

    mtk_trickle_timer_set(&ctx->timer, broadcast_trickle_callback, ctx);

    P_DEBUG_DS_W("%08lx @ %9lu: Resumed\n", ctx->id, ctx->version);
    return 0;
}
