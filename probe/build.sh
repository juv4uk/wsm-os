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
# Usage: ./build.sh [source.c] [subsystem]
#   subsystem: efi_application (default) or efi_boot_service_driver --
#   a driver-subsystem image is meant to be embedded in the firmware
#   itself as EFI_FV_FILETYPE_DRIVER and auto-dispatched by DXE on every
#   boot; an application-subsystem image is meant to be launched
#   manually (Ventoy USB, UEFI Shell). These are NOT interchangeable at
#   the source level -- a driver must return promptly (DXE dispatch
#   blocks on it) where an application may loop forever; see
#   wsm-probe-driver.c's header comment for the concrete case that
#   forced this split.
set -euo pipefail
cd "$(dirname "$0")"

src="${1:-handoff-probe.c}"
subsystem="${2:-efi_application}"
[[ -s "$src" ]] || { echo "source not found: $src" >&2; exit 2; }
base="${src%.c}"

# Prefer Guix (this repo's original environment) if available, else fall
# back to the distro package (e.g. Kali's `apt install gnu-efi`, which
# installs headers straight to /usr/include/efi -- no build/store step).
: "${GNUEFI_DIR:=$(guix build gnu-efi 2>/dev/null || true)}"
if [[ -d "${GNUEFI_DIR:-}" ]]; then
  INC="$GNUEFI_DIR/include/efi"
elif [[ -f /usr/include/efi/efi.h ]]; then
  INC="/usr/include/efi"
else
  echo "gnu-efi headers not found (tried Guix's 'guix build gnu-efi' and /usr/include/efi -- on Debian/Kali: apt install gnu-efi)" >&2
  exit 2
fi

echo "gnu-efi headers: $INC" >&2
echo "building: $src -> $base.efi (subsystem=$subsystem)" >&2

clang -target x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone \
  -I"$INC" -I"$INC/x86_64" \
  -c "$src" -o "$base.o"

lld-link "/subsystem:$subsystem" /entry:efi_main /nodefaultlib \
  /out:"$base.efi" "$base.o"

echo "built: $(pwd)/$base.efi" >&2
file "$base.efi" >&2 || true
