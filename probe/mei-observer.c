/*
 * mei-observer.c -- minimal, read-only Host<->ME channel observer.
 * Built per owner's explicit research mission (2026-09-03): OBSERVE ->
 * MAP -> VERIFY -> DOCUMENT, not OBSERVE -> MODIFY. NOT a generic
 * arbitrary-command sender -- the query set below is a fixed table of
 * commands already independently confirmed read-only elsewhere in
 * this repo (mei-fw-version.c, mei-fw-caps.c, mei-fw-feature-state.c,
 * mei-hmrfpo-status.c). Every run appends a structured RAW +
 * INTERPRETATION + PROVENANCE + STATUS record to mei-observer.log,
 * with raw request/response bytes preserved independent of any
 * decoding -- so a future, better decoder can re-interpret past
 * captures without re-querying the machine.
 *
 * Every read() is guarded by poll() with a hard timeout instead of
 * blocking forever -- BUP_COMMON_GET_BOOT_PARTITION_INFO and
 * GET_BOOT_PERF_DATA (group 0xf0) are LIVE-CONFIRMED to hang
 * indefinitely on this exact ME generation (hardware/CSME-
 * ARCHITECTURE.md); this tool treats a timeout as a normal, loggable
 * outcome (NO_RESPONSE), not a crash.
 *
 * UUID/ioctl/struct definitions checked directly against the LOCAL
 * primary source /usr/include/linux/mei.h (matches every sibling
 * probe already using it). MKHI command layouts checked directly
 * against the LOCAL coreboot clone (/home/user/GitHub/coreboot),
 * cited per query below -- not fetched over the network, not
 * guessed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/mei.h>

#define MEI_DEVICE "/dev/mei0"
#define LOG_PATH   "mei-observer.log"
#define READ_TIMEOUT_MS 3000

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

struct query {
    const char *name;
    const char *provenance;
    size_t req_len;
    void (*build_req)(uint8_t *buf);
    void (*decode)(const uint8_t *buf, size_t n, char *out, size_t outlen);
};

/* ---- fw-version ---- */
static void build_fw_version(uint8_t *buf) {
    struct mkhi_header h = {0xff, 0x02, 0, 0};
    memcpy(buf, &h, sizeof(h));
}
static void decode_fw_version(const uint8_t *buf, size_t n, char *out, size_t outlen) {
    if (n < sizeof(struct mkhi_header) + 24) { snprintf(out, outlen, "response too short (%zu bytes)", n); return; }
    const uint16_t *w = (const uint16_t *)(buf + sizeof(struct mkhi_header));
    snprintf(out, outlen,
        "code=%u.%u.%u.%u recovery=%u.%u.%u.%u fitc=%u.%u.%u.%u",
        w[1], w[0], w[3], w[2], w[5], w[4], w[7], w[6], w[9], w[8], w[11], w[10]);
}

/* ---- fwcaps-sku (rule_id=0) ---- */
static void build_fwcaps_sku(uint8_t *buf) {
    struct mkhi_header h = {0x03, 0x02, 0, 0};
    uint32_t rule_id = 0;
    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(h), &rule_id, 4);
}
static void decode_fwcaps_sku(const uint8_t *buf, size_t n, char *out, size_t outlen) {
    size_t off = sizeof(struct mkhi_header) + 4 /*id*/ + 1 /*length*/;
    if (n < off + 4) { snprintf(out, outlen, "response too short (%zu bytes)", n); return; }
    uint32_t caps;
    memcpy(&caps, buf + off, 4);
    snprintf(out, outlen, "mefwcaps_sku raw=0x%08x manageability_bit2=%d pavp_bit12=%d",
             caps, (caps >> 2) & 1, (caps >> 12) & 1);
}

/* ---- fw-feature-state (rule_id=0x20) ---- */
static void build_fw_feature_state(uint8_t *buf) {
    struct mkhi_header h = {0x03, 0x02, 0, 0};
    uint32_t rule_id = 0x20;
    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(h), &rule_id, 4);
}
static void decode_fw_feature_state(const uint8_t *buf, size_t n, char *out, size_t outlen) {
    size_t off = sizeof(struct mkhi_header) + 4 /*rule_id*/ + 1 /*rule_len*/;
    if (n < off + 4) { snprintf(out, outlen, "response too short (%zu bytes)", n); return; }
    uint32_t status;
    memcpy(&status, buf + off, 4);
    snprintf(out, outlen, "fw_runtime_status raw=0x%08x PTT(bit29)=%d PSR(bit5)=%d",
             status, (status >> 29) & 1, (status >> 5) & 1);
}

/* ---- hmrfpo-status ---- */
static void build_hmrfpo_status(uint8_t *buf) {
    struct mkhi_header h = {0x05, 0x03, 0, 0};
    memcpy(buf, &h, sizeof(h));
}
static void decode_hmrfpo_status(const uint8_t *buf, size_t n, char *out, size_t outlen) {
    size_t off = sizeof(struct mkhi_header);
    if (n < off + 1) { snprintf(out, outlen, "response too short (%zu bytes)", n); return; }
    uint8_t status = buf[off];
    const char *m = status == 0 ? "DISABLED" : status == 1 ? "LOCKED" : status == 2 ? "ENABLED" : "невідомо";
    snprintf(out, outlen, "status=%u (%s)", status, m);
}

