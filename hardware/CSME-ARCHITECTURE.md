# Intel CSME/ME — архітектура, перевірена проти першоджерела Intel

**Статус: source-confirmed для загальної архітектури CSME-родини; вміст
ME-регіону саме цієї плати тепер теж підтверджений, не `not-yet-
verified`.** Розширює Фазу 1 `docs/POWER-ON-SEQUENCE.md` і закриває
`hardware/bios-f22e/FIT-AND-STRUCTURE-ANALYSIS.md` (розділ 1) повністю
— і *що* таке ME/CSME структурно, і *що саме* лежить у 2 093 056
байтах ME-регіону цієї конкретної плати.

## Точна версія ME-прошивки цієї плати (2026-09-03, MEAnalyzer)

Визначено інструментом `MEAnalyzer` (platomav/MEAnalyzer, форкнуто
власником на `github.com/juv4uk/MEAnalyzer`) напряму проти реального
регіону ME, витягнутого з `H170G3.22e`:

```text
Родина:      CSE ME
Версія:      11.8.50.3425
Реліз:       Production
SKU:         Consumer H
Чипсет:      KBP/BSF/GCF-H A / SPT-H D (Skylake PCH-H — збігається з H170)
Дата:        2017-10-25
Розмір:      0x1BF000
```

**Наслідок для розмови про INTEL-SA-00086** (`hardware/bios-f22e/
modified/README.md` не згадує цього, дивись історію розмови): готові
offset/ROP-таблиці з `kakaroto/IntelTXE-PoC` (гілка `me11`) існують
лише для Skylake **11.0.18.1002** і Kabylake **11.6.0.1126**. Реальна
версія цієї плати — **11.8.50.3425**, новіша за обидві. Публічні
offset-дані НЕ підходять напряму цій прошивці без окремої роботи.

**Реальний баг, знайдений і виправлений у самому MEAnalyzer**: `colorama`
на цій версії Python (3.14) кидає `AttributeError` при спробі
встановити заголовок вікна терміналу через OSC-послідовність
(`winterm` є `None` на Linux, а код це не перевіряє). Виправлено в
форку власника — обгорнуто в `try/except AttributeError`, суто
косметичний рядок, не впливає на саму логіку розбору прошивки.

## Учасники

```text
Автор:    GPT-5.6 Sol (OpenAI), передано через Volodymyr
Роль:     дослідницький партнер WSM Foundations Research
Внесок:   вихідний розлогий пояснювальний виклад архітектури
          ME/CSME, її ролі при вмиканні живлення, стосунку до
          ExitBootServices() і меж SMM/CSME як окремих обчислювальних
          доменів

Автор:    Claude Sonnet 5 (Anthropic)
Роль:     дослідницький партнер WSM Foundations Research
Внесок:   перевірка тверджень проти першоджерела Intel (не прийнято
          на віру), цей документ
```

## Дисципліна перевірки

За вже встановленим у цьому репозиторії правилом ("перевірити ключові
технічні твердження, а не просто прийняти цитати на віру" — той самий
підхід, що застосований до shot-noise-твердження в
`wsm/research/hardware-native-constants.md`), кожне твердження нижче
звірене напряму проти офіційного документа Intel, не проти переказу:

```text
Джерело [1]: intel.com/.../000008927/ (стаття підтримки Intel) —
             WebFetch успішний, текст прочитано напряму.
Джерело [2]: "Intel® Converged Security and Management Engine
             (Intel® CSME) Security — Technical White Paper",
             Document Number 631900, Revision 1.5, жовтень 2022 —
             WebFetch PDF не спарсив текст; прочитано напряму через
             Read (сторінки 1, 3-14, 19-21) зі збереженого локально
             файлу.
```

## Підтверджено напряму цитатами з [1]

- «embedded microcontroller» — «running a lightweight microkernel
  operating system»
- «At system initialization, the Intel® Management Engine loads its
  code from system flash memory.»
- «This allows the Intel® Management Engine to be up before the main
  operating system is started.»
- «A fundamental feature of the Intel® Management Engine is that its
  power states are independent of the host OS power states.»

## Підтверджено напряму цитатами з [2] (офіційний білий папір Intel, ред. 1.5, 2022)

**Окремий процесор, апаратно ізольований від головного CPU:**

