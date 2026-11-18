# RESEARCH: плагины TypeBox — инвентарь, пробелы, следующая волна, SDK v1

Дата: 2026-11-18 · База: v0.27.1 · Проверено по `src/main.cpp` (строки 1125–1340, 2059–2072,
3158–3239, 3480–3513, 4375–4417, 4827–4967), `examples/plugin_*`, `docs/PLUGIN_API.md`,
`docs/PLUGIN_GALLERY.md`, `docs/IDEAS-RESEARCH*.md`, `docs/ROADMAP-50.md`, `CMakeLists.txt`.

## Инвентарь API плагинов

ABI унаследован от TypeAndRun (`Plugins.pas`): нативные DLL, `__cdecl`, строки ANSI (CP_ACP).
Хост (`DiscoverPlugins`, main.cpp:1250) сканирует `<ExeDir>\Plugins\*.dll` и секцию `[Plugins]`
главного INI (`Имя=путь`), грузит `LoadLibraryW`, игнорирует DLL без `RunString` и `GetInfo`.
Идентичность плагина = имя dll без расширения (`calc.dll` → `calc`). `Load(HWND)` вызывается
лениво перед первым `RunString`, если ещё не вызван.

### Обязательный экспорт (контракт для всех 13 плагинов)

| Экспорт | Сигнатура | Семантика |
|---|---|---|
| `Load` | `void __cdecl Load(HWND mainWnd)` | инициализация; host window handle только как родитель MessageBox |
| `Unload` | `void __cdecl Unload(void)` | обязан убить окна/таймеры/трей — хост зовёт перед `FreeLibrary` при `/reread` |
| `GetInfo` | `TInfo __cdecl GetInfo(void)` | name/version/description/author/copyright/homepage — **ASCII**, иначе mojibake на Settings→Plugins (урок v0.12) |
| `GetCount` | `int __cdecl GetCount(void)` | число команд-слов; кормит Tab/дропдаун на первом слове |
| `GetString` | `const char* __cdecl GetString(int n)` | n-е слово команды |
| `GetCountET` | `int __cdecl GetCountET(const char* input)` | с 0.24 получает **всю строку** (не только слово у каретки!), до 0.26 гейт: не запускать, если строка начинается с `/` |
| `GetStringET` | `const char* __cdecl GetStringET(int n)` | n-я строка-кандидат; хост дедуплицирует, режет до 100, фильтрует fuzzy/prefix по слову каретки; Tab заменяет слово каретки, а если кандидат повторяет префикс — замену от нуля строки |
| `RunString` | `TExec __cdecl RunString(const char* str)` | исполнить строку; **первый вернувший `run != 0` забирает строку** (порядок = порядок discovery). Хост проверяет `ex.size == sizeof(TExec)`, иначе `run` отбрасывается |

`TInfo { int size; const char *name,*version,*description,*author,*copyright,*homepage; }`,
`TExec { int size; int run; const char* con_text; int con_sel_start; int con_sel_length; }`
(`con_sel_length < 0` трактуется как 0). С 0.14 при `EXC_PLUGIN` консоль остаётся открытой с
выделенным `con_text` (main.cpp:3084–3088) — таблица «Known host gaps» из галереи устарела.

### Опциональный экспорт

- `About` / `Options` — **реально вызываются** кнопками Settings→Plugins (main.cpp:4401–4416).
  Плагину с настройками обязательно их экспортировать.
- `GetPluginMenu`, `FastConfig`, `HookKey` — **биндятся, но хостом никогда не вызываются**
  (только BindPlugin, grep подтверждает). В SDK v1 пометить как reserved.
- Хост терпит 32-битные cdecl-выводы `_Name` (GetSym, main.cpp:1202).

### Управление из INI

- `[PluginState]`: `<dllbase>_Load=0/1` (гейт RunString/ET в дропдауне), `<dllbase>_SaveHistory`
  (дефолт 1, гейт попадания строки в History.dat, main.cpp:2068). Чеклист Settings→Plugins
  пишет обе секции на OK/Apply и применяет сразу (main.cpp:4962–4967, 5018).
