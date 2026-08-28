/*
 * serial-pico.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pico-plat.h>
#include <libmeshtastic.h>

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
    mtc->last_byte_ts = 0;

done:

    return ret;
}

int mt_serial_detach(struct mt_client *mtc)
{
    if (mtc != NULL) {
        mtc->inbuf_len = 0;
        mtc->last_byte_ts = 0;
    }

    return 0;
}

static void mt_log_append(struct mt_client *mtc, const uint8_t *buf, size_t size)
{
    if (mtc->logger != NULL) {
        mtc->logger(mtc, (const char *) buf, size);
    }
}

static void mt_serial_resync(struct mt_client *mtc)
{
    size_t i;

    if (mtc->inbuf_len == 0) {
        return;
    }

    /* Scan for next START1 starting after the current initial byte */
    for (i = 1; i < mtc->inbuf_len; i++) {
        if (mtc->inbuf[i] == MT_PB_START1) {
            break;
        }
    }

    /* Log discarded noisy/corrupted bytes */
    mt_log_append(mtc, mtc->inbuf, i);

    if (i < mtc->inbuf_len) {
        memmove(mtc->inbuf, mtc->inbuf + i, mtc->inbuf_len - i);
        mtc->inbuf_len -= i;
    } else {
        mtc->inbuf_len = 0;
    }
}

int mt_serial_process(struct mt_client *mtc, uint32_t timeout_ms)
{
    int ret = 0;
    size_t should_read;

    (void)(timeout_ms);

    if (mtc == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* Stalled partial frame timeout recovery */
    if ((mtc->inbuf_len > 0) && (mtc->last_byte_ts != 0)) {
        time_t now = mt_impl_now();
        if (now >= (time_t) mtc->last_byte_ts + 2) {
            mt_serial_resync(mtc);
            mtc->last_byte_ts = (mtc->inbuf_len > 0) ? (uint32_t) now : 0;
        }
    }

    /* Read all currently available bytes from UART FIFO up to inbuf capacity */
    should_read = sizeof(mtc->inbuf) - mtc->inbuf_len;
    if (should_read > 0) {
        ret = serial1_rx_ready();
        if (ret > 0) {
            if ((size_t) ret < should_read) {
                should_read = (size_t) ret;
            }

            ret = serial1_read(mtc->inbuf + mtc->inbuf_len, should_read);
            if (ret > 0) {
                mtc->inbuf_len += (size_t) ret;
                mtc->last_byte_ts = (uint32_t) mt_impl_now();
            } else if (ret < 0) {
                return ret;
            }
        }
    }

    /* Process and dispatch all complete frames buffered in inbuf */
    while (mtc->inbuf_len > 0) {
        if (mtc->inbuf[0] != MT_PB_START1) {
            mt_serial_resync(mtc);
            continue;
        }

        if ((mtc->inbuf_len >= 2) && (mtc->inbuf[1] != MT_PB_START2)) {
            mt_serial_resync(mtc);
            continue;
        }

        if (mtc->inbuf_len >= sizeof(struct mt_pb_header)) {
            const struct mt_pb_header *hdr =
                (const struct mt_pb_header *) mtc->inbuf;
            uint16_t plen = ((uint16_t) hdr->h_len << 8) | hdr->l_len;
            if (plen > (sizeof(mtc->inbuf) - sizeof(struct mt_pb_header))) {
                mt_serial_resync(mtc);
                continue;
            }

            size_t total_len = sizeof(struct mt_pb_header) + plen;
            if (mtc->inbuf_len >= total_len) {
                int pkt_ret = mt_recv_packet(mtc, mtc->inbuf, total_len);
                if (pkt_ret != 0) {
                    /* Corrupted frame or decode error: resync non-fatally */
                    mt_serial_resync(mtc);
                    continue;
                } else {
                    /* Successfully received packet: shift out consumed frame */
                    if (mtc->inbuf_len > total_len) {
                        memmove(mtc->inbuf, mtc->inbuf + total_len,
                                mtc->inbuf_len - total_len);
                        mtc->inbuf_len -= total_len;
                    } else {
                        mtc->inbuf_len = 0;
                    }
                    continue;
                }
            }
        }

        /* Incomplete frame awaiting more bytes from UART */
        break;
    }

    if (mtc->inbuf_len == 0) {
        mtc->last_byte_ts = 0;
    }

    return 0;
}

int mt_serial_send(struct mt_client *mtc, const uint8_t *packet,
                   size_t size)
{
    int ret = 0;
    int written;

    (void)(mtc);

    if (packet == NULL || size == 0) {
        return 0;
    }

    while (size > 0) {
        written = serial1_write(packet, size);
        if (written < 0) {
            ret = -1;
            goto done;
        } else if (written == 0) {
            /* TX buffer full / line busy: do not busy-spin to avoid watchdog reboot */
            break;
        }

        size -= (size_t) written;
        packet += (size_t) written;
    }

    ret = 0;

done:

    return ret;
}

time_t mt_impl_now(void)
{
    return time(NULL);
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
