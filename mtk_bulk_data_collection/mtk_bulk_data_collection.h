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

#ifndef MTK_BULK_DATA_COLLECTION_H
#define MTK_BULK_DATA_COLLECTION_H

#include <mira.h>

#include <stdbool.h>
#include <stdint.h>

/* Open port receiver for signals */
#define MTK_BULK_DATA_COLLECTION_RX_UDP_PORT (1520)

/* Size of single frames to split the large packet into.  This size may be
 * larger than max payload for a single radio packet, in which case Mira
 * (6LoWPAN) divides the sub-packet into fragments. This has the advantage of
 * reducing overhead, at the cost of possible increase of number
 * re-transmissions. */
#define MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES (330)

/* Max number of messages into which a large packet may be split */
/* The bit mask sent in requests must be large enough to accommodate for this
 * number of sub-packets. */
#define MTK_BULK_DATA_COLLECTION_MAX_NUMBER_OF_SUBPACKETS (64)

/* Byte size of headers, which determines the type of message. */
#define MTK_BULK_DATA_COLLECTION_HEADER_SIZE (2)

/* Only one of two roles currently supported */
typedef enum
{
    MTK_BULK_DATA_COLLECTION_RECEIVER,
    MTK_BULK_DATA_COLLECTION_SENDER,
} mtk_bulk_data_collection_role_t;

/* Type used both on the receiving and the sending nodes */
typedef struct
{
    uint8_t* payload;
    uint16_t len;
    /* Address and port to the other node participating in the communication */
    mira_net_address_t node_addr;
    uint16_t node_port;
    uint16_t id;
    uint16_t period_ms;
    uint64_t mask; /* bit 1 for sub-packets to send, or received */
    uint8_t num_sub_packets;
} mtk_bulk_data_collection_packet_t;

int mtk_bulk_data_collection_init(mtk_bulk_data_collection_role_t role);

/* Get the mask for requesting all sub-packets */
int mtk_bulk_data_collection_send_whole_mask_get(uint64_t* mask, const uint16_t n_sub_packets);

/* Get number of sub-packets that make up a large packet of size n_bytes. */
uint8_t mtk_bulk_data_collection_n_sub_packets_get(const uint16_t n_bytes);

/* Register the data to send. Transmission occurs only when requested by a
 * receiver. */
int mtk_bulk_data_collection_register_tx(mtk_bulk_data_collection_packet_t* packet,
                                         const uint16_t packet_id,
                                         const uint8_t* payload,
                                         const uint16_t len);

/* Send the registered large packet. */
int mtk_bulk_data_collection_send(mtk_bulk_data_collection_packet_t* packet);

/* Request sub-packets from dst, only the sub-packets defined by sub_packet_mask
 * bit at 1. */
int mtk_bulk_data_collection_request(const mira_net_address_t* dst,
                                     const uint64_t sub_packet_mask,
                                     const uint16_t sub_packet_period_ms);

/* Start this process upon sending requests, to handle reception. */
PROCESS_NAME(mtk_bulk_data_collection_receive_proc);

#endif
