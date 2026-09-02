#!/usr/bin/env bash
# Builds a probe .c file into a .efi using clang's native
# x86_64-unknown-windows (COFF/PE) target + lld-link, NOT gnu-efi's
# gcc+ld+objcopy ELF-to-PE pipeline. That pipeline was tried first and
# produced a binary that loaded under OVMF but then emitted an endless
# garbage byte pattern instead of running -- see the header comment in
# handoff-probe.c for the full bisection that isolated this. Only
# gnu-efi's headers are still used (portable C structs); its runtime
# library is not linked at all -- every probe here is self-contained.
#
# Usage: ./build.sh [source.c]   (default: handoff-probe.c)
set -euo pipefail
cd "$(dirname "$0")"

src="${1:-handoff-probe.c}"
[[ -s "$src" ]] || { echo "source not found: $src" >&2; exit 2; }
base="${src%.c}"

: "${GNUEFI_DIR:=$(guix build gnu-efi 2>/dev/null)}"
[[ -d "$GNUEFI_DIR" ]] || { echo "gnu-efi not found (only its headers are needed; set GNUEFI_DIR or ensure 'guix build gnu-efi' works)" >&2; exit 2; }
INC="$GNUEFI_DIR/include/efi"

echo "gnu-efi headers: $INC" >&2
echo "building: $src -> $base.efi" >&2

clang -target x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone \
  -I"$INC" -I"$INC/x86_64" \
  -c "$src" -o "$base.o"

lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib \
  /out:"$base.efi" "$base.o"

echo "built: $(pwd)/$base.efi" >&2
file "$base.efi" >&2 || true
