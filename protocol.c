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

static int mt_send_admin_message(struct mt_client *mtc, uint32_t dest,
                                 const meshtastic_AdminMessage *admin_message,
                                 bool want_response)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    pb_ostream_t ostream;

    if (mtc == NULL || admin_message == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.to = dest;
    to_radio.packet.decoded.portnum = meshtastic_PortNum_ADMIN_APP;
    to_radio.packet.hop_start = 3;
    to_radio.packet.hop_limit = 3;
    to_radio.packet.decoded.want_response = want_response;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, meshtastic_AdminMessage_fields, admin_message);
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

static int mt_send_app_packet(struct mt_client *mtc,
                              uint32_t dest, uint8_t channel,
                              meshtastic_PortNum portnum,
                              const pb_msgdesc_t *fields,
                              const void *src_struct,
                              unsigned int hop_start,
                              bool want_ack,
                              bool want_response)
{
    int ret = 0;
    meshtastic_ToRadio to_radio;
    pb_ostream_t ostream;

    if (mtc == NULL || fields == NULL || src_struct == NULL) {
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

    bzero(&to_radio, sizeof(to_radio));
    to_radio.which_payload_variant = meshtastic_ToRadio_packet_tag;
    to_radio.packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    to_radio.packet.id = mt_next_packet_id();
    to_radio.packet.to = dest;
    to_radio.packet.channel = channel;
    to_radio.packet.decoded.portnum = portnum;
    to_radio.packet.hop_start = hop_start;
    to_radio.packet.hop_limit = hop_start;
    to_radio.packet.want_ack = want_ack;
    if (want_ack) {
        to_radio.packet.priority = meshtastic_MeshPacket_Priority_RELIABLE;
    }
    to_radio.packet.decoded.want_response = want_response;

    ostream = pb_ostream_from_buffer(to_radio.packet.decoded.payload.bytes,
                                     sizeof(to_radio.packet.decoded.payload.bytes));
    ret = pb_encode(&ostream, fields, src_struct);
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

    if (packet == NULL || size < sizeof(*header)) {
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

    if (mtc == NULL || message == NULL) {
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

/* ========================================================================= */
/* Group 1: Device Configuration (meshtastic_Config)                         */
/* ========================================================================= */

int mt_admin_message_get_config_request(struct mt_client *mtc,
                                       uint32_t dest,
                                       meshtastic_AdminMessage_ConfigType config_type)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_config_request_tag;
    admin_message.get_config_request = config_type;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_set_config(struct mt_client *mtc,
                               uint32_t dest,
                               const meshtastic_Config *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_device_config(struct mt_client *mtc,
                                      uint32_t dest,
                                      const meshtastic_Config_DeviceConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_device_tag;
    admin_message.set_config.payload_variant.device = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_position_config(struct mt_client *mtc,
                                        uint32_t dest,
                                        const meshtastic_Config_PositionConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_position_tag;
    admin_message.set_config.payload_variant.position = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_power_config(struct mt_client *mtc,
                                     uint32_t dest,
                                     const meshtastic_Config_PowerConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_power_tag;
    admin_message.set_config.payload_variant.power = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_network_config(struct mt_client *mtc,
                                       uint32_t dest,
                                       const meshtastic_Config_NetworkConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_network_tag;
    admin_message.set_config.payload_variant.network = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_display_config(struct mt_client *mtc,
                                       uint32_t dest,
                                       const meshtastic_Config_DisplayConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_display_tag;
    admin_message.set_config.payload_variant.display = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_lora_config(struct mt_client *mtc,
                                    uint32_t dest,
                                    const meshtastic_Config_LoRaConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_lora_tag;
    admin_message.set_config.payload_variant.lora = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_bluetooth_config(struct mt_client *mtc,
                                         uint32_t dest,
                                         const meshtastic_Config_BluetoothConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_bluetooth_tag;
    admin_message.set_config.payload_variant.bluetooth = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_security_config(struct mt_client *mtc,
                                        uint32_t dest,
                                        const meshtastic_Config_SecurityConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_security_tag;
    admin_message.set_config.payload_variant.security = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_sessionkey_config(struct mt_client *mtc,
                                          uint32_t dest,
                                          const meshtastic_Config_SessionkeyConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_config_tag;
    admin_message.set_config.which_payload_variant =
        meshtastic_Config_sessionkey_tag;
    admin_message.set_config.payload_variant.sessionkey = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 2: Module Configuration (meshtastic_ModuleConfig)                   */
/* ========================================================================= */

int mt_admin_message_get_module_config_request(struct mt_client *mtc,
                                              uint32_t dest,
                                              meshtastic_AdminMessage_ModuleConfigType module_config_type)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_module_config_request_tag;
    admin_message.get_module_config_request = module_config_type;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_set_module_config(struct mt_client *mtc,
                                      uint32_t dest,
                                      const meshtastic_ModuleConfig *module_config)
{
    meshtastic_AdminMessage admin_message;

    if (module_config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config = *module_config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_mqtt_config(struct mt_client *mtc,
                                    uint32_t dest,
                                    const meshtastic_ModuleConfig_MQTTConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_mqtt_tag;
    admin_message.set_module_config.payload_variant.mqtt = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_serial_module_config(struct mt_client *mtc,
                                             uint32_t dest,
                                             const meshtastic_ModuleConfig_SerialConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_serial_tag;
    admin_message.set_module_config.payload_variant.serial = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_extnotif_config(struct mt_client *mtc,
                                        uint32_t dest,
                                        const meshtastic_ModuleConfig_ExternalNotificationConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_external_notification_tag;
    admin_message.set_module_config.payload_variant.external_notification = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_storeforward_config(struct mt_client *mtc,
                                            uint32_t dest,
                                            const meshtastic_ModuleConfig_StoreForwardConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_store_forward_tag;
    admin_message.set_module_config.payload_variant.store_forward = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_rangetest_config(struct mt_client *mtc,
                                         uint32_t dest,
                                         const meshtastic_ModuleConfig_RangeTestConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_range_test_tag;
    admin_message.set_module_config.payload_variant.range_test = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_telemetry_config(struct mt_client *mtc,
                                         uint32_t dest,
                                         const meshtastic_ModuleConfig_TelemetryConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_telemetry_tag;
    admin_message.set_module_config.payload_variant.telemetry = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_cannedmsg_config(struct mt_client *mtc,
                                         uint32_t dest,
                                         const meshtastic_ModuleConfig_CannedMessageConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_canned_message_tag;
    admin_message.set_module_config.payload_variant.canned_message = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_audio_config(struct mt_client *mtc,
                                     uint32_t dest,
                                     const meshtastic_ModuleConfig_AudioConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_audio_tag;
    admin_message.set_module_config.payload_variant.audio = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_remotehardware_config(struct mt_client *mtc,
                                              uint32_t dest,
                                              const meshtastic_ModuleConfig_RemoteHardwareConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_remote_hardware_tag;
    admin_message.set_module_config.payload_variant.remote_hardware = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_neighborinfo_config(struct mt_client *mtc,
                                            uint32_t dest,
                                            const meshtastic_ModuleConfig_NeighborInfoConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_neighbor_info_tag;
    admin_message.set_module_config.payload_variant.neighbor_info = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_ambientlighting_config(struct mt_client *mtc,
                                               uint32_t dest,
                                               const meshtastic_ModuleConfig_AmbientLightingConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_ambient_lighting_tag;
    admin_message.set_module_config.payload_variant.ambient_lighting = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_detectionsensor_config(struct mt_client *mtc,
                                               uint32_t dest,
                                               const meshtastic_ModuleConfig_DetectionSensorConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_detection_sensor_tag;
    admin_message.set_module_config.payload_variant.detection_sensor = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_paxcounter_config(struct mt_client *mtc,
                                          uint32_t dest,
                                          const meshtastic_ModuleConfig_PaxcounterConfig *config)
{
    meshtastic_AdminMessage admin_message;

    if (config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_module_config_tag;
    admin_message.set_module_config.which_payload_variant =
        meshtastic_ModuleConfig_paxcounter_tag;
    admin_message.set_module_config.payload_variant.paxcounter = *config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 3: Channel Configuration (meshtastic_Channel)                       */
/* ========================================================================= */

int mt_admin_message_get_channel_request(struct mt_client *mtc,
                                        uint32_t dest,
                                        uint32_t channel_index)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_channel_request_tag;
    admin_message.get_channel_request = channel_index + 1;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_set_channel(struct mt_client *mtc,
                                uint32_t dest,
                                const meshtastic_Channel *channel)
{
    meshtastic_AdminMessage admin_message;

    if (channel == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_channel_tag;
    admin_message.set_channel = *channel;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 4: Owner Identity & Ham Radio Parameters                            */
/* ========================================================================= */

int mt_admin_message_get_owner_request(struct mt_client *mtc,
                                      uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_owner_request_tag;
    admin_message.get_owner_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_set_owner(struct mt_client *mtc,
                              uint32_t dest,
                              const meshtastic_User *user)
{
    meshtastic_AdminMessage admin_message;

    if (user == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_owner_tag;
    admin_message.set_owner = *user;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_ham_mode(struct mt_client *mtc,
                                 uint32_t dest,
                                 const meshtastic_HamParameters *ham_params)
{
    meshtastic_AdminMessage admin_message;

    if (ham_params == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_ham_mode_tag;
    admin_message.set_ham_mode = *ham_params;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 5: Node Database & Contact Operations                               */
/* ========================================================================= */

int mt_admin_message_set_favorite_node(struct mt_client *mtc,
                                      uint32_t dest,
                                      uint32_t nodenum)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_favorite_node_tag;
    admin_message.set_favorite_node = nodenum;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_remove_favorite_node(struct mt_client *mtc,
                                         uint32_t dest,
                                         uint32_t nodenum)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_remove_favorite_node_tag;
    admin_message.remove_favorite_node = nodenum;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_ignored_node(struct mt_client *mtc,
                                     uint32_t dest,
                                     uint32_t nodenum)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_ignored_node_tag;
    admin_message.set_ignored_node = nodenum;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_remove_ignored_node(struct mt_client *mtc,
                                        uint32_t dest,
                                        uint32_t nodenum)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_remove_ignored_node_tag;
    admin_message.remove_ignored_node = nodenum;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_remove_by_nodenum(struct mt_client *mtc,
                                       uint32_t dest,
                                       uint32_t nodenum)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_remove_by_nodenum_tag;
    admin_message.remove_by_nodenum = nodenum;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_reset_nodedb(struct mt_client *mtc,
                                 uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_nodedb_reset_tag;
    admin_message.nodedb_reset = 1;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_add_contact(struct mt_client *mtc,
                                uint32_t dest,
                                const meshtastic_SharedContact *contact)
{
    meshtastic_AdminMessage admin_message;

    if (contact == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_add_contact_tag;
    admin_message.add_contact = *contact;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 6: Fixed Position & Location Operations                             */
/* ========================================================================= */

int mt_admin_message_set_fixed_position(struct mt_client *mtc,
                                        uint32_t dest,
                                        const meshtastic_Position *position)
{
    meshtastic_AdminMessage admin_message;

    if (position == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_fixed_position_tag;
    admin_message.set_fixed_position = *position;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_remove_fixed_position(struct mt_client *mtc,
                                          uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_remove_fixed_position_tag;
    admin_message.remove_fixed_position = true;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 7: Device Status, UI Config & Diagnostics                           */
/* ========================================================================= */

int mt_admin_message_device_metadata_request(struct mt_client *mtc)
{
    return mt_admin_message_get_device_metadata_request(mtc, 0);
}

int mt_admin_message_get_device_metadata_request(struct mt_client *mtc,
                                                uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_device_metadata_request_tag;
    admin_message.get_device_metadata_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_get_device_connection_status_request(struct mt_client *mtc,
                                                         uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_device_connection_status_request_tag;
    admin_message.get_device_connection_status_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_get_node_remote_hardware_pins_request(struct mt_client *mtc,
                                                          uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_node_remote_hardware_pins_request_tag;
    admin_message.get_node_remote_hardware_pins_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_get_canned_messages_request(struct mt_client *mtc,
                                                uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_canned_message_module_messages_request_tag;
    admin_message.get_canned_message_module_messages_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_set_canned_messages(struct mt_client *mtc,
                                        uint32_t dest,
                                        const char *messages)
{
    meshtastic_AdminMessage admin_message;

    if (messages == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_canned_message_module_messages_tag;
    strncpy(admin_message.set_canned_message_module_messages, messages,
            sizeof(admin_message.set_canned_message_module_messages) - 1);
    admin_message.set_canned_message_module_messages[sizeof(admin_message.set_canned_message_module_messages) - 1] = '\0';

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_get_ringtone_request(struct mt_client *mtc,
                                         uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_ringtone_request_tag;
    admin_message.get_ringtone_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_set_ringtone_message(struct mt_client *mtc,
                                         uint32_t dest,
                                         const char *ringtone)
{
    meshtastic_AdminMessage admin_message;

    if (ringtone == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_ringtone_message_tag;
    strncpy(admin_message.set_ringtone_message, ringtone,
            sizeof(admin_message.set_ringtone_message) - 1);
    admin_message.set_ringtone_message[sizeof(admin_message.set_ringtone_message) - 1] = '\0';

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_get_ui_config_request(struct mt_client *mtc,
                                          uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_get_ui_config_request_tag;
    admin_message.get_ui_config_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, true);
}

int mt_admin_message_store_ui_config(struct mt_client *mtc,
                                     uint32_t dest,
                                     const meshtastic_DeviceUIConfig *ui_config)
{
    meshtastic_AdminMessage admin_message;

    if (ui_config == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_store_ui_config_tag;
    admin_message.store_ui_config = *ui_config;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 8: Device Lifecycle, Transactions & Maintenance                     */
/* ========================================================================= */

int mt_admin_message_begin_edit_settings(struct mt_client *mtc,
                                        uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_begin_edit_settings_tag;
    admin_message.begin_edit_settings = true;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_commit_edit_settings(struct mt_client *mtc, uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_commit_edit_settings_tag;
    admin_message.commit_edit_settings = true;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_reboot(struct mt_client *mtc, uint32_t dest,
                            uint32_t seconds)
{
    meshtastic_AdminMessage admin_message;

    if (seconds > (uint32_t) INT32_MAX) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_reboot_seconds_tag;
    admin_message.reboot_seconds = (int32_t) seconds;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_reboot_ota(struct mt_client *mtc, uint32_t dest,
                               uint32_t seconds)
{
    meshtastic_AdminMessage admin_message;

    if (seconds > (uint32_t) INT32_MAX) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_reboot_ota_seconds_tag;
    admin_message.reboot_ota_seconds = (int32_t) seconds;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_shutdown(struct mt_client *mtc, uint32_t dest,
                             uint32_t seconds)
{
    meshtastic_AdminMessage admin_message;

    if (seconds > (uint32_t) INT32_MAX) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_shutdown_seconds_tag;
    admin_message.shutdown_seconds = (int32_t) seconds;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_factory_reset_config(struct mt_client *mtc,
                                         uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_factory_reset_config_tag;
    admin_message.factory_reset_config = 1;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_factory_reset_device(struct mt_client *mtc,
                                         uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_factory_reset_device_tag;
    admin_message.factory_reset_device = 1;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_enter_dfu_mode(struct mt_client *mtc,
                                   uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_enter_dfu_mode_request_tag;
    admin_message.enter_dfu_mode_request = true;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_backup_preferences(struct mt_client *mtc,
                                       uint32_t dest,
                                       meshtastic_AdminMessage_BackupLocation location)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_backup_preferences_tag;
    admin_message.backup_preferences = location;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_restore_preferences(struct mt_client *mtc,
                                        uint32_t dest,
                                        meshtastic_AdminMessage_BackupLocation location)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_restore_preferences_tag;
    admin_message.restore_preferences = location;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_remove_backup_preferences(struct mt_client *mtc,
                                              uint32_t dest,
                                              meshtastic_AdminMessage_BackupLocation location)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_remove_backup_preferences_tag;
    admin_message.remove_backup_preferences = location;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_delete_file(struct mt_client *mtc,
                                uint32_t dest,
                                const char *filepath)
{
    meshtastic_AdminMessage admin_message;

    if (filepath == NULL) {
        errno = EINVAL;
        return -1;
    }

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_delete_file_request_tag;
    strncpy(admin_message.delete_file_request, filepath,
            sizeof(admin_message.delete_file_request) - 1);
    admin_message.delete_file_request[sizeof(admin_message.delete_file_request) - 1] = '\0';

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_scale(struct mt_client *mtc,
                              uint32_t dest,
                              uint32_t scale)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_scale_tag;
    admin_message.set_scale = scale;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_exit_simulator(struct mt_client *mtc,
                                   uint32_t dest)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_exit_simulator_tag;
    admin_message.exit_simulator = true;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_time(struct mt_client *mtc, uint32_t dest,
                              uint32_t epoch_seconds)
{
    meshtastic_AdminMessage admin_message;

    bzero(&admin_message, sizeof(admin_message));
    admin_message.which_payload_variant =
        meshtastic_AdminMessage_set_time_only_tag;
    admin_message.set_time_only = epoch_seconds;

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

int mt_admin_message_set_tzdef(struct mt_client *mtc, uint32_t dest,
                               const meshtastic_Config_DeviceConfig *current_device_config,
                               const char *tzdef)
{
    meshtastic_AdminMessage admin_message;

    if (tzdef == NULL) {
        errno = EINVAL;
        return -1;
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

    return mt_send_admin_message(mtc, dest, &admin_message, false);
}

/* ========================================================================= */
/* Group 9: Mesh Application Payloads (Non-Admin Ports)                      */
/* ========================================================================= */

int mt_send_position(struct mt_client *mtc,
                     uint32_t dest, uint8_t channel,
                     const meshtastic_Position *position,
                     unsigned int hop_start, bool want_ack)
{
    if (position == NULL) {
        errno = EINVAL;
        return -1;
    }

    return mt_send_app_packet(mtc, dest, channel,
                             meshtastic_PortNum_POSITION_APP,
                             meshtastic_Position_fields,
                             position, hop_start, want_ack, false);
}

int mt_send_user(struct mt_client *mtc,
                 uint32_t dest, uint8_t channel,
                 const meshtastic_User *user,
                 unsigned int hop_start, bool want_ack)
{
    if (user == NULL) {
        errno = EINVAL;
        return -1;
    }

    return mt_send_app_packet(mtc, dest, channel,
                             meshtastic_PortNum_NODEINFO_APP,
                             meshtastic_User_fields,
                             user, hop_start, want_ack, false);
}

int mt_send_waypoint(struct mt_client *mtc,
                     uint32_t dest, uint8_t channel,
                     const meshtastic_Waypoint *waypoint,
                     unsigned int hop_start, bool want_ack)
{
    if (waypoint == NULL) {
        errno = EINVAL;
        return -1;
    }

    return mt_send_app_packet(mtc, dest, channel,
                             meshtastic_PortNum_WAYPOINT_APP,
                             meshtastic_Waypoint_fields,
                             waypoint, hop_start, want_ack, false);
}

int mt_send_traceroute(struct mt_client *mtc,
                       uint32_t dest, uint8_t channel,
                       unsigned int hop_start)
{
    meshtastic_RouteDiscovery route_discovery;

    bzero(&route_discovery, sizeof(route_discovery));

    return mt_send_app_packet(mtc, dest, channel,
                             meshtastic_PortNum_TRACEROUTE_APP,
                             meshtastic_RouteDiscovery_fields,
                             &route_discovery, hop_start, false, true);
}

int mt_send_telemetry(struct mt_client *mtc,
                      uint32_t dest, uint8_t channel,
                      const meshtastic_Telemetry *telemetry,
                      unsigned int hop_start, bool want_ack)
{
    if (telemetry == NULL) {
        errno = EINVAL;
        return -1;
    }

    return mt_send_app_packet(mtc, dest, channel,
                             meshtastic_PortNum_TELEMETRY_APP,
                             meshtastic_Telemetry_fields,
                             telemetry, hop_start, want_ack, false);
}

int mt_send_telemetry_req(struct mt_client *mtc,
                          uint32_t dest, uint8_t channel,
                          pb_size_t variant_tag,
                          unsigned int hop_start)
{
    meshtastic_Telemetry telemetry;

    bzero(&telemetry, sizeof(telemetry));
    telemetry.time = (uint32_t) mt_impl_now();
    telemetry.which_variant = variant_tag;

    return mt_send_app_packet(mtc, dest, channel,
                             meshtastic_PortNum_TELEMETRY_APP,
                             meshtastic_Telemetry_fields,
                             &telemetry, hop_start, false, true);
}

int mt_send_remote_hardware_req(struct mt_client *mtc,
                                uint32_t dest, uint8_t channel,
                                meshtastic_HardwareMessage_Type type,
                                uint64_t gpio_mask, uint64_t gpio_value,
                                unsigned int hop_start, bool want_ack)
{
    meshtastic_HardwareMessage hw_msg;

    bzero(&hw_msg, sizeof(hw_msg));
    hw_msg.type = type;
    hw_msg.gpio_mask = gpio_mask;
    hw_msg.gpio_value = gpio_value;

    return mt_send_app_packet(mtc, dest, channel,
                             meshtastic_PortNum_REMOTE_HARDWARE_APP,
                             meshtastic_HardwareMessage_fields,
                             &hw_msg, hop_start, want_ack,
                             (type == meshtastic_HardwareMessage_Type_READ_GPIOS));
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
