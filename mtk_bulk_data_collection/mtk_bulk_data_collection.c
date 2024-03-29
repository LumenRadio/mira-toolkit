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

#include <limits.h>
#include <mira.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "mtk_bulk_data_collection.h"
#include "mtk_bdc_events.h"
#include "mtk_bdc_request.h"
#include "mtk_bdc_signal.h"
#include "mtk_bdc_subpacket.h"

#define DEBUG_LEVEL 0
#include "mtk_bdc_utils.h"

process_event_t event_bdc_received;

typedef struct
{
    uint8_t index; /* placement of sub-packet in large packet */
    uint16_t len;
    uint8_t const* payload;
} sub_packet_t;

/* Max number of times to request re-transmission of missing sub-packets. */
#define LP_MAX_NUM_RETRANSMISSION_REQUESTS (4)

/* Inject faults for testing re-transmissions */
#ifndef FAULT_RATE_PERCENT
#define FAULT_RATE_PERCENT (0)
#endif

static mira_net_udp_connection_t* large_packet_udp_connection;
static bool large_packet_currently_sending = false;

PROCESS(mtk_bulk_data_collection_send_proc, "Sending of large packets");
PROCESS(mtk_bulk_data_collection_receive_proc, "Receive sub-packets for large packet");

static void request_for_missing_subpackets(const mtk_bulk_data_collection_packet_t* lp);

static void large_packet_udp_listen_callback(mira_net_udp_connection_t* connection,
                                             const void* data,
                                             uint16_t data_len,
                                             const mira_net_udp_callback_metadata_t* metadata,
                                             void* storage);

static int next_sub_packet_send(mtk_bulk_data_collection_packet_t* large_packet);

static sub_packet_t pick_next_to_send(const mtk_bulk_data_collection_packet_t* lp);

static bool lp_fault_injected(void);

int mtk_bulk_data_collection_init(mtk_bulk_data_collection_role_t role)
{
    if (large_packet_udp_connection != NULL) {
        /* Error is acceptable here, since the UDP connection probably doesn't
           exist yet. */
        (void)mira_net_udp_close(large_packet_udp_connection);
    }

    if (role == MTK_BULK_DATA_COLLECTION_RECEIVER) {
        large_packet_udp_connection = mira_net_udp_listen(
          MTK_BULK_DATA_COLLECTION_RX_UDP_PORT, large_packet_udp_listen_callback, NULL);
    } else {
        /* Destination address and port unknown at init. Need to use
         * mira_net_udp_send_to(), after getting destination info (joining
         * network). */
        large_packet_udp_connection =
          mira_net_udp_connect(NULL, 0, large_packet_udp_listen_callback, NULL);
    }

    if (large_packet_udp_connection == NULL) {
        P_ERR("%s: Could not open UDP connection\n", __func__);
        return -1;
    }

    if (mtk_bdcsig_init(large_packet_udp_connection) < 0) {
        P_ERR("%s: mtk_bdcsig_init\n", __func__);
        return -1;
    }
    if (mtk_bdcreq_init(large_packet_udp_connection) < 0) {
        P_ERR("%s: mtk_bdcreq_init\n", __func__);
        return -1;
    }
    if (mtk_bdcsp_init(large_packet_udp_connection) < 0) {
        P_ERR("%s: mtk_bdcsp_init\n", __func__);
        return -1;
    }

    large_packet_currently_sending = false;

    event_bdc_received = process_alloc_event();

    return 0;
}

int mtk_bulk_data_collection_send_whole_mask_get(uint64_t* mask, const uint16_t n_sub_packets)
{
    if (n_sub_packets > MTK_BULK_DATA_COLLECTION_MAX_NUMBER_OF_SUBPACKETS) {
        *mask = 0;
        return -1;
    }

    /* Use hard-coded value since it's coupled to the variable type. */
    if (n_sub_packets == 64) {
        *mask = UINT64_MAX;
    } else {
        *mask = (((uint64_t)1) << n_sub_packets) - 1;
    }

    return 0;
}

uint8_t mtk_bulk_data_collection_n_sub_packets_get(const uint16_t n_bytes)
{
    div_t d = div(n_bytes, MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES);
    return d.quot + ((d.rem > 0) ? 1 : 0);
}