- `[Exec] Plugins=0` — глобальное выключение диспатча. `[Plugins] Имя=путь` — внешние dll.
  Конфиг плагин читает сам через `GetPrivateProfileStringW` из `<ExeDir>\TypeBox.ini`.
- Найденный баг: `EtComplete` (main.cpp:3215–3216) НЕ проверяет `PluginLoadAllowed` —
  выключенный плагин молчит в дропдауне, но Tab-дополнение всё ещё отдаёт его ET-строки.
  Комментарий про `Plugins.ini` (main.cpp:1249) — код его не читает, задокументировать или убрать.

### Что есть у 13 shipped-плагинов (команды → секция INI)

`calc` (нет) · `units`: conv hex bin dec oct → `[Units] CopyResult,Popup` ·
`codec`: b64enc/b64dec/urlenc/urldec/hexenc/hexdec → `[Codec] CopyResult,Popup` ·
`vol`: vol +/-/mute/0..10 (SendInput media keys) → `[Vol] Step,Beeps` ·
`cliphist`: clip list/`<n>`/clear/pin/unpin/paste/type/ring + глобальный хоткей Ring →
`[Clipboard] Max,PollMs,RowChars,Persist,Balloons,Popup,PasteOnSelect,TypeDelayMs,Ring` ·
`taskman`: kill task top restart → `[Taskman] Confirm,ListMax,TopN` ·
`winlist`: win `<title>`/`<n>` → `[Winlist] AltTrick,Titles` · `pwgen`: pw len count flags
(CryptGenRandom) → `[Pwgen] Len,Symbols,Popup` · `timer`: timer/sw → `[Timer] Max,Sound,MiniWindow` ·
`sysmon`: cpu ram batt disk sys → `[Sysmon] SampleMs,DiskMinGB,Popup` · `util`: ts slug
sha256 /verify (BCrypt) · `note`: note/+ /list/open → `[Notes] Folder,Prefill` ·
`yt`: yt `<query>`/`<n>`/`/clear` (winhttp, три тира, сеть только по typed-команде) →
`[Youtube] ApiKey,Top,TimeoutMs,Instances,InstancesOverride`.

> Снапшот снят до вливания `weather` тем же днём. Плагин №14: `weather <city>` —
> одна строка (now + сегодня/завтра), `weather` = кэш → `[Weather] City` → IP-geo,
> `weather /clear` (winhttp, Open-Meteo + ipwho.is keyless, сеть только по typed-линии)
> → `[Weather] City,Units,TimeoutMs`.

Библиотеки по CMakeLists: всем `user32`; + `shell32,gdi32` (cliphist, note), `shell32` (taskman),
`advapi32` (pwgen), `winmm` (timer), `bcrypt` (util), `winhttp` (yt). `iphlpapi` — пока никем
не линкуется (решение для будущего `netinfo`).

## Что не зашиплено из галереи

Из 10 спецификаций `PLUGIN_GALLERY.md` не реализована ровно одна:

- **#10 nettools** (`weather <city>`, `ip`, `dns <host>`) — ждала решения по TLS
  («plain HTTP until the wininet-vs-winhttp policy call»). Решение принято в v0.26: `yt.dll`
  shipped с `winhttp.lib` и сетью строго по typed-команде. Спецификация галереи устарела
  (ws2_32 + plain HTTP) — переписать под winhttp/https, как в yt.c.

Backlog галереи (не специфицирован, но одобрен как ABI-feasible): `recycle.dll` (`rb list/empty`,
per-item restore требует ole32 — нет), `off.dll` (`off 30`), `find.dll` (нужен полноценный
ET — уже есть с 0.24!), `re.dll`, `todo.dll`, `wifi.dll` (netsh-парсинг хрупкий).

