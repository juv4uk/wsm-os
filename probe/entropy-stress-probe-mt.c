/*
 * entropy-stress-probe-mt.c -- follow-up to entropy-stress-probe.c.
 * 2,000,000 back-to-back RDSEED calls on a single thread showed zero
 * retries -- this Skylake's on-die entropy conditioner keeps up with
 * single-threaded demand. Documented real-world RDSEED exhaustion
 * (e.g. the reports this project's own research pointed at) is
 * typically observed under *concurrent* demand from multiple cores
 * hammering the same shared conditioner at once, not single-threaded
 * rate. This spawns one thread per physical core (4 on this i5-6400,
 * confirmed via lscpu, no hyperthreading) all sampling simultaneously.
 */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#define NUM_THREADS 4
#define SAMPLES_PER_THREAD 2000000

static inline int rdseed64(uint64_t *out) {
    uint8_t ok;
    __asm__ __volatile__("rdseed %0\n\tsetc %1" : "=r"(*out), "=qm"(ok) :: "cc");
    return ok;
}

typedef struct {
    int thread_id;
    uint64_t retry_sum;
    uint64_t retry_max;
    uint32_t nonzero_retries;
    uint32_t histogram[32];
} thread_result_t;

static void *worker(void *arg) {
    thread_result_t *r = (thread_result_t *)arg;
    for (uint32_t i = 0; i < SAMPLES_PER_THREAD; i++) {
        uint64_t v;
        uint32_t attempts = 0;
        while (attempts < 100000) {
            attempts++;
            if (rdseed64(&v)) break;
        }
        uint32_t retries = attempts - 1;
        r->retry_sum += retries;
        if (retries > r->retry_max) r->retry_max = retries;
        if (retries > 0) r->nonzero_retries++;
        uint32_t bucket = retries > 31 ? 31 : retries;
        r->histogram[bucket]++;
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    thread_result_t results[NUM_THREADS] = {0};

    printf("=== entropy-stress-probe-mt: %d threads x %d RDSEED calls, concurrent, real hardware ===\n",
           NUM_THREADS, SAMPLES_PER_THREAD);

    for (int i = 0; i < NUM_THREADS; i++) {
        results[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker, &results[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    uint64_t total_samples = (uint64_t)NUM_THREADS * SAMPLES_PER_THREAD;
    uint64_t total_retry_sum = 0, total_max = 0;
    uint32_t total_nonzero = 0;
    uint32_t combined_hist[32] = {0};

    for (int i = 0; i < NUM_THREADS; i++) {
        printf("thread %d: nonzero_retries=%u retry_sum=%lu max=%lu\n",
               i, results[i].nonzero_retries, (unsigned long)results[i].retry_sum, (unsigned long)results[i].retry_max);
        total_retry_sum += results[i].retry_sum;
        total_nonzero += results[i].nonzero_retries;
        if (results[i].retry_max > total_max) total_max = results[i].retry_max;
        for (int b = 0; b < 32; b++) combined_hist[b] += results[i].histogram[b];
    }

    printf("\nTOTAL: samples=%lu nonzero_retries=%u (%.6f%%) retry_sum=%lu mean=%.6f max=%lu\n",
           (unsigned long)total_samples, total_nonzero, 100.0 * total_nonzero / total_samples,
           (unsigned long)total_retry_sum, (double)total_retry_sum / total_samples, (unsigned long)total_max);

    printf("\nCombined histogram of retries-before-success:\n");
    for (int i = 0; i < 32; i++) {
        if (combined_hist[i] > 0) {
            printf("  retries=%2d : count=%u\n", i, combined_hist[i]);
        }
    }

    if (total_retry_sum > 0) {
        double mean = (double)total_retry_sum / total_samples;
        double implied_p = 1.0 / (mean + 1.0);
        printf("\nIf geometric: implied per-attempt success probability p = %.6f\n", implied_p);
    } else {
        printf("\nZero retries even under %d-way concurrent load -- this conditioner outpaces this machine's full demand.\n", NUM_THREADS);
    }

    return 0;
}
