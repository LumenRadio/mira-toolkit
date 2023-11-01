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

#ifndef MTK_BROADCAST_H
#define MTK_BROADCAST_H

#include <mira.h>

typedef enum
{
    MTK_BROADCAST_SUCCESS = 0,
    MTK_BROADCAST_ERROR_NO_MEMORY,
    MTK_BROADCAST_ERROR_INTERNAL,
    MTK_BROADCAST_ERROR_NOT_INITIALIZED,

} mtk_broadcast_status_t;

/**
 * @brief Definition of callback function called on incoming update
 */
typedef void (*mtk_broadcast_callback_t)(uint32_t data_id,
                                         void* data,
                                         mira_size_t size,
                                         const mira_net_udp_callback_metadata_t* metadata,
                                         void* storage);

/**
 * @brief Initialize the broadcast backend. Shall be called before mtk_broadcast_register()
 *
 * @param broadcast_addr    The link-local multicast address used for broadcast.
 * Format of the link-local multicast address should be as specified in
 * [Mira
 * documentation](https://docs.lumenrad.io/miraos/2.8.1/api/common/net/mira_net_udp.html#mira_net_udp_multicast_group_join)
 * @param broadcast_udp_port The udp port to send and receive broadcast packets on.
 * @return mtk_broadcast_status_t
 * @retval MTK_BROADCAST_SUCCESS
 * @retval MTK_BROADCAST_ERROR_INTERNAL, internal error in worker.
 */
mtk_broadcast_status_t mtk_broadcast_init(mira_net_address_t* broadcast_addr,
                                          uint16_t broadcast_udp_port);

/**
 * @brief Register a new data set to distribute over the network
 *
 * @param data_id        Unique identifier for broadcasted data
 * @param data           Storage of data to distribute
 * @param size           Size of broadcasted data, max 230
 * @param update_handler Function called on incoming update
 * @param storage        Generic storage of data which may be
 *                       accessed in the callback function
 *
 * @return Status of the operation
 * @retval MTK_BROADCAST_SUCCESS
 * @retval MTK_BROADCAST_ERROR_NO_MEMORY, no place for new service context.
 * @retval MTK_BROADCAST_ERROR_INTERNAL, internal error in worker.
 */
mtk_broadcast_status_t mtk_broadcast_register(uint32_t data_id,
                                              void* data,
                                              mira_size_t size,
                                              mtk_broadcast_callback_t update_handler,
                                              void* storage);

/**
 * @brief Update the broadcasted data with new content
 *
 * @param data_id Unique identifier for broadcasted data
 * @param data    Storage of broadcasted data
 * @param size    New size of broadcasted data
 *
 * @return Status of the operation
 * @retval MTK_BROADCAST_SUCCESS
 * @retval MTK_BROADCAST_ERROR_NOT_INITIALIZED, no broadcast service with that data_id.
 * @retval MTK_BROADCAST_ERROR_INTERNAL, internal error in worker.
 */
int mtk_broadcast_update(uint32_t data_id, void* data, mira_size_t size);

/**
 * @brief Pause a running broadcast, preventing sending and receiving updates
 *
 * @param data_id Unique identifier fo broadcasted data
 *
 * @return Status of the operation
 * @retval MTK_BROADCAST_SUCCESS
 * @retval MTK_BROADCAST_ERROR_NOT_INITIALIZED, no broadcast service with that data_id.
 * @retval MTK_BROADCAST_ERROR_INTERNAL, internal error in worker.
 */
int mtk_broadcast_pause(uint32_t data_id);

/**
 * @brief Resume a paused broadcast, preventing sending and receiving updates
 *
 * @param data_id Unique identifier for broadcasted data
 *
 * @return Status of the operation
 * @retval MTK_BROADCAST_SUCCESS
 * @retval MTK_BROADCAST_ERROR_NOT_INITIALIZED, no broadcast service with that data_id.
 * @retval MTK_BROADCAST_ERROR_INTERNAL, internal error in worker.
 */
int mtk_broadcast_resume(uint32_t data_id);

#endif
