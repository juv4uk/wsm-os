/*
 * mei-cse-boot-perf.c -- reads CSE's own boot-performance timestamp
 * data via MKHI_BUP_COMMON_GET_BOOT_PERF_DATA (group 0xf0
 * MKHI_GROUP_ID_BUP_COMMON, command 0x08). Same group as
 * mei-cse-boot-partition-info.c's GET_BOOT_PARTITION_INFO (cmd 0x1c),
 * which this exact chip did NOT answer at all (write succeeded,
 * response never arrived -- LIVE-CONFIRMED silent drop, not an MKHI
 * error, see hardware/CSME-ARCHITECTURE.md). This probe exists to
 * check whether that silence is specific to GET_BOOT_PARTITION_INFO,
 * or whether the whole BUP_COMMON group (0xf0) goes unanswered on
 * this ME generation -- a real, discriminating next step, not a
 * repeat of the same question.
 *
 * ALWAYS run this through `timeout`, given the sibling probe's
 * result: a real risk here is the same silent hang, not just an
 * error response.
 *
 * Request/response layout taken directly from the LOCAL clone of
 * coreboot (/home/user/GitHub/coreboot), not fetched over the network:
 *   src/soc/intel/common/block/cse/telemetry.c, cse_get_boot_performance_data()
 *   src/soc/intel/common/block/include/intelblocks/cse.h, struct cse_boot_perf_rsp
 *   src/soc/intel/common/block/include/intelblocks/cse_telemetry_v1.h, NUM_CSE_BOOT_PERF_DATA=64
 *
 * Read-only status query.
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
    uint8_t  group_id;
    uint8_t  command;
    uint8_t  reserved;
    uint8_t  result;
};

#define NUM_CSE_BOOT_PERF_DATA 64

struct cse_boot_perf_rsp {
    struct mkhi_header hdr;
    uint32_t version;
    uint32_t num_valid_timestamps;
    uint32_t timestamp[NUM_CSE_BOOT_PERF_DATA];
};
#pragma pack()

#define MKHI_GROUP_ID_BUP_COMMON          0xf0
#define MKHI_BUP_COMMON_GET_BOOT_PERF_DATA 0x08

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

    struct {
        struct mkhi_header hdr;
        uint32_t reserved;
    } req = {0};
    req.hdr.group_id = MKHI_GROUP_ID_BUP_COMMON;
    req.hdr.command  = MKHI_BUP_COMMON_GET_BOOT_PERF_DATA;

    ssize_t written = write(fd, &req, sizeof(req));
    if (written != (ssize_t)sizeof(req)) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Sent GET_BOOT_PERF_DATA request (%zd bytes) -- waiting for response...\n", written);

    uint8_t buf[512];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    printf("Received %zd bytes\n", n);

    struct mkhi_header *hdr = (struct mkhi_header *)buf;
    if (n < (ssize_t)sizeof(struct mkhi_header)) {
        printf("Response too short even for an MKHI header.\n");
        close(fd);
        return 1;
    }
    printf("Response header: group_id=0x%02x command=0x%02x result=0x%02x\n",
           hdr->group_id, hdr->command & 0x7f, hdr->result);

    if (hdr->result) {
        printf("MKHI result != 0: command rejected by ME (result=0x%02x)\n", hdr->result);
        close(fd);
        return 1;
    }

    if (n < (ssize_t)(sizeof(struct mkhi_header) + 8)) {
        printf("Response too short for version+num_valid_timestamps.\n");
        close(fd);
        return 1;
    }

    struct cse_boot_perf_rsp *resp = (struct cse_boot_perf_rsp *)buf;
    printf("\n=== CSE Boot Performance Data (live, MKHI) ===\n");
    printf("version               = %u\n", resp->version);
    printf("num_valid_timestamps  = %u\n", resp->num_valid_timestamps);

    uint32_t count = resp->num_valid_timestamps;
    if (count > NUM_CSE_BOOT_PERF_DATA) count = NUM_CSE_BOOT_PERF_DATA;
    size_t max_from_wire = (n - sizeof(struct mkhi_header) - 8) / sizeof(uint32_t);
    if (count > max_from_wire) count = max_from_wire;

    for (uint32_t i = 0; i < count; i++) {
        printf("  timestamp[%2u] = %u\n", i, resp->timestamp[i]);
    }

    close(fd);
    return 0;
}
