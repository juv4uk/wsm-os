# Карта каналу Host ↔ Intel ME через Linux MEI (GA-H170-Gaming 3 / i5-6400)

**Місія, за якою написано цей документ (2026-09-03, власник)**: пасивно
картографувати вже доступний канал host↔ME, **не змінюючи стан ME,
flash чи firmware**. Дисципліна: `OBSERVE → MAP → VERIFY → DOCUMENT`,
не `OBSERVE → MODIFY`. Критерій завершення: можемо пояснити, яким саме
шляхом один підтверджений read-only запит проходить від Linux
userspace до ME `11.8.50.3425` і назад, і кожна інтерпретація пакета
має незалежне джерело.

**Заборонено на цьому етапі без нового явного дозволу власника** (з
тексту місії, дослівно): `HMRFPO_ENABLE` або інші state-changing MKHI
команди, flash writes/erase, зміна ME-region, зміна Manufacturing
Mode, reset/power-state команди, недокументовані команди навмання,
fuzzing, malformed-packet експерименти, обхід привілеїв/захисту,
робота з SA-00086. Жодна з дій нижче цю межу не перетинає — усе,
описане тут, уже було зроблено раніше в цій сесії (HMRFPO_ENABLE
зокрема) і лише **читається й документується** зараз, не повторюється
й не розширюється.

## Підтверджений шлях каналу

```text
Host userspace (це дослідження, mei-observer.c)
      │
      ▼
  /dev/mei0                                  crw-rw---- root:mei
      │                                      (LIVE-CONFIRMED, доступ без
      │                                       root підтверджено раніше
      │                                       цієї ж сесії)
      ▼
Linux MEI driver (drivers/misc/mei/, ядро 7.1.5+kali-amd64)
      │
      ▼
MEI/HECI transport (PCI 00:16.0, Intel 100/C230 MEI #1, ID 8086:a13a)
      │
      ▼
firmware clients (13 реальних клієнтів, перелік нижче)
      │
      ▼
ME 11.8.50.3425 (Consumer H, Production, 2017-10-25)
```

**Первинні джерела для цього шляху** — перевірено напряму, не
переказано:

- `/usr/include/linux/mei.h` (**локально**, точний UAPI-заголовок цього
  ядра) — визначає `IOCTL_MEI_CONNECT_CLIENT`, `struct
  mei_connect_client_data`, `struct mei_client`. Той самий заголовок,
  який використовують усі проби цього репозиторію.
  Додатково задокументовано, але **не використано** жодною пробою:
  `IOCTL_MEI_NOTIFY_SET`/`IOCTL_MEI_NOTIFY_GET` (підписка на події) і
  `IOCTL_MEI_CONNECT_CLIENT_VTAG` (connect із virtual tag) — поза
  межами цього етапу дослідження.
- `drivers/misc/mei/main.c`, `client.c`, `debugfs.c`, `bus-fixup.c`
  (mainline-ядро torvalds/linux, перевірено раніше цієї ж сесії —
  джерело `ENOTTY`/`EBUSY`-семантики, формат `meclients`, UUID
  `HDCP`/`PAVP`/`MKHIF_FIX`).
- `lspci -nn`: `00:16.0 Communication controller [0780]: Intel
  Corporation 100/C230 Series Chipset Family MEI #1 [8086:a13a]`.

## Три різні речі — розділено явно

```text
transport exists     -- PCI-пристрій 00:16.0 реально є, /dev/mei0 реально є
client exists         -- конкретний UUID реально є у списку meclients
command is understood -- ми реально відправили запит і РОЗШИФРУВАЛИ відповідь
```

Успішний `connect()` підтверджує лише перші дві. Нижче в таблиці явно
видно: усі 13 клієнтів мають підтверджений transport+client, але лише
до одного (`MKHI`) ми реально відправляли команди й розшифровували
відповіді в цій сесії.

## Таблиця реальних клієнтів (LIVE-CONFIRMED через `mei-client-enum-probe.sh`, HBM-енумерація, не переказ)

