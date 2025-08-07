/*
 * serial-esp32.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdarg.h>
#include <errno.h>
#include <libmeshtastic.h>
#include <serial.h>

int mt_serial_attach(struct mt_client *mtc, const char *device)
{
    int ret = 0;

    (void)(device);

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    mtc->inbuf_len = 0;

done:

    return ret;
}

int mt_serial_detach(struct mt_client *mtc)
{
    (void)(mtc);
    return 0;
}

static void mt_log_append(struct mt_client *mtc, uint8_t *buf, size_t size)
{
    if (mtc->logger != NULL) {
        mtc->logger(mtc, (const char *) buf, size);
    }
}

int mt_serial_process(struct mt_client *mtc, uint32_t timeout_ms)
{
    int ret = 0;
    const struct mt_pb_header *mt_pb_header;
    uint16_t mt_pb_len;
    size_t should_read = 0;

    (void)(timeout_ms);

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    mt_pb_header = (const struct mt_pb_header *) mtc->inbuf;
    mt_pb_len = (mt_pb_header->h_len << 8) | mt_pb_header->l_len;

    if ((mt_pb_header->start1 == MT_PB_START1) &&
        (mt_pb_header->start2 == MT_PB_START2) &&
        (mtc->inbuf_len < sizeof(struct mt_pb_header))) {
        /* Header is sane but missing length, look for mt_pb_len */
        should_read = sizeof(struct mt_pb_header) - mtc->inbuf_len;
    } else if ((mt_pb_header->start1 == MT_PB_START1) &&
               (mt_pb_header->start2 == MT_PB_START2) &&
               ((mtc->inbuf_len > sizeof(struct mt_pb_header)) ||
                (mt_pb_len <
                 (sizeof(mtc->inbuf) - sizeof(struct mt_pb_header))))) {
        /* Header is sane, we should read till the packet is filled */
        should_read = mt_pb_len -
            (mtc->inbuf_len - sizeof(struct mt_pb_header));
    } else if (mtc->inbuf_len == 0) {
        /* Look for START1 */
        should_read = 1;
    } else if ((mt_pb_header->start1 == MT_PB_START1) &&
               (mtc->inbuf_len == 1)) {
        /* Look for START2 */
        should_read = 1;
    } else {
        /* Start over and look for START1 */
        mt_log_append(mtc, mtc->inbuf, mtc->inbuf_len);
        mtc->inbuf_len = 0;
        should_read = 1;
    }

    if (serial_rx_ready() >= 0) {
        ret = serial_read(mtc->inbuf + mtc->inbuf_len, should_read);
    } else {
        ret = 0;
    }

    mtc->inbuf_len += ret;
    mt_pb_len = (mt_pb_header->h_len << 8) | mt_pb_header->l_len;

    if ((mt_pb_header->start1 == MT_PB_START1) &&
        (mt_pb_header->start2 == MT_PB_START2) &&
        (mtc->inbuf_len == (sizeof(struct mt_pb_header) + mt_pb_len))) {
        ret = mt_recv_packet(mtc, mtc->inbuf, mtc->inbuf_len);
        mtc->inbuf_len = 0;
        mtc->inbuf[0] = 0x0;
        mtc->inbuf[1] = 0x0;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int mt_serial_send(struct mt_client *mtc, const uint8_t *packet,
                   size_t size)
{
    int ret = 0;

    (void)(mtc);

    while (size > 0) {
        ret = serial_write(packet, size);
        if (ret == -1) {
            usb_printf("%s: write() returned %d!\n", ret);
            goto done;
        }

        size -= (size_t) ret;
        packet += (size_t) ret;
    }


    ret = 0;

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