static struct query queries[] = {
    { "fw-version", "coreboot util/intelmetool/src/me.h GEN_GET_FW_VERSION; local clone: same struct reused verbatim from mei-fw-version.c", 4, build_fw_version, decode_fw_version },
    { "fwcaps-sku", "coreboot util/intelmetool/src/me.c mkhi_get_fwcaps(); local clone: same struct reused verbatim from mei-fw-caps.c", 8, build_fwcaps_sku, decode_fwcaps_sku },
    { "fw-feature-state", "coreboot src/soc/intel/common/block/cse/cse.c cse_get_fw_feature_state(), local clone /home/user/GitHub/coreboot", 8, build_fw_feature_state, decode_fw_feature_state },
    { "hmrfpo-status", "coreboot src/soc/intel/common/block/cse/cse.c cse_hmrfpo_get_status(); read-only, local clone", 4, build_hmrfpo_status, decode_hmrfpo_status },
};
#define NUM_QUERIES (sizeof(queries) / sizeof(queries[0]))

static void hex_dump(const uint8_t *buf, size_t n, char *out, size_t outlen) {
    size_t pos = 0;
    for (size_t i = 0; i < n && pos + 3 < outlen; i++)
        pos += snprintf(out + pos, outlen - pos, "%02x", buf[i]);
}

static void log_line(FILE *log, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(log, fmt, ap);
    va_end(ap);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <query>\n  available queries:\n", argv[0]);
        for (size_t i = 0; i < NUM_QUERIES; i++)
            fprintf(stderr, "    %s\n", queries[i].name);
        return 2;
    }

    struct query *q = NULL;
    for (size_t i = 0; i < NUM_QUERIES; i++)
        if (strcmp(argv[1], queries[i].name) == 0) { q = &queries[i]; break; }
    if (!q) {
        fprintf(stderr, "unknown query: %s\n", argv[1]);
        return 2;
    }

    FILE *log = fopen(LOG_PATH, "a");
    if (!log) { perror("fopen " LOG_PATH); return 1; }

    time_t now = time(NULL);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S%z", localtime(&now));

    log_line(log, "\n=== %s query=%s ===\n", timebuf, q->name);
    log_line(log, "PROVENANCE: %s\n", q->provenance);

    int fd = open(MEI_DEVICE, O_RDWR);
    if (fd < 0) {
        log_line(log, "STATUS: ERROR (open %s: %s)\n", MEI_DEVICE, strerror(errno));
        fclose(log);
        return 1;
    }

    struct mei_connect_client_data data;
    memset(&data, 0, sizeof(data));
    data.in_client_uuid = mkhi_guid;

    if (ioctl(fd, IOCTL_MEI_CONNECT_CLIENT, &data) < 0) {
        log_line(log, "STATUS: ERROR (connect: %s)\n", strerror(errno));
        close(fd);
        fclose(log);
        return 1;
    }
    log_line(log, "CONNECT: protocol_version=%u max_msg_length=%u\n",
              data.out_client_properties.protocol_version,
              data.out_client_properties.max_msg_length);

    uint8_t reqbuf[64] = {0};
    q->build_req(reqbuf);
    char reqhex[256];
    hex_dump(reqbuf, q->req_len, reqhex, sizeof(reqhex));
    log_line(log, "REQUEST_BYTES(%zu): %s\n", q->req_len, reqhex);

    ssize_t written = write(fd, reqbuf, q->req_len);
    if (written != (ssize_t)q->req_len) {
        log_line(log, "STATUS: ERROR (write: %s)\n", strerror(errno));
        close(fd);
        fclose(log);
        return 1;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int pr = poll(&pfd, 1, READ_TIMEOUT_MS);
    if (pr == 0) {
        log_line(log, "STATUS: NO_RESPONSE (poll timeout %dms -- write succeeded, ME never answered; LIVE-CONFIRMED behavior for MKHI_GROUP_ID_BUP_COMMON on this chip, see CSME-ARCHITECTURE.md)\n", READ_TIMEOUT_MS);
        close(fd);
        fclose(log);
        return 3;
    }
    if (pr < 0) {
        log_line(log, "STATUS: ERROR (poll: %s)\n", strerror(errno));
        close(fd);
        fclose(log);
        return 1;
    }

    uint8_t respbuf[512];
    ssize_t n = read(fd, respbuf, sizeof(respbuf));
    if (n < 0) {
        log_line(log, "STATUS: ERROR (read: %s)\n", strerror(errno));
        close(fd);
        fclose(log);
        return 1;
    }

    char resphex[1024];
    hex_dump(respbuf, n, resphex, sizeof(resphex));
    log_line(log, "RESPONSE_BYTES(%zd): %s\n", n, resphex);

    if ((size_t)n >= sizeof(struct mkhi_header)) {
        struct mkhi_header *h = (struct mkhi_header *)respbuf;
        log_line(log, "MKHI_HEADER: group_id=0x%02x command=0x%02x result=0x%02x\n",
                  h->group_id, h->command & 0x7f, h->result);
        if (h->result) {
            log_line(log, "STATUS: MKHI_REJECTED (result=0x%02x)\n", h->result);
            close(fd);
            fclose(log);
            return 1;
        }
    }

    char interp[512];
    q->decode(respbuf, n, interp, sizeof(interp));
    log_line(log, "INTERPRETATION: %s\n", interp);
    log_line(log, "STATUS: LIVE-CONFIRMED\n");

    close(fd);
    fclose(log);
    return 0;
}