| UUID | client_id | fixed_addr | con | msg_len | host-драйвер зараз | призначення | джерело | read-only query відомий? |
|---|---|---|---|---|---|---|---|---|
| `b638ab7e-94e2-4ea2-a552-d1c54b627f04` | 41 | 0 | 2 | 4096 | **так**, `mei_hdcp` | HDCP (захист відеоконтенту) | `MEI_UUID_HDCP`, `drivers/misc/mei/bus-fixup.c` | UNKNOWN (протокол не досліджувався нами — керує ним kernel-драйвер) |
| `fbf6fcf1-96cf-4e2e-a6a6-1bab8cbe36b1` | 40 | 0 | 3 | 15360 | **так**, `mei_pxp` | PAVP/Protected Audio-Video Path | `MEI_UUID_PAVP`, той самий файл | UNKNOWN |
| `082ee5a7-7c25-470a-9643-0c06f0466ea1` | 39 | 0 | 1 | 8196 | ні | **UNKNOWN** | — | UNKNOWN |
| `3c4852d6-d47b-4f46-b05e-b5edc1aa440e` | 38 | 0 | 1 | 4096 | ні | **UNKNOWN** | — | UNKNOWN |
| `5565a099-7fe2-45c1-a22b-d7e9dfea9a2e` | 37 | 0 | 1 | 4096 | ні | **UNKNOWN** | — | UNKNOWN |
| `dba4d603-d7ed-4931-8823-17ad585705d5` | 36 | 0 | 1 | 4096 | ні | **UNKNOWN** | — | UNKNOWN |
| `f908627d-13bf-4a04-b91f-a64e9245323d` | 35 | 0 | 1 | 13312 | ні | **UNKNOWN** | — | UNKNOWN |
| `309dcde8-ccb1-4062-8f78-600115a34327` | 34 | 0 | 1 | 4096 | ні | **UNKNOWN** | — | UNKNOWN |
| `8c2f4425-77d6-4755-aca3-891fdbc66a58` | 33 | 0 | 1 | 3092 | ні | **UNKNOWN** | — | UNKNOWN |
| `8e6a6715-9abc-4043-88ef-9e39c6f63e0f` | 32 | 0 | 1 | 2048 | ні (userspace MKHI) | MKHI (генеричний керівний канал) | Linux kernel MEI docs + coreboot | **так, 4 підтверджені (нижче)** |
| `01e88543-8050-4380-9d6f-4f9cec704917` | 15 | 15 | 0 | 36 | ні | **UNKNOWN** (fixed-address) | — | UNKNOWN |
| `42b3ce2f-bd9f-485a-96ae-26406230b1ff` | 8 | 8 | 0 | 3092 | ні | **UNKNOWN** (fixed-address) | — | UNKNOWN |
| `55213584-9a29-4916-badf-0fb7ed682aeb` | 7 | 7 | 0 | 2048 | ні (bus-fixup інлайн) | `MEI_UUID_MKHIF_FIX` (протокольний quirk, не сервіс) | `drivers/misc/mei/bus-fixup.c` | н/д (не сервіс) |

**AMT-клієнт (`12f80028-b4b7-4b2d-aca8-46e0ff65814c`) у цьому списку
відсутній взагалі** — LIVE-CONFIRMED трьома незалежними методами
(`hardware/CSME-ARCHITECTURE.md`, коміти `1fa384f`, `39addf0`,
`be370f3`). 9 клієнтів вище позначені `UNKNOWN` чесно — жодна назва їм
не вигадана, спроба ідентифікації через веб-пошук дала лише
низько-впевнені, неперевірювані здогадки, які свідомо не занесено
сюди як факт.

## Чотири read-only запити — RAW + INTERPRETATION + PROVENANCE + STATUS

Отримано через новий інструмент `probe/mei-observer.c` — мінімальний,
**не** універсальний відправник довільних команд: фіксована таблиця з
чотирьох уже незалежно підтверджених read-only запитів
(`mei-fw-version.c`, `mei-fw-caps.c`, `mei-fw-feature-state.c`,
`mei-hmrfpo-status.c` — той самий wire-протокол, той самий канал MKHI,
переписаний з логуванням сирих байтів). Кожен запуск дописує запис у
`probe/mei-observer.log` (RAW request/response hex, timestamp) —
сирі байти лишаються доступними незалежно від декодера, за вимогою
місії.

### fw-version

```text
REQUEST_BYTES(4):  ff020000
RESPONSE_BYTES(28): ff82000008000b00610d320008000b00610d320008000b00610d3200
MKHI_HEADER: group_id=0xff command=0x02 result=0x00
INTERPRETATION: code=11.8.50.3425 recovery=11.8.50.3425 fitc=11.8.50.3425
PROVENANCE: coreboot util/intelmetool/src/me.h GEN_GET_FW_VERSION (локальний клон)
STATUS: LIVE-CONFIRMED
```

### fwcaps-sku (rule_id=0)

```text
REQUEST_BYTES(8):  0302000000000000
RESPONSE_BYTES(13): 03820000000000000440111031
MKHI_HEADER: group_id=0x03 command=0x02 result=0x00
INTERPRETATION: mefwcaps_sku raw=0x31101140 manageability_bit2=0 pavp_bit12=1
PROVENANCE: coreboot util/intelmetool/src/me.c mkhi_get_fwcaps() (локальний клон)
STATUS: LIVE-CONFIRMED
```

Перехресна узгодженість, не збіг: `manageability_bit2=0` узгоджується
з уже потрійно підтвердженою відсутністю AMT; `pavp_bit12=1`
узгоджується з тим, що `mei_pxp`/`mei_hdcp` реально прив'язані як
драйвери в таблиці клієнтів вище — дві незалежні частини цього
дослідження (SKU-прапорці й живий стан драйверів ядра)
взаємопідтверджуються.

