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

#ifndef MTK_BDC_SUBPACKET_H
#define MTK_BDC_SUBPACKET_H

#include <mira.h>
#include <stdint.h>

/* Initialize the module, with UDP setup to send messages. */
int mtk_bdcsp_init(mira_net_udp_connection_t* udp_connection);

/* Send sub-packet to dst */
int mtk_bdcsp_send(const mira_net_address_t* dst,
                   uint16_t dst_port,
                   uint16_t packt_id,
                   uint8_t sub_packet_index,
                   uint8_t n_sub_packets,
                   const uint8_t* data,
                   const uint16_t data_len);

/* Handle incoming data, if relevant. This function first tests if the data is a
 * valid sub-packet message. If it is, it acts by posting an event. */
void mtk_bdcsp_handle_data(const void* data,
                           const uint16_t data_len,
                           const mira_net_udp_callback_metadata_t* metadata);

#endif
