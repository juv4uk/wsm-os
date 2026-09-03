/*
 * mei-amt-probe.c -- attempts ONLY the MEI connect handshake to the
 * AMT client (UUID 12f80028-b4b7-4b2d-aca8-46e0ff65814c, confirmed
 * directly from fwupd's real source, plugins/intel-amt/
 * fu-intel-amt-device.c: FU_INTEL_AMT_DEVICE_UUID) -- no message is
 * ever written or read. This tests one specific thing: does this
 * chip's ME even expose an AMT client to connect to at all, as an
 * independent, empirical check of what FWCAPS_GET_RULE already
 * reported (Manageability=OFF on this Consumer H SKU) -- not a new
 * state-changing action, a verification of an already-drawn
 * conclusion via a different mechanism (the connect handshake itself,
 * not another status read).
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/mei.h>

#define MEI_DEVICE "/dev/mei0"

/* 12f80028-b4b7-4b2d-aca8-46e0ff65814c, byte-swapped per uuid_le's
 * UUID_LE() convention (see mei-fw-version.c for the derivation this
 * follows: a,b,c fields reversed, d0..d7 kept as-is). */
static const uuid_le amt_guid = {
    0x28, 0x00, 0xf8, 0x12, 0xb7, 0xb4, 0x2d, 0x4b,
    0xac, 0xa8, 0x46, 0xe0, 0xff, 0x65, 0x81, 0x4c
};

int main(void) {
    int fd = open(MEI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open " MEI_DEVICE);
        return 1;
    }

    struct mei_connect_client_data data;
    memset(&data, 0, sizeof(data));
    data.in_client_uuid = amt_guid;

    printf("Attempting MEI connect handshake to AMT client (12f80028-...) -- no message will be sent.\n");

    if (ioctl(fd, IOCTL_MEI_CONNECT_CLIENT, &data) < 0) {
        printf("Connect FAILED: %s (errno=%d)\n", strerror(errno), errno);
        printf("=> AMT client does not appear to be present/exposed on this ME instance.\n");
        close(fd);
        return 0; /* a clean failure here is the expected, informative outcome, not an error */
    }

    printf("Connect SUCCEEDED: protocol_version=%u max_msg_length=%u\n",
           data.out_client_properties.protocol_version,
           data.out_client_properties.max_msg_length);
    printf("=> AMT client IS present and connectable on this ME instance -- unexpected given FWCAPS said Manageability=OFF, worth investigating further before doing anything else with it.\n");

    close(fd);
    return 0;
}