**Реальний баг, знайдений і виправлений у процесі цього ж дослідження,
не залишений на здогад**: перша версія декодера цього запиту
перевіряла `pavp` на біті 13 замість правильного біта 12 (структура
`mefwcaps_sku` з `mei-fw-caps.c`: `full_net`=0…`intel_cls`=6,
`reserved`=7-9, `intel_mpc`=10, `icc_over_clocking`=11, `pavp`=12) —
виправлено до того, як результат потрапив у цей документ.

### fw-feature-state (rule_id=0x20)

```text
REQUEST_BYTES(8):  0302000020000000
RESPONSE_BYTES(13): 03820000200000000440111011
MKHI_HEADER: group_id=0x03 command=0x02 result=0x00
INTERPRETATION: fw_runtime_status raw=0x11101140 PTT(bit29)=0 PSR(bit5)=0
PROVENANCE: coreboot src/soc/intel/common/block/cse/cse.c cse_get_fw_feature_state() (локальний клон)
STATUS: LIVE-CONFIRMED
```

Той самий результат, що вже задокументований раніше цієї сесії — тут
уперше з повними сирими байтами замість лише декодованого підсумку.

### hmrfpo-status

```text
REQUEST_BYTES(4):  05030000
RESPONSE_BYTES(8):  0583000002000000
MKHI_HEADER: group_id=0x05 command=0x03 result=0x00
INTERPRETATION: status=2 (ENABLED)
PROVENANCE: coreboot src/soc/intel/common/block/cse/cse.c cse_hmrfpo_get_status() (локальний клон)
STATUS: LIVE-CONFIRMED
```

`ENABLED` тут — це читання **вже існуючого** стану (встановленого
раніше цієї сесії через `mei-hmrfpo-enable`, ще до цієї дослідницької
місії), не нова, повторно ініційована зміна. Сам `mei-observer` не
містить жодного state-changing запиту в своїй таблиці — `hmrfpo-status`
у ньому лише читає, ніколи не вмикає.

## Контрольний приклад (п.6 місії): STATIC vs LIVE, незалежне узгодження

```text
STATIC firmware analysis (MEAnalyzer, аналіз файлу F22e)
         │
         ├── 11.8.50.3425
         │
LIVE MEI query (mei-observer fw-version, щойно, /dev/mei0)
         │
         └── 11.8.50.3425
```

Два геть різні методи (парсинг файлу прошивки офлайн проти живого
MKHI-запиту через реальний PCI-канал) дають той самий результат — це
й є перевірка того, що observer коректно бачить реальність, а не
артефакт одного інструменту.

## Явні межі — чого ми ще не знаємо (сказано прямо, не замовчено)

- **9 із 13 клієнтів — повністю `UNKNOWN`**: ні призначення, ні
  протоколу, ні того, чи існує безпечний read-only запит до них. Не
  підключались до жодного з них жодним запитом цієї сесії.
- **`MKHI_GROUP_ID_BUP_COMMON` (0xf0) підтверджено мовчазно не
  відповідає** на цьому ME (LIVE-CONFIRMED раніше цієї сесії,
  `hardware/CSME-ARCHITECTURE.md`, коміт `cb76b56`) — `mei-observer`
  свідомо НЕ включає ці дві команди у свою таблицю запитів, хоча вони
  теж формально read-only за задумом coreboot; уникнення зависання
  тут важливіше за повноту списку.
- **`IOCTL_MEI_CONNECT_CLIENT_VTAG` і `IOCTL_MEI_NOTIFY_*`** —
  задокументовані в первинному джерелі (`/usr/include/linux/mei.h`),
  **жодного разу не використані** жодною пробою цього репозиторію.
- **Це дослідження — лише `userspace → Linux → MEI → ME`.** Це НЕ
  доказ того, що майбутній `wsm` після власного `ExitBootServices()`
  зможе говорити напряму `WSM → HECI MMIO → ME`, минаючи Linux і
  kernel-драйвер MEI повністю. Це окрема, майбутня, не досліджена тут
  гіпотеза — навмисно не змішана з цим результатом.

## Критерій завершення місії

Один підтверджений read-only запит (наприклад, `fw-version`) реально
проходить шлях: `userspace mei-observer` → `open(/dev/mei0)`
(дозволено групою `mei`, без root) → `ioctl(IOCTL_MEI_CONNECT_CLIENT)`
до UUID `8e6a6715-...` (первинне джерело: Linux kernel MEI docs) →
`write()` 4 байти MKHI-запиту (структура: coreboot, локальний клон) →
PCI-транспорт 00:16.0 (`lspci`, підтверджено) → ME `11.8.50.3425`
відповідає → `read()` 28 байт → декодовано в `11.8.50.3425` → збігається
зі статичним аналізом того самого значення іншим інструментом
(`MEAnalyzer`). Кожна ланка цього ланцюжка процитована окремо вище.
Місія зупиняється тут — жодної команди state-changing чи прямого
HECI/MMIO не виконано і не заплановано в межах цього документа.
