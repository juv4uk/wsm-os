/*
 * entropy-source-probe-native.c -- the same sampling methodology as
 * entropy-source-probe.c (the pre-OS UEFI probe, run once already
 * under QEMU/TCG -- see ENTROPY-RESULTS.md), run instead as an
 * ordinary Linux userspace program on the owner's real, physical CPU.
 *
 * Why this exists as a separate native build rather than editing the
 * UEFI probe: RDSEED/RDRAND are plain user-mode instructions on real
 * x86-64 Linux, need no EFI Boot Services and no root -- a genuine
 * userspace program is the natural way to run this on real silicon,
 * where the UEFI probe was only ever a vehicle for running pre-OS.
 *
 * ENTROPY-RESULTS.md's open question: is RDSEED's retries-before-
 * success behavior consistent with a geometric/Poisson process (real
 * hardware entropy-conditioner exhaustion), per the idea in
 * wsm/research/hardware-native-constants.md? The QEMU/TCG run gave
 * zero retries across 256 samples -- strong evidence TCG's RDSEED
 * doesn't model real exhaustion at all, but that result is completely
 * silent on what real hardware actually does. This is the first run
 * where that's even measurable.
 *
 * Same PLURAL INTERNAL / SINGULAR EXTERNAL discipline as every other
 * probe here (wsm/research/the-observer-gap.md): raw samples and
 * statistics only, no claim about real-world constants asserted here.
 */
#include <stdio.h>
#include <stdint.h>
#include <cpuid.h>

static inline void do_cpuid(uint32_t leaf, uint32_t sub, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ __volatile__("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(sub));
}
static inline int rdseed64(uint64_t *out) {
    uint8_t ok;
    __asm__ __volatile__("rdseed %0\n\tsetc %1" : "=r"(*out), "=qm"(ok) :: "cc");
    return ok;
}
static inline int rdrand64(uint64_t *out) {
    uint8_t ok;
    __asm__ __volatile__("rdrand %0\n\tsetc %1" : "=r"(*out), "=qm"(ok) :: "cc");
    return ok;
}
static inline uint64_t rdtsc64(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static inline int popcount64(uint64_t v) {
    int c = 0;
    while (v) { c += (int)(v & 1); v >>= 1; }
    return c;
}

int main(void) {
    printf("\n=== entropy-source-probe-native: same methodology, real silicon, no QEMU ===\n\n");

    uint32_t a, b, c, d;
    do_cpuid(1, 0, &a, &b, &c, &d);
    int has_rdrand = (c >> 30) & 1;
    do_cpuid(7, 0, &a, &b, &c, &d);
    int has_rdseed = (b >> 18) & 1;
    printf("CPUID reports RDRAND=%d RDSEED=%d\n", has_rdrand, has_rdseed);

    if (!has_rdseed && !has_rdrand) {
        printf("Neither present per CPUID -- nothing to sample. Stopping honestly, not faking data.\n");
        return 1;
    }

    const uint32_t N = 256;
    uint64_t samples[256];
    uint32_t seed_ok_count = 0;
    uint32_t retries_before_success[256];
    uint32_t total_bits_set = 0;

    for (uint32_t i = 0; i < N; i++) {
        uint64_t v = 0;
        uint32_t attempts = 0;
        int ok = 0;
        if (has_rdseed) {
            while (attempts < 1000) {
                attempts++;
                if (rdseed64(&v)) { ok = 1; break; }
            }
        } else {
            ok = rdrand64(&v);
            attempts = 1;
        }
        samples[i] = v;
        retries_before_success[i] = attempts - 1;
        if (ok) { seed_ok_count++; total_bits_set += (uint32_t)popcount64(v); }
    }

    printf("Samples requested: %u\n", N);
    printf("Successful reads:  %u\n", seed_ok_count);
    printf("Total bits set across successful 64-bit samples: %u of %u (expect ~50%% if genuinely uniform)\n",
           total_bits_set, seed_ok_count * 64);

    uint64_t retry_sum = 0, retry_max = 0;
    uint32_t nonzero_retries = 0;
    for (uint32_t i = 0; i < N; i++) {
        retry_sum += retries_before_success[i];
        if (retries_before_success[i] > retry_max) retry_max = retries_before_success[i];
        if (retries_before_success[i] > 0) nonzero_retries++;
    }
    printf("Retries-before-success: sum=%lu mean_x1000=%lu max=%lu nonzero_samples=%u\n",
           (unsigned long)retry_sum, (unsigned long)(retry_sum * 1000) / N, (unsigned long)retry_max, nonzero_retries);

    printf("\nFirst 8 raw 64-bit samples (for independent external inspection, not self-judged here):\n");
    for (uint32_t i = 0; i < 8 && i < N; i++) {
        printf("  [%u] 0x%016lx\n", i, (unsigned long)samples[i]);
    }

    /* Full retry histogram -- printed raw, not interpreted, so an
     * external reader can judge geometric/Poisson-consistency themselves. */
    printf("\nFull retries-before-success list (256 values, raw):\n");
    for (uint32_t i = 0; i < N; i++) {
        printf("%u%s", retries_before_success[i], (i % 16 == 15) ? "\n" : " ");
    }
    printf("\n");

    uint64_t tsc0 = rdtsc64();
    for (volatile uint32_t i = 0; i < 5000000; i++) { }
    uint64_t tsc1 = rdtsc64();
    printf("\nTSC delta over the same fixed spin loop (real hardware timing, not interpreted here): 0x%016lx (%lu cycles)\n",
           (unsigned long)(tsc1 - tsc0), (unsigned long)(tsc1 - tsc0));

    printf("\n=== end -- this is PLURAL INTERNAL candidate data only. ===\n");
    printf("=== No claim about real-world constants is made or confirmed here. ===\n");
    printf("=== SINGULAR EXTERNAL confirmation happens outside this probe. ===\n");

    return 0;
}
