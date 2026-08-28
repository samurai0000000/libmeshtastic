/*
 * serial-posix.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <libmeshtastic.h>

int mt_serial_attach(struct mt_client *mtc, const char *device)
{
    int ret = 0;
    struct termios tty;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (device == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    mt_serial_detach(mtc);

    mtc->type = MT_CLIENT_SERIAL;
    mtc->device = (const char *) strdup(device);
    if (mtc->device == NULL) {
        ret = -1;
        goto done;
    }

    mtc->fd = open(mtc->device, O_RDWR | O_NOCTTY);
    if (mtc->fd == -1) {
        fprintf(stderr, "%s: %s\n", mtc->device, strerror(errno));
        ret = -1;
        goto done;
    }

    ret = tcgetattr(mtc->fd, &tty);
    if (ret != 0) {
        fprintf(stderr, "%s: %s\n", mtc->device, strerror(errno));
        goto done;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    cfmakeraw(&tty);

    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 10;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= (CLOCAL | CREAD);

    ret = tcflush(mtc->fd, TCIFLUSH);
    if (ret != 0) {
        fprintf(stderr, "%s: %s\n", mtc->device, strerror(errno));
        goto done;
    }

    ret = tcsetattr(mtc->fd, TCSANOW, &tty);
    if (ret != 0) {
        fprintf(stderr, "%s: %s\n", mtc->device, strerror(errno));
        goto done;
    }

    ret = 0;

done:

    if (ret != 0) {
        mt_serial_detach(mtc);
    }

    return ret;
}

int mt_serial_detach(struct mt_client *mtc)
{
    int ret = 0;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (mtc->fd >= 0) {
        close(mtc->fd);
        mtc->fd = -1;
    }

    if (mtc->device) {
        free((void *) mtc->device);
    }

    mtc->fd = -1;
    mtc->device = NULL;
    mtc->inbuf_len = 0;
    mtc->last_byte_ts = 0;

    ret = 0;

done:

    return ret;
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
    struct timeval timeout;
    int nfds;
    fd_set rfds;
    size_t should_read;

    if (mtc == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (mtc->fd < 0) {
        errno = EBADFD;
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

    /* Wait and read available bytes from file descriptor */
    should_read = sizeof(mtc->inbuf) - mtc->inbuf_len;
    if (should_read > 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms - (timeout.tv_sec * 1000)) * 1000;

        FD_ZERO(&rfds);
        FD_SET(mtc->fd, &rfds);
        nfds = mtc->fd + 1;

        ret = select(nfds, &rfds, NULL, NULL, &timeout);
        if (ret == -1) {
            if (errno == EINTR) {
                return 0;
            }
            fprintf(stderr, "%s: %s\n", mtc->device, strerror(errno));
            return -1;
        } else if (ret > 0) {
            ret = read(mtc->fd, mtc->inbuf + mtc->inbuf_len, should_read);
            if (ret > 0) {
                mtc->inbuf_len += (size_t) ret;
                mtc->last_byte_ts = (uint32_t) mt_impl_now();
            } else if (ret == 0) {
                fprintf(stderr, "%s: EOF!\n", mtc->device);
                mtc->inbuf_len = 0;
                errno = EIO;
                return -1;
            } else if (ret < 0) {
                if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    fprintf(stderr, "%s: %s!\n", mtc->device, strerror(errno));
                    mtc->inbuf_len = 0;
                    return -1;
                }
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
    ssize_t written;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (mtc->type != MT_CLIENT_SERIAL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (mtc->fd < 0) {
        errno = EBADFD;
        ret = -1;
        goto done;
    }

    if (packet == NULL || size == 0) {
        return 0;
    }

    while (size > 0) {
        written = write(mtc->fd, packet, size);
        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                break;
            }
            fprintf(stderr, "%s: %s!\n", mtc->device, strerror(errno));
            ret = -1;
            goto done;
        } else if (written == 0) {
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
