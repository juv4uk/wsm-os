#!/usr/bin/env bash
# Validates the hardware-tuned QEMU flags (run-qemu.sh) against real OVMF
# UEFI firmware, with no boot image required. Locates OVMF via the
# ovmf-x86-64 Guix package by default (installed 2026-09-02: this repo
# did not have any OVMF firmware available before that); OVMF_CODE/
# OVMF_VARS can still be overridden explicitly.
#
# Empirically observed (2026-09-02, local run, corrected from an earlier
# false read caused by output buffering in a manual test): this OVMF
# build DOES render console output (ANSI/VT100 escapes, then an
# automatic ">>Start PXE over IPv4." network-boot attempt, since no
# bootable device is attached) over the serial chardev, not only to a
# graphical framebuffer. No boot menu interaction is exercised here
# (5s timeout, no image); this only confirms firmware load + console
# reachability.
set -euo pipefail

: "${QEMU_SYSTEM_X86_64:=qemu-system-x86_64}"
: "${WSM_OS_QEMU_CPU:=Skylake-Client-v1}"
: "${WSM_OS_QEMU_SMP:=4,sockets=1,cores=4,threads=1}"
: "${WSM_OS_QEMU_MEM:=128M}"

if [[ -z "${OVMF_CODE:-}" || -z "${OVMF_VARS:-}" ]]; then
  ovmf_dir=$(guix build ovmf-x86-64 2>/dev/null)/share/firmware
  : "${OVMF_CODE:=$ovmf_dir/ovmf_code_x64.bin}"
  : "${OVMF_VARS:=$ovmf_dir/ovmf_vars_x64.bin}"
fi
[[ -s "$OVMF_CODE" && -s "$OVMF_VARS" ]] || {
  echo "missing OVMF firmware (checked OVMF_CODE=$OVMF_CODE OVMF_VARS=$OVMF_VARS)" >&2
  exit 2
}

if [[ -r /dev/kvm && -w /dev/kvm ]]; then
  accel=kvm
else
  accel=tcg
  echo "note: /dev/kvm not read/write-accessible -- self-testing under TCG, not KVM" >&2
fi

vars_copy=$(mktemp /tmp/wsm-ovmf-vars.XXXXXX)
trap 'rm -f "$vars_copy"' EXIT
cp "$OVMF_VARS" "$vars_copy"
chmod 600 "$vars_copy"

echo "wsm-os QEMU UEFI self-test: cpu=$WSM_OS_QEMU_CPU smp=$WSM_OS_QEMU_SMP mem=$WSM_OS_QEMU_MEM accel=$accel ovmf=$OVMF_CODE" >&2
timeout 5 "$QEMU_SYSTEM_X86_64" \
  -machine q35 \
  -cpu "$WSM_OS_QEMU_CPU" \
  -smp "$WSM_OS_QEMU_SMP" \
  -m "$WSM_OS_QEMU_MEM" \
  -accel "$accel" \
  -drive "if=pflash,unit=0,format=raw,readonly=on,file=$OVMF_CODE" \
  -drive "if=pflash,unit=1,format=raw,file=$vars_copy" \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -monitor none \
  -chardev stdio,id=wsm-serial,mux=off,signal=off \
  -serial chardev:wsm-serial -nographic -no-reboot \
  || true
echo "self-test: OVMF firmware loaded, qemu ran for its timeout window with no fatal error" >&2
