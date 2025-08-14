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

    ret = 0;

done:

    return ret;
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
    struct timeval timeout;
    int nfds;
    fd_set rfds;
    const struct mt_pb_header *mt_pb_header;
    uint16_t mt_pb_len;
    size_t should_read = 0;

    if (mtc == NULL) {
        errno = EINVAL;
        ret = -1;
        goto done;
    }

    if (mtc->fd < 0) {
        errno = EBADFD;
        ret = -1;
        goto done;
    }

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms - (timeout.tv_sec * 1000)) * 1000;

    FD_ZERO(&rfds);
    FD_SET(mtc->fd, &rfds);
    nfds = mtc->fd + 1;

    ret = select(nfds, &rfds, NULL, NULL, &timeout);
    if (ret == -1) {
        fprintf(stderr, "%s: %s\n", mtc->device, strerror(errno));
        goto done;
    } else if (ret == 0) {
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

    ret = read(mtc->fd, mtc->inbuf + mtc->inbuf_len, should_read);
    if (ret == -1) {
        fprintf(stderr, "%s: %s!\n", mtc->device, strerror(errno));
        mtc->inbuf_len = 0;
        goto done;
    } else if (ret == 0) {
        fprintf(stderr, "%s: EOF!\n", mtc->device);
        mtc->inbuf_len = 0;
        goto done;
    }

    mtc->inbuf_len += ret;
    mt_pb_len = (mt_pb_header->h_len << 8) | mt_pb_header->l_len;

    if ((mt_pb_header->start1 == MT_PB_START1) &&
        (mt_pb_header->start2 == MT_PB_START2) &&
        (mtc->inbuf_len == (sizeof(struct mt_pb_header) + mt_pb_len))) {
        ret = mt_recv_packet(mtc, mtc->inbuf, mtc->inbuf_len);
        mtc->inbuf_len = 0;
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

    while (size > 0) {
        ret = write(mtc->fd, packet, size);
        if (ret == -1) {
            fprintf(stderr, "%s: %s!\n", mtc->device, strerror(errno));
            goto done;
        }

        size -= (size_t) ret;
        packet += (size_t) ret;
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
