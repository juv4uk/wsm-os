/*
 * mei-fw-feature-state.c -- reads the LIVE runtime feature-state
 * bitmask via MKHI_FWCAPS_GET_RULE (group 0x03, command 0x02), same
 * wire command as mei-fw-caps.c, but with rule_id=0x20
 * (ME_FEATURE_STATE_RULE_ID) instead of rule_id=0. Different rule_id,
 * different response shape: this one returns a plain uint32_t runtime
 * bitmask (fw_runtime_status), not the mefwcaps_sku bitfield struct.
 *
 * mei-fw-caps.c answers "what is this SKU capable of" (static
 * capability flags). This answers "what is actually turned on right
 * now" (live runtime state) -- a distinct question, same command
 * family, different rule_id.
 *
 * Request/response layout and rule_id taken directly from coreboot's
 * src/soc/intel/common/block/cse/cse.c, cse_get_fw_feature_state():
 *   request  = mkhi_hdr(4) + rule_id(4)                    = 8 bytes
 *   response = mkhi_hdr(4) + rule_id(4) + rule_len(1) + fw_runtime_status(4) = 13 bytes
 * Named bits confirmed from intelblocks/cse.h:
 *   ME_FW_FEATURE_PTT = BIT(29)  -- Platform Trust Technology (fTPM)
 *   ME_FW_FEATURE_PSR = BIT(5)   -- Platform Service Record (anti-theft/asset tracking)
 * Any other set bit is printed as an unnamed bit number, not guessed.
 *
 * Read-only status query, same class of operation as GET_FW_VERSION /
 * FWCAPS_GET_RULE / HMRFPO_GET_STATUS.
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

struct fw_feature_state_resp {
    struct mkhi_header hdr;
    uint32_t rule_id;
    uint8_t  rule_len;
    uint32_t fw_runtime_status;
};
#pragma pack()

#define MKHI_GROUP_ID_FWCAPS        0x03
#define MKHI_FWCAPS_GET_RULE        0x02
#define ME_FEATURE_STATE_RULE_ID    0x20

#define ME_FW_FEATURE_PTT_BIT   29
#define ME_FW_FEATURE_PSR_BIT   5

static void print_cap(const char *name, int state) {
    printf("  %-45s : %s\n", name, state ? "УВІМКНЕНО" : "вимкнено");
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
        uint32_t rule_id;
    } req = {0};
    req.hdr.group_id = MKHI_GROUP_ID_FWCAPS;
    req.hdr.command  = MKHI_FWCAPS_GET_RULE;
    req.rule_id = ME_FEATURE_STATE_RULE_ID;

    ssize_t written = write(fd, &req, sizeof(req));
    if (written != (ssize_t)sizeof(req)) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Sent FWCAPS_GET_RULE(rule_id=0x%02x, feature state) request (%zd bytes)\n",
           ME_FEATURE_STATE_RULE_ID, written);

    uint8_t buf[64];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    printf("Received %zd bytes\n", n);

    if (n < (ssize_t)sizeof(struct fw_feature_state_resp)) {
        printf("Response too short (%zd bytes, need %zu).\n",
               n, sizeof(struct fw_feature_state_resp));
        close(fd);
        return 1;
    }

    struct fw_feature_state_resp *resp = (struct fw_feature_state_resp *)buf;
    printf("Response header: group_id=0x%02x command=0x%02x result=0x%02x\n",
           resp->hdr.group_id, resp->hdr.command & 0x7f, resp->hdr.result);

    if (resp->hdr.result) {
        printf("MKHI result != 0: command rejected by ME (result=0x%02x)\n", resp->hdr.result);
        close(fd);
        return 1;
    }
    if (resp->rule_len != sizeof(resp->fw_runtime_status)) {
        printf("Unexpected rule_len=%u (expected %zu) -- response shape differs from what coreboot's source describes.\n",
               resp->rule_len, sizeof(resp->fw_runtime_status));
        close(fd);
        return 1;
    }

    uint32_t status = resp->fw_runtime_status;
    printf("\n=== Intel ME Firmware Feature State (live runtime, MKHI rule_id=0x%02x) ===\n",
           ME_FEATURE_STATE_RULE_ID);
    printf("raw fw_runtime_status = 0x%08x\n\n", status);
    print_cap("PTT (Platform Trust Technology / fTPM)", (status >> ME_FW_FEATURE_PTT_BIT) & 1);
    print_cap("PSR (Platform Service Record)", (status >> ME_FW_FEATURE_PSR_BIT) & 1);

    for (int bit = 0; bit < 32; bit++) {
        if (bit == ME_FW_FEATURE_PTT_BIT || bit == ME_FW_FEATURE_PSR_BIT)
            continue;
        if ((status >> bit) & 1)
            printf("  біт %-2d (неідентифікований)                    : УВІМКНЕНО\n", bit);
    }

    close(fd);
    return 0;
}
