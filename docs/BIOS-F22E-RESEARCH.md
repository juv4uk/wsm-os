# BIOS F22e — зовнішнє дослідження

Зовнішнє web-підтвердження версії BIOS, вже підтвердженої наживо на
платі власника в `docs/OWNER-HARDWARE-PROFILE.md`
(`SMBIOSBIOSVersion F22e`, American Megatrends Inc., живий запит двічі:
2026-08-29 і перепідтверджено 2026-09-02). Цей документ додає зовнішні
джерела; він не замінює живий системний запит, який лишається
найавторитетнішим окремим фактом за власним порядком пріоритету
доказів екосистеми (прийняті докази > стан репозиторію/живий стан >
зовнішня документація).

**Метод:** `WebSearch` + `WebFetch`, 2026-09-02. Два прямі запити до
`gigabyte.com` і `drivers.softpedia.com` повернули HTTP 403
(заблоковано, не отримано — їхній вміст тут не відтворюється, лише те,
що незалежно показали фрагменти пошуку). Спроба через Wayback Machine
не підтримується наявним інструментом. Жоден вміст із заблокованої
сторінки нижче не стверджується.

## Ідентичність плати / BIOS

- Плата: Gigabyte **GA-H170-Gaming 3**, чипсет H170, сокети для 6-го/7-го
  покоління Intel Core, двоканальний DDR4 (4 DIMM), подвійний PCIe Gen3
  x4 M.2 — збігається з живо підтвердженою платою власника
  (`docs/OWNER-HARDWARE-PROFILE.md`: Gigabyte H170-Gaming 3, i5-6400 =
  6-те покоління). `source-confirmed` (фрагмент пошуку Softpedia).
- F22e існує як реліз для **обох ревізій плати**, rev. 1.0 і rev. 1.1.
  Власний живий запит власника повернув `Version: x.x` для
  `Win32_BaseBoard`, що не розрізняє ревізію — **яка саме ревізія
  фізичної плати власника, лишається `not-yet-verified`** з цього
  дослідження; не було повторно запитано в цьому проході.
  `source-confirmed`, що обидва варіанти існують; `unknown`, який саме
  має власник.

## Дата релізу — ВИРІШЕНО 2026-09-02 реальним файлом прошивки

**Оновлення:** власник завантажив реальний архів BIOS F22e напряму з
Gigabyte, і тепер він заархівований у
`hardware/bios-f22e/mb_bios_ga-h170-gaming3_f22e.zip` (SHA-256 у README
тієї директорії). Власна внутрішня позначка часу ROM-файлу —
`H170G3.22e`, дата `2018-03-09 20:47` всередині zip — точно збігається
з `ReleaseDate`, отриманим із живого запиту власника. Це тепер
`source-confirmed` із самого файлу прошивки, не висновок. Розбіжність
нижче вирішена: "01 Apr 2021" від `driverscollection.com` підтверджено
як дата власного індексування того дзеркального сайту, не реальна дата
релізу прошивки.

<details>
<summary>Оригінальний запис розбіжності (лишено для запису, не видалено)</summary>

- Власна машина власника показує `ReleaseDate: 2018-03-09` для саме
  цього BIOS (`Win32_BIOS`, запитано наживо двічі). `empirically
  confirmed`, локальний запуск, WINDOWS-OBSERVED — найсильніший доказ,
  доступний тут.
- Одне резюме WebSearch (агреговане, не єдине першоджерело) незалежно
  стверджувало, що F22e/F22b "випущено 2018-04-03" — у межах чотирьох
  тижнів від дати власної системи власника, достатньо узгоджено, щоб
  бути тим самим циклом релізу. `predicted` (резюме агрегатора, не
  прочитана напряму першоджерельна сторінка).
- Власний лістинг `driverscollection.com` стверджує **"01 Apr 2021"**
  для того самого файлу F22e. Це конфліктує з обома пунктами вище
  приблизно на три роки. Значно ймовірніше, що це дата, коли те
  дзеркало проіндексувало/передзавантажило файл, а не реальна дата
  релізу прошивки — типовий патерн для сторонніх сайтів-дзеркал
  драйверів — але це `predicted`, не підтверджено; жодну першоджерельну
  сторінку Gigabyte не вдалося успішно прочитати, щоб це вирішити. **Не
  вважати дату `driverscollection.com` авторитетною; авторитетна —
  власна жива системна дата власника.**

</details>

## Changelog

- Два незалежні лістинги дзеркальних сайтів (фрагмент пошуку Softpedia,
  прямий запит `driverscollection.com`) дають однаковий однорядковий
  changelog: **"Update CPU Microcode."** `source-confirmed` із двох
  незалежних дзеркал, але жодне з них не є офіційною сторінкою
  changelog самого Gigabyte — обидва прямі запити до `gigabyte.com` і
  до самої сторінки Softpedia повернули HTTP 403. Жодного офіційного
  першоджерельного тексту changelog не прочитано.

## Правдоподібно, але НЕ підтверджено: зв'язок зі Spectre/Meltdown

