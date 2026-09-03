/*
 * mei-hmrfpo-enable.c -- sends HMRFPO_ENABLE via /dev/mei0/MKHI.
 *
 * THIS CHANGES HARDWARE STATE: it requests the ME to allow host writes
 * to the ME flash region.  Requires Manufacturing Mode to be open
 * (confirmed on this board via intelmetool + mmdetect).
 *
 * Structure taken directly from coreboot's cse_hmrfpo_enable() in
 * src/soc/intel/common/block/cse/cse.c (lines 834-889) -- real,
 * working implementation, not a guess.
 *
 * Request:  mkhi_hdr(4) + nonce[2](8) = 12 bytes
 * Response: mkhi_hdr(4) + fct_base(4) + fct_limit(4) + status(1) + reserved(3) = 16 bytes
 *
 * After this command succeeds, a subsequent HMRFPO_GET_STATUS should
 * return 2 (ENABLED) instead of 0 (DISABLED).
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

struct hmrfpo_enable_resp {
    struct mkhi_header hdr;
    uint32_t fct_base;
    uint32_t fct_limit;
    uint8_t  status;
    uint8_t  reserved[3];
};
#pragma pack()

#define MKHI_GROUP_ID_HMRFPO   0x05
#define MKHI_HMRFPO_ENABLE     0x01

int main(void)
{
    printf("=== HMRFPO_ENABLE ===\n");
    printf("WARNING: This sends a command that requests ME to allow host\n");
    printf("writes to the ME flash region.  Requires open Manufacturing Mode.\n");
    printf("Hardware state WILL change if Manufacturing Mode is open.\n\n");

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

    /* Build HMRFPO_ENABLE request: header + 8 bytes of zero nonce */
    struct {
        struct mkhi_header hdr;
        uint32_t nonce[2];
    } __attribute__((packed)) req = {0};

    req.hdr.group_id = MKHI_GROUP_ID_HMRFPO;
    req.hdr.command  = MKHI_HMRFPO_ENABLE;

    printf("Sending HMRFPO_ENABLE (group=0x%02x cmd=0x%02x, nonce=0)\n",
           req.hdr.group_id, req.hdr.command);

    ssize_t written = write(fd, &req, sizeof(req));
    if (written != (ssize_t)sizeof(req)) {
        perror("write HMRFPO_ENABLE");
        close(fd);
        return 1;
    }
    printf("Sent %zd bytes\n", written);

    struct hmrfpo_enable_resp resp;
    size_t resp_len = sizeof(resp);
    memset(&resp, 0, resp_len);

    ssize_t n = read(fd, &resp, resp_len);
    if (n < (ssize_t)sizeof(struct mkhi_header)) {
        fprintf(stderr, "Response too short (%zd bytes)\n", n);
        close(fd);
        return 1;
    }
    printf("Received %zd bytes\n", n);

    printf("\nResponse header: group_id=0x%02x command=0x%02x result=0x%02x\n",
           resp.hdr.group_id, resp.hdr.command & 0x7f, resp.hdr.result);

    if (n >= (ssize_t)sizeof(resp)) {
        printf("fct_base  = 0x%08x\n", resp.fct_base);
        printf("fct_limit = 0x%08x\n", resp.fct_limit);
        printf("status    = %u\n", resp.status);
    }

    if (resp.hdr.result) {
        printf("\nMKHI result != 0: command rejected by ME (result=0x%02x)\n",
               resp.hdr.result);
    } else if (resp.status) {
        printf("\nHMRFPO_ENABLE failed: status=%u\n", resp.status);
    } else {
        printf("\nHMRFPO_ENABLE appears to have succeeded.\n");
        printf("Run mei-hmrfpo-status to confirm status changed to ENABLED.\n");
    }

    close(fd);
    return 0;
}
