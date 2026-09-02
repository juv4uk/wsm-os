#!/usr/bin/env bash
# Builds a properly GPT-partitioned raw disk image with a FAT32 EFI
# System Partition containing handoff-probe.efi at \EFI\BOOT\BOOTX64.EFI.
#
# Does NOT use QEMU's own `fat:rw:<dir>` on-the-fly driver -- that was
# tried first and reproduced the same broken-binary symptom this whole
# probe's build was debugged around, which made it look (misleadingly)
# like a disk-layout problem rather than a compiler/linker one. It
# wasn't; see handoff-probe.c's header comment for the real story. This
# script exists mainly so the disk image is a real, inspectable
# artifact (mdir/mcopy-readable) rather than an ephemeral directory
# QEMU maps on the fly.
#
# Uses sgdisk (GPT partitioning) and mtools (mformat/mmd/mcopy, all of
# which operate on a plain file at a byte offset -- no losetup, no
# mount, no root needed).
set -euo pipefail
cd "$(dirname "$0")"

efi_file="${1:-handoff-probe.efi}"
[[ -s "$efi_file" ]] || { echo "$efi_file not found -- run ./build.sh [source.c] first" >&2; exit 2; }

: "${ESP_IMAGE:=esp.img}"
: "${ESP_SIZE_MIB:=64}"

rm -f "$ESP_IMAGE"
dd if=/dev/zero of="$ESP_IMAGE" bs=1M count="$ESP_SIZE_MIB" status=none

sgdisk -o "$ESP_IMAGE" >/dev/null
sgdisk -n 1:2048:0 -t 1:ef00 -c 1:"EFI System" "$ESP_IMAGE" >/dev/null

start_sector=$(sgdisk -i 1 "$ESP_IMAGE" | awk '/^First sector/ {print $3}')
offset=$((start_sector * 512))
echo "ESP partition starts at sector $start_sector (byte offset $offset)" >&2

mformat -i "$ESP_IMAGE@@$offset" -F ::
mmd -i "$ESP_IMAGE@@$offset" ::/EFI
mmd -i "$ESP_IMAGE@@$offset" ::/EFI/BOOT
mcopy -o -i "$ESP_IMAGE@@$offset" "$efi_file" ::/EFI/BOOT/BOOTX64.EFI

echo "built: $(pwd)/$ESP_IMAGE (ESP at byte offset $offset)" >&2