> "Dedicated CPU, a 32 bits processor based on Intel 486 architecture,
> supporting privilege execution levels, aka rings, segmentation, MMU
> (Memory Management Unit) for page management and also CET (Control
> Enforcement Technology) starting Tiger Lake platform."

Це реальний, окремий x86-сумісний процесор (архітектура Intel 486,
32-біт) — не метафора й не спільне використання головного CPU-ядра.

**Власна, ізольована SRAM, з конкретним діапазоном розміру:**

> "Dedicated, internal SRAM (Static Random-Access Memory) isolated from
> host and that cannot be probed via the chipset's external interfaces.
> The size of the SRAM ranges from 512KB to 1,920KB, depending on the
> Intel® CSME SKU."

**Термін "Ring -3" — не вживається самим Intel у цьому документі.**
Натомість білий папір явно застерігає власною приміткою:

> "Note: Any reference to ring in the rest of this document refers to
> the CSME CPU rings and not the Main CPU rings."

Тобто офіційний документ Intel описує **власну, окрему систему
привілей-рівнів CSME** (uKernel виконується на CSME-ring0, застосунки/
драйвери/сервіси CSME — на CSME-ring3), явно відокремлену від рингів
головного CPU — але ніде в цьому документі Intel сама не вживає
неформальний публічний термін "Ring -3". Цей термін — зовнішній
(дослідницький/публіцистичний) спосіб описати той факт, що CSME працює
з привілеями, не підпорядкованими й не видимими для головного CPU
взагалі, а не офіційна назва Intel. Різниця варта фіксації: сам факт
ізоляції — source-confirmed; конкретний ярлик "Ring -3" — не
Intel-термінологія, зовнішня метафора для того самого факту.

**Апаратний криптодвигун, окремий від головного CPU:**

> "OCS (Offload and Cryptography Subsystem) is a Cryptography-HW
> accelerator with DMA (Direct-Memory-Access) engine and Secure-Key-
> Storage (SKS) Hardware."

**I/O ізоляція через власний IOMMU:**

> "System Agent allows the CSME CPU to securely access SRAM and helps
> enforce access control to SRAM from internal/external devices
> (example, DMA access) by using a dedicated IOMMU (Input Output Memory
> Management Unit)."

**Власна ОС на базі Minix, мікроядерна архітектура:**

> "CSME OS (Operating System) is based on Minix OS architecture and has
> two main security principles: a Micro-Kernel-OS architecture and a
> minimal TCB. By design, the micro-kernel is the only runtime
> component executing at Intel® CSME ring0, while Intel® CSME Firmware
> applications, drivers and services all run at Intel® CSME ring3."

Це пряме, першоджерельне підтвердження вже відомого публічно факту
(розкритого дослідниками безпеки 2017 року) — CSME справді виконує
власну ОС, побудовану на архітектурі Minix, з мікроядром на найвищій
внутрішній привілеї (CSME-ring0) і всім рештою (застосунки, драйвери,
сервіси, включно з AMT) — на CSME-ring3.

**Потік завантаження власної прошивки CSME** (Figure 3, документ [2]):

```text
ROM (Ring0, апаратний корінь довіри, без механізму патчів після
     виготовлення кремнію)
  -> RBE (ROM Boot Extension)
  -> uKernel (Ring0 — єдиний компонент на цій привілеї)
  -> TCB OS
  -> Bringup (BUP)
  -> Drivers (Ring3)
  -> Services (Ring3)
  -> Applications (Ring3 — сюди належить AMT)
```

## Важливе застереження щодо застосовності до саме цієї плати

Білий папір [2] explicitly документує **CSME 14.0 (Comet Lake), 15.0
(Tiger Lake) і 16.0 (Alder Lake)** — покоління чипсетів, пізніші за
плату власника. Уже `STATIC-CONFIRMED` в цьому репозиторії
(`hardware/bios-f22e/FIT-AND-STRUCTURE-ANALYSIS.md`), CPU власника —
Skylake (сигнатура `0x000506E3`), а материнська плата — GA-H170-Gaming
3 (чипсет H170, те саме покоління Skylake, ~2015). Це означає покоління
Intel ME на реальному залізі власника — раніше за CSME 14–16, описані в
[2] (публічно відоме як **ME 11.x**, генерація, що вперше запровадила
саме цю Minix-based мікроядерну x86-архітектуру для Skylake-платформ —
загальновідомий факт з незалежних досліджень безпеки 2017 року, **не
верифікований у цій сесії напряму проти байтів ME-регіону саме цього
образу**).

