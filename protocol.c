/*
 * protocol.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <libmeshtastic.h>

int mt_recv_packet(struct mt_client *mtc, uint8_t *packet, size_t size)
{
    int ret = 0;
    struct mt_pb_header *header = (struct mt_pb_header *) packet;
    uint16_t mt_pb_len;
    pb_istream_t stream;
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
    stream = pb_istream_from_buffer(packet + sizeof(*header), mt_pb_len);
    ret = pb_decode(&stream, meshtastic_FromRadio_fields, &from_radio);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }

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
    uint8_t pb_buf[512];
    struct mt_pb_header *header = (struct mt_pb_header *) pb_buf;
    pb_ostream_t stream;

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

    stream = pb_ostream_from_buffer(pb_buf + sizeof(*header),
                                    sizeof(pb_buf) - sizeof(*header));
    ret = pb_encode(&stream, meshtastic_ToRadio_fields, to_radio);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }

    header->start1 = MT_PB_START1;
    header->start2 = MT_PB_START2;
    header->h_len = stream.bytes_written / 256;
    header->l_len = stream.bytes_written % 256;

    switch (mtc->type) {
    case MT_CLIENT_SERIAL:
        ret = mt_serial_send(mtc, pb_buf,
                             sizeof(*header) + stream.bytes_written);
        break;
    default:
        errno = EBADF;
        ret = -1;
        break;
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

int mt_send_want_config(struct mt_client *mtc)
{
    static int __seeded_rand = 0;
    int ret = 0;
    meshtastic_ToRadio to_radio;

    if (!__seeded_rand) {
        srand(time(NULL));
        __seeded_rand = 1;
    }

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
    meshtastic_ToRadio to_radio;
    size_t message_len = 0;

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
    to_radio.packet.id = rand() & 0x7fffffff;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    to_radio.packet.to = dest;
    to_radio.packet.channel = channel;
    to_radio.packet.want_ack = want_ack;
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
    meshtastic_Data data;
    meshtastic_MeshPacket packet;
    meshtastic_AdminMessage admin_message;
    pb_ostream_t stream;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_device_metadata_request_tag;
    admin_message.get_device_metadata_request = true;

    bzero(&data, sizeof(data));
    data.portnum = meshtastic_PortNum_ADMIN_APP;
    data.want_response = true;
    stream = pb_ostream_from_buffer(data.payload.bytes, sizeof(data.payload));
    ret = pb_encode(&stream, meshtastic_AdminMessage_fields, &admin_message);
    if (ret != 1) {
        errno = EIO;
        ret = -1;
        goto done;
    }

    memcpy(&packet.decoded, &data, sizeof(data));

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    memcpy(&to_radio.packet, &packet, sizeof(packet));

    ret = mt_send_to_radio(mtc, &to_radio);

done:

    return ret;
}

int mt_admin_message_reboot(struct mt_client *mtc, uint32_t seconds)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&to_radio, sizeof(to_radio));

    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = rand() & 0x7fffffff;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;
    (void)(seconds);

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
