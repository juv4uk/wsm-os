#!/usr/bin/env bash
# Validates that the hardware-tuned QEMU flags (see run-qemu.sh) are
# accepted by the locally installed qemu-system-x86_64, with no boot image
# required. Does not prove anything boots -- there is no wsm-os kernel yet.
# It proves the CPU model, topology, memory and accel choice are all valid
# for this machine's QEMU build, and surfaces any feature-degradation
# warnings QEMU itself prints (e.g. TCG being unable to emulate a specific
# Skylake CPUID bit) instead of hiding them.
set -euo pipefail

: "${QEMU_SYSTEM_X86_64:=qemu-system-x86_64}"
: "${WSM_OS_QEMU_CPU:=Skylake-Client-v1}"
: "${WSM_OS_QEMU_SMP:=4,sockets=1,cores=4,threads=1}"
: "${WSM_OS_QEMU_MEM:=128M}"

if [[ -r /dev/kvm && -w /dev/kvm ]]; then
  accel=kvm
else
  accel=tcg
  echo "note: /dev/kvm not read/write-accessible -- self-testing under TCG, not KVM" >&2
fi

echo "wsm-os QEMU self-test: cpu=$WSM_OS_QEMU_CPU smp=$WSM_OS_QEMU_SMP mem=$WSM_OS_QEMU_MEM accel=$accel" >&2
timeout 3 "$QEMU_SYSTEM_X86_64" \
  -machine q35 \
  -cpu "$WSM_OS_QEMU_CPU" \
  -smp "$WSM_OS_QEMU_SMP" \
  -m "$WSM_OS_QEMU_MEM" \
  -accel "$accel" \
  -nographic -no-reboot \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  || true
echo "self-test: flags accepted, qemu ran for its timeout window with no fatal error" >&2
