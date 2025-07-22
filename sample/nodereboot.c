/*
 * nodereboot.c
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <libmeshtastic.h>

static void mt_handler(struct mt_client *mtc, const void *packet, size_t size,
                       const meshtastic_FromRadio *from_radio)
{
    (void)(mtc);
    (void)(packet);
    (void)(size);

    switch (from_radio->which_payload_variant) {
    case meshtastic_FromRadio_config_complete_id_tag:
        printf("rebooting\n");
        mt_admin_message_reboot(mtc, 0);
 	    break;
    default:
        break;
    }
}

static struct mt_client mtc = {
    .type = 0,
    .fd = -1,
    .device = NULL,
    .inbuf = { 0x0, },
    .inbuf_len = 0,
    .handler = mt_handler,
    .logger = NULL,
    .ctx = NULL,
};

static void cleanup(void)
{
    mt_send_disconnect(&mtc);
    mt_serial_detach(&mtc);
}

static const struct option long_options[] = {
    { "device", required_argument, NULL, 'd', },
};

int main(int argc, char **argv)
{
    int ret = 0;
    const char *device = "/dev/ttyAMA0";

    for (;;) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "d:",
                            long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'd':
            device = optarg;
            break;
        default:
            fprintf(stderr, "Unrecognized argument specified!\n");
            exit(EXIT_FAILURE);
            break;
        }
    }

    ret = mt_serial_attach(&mtc, device);
    if (ret != 0) {
        fprintf(stderr, "%s: %s!\n", device, strerror(errno));
        goto done;
    }

    atexit(cleanup);

    ret = mt_send_want_config(&mtc);
    if (ret != 0) {
        goto done;
    }

    for (;;) {
        ret = mt_serial_process(&mtc, 1000);
        if (ret != 0) {
            goto done;
        }
    }

done:

    return ret;
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
