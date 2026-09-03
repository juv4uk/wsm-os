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

## Що лишається невідкритим

Вміст самих 2 093 056 байтів ME-регіону цього образу (`STATIC-CONFIRMED`
за межами/офсетом у `FIT-AND-STRUCTURE-ANALYSIS.md`, `not-yet-verified`
всередині) — розбір власне прошивки ME цієї плати не входив у цей
прохід і лишається окремою, явно не розпочатою роботою.

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
