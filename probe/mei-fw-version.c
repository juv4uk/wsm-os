/*
 * mei-fw-version.c -- reads the Intel ME firmware version live, from
 * Linux userspace, via the official /dev/mei0 kernel driver interface
 * -- not via raw physical memory mapping (which is what intelmetool
 * does, and which is blocked on this system by CONFIG_STRICT_DEVMEM,
 * confirmed earlier this session).
 *
 * Protocol: MKHI (Management Engine Kernel Host Interface) over MEI.
 * Message structures and the GEN_GET_FW_VERSION command value are
 * taken directly from coreboot's util/intelmetool/src/me.h (real,
 * working source, not guessed). The MKHI client UUID
 * (8e6a6715-9abc-4043-88ef-9e39c6f63e0f) is documented in the Linux
 * kernel's own MEI driver documentation (docs.kernel.org/driver-api/
 * mei/mei.html) and confirmed against public MEI tooling (u-root's
 * mei package, Intel's own LMS project).
 *
 * This is a read-only status query -- GET_FW_VERSION has no side
 * effects, it's the same operation dozens of legitimate tools
 * (mei-amt-check, LMS, etc.) perform routinely. No write/configure
 * MKHI commands are sent here.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/mei.h>

#define MEI_DEVICE "/dev/mei0"

/* 8e6a6715-9abc-4043-88ef-9e39c6f63e0f, MKHI client.
 *
 * uuid_le stores the first three RFC4122 fields byte-swapped (little-
 * endian), per linux/mei_uuid.h's own UUID_LE() macro -- NOT in the
 * order the UUID is normally written/read. First attempt used the
 * literal display-order bytes here, which is wrong for this specific
 * type (confirmed by reading mei_uuid.h directly after the first run
 * failed with ENOTTY). a=0x8e6a6715 -> bytes 15,67,6a,8e (reversed);
 * b=0x9abc -> bc,9a; c=0x4043 -> 43,40; d0..d7 kept as-is. */
static const uuid_le mkhi_guid = {
    0x15, 0x67, 0x6a, 0x8e, 0xbc, 0x9a, 0x43, 0x40,
    0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0x0f
};

#pragma pack(1)
struct mkhi_header {
    uint8_t  group_id;
    uint8_t  command;      /* bit7 of this byte in coreboot's bitfield is is_response; kept separate here for clarity */
    uint8_t  reserved;
    uint8_t  result;
};

struct me_fw_version {
    uint16_t code_minor;
    uint16_t code_major;
    uint16_t code_build_number;
    uint16_t code_hot_fix;
    uint16_t recovery_minor;
    uint16_t recovery_major;
    uint16_t recovery_build_number;
    uint16_t recovery_hot_fix;
    uint16_t fitcminor;
    uint16_t fitcmajor;
    uint16_t fitcbuildno;
    uint16_t fitchotfix;
};
#pragma pack()

#define MKHI_GROUP_ID_GEN   0xff
#define GEN_GET_FW_VERSION  0x02

int main(void) {
    int fd = open(MEI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open " MEI_DEVICE);
        return 1;
    }

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
    req.group_id = MKHI_GROUP_ID_GEN;
    req.command  = GEN_GET_FW_VERSION; /* is_response bit (0x80) left clear -- this is a request */

    ssize_t written = write(fd, &req, sizeof(req));
    if (written != (ssize_t)sizeof(req)) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Sent GET_FW_VERSION request (%zd bytes)\n", written);

    uint8_t buf[256];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    printf("Received %zd bytes\n", n);

    if (n < (ssize_t)sizeof(struct mkhi_header)) {
        printf("Response too short for even an MKHI header.\n");
        close(fd);
        return 1;
    }

    struct mkhi_header *resp_hdr = (struct mkhi_header *)buf;
    printf("Response header: group_id=0x%02x command=0x%02x result=0x%02x\n",
           resp_hdr->group_id, resp_hdr->command & 0x7f, resp_hdr->result);

    if (n >= (ssize_t)(sizeof(struct mkhi_header) + sizeof(struct me_fw_version))) {
        struct me_fw_version *v = (struct me_fw_version *)(buf + sizeof(struct mkhi_header));
        printf("\n=== Intel ME Firmware Version (live, via MKHI/MEI) ===\n");
        printf("Code:     %u.%u.%u.%u\n", v->code_major, v->code_minor, v->code_hot_fix, v->code_build_number);
        printf("Recovery: %u.%u.%u.%u\n", v->recovery_major, v->recovery_minor, v->recovery_hot_fix, v->recovery_build_number);
        printf("FITC:     %u.%u.%u.%u\n", v->fitcmajor, v->fitcminor, v->fitchotfix, v->fitcbuildno);
    } else {
        printf("Response body shorter than expected me_fw_version struct.\n");
    }

    close(fd);
    return 0;
}