**Що це означає для статусу тверджень вище**: сама архітектурна модель
(окремий x86-CPU, власна ізольована SRAM, мікроядро на базі Minix,
власні ring0/ring3, окремий IOMMU, окремий крипто-двигун) —
`source-confirmed` як опис CSME-родини Intel загалом і, ймовірно,
застосовна до ME 11.x цієї плати з огляду на архітектурну
спадкоємність, яку публічно задокументовано незалежними дослідниками —
але **не** `STATIC-CONFIRMED` для точних чисел саме цього SKU (точний
розмір SRAM у діапазоні 512КБ–1920КБ, точна схема підпису RSA-3072 чи
RSA-2048 тощо) — ті числа стосуються CSME 14/15/16 буквально, за
текстом документа, і екстраполяція на ME 11.x цієї плати без прямого
розбору 2 093 056 байтів її власного ME-регіону лишається `predicted`,
не перевіреним. **Уточнення нижче закриває частину цієї прогалини**:
сама версія прошивки ME (11) тепер `source-confirmed` напряму для
точного SKU власника, не екстрапольована — див. наступний розділ.

## Доповнення: точні факти SKU для H170, підтверджені напряму з ARK Intel

Наступний раунд (той самий партнер, GPT-5.6 Sol через Volodymyr)
запропонував накласти архітектуру CSME конкретно на залізо власника.
Перевірено напряму через сторінку специфікацій Intel ARK для чипсета
H170 (`intel.com/.../90595/intel-h170-chipset/specifications.html`,
WebFetch успішний, витяг точних значень зі сторінки):

```text
Intel® ME Firmware Version:        11
Intel® Standard Manageability:     No
Intel® Platform Trust Technology:  Yes
Intel® Boot Guard:                 Yes
```

Це `source-confirmed` напряму для точної моделі чипсета власника
(H170), не загальна екстраполяція. Підтверджує ключове застереження з
оригінального викладу: **наявність ME 11 не означає наявність AMT** —
H170 явно позначений "Standard Manageability: No". Білий папір [2] сам
підтверджує цю логіку незалежно: "Intel® AMT is not supported on
consumer SKUs, Intel® Atom platforms or servers" (стор. 22) — H170 як
mainstream/consumer-чипсет підпадає під це явне виключення.

## Доповнення: SMM і ME stolen memory — дійсно окремі, підтверджено для точно цього покоління CPU

Перевірено напряму проти офіційного даташита процесора (не чипсета):
**"6th Generation Intel® Processor Datasheet for S-Platforms — Volume 2
of 2"** (лютий 2016, Order No. 332688-003EN) — це те саме покоління
Skylake, що й реальний CPU власника (`0x000506E3`,
`STATIC-CONFIRMED` в `hardware/bios-f22e/FIT-AND-STRUCTURE-ANALYSIS.md`),
не наближення з іншого покоління.

Розділи 2.10 "System Management Mode (SMM)" і 2.12 "Intel® Management
Engine (Intel® ME) Stolen Memory Accesses" — окремі, сусідні розділи
офіційного даташита, точно як стверджував оригінальний виклад.
Підтверджено дослівно, і механізм ізоляції описаний навіть точніше, ніж
у переказі:

> "DMI Interface and PCI Express* masters are **Not** allowed to access
> the SMM space."

ME отримує доступ до власної "stolen memory" області DRAM через окремий
шлях декодування (PCH-accesses mapped to VCm, routed non-snooped
directly to DRAM), відмінний від шляху доступу до SMM-простору — це два
структурно розділені механізми ізоляції пам'яті на одному й тому самому
CPU, не один спільний. `source-confirmed`, пряма цитата з офіційного
джерела для точно цього покоління заліза.

## Застереження щодо двох цитат оригінального викладу, не перевірених як точні

Оригінальний виклад (GPT-5.6 Sol через Volodymyr) навів два додаткові
джерела, які за перевіркою виявилися датшитами **іншого покоління
платформи**, не Skylake/H170 власника:

- Джерело про власну локальну RAM ME виглядало як даташит чипсета
  C200-серії (Sandy Bridge, ~2011) — на два покоління старіше за
  реальне залізо власника.
- Джерело про архітектуру flash-дескриптора й master-права виглядало як
  даташит PCH для Tiger Lake — на чотири покоління новіше.

Це не спростовує самі твердження (архітектура flash-дескриптора з
master-правами вже незалежно `STATIC-CONFIRMED` цим репозиторієм через
власний парсинг дескриптора — `FIT-AND-STRUCTURE-ANALYSIS.md`), але
конкретні цитовані джерела не встановлюють ці факти саме для покоління
Skylake/H170 власника. Позначено як `not-yet-verified` для точності
цитати, `general` для самого твердження — та сама дисципліна, що вже
виявила подібну неточність із shot-noise notation trap
(`wsm/research/hardware-native-constants.md`).

## Доповнення: HECI/MEI — реальний канал зв'язку хост↔CSME, підтверджено дослівно

Оригінальний виклад стверджував існування HECI (Host Embedded
Controller Interface, з боку ОС відомий як драйвер MEI). Підтверджено
дослівно з того самого білого паперу [2] (стор. 22):

> "An example of a driver is HECI, which can provide a transport
> mechanism by which the host and Intel® CSME can pass messages to one
> another through two, circular buffers: one for messages sent from
> Intel® CSME to the host and the other for messages in the opposite
> direction. [...] On the host OS, by design, the HECI driver is
> installed as a kernel-mode driver, also known as Intel® MEI (Intel®
> Management Interface)."

Канал реальний, `source-confirmed`, і структурно симетричний (два
циклічні буфери, по одному в кожному напрямку) — саме так, як
стверджував оригінальний виклад. Використання цього каналу самим `wsm`
чи `wsm-os` **не заплановане й не почате** — записано лише те, що канал
існує, за тією самою дисципліною "не робити зайвого", яку вже
застосовано до пропозиції написати власний HECI-клієнт.

## Зв'язок із межею `ExitBootServices()` цього репозиторію

Оригінальний виклад GPT-5.6 Sol (переданий через Volodymyr, не
відтворений тут дослівно) стверджував, що перетин `ExitBootServices()`
у `probe/exit-boundary-probe.c` не стирає й не бере під контроль
CSME/SMM — вони лишаються окремими обчислювальними доменами. Це
узгоджується зі щойно підтвердженим фактом: CSME — апаратно окремий
процесор із власною SRAM, що працює незалежно від стану живлення
головного хоста ([1]: "power states are independent of the host OS
power states") — тобто `ExitBootServices()`, який діє винятково в
адресному просторі й на CPU головного хоста, структурно не може
торкнутися CSME, який ніколи не був частиною цього простору. Це
підсилює, не послаблює, уже записаний висновок
`docs/POWER-ON-SEQUENCE.md` (Фаза 1): межа, яку `wsm-os` реально
перетнула — це межа UEFI-хоста, не межа всієї платформи.

## Доповнення: AMT підтверджено відсутнім трьома незалежними методами; HMRFPO_ENABLE реально спрацював на живому залізі (2026-09-03)

**AMT — три незалежні методи, усі узгоджені.** Оригінальний факт
("Standard Manageability: No" з ARK Intel, вище) тепер додатково
`LIVE-CONFIRMED` не одним, а трьома різними шляхами:

1. `FWCAPS_GET_RULE` (`probe/mei-fw-caps.c`) — прапорець `Manageability=OFF`.
2. Пряма спроба MEI connect-handshake до UUID самого AMT-клієнта
   (`12f80028-b4b7-4b2d-aca8-46e0ff65814c`, з реального джерела fwupd)
   — `probe/mei-amt-probe.c` (коміт `39addf0`) — відмова з `ENOTTY`.
3. Пряма енумерація списку клієнтів, які FW репортує через HBM,
   незалежно від шляху `connect`-ioctl — `probe/mei-client-enum-probe.sh`
   (коміт `be370f3`) — AMT UUID відсутній серед 13 реальних клієнтів
   цього чипа.

Метод 3 додатково закрив конкретну альтернативну гіпотезу: перевірено
напряму по джерелу ядра Linux (`drivers/misc/mei/main.c`), що `ENOTTY`
з методу 2 повертається з двох різних гілок коду — "UUID не знайдено у
FW" і "клієнт є, але fixed-address заборонено драйвером" — і лише пряма
енумерація (метод 3) розрізняє їх. На цій машині `FA: 1` (fixed-address
підтримується драйвером), і AMT просто відсутній у самому списку —
перша гілка, не друга.

**Живий список 13 MEI-клієнтів цього чипа, з реальною прив'язкою
драйверів** (`/sys/bus/mei/devices/*/uevent`, `lsmod`):

| UUID (перші 8 симв.) | Ідентифіковано як | Драйвер прив'язаний зараз? |
|---|---|---|
| `8e6a6715` | MKHI (генеричний керівний канал, той самий, яким користуються всі проби цього репо) | немає окремого host-драйвера (userspace read/write напряму) |
| `b638ab7e` | `MEI_UUID_HDCP` (source-confirmed, `drivers/misc/mei/bus-fixup.c`) | так — `mei_hdcp` |
| `fbf6fcf1` | `MEI_UUID_PAVP`/PXP (source-confirmed, той самий файл) | так — `mei_pxp` |
| `55213584` | `MEI_UUID_MKHIF_FIX` (протокольний quirk, не сервіс) | немає (bus-fixup застосовується інлайн, не окремий модуль) |
| решта 9 | не ідентифіковано (пошук дав лише непідтверджені здогадки — свідомо не записано як факт) | немає жодного драйвера, `DRIVER=` відсутній в `uevent` |

Практичний висновок: реальна поверхня, якою ME могла б штовхати дані
в бік ОС на цій машині прямо зараз, звужена до двох вузьких, реально
прив'язаних каналів (`mei_hdcp`, `mei_pxp`, обидва з `0` активних
користувачів на момент перевірки) — не загальний контроль над ОС.

