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
#include <stdint.h>
#include <string.h>

#include "mtk_bulk_data_collection.h"
#include "mtk_bdc_events.h"
#include "mtk_bdc_subpacket.h"

#define DEBUG_LEVEL 0
#include "mtk_bdc_utils.h"

process_event_t event_bdc_subpacket_received;

static const uint8_t lpsp_header[MTK_BULK_DATA_COLLECTION_HEADER_SIZE] = { 0x1f, 0xb3 };

static mira_net_udp_connection_t* lpsp_udp_connection;

static void lpsp_pack_buffer(uint8_t* buffer,
                             uint16_t packet_id,
                             uint8_t sub_packet_index,
                             uint8_t n_sub_packets,
                             const uint8_t* payload,
                             uint16_t payload_len);

static int lpsp_unpack_buffer(uint16_t* packet_id,
                              uint8_t* sub_packet_index,
                              uint8_t* n_sub_packets,
                              uint16_t* payload_len,
                              uint8_t* payload,
                              const uint8_t* buffer,
                              uint16_t buf_len);

int mtk_bdcsp_init(mira_net_udp_connection_t* udp_connection)
{
    lpsp_udp_connection = udp_connection;

    event_bdc_subpacket_received = process_alloc_event();

    return 0;
}

int mtk_bdcsp_send(const mira_net_address_t* dst,
                   uint16_t dst_port,
                   uint16_t packet_id,
                   uint8_t sub_packet_index,
                   uint8_t n_sub_packets,
                   const uint8_t* data,
                   const uint16_t data_len)
{
    uint8_t sub_packet_frame[sizeof(lpsp_header) + sizeof(packet_id) + sizeof(sub_packet_index) +
                             sizeof(n_sub_packets) + sizeof(data_len) + data_len];

    lpsp_pack_buffer(sub_packet_frame, packet_id, sub_packet_index, n_sub_packets, data, data_len);

    mira_status_t ret = mira_net_udp_send_to(
      lpsp_udp_connection, dst, dst_port, sub_packet_frame, sizeof(sub_packet_frame));

    if (ret != MIRA_SUCCESS) {
        P_ERR("%s: could not send on UDP\n", __func__);
        return -1;
    }
    return 0;
}

void mtk_bdcsp_handle_data(const void* data,
                           const uint16_t data_len,
                           const mira_net_udp_callback_metadata_t* metadata)
{
    if (data_len < MTK_BULK_DATA_COLLECTION_HEADER_SIZE) {
        P_ERR("%s: packet too short\n", __func__);
        return;
    }

    if (memcmp(data, lpsp_header, sizeof(lpsp_header)) != 0) {
        /* Not a sub-packet */
        return;
    }

    /* Beside keeping track of packet_id, this function should do it by message
    source (metadata->source_address). Failing to do so results in mixing up two
    messages with the same packet_id but from different sources. */

    uint16_t packet_id;
    uint8_t sub_packet_index;
    uint8_t n_sub_packets;
    static uint8_t payload[MTK_BULK_DATA_COLLECTION_SUBPACKET_MAX_BYTES];
    uint16_t payload_len;

    if (lpsp_unpack_buffer(
          &packet_id, &sub_packet_index, &n_sub_packets, &payload_len, payload, data, data_len) <
        0) {
        P_ERR("%s: invalid sub-packet\n", __func__);
        return;
    }

    /* Post event with data */
    static mtk_bdc_event_subpacket_data_t lpsp_event_data;
    lpsp_event_data = (mtk_bdc_event_subpacket_data_t){
        .packet_id = packet_id,
        .sub_packet_index = sub_packet_index,
        .n_sub_packets = n_sub_packets,
        .payload_len = payload_len,
        .payload = payload,
        .src_port = metadata->source_port,
    };
    memcpy(&lpsp_event_data.src, metadata->source_address, sizeof(mira_net_address_t));

    if (process_post(PROCESS_BROADCAST, event_bdc_subpacket_received, &lpsp_event_data) !=
        PROCESS_ERR_OK) {
        P_ERR("%s: process_post\n", __func__);
        return;
    }
}

/* Sub-packet format:
 *
 *  +-------------------+----------------------+---------------------------+
 *  | header  (16 bits) |  packet_id (16_bits) | sub_packet_index (8 bits) | ...
 *  +-------------------+----------------------+------------------------+--+
 *
 *  +----------------------+----------------------+-----------------------------+
 *  n_sub_packets (8 bits) | payload_len (8 bits) | payload (payload_len bytes) |
 *  +----------------------+----------------------+-----------------------------+
 *
 * Little endian.
 */

static void lpsp_pack_buffer(uint8_t* buffer,
                             uint16_t packet_id,
                             uint8_t sub_packet_index,
                             uint8_t n_sub_packets,
                             const uint8_t* payload,
                             uint16_t payload_len)
{
    memcpy(buffer, &lpsp_header, sizeof(lpsp_header));
    buffer += sizeof(lpsp_header);

    LITTLE_ENDIAN_STORE(buffer, packet_id);
    buffer += sizeof(packet_id);

    LITTLE_ENDIAN_STORE(buffer, sub_packet_index);
    buffer += sizeof(sub_packet_index);

    LITTLE_ENDIAN_STORE(buffer, n_sub_packets);
    buffer += sizeof(n_sub_packets);

    LITTLE_ENDIAN_STORE(buffer, payload_len);
    buffer += sizeof(payload_len);

    memcpy(buffer, payload, payload_len);
    buffer += payload_len;
}

static int lpsp_unpack_buffer(uint16_t* packet_id,
                              uint8_t* sub_packet_index,
                              uint8_t* n_sub_packets,
                              uint16_t* payload_len,
                              uint8_t* payload,
                              const uint8_t* buffer,
                              uint16_t buf_len)
{
    if ((packet_id == NULL) || (sub_packet_index == NULL) || (n_sub_packets == NULL) ||
        (payload_len == NULL) || (payload == NULL) || (buffer == NULL)) {
        P_ERR("%s: pointer error!\n", __func__);
        return -1;
    }

    /* Discard header, which the caller must check before unpacking. */
    buffer += sizeof(lpsp_header);

    LITTLE_ENDIAN_LOAD(packet_id, buffer);
    buffer += sizeof(*packet_id);

    LITTLE_ENDIAN_LOAD(sub_packet_index, buffer);
    buffer += sizeof(*sub_packet_index);

    LITTLE_ENDIAN_LOAD(n_sub_packets, buffer);
    buffer += sizeof(*n_sub_packets);

    LITTLE_ENDIAN_LOAD(payload_len, buffer);
    buffer += sizeof(*payload_len);

    if (buf_len != sizeof(lpsp_header) + sizeof(*packet_id) + sizeof(*sub_packet_index) +
                     sizeof(*n_sub_packets) + sizeof(*payload_len) + *payload_len) {
        P_ERR(
          "%s: wrong sub-packet size (%d). Payload size: %d\n", __func__, buf_len, *payload_len);
        return -1;
    }

    memcpy(payload, buffer, *payload_len);
    buffer += *payload_len;

    return 0;
}
