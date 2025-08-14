/*
 * libmeshtastic.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef LIBMESHTASTIC_H
#define LIBMESHTASTIC_H

#if !defined(EXTERN_C_BEGIN)
#if defined(__cplusplus)
#define EXTERN_C_BEGIN extern "C" {
#else
#define EXTERN_C_BEGIN
#endif
#endif

#if !defined(EXTERN_C_END)
#if defined(__cplusplus)
#define EXTERN_C_END }
#else
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

#include <time.h>
#include <meshtastic/mesh.pb.h>
#include <meshtastic/admin.pb.h>
#include <meshtastic/mqtt.pb.h>
#include <pb_encode.h>
#include <pb_decode.h>

struct mt_pb_header {
    uint8_t start1;
#define MT_PB_START1 0x94U
    uint8_t start2;
#define MT_PB_START2 0xc3U
    uint8_t h_len;
    uint8_t l_len;
};

struct mt_client
{
    uint32_t type;
#define MT_CLIENT_SERIAL 0
    int fd;
    const char *device;
    uint8_t inbuf[sizeof(struct mt_pb_header) + 512];
    size_t inbuf_len;
    void (*handler)(struct mt_client *mtc, const void *packet, size_t size,
                    const meshtastic_FromRadio *from_radio);
    void (*logger)(struct mt_client *mtc, const char *msg, size_t len);
    void *ctx;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t last_packet_ts;
};

#if defined(LIB_PICO_PLATFORM)
extern void mt_serial_init(void);
extern void mt_serial_write(unsigned int chan, const void *buf, size_t len);
#endif

extern int mt_serial_attach(struct mt_client *mtc, const char *device);
extern int mt_serial_detach(struct mt_client *mtc);
extern int mt_serial_process(struct mt_client *mtc, uint32_t timeout_ms);
extern int mt_serial_send(struct mt_client *mtc, const uint8_t *packet,
                          size_t size);

extern int mt_recv_packet(struct mt_client *mtc, uint8_t *packet, size_t size);

extern int mt_send_null(struct mt_client *mtc);
extern int mt_send_disconnect(struct mt_client *mtc);
extern int mt_send_heartbeat(struct mt_client *mtc);
extern int mt_send_want_config(struct mt_client *mtc);

extern int mt_text_message(struct mt_client *mtc,
                           uint32_t dest, uint8_t channel,
                           const char *message,
                           unsigned int hop_start, bool want_ack);

extern int mt_admin_message_device_metadata_request(
    struct mt_client *mtc);
extern int mt_admin_message_reboot(struct mt_client *mtc,
                                   uint32_t seconds);

extern time_t mt_impl_now(void);

EXTERN_C_END

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
