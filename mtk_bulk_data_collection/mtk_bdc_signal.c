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

#include <mira.h>
#include <string.h>

#include "mtk_bulk_data_collection.h"
#include "mtk_bdc_events.h"
#include "mtk_bdc_signal.h"

#define DEBUG_LEVEL 0
#include "mtk_bdc_utils.h"

process_event_t event_bdc_signaled_ready;

static const uint8_t lpsig_header[MTK_BULK_DATA_COLLECTION_HEADER_SIZE] = { 0x54, 0xab };

static mira_net_udp_connection_t* lpsig_udp_connection;

static void lpsig_pack_buffer(uint8_t* buffer, uint16_t packet_id, uint8_t n_sub_packets);

static int lpsig_unpack_buffer(uint8_t* n_sub_packets,
                               uint16_t* packet_id,
                               const uint8_t* buffer,
                               uint8_t len);

int mtk_bdcsig_init(mira_net_udp_connection_t* udp_connection)
{
    event_bdc_signaled_ready = process_alloc_event();

    lpsig_udp_connection = udp_connection;

    return 0;
}

int mtk_bdcsig_send(const mira_net_address_t* dst, uint16_t packet_id, uint8_t n_sub_packets)
{
    uint8_t packet_ready_message[sizeof(lpsig_header) + sizeof(packet_id) + sizeof(n_sub_packets)];

#if DEBUG_LEVEL > 0
    char addr_str_buffer[MIRA_NET_MAX_ADDRESS_STR_LEN];
#endif
    P_DEBUG("Sending lp signal to %s: id %d, %d sub-packets\n",
            mira_net_toolkit_format_address(addr_str_buffer, dst),
            packet_id,
            n_sub_packets);

    lpsig_pack_buffer(packet_ready_message, packet_id, n_sub_packets);

    mira_status_t ret;
    ret = mira_net_udp_send_to(lpsig_udp_connection,
                               dst,
                               MTK_BULK_DATA_COLLECTION_RX_UDP_PORT,
                               packet_ready_message,
                               sizeof(packet_ready_message));

    if (ret != MIRA_SUCCESS) {
        P_ERR("[%d]: mira_net_udp_send_to\n", ret);
        return -1;
    }

    return 0;
}

void mtk_bdcsig_handle_data(const void* data,
                            const uint16_t data_len,
                            const mira_net_udp_callback_metadata_t* metadata)
{
    if (data_len < MTK_BULK_DATA_COLLECTION_HEADER_SIZE) {
        P_ERR("%s: packet too short\n", __func__);
        return;
    }

    if (memcmp(data, lpsig_header, sizeof(lpsig_header)) != 0) {
        /* Not a signal packet */
        return;
    }

    /* Beside keeping track of packet_id, this function should do it by message
    source (metadata->source_address). Failing to do so results in mixing up two
    messages with the same packet_id but from different sources. */

    uint8_t n_sub_packets;
    uint16_t packet_id;
    if (lpsig_unpack_buffer(&n_sub_packets, &packet_id, data, data_len) < 0) {
        P_ERR("Invalid notification\n");
        return;
    }

    P_DEBUG("Signal received for packet id %d with %d sub-packets\n", packet_id, n_sub_packets);

    /* Post event with data */
    static mtk_bdc_event_signaled_data_t lpsig_event_data;
    lpsig_event_data = (mtk_bdc_event_signaled_data_t){
        .n_sub_packets = n_sub_packets,
        .packet_id = packet_id,
        .src_port = metadata->source_port,
    };
    memcpy(&lpsig_event_data.src, metadata->source_address, sizeof(mira_net_address_t));

    if (process_post(PROCESS_BROADCAST, event_bdc_signaled_ready, &lpsig_event_data) !=
        PROCESS_ERR_OK) {
        P_ERR("%s: process_post\n", __func__);
        return;
    }
}

/* Large packet signal format:
 *
 *  +-------------------+----------------------+------------------------+
 *  | header  (16 bits) |  packet_id (16_bits) | n_sub_packets (8 bits) |
 *  +-------------------+----------------------+------------------------+
 *
 * Little endian.
 */

static void lpsig_pack_buffer(uint8_t* buffer, uint16_t packet_id, uint8_t n_sub_packets)
{
    memcpy(buffer, lpsig_header, sizeof(lpsig_header));
    buffer += sizeof(lpsig_header);

    LITTLE_ENDIAN_STORE(buffer, packet_id);
    buffer += sizeof(packet_id);

    LITTLE_ENDIAN_STORE(buffer, n_sub_packets);
    buffer += sizeof(n_sub_packets);
}

static int lpsig_unpack_buffer(uint8_t* n_sub_packets,
                               uint16_t* packet_id,
                               const uint8_t* buffer,
                               uint8_t len)
{
    if ((n_sub_packets == NULL) || (packet_id == NULL) || (buffer == NULL)) {
        P_ERR("%s: pointer error!\n", __func__);
        return -1;
    }

    if (len != (sizeof(lpsig_header) + sizeof(*n_sub_packets) + sizeof(*packet_id))) {
        P_ERR("%s: wrong lp signal packet size (%d)!\n", __func__, len);
        return -1;
    }

    buffer += sizeof(lpsig_header);

    LITTLE_ENDIAN_LOAD(packet_id, buffer);
    buffer += sizeof(*packet_id);

    LITTLE_ENDIAN_LOAD(n_sub_packets, buffer);
    buffer += sizeof(*n_sub_packets);

    return 0;
}
