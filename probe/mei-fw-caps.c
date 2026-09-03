/*
 * mei-fw-caps.c -- reads the live Intel ME firmware capability/SKU
 * flags (mefwcaps_sku) via /dev/mei0, MKHI_FWCAPS_GET_RULE (group
 * 0x03, command 0x02, rule_id=0). Same connection pattern as
 * mei-fw-version.c (see that file's header comment for the UUID
 * byte-order lesson learned there -- reused correctly here from the
 * start). Request/response byte layout taken directly from coreboot's
 * util/intelmetool/src/me.c mkhi_get_fwcaps(), a real, working
 * implementation, not guessed.
 *
 * Read-only status query, same class of operation as GET_FW_VERSION.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/mei.h>

#define MEI_DEVICE "/dev/mei0"

/* 8e6a6715-9abc-4043-88ef-9e39c6f63e0f, MKHI client -- uuid_le byte-swaps
 * the first three RFC4122 fields, see mei-fw-version.c for the derivation. */
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

typedef struct {
    uint32_t  full_net              : 1;
    uint32_t  std_net               : 1;
    uint32_t  manageability         : 1;
    uint32_t  small_business        : 1;
    uint32_t  l3manageability       : 1;
    uint32_t  intel_at              : 1;
    uint32_t  intel_cls             : 1;
    uint32_t  reserved              : 3;
    uint32_t  intel_mpc             : 1;
    uint32_t  icc_over_clocking     : 1;
    uint32_t  pavp                  : 1;
    uint32_t  reserved_1            : 4;
    uint32_t  ipv6                  : 1;
    uint32_t  kvm                   : 1;
    uint32_t  och                   : 1;
    uint32_t  vlan                  : 1;
    uint32_t  tls                   : 1;
    uint32_t  reserved_4            : 1;
    uint32_t  wlan                  : 1;
    uint32_t  reserved_5            : 8;
} mefwcaps_sku;

struct me_fwcaps {
    uint32_t     id;
    uint8_t      length;
    mefwcaps_sku caps_sku;
    uint8_t      reserved[3];
};
#pragma pack()

#define MKHI_GROUP_ID_FWCAPS   0x03
#define MKHI_FWCAPS_GET_RULE   0x02

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
    req.rule_id = 0;

    ssize_t written = write(fd, &req, sizeof(req));
    if (written != (ssize_t)sizeof(req)) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Sent FWCAPS_GET_RULE request (%zd bytes)\n", written);

    uint8_t buf[256];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    printf("Received %zd bytes\n", n);

    /* Only mkhi_header + id + length + caps_sku (9 bytes) actually
     * arrive on the wire -- confirmed empirically (real response was
     * 13 bytes, not the 16 a naive sizeof(struct me_fwcaps) implies).
     * struct me_fwcaps's trailing reserved[3] is local-buffer padding
     * in coreboot's own code, not part of the real MKHI response. */
    size_t min_len = sizeof(struct mkhi_header) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(mefwcaps_sku);
    if (n < (ssize_t)min_len) {
        printf("Response too short (%zd bytes, need at least %zu).\n", n, min_len);
        close(fd);
        return 1;
    }

    struct mkhi_header *resp_hdr = (struct mkhi_header *)buf;
    printf("Response header: group_id=0x%02x command=0x%02x result=0x%02x\n",
           resp_hdr->group_id, resp_hdr->command & 0x7f, resp_hdr->result);

    struct me_fwcaps *caps = (struct me_fwcaps *)(buf + sizeof(struct mkhi_header));
    printf("\n=== Intel ME Firmware Capabilities (SKU), live via MKHI ===\n");
    print_cap("Full Network manageability", caps->caps_sku.full_net);
    print_cap("Regular Network manageability", caps->caps_sku.std_net);
    print_cap("Manageability", caps->caps_sku.manageability);
    print_cap("Small business technology", caps->caps_sku.small_business);
    print_cap("Level III manageability", caps->caps_sku.l3manageability);
    print_cap("Intel Anti-Theft (AT)", caps->caps_sku.intel_at);
    print_cap("Intel Capability Licensing Service (CLS)", caps->caps_sku.intel_cls);
    print_cap("Intel Power Sharing Technology (MPC)", caps->caps_sku.intel_mpc);
    print_cap("ICC Over Clocking", caps->caps_sku.icc_over_clocking);
    print_cap("Protected Audio Video Path (PAVP)", caps->caps_sku.pavp);
    print_cap("IPv6", caps->caps_sku.ipv6);
    print_cap("KVM Remote Control", caps->caps_sku.kvm);
    print_cap("Outbreak Containment Heuristic (OCH)", caps->caps_sku.och);
    print_cap("Virtual LAN (VLAN)", caps->caps_sku.vlan);
    print_cap("TLS", caps->caps_sku.tls);
    print_cap("Wireless LAN (WLAN)", caps->caps_sku.wlan);

    close(fd);
    return 0;
}
