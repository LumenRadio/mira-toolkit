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

#ifndef MTK_BDC_EVENTS_H
#define MTK_BDC_EVENTS_H

/* Event: received a notification for large packet available */
extern process_event_t event_bdc_signaled_ready;
typedef struct
{
    uint8_t n_sub_packets;
    uint16_t packet_id;
    mira_net_address_t src;
    uint16_t src_port;
} mtk_bdc_event_signaled_data_t;

/* Event: received a request for large packet, with selected sub-packets */
extern process_event_t event_bdc_requested;
typedef struct
{
    uint16_t packet_id;
    uint64_t mask;
    uint16_t period_ms;
    /* source and port of the request, used as destination for large packet */
    mira_net_address_t src;
    uint16_t src_port;
} mtk_bdc_event_requested_data_t;

/* Event: received a sub-packet */
extern process_event_t event_bdc_subpacket_received;
typedef struct
{
    uint16_t packet_id;
    uint8_t sub_packet_index;
    uint8_t n_sub_packets;
    uint16_t payload_len;
    uint8_t* payload;
    mira_net_address_t src;
    uint16_t src_port;
} mtk_bdc_event_subpacket_data_t;

/* Event: received a large packet */
extern process_event_t event_bdc_received;

#endif
