/*
 * mei-hmrfpo-status.c -- reads the live HMRFPO (Host ME Region Flash
 * Protection Override) status via /dev/mei0/MKHI -- read-only, does
 * NOT send HMRFPO_ENABLE (which actually changes flash write-
 * protection state; deliberately not implemented here).
 *
 * Request/response layout confirmed directly from coreboot's real,
 * working implementation: src/soc/intel/common/block/cse/cse.c,
 * cse_hmrfpo_get_status(). Group/command values from
 * intelblocks/cse.h: MKHI_GROUP_ID_HMRFPO=0x5, MKHI_HMRFPO_GET_STATUS=0x3.
 *
 * Status meanings (from cse.h's own comments):
 *   0 = DISABLED -- host can't access ME region
 *   1 = LOCKED   -- ME firmware locked down HMRFPO, host can't access
 *   2 = ENABLED  -- host CAN access ME region
 *
 * Same UUID/connect pattern as mei-fw-version.c.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/mei.h>

#define MEI_DEVICE "/dev/mei0"

static const uuid_le mkhi_guid = {
    0x15, 0x67, 0x6a, 0x8e, 0xbc, 0x9a, 0x43, 0x40,
    0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0x0f
};

#pragma pack(1)
struct mkhi_header {
    uint8_t group_id;
    uint8_t command;
    uint8_t reserved;
    uint8_t result;
};
#pragma pack()

#define MKHI_GROUP_ID_HMRFPO    0x05
#define MKHI_HMRFPO_GET_STATUS  0x03

int main(void) {
    int fd = open(MEI_DEVICE, O_RDWR);
    if (fd < 0) { perror("open " MEI_DEVICE); return 1; }

    struct mei_connect_client_data data;
    memset(&data, 0, sizeof(data));
    data.in_client_uuid = mkhi_guid;

    if (ioctl(fd, IOCTL_MEI_CONNECT_CLIENT, &data) < 0) {
        perror("IOCTL_MEI_CONNECT_CLIENT");
        close(fd);
        return 1;
    }
    printf("Connected to MKHI client: protocol_version=%u max_msg_length=%u\n",
           data.out_client_properties.protocol_version,
           data.out_client_properties.max_msg_length);

    struct mkhi_header req = {0};
    req.group_id = MKHI_GROUP_ID_HMRFPO;
    req.command  = MKHI_HMRFPO_GET_STATUS;

    ssize_t written = write(fd, &req, sizeof(req));
    if (written != (ssize_t)sizeof(req)) { perror("write"); close(fd); return 1; }
    printf("Sent HMRFPO_GET_STATUS request (%zd bytes)\n", written);

    uint8_t buf[64];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) { perror("read"); close(fd); return 1; }
    printf("Received %zd bytes\n", n);

    if (n < (ssize_t)(sizeof(struct mkhi_header) + 1)) {
        printf("Response too short.\n");
        close(fd);
        return 1;
    }

    struct mkhi_header *resp_hdr = (struct mkhi_header *)buf;
    uint8_t status = buf[sizeof(struct mkhi_header)];

    printf("Response header: group_id=0x%02x command=0x%02x result=0x%02x\n",
           resp_hdr->group_id, resp_hdr->command & 0x7f, resp_hdr->result);

    const char *meaning;
    switch (status) {
        case 0: meaning = "DISABLED -- хост НЕ має доступу до регіону ME"; break;
        case 1: meaning = "LOCKED -- прошивка ME заблокувала HMRFPO, хост не має доступу"; break;
        case 2: meaning = "ENABLED -- хост МАЄ доступ до регіону ME прямо зараз"; break;
        default: meaning = "невідоме значення"; break;
    }
    printf("\n=== HMRFPO Status (live, read-only) ===\n");
    printf("status = %u (%s)\n", status, meaning);

    close(fd);
    return 0;
}
