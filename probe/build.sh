#!/usr/bin/env bash
# Builds handoff-probe.c into handoff-probe.efi using clang's native
# x86_64-unknown-windows (COFF/PE) target + lld-link, NOT gnu-efi's
# gcc+ld+objcopy ELF-to-PE pipeline. That pipeline was tried first and
# produced a binary that loaded under OVMF but then emitted an endless
# garbage byte pattern instead of running -- see the header comment in
# handoff-probe.c for the full bisection that isolated this. Only
# gnu-efi's headers are still used (portable C structs); its runtime
# library is not linked at all -- handoff-probe.c is self-contained.
set -euo pipefail
cd "$(dirname "$0")"

: "${GNUEFI_DIR:=$(guix build gnu-efi 2>/dev/null)}"
[[ -d "$GNUEFI_DIR" ]] || { echo "gnu-efi not found (only its headers are needed; set GNUEFI_DIR or ensure 'guix build gnu-efi' works)" >&2; exit 2; }
INC="$GNUEFI_DIR/include/efi"

echo "gnu-efi headers: $INC" >&2

clang -target x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone \
  -I"$INC" -I"$INC/x86_64" \
  -c handoff-probe.c -o handoff-probe.o

lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib \
  /out:handoff-probe.efi handoff-probe.o

echo "built: $(pwd)/handoff-probe.efi" >&2
file handoff-probe.efi >&2 || true