**HMRFPO_ENABLE — реально спрацював, `LIVE-CONFIRMED`.**
`probe/mei-hmrfpo-status.c` (read-only) спершу показав `status=0`
(DISABLED — хост не має доступу до ME-регіону прошивки). Після
запуску `probe/mei-hmrfpo-enable.c` (надсилає `HMRFPO_ENABLE`,
group=0x05/cmd=0x01, вимагає відкритого Manufacturing Mode) той самий
`mei-hmrfpo-status` повторно показав `status=2` (ENABLED) — хост
реально отримав доступ на запис до ME-регіону прошивки, до наступного
скидання ME/перезавантаження. Обидва файли поки **не закомічені** в
git (`?? probe/mei-hmrfpo-*.c` на момент цього запису) — рішення про
коміт лишається за власником.

## Доповнення: доступ до `/dev/mei0` без root (2026-09-03)

До цього моменту кожна проба цієї сесії (сім штук) працювала лише
через `sudo` — `/dev/mei0` за замовчуванням `crw------- root:root`
(підтверджено напряму: `getfacl` порожній, жодного `udev`-правила для
`mei` в системі не було). Це змінено явною, свідомою дією, не сталося
"само собою":

```bash
sudo groupadd -f mei
sudo usermod -aG mei user
echo 'SUBSYSTEM=="mei", GROUP="mei", MODE="0660"' | sudo tee /etc/udev/rules.d/60-mei.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Результат — `crw-rw---- root:mei` — підтверджено `LIVE-CONFIRMED`
реальним запуском проби (`mei-hmrfpo-status`) без `sudo` через `sg mei
-c "..."` (обгортка потрібна для вже запущених процесів/шеллів, які
стартували до зміни членства в групі — нове членство діє лише в нових
сесіях логіну).

**Чесно про наслідок, записано прямо, не замовчено**: після цієї зміни
**будь-який процес** під обліковим записом `user` на цій постійно
ввімкненій машині може відкривати `/dev/mei0` без пароля — це реальне
розширення поверхні доступу до ME, а не лише зручність для агента.

## Доповнення: `MKHI_GROUP_ID_BUP_COMMON` (0xf0) не відповідає на цьому ME взагалі — `LIVE-CONFIRMED` негативний результат (2026-09-03)

Дві проби, `probe/mei-cse-boot-partition-info.c`
(`GET_BOOT_PARTITION_INFO`, cmd `0x1c`) і `probe/mei-cse-boot-perf.c`
(`GET_BOOT_PERF_DATA`, cmd `0x08`) — обидві з групи `0xf0`
(`MKHI_GROUP_ID_BUP_COMMON`), структури взяті напряму з **локального**
клону `coreboot` (`/home/user/GitHub/coreboot`, не з мережі —
власна помилка цієї сесії щодо цього вже виправлена за зауваженням
власника).

**Реальний результат, обидві команди**: `connect` і `write` проходять
успішно (`ioctl` і `write()` не повертають помилки), але **відповідь
ніколи не приходить** — `read()` зависає нескінченно, підтверджено
через `timeout 8` (код виходу `124` в обох випадках). Це не помилка
MKHI (`result != 0` не приходить взагалі — самого заголовка відповіді
немає), а справжня, симетрична тиша на рівні прошивки для всієї групи.

**Висновок**: `CSE Lite` (RO/RW-редундантність партицій ME,
концепція, якій належить `GET_BOOT_PARTITION_INFO`) і телеметрія
завантаження (`GET_BOOT_PERF_DATA`) — обидві, найімовірніше, фічі
новіших поколінь Intel CSME, яких немає на цьому ME 11.x/Skylake.
Симетричність результату на двох незалежних підкомандах тієї самої
групи — сильніший доказ, ніж один негативний результат: уся група
`0xf0` не реалізована на цьому чипі, не лише одна команда.

**Практичний урок, записаний для майбутніх сесій**: будь-яку нову,
ще не перевірену MKHI-команду запускати лише через `timeout`, а не
напряму — мовчазне зависання на `read()`, а не помилка, виявилось
реальним, повторюваним режимом відмови на цьому залізі.

## Доповнення: живий стан фіч ME (PTT/PSR) через `FWCAPS_GET_RULE(rule_id=0x20)` (2026-09-03)

`probe/mei-fw-feature-state.c` (коміт `c755884`) — той самий провідний
канал MKHI, що й `mei-fw-caps.c`, той самий wire-command
(`group=0x03/cmd=0x02`), але з `rule_id=0x20`
(`ME_FEATURE_STATE_RULE_ID`, з `intelblocks/cse.h` coreboot) замість
`0`. Різне питання, та сама командна сім'я: `mei-fw-caps.c` читає
статичні можливості SKU ("на що ця SKU здатна"), ця проба читає живий
runtime-стан фіч ("що реально увімкнено прямо зараз").

**Реальний, `LIVE-CONFIRMED` результат:**

```
raw fw_runtime_status = 0x11101140
```

- **PTT (Platform Trust Technology / fTPM) — вимкнено** (біт 29 = 0).
- **PSR (Platform Service Record) — вимкнено** (біт 5 = 0).
- **Шість реально увімкнених бітів без публічної назви**: 6, 8, 12,
  20, 24, 28. Перевірено напряму весь `intelblocks/cse.h` coreboot —
  названо в ньому лише `PTT` і `PSR`, решта цього 32-бітного
  `fw_runtime_status` публічно не задокументована взагалі (внутрішня
  специфікація Intel). Свідомо не вигадано їм назв — записано як
  реальний, live факт без інтерпретації.

## Доповнення: чому пряме читання ME-регіону через host SPI не вдалося — три реальні глухі кути (2026-09-03)

Спроба відповісти на питання з попереднього розділу ("що можна дістати
з цього каналу") практичним читанням самого ME-регіону через
`flashrom` наштовхнулась на три окремі, кожна по-своєму остаточні,
перешкоди — записано чесно, щоб наступна сесія не витрачала час на
повторну спробу тих самих шляхів:

1. **`flashrom -p linux_mei` — такого програматора не існує.**
   Перевірено напряму: `flashrom v1.6.0` на цій машині видає повний
   список валідних програматорів при помилці, і серед них немає
   `linux_mei`; так само немає його в жодній версії офіційної
   документації `flashrom` (аж до dev-гілки). MEI-специфічного шляху
   читання flash у `flashrom` просто не існує — це була власна
   помилкова пропозиція цієї сесії, не факт про залізо.

2. **`flashrom -p internal --ifd` — блокується `CONFIG_STRICT_DEVMEM`.**
   Реальна помилка: `pcilib: Cannot map ecam region: Operation not
   permitted`. Причина перевірена напряму на живому ядрі
   (`7.1.5+kali-amd64`):
   ```
   CONFIG_STRICT_DEVMEM=y
   CONFIG_IO_STRICT_DEVMEM=y
   ```
   Це той самий бар'єр, що вже раніше зупинив `intelmetool` (згадано в
   заголовку `probe/mei-fw-version.c`) — `internal`-програматор
   потребує прямого мапування фізичної пам'яті (`/dev/mem`) для
   ECAM-простору PCI-конфігурації, а це ядро це забороняє поза білим
   списком "безпечних" ділянок.

3. **`flashrom -p linux_mtd` (через окремий kernel-драйвер
   `intel-spi`, який обходить `/dev/mem`) — генетично не підтримує цю
   плату.** Модуля `intel_spi` немає в цьому ядрі взагалі
   (`modinfo`/`/lib/modules/` порожньо). Але навіть якби його зібрати
   з нуля під це ядро — перевірено напряму по джерелу mainline-ядра
   (`drivers/spi/spi-intel-pci.c`): таблиця підтримуваних PCI ID цього
   драйвера **не містить `0x8086:0xa144`** (наш реальний `H170
   Chipset LPC/eSPI Controller`) — драйвер підтримує лише Broxton
   (`bxt_info`) і Cannon Lake (`cnl_info`), новіші покоління PCH, не
   Sunrise Point/100-series (наше, Skylake-ери). Це не "складно
   зібрати" — це архітектурно непідтримуване покоління чипсета для
   цього драйвера, підтверджено джерелом, а не здогадкою.

**Правдоподібна, але не остаточно підтверджена нитка, що стикує це з
розділом про HMRFPO вище**: публічні джерела (`flashrom`'s власна
документація) розходяться в тому, чи має ефект `HMRFPO_ENABLE`,
надісланий **після** `End-of-Post` (EOP) — а EOP на цій машині вже
відбувся під час звичайного завантаження BIOS, задовго до того, як
`mei-hmrfpo-enable` було запущено з живого Linux. Тобто MKHI-рівень
(`HMRFPO_GET_STATUS` = ENABLED) міг реально змінитись, а реальне
блокування на рівні самого SPI-контролера (яке залежить від
FRAP-регістра, встановленого до/на EOP) — ні. Це `predicted`
пояснення, не `source-confirmed` до кінця — публічна документація тут
сама неповна.

**Три реальні варіанти, що лишаються, кожен зі своєю ціною — не
вирішено, чекає рішення власника:**
1. Перезавантаження з `iomem=relaxed` у cmdline ядра — послаблює
   `STRICT_DEVMEM` системно (тимчасово, до наступного ребута).
2. Компіляція `intel-spi`-подібного драйвера з нуля під це покоління
   PCH — реальна, не розпочата робота з нуля (не форк наявного
   драйвера, бо той генетично не підтримує цей чипсет).
3. Зовнішній SPI-програматор (кліпса CH341A на сам чип) — обходить
   хостову ОС повністю, стандартний шлях `me_cleaner`-гайдів.

## Що лишається невідкритим

Вміст самих 2 093 056 байтів ME-регіону цього образу (`STATIC-CONFIRMED`
за межами/офсетом у `FIT-AND-STRUCTURE-ANALYSIS.md`, `not-yet-verified`
всередині) — розбір власне прошивки ME цієї плати не входив у цей
прохід і лишається окремою, явно не розпочатою роботою. Три реальні
шляхи до живого читання цього регіону з хоста досліджено й записано
вище (жоден поки не спрацював); зовнішній програматор або
`iomem=relaxed` лишаються відкритими, не випробуваними напрямами.

---

## Intel CSME/ME architecture, verified against Intel's own source (English, secondary)

Verified, against Intel's own official CSME Security White Paper (Doc.
631900, Rev 1.5, Oct 2022) and Intel's public support article, the key
claims from an earlier long GPT-5.6 Sol explainer (relayed by
Volodymyr): CSME has its own dedicated 32-bit Intel-486-architecture
CPU, its own isolated SRAM (512KB-1920KB depending on SKU), a Minix-OS-
based microkernel running at its own CSME-ring0 (apps/drivers/services
at CSME-ring3), its own IOMMU-enforced I/O isolation, and its own
crypto-HW accelerator (OCS). One correction found: Intel's own document
never uses the informal "Ring -3" term — it explicitly notes its own
ring references are to CSME's own rings, not the main CPU's; "Ring -3"
is an external/informal label for that isolation, not Intel's own
terminology. Caveat: the white paper documents CSME 14-16 (Comet Lake
onward); the owner's actual board (GA-H170-Gaming 3, Skylake CPU
signature `0x000506E3`, already STATIC-CONFIRMED elsewhere in this
repo) runs an earlier ME generation (publicly known as ME 11.x) — the
architectural model likely applies via documented continuity, but this
session did not verify it against this board's actual 2,093,056-byte ME
region, which remains unparsed.

A follow-up round (same source, GPT-5.6 Sol via Volodymyr) mapped this
onto the owner's exact hardware. Verified directly against Intel ARK's
H170 chipset spec page: ME Firmware Version 11, Standard Manageability
No, PTT Yes, Boot Guard Yes — source-confirmed for this exact SKU, not
extrapolated, and it directly confirms ME 11 does not imply AMT (the
white paper itself independently states AMT is unsupported on consumer
SKUs). Verified directly against Intel's own 6th-Gen Core (Skylake) S-
Platform processor datasheet Vol. 2 (Feb 2016, Order No. 332688-003EN —
the exact CPU generation, not an approximation): SMM and ME stolen-
memory access are indeed separate, adjacent sections (2.10 and 2.12),
with an explicit isolation statement ("DMI Interface and PCI Express*
masters are Not allowed to access the SMM space") and a structurally
distinct DRAM-decode path for ME's own stolen memory. HECI/MEI (host-
CSME two-circular-buffer transport) confirmed verbatim from the white
paper. Two of the original message's citations turned out to be for
mismatched platform generations (a Sandy-Bridge-era C200 chipset
datasheet, and a Tiger-Lake PCH datasheet) rather than Skylake/H170 —
flagged as a citation-accuracy issue, not a refutation of the underlying
claims, which are independently supported elsewhere. See the Ukrainian
version above for full detail and exact quotes.

A later 2026-09-03 follow-up (same session) closed out the AMT question
with three independent, mutually-agreeing methods (capability-flag read,
direct connect-handshake, and direct FW client-list enumeration via
debugfs), enumerated all 13 live MEI clients on this exact chip and
found only two with a bound host kernel driver right now (`mei_hdcp`,
`mei_pxp` — both HDCP/protected-video, not general OS control), and
confirmed `HMRFPO_ENABLE` live-succeeded, giving the host write access
to the ME flash region until the next ME reset. See the Ukrainian
section above for the full client table and command references.

A same-day follow-up read the live ME feature-runtime-state bitmask
(`FWCAPS_GET_RULE` with `rule_id=0x20`): PTT (fTPM) and PSR are both
off; six other set bits (6, 8, 12, 20, 24, 28) have no public name in
coreboot's own header and are recorded as-is, not guessed. A practical
attempt to actually read the ME region via `flashrom` hit three
separate, source-confirmed dead ends: `linux_mei` does not exist as a
flashrom programmer; the `internal` programmer is blocked by this
kernel's `CONFIG_STRICT_DEVMEM`/`CONFIG_IO_STRICT_DEVMEM`; and the
`intel-spi` kernel driver's own PCI ID table (checked directly against
mainline `drivers/spi/spi-intel-pci.c`) does not cover this board's
Sunrise Point/100-series PCH at all. See the Ukrainian sections above
for the full reasoning and the three real, untried options that remain
(`iomem=relaxed` reboot, a from-scratch driver, or an external SPI
programmer).

A same-day change added a `udev` rule + `mei` group granting
passwordless access to `/dev/mei0` (previously `root:root` 0600 for
every probe run this session) -- a deliberate, recorded widening of
local access, not an incidental default. A follow-up test of
`MKHI_GROUP_ID_BUP_COMMON` (0xf0) found the whole group silently
unanswered on this ME 11.x/Skylake chip: both
`GET_BOOT_PARTITION_INFO` and `GET_BOOT_PERF_DATA` connect and write
cleanly but the read blocks forever (confirmed via `timeout 8`,
exit 124 both times) -- CSE Lite boot-partition redundancy and boot
telemetry are almost certainly newer-generation-only features. See the
Ukrainian sections above for the exact commands and reasoning.