int mtk_bulk_data_collection_register_tx(mtk_bulk_data_collection_packet_t* large_packet,
                                         const uint16_t packet_id,
                                         const uint8_t* payload,
                                         const uint16_t len)
{
    if (payload == NULL || len == 0) {
        return -1;
    }
    if (len > (MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES *
               MTK_BULK_DATA_COLLECTION_MAX_NUMBER_OF_SUBPACKETS)) {
        P_ERR("%s: ! packet too large\n", __func__);
        return -1;
    }

    large_packet->payload = (uint8_t*)payload;
    large_packet->len = len;
    large_packet->id = packet_id;

    div_t d = div(len, MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES);
    large_packet->num_sub_packets = d.quot + ((d.rem != 0) ? 1 : 0);

    /* Assuming chars, and more than 10 of them */
    P_DEBUG("Registered for transmission: packet %d, len %d, num_sub_packets %d. Content start: "
            "\"%.10s...\n",
            packet_id,
            len,
            large_packet->num_sub_packets,
            payload);

    return 0;
}

int mtk_bulk_data_collection_send(mtk_bulk_data_collection_packet_t* large_packet)
{
    if (large_packet_currently_sending) {
        P_DEBUG("Large packet sending requested while not available\n");
        return -1;
    }

    /* Kill possibly running sending before starting anew. */
    process_exit(&mtk_bulk_data_collection_send_proc);
    process_start(&mtk_bulk_data_collection_send_proc, large_packet);

    return 0;
}

PROCESS_THREAD(mtk_bulk_data_collection_receive_proc, ev, data)
{
    PROCESS_BEGIN();

    static struct etimer timeout_timer;
    static mtk_bulk_data_collection_packet_t* lp;
    static bool rx_done;
    static int re_tx_requests_left;

    lp = (mtk_bulk_data_collection_packet_t*)data;
    re_tx_requests_left = LP_MAX_NUM_RETRANSMISSION_REQUESTS;

    rx_done = false;

    while (!rx_done) {
        uint32_t timeout_ticks = 10 * lp->period_ms * CLOCK_SECOND / 1000;
        etimer_set(&timeout_timer, timeout_ticks);

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timeout_timer) ||
                                 ev == event_bdc_subpacket_received);

        if (etimer_expired(&timeout_timer)) {
            P_DEBUG("%s: timed out while receiving sub-packets\n", __func__);
            if (re_tx_requests_left > 0) {
                request_for_missing_subpackets(lp);
                re_tx_requests_left--;
            } else {
                P_DEBUG("%s: max number of re-transmission requests reached (%d). Abort.\n",
                        __func__,
                        LP_MAX_NUM_RETRANSMISSION_REQUESTS);
                PROCESS_EXIT();
            }
        } else if (ev == event_bdc_subpacket_received) {
            mtk_bdc_event_subpacket_data_t* ed = (mtk_bdc_event_subpacket_data_t*)data;

            if (lp_fault_injected()) {
                P_DEBUG("%s: simulate packet loss by discarding sub-packet %d\n",
                        __func__,
                        ed->sub_packet_index);
                continue;
            }

            if (ed->packet_id != lp->id) {
                P_DEBUG("%s: received sub-packet with id %d, expected %d\n",
                        __func__,
                        ed->packet_id,
                        lp->id);
                printf("Got wrong subpacket\n");
                PROCESS_EXIT();
            }

            /* Add checking of address and port here, especially if
             * implementing multiple parallel transactions.  */

            uint64_t sub_packet_received_mask_bit = ((uint64_t)1) << ed->sub_packet_index;

            if (lp->mask & sub_packet_received_mask_bit) {
                P_DEBUG("Duplicate sub-packet received\n");
                continue;
            }

            lp->mask |= sub_packet_received_mask_bit;

            uint16_t offset_in_dst_payload =
              ed->sub_packet_index * MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES;

            memcpy(lp->payload + offset_in_dst_payload, ed->payload, ed->payload_len);
            lp->len += ed->payload_len;

            uint64_t all_done_mask =
              (ed->n_sub_packets == 64) ? UINT64_MAX : (((uint64_t)1) << ed->n_sub_packets) - 1;
            rx_done = lp->mask == all_done_mask;
        }
    }

    if (process_post(PROCESS_BROADCAST, event_bdc_received, NULL) != PROCESS_ERR_OK) {
        P_ERR("%s: process_post event_bdc_received\n", __func__);
    }

    PROCESS_END();
}

