/*
 * entropy-source-probe.c -- what can this machine learn about the real
 * world through its own hardware, right now, empirically -- not talk.
 *
 * Uses RDSEED/RDRAND: real Intel instructions specifically documented
 * to draw from a hardware entropy source (thermal noise conditioned by
 * an on-die DRNG), not a software PRNG -- the concrete, actually-
 * testable version of the shot-noise idea from
 * wsm/research/hardware-native-constants.md. Confirmed present on the
 * owner's real CPU (OWNER-HARDWARE-PROFILE.md ISA feature list:
 * "RDRAND RDSEED"). Whether QEMU/TCG's emulation of these instructions
 * is a genuine passthrough to host entropy or a software stand-in is
 * checked here empirically, not assumed either way.
 *
 * Per the same discipline as every other probe in this repo: this
 * generates PLURAL INTERNAL candidate data (raw samples, basic
 * statistics) -- it does not itself confer any claim about real-world
 * constants. That still needs SINGULAR EXTERNAL confirmation, per
 * wsm/research/the-observer-gap.md.
 */

#include <efi.h>

static EFI_SYSTEM_TABLE *gST;
static void pstr(CHAR16 *s) { gST->ConOut->OutputString(gST->ConOut, s); }
static void phex64(UINT64 v) {
  CHAR16 buf[19]; const CHAR16 *d = L"0123456789ABCDEF";
  buf[0]=L'0'; buf[1]=L'x';
  for (int i=0;i<16;i++) buf[2+i]=d[(v>>((15-i)*4))&0xF];
  buf[18]=0; pstr(buf);
}
static void pdec(UINT64 v) {
  CHAR16 buf[21]; int i=20; buf[20]=0;
  if (v==0) buf[--i]=L'0';
  else while (v>0 && i>0) { buf[--i]=L'0'+(CHAR16)(v%10); v/=10; }
  pstr(&buf[i]);
}
static void pnl(void) { pstr(L"\r\n"); }

static inline void do_cpuid(UINT32 leaf, UINT32 sub, UINT32 *a, UINT32 *b, UINT32 *c, UINT32 *d) {
  __asm__ __volatile__("cpuid" : "=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d) : "a"(leaf),"c"(sub));
}
static inline int rdseed64(UINT64 *out) {
  UINT8 ok;
  __asm__ __volatile__("rdseed %0\n\tsetc %1" : "=r"(*out), "=qm"(ok) :: "cc");
  return ok;
}
static inline int rdrand64(UINT64 *out) {
  UINT8 ok;
  __asm__ __volatile__("rdrand %0\n\tsetc %1" : "=r"(*out), "=qm"(ok) :: "cc");
  return ok;
}
static inline UINT64 rdtsc64(void) {
  UINT32 lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((UINT64)hi << 32) | lo;
}
static inline int popcount64(UINT64 v) {
  int c = 0;
  while (v) { c += (int)(v & 1); v >>= 1; }
  return c;
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  (void)ImageHandle;
  gST = SystemTable;

  pnl();
  pstr(L"=== entropy-source-probe: what THIS machine can learn about reality, empirically ===\r\n\r\n");

  UINT32 a,b,c,d;
  do_cpuid(1,0,&a,&b,&c,&d);
  int has_rdrand = (c >> 30) & 1;
  do_cpuid(7,0,&a,&b,&c,&d);
  int has_rdseed = (b >> 18) & 1;
  pstr(L"CPUID reports RDRAND="); pdec(has_rdrand);
  pstr(L" RDSEED="); pdec(has_rdseed); pnl();

  if (!has_rdseed && !has_rdrand) {
    pstr(L"Neither present per CPUID -- nothing to sample. Stopping honestly, not faking data.\r\n");
    for (;;) { __asm__ __volatile__("hlt"); }
  }

  const UINT32 N = 256;
  UINT64 samples[256];
  UINT32 seed_ok_count = 0, rand_ok_count = 0;
  UINT32 retries_before_success[256];
  UINT32 total_bits_set = 0;

  for (UINT32 i = 0; i < N; i++) {
    UINT64 v = 0;
    UINT32 attempts = 0;
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
    retries_before_success[i] = attempts - 1; /* failures before the success */
    if (ok) { seed_ok_count++; total_bits_set += (UINT32)popcount64(v); }
  }

  pstr(L"Samples requested: "); pdec(N); pnl();
  pstr(L"Successful reads:  "); pdec(seed_ok_count); pnl();
  pstr(L"Total bits set across successful 64-bit samples: "); pdec(total_bits_set);
  pstr(L" of "); pdec((UINT64)seed_ok_count * 64);
  pstr(L" (expect ~50% if genuinely uniform)\r\n");

  UINT64 retry_sum = 0, retry_max = 0;
  for (UINT32 i = 0; i < N; i++) {
    retry_sum += retries_before_success[i];
    if (retries_before_success[i] > retry_max) retry_max = retries_before_success[i];
  }
  pstr(L"Retries-before-success: sum="); pdec(retry_sum);
  pstr(L" mean_x1000="); pdec((retry_sum * 1000) / N);
  pstr(L" max="); pdec(retry_max); pnl();

  pstr(L"\r\nFirst 8 raw 64-bit samples (for independent external inspection, not self-judged here):\r\n");
  for (UINT32 i = 0; i < 8 && i < N; i++) {
    pstr(L"  ["); pdec(i); pstr(L"] "); phex64(samples[i]); pnl();
  }

  UINT64 tsc0 = rdtsc64();
  for (volatile UINT32 i = 0; i < 5000000; i++) { }
  UINT64 tsc1 = rdtsc64();
  pstr(L"\r\nTSC delta over a fixed spin loop (real hardware/TCG timing artifact, not interpreted here): ");
  phex64(tsc1 - tsc0); pnl();

  pstr(L"\r\n=== end -- this is PLURAL INTERNAL candidate data only. ===\r\n");
  pstr(L"=== No claim about real-world constants is made or confirmed here. ===\r\n");
  pstr(L"=== SINGULAR EXTERNAL confirmation happens outside this probe. ===\r\n");

  for (;;) { __asm__ __volatile__("hlt"); }
  return EFI_SUCCESS;
}
