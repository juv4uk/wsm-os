# MINIX-0: як userspace-драйвер MINIX 3 знаходить PCI-пристрій, читає config space і мапить BAR

**Місія MINIX-0 (2026-09-03).** Суто розвідка в локальному клоні
MINIX 3 (`/home/user/GitHub/minix`) — **нічого не портовано з Linux,
жодного HECI-transport не написано, жодного MMIO read/write не
виконано, жодного нового драйвера не створено.** Мета — відповісти на
сім конкретних питань про архітектуру PCI-доступу в MINIX, кожне з
конкретним файлом, функцією і реальним прикладом із самого дерева
MINIX, не з переказу чи здогадки.

**Ціль, яку шукатимемо в MINIX (не чіпаючи її тут)**:
`vendor=0x8086, device=0xa13a` — той самий Intel MEI SPT-H (Sunrise
Point H), що й у `hardware/HECI-TRANSPORT-MODEL.md` для Linux.

## Архітектурна відмінність від Linux, одразу варта згадки

У Linux PCI-доступ — частина ядра, викликається напряму з
kernel-простору. У MINIX (мікроядро, драйвери — окремі userspace-
процеси) PCI-доступ **сам є окремим сервісом**:

```text
driver-процес (наприклад, майбутній HECI-драйвер)
      │  виклики бібліотечних функцій (syslib)
      ▼
PCI bus driver -- окремий процес, minix/drivers/bus/pci/
      │  IPC-повідомлення
      ▼
реальне читання PCI config space (port I/O 0xCF8/0xCFC або MMCONFIG)
```

Є ще старіша, окрема `lib/libpci/` (заголовок `pci.h`, файли
`pci_bus.c`/`pci_device.c`) — це портований із NetBSD **POSIX-рівень**
через `/dev/pci`-файлові дескриптори, задокументований у `pci.3`
(man-сторінка). Це не те, чим користуються реальні драйвери апаратних
пристроїв — це окремий, файл-дескрипторний шлях для userland-
інструментів на кшталт `pcictl`. Реальний, нативний API драйверів —
інший, описаний нижче.

---

## 1. Як driver отримує доступ до PCI subsystem?

**Файл**: `minix/include/minix/syslib.h`
**Функція**: `void pci_init(void);`

Кожен реальний driver-процес, що потребує PCI, лінкується проти
`libsys` (стандартна системна бібліотека MINIX-драйверів) і починає з
виклику `pci_init()` — це встановлює IPC-з'єднання з PCI bus driver
процесом. Без цього виклику жодна інша `pci_*`-функція не працює.

**Реальний приклад** (`minix/drivers/storage/ahci/ahci.c:2022`):
```c
static int ahci_probe(int skip)
{
	int r, devind;
	u16_t vid, did;

	pci_init();
	...
```

## 2. Як enumerate/find device за vendor/device ID?

**Файл**: `minix/include/minix/syslib.h`
**Функції**:
```c
int pci_first_dev(int *devindp, u16_t *vidp, u16_t *didp);
int pci_next_dev(int *devindp, u16_t *vidp, u16_t *didp);
```

