/*
 * entropy-stress-probe.c -- follow-up to entropy-source-probe-native.c.
 * N=256 sequential RDSEED calls showed zero retries on real hardware,
 * same as under QEMU/TCG -- but for a different, honest reason (not
 * enough sampling pressure to exhaust the on-die entropy conditioner's
 * replenishment rate, rather than QEMU simply not modeling exhaustion
 * at all). This raises N by several orders of magnitude, back-to-back
 * with no other work between calls, specifically to try to observe
 * real exhaustion-driven retries -- the only way to get an actual data
 * point for the geometric/Poisson question in
 * wsm/research/hardware-native-constants.md instead of another null
 * result at too-low a sampling rate.
 */
#include <stdio.h>
#include <stdint.h>

static inline int rdseed64(uint64_t *out) {
    uint8_t ok;
    __asm__ __volatile__("rdseed %0\n\tsetc %1" : "=r"(*out), "=qm"(ok) :: "cc");
    return ok;
}

int main(void) {
    const uint32_t N = 2000000;
    uint32_t nonzero_retries = 0;
    uint64_t retry_sum = 0, retry_max = 0;
    uint32_t histogram[32] = {0}; /* retries 0..30, 31+ bucketed at [31] */

    printf("=== entropy-stress-probe: %u back-to-back RDSEED calls, real hardware ===\n", N);

    for (uint32_t i = 0; i < N; i++) {
        uint64_t v;
        uint32_t attempts = 0;
        while (attempts < 100000) {
            attempts++;
            if (rdseed64(&v)) break;
        }
        uint32_t retries = attempts - 1;
        retry_sum += retries;
        if (retries > retry_max) retry_max = retries;
        if (retries > 0) nonzero_retries++;
        uint32_t bucket = retries > 31 ? 31 : retries;
        histogram[bucket]++;
    }

    printf("Total samples: %u\n", N);
    printf("Samples with >=1 retry: %u (%.4f%%)\n", nonzero_retries, 100.0 * nonzero_retries / N);
    printf("Retry sum=%lu mean=%.6f max=%lu\n",
           (unsigned long)retry_sum, (double)retry_sum / N, (unsigned long)retry_max);

    printf("\nHistogram of retries-before-success (bucket 31 = 31 or more):\n");
    for (int i = 0; i < 32; i++) {
        if (histogram[i] > 0) {
            printf("  retries=%2d : count=%u\n", i, histogram[i]);
        }
    }

    /* If retries are genuinely geometric with success probability p per
     * attempt, mean retries = (1-p)/p, so p = 1/(mean+1). Printed raw,
     * not claimed as confirmed -- SINGULAR EXTERNAL judgment happens
     * outside this probe. */
    double mean = (double)retry_sum / N;
    double implied_p = 1.0 / (mean + 1.0);
    printf("\nIf geometric: implied per-attempt success probability p = %.6f\n", implied_p);

    return 0;
}
