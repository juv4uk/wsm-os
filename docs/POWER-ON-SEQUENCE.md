# Що насправді стається при вмиканні живлення

Повний шлях від натискання кнопки живлення до точки, яку цей
репозиторій вже реально довів досяжною (`probe/exit-boundary-probe.c`,
`ExitBootServices()`, сирий serial-контроль). Кожна фаза нижче цитує,
на чому вона ґрунтується: `STATIC-CONFIRMED` (прочитано напряму з
власного образу флеш F22e цієї плати, див. `hardware/bios-f22e/`),
`LIVE-CONFIRMED` (безпосередньо спостережено у власних запусках
QEMU/OVMF цього репозиторію — реальний вивід BDS, не опис про нього),
або `general` (добре задокументована публічна архітектура платформ
Intel/x86, не перевірена незалежно проти стану запобіжників кремнію
саме цієї плати).

## Фаза 0 — фізична / аналогова (general, не специфічна для плати)

Чергова шина 5VSB блоку живлення ATX жива, доки машина увімкнена в
розетку, незалежно від кнопки живлення. Натискання кнопки живлення
тягне `PWR_BTN#` вниз; вбудований контролер материнської плати /
логіка секвенування живлення PCH піднімає `PS_ON#`, щоб підняти
основні шини блоку живлення, потім секвенує власні внутрішні площини
живлення (`RSMRST#`, автомат станів `SLP_S3#`/`SLP_S4#`/`SLP_S5#`).
Тільки коли живлення самого PCH стабільне, він звільняє `PLTRST#`
(скидання платформи), що й реально звільняє CPU зі стану скидання.
Нічого з цього не перевірено проти власної схеми H170-Gaming 3 — це
стандартний дизайн платформи ATX/Intel, процитований як `general`, не
`STATIC-CONFIRMED`.

## Фаза 1 — Intel Management Engine завантажується першим (general)

Перш ніж головний CPU взагалі почне щось виконувати, Intel Management
Engine — окреме x86-ядро, вбудоване в PCH, що виконує власну незалежну
ОС — завантажується самостійно. Послідовність скидання головного CPU
не продовжується, доки ME не подасть сигнал готовності. Власний аналіз
цього репозиторію не розбирав вміст регіону ME (`FIT-AND-STRUCTURE-ANALYSIS.md`
явно позначив це як поза межами обсягу — 2 093 056 байтів регіону ME
`STATIC-CONFIRMED` присутні за розміром/офсетом, але не декодовані
всередині). Те, що ME існує й контролює старт головного CPU — це
`general`, добре задокументована архітектура платформи, а не щось, що
цей репозиторій незалежно підтвердив для саме цієї прошивки.

## Фаза 2 — скидання CPU, FIT і реальне завантаження мікрокоду — STATIC-CONFIRMED який саме мікрокод

Вектор скидання головного CPU архітектурно живе за адресою
`0xFFFFFFF0` ще з 8086 — але на цьому поколінні платформи це вже не
*перше*, що виконується. При скиданні власний мікрокод CPU читає
покажчик за фіксованою фізичною адресою `0xFFFFFFC0` (`general`,
власна опублікована специфікація FIT від Intel) — **цей репозиторій
уже розпарсив саме цей покажчик на реальному образі F22e і знайшов там
чинну таблицю `_FIT_`** (`FIT-AND-STRUCTURE-ANALYSIS.md`,
`STATIC-CONFIRMED`). Записи FIT типу 1 (Microcode Update) обробляються
*до того*, як взагалі виконається легасі-вектор скидання — FIT цієї
плати має три записи, один із яких збігається з точною сигнатурою CPU
власника (`0x000506E3`): **ревізія `0xC2`, дата 2017-11-16**
(`STATIC-CONFIRMED`, перехресно підтверджено другим, незалежним
інструментом, що знайшов той самий набір blob-ів усередині контейнера
з назвою `CPU_MICROCODE_FILE_GUID`). Це не гіпотетичний крок — це
конкретний, ідентифікований блоб мікрокоду, який прошивка саме цієї
плати завантажила б у саме цей CPU на саме цій фазі, при кожному
реальному холодному завантаженні.

## Фаза 3 — Startup ACM / Boot Guard, якщо запобіжники прошиті (загальна архітектура; примусове застосування не перевірено)

