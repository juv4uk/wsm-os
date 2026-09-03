#!/usr/bin/env bash
# heci-trace-capture.sh -- Місія 3C: LIVE HECI TRACE CORRELATION.
#
# Не пише жодного нового коду в ME, не торкається MMIO/BAR напряму,
# не надсилає нових команд у firmware. Вмикає лише два вже наявні,
# LIVE-CONFIRMED ftrace-tracepoints ядра (mei_reg_read, mei_reg_write
# -- підтверджено раніше цієї сесії через /proc/kallsyms,
# hardware/HECI-TRANSPORT-MODEL.md), спостерігає через ЛЕГІТИМНОГО
# власника пристрою (mei_me), не через другого читача BAR0.
#
# Точна послідовність, за прямою вказівкою власника:
#   1. tracing_on=0, очистити trace buffer
#   2. вимкнути ВСІ events (чиста стартова точка)
#   3. увімкнути лише mei_reg_read і mei_reg_write (не mei_pci_cfg_read
#      -- власник явно попросив лише ці два)
#   4. tracing_on=1, кілька секунд idle-baseline
#   5. запустити вже наявний mei-observer fw-version (не новий код)
#   6. одразу tracing_on=0
#   7. зберегти сирий trace без змін
#
# Потребує root (tracefs). Запускати: sudo ./heci-trace-capture.sh
set -euo pipefail
cd "$(dirname "$0")"

if [[ $EUID -ne 0 ]]; then
  echo "потрібен root (tracefs) -- перезапусти через sudo" >&2
  exit 2
fi

TRACE_DIR="/sys/kernel/tracing"
OUT="heci-fw-version.trace"
IDLE_SECONDS="${IDLE_SECONDS:-3}"

if [[ ! -d "$TRACE_DIR/events/mei" ]]; then
  echo "не знайдено $TRACE_DIR/events/mei -- перевір, що модуль mei завантажений" >&2
  exit 2
fi

echo "=== Місія 3C: LIVE HECI TRACE CORRELATION ==="

echo 0 > "$TRACE_DIR/tracing_on"
: > "$TRACE_DIR/trace"
echo 0 > "$TRACE_DIR/events/enable"
echo "trace buffer очищено, усі events вимкнено (чиста стартова точка)"

echo 1 > "$TRACE_DIR/events/mei/mei_reg_read/enable"
echo 1 > "$TRACE_DIR/events/mei/mei_reg_write/enable"
echo "увімкнено лише: mei_reg_read, mei_reg_write"

echo 1 > "$TRACE_DIR/tracing_on"
echo "tracing_on=1 -- idle-baseline ${IDLE_SECONDS}с..."
sleep "$IDLE_SECONDS"

echo "--- запуск mei-observer fw-version ---"
if [[ -x ./mei-observer ]]; then
  ./mei-observer fw-version || true
else
  echo "УВАГА: ./mei-observer не зібраний -- запусти 'gcc -O2 -o mei-observer mei-observer.c' і повтори" >&2
fi

echo 0 > "$TRACE_DIR/tracing_on"
echo "tracing_on=0 -- зупинено одразу після відповіді"

cp "$TRACE_DIR/trace" "$OUT"
chmod 644 "$OUT"
echo "=== збережено: $(pwd)/$OUT ($(wc -l < "$OUT") рядків) ==="