PROCESS_THREAD(mtk_bulk_data_collection_send_proc, ev, data)
{
    static struct etimer timer;
    static int sub_packet_send_status;
    static mtk_bulk_data_collection_packet_t* large_packet;

    PROCESS_BEGIN();

    large_packet = (mtk_bulk_data_collection_packet_t*)data;

    large_packet_currently_sending = true;
    sub_packet_send_status = 0; /* >= 0 means OK */

    P_DEBUG("Start of large packet transmission (@%d ms), mask 0x%08" PRIu32 "%08" PRIu32 "\n",
            large_packet->period_ms,
            (uint32_t)(large_packet->mask >> 32),
            (uint32_t)(large_packet->mask & (UINT32_MAX)));

    while (large_packet->mask != 0 && sub_packet_send_status >= 0) {
        sub_packet_send_status = next_sub_packet_send(large_packet);
        etimer_set(&timer, large_packet->period_ms * CLOCK_SECOND / 1000);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    }

    P_DEBUG("Large packet sent: %s\n", (sub_packet_send_status >= 0) ? "OK" : "Failed");

    large_packet_currently_sending = false;

    PROCESS_END();
}

static void request_for_missing_subpackets(const mtk_bulk_data_collection_packet_t* lp)
{
    uint64_t received_mask = lp->mask;

    uint64_t new_request_mask = received_mask ^ UINT64_MAX;

    if (lp->num_sub_packets < 64) {
        new_request_mask &= (((uint64_t)1) << lp->num_sub_packets) - 1;
    }

    RUN_CHECK(
      mtk_bdcreq_send(&lp->node_addr, lp->node_port, lp->id, new_request_mask, lp->period_ms));
}

static int next_sub_packet_send(mtk_bulk_data_collection_packet_t* large_packet)
{
    if (large_packet_udp_connection == NULL) {
        P_ERR("%s: no UDP connection!\n", __func__);
        return -1;
    }

    sub_packet_t sub_packet = pick_next_to_send(large_packet);

    if (sub_packet.len > MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES) {
        P_ERR("%s: sub-packet too large! (%d > %d)\n",
              __func__,
              sub_packet.len,
              MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES);
        return -1;
    }

    int ret = mtk_bdcsp_send(&large_packet->node_addr,
                             large_packet->node_port,
                             large_packet->id,
                             sub_packet.index,
                             large_packet->num_sub_packets,
                             sub_packet.payload,
                             sub_packet.len);

    if (ret >= 0) {
        large_packet->mask &= ~(((uint64_t)1) << sub_packet.index);
    } else {
        P_ERR("%s: could not send sub-packet\n", __func__);
    }

    return ret;
}

static sub_packet_t pick_next_to_send(const mtk_bulk_data_collection_packet_t* lp)
{
    sub_packet_t sp = {
        .payload = NULL,
    };

    for (int i = 0; sp.payload == NULL && i < MTK_BULK_DATA_COLLECTION_MAX_NUMBER_OF_SUBPACKETS &&
                    i < lp->num_sub_packets;
         ++i) {
        if ((((uint64_t)1) << i) & lp->mask) {
            sp.index = i;
            sp.payload = lp->payload + i * MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES;

            if (i == (lp->num_sub_packets - 1)) {
                /* last sub-packet might be smaller than max */
                sp.len = lp->len % MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES;
                if (sp.len == 0) {
                    /* but if last sub-packet is MAX_BYTES long, modulo gives
                     * 0. Set correct length (full length) instead. */
                    sp.len = MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES;
                }
            } else {
                sp.len = MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES;
            }
        }
    }

    return sp;
}

static void large_packet_udp_listen_callback(mira_net_udp_connection_t* connection,
                                             const void* data,
                                             uint16_t data_len,
                                             const mira_net_udp_callback_metadata_t* metadata,
                                             void* storage)
{
#if DEBUG > 0
    char buffer[MIRA_NET_MAX_ADDRESS_STR_LEN];
#endif
    P_DEBUG("Received UDP packet from [%s]:%u, len %d\n",
            mira_net_toolkit_format_address(buffer, metadata->source_address),
            metadata->source_port,
            data_len);

    if (data_len < MTK_BULK_DATA_COLLECTION_HEADER_SIZE) {
        P_ERR("%s: UDP packet too short\n", __func__);
        return;
    }

    mtk_bdcsig_handle_data(data, data_len, metadata);
    mtk_bdcreq_handle_data(data, data_len, metadata);
    mtk_bdcsp_handle_data(data, data_len, metadata);
}

static bool lp_fault_injected(void)
{
    return mira_random_generate() < (FAULT_RATE_PERCENT * UINT16_MAX / 100);
}
