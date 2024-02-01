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

#ifndef MTK_BDC_REQUEST_H
#define MTK_BDC_REQUEST_H

/* Function identifier prefix: lpreq_ */

#include <stdint.h>

#include "mtk_bulk_data_collection.h"

int mtk_bdcreq_init(mira_net_udp_connection_t* udp_connection);

/* Send a request for large packet */
int mtk_bdcreq_send(const mira_net_address_t* dst,
                    const uint16_t port,
                    const uint16_t packet_id,
                    const uint64_t sub_packet_mask,
                    const uint16_t sub_packet_period_ms);

/* Handle incoming data, if relevant. This function first tests if the data is a
 * valid request message. If it is, it acts by posting an event. */
void mtk_bdcreq_handle_data(const void* data,
                            const uint16_t data_len,
                            const mira_net_udp_callback_metadata_t* metadata);

#endif
