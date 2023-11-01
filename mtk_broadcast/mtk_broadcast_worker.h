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

#ifndef MTK_BROADCAST_WORKER_H
#define MTK_BROADCAST_WORKER_H

#include <mira.h>
#include "mtk_trickle_timer.h"

typedef void (*mtk_int_broadcast_worker_callback_t)(
  uint32_t data_id,
  void* data,
  uint32_t size,
  const mira_net_udp_callback_metadata_t* metadata,
  void* storage);

typedef struct mtk_int_broadcast_worker
{
    struct mtk_int_broadcast_worker* next;

    uint32_t id;
    uint32_t version;

    void* data;
    uint32_t size;

    struct mtk_trickle_timer timer;

    mtk_int_broadcast_worker_callback_t update_handler;
    void* storage;
} mtk_int_broadcast_worker_t;

/**
 * @brief Initialize broadcast worker
 * @note Used internally by mtk_broadcast and should not be called directly.
 *
 * @param broadcast_addr
 * @param broadcast_port
 * @return int
 */
int mtk_int_broadcast_worker_init_net(mira_net_address_t* broadcast_addr, uint16_t broadcast_port);

/**
 * @brief Register a broadcast session
 * @note Used internally by mtk_broadcast and should not be called directly.
 *
 * @param ctx
 * @param id
 * @param data
 * @param size
 * @param update_handler
 * @param storage
 * @return int
 */
int mtk_int_broadcast_worker_register(mtk_int_broadcast_worker_t* ctx,
                                      uint32_t id,
                                      void* data,
                                      uint32_t size,
                                      mtk_int_broadcast_worker_callback_t update_handler,
                                      void* storage);

/**
 * @brief Update broadcasted data
 * @note Used internally by mtk_broadcast and should not be called directly.
 *
 * @param ctx
 * @param data
 * @param size
 * @return int
 */
int mtk_int_broadcast_worker_update(mtk_int_broadcast_worker_t* ctx, void* data, uint32_t size);

/**
 * @brief Pause a broadcast
 * @note Used internally by mtk_broadcast and should not be called directly.
 *
 * @param ctx
 * @return int
 */
int mtk_int_broadcast_worker_pause(mtk_int_broadcast_worker_t* ctx);

/**
 * @brief Resume a paused broadcast
 * @note Used internally by mtk_broadcast and should not be called directly.
 *
 * @param ctx
 * @return int
 */
int mtk_int_broadcast_worker_resume(mtk_int_broadcast_worker_t* ctx);

#endif
