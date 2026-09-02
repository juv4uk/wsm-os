#!/usr/bin/env bash
# Boots handoff-probe.efi under the same hardware-tuned QEMU/OVMF setup
# established in scripts/qemu-uefi-selftest.sh (Skylake-Client-v1,
# 4-core topology, KVM auto-detect falling back to TCG). Requires
# esp.img (run ./build.sh then ./make-esp.sh first).
#
# IMPORTANT scope note: everything this probe prints is
# LIVE-CONFIRMED for the QEMU/OVMF virtual environment it actually ran
# in, on this host -- NOT the owner's physical machine. Booting this
# on real hardware is real, separate, owner-authorized future work
# (see docs/QEMU-SETUP.md's own physical-hardware boundary note).
# Values that depend on real silicon behavior QEMU/TCG does not
# emulate (e.g. the live microcode-revision read) will differ, likely
# substantially, from what the same probe would read on the real i5-6400.
set -euo pipefail
cd "$(dirname "$0")"

[[ -s esp.img ]] || { echo "esp.img not found -- run ./build.sh then ./make-esp.sh first" >&2; exit 2; }

: "${QEMU_SYSTEM_X86_64:=qemu-system-x86_64}"
: "${WSM_OS_QEMU_CPU:=Skylake-Client-v1}"
: "${WSM_OS_QEMU_SMP:=4,sockets=1,cores=4,threads=1}"
: "${WSM_OS_QEMU_MEM:=128M}"
: "${WSM_OS_QEMU_TIMEOUT:=15}"

if [[ -z "${OVMF_CODE:-}" || -z "${OVMF_VARS:-}" ]]; then
  ovmf_dir=$(guix build ovmf-x86-64 2>/dev/null)/share/firmware
  : "${OVMF_CODE:=$ovmf_dir/ovmf_code_x64.bin}"
  : "${OVMF_VARS:=$ovmf_dir/ovmf_vars_x64.bin}"
fi

if [[ -r /dev/kvm && -w /dev/kvm ]]; then
  accel=kvm
else
  accel=tcg
  echo "note: /dev/kvm not read/write-accessible -- running under TCG, not KVM" >&2
fi

vars_copy=$(mktemp /tmp/wsm-probe-vars.XXXXXX)
trap 'rm -f "$vars_copy"' EXIT
cp "$OVMF_VARS" "$vars_copy"
chmod 600 "$vars_copy"

echo "wsm-os handoff-probe: cpu=$WSM_OS_QEMU_CPU smp=$WSM_OS_QEMU_SMP mem=$WSM_OS_QEMU_MEM accel=$accel" >&2
timeout "$WSM_OS_QEMU_TIMEOUT" "$QEMU_SYSTEM_X86_64" \
  -machine q35 \
  -cpu "$WSM_OS_QEMU_CPU" \
  -smp "$WSM_OS_QEMU_SMP" \
  -m "$WSM_OS_QEMU_MEM" \
  -accel "$accel" \
  -drive "if=pflash,unit=0,format=raw,readonly=on,file=$OVMF_CODE" \
  -drive "if=pflash,unit=1,format=raw,file=$vars_copy" \
  -drive "file=esp.img,format=raw" \
  -monitor none \
  -chardev stdio,id=wsm-serial,mux=off,signal=off \
  -serial chardev:wsm-serial -nographic -no-reboot \
  || true