Обидві повертають `devind` (внутрішній індекс пристрою в таблиці PCI
bus driver'а, не bus/dev/func) плюс `vid`/`did` (vendor/device ID) для
поточного знайденого пристрою. Реальний driver порівнює `vid`/`did` із
власними очікуваними значеннями сам, у циклі — MINIX не має вбудованої
"знайди мені 8086:a13a" функції за одним викликом.

**Реальний приклад** (`ahci.c:2022-2036`, спрощено до структури циклу):
```c
r = pci_first_dev(&devind, &vid, &did);
while (skip--)
    r = pci_next_dev(&devind, &vid, &did);
```

Для нашої цілі це виглядало б так (не написано, лише формула):
```c
r = pci_first_dev(&devind, &vid, &did);
while (r > 0) {
    if (vid == 0x8086 && did == 0xa13a) break;   /* Intel MEI SPT-H */
    r = pci_next_dev(&devind, &vid, &did);
}
```

## 3. Як читається PCI config space?

**Файл**: `minix/include/minix/syslib.h`
**Функції**:
```c
u8_t  pci_attr_r8 (int devind, int port);
u16_t pci_attr_r16(int devind, int port);
u32_t pci_attr_r32(int devind, int port);
```
(і симетричні `pci_attr_w8/16/32` для запису — не потрібні для
пасивного дослідження).

`port` тут — офсет усередині 256-байтного (чи 4КіБ у extended
config space) простору конфігурації того самого пристрою, знайденого
через `devind`. Приклад реального використання —
зчитування Interrupt Line Register (`PCI_ILR`) для отримання номера
IRQ (`ahci.c:2093`, деталі в п.7 нижче).

## 4. Як отримується BAR0?

**Файл**: `minix/include/minix/syslib.h`
**Функція**:
```c
int pci_get_bar(int devind, int port, u32_t *base, u32_t *size, int *ioflag);
```

`port` тут — офсет конкретного BAR у конфігураційному просторі
(наприклад, `PCI_BAR` для BAR0, `PCI_BAR_2` для BAR1 і т.д. —
константи з `minix/drivers/bus/pci/pci.h`). Повертає фізичну адресу
(`base`), розмір регіону (`size`) і прапорець типу (`ioflag` — I/O
port простір проти memory-mapped, наш HECI BAR0 — завжди memory,
`ioflag=0`).

**Реальний приклад** (`ahci.c:2070`):
```c
if ((r = pci_get_bar(devind, PCI_BAR_6, &base, &size, &ioflag)) != OK)
    panic("unable to retrieve BAR: %d", r);
```
(AHCI використовує BAR5/`PCI_BAR_6`-індексацію через ABAR; для нашого
HECI-пристрою це був би `PCI_BAR` — перший BAR, той самий, що ми вже
підтвердили в Linux: `0xef32d000`, 4КіБ.)

## 5. Який API дає mapping MMIO?

**Файл**: `minix/include/minix/vm.h`
**Функція**:
```c
void *vm_map_phys(endpoint_t who, void *physaddr, size_t len);
```

Це виклик до VM-сервера (окремий системний процес пам'яті MINIX) —
просить відобразити фізичний діапазон (`physaddr`, `len` — саме те,
що повернув `pci_get_bar()`) у віртуальний адресний простір
викликаючого процесу (`who`, зазвичай `SELF`). Результат — звичайний
`void*`, з яким далі можна працювати як зі звичайною пам'яттю
(`*(volatile u32_t*)(base + H_CSR)` — але це вже MMIO read/write, поза
межами цієї місії).

**Реальний приклад** (`ahci.c:2087`):
```c
hba_state.base = (u32_t *) vm_map_phys(SELF, (void *) base, size);
if (hba_state.base == MAP_FAILED)
    panic("unable to map HBA memory");
```

## 6. Який механізм ownership/reservation пристрою?

Тут — **два окремі шари**, не один:

**Шар 1 — системна авторизація (RS ACL), перевіряється до того, як
драйвер взагалі може щось зробити.** Файл `minix/include/minix/rs.h`:
```c
struct rs_pci_id {
    u16_t vid;
    u16_t did;
    u16_t sub_vid;
    u16_t sub_did;
};

struct rs_pci {
    char rsp_label[RS_MAX_LABEL_LEN];
    int  rsp_endpoint;
    int  rsp_nr_device;
    struct rs_pci_id rsp_device[RS_NR_PCI_DEVICE];
    ...
};
```
Кожен системний сервіс (драйвер-процес), який Reincarnation Server
(RS) запускає, має декларований список `vid`/`did`-пар, які йому
взагалі дозволено чіпати — це перевіряється на рівні системи, не
самим драйвером. `pci_set_acl(struct rs_pci *rs_pci)` (`syslib.h`)
реєструє цей список.

**Шар 2 — runtime-резервація конкретного знайденого пристрою**:
```c
void pci_reserve(int devind);
int  pci_reserve_ok(int devind);
```
Викликається вже ПІСЛЯ того, як `pci_first_dev`/`pci_next_dev`
знайшли потрібний `devind` — аналог `pci_request_region()` з Linux
(`hardware/HECI-TRANSPORT-MODEL.md`), лише через IPC до PCI bus
driver'а, а не напряму в kernel-структурі ресурсів.

**Реальний приклад** (`ahci.c:2034`):
```c
pci_reserve(devind);
```

**Чесна межа**: конкретний файл, де для реального драйвера (наприклад,
`ahci`) заповнюється `rsp_device[]` під час запуску через RS, не
знайдено швидким пошуком у цьому проході (ймовірно, генерується з
system-конфігурації окремо) — залишено як відкрите питання, не
вигадано відповіді.

## 7. Як драйвер отримує IRQ?

**Файл**: `minix/include/minix/syslib.h`
**Функції** (макроси над `sys_irqctl()`):
```c
#define sys_irqsetpolicy(irq_vec, policy, hook_id) \
    sys_irqctl(IRQ_SETPOLICY, irq_vec, policy, hook_id)
#define sys_irqenable(hook_id) \
    sys_irqctl(IRQ_ENABLE, 0, 0, hook_id)
```

Номер IRQ спочатку читається з самого PCI config space через
`pci_attr_r8(devind, PCI_ILR)` (Interrupt Line Register) — той самий
номер, що Linux бачить через `lspci` (`Interrupts: ... MSI(X) routed
to IRQ 140` для нашого HECI). Потім реєструється політика
(`sys_irqsetpolicy`) і вмикається (`sys_irqenable`).

**Реальний приклад** (`ahci.c:2093-2100`):
```c
hba_state.irq = pci_attr_r8(devind, PCI_ILR);
hba_state.hook_id = 0;

if ((r = sys_irqsetpolicy(hba_state.irq, 0, &hba_state.hook_id)) != OK)
    panic("unable to register IRQ: %d", r);
if ((r = sys_irqenable(&hba_state.hook_id)) != OK)
    panic("unable to enable IRQ: %d", r);
```

---

## Підсумок — критерій завершення Місії MINIX-0

```text
MINIX:
"ось API, яким я можу знайти 8086:a13a"
  -> pci_init() + pci_first_dev()/pci_next_dev() цикл, порівнюючи vid/did

"ось API, яким я отримаю BAR0"
  -> pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag)

"ось API, яким я пізніше зможу мапити його"
  -> vm_map_phys(SELF, (void*)base, size)
```

Кожен пункт — конкретний файл, конкретна функція, реальний робочий
приклад (`ahci.c`), не переказ і не здогадка. Жодного MMIO read/write,
жодного HECI/MEI-пакета, жодного нового драйвера в цьому документі
немає. Зупинено тут.