Якщо цього вимагають одноразові запобіжники платформи, мікрокод CPU
також читає запис FIT типу 2 (Startup ACM), копіює Authenticated Code
Module у кеш CPU (cache-as-RAM, оскільки реальна DRAM ще не
ініціалізована — копіювання в кеш спочатку конкретно запобігає атаці
підміни флеша між перевіркою й виконанням), і перевіряє його підпис
проти жорстко вшитого ключа Intel. Потім ACM виконується в 32-бітному
захищеному режимі, читає хеш публічного ключа OEM із запобіжників, і
перевіряє підпис Initial Boot Block, перш ніж мікрокод перемикається
*назад* у 16-бітний реальний режим для легасі-сумісності й передає
керування традиційному вектору скидання.

**Цей репозиторій уже знайшов конкретний запис FIT, який це б
використало**: тип 2 "BIOS Startup Module (ACM)" за flash-мапованою
адресою `0xFFFF0000` (`FIT-AND-STRUCTURE-ANALYSIS.md`), що збігається
з FFS-файлом на ім'я `PEI_BIOS_ACM_FILE_GUID` (184 088 байтів),
знайденим незалежно другим інструментом. **Що чесно `not-yet-verified`:
чи Boot Guard реально прошитий/примусово застосовується на цій
конкретній платі власника взагалі.** Його присутність в образі
прошивки доводить лише те, що механізм існує у флеш-пам'яті — більшість
масових споживчих плат (цей чипсет H170 — масовий споживчий продукт,
не керований/enterprise SKU) постачаються із запобіжниками Boot Guard,
не прошитими OEM-виробником, і в такому разі ця фаза присутня в образі,
але ніколи реально не застосовується CPU. Це реальний факт про стан
запобіжників CPU/кремнію, не щось, що читається лише з образу
прошивки, і в цьому проході не перевірялось.

## Фаза 4 — фаза SEC (Security) (general)

Найраніша фаза за специфікацією UEFI PI. Виконується без доступної
реальної DRAM — тимчасове сховище живе у власному кеші CPU,
налаштованому як cache-as-RAM (CAR). Мінімальний код: знаходить і
перевіряє PEI Foundation, налаштовує тимчасовий стек, передає
керування.

## Фаза 5 — PEI (Pre-EFI Initialization) — STATIC-CONFIRMED присутність модулів

Тут виконуються PEI-модулі (PEIM), найважливіше — Memory Reference
Code, який реально тренує й ініціалізує реальні DDR4 DIMM-и (реальну
пару власника `CT8G4DFS8213.C8FBD1` + `TEAMGROUP-UD4-2133`, за
`OWNER-HARDWARE-PROFILE.md`) — це перша точка, де реальна системна RAM
взагалі стає використовною. Власна інвентаризація FFS цього
репозиторію (`FIT-AND-STRUCTURE-ANALYSIS.md`) уже знайшла 164
FFS-файли типу `0x02` (PEIM) серед 780 файлів цього образу, плюс
конкретно названий файл `PEI_AP_STARTUP_FILE_GUID` (запуск
Application-Processor — механізм, що зрештою вводить в дію інші 3
ядра CPU, прямо стосується майбутнього поля стану handoff "cores
online"). PEI завершується побудовою списку HOB (Hand-Off Block), що
описує знайдене, і передачею керування DXE.

## Фаза 6 — DXE (Driver Execution Environment) — STATIC-CONFIRMED присутність таблиць

Фаза, що робить більшість того, що люди мають на увазі під "BIOS":
перерахування PCI, диспетчеризація драйверів чипсету/пристроїв,
побудова таблиць ACPI й SMBIOS, які пізніше прочитає ОС. Цей
репозиторій уже підтвердив (`FIT-AND-STRUCTURE-ANALYSIS.md`), що
розпакований головний том прошивки містить реальні сигнатури
`RSD PTR `, `FACP`, `APIC`, `MCFG`, `DMAR`, `HPET`, `DSDT` (×19),
`SSDT` (×64) та точки входу SMBIOS — а окремо власний реальний запуск
`probe/handoff-probe.c` (`probe/README.md`) напряму прочитав живий
покажчик ACPI RSDP і покажчик SMBIOS з `SystemTable->ConfigurationTable`,
підтверджуючи, що вивід цієї фази реально досяжний під час виконання,
не лише присутній як шаблони у флеш-пам'яті. Сховища NVRAM-змінних
(`NvramPei`/`NvramDxe`/`NvramSmm`) теж будуються/консультуються тут.

## Фаза 7 — BDS (Boot Device Selection) — LIVE-CONFIRMED, безпосередньо спостережено

Це не висновок із документації — **буквально кожен запуск проби в
налаштуванні QEMU/OVMF цього репозиторію, без винятку, друкував цю
фазу за назвою**:

```text
BdsDxe: loading Boot0001 "UEFI QEMU HARDDISK QM00001 " from PciRoot(0x0)/Pci(0x1F,0x2)/Sata(0x0,0xFFFF,0x0)
BdsDxe: starting Boot0001 "UEFI QEMU HARDDISK QM00001 " from PciRoot(0x0)/Pci(0x1F,0x2)/Sata(0x0,0xFFFF,0x0)
```

BDS обходить змінні опцій завантаження (або, за їх відсутності,
падає назад на `\EFI\BOOT\BOOTX64.EFI` на підключеному знімному/
фіксованому пристрої, саме тим шляхом, яким завантажується кожна проба
цього репозиторію) і передає керування обраному UEFI-застосунку — у
випадку цього репозиторію, нашим власним пробам; на реальній машині
власника зазвичай власному завантажувачу ОС.

## Фаза 8 — ера Boot Services, потім межа, яку цей репозиторій уже перетнув

Звідси завантажений UEFI-застосунок (завантажувач ОС, або власні проби
цього репозиторію) може вільно викликати Boot Services — консольний
ввід/вивід, виділення пам'яті, пошук протоколів — саме те, що
`handoff-probe.c` використав, щоб прочитати CR0/CR3/CR4/EFER/CPUID/
живий-мікрокод/карту-пам'яті/ACPI/SMBIOS/framebuffer (`probe/README.md`,
`LIVE-CONFIRMED` для віртуального середовища QEMU/OVMF). Останній крок
— той, що цей репозиторій уже **довів, а не лише описав** —
`ExitBootServices()`: `exit-boundary-probe.c` реально його викликає, і
з Boot Services (включно з консоллю), тепер справді недійсними,
доводить продовження життєздатності через сирий канал serial port-I/O
без жодного UEFI API:

```text
BEFORE_EXIT              <- UEFI ConOut, Boot Services ще чинні
ExitBootServices()
AFTER_EXIT                 <- сирий port-I/O на UART 16550 за 0x3F8, взагалі без UEFI
RAW_CONTROL_REACHED
```

Усе від Фази 0 до цього моменту — це те, що прошивка робить *для*
майбутньої точки входу WSM, а не щось, що сама WSM мусить
переімплементувати, повністю розуміти чи довіряти понад те, що вона
може незалежно перевірити — за наявним стоячим розрізненням, уже
записаним у `wsm/research/handoff-state.md`.

## Джерела

- [Intel Firmware Interface Table BIOS Specification, doc 599500](https://www.intel.com/content/dam/develop/external/us/en/documents/firmware-interface-table-bios-specification-r1p2p1.pdf)
- [coreboot: Intel Firmware Interface Table](https://doc.coreboot.org/soc/intel/fit.html)
- [Intel EDC: Startup ACM (Type 2) Rules](https://edc.intel.com/content/www/us/en/design/products-and-solutions/software-and-services/firmware-and-bios/firmware-interface-table/1.4/startup-acm-type-2-rules/)
- [mjg59: Booting modern Intel CPUs](https://mjg59.dreamwidth.org/66109.html)
- [Trammell Hudson: Bootguard](https://trmm.net/Bootguard/)
- [Wikipedia: Reset vector](https://en.wikipedia.org/wiki/Reset_vector)
- Власні: `hardware/bios-f22e/BINARY-ANALYSIS.md`, `hardware/bios-f22e/FIT-AND-STRUCTURE-ANALYSIS.md`, `docs/OWNER-HARDWARE-PROFILE.md`, `probe/README.md`

---

## What actually happens when the power turns on (English, secondary)

Full phase-by-phase power-on sequence, from the physical power button
through ME boot, CPU reset + FIT microcode loading (this board's own
real microcode, revision `0xC2`), optional Boot Guard, SEC, PEI (real
DRAM init), DXE (ACPI/SMBIOS), BDS (directly observed in every probe
run), Boot Services, and the `ExitBootServices()` boundary this repo
has already proven crossable (`RAW_CONTROL_REACHED`). See the Ukrainian
version above for full detail and sources.
