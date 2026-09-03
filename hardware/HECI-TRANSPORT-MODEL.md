# Модель транспорту HECI: від `write()` у userspace до апаратних регістрів ME

**Місія 2 (2026-09-03, власник)**: розкрити чорну коробку між Linux
MEI driver і апаратним HECI-транспортом **без надсилання жодної нової
команди в ME, без зміни стану, без прямого MMIO**. Дисципліна та сама:
`OBSERVE → MAP → VERIFY → DOCUMENT`. Це продовження
`hardware/MEI-COMMUNICATION-MAP.md` (Місія 1) — той документ картографував
`userspace ↔ ME`; цей розкриває, що конкретно відбувається всередині
`Linux MEI driver ↔ HECI hardware`.

**Нічого MMIO/`/dev/mem`/кастомних модулів у цьому документі не
використано.** Усе нижче — або читання первинного джерела (вихідний
код mainline-ядра Linux, перевірено WebFetch напряму з
`torvalds/linux`, не переказ), або пасивний live-знімок через
`lspci`/`sysfs`/`/proc` (без root-обхідних шляхів, без side-effect
читань).

## Наскрізна схема, з міткою достовірності на кожній стрілці

```text
mei-observer.c: write(fd, "ff020000", 4)
      │
      ▼  SOURCE-CONFIRMED (main.c, mei_write())
mei_write()                     -- перевіряє стан з'єднання, mtu,
      │                            копіює 4 байти з userspace
      ▼  SOURCE-CONFIRMED (client.c, mei_cl_write())
mei_cl_write()                  -- будує mei_msg_hdr через mei_msg_hdr_init()
      │
      ▼  SOURCE-CONFIRMED (client.c -> hw-me.c, mei_write_message())
mei_write_message()              -- диспетчер до апаратного backend
      │
      ▼  SOURCE-CONFIRMED (hw-me.c, mei_me_hbuf_write())
mei_me_hbuf_write()
      │  цикл: mei_me_hcbww_write(dev, reg_buf[i])  -- header DWORD(и)
      │  цикл: mei_me_hcbww_write(dev, reg_buf[i])  -- payload DWORD(и)
      ▼  SOURCE-CONFIRMED
запис у регістр H_CB_WW (офсет 0x0 від BAR0)
      │
      ▼  SOURCE-CONFIRMED (mei_hcsr_set_hig())
встановлення біта H_IG у регістрі H_CSR (офсет 0x4)  -- "дані готові"
      │
      ▼  INFERRED (документована архітектура PCI MSI, не перетрасовано
      │            за іменем конкретної ISR-функції в самому ME/PCH)
апаратний HECI-контролер PCI 00:16.0 бачить H_IG, готує дані для ME
      │
      ▼  UNKNOWN (внутрішня мікроархітектура ME -- поза Linux-джерелом,
      │           за визначенням недоступна цьому дослідженню)
ME 11.8.50.3425 обробляє MKHI-запит (group=0xff, cmd=0x02)
      │
      ▼  SOURCE-CONFIRMED (mei_me_irq_quick_handler())
MSI IRQ 140 -- mei_hcsr_read() читає H_CSR, me_intr_src(hcsr) =
      hcsr & H_CSR_IS_MASK -- підтверджує, що переривання від цього пристрою
      │
      ▼  SOURCE-CONFIRMED (interrupt.c, mei_irq_thread_handler())
диспетчеризація: hdr_is_hbm(mei_hdr) ? mei_hbm_dispatch()
                                      : пошук клієнта за host/me address
      │
      ▼  SOURCE-CONFIRMED (mei_cl_irq_read_msg() -> mei_irq_compl_handler()
      │                     -> mei_cl_complete())
дані копіюються в буфер cb, клієнт (MKHI, address 32) розбуджується
      │
      ▼  SOURCE-CONFIRMED (main.c, mei_read())
wait_event_interruptible() прокидається, read() копіює 28 байт у userspace
      │
      ▼
mei-observer.c бачить RESPONSE_BYTES(28): ff8200...3425...
```

## Чотири шари протоколу — розділено явно, за вимогою місії

