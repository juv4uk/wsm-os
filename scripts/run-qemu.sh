#!/usr/bin/env bash
# Launch a wsm-os boot image under QEMU, configured to match the owner's
# real machine (see docs/OWNER-HARDWARE-PROFILE.md): Intel Core i5-6400,
# Skylake, 1 socket / 4 cores / no SMT. QEMU's -cpu model can only ever be
# an emulated approximation of that silicon, not the real thing -- see
# docs/QEMU-SETUP.md for what is and isn't actually matched, and for the
# TCG CPUID-feature-degradation warnings this configuration is known to
# print when KVM passthrough isn't available (empirically observed, not
# hidden).
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <boot-image> [extra qemu args...]" >&2
  echo "  (no boot image exists in this repo yet -- see docs/QEMU-SETUP.md" >&2
  echo "   and scripts/qemu-selftest.sh to validate the flags without one)" >&2
  exit 2
fi
image=$1
shift
[[ -s "$image" ]] || { echo "missing or empty boot image: $image" >&2; exit 2; }

: "${QEMU_SYSTEM_X86_64:=qemu-system-x86_64}"
: "${WSM_OS_QEMU_CPU:=Skylake-Client-v1}"
: "${WSM_OS_QEMU_SMP:=4,sockets=1,cores=4,threads=1}"
: "${WSM_OS_QEMU_MEM:=128M}"
: "${WSM_OS_QEMU_TIMEOUT:=300}"

# KVM needs both the device node and group membership on it; the owner's
# WSL2 agent user is not currently in the kvm group (confirmed live,
# 2026-09-02), so this degrades to TCG software emulation by default
# rather than failing. Re-check live rather than trusting this comment.
if [[ -r /dev/kvm && -w /dev/kvm ]]; then
  accel=kvm
else
  accel=tcg
  echo "note: /dev/kvm not read/write-accessible to this process -- using TCG (software emulation), not KVM passthrough" >&2
fi

echo "wsm-os QEMU: cpu=$WSM_OS_QEMU_CPU smp=$WSM_OS_QEMU_SMP mem=$WSM_OS_QEMU_MEM accel=$accel storage=nvme" >&2
# Real machine boots from an NVMe SSD (Kingston SNV2S1000G, per
# docs/OWNER-HARDWARE-PROFILE.md) -- emulate an actual NVMe controller
# (-device nvme) instead of a generic/IDE drive, so anything wsm-os later
# does at the storage-driver level is exercised against the same device
# class the owner's real machine actually presents, not a QEMU default.
timeout "$WSM_OS_QEMU_TIMEOUT" "$QEMU_SYSTEM_X86_64" \
  -machine q35 \
  -cpu "$WSM_OS_QEMU_CPU" \
  -smp "$WSM_OS_QEMU_SMP" \
  -m "$WSM_OS_QEMU_MEM" \
  -accel "$accel" \
  -drive "if=none,format=raw,file=$image,id=wsm-nvm0" \
  -device nvme,drive=wsm-nvm0,serial=wsm-os-qemu-nvme0 \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -monitor none \
  -chardev stdio,id=wsm-serial,mux=off,signal=off \
  -serial chardev:wsm-serial -nographic -no-reboot \
  "$@"