Наиболее ценные unshipped идеи, которые естественно имеют форму ПЛАГИНА (не хоста):
RI3 #237 net toolkit (`netinfo`/`ping`), #238 `wol` (ws2_32), #242 QR в консоли, #243 regex lab,
#244 JSON tools, #245 diff, #247 `jwt`, #248 cron humanizer, #249 lorem, #252 `pwcheck`,
#254 todo с due-датами, #259 untrash, #265 `unshort` (winhttp, typed-only), #276 webhooks
(winhttp, typed-only), #236 просмотр планировщика, #234 `clean temp`, #241 color picker.
Хостовые (не сюда): #201 Ctrl+R (ships v0.27), #202 макро-рекордер, #235 startup manager
(реестр — политика opt-in), #256 WUA (COM).

## Топ-5 следующих плагинов (ранжировано)

### 1. `nettools.dll` — weather / ip / dns / netinfo / ping  — M, сеть по typed-команде

- Команды: `weather <город>`, `ip`, `dns <хост>`, `netinfo`, `ping <хост> [n]`.
- Почему earns its keep: закрывает галерею 10/10 (нарратив релиза); `ip`/`dns`/`ping` —
  helpdesk-однодневки, которые сейчас требуют браузера или cmd.
- Effort: M (~350 LOC): HTTP-хелпер winhttp копипастится из `yt.c:257–297` (готовый
  GetConnect/SetTimeouts/ReadData-цикл); `netinfo` = `GetAdaptersInfo` → новая линковка
  `iphlpapi.lib` (системная, zero-dep правило не нарушает — зафиксировать решение).
- INI `[NetTools]`: `TimeoutMs=1500`, `WeatherURL=https://wttr.in/%s?format=3`,
  `IpUrl=https://checkip.amazonaws.com/`, `CacheSec=60`, `PingCount=4`, `LastCity=` (ET-кэш).
- i18n: 0 ключей — все строки владеет плагин (TInfo ASCII + консольная строка).
- ABI: не требуется. Полезные кандидаты в v1: `TB_IsHeadless`, `TB_OpenUrl` (yt читает
  `[UI] Headless` сам, main.cpp-политику «URL минимизированно» приходится дублировать).
- Trigger `w` НЕ брать — занят Wikipedia-quicklink'ом (плагин его затенит: plugins > quicklinks,
  v0.26). Сеть: только RunString; Load/ET — ноль пакетов (тест как в test-v0260).

### 2. `off.dll` — отложенное выключение/перезагрузка  — S, offline-safe

- Команды: `off 30`, `off now`, `off cancel`, `restart [мин]`.
- Почему: «выключи комп через 30 минут» — классика, которую сейчас делают через `shutdown /s /t`
  в cmd; галерейный backlog («off.dll») прямо ждёт. `stayawake` уже shipped — плагин симметричен.
- Effort: S (~150 LOC): `ExitWindowsEx`/`InitiateSystemShutdownExW` (advapi32) или `WinExec`
  shutdown.exe; свой таймер-уведомление за `[Off] WarnSec=60` до выключения.
- INI `[Off]`: `Confirm=1`, `WarnSec=60`, `DefaultMin=30`.
- i18n: 0 host-ключей; строки подтверждения — плагиновые (аналог `[Taskman] Confirm`).
- ABI: не требуется. Кандидат: `TB_Confirm(msg)` — единый danger-UI хоста вместо своего
  MessageBox (у taskman уже прецедент собственного confirm).
- Offline-safe полностью (shutdown.exe локален).

### 3. `todo.dll` — todo с человеческими датами  — M, offline-safe

- Команды: `todo купить молоко tomorrow 18:00`, `todo` (лист), `todo done <n>`,
  `todo del <n>`, `todo today`.
- Почему: RI3 #254 + галерейный backlog («todo.dll, great fit, ~220 LOC»); natural-date
  парсер (today/tomorrow/weekday/+Nd/HH:MM) — фича, которой нет ни в одном запускателе;
  отлично дружит с shipped `note.dll` (capture → организация).
- Effort: M (~300 LOC): файловое IO kernel32, паттерн строк из cliphist (кэш + WM_TIMER,
  ET из кэша).