```text
Layer 4: MKHI command          -- "ff020000" -- payload КОНКРЕТНОГО клієнта (MKHI, address 32),
                                   НЕ HECI-команда сама по собі
Layer 3: firmware client        -- MEI client addressing (host_addr/me_addr у mei_msg_hdr),
                                    HBM connect/flow-control -- окремий підпротокол,
                                    що керує самими клієнтами, не переносить MKHI-дані
Layer 2: MEI/HECI framing       -- struct mei_msg_hdr (4 байти, нижче), DWORD-вирівняні слоти
Layer 1: PCI BAR + registers    -- H_CB_WW/H_CSR/ME_CB_RW/ME_CSR_HA, BAR0 фізична адреса
```

**Layer 4 payload наших чотирьох запитів** (`ff020000` тощо) — це
`struct mkhi_header`, яку самі проби будують; вона стає "вантажем"
усередині Layer-2-фрейму лише після того, як `mei_write_message()`
додає `mei_msg_hdr` спереду. Ніде до фізичних регістрів "ff020000" не
існує без цього обгортання — назвати сирі 4 байти "HECI-командою" було
б неточно, і місія прямо попереджала проти цього.

## Layer 2: `struct mei_msg_hdr` — точний бітовий розклад (`SOURCE-CONFIRMED`, `drivers/misc/mei/hw.h`)

```c
struct mei_msg_hdr {
    u32 me_addr:8;        // адреса клієнта на боці ME (32 = MKHI)
    u32 host_addr:8;      // адреса клієнта на боці host (призначається ядром при connect)
    u32 length:9;         // довжина Layer-4-payload у цьому пакеті
    u32 reserved:3;
    u32 extended:1;       // чи є розширений заголовок
    u32 dma_ring:1;       // чи повідомлення йде через DMA-кільце (не наш шлях)
    u32 internal:1;       // внутрішнє повідомлення MEI-підсистеми
    u32 msg_complete:1;   // це останній пакет повідомлення (фрагментація)
} __packed;               // рівно 4 байти = 1 DWORD
```

Для нашого запиту `fw-version`: `me_addr=32` (MKHI), `host_addr`
призначається ядром при `connect()` (значення конкретно не
перехоплювалось нашими пробами — `INFERRED`, не `LIVE-CONFIRMED`, бо
жодна проба цього репозиторію поки не друкує сирий `mei_msg_hdr`
власного запиту, лише MKHI-payload після нього), `length=4`
(`ff020000`), `msg_complete=1` (уміщається в один пакет, фрагментація
не потрібна для такого малого payload).

## Layer 1: карта регістрів HECI (`SOURCE-CONFIRMED`, `drivers/misc/mei/hw-me-regs.h`)

**Наш точний PCI ID підтверджено в самому цьому файлі, не
екстрапольовано з іншого покоління** (та сама дисципліна, що виявила
непідтримку `intel-spi` для нашої плати раніше цієй сесії — тут,
навпаки, збіг підтверджено):

```c
#define PCI_DEVICE_ID_INTEL_MEI_SPT_H  0xA13A  /* Sunrise Point H */
```

Наш `lspci`: `00:16.0 ... [8086:a13a]` — точний, буквальний збіг, не
"найближчий сусід" (як було з `spi-intel-pci.c`).

| Регістр | Офсет від BAR0 | R/W | Призначення (з коментарів заголовка) |
|---|---|---|---|
| `H_CB_WW` | `0x0` | W (з боку host) | Host Circular Buffer Write Window — сюди пишуться DWORD-и вихідного повідомлення |
| `H_CSR` | `0x4` | R/W | Host Control/Status — статус і керування з боку host; біти нижче |
| `ME_CB_RW` | `0x8` | R (з боку host) | ME Circular Buffer Read Window — звідси читаються DWORD-и вхідної відповіді |
| `ME_CSR_HA` | `0xC` | R (з боку host) | ME Control/Status, Host Access — дзеркало статусу ME, видиме хосту |
| `H_HPG_CSR` | `0x10` | R/W | Power-gating запит/статус (не наш шлях, read-only проби цього не торкаються) |
| `H_D0I3C` | `0x800` | R/W | D0i3-стан живлення (не наш шлях) |

**Біти `H_CSR`** (`SOURCE-CONFIRMED`, той самий файл): `H_CBD`
(circular buffer depth), `H_CBWP`/`H_CBRP` (write/read pointers),
`H_RST` (reset), `H_RDY` (host ready), `H_IG` (interrupt generate —
саме цей біт встановлює `mei_hcsr_set_hig()` після запису payload),
`H_IS` (interrupt status), `H_IE` (interrupt enable).