Живо підтверджений час релізу F22e (2018-03-09) потрапляє в
загальногалузеве пікове вікно (січень–квітень 2018) для оновлень
мікрокоду Intel, що адресують Spectre variant 2 (CVE-2017-5715). У
поєднанні з текстом changelog "Update CPU Microcode" це
**правдоподібне**, але **не source-confirmed** пояснення того, що
насправді змінив F22e. Перевірено напряму: трекер, що підтримується
спільнотою,
[`meltdown-spectre-bios-list`](https://github.com/mathse/meltdown-spectre-bios-list)
взагалі не має секції Gigabyte, тож він ні підтверджує, ні спростовує
це для саме цієї плати. **`predicted`, не `source-confirmed`** —
викладено тут як гіпотеза, варта позначення, не факт, на якому можна
будувати.

## Знайдені реальні звіти з форумів

### Tom's Hardware — НЕ дефект F22e (корінна причина — погана RAM)

Користувач повідомив про оновлення саме цієї плати з BIOS F5 до F22e
одночасно зі встановленням нового NVMe SSD, після чого зіткнувся з
циклом перезавантаження (червоний Ambient LED, вентилятори крутяться,
повторний цикл живлення, нема екрана завантаження). Це легко можна
*неправильно прочитати* як "F22e ламає завантаження NVMe" — це не так.
Власне вирішення теми: після ретельного усунення несправностей (скидання
CMOS, заміна батарейки, заміна БЖ, Memtest86, обмін компонентами між
машинами) користувач ізолював причину до **конкретної пари DIMM
пам'яті** — їх видалення виправило машину, з іншою парою RAM, що
працює нормально. Оновлення BIOS і встановлення NVMe збіглися за часом
випадково, це не була причина. `source-confirmed` (тему прочитано
напряму), що це була несправність RAM, не дефект прошивки.
[Тема Tom's Hardware](https://forums.tomshardware.com/threads/gigabyte-ga-h170-gaming-3-is-not-working.3850371/)

### AnandTech — інший BIOS (ери F5), особливість CSM/legacy, не F22e

Інший користувач із цього сімейства плат повідомив, що після оновлення
до ранньої ревізії BIOS (за їхньою власною нумерацією: "Revision 5",
не F22e) і встановлення Samsung 950 Pro NVMe SSD жоден із його дисків
(включно з трьома legacy SATA-дисками) не виявлявся. Вирішення:
перемикання CSM на "Legacy only" відновило виявлення старіших дисків;
NVMe-диск працював, попри те, що не з'являвся як перелічений
PCIe-пристрій у BIOS. Цей звіт передує F22e багатьма ревізіями BIOS і
не стосується самого F22e — включено лише як контекст про відому ранню
чутливість цього сімейства плат до NVMe/CSM, не як доказ про реально
встановлену версію власника. `source-confirmed` (тему прочитано
напряму), але `not-applicable` до саме F22e.
[Тема AnandTech](https://forums.anandtech.com/threads/gigabyte-h170-gamer-3-and-nvme-issues.2481416/)

## Стосунок до wsm-os

Жодного доказу з форумів, що пов'язує сам F22e з будь-яким дефектом,
не знайдено. Жоден зі звітів вище насправді не про те, що F22e
спричиняє проблему (один — випадкова несправність RAM, інший —
геть інший, набагато старіший BIOS). Живо підтверджений NVMe-диск
власника (Kingston `SNV2S1000G`, `Status: OK`) не показав жодного
симптому, що збігається з будь-яким зі звітів. Це не змінює жодного
наявного архітектурного рішення в `OWNER-HARDWARE-PROFILE.md` — робота
з фізичним завантаженням на рівні BIOS лишається відкладеною незалежно
від цього (рішення №2, QEMU спочатку).

## Джерела

- [Softpedia: GA-H170-Gaming 3 (rev. 1.1) BIOS F22e](https://drivers.softpedia.com/get/BIOS/Gigabyte/Gigabyte-GA-H170-Gaming-3-rev-1-1-BIOS-F22e.shtml) — запит заблоковано (403); лише назва/метадані, через фрагмент пошуку
- [Softpedia: GA-H170-Gaming 3 (rev. 1.0) BIOS F22e](https://drivers.softpedia.com/get/BIOS/Gigabyte/Gigabyte-GA-H170-Gaming-3-rev-1-0-BIOS-F22e.shtml) — так само
- [driverscollection.com: BIOS v.F22e](https://driverscollection.com/_53541113472cbeef773536c45d4/Download-Gigabyte-GA-H170-Gaming-3-(rev.-1.1)-BIOS-v.F22e-free) — отримано напряму
- [Сторінка підтримки GIGABYTE GA-H170-Gaming 3 (rev. 1.1)](https://www.gigabyte.com/Motherboard/GA-H170-Gaming-3-rev-11/support) — запит заблоковано (403), не прочитано
- [Tom's Hardware: "Gigabyte GA-H170-Gaming 3 is not working?"](https://forums.tomshardware.com/threads/gigabyte-ga-h170-gaming-3-is-not-working.3850371/) — отримано й прочитано напряму
- [AnandTech: "Gigabyte H170 Gamer 3 and NVMe issues"](https://forums.anandtech.com/threads/gigabyte-h170-gamer-3-and-nvme-issues.2481416/) — отримано й прочитано напряму
- [mathse/meltdown-spectre-bios-list (GitHub)](https://github.com/mathse/meltdown-spectre-bios-list) — отримано й прочитано напряму; нема секції Gigabyte

---

## BIOS F22e — external research (English, secondary)

External-web corroboration for the BIOS version already confirmed live
on the owner's board. Board/BIOS identity, release date (now resolved
by the actual firmware file's own internal timestamp), changelog
("Update CPU Microcode"), the plausible-but-unconfirmed Spectre/Meltdown
link, two real forum reports checked and found not to implicate F22e
itself, and full sources. See the Ukrainian version above for complete
detail.