- INI `[Todo]`: `File=todo.txt` (относительно ExeDir), `MaxRows=20`, `DueWarn=1`
  (баллон «просрочено N» при Load).
- i18n: 0 host-ключей.
- ABI: не требуется. Кандидат: `TB_PluginDataDir` (RI3 #275) — данные в `PluginsData\<name>\`
  вместо крошек у exe (note учил: дефолт обязан считываться «со стороны хоста»).
- Offline-safe.

### 4. `json.dll` — fmt / minify / get / keys + `jwt`  — M, offline-safe

- Команды: `json fmt`, `json minify`, `json keys`, `json get <$.a[0].b>` (вход — аргумент
  или буфер обмена), `jwt` (декод header/payload, humanize `exp`, флаг `alg:none`).
- Почему: RI3 #244 (H) + #247 (M) — ежедневный рабочий инструмент API-разработчика;
  калька с Keypirinha utility-набора, который осиротел; соседи `util` уже дали `ts`/`slug`/`sha256`.
- Effort: M (~380 LOC рекурсивный парсер + форматтер; JWT — base64url из codec-паттерна).
  Лимит `[Json] MaxInput=5242880`, `Indent=2`, `Copy=1`.
- INI `[Json]`: `MaxInput`, `Indent`, `Copy`; `[Json] ClipboardFallback=1`.
- i18n: 0 host-ключей.
- ABI: кандидат `TB_SetClipboardText(utf16)` — брокер буфера обмена от имени хостового
  thread-window (урок висящих клипбордов из v0.24).
- Offline-safe.

### 5. `qr.dll` — QR прямо в консоли  — M, offline-safe

- Команды: `qr <текст|url>`, `qr clip`, `qr png <файл>`.
- Почему: RI3 #242; рендер half-block `▄█` в одну консольную строку — скриншот-фича и
  реальная польза (паринги, ссылки, «передай телефон»); сетевые QR-сервисы отпадают сами
  по offline-first.
- Effort: M (~320 LOC, байт-режим QR-энкодера ~250 LOC с нуля, таблицы ECC).
- INI `[Qr]`: `Ecl=M` (L/M/Q/H), `Margin=1`, `AutoCopy=1`, `PngDir=`.
- i18n: 0 host-ключей.
- ABI: не требуется (опционально `TB_ThemeColors` для dark/light png-инверсии).
- Offline-safe.

Запасные (следующая после этой волна): `unshort` (#265), `webhooks` (#276), `diff` (#245),
`du`-бары (#233), `untrash` (#259), `wol` (#238), `startup` (#235, требует решения по реестру).

## Что нужно для SDK v1 (ABI additions)

План v0.29: `include/typebox_plugin.h` + правки `docs/PLUGIN_API.md`. Заморозить ABI v1 как
подмножество существующего (обратная совместимость 100%), новое — только опциональный экспорт.

### Что должен содержать заголовок

1. Типы и константы: `TInfo`, `TExec` (как сейчас), макросы `TB_EXPORT`
   (`__declspec(dllexport)`), `TB_PLUGIN_API` (`__cdecl`), `TB_ABI_VERSION 1`,
   `TB_INIT_TINFO/TB_INIT_TEXEC` (обязательный `size` — хост молча выбрасывает `run`
   при неверном `size`; `TInfo.size` хост не проверяет — пусть проверяет).
2. Все 13 typedef'ов (8 обязательных + 5 опциональных), с явной пометкой:
   `GetPluginMenu/FastConfig/HookKey` — **host never calls, reserved**;
   `About/Options` — вызывать обязательно, если есть настройки.
3. Конвенции строк: ANSI CP_ACP на границе; `TInfo` только ASCII; кандидаты ET — готовые
   команды целиком; один `static char[]` на результат, не стек.
4. Протокол ET (устно передаваемый сегодня): `GetCountET` получает всю строку (v0.24),
   строка с ведущим `/` — внутренняя (v0.26), фильтр хоста fuzzy/prefix по слову каретки,
   кэп 100, дедуп; бюджет — микросекунды, тяжёлое в `RunString`, кэш через HWND_MESSAGE+WM_TIMER.
5. `[PluginState]`-контракт: `<dllbase>_Load`, `<dllbase>_SaveHistory` (дефолт 1 — документировать
   `pwgen_SaveHistory=0`-стиль для секретов), чеклист Settings→Plugins, `[Exec] Plugins=0`.

### Хост→плагин колбэки (сейчас каждый плагин изобретает сам) — новое поле ABI

Опциональный экспорт `void __cdecl Load2(HWND, const TB_HOST_API*)` +
```c
typedef struct TB_HOST_API {
    unsigned size;                    /* sizeof — версионный гейт */
    unsigned abi_version;             /* 1 */
    const wchar_t* (*exe_dir)(void);
    const wchar_t* (*ini_path)(void);
    int   (*ini_int)(const wchar_t* sec, const wchar_t* key, int def);
    const wchar_t* (*ini_str)(const wchar_t* sec, const wchar_t* key,
                              const wchar_t* def, wchar_t* buf, int cch);
    int   (*is_headless)(void);       /* сейчас yt/note/sysmon/cliphist читают [UI] Headless сами */
    void  (*show_balloon)(const wchar_t* title, const wchar_t* text);
    int   (*open_url)(const wchar_t* url);      /* headless-политика минимизации — за хостом */
    int   (*set_clipboard)(const wchar_t* text);/* против висящего клипборда (урок v0.24) */
    int   (*plugin_data_dir)(const wchar_t* name, wchar_t* buf, int cch); /* RI3 #275 */
    int   (*can_inject)(HWND target);           /* allowlist TypeBoxTestPad, см. ниже */
} TB_HOST_API;
```
Хост вызывает `Load2`, если экспорт есть, иначе `Load(HWND)` — пилот: cliphist + yt.

### Что сейчас ad-hoc и обязано стать контрактом

- **Headless/TestPad**: allowlist «инжектировать только в окно класса `TypeBoxTestPad`»
  захардкожен в cliphist.c:497–502 и 567–572, чтение `[UI] Headless` — в 4 плагинах отдельно.
  Заморозить как `is_headless()`/`can_inject()`; иначе новый плагин, шипящий SendInput,
  в CI будет печатать в чужие приложения.
- **Магические числа**: трей-`uID` 0x04CB (cliphist), 0x071D (timer), `RegisterHotKey` id 991 —
  в header: `TB_TRAY_UID(base)` (хэш имени dll) и таблица резервирования hotkey-id;
  иначе коллизии при росте числа плагинов.
- **Хендшейк**: нет `GetAbiVersion()` на плагине — добавить опциональным экспортом;
  `/load` обязан печатать отсутствующие обязательные экспорты (RI3 #268 ABI gate) вместо
  тихого «not a plugin».
- **Unicode v2**: зафиксировать в header слот `HOST_FLAGS_UNICODE` + имена
  `GetInfoW/RunStringW/GetStringETW` (обещание из PLUGIN_API.md), но в v1 не вводить —
  отдельный минор; `TInfo` ASCII-политика до v2 обязательна.
- **Багофиксы хоста до заморозки**: гейт `PluginLoadAllowed` в `EtComplete` (см. Инвентарь);
  убрать/реализовать `Plugins.ini` из комментария; править «Known host gaps» в галерее
  (con_text live с 0.14, whole-line ET с 0.24, https-политика закрыта 0.26).
- **Тесты**: заморозить контракт CI-харнеса `plugtest <dll> <line>...` (RI3 #272) и
  «offline-at-load» гейтку (0 пакетов до первой typed-команды) как приёмочные для SDK v1.

Порядок работ: header + тестовый harness → клининг ad-hoc в 4 «headless»-плагинах →
Load2-пилот (cliphist) → nettools на свежем ABI → правка PLUGIN_GALLERY/PLUGIN_API.
ABI-изменения = минор хоста; плагины и хост поставляются атомарно, поэтому ломать
что-либо из восьми обязательных экспортов в v1 запрещено.
