#!/usr/bin/env bash
# mei-client-enum-probe.sh -- розводить дві конкуруючі гіпотези щодо
# ENOTTY, отриманого mei-amt-probe.c при спробі connect до AMT-клієнта
# (UUID 12f80028-b4b7-4b2d-aca8-46e0ff65814c):
#
#   (1) AMT-клієнта немає у списку клієнтів, які FW взагалі репортує
#       через HBM-енумерацію -- висновок коміту 39addf0 ("AMT
#       genuinely absent") правильний.
#   (2) Клієнт у FW є, але це fixed-address клієнт, і драйвер MEI
#       забороняє connect до fixed-address клієнтів на цій системі
#       (dev->hbm_f_fa_supported=false чи allow_fixed_address=0) --
#       тоді ENOTTY означає не "відсутній", а "заблокований політикою
#       драйвера", геть інший стан.
#
# Перевірено напряму по джерелу ядра Linux
# (drivers/misc/mei/main.c, drivers/misc/mei/client.c,
# drivers/misc/mei/debugfs.c), не за переказом:
#   - "UUID не знайдено у FW" -> -ENOTTY, з mei_ioctl_connect_client()
#   - "вже підключено, ліміт вичерпано" -> -EBUSY (окремий код,
#     НЕ ENOTTY -- тобто гіпотеза "інший процес вже тримає лінк"
#     сама по собі не пояснює саме ENOTTY)
#   - "fixed-address заборонено" -> теж -ENOTTY, друга, окрема гілка
#     того самого коду помилки -- ось справжній confound, який
#     mei-amt-probe.c не розрізняє.
#
# Потребує: root (debugfs), реальне фізичне залізо з /dev/mei0.
set -euo pipefail
cd "$(dirname "$0")"

AMT_UUID="12f80028-b4b7-4b2d-aca8-46e0ff65814c"
DBG="/sys/kernel/debug/mei0"

if [[ $EUID -ne 0 ]]; then
  echo "потрібен root (читання debugfs) -- перезапусти через sudo" >&2
  exit 2
fi

if [[ ! -d /sys/kernel/debug ]] || ! mountpoint -q /sys/kernel/debug 2>/dev/null; then
  echo "монтую debugfs..." >&2
  mount -t debugfs none /sys/kernel/debug
fi

if [[ ! -d "$DBG" ]]; then
  echo "не знайдено $DBG -- перевір, що модуль mei/mei_me завантажений і /dev/mei0 існує" >&2
  exit 2
fi

echo "=== mei-client-enum-probe: живий список клієнтів FW через HBM-енумерацію ==="
echo "(LIVE-CONFIRMED, якщо цей рядок видно -- читання з реального заліза, не реконструкція)"
echo

echo "--- $DBG/meclients (формат: |id|fix|UUID|con|msg len|sb|refc|vt|) ---"
meclients_out=$(cat "$DBG/meclients")
echo "$meclients_out"
echo

echo "--- $DBG/devstate (шукай прапорець FA серед HBM feature flags) ---"
devstate_out=$(cat "$DBG/devstate")
echo "$devstate_out"
echo

amt_line=$(grep -i "$AMT_UUID" <<<"$meclients_out" || true)

echo "=== Вердикт ==="
if [[ -z "$amt_line" ]]; then
  echo "AMT UUID ($AMT_UUID) у списку meclients ВІДСУТНІЙ."
  echo "=> Гіпотеза (1) підтверджена: AMT-клієнта немає у FW взагалі."
  echo "   Висновок коміту 39addf0 (\"AMT genuinely absent\") -- LIVE-CONFIRMED цим незалежним методом."
else
  echo "Знайдено рядок AMT-клієнта в meclients:"
  echo "  $amt_line"
  fix_col=$(awk -F'|' '{print $3}' <<<"|$amt_line" | tr -d ' ')
  if [[ "$fix_col" == "1" ]]; then
    echo "=> Гіпотеза (2) підтверджена: клієнт присутній у FW, fix=1 (fixed-address)."
    echo "   ENOTTY у mei-amt-probe.c, найімовірніше, означає \"заборонено драйвером\","
    echo "   НЕ \"відсутній у прошивці\" -- висновок коміту 39addf0 потребує уточнення."
    echo "   Наступний крок: перевір FA-прапорець у devstate вище; якщо вимкнено,"
    echo "   спробуй ./mei-client-enum-probe.sh --try-fixed-address"
  else
    echo "=> Клієнт присутній і НЕ fixed-address (fix=0) -- ENOTTY цим не пояснюється,"
    echo "   потрібно окремо розбиратись у коді mei-amt-probe.c."
  fi
fi

if [[ "${1:-}" == "--try-fixed-address" ]]; then
  echo
  echo "=== --try-fixed-address: тимчасовий, зворотний тест дозволу fixed-address ==="
  afa="$DBG/allow_fixed_address"
  if [[ ! -e "$afa" ]]; then
    echo "$afa не існує в цьому ядрі -- пропускаю" >&2
    exit 0
  fi
  orig=$(cat "$afa")
  echo "поточне значення allow_fixed_address: $orig"
  restore() { echo "$orig" > "$afa"; echo "allow_fixed_address повернуто до $orig"; }
  trap restore EXIT
  echo 1 > "$afa"
  echo "allow_fixed_address встановлено в 1 -- повторюю mei-amt-probe"

  bin="./mei-amt-probe"
  if [[ ! -x "$bin" ]]; then
    echo "збираю mei-amt-probe.c (gcc)..." >&2
    gcc -O2 -o "$bin" mei-amt-probe.c
  fi
  "$bin" || true
  echo "(allow_fixed_address буде автоматично повернуто до $orig при виході)"
fi
