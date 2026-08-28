/*
 * protocol.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <libmeshtastic.h>

#if defined(ESP_PLATFORM)
#include <serial.h>
#elif defined(LIB_PICO_PLATFORM)
#include <pico-plat.h>
#endif


#define PB_BUF_SIZE 512

int mt_recv_packet(struct mt_client *mtc, uint8_t *packet, size_t size)
{
    int ret = 0;
    struct mt_pb_header *header = (struct mt_pb_header *) packet;
    uint16_t mt_pb_len;
    pb_istream_t istream;
    meshtastic_FromRadio from_radio;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (packet == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if ((header->start1 != MT_PB_START1) ||
        (header->start2 != MT_PB_START2)) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    mt_pb_len = (header->h_len << 8) | header->l_len;
    if (size != (sizeof(*header) + mt_pb_len)) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    memset(&from_radio, 0x0, sizeof(from_radio));
    istream = pb_istream_from_buffer(packet + sizeof(*header), mt_pb_len);
    ret = pb_decode(&istream, meshtastic_FromRadio_fields, &from_radio);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }

    mtc->bytes_rx += (sizeof(*header) + mt_pb_len);
    mtc->packets_rx++;
    mtc->last_packet_ts = mt_impl_now();

    if (mtc->handler) {
        mtc->handler(mtc, packet, size, &from_radio);
    }

    ret = 0;

done:

    return ret;
}

static int mt_send_to_radio(struct mt_client *mtc,
                            meshtastic_ToRadio *to_radio)
{
    int ret = 0;
    uint8_t pb_buf[sizeof(struct mt_pb_header) + PB_BUF_SIZE];
    pb_ostream_t ostream;
    struct mt_pb_header *header = (struct mt_pb_header *) pb_buf;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (to_radio == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    ostream = pb_ostream_from_buffer(pb_buf + sizeof(*header), PB_BUF_SIZE);
    ret = pb_encode(&ostream, meshtastic_ToRadio_fields, to_radio);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }

    header->start1 = MT_PB_START1;
    header->start2 = MT_PB_START2;
    header->h_len = ostream.bytes_written / 256;
    header->l_len = ostream.bytes_written % 256;

    switch (mtc->type) {
    case MT_CLIENT_SERIAL:
        ret = mt_serial_send(mtc, pb_buf,
                             sizeof(*header) + ostream.bytes_written);
        break;
    default:
        errno = EBADF;
        ret = -1;
        break;
    }

    if (ret == 0) {
        mtc->bytes_tx += (sizeof(*header) + ostream.bytes_written);
        mtc->packets_tx++;
    }

done:

    return ret;
}

int mt_send_null(struct mt_client *mtc)
{
    int ret = 0;
    struct mt_pb_header header;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    header.start1 = MT_PB_START1;
    header.start2 = MT_PB_START2;
    header.h_len = 0;
    header.l_len = 0;

    switch (mtc->type) {
    case MT_CLIENT_SERIAL:
        ret = mt_serial_send(mtc, (const uint8_t *) &header, sizeof(header));
        break;
    default:
        errno = EBADF;
        ret = -1;
        break;
    }

done:

    return ret;
}

int mt_send_disconnect(struct mt_client *mtc)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    memset(&to_radio, 0x0, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_disconnect_tag;
    to_radio.disconnect = true;
    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_send_heartbeat(struct mt_client *mtc)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    memset(&to_radio, 0x0, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_heartbeat_tag;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

static void mt_seed_rand(void)
{
    static int seeded = 0;

    if (!seeded) {
        srand((unsigned int) time(NULL));
        seeded = 1;
    }
}

static uint32_t mt_next_packet_id(void)
{
    static uint32_t seq = 0;
    uint32_t id;

    mt_seed_rand();

    if (seq == 0) {
        seq = ((uint32_t) time(NULL) << 8) ^ ((uint32_t) rand() << 1);
        if (seq == 0) {
            seq = 1;
        }
    }

    seq++;
    id = seq & 0x7fffffffU;
    if (id == 0) {
        seq = 1;
        id = 1;
    }

    return id;
}

int mt_send_want_config(struct mt_client *mtc)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;

    mt_seed_rand();

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    memset(&to_radio, 0x0, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    to_radio.want_config_id = rand() & 0x7fffffff;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_text_message(struct mt_client *mtc,
                    uint32_t dest, uint8_t channel,
                    const char *message,
                    unsigned int hop_start, bool want_ack)
{
    int ret = 0;
    size_t message_len = 0;
    meshtastic_ToRadio to_radio;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (message == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (hop_start > 7) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (hop_start == 0) {
        hop_start = 3;
    }

    message_len = strlen(message);
    if (message_len > 200) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    to_radio.packet.to = dest;
    to_radio.packet.channel = channel;
    to_radio.packet.want_ack = want_ack;
    if (want_ack) {
        to_radio.packet.priority = meshtastic_MeshPacket_Priority_RELIABLE;
    }
    to_radio.packet.hop_start = hop_start;
    to_radio.packet.hop_limit = hop_start;
    to_radio.packet.decoded.payload.size = message_len;
    memcpy(to_radio.packet.decoded.payload.bytes, message,
           to_radio.packet.decoded.payload.size);

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_admin_message_device_metadata_request(struct mt_client *mtc)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    meshtastic_AdminMessage admin_message;
    pb_ostream_t ostream;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_device_metadata_request_tag;
    admin_message.get_device_metadata_request = true;

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;
    to_radio.packet.decoded.want_response = true;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, meshtastic_AdminMessage_fields, &admin_message);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }
    to_radio.packet.decoded.payload.size = ostream.bytes_written;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_admin_message_reboot(struct mt_client *mtc, uint32_t dest,
                            uint32_t seconds)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    meshtastic_AdminMessage admin_message;
    pb_ostream_t ostream;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (seconds > (uint32_t) INT32_MAX) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_reboot_seconds_tag;
    admin_message.reboot_seconds = (int32_t) seconds;

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.to = dest;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, meshtastic_AdminMessage_fields, &admin_message);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }
    to_radio.packet.decoded.payload.size = ostream.bytes_written;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_admin_message_commit_edit_settings(struct mt_client *mtc, uint32_t dest)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    meshtastic_AdminMessage admin_message;
    pb_ostream_t ostream;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_commit_edit_settings_tag;
    admin_message.commit_edit_settings = true;

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.to = dest;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, meshtastic_AdminMessage_fields, &admin_message);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }
    to_radio.packet.decoded.payload.size = ostream.bytes_written;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_admin_message_remove_by_nodenum(struct mt_client *mtc, uint32_t dest,
                                       uint32_t nodenum)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    meshtastic_AdminMessage admin_message;
    pb_ostream_t ostream;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_remove_by_nodenum_tag;
    admin_message.remove_by_nodenum = nodenum;

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.to = dest;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, meshtastic_AdminMessage_fields, &admin_message);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }
    to_radio.packet.decoded.payload.size = ostream.bytes_written;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_admin_message_set_time(struct mt_client *mtc, uint32_t dest,
                              uint32_t epoch_seconds)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    meshtastic_AdminMessage admin_message;
    pb_ostream_t ostream;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_time_only_tag;
    admin_message.set_time_only = epoch_seconds;

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.to = dest;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, meshtastic_AdminMessage_fields, &admin_message);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }
    to_radio.packet.decoded.payload.size = ostream.bytes_written;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_admin_message_set_tzdef(struct mt_client *mtc, uint32_t dest,
                               const meshtastic_Config_DeviceConfig *current_device_config,
                               const char *tzdef)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    meshtastic_AdminMessage admin_message;
    pb_ostream_t ostream;

    if (mtc == NULL || tzdef == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_device_tag;
    if (current_device_config != NULL) {
        admin_message.set_config.payload_variant.device = *current_device_config;
    }
    strncpy(admin_message.set_config.payload_variant.device.tzdef, tzdef,
            sizeof(admin_message.set_config.payload_variant.device.tzdef) - 1);
    admin_message.set_config.payload_variant.device.tzdef[sizeof(admin_message.set_config.payload_variant.device.tzdef) - 1] = '\0';

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.to = dest;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, meshtastic_AdminMessage_fields, &admin_message);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }
    to_radio.packet.decoded.payload.size = ostream.bytes_written;

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
