#!/usr/bin/env bash
# Reusable benchmark battery for comparing two kernel builds on the same
# physical hardware (Gigabyte H170-Gaming 3 / i5-6400): the stock
# 7.1.5+kali-amd64 vs the custom 7.1.5-native (march=native + localmodconfig).
#
# Two kinds of tests, deliberately kept separate:
#  - KERNEL-SENSITIVE: syscall and context-switch overhead. These exercise
#    the kernel's own code paths and are the only tests here that can
#    plausibly show a difference caused by the kernel build itself.
#  - CPU BASELINE (openssl/7z): dominated by the physical CPU, not by which
#    kernel is booted. Included for completeness, but a delta here mostly
#    reflects run-to-run noise, not the kernel.
#
# Usage: ./native-kernel-benchmark.sh > result-$(uname -r).txt

set -euo pipefail

echo "== ENVIRONMENT =="
date -Is
uname -a
echo

echo "== [KERNEL-SENSITIVE] SYSCALL OVERHEAD (10M getpid() calls) =="
TMPC=$(mktemp --suffix=.c)
TMPBIN=$(mktemp)
cat > "$TMPC" <<'EOF'
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <time.h>
int main(void) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long i = 0; i < 10000000L; i++) {
        syscall(SYS_getpid);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("10M getpid(): %.3f s  (%.1f ns/call)\n", sec, sec * 1e9 / 10000000L);
    return 0;
}
EOF
gcc -O2 "$TMPC" -o "$TMPBIN"
"$TMPBIN"
rm -f "$TMPC" "$TMPBIN"
echo

echo "== [KERNEL-SENSITIVE] CONTEXT-SWITCH OVERHEAD (pipe ping-pong, 200k round trips) =="
TMPC=$(mktemp --suffix=.c)
TMPBIN=$(mktemp)
cat > "$TMPC" <<'EOF'
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>
int main(void) {
    int p1[2], p2[2];
    pipe(p1); pipe(p2);
    pid_t pid = fork();
    char buf = 0;
    long N = 200000;
    if (pid == 0) {
        for (long i = 0; i < N; i++) {
            read(p1[0], &buf, 1);
            write(p2[1], &buf, 1);
        }
        _exit(0);
    } else {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (long i = 0; i < N; i++) {
            write(p1[1], &buf, 1);
            read(p2[0], &buf, 1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        waitpid(pid, NULL, 0);
        double sec = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        printf("%ld round trips: %.3f s  (%.1f us/round-trip, ~%.0f ns/switch)\n",
               N, sec, sec * 1e6 / N, sec * 1e9 / (N * 2));
    }
    return 0;
}
EOF
gcc -O2 "$TMPC" -o "$TMPBIN"
"$TMPBIN"
rm -f "$TMPC" "$TMPBIN"
echo

echo "== [CPU BASELINE, not kernel-sensitive] OPENSSL SPEED (AES-256-CBC, SHA-256) =="
openssl speed -elapsed -evp aes-256-cbc 2>&1 | tail -5
openssl speed -elapsed sha256 2>&1 | tail -5
echo

echo "== [CPU BASELINE, not kernel-sensitive] 7-ZIP BUILT-IN BENCHMARK =="
7z b 2>&1 | tail -20
