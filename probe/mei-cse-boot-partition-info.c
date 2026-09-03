/*
 * mei-cse-boot-partition-info.c -- reads the live CSE boot-partition
 * info via MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO (group 0xf0
 * MKHI_GROUP_ID_BUP_COMMON, command 0x1c). Reports which of CSE's
 * (up to 3) boot partitions is currently active, which will be used
 * on next reset, and per-partition version/status/offset -- whether
 * this chip actually uses CSE Lite's RO/RW dual-partition redundancy
 * scheme at all, or answers with a single, non-redundant partition
 * (this SoC generation predates "CSE Lite SKU" as coreboot names it;
 * empirical result decides, not the driver's own build-time guess).
 *
 * Request/response layout taken directly from the LOCAL clone of
 * coreboot (/home/user/GitHub/coreboot), not fetched over the network:
 *   src/soc/intel/common/block/cse/cse_lite.c, cse_get_bp_info()
 *   src/soc/intel/common/block/include/intelblocks/cse_lite.h,
 *     struct cse_bp_info / struct cse_bp_entry / CSE_MAX_BOOT_PARTITIONS=3
 *   src/soc/intel/common/block/include/intelblocks/cse.h, struct fw_version
 *
 * Read-only status query, same class of operation as GET_FW_VERSION /
 * FWCAPS_GET_RULE / HMRFPO_GET_STATUS / FW_FEATURE_STATE.
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

struct fw_version {
    uint16_t major;
    uint16_t minor;
    uint16_t hotfix;
    uint16_t build;
};

struct cse_bp_entry {
    struct fw_version fw_ver;
    uint32_t status;
    uint32_t start_offset;
    uint32_t end_offset;
    uint8_t  reserved[12];
};

#define CSE_MAX_BOOT_PARTITIONS 3

struct cse_bp_info {
    uint8_t total_number_of_bp;
    uint8_t current_bp;
    uint8_t next_bp;
    uint8_t flags;
    struct cse_bp_entry bp_entries[CSE_MAX_BOOT_PARTITIONS];
};

struct get_bp_info_rsp {
    struct mkhi_header hdr;
    struct cse_bp_info bp_info;
};
#pragma pack()

#define MKHI_GROUP_ID_BUP_COMMON              0xf0
#define MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO  0x1c

#define BP_INFO_REDUNDANCY_EN     (1 << 0)
#define BP_INFO_MIN_RECOV_MODE_EN (1 << 1)
#define BP_INFO_READ_ONLY_CFG     (1 << 2)

static const char *bp_status_name(uint32_t s) {
    switch (s) {
        case 0: return "SUCCESS";
        case 1: return "GENERAL_FAILURE";
        case 2: return "PARTITION_NOT_PRESENT";
        case 3: return "DATA_FAILURE";
        default: return "невідомий";
    }
}

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
        uint8_t reserved[4];
    } req = {0};
    req.hdr.group_id = MKHI_GROUP_ID_BUP_COMMON;
    req.hdr.command  = MKHI_BUP_COMMON_GET_BOOT_PARTITION_INFO;

    ssize_t written = write(fd, &req, sizeof(req));
    if (written != (ssize_t)sizeof(req)) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Sent GET_BOOT_PARTITION_INFO request (%zd bytes)\n", written);

    uint8_t buf[256];
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
        printf("MKHI result != 0: command rejected by ME (result=0x%02x) -- this chip/firmware may not implement GET_BOOT_PARTITION_INFO.\n",
               hdr->result);
        close(fd);
        return 1;
    }

    size_t fixed_len = sizeof(struct mkhi_header) + 4; /* total/current/next/flags */
    if (n < (ssize_t)fixed_len) {
        printf("Response too short for even the fixed bp_info header (%zd bytes, need %zu).\n", n, fixed_len);
        close(fd);
        return 1;
    }

    struct cse_bp_info *bp = (struct cse_bp_info *)(buf + sizeof(struct mkhi_header));
    printf("\n=== CSE Boot Partition Info (live, MKHI) ===\n");
    printf("total_number_of_bp = %u\n", bp->total_number_of_bp);
    printf("current_bp          = %u\n", bp->current_bp);
    printf("next_bp             = %u\n", bp->next_bp);
    printf("flags               = 0x%02x  (redundancy_en=%d min_recov_mode_en=%d read_only_cfg=%d)\n",
           bp->flags,
           !!(bp->flags & BP_INFO_REDUNDANCY_EN),
           !!(bp->flags & BP_INFO_MIN_RECOV_MODE_EN),
           !!(bp->flags & BP_INFO_READ_ONLY_CFG));

    size_t per_entry = sizeof(struct cse_bp_entry);
    size_t available_entries = (n - fixed_len) / per_entry;
    printf("(відповідь містить дані для %zu записів партицій з максимум %d очікуваних)\n",
           available_entries, CSE_MAX_BOOT_PARTITIONS);

    size_t show = bp->total_number_of_bp;
    if (show > CSE_MAX_BOOT_PARTITIONS) show = CSE_MAX_BOOT_PARTITIONS;
    if (show > available_entries) show = available_entries;

    for (size_t i = 0; i < show; i++) {
        struct cse_bp_entry *e = &bp->bp_entries[i];
        printf("\n-- boot partition %zu --\n", i);
        printf("  version       = %u.%u.%u.%u\n", e->fw_ver.major, e->fw_ver.minor, e->fw_ver.hotfix, e->fw_ver.build);
        printf("  status        = %u (%s)\n", e->status, bp_status_name(e->status));
        printf("  start_offset  = 0x%08x\n", e->start_offset);
        printf("  end_offset    = 0x%08x\n", e->end_offset);
    }

    close(fd);
    return 0;
}