**Біти `ME_CSR_HA`**: `ME_CBD_HRA`, `ME_CBWP_HRA`/`ME_CBRP_HRA`,
`ME_RST_HRA`, `ME_RDY_HRA` (перевіряється через
`mei_me_hw_is_ready()` перед записом), `ME_IG_HRA`/`ME_IS_HRA`/`ME_IE_HRA`.

### Live, пасивний знімок нашого точного BAR (без `/dev/mem`, без MMIO — лише `lspci`/`sysfs`/`/proc`)

```text
00:16.0 Communication controller: Intel Corporation 100/C230 Series
        Chipset Family MEI #1 (rev 31)
Region 0: Memory at ef32d000 (64-bit, non-prefetchable) [size=4K]
Interrupts: pin B disabled, MSI(X) routed to IRQ 140
Kernel driver in use: mei_me
```

```text
/proc/interrupts: 140  190  ...  IR-PCI-MSI-0000:00:16.0  0-edge  mei_me
```

Тобто фізично `H_CB_WW`/`H_CSR`/`ME_CB_RW`/`ME_CSR_HA` живуть за
адресами `0xef32d000`, `0xef32d004`, `0xef32d008`, `0xef32d00c` — це
`LIVE-CONFIRMED` розташування (з `lspci`), але **самі значення цих
регістрів не читались** (заборонено місією без окремого дозволу —
знадобився б `/dev/mem`, якого ми свідомо не використовуємо).

## HBM (Host Bus Message) — окремий підпротокол, не MKHI

`mei_hbm_cl_flow_control_req()` (`SOURCE-CONFIRMED`, `hbm.c`) будує
`struct hbm_flow_control` і надсилає його тим самим шляхом
(`mei_hbm_cl_write()` → зрештою той самий `mei_write_message()`), але
з **іншим типом повідомлення** (`MEI_FLOW_CONTROL_CMD`), не з нашим
MKHI-payload. Так само `mei_hbm_cl_connect_req()` будує
`struct hbm_client_connect_request` — саме це реально відбувається
всередині `IOCTL_MEI_CONNECT_CLIENT`, до того, як з'являється
можливість писати MKHI-дані взагалі.

**Важливо, сказано чесно**: чи має HBM зарезервовану адресу `0` окремо
від адрес реальних клієнтів (MKHI=32 тощо) — у прочитаних фрагментах
джерела це не підтверджено явним коментарем чи константою. Позначено
`INFERRED` (архітектурно очікувано для bus-management-протоколу), не
`SOURCE-CONFIRMED`.

## Fixed-address клієнти — чому Linux поводиться з ними інакше (`SOURCE-CONFIRMED`, перевірено раніше цієї сесії)

Із `drivers/misc/mei/client.c`, `mei_cl_set_connecting()`:

```c
if (me_cl->props.fixed_address) {
    if (me_cl->connect_count) {
        mei_me_cl_put(me_cl);
        return -EBUSY;
    }
}
```

Тобто fixed-address клієнт (наші `client 15`, `client 8`, `client 7`
у таблиці Місії 1) дозволяє **лише одне** з'єднання одночасно
(`connect_count` мусить бути `0`) — на відміну від динамічних клієнтів
(як MKHI), які можуть мати кілька одночасних з'єднань. Додатково, з
`drivers/misc/mei/main.c`: конект до fixed-address клієнта може бути
взагалі заборонений драйверною політикою (`hbm_f_fa_supported`/
`allow_fixed_address`) — та сама гілка коду, що дала `ENOTTY` для AMT
раніше цієї сесії, хоча в нашому випадку `FA: 1` (підтримується), тож
ця конкретна заборона не діяла для наших спроб.

`client 7` (`55213584-...`) — це `MEI_UUID_MKHIF_FIX`, зареєстрований
у `bus-fixup.c` як протокольний "quirk" для сумісності старішої
версії MKHI-фреймінгу, застосовується інлайн самим ядром при
enumeration, не окремий сервіс і не окремий модуль.

## 9 невідомих клієнтів — пасивна ідентифікація, лише `SOURCE-CONFIRMED`/`CORRELATED`/`UNKNOWN`

Перевірено: локальний клон `coreboot` (нуль збігів для всіх дев'яти),
потім пошук по первинних відкритих джерелах (не форуми/gist/SEO —
свідомо відкинуто кілька результатів, які лише показували, що ту саму
адресу бачили на ІНШИХ машинах, без пояснення призначення).

| UUID | Статус | Джерело |
|---|---|---|
| `3c4852d6-d47b-4f46-b05e-b5edc1aa440e` | **SOURCE-CONFIRMED** | Офіційний репозиторій Intel `intel/dynamic-application-loader-host-interface`, файл `VisualStudio/Universal/DAL.inf` — той самий UUID буквально прописаний як ідентифікатор пристрою для драйвера **Dynamic Application Loader (DAL/JHI)** — підсистема ME для запуску довірених "Intel Applets" (Java-based, TEE) |
| `082ee5a7-7c25-470a-9643-0c06f0466ea1` | UNKNOWN | Знайдено лише на інших машинах (systemd issue #6650) — не ідентифікація, лише підтвердження присутності |
| `5565a099-7fe2-45c1-a22b-d7e9dfea9a2e` | UNKNOWN | Те саме |
| `dba4d603-d7ed-4931-8823-17ad585705d5` | UNKNOWN | Те саме |
| `f908627d-13bf-4a04-b91f-a64e9245323d` | UNKNOWN | Те саме |
| `309dcde8-ccb1-4062-8f78-600115a34327` | UNKNOWN | Те саме (rhboot test-data, теж лише присутність) |
| `8c2f4425-77d6-4755-aca3-891fdbc66a58` | UNKNOWN | Те саме |
| `01e88543-8050-4380-9d6f-4f9cec704917` | UNKNOWN | Жодного публічного збігу взагалі |
| `42b3ce2f-bd9f-485a-96ae-26406230b1ff` | UNKNOWN | Жодного публічного збігу взагалі |

8 із 9 свідомо лишено `UNKNOWN` — жодна назва не приписана без
першоджерела, за прямою вимогою місії.

## Явні межі — чого це дослідження НЕ довело

- **Не перехоплено сирий `mei_msg_hdr` наших власних запитів.** Ми
  знаємо його структуру (`SOURCE-CONFIRMED`) і можемо вивести значення
  логічно (`INFERRED`), але жодна проба цього репозиторію поки не
  друкує реальні байти цього заголовка окремо від MKHI-payload.
- **Внутрішня обробка запиту всередині ME (`11.8.50.3425`) —
  `UNKNOWN` за визначенням.** Це заявлена межа: Linux-джерело
  закінчується на "ME_RDY_HRA підтверджено, H_IG встановлено", що
  відбувається апаратно всередині ME — поза досяжністю цього
  дослідження.
- **Значення регістрів `H_CB_WW`/`H_CSR`/`ME_CB_RW`/`ME_CSR_HA` не
  прочитані.** Знаємо їхню фізичну адресу (`LIVE-CONFIRMED` через
  `lspci`), не їхній вміст — це вимагало б `/dev/mem`, свідомо не
  використаного.
- **Це дослідження — лише `Linux MEI driver ↔ HECI hardware`, як і
  вимагала місія.** Жодного прямого MMIO-коду не написано й не
  заплановано. Питання "чи може майбутній `wsm` після власного
  `ExitBootServices()` відтворити мінімальний безпечний підмножина
  цього шляху сам" — окрема, ще не розпочата місія, свідомо не змішана
  із цим документом.

## Місія 3: DIRECT PARALLEL MMIO OBSERVATION — `SOURCE-CONFIRMED BLOCKED`

**Гіпотеза, яку перевіряли**: чи можна пасивно прочитати `H_CSR`
(`BAR0+0x4`) і `ME_CSR_HA` (`BAR0+0xC`) через `/dev/mem`, поки
`mei_me` живий і володіє пристроєм, не втручаючись у його роботу.

**Результат — зламано на етапі аудиту джерела, до будь-якої спроби
запуску:**

```text
mei_me_probe()
   ↓
pcim_iomap_regions()
   ↓
pci_request_region()
   ↓
BAR0 = IORESOURCE_BUSY
   ↓
CONFIG_IO_STRICT_DEVMEM=y   (підтверджено на цьому ядрі раніше цієї сесії)
   ↓
resource_is_exclusive()     (kernel/resource.c)
   ↓
iomem_is_exclusive()
   ↓
devmem_is_allowed() = false
```

```text
status: SOURCE-CONFIRMED BLOCKED

reason:
active mei_me owns BAR0 through pci_request_region;
with CONFIG_IO_STRICT_DEVMEM=y an IORESOURCE_BUSY iomem region is
treated as exclusive by the /dev/mem permission path;
therefore an independent /dev/mem reader is not an allowed
observation mechanism while mei_me owns the device.

LIVE test: NOT RUN
reason: would add no architectural information unless it contradicts
the source-derived prediction.
```

**Точне формулювання, навмисно, не інше**:

```text
НЕ: hardware prevents reading BAR0

А: current Linux ownership/access policy prevents an independent
   /dev/mem reader from reading BAR0 while mei_me owns the resource
```

Це не апаратне обмеження — це навмисна модель володіння ресурсом у
самому ядрі Linux. Емпіричний запуск (`sudo devmem...` → `EPERM`) не
додав би нового архітектурного знання понад те, що вже встановлено
джерелом — тому свідомо **не запущено**. Реальна нова знахідка тут —
сам механізм (`IORESOURCE_BUSY` → `CONFIG_IO_STRICT_DEVMEM` →
`exclusive`), не факт відмови.

**Свідомо не обходжено** (`iomem=relaxed`, unbind `mei_me`, власний
kernel-модуль) — кожен із цих шляхів змінює саму умову експерименту
(живий `mei_me` + незалежний пасивний спостерігач) на геть інше
питання ("що станеться, коли ми самі володіємо HECI") — окрема,
свідомо не розпочата тут фаза.

## Місія 3B: спостереження через самого законного власника (`mei_me`), без нового коду

Перш ніж думати про будь-який instrumentation-патч чи модуль,
перевірено (без написання коду), чи Linux MEI вже має вбудовані
механізми спостереження, якими можна скористатись через самого
власника ресурсу, не створюючи другого читача BAR0.

### Tracepoints — виправлення власної помилки цієї сесії

Перша перевірка шукала в неправильному місці
(`include/trace/events/mei.h`, за аналогією з іншими підсистемами) і
дала хибний негативний результат — записано тут чесно, не приховано.
Реальний файл лежить локально в самому каталозі драйвера, не в
загальному `include/trace/events/`:

```text
drivers/misc/mei/mei-trace.c   -- CREATE_TRACE_POINTS, EXPORT_TRACEPOINT_SYMBOL
drivers/misc/mei/mei-trace.h   -- самі TRACE_EVENT-визначення
```

**Три tracepoints, `SOURCE-CONFIRMED` (mainline `torvalds/linux`),
і `LIVE-CONFIRMED` активні прямо в цьому запущеному ядрі:**

```c
TRACE_EVENT(mei_reg_read,
    TP_PROTO(const struct device *dev, const char *reg, u32 offs, u32 val),
    ...
    TP_printk("[%s] read %s:[%#x] = %#x")
);
TRACE_EVENT(mei_reg_write, /* та сама сигнатура */
    TP_printk("[%s] write %s[%#x] = %#x")
);
TRACE_EVENT(mei_pci_cfg_read, ...);
```

**Точки виклику в `hw-me.c` — саме наші цільові регістри:**

```text
mei_me_mecsr_read()  -> trace_mei_reg_read(dev, "ME_CSR_HA", ME_CSR_HA, reg)
mei_hcsr_read()       -> trace_mei_reg_read(dev, "H_CSR", H_CSR, reg)
mei_hcsr_write()      -> trace_mei_reg_write(dev, "H_CSR", H_CSR, reg)
```

Тобто кожне читання `ME_CSR_HA` і кожне читання/запис `H_CSR`, які й
так реально відбуваються під час нормальної роботи `mei_me`
(наприклад, під час нашого власного `fw-version` запиту), **вже
несуть повне ім'я регістру, офсет і фактичне значення** через цей
канал — саме те, що Місія 3B шукала.

**Перевірено напряму, що це не лише теоретично існує в коді, а живе
прямо зараз у завантаженому модулі цього ядра:**

```bash
grep -i "mei_reg_read\|mei_reg_write" /proc/kallsyms
```

```text
D __tracepoint_mei_reg_read      [mei]
D __tracepoint_mei_reg_write     [mei]
D __tracepoint_mei_pci_cfg_read  [mei]
```

`LIVE-CONFIRMED`, не `SOURCE-CONFIRMED` лише за кодом — символи
реально присутні в `/proc/kallsyms` цього самого, зараз запущеного
`mei`/`mei_me`. **Жодне інше ядро не потрібне.**

`/sys/kernel/debug/tracing/events/mei/` (стандартна ftrace-точка
входу для увімкнення) поки недоступна без root — той самий,
уже знайомий цій сесії патерн прав доступу до `debugfs`, не ознака
відсутності.

### dynamic_debug (`cl_dbg`/`dev_dbg`)

Джерело (`main.c`, `client.c` — уже цитоване раніше в цьому документі)
рясно використовує `cl_dbg(dev, cl, ...)` — це macro-обгортка над
`dev_dbg()`, яка при увімкненому `CONFIG_DYNAMIC_DEBUG` стає
керованою через `/sys/kernel/debug/dynamic_debug/control` **без
перекомпіляції й без нового коду**. Перевірено напряму на цьому ядрі
(`/boot/config-7.1.5+kali-amd64`): `CONFIG_DYNAMIC_DEBUG=y`,
`CONFIG_DYNAMIC_DEBUG_CORE=y` — обидва увімкнені, механізм реально
доступний, не лише теоретично існує в коді. Приклади вже процитованих
викликів у цьому самому документі: `"Cannot connect to FW Client
UUID..."`, `"sending flow control"`, `"is not connected"` — жоден із
них не друкує сирі значення `H_CSR`/`ME_CSR_HA` безпосередньо, але
рівень деталізації самого протоколу (connect/flow-control/disconnect
переходи) став би видимим у `dmesg` без жодного нового спостерігача.

### debugfs (`meclients`/`active`/`devstate`/`allow_fixed_address`)

Уже використано раніше цієї сесії (`hardware/MEI-COMMUNICATION-MAP.md`).
Жоден із чотирьох файлів `debugfs.c` (перевірено раніше, `SOURCE-CONFIRMED`)
не дає сирого дампу `H_CSR`/`ME_CSR_HA` — `devstate` показує
декодований HBM-стан (`hbm features`, `pg`, `pxp`), не регістри
transport-рівня.

### Підсумок Місії 3B (перша частина — розвідка без коду)

| Механізм | Чи існує | Чи дає H_CSR/ME_CSR_HA |
|---|---|---|
| ftrace tracepoints (`drivers/misc/mei/mei-trace.h`) | **є, `LIVE-CONFIRMED` активні в поточному ядрі** | **так — ім'я регістру, офсет, фактичне значення** |
| `dynamic_debug` (`cl_dbg`/`dev_dbg`) | **є**, готовий до вмикання | лише протокольні переходи, не сирі регістри |
| `debugfs` (`meclients`/`active`/`devstate`) | **є**, уже використано | декодований HBM-стан, не сирі регістри |

**Найкращий інструмент уже знайдено, готовий, ніякого нового ядра чи
коду не потрібно.** Питання інструментального патча (яке лишалось
відкритим у першій версії цього розділу) закрите саме тим, що
`mei-trace.h` вже й так друкує `H_CSR`/`ME_CSR_HA` разом зі значенням
у власних, давно наявних точках виклику драйвера (`mei_hcsr_read()`,
`mei_hcsr_write()`, `mei_me_mecsr_read()`). Наступний практичний крок
— увімкнути ці events через ftrace (потребує root) під час звичайного
запуску `mei-observer`, щоб зіставити задокументований у Місії 2
ланцюжок викликів із фактичними значеннями регістрів наживо.

## Критерій завершення місії

Шлях `fw-version` розкладено настільки далеко, наскільки дозволяють
первинні джерела: `write()` → `mei_write()` → `mei_cl_write()` →
`mei_write_message()` → `mei_me_hbuf_write()` → запис `H_CB_WW` →
встановлення `H_IG` в `H_CSR` → (UNKNOWN: внутрішня обробка ME) →
MSI IRQ 140 → `mei_me_irq_quick_handler()` → `mei_irq_thread_handler()`
→ `mei_cl_irq_read_msg()` → `mei_cl_complete()` → `read()` повертає
28 байт із `11.8.50.3425`. Кожна ланка процитована з конкретного файлу
й функції первинного джерела, кожна невідома ланка позначена чесно.
Місія зупиняється тут — жодного прямого MMIO, жодної нової команди в
ME.
