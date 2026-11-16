# TypeBox plugin gallery

Ten concrete, zero-dependency plugin designs for the TypeAndRun-compatible ABI
([docs/PLUGIN_API.md](PLUGIN_API.md)), each buildable as one C file with the
same toolchain the host ships. Every claimed behavior below was checked against
the host implementation in `src/main.cpp` (plugin loader, drop-down feed,
`RunPlugins` chain). A C programmer should be able to implement any plugin
straight from its spec — copy the skeleton from
[`examples/plugin_calc/calc.c`](../examples/plugin_calc/calc.c) and follow the
per-plugin sections.

## How a TypeBox plugin works

- A plugin is a native DLL exporting cdecl functions: `Load(HWND)`, `Unload()`,
  `GetInfo()`, `GetCount()`, `GetString(n)`, `GetCountET(input)`,
  `GetStringET(n)`, `RunString(str)` (plus optional `About`/`Options`, see below).
- The host scans `<ExeDir>\Plugins\*.dll` and `[Plugins] Name=path` entries in
  `TypeBox.ini` (`DiscoverPlugins`), `LoadLibraryW`s each, and ignores DLLs that
  export neither `RunString` nor `GetInfo`. `Load(mainWnd)` is called once,
  lazily before the first `RunString` if not already called.
- `GetInfo` returns `TInfo` — its six string fields are shown verbatim on the
  Settings -> Plugins page, so they must be **ASCII** (the host converts them
  from ANSI/CP_ACP; non-ASCII bytes arrive as mojibake — see the calc.c fix in
  the v0.12.0 changelog).
- `GetCount`/`GetString(n)` return the plugin's command words (e.g. `"calc"`);
  they feed the drop-down/Tab completion only when the caret is on the **first
  word** of the line.
- While typing, the host calls `GetCountET(word)` + `GetStringET(n)` with only
  the **word at the caret** (ANSI). Returned rows replace that whole word when
  accepted. The host de-duplicates rows across all sources, caps the list at
  100, and filters them: in prefix mode a row must start with the caret word;
  in the default fuzzy mode a row must score > 0 (caret word as subsequence).
- On Enter the host runs aliases/sysaliases first, then hands the whole ANSI
  line to each enabled plugin's `RunString`; the first plugin that returns
  `TExec` with `size == sizeof(TExec)` **and** `run != 0` consumes the line and
  the host plays the plugin success sound.
- With `run != 0` the host also applies `con_text`/`con_sel_start`/
  `con_sel_length` to the console edit — but `Submit()` then clears the edit
  and hides the box, so `con_text` is **not a lasting output surface** (see
  "Known host gaps" below); plugins show output via their own dialog/window or
  the clipboard.

## Ground rules for every plugin in this gallery

These constraints keep each plugin one C file, portable, and host-compatible:

- **Build**: `gcc -shared -O2 -static -o X.dll X.c -l<libs>` (the `-static`
  embeds the MinGW runtime so the DLL needs nothing from a MinGW PATH; MSVC:
  `cl /LD /Fe:X.dll X.c`, add `#define PLUGIN_API __declspec(dllexport)` like
  calc.c does). No third-party code, ever.
- **Allowed libraries** (already system libs, no exotic ones):
  `user32`, `shell32`, `shlwapi`, `gdi32`, `comctl32`, `comdlg32`, `winmm`,
  `kernel32`, `advapi32`, `ws2_32`. Nothing else gets a link flag in this doc.
- **Strings cross the ABI as ANSI (CP_ACP)**. Convert to wide internally with
  `MultiByteToWideChar(CP_ACP, ...)` for the W-APIs. Non-ACP characters from
  the console are already lossy at the host boundary (`ToAnsi` uses default
  chars), so design triggers and outputs ASCII-first.
- **Returned strings must outlive the call only until the host copies them**
  (the host converts each row immediately), but never return stack pointers:
  use one `static char` buffer per call site, like calc.c's `g_result`.
- **`TExec.size` must be `sizeof(TExec)`** — the host drops `run` unless
  `ex.size == sizeof(TExec)`. Same habit for `TInfo.size` (host doesn't check
  it, calc.c sets it).
- **All callbacks run on the host UI thread.** `GetCountET`/`GetStringET` fire
  on every keystroke: budget them at microseconds-to-milliseconds (serve from
  cache, refresh via your own hidden message-only window + `WM_TIMER`, never
  on the keystroke path). Long work (network, walk) only inside `RunString`,
  always with a hard timeout.
- **`Unload()` must undo `Load()`**: destroy plugin windows/timers/threads and
  remove tray icons — the host calls `Unload` for every loaded plugin before
  `FreeLibrary` on rescan (`/reread`). Keep `DllMain` empty; do all setup in
  `Load`.
- Do not parent your top-level windows on the `mainWnd` from `Load(HWND)` —
  owned windows would hide with the console. Use `HWND_MESSAGE` for hidden
  pump windows and plain unowned top-level windows for visible ones; use the
  host `mainWnd` only as a transient `MessageBox` parent inside `RunString`.
- `GetPluginMenu`, `FastConfig` and `HookKey` are bound by the host but **never
  called** today — don't build on them. `About()` and `Options()` **are**
  called from the Settings -> Plugins page buttons, so any plugin with settings
  should export them.
- Per-plugin enable and history live in `[PluginState]` of `TypeBox.ini`:
  `<dllbase>_Load=0/1` gates `RunString`/ET/GetString; `<dllbase>_SaveHistory`
  (default 1) decides whether the plugin's typed line enters history. The
  global `[Exec] Plugins=0` switch disables all plugin dispatch.
- **Config**: plugins read user settings with `GetPrivateProfileStringW` from
  `<ExeDir>\TypeBox.ini` (portable layout; get `ExeDir` via
  `GetModuleFileNameW(NULL, ...)`). The host writes its INI key-by-key, so an
  unknown `[Section]` next to it is preserved. Plugin-owned data files go to
  `<ExeDir>\Plugins\<name>.dat` (stays portable).

### Known host gaps affecting plugins

| Gap | Effect on plugins | Workaround used below | Host support needed |
|---|---|---|---|
| `Submit()` clears the console + hides the box right after a plugin consumes the line | `con_text` results flash invisibly; typed line only survives in history/`g_lastRun` (`/undo`) | show results in a plugin `MessageBoxW`/window and copy them to the clipboard | one-line host change: when a plugin consumed the line with non-empty `con_text`, skip the clear+hide and keep the caret text selected |
| `GetCountET` receives only the caret **word**, never the full line | no context-aware argument completion; ET can't tell "`notep` after `kill`" from "`notep` anywhere" | two-mode rows: full `kill <name>` rows while typing the command word; bare `<name>` rows otherwise (word-replacement makes them land correctly) | richer variant, e.g. `GetCountETW(const wchar_t* line, int caret)` (would also kill the ANSI lossiness) |
| ET runs uncached on every keystroke (roadmap item #45) | snapshot-based plugins (processes, windows) must self-cache | message-only window + `WM_TIMER` refresh, ET answers from cache | host ET result cache (#45), invalidate on `/reread` |
| No host-provided status strip / balloon API for plugins | plugins alert via their own tray icon (`Shell_NotifyIconW`, own `uID`) | per-plugin balloon or `MessageBoxW` | optional: host-brokered balloon/`ShowBox` entry point |
| No TLS in the allowed library set (no `wininet`/`winhttp`; `ws2_32` is raw sockets) | network plugins can only speak **plain HTTP** | use HTTP-only endpoints (wttr.in, checkip.amazonaws.com) | if HTTPS is wanted, the host build gate must allow `-lwinurl`-free `wininet.lib`/`winhttp.lib` as system libs |

### Shared TInfo constants

Every gallery plugin uses the same identity fields (ASCII only — this is a
host parser/display limitation, see the v0.12.0 calc fix):

```c
i.author    = "TypeBox project";
i.copyright = "GPL-3.0";
i.homepage  = "https://github.com/shevchukdp/TypeBox";
```

The per-plugin tables below give the remaining `name` / `version` /
`description` values.

---

## 1. units - unit & base converter (`units.dll`)
> **Status: shipped in v0.14.0** — implemented as `examples/plugin_units/units.c`.

| Field | Value |
|---|---|
| DLL | `Plugins\units.dll` |
| Triggers | `conv <value> <from> <to>` · `hex <n>` · `bin <n>` · `dec <n>` · `oct <n>` |
| Build | `gcc -shared -O2 -static -o units.dll units.c` (no `-l` libs) |
| LOC | ~260 |

**TInfo**

| Field | Value |
|---|---|
| name | `Units` |
| version | `1.0` |
| description | `conv 10 km mi - units and hex/bin/dec/oct base converter` |

**What it does.** Converts values between units in six groups (length, mass,
temperature, speed, data, time) with `conv 10 km mi`, and converts integers
between bases with `hex 255`, `bin 0xFF`, `dec 1010`, `oct 777`. Everything is
pure arithmetic in one C file — no Win32 beyond the ABI. Results are copied to
the clipboard and shown in a `MessageBoxW` (the `con_text` path is set too, so
the result would appear as-is if the host adopts the keep-visible fix).

**Config**

| `[Units]` key | Meaning | Default |
|---|---|---|
| `CopyResult` | also place the result on the clipboard | `1` |
| `Popup` | show result MessageBox; `0` = clipboard only (silent) | `1` |

**GetCount/GetString**: `5` — `"conv"`, `"hex"`, `"bin"`, `"dec"`, `"oct"`.

**Example dropdown rows** (typed word -> rows; all rows are runnable lines):

- `con` -> `conv 10 km mi`, `conv 98.6 f c`, `conv 70 kg lb`, `conv 100 kph mph`, `conv 100 gb mb`
- `he` -> `hex `, `hex 255`
- `bi` -> `bin `, `bin 0xFF`

**Edge cases / failure behavior.** Unknown unit -> `conv: unknown unit 'xy' (try km,mi,kg,lb,f,c,...)`;
garbage number -> parse error row; divide/overflow guards use `strtod`/
`strtoull` error flags and print `"out of range"`; base inputs above 2^64-1
rejected; temperature `k` refuses negatives below absolute zero; empty argument
-> usage hint instead of an error.

**Implementation notes.** Table-driven: `struct { const char* u; double to_base; }`
per group + special-case affine functions for `f`/`c`/`k`; bases reuse one
`strtoull(base)` + `_ultow`-style formatter with `0x`/`0b` prefixes. Clipboard:
`OpenClipboard`/`EmptyClipboard`/`SetClipboardData(CF_TEXT, ...)` (user32).
Result MessageBox: `MessageBoxW(mainWnd /*may be NULL*/, text, L"units", MB_OK)`.
`Load`/`Unload` are one-liners (no state).

**Host support:** benefits from the keep-console-visible fix (result would then
stay in the console, clipboard copy optional); nothing required.

---

## 2. codec - base64 / URL / hex codec (`codec.dll`)
> **Status: shipped in v0.14.0** — implemented as `examples/plugin_codec/codec.c`.

| Field | Value |
|---|---|
| DLL | `Plugins\codec.dll` |
| Triggers | `b64enc <text>` · `b64dec <b64>` · `urlenc <text>` · `urldec <pct>` · `hexenc <text>` · `hexdec <hex>` |
| Build | `gcc -shared -O2 -static -o codec.dll codec.c` (no `-l` libs) |
| LOC | ~220 |

**TInfo**

| Field | Value |
|---|---|
| name | `Codec` |
| version | `1.0` |
| description | `b64enc/b64dec urlenc/urldec hexenc/hexdec - paste, encode, decode` |

**What it does.** One-shot encoder/decoder for the three encodings a launcher
user pastes all day. `b64dec` tolerates whitespace/URL-safe alphabets (maps
`-`/`_` and ignores stray padding errors when recoverable); `urlenc` escapes
everything outside RFC 3986 unreserved set; results go to the clipboard and a
short popup. Pure C tables, no Win32 beyond clipboard/popup.

**Config**

| `[Codec]` key | Meaning | Default |
|---|---|---|
| `CopyResult` | copy result to clipboard | `1` |
| `MaxInput` | refuse input longer than N chars | `4096` |

**GetCount/GetString**: `6` — `"b64enc"`, `"b64dec"`, `"urlenc"`, `"urldec"`, `"hexenc"`, `"hexdec"`.

**Example dropdown rows** (typed word -> rows):

- `b64` -> `b64enc `, `b64dec `
- `ur` -> `urlenc `, `urldec `
- `hex` -> `hexenc `, `hexdec `

**Edge cases / failure behavior.** Invalid base64 char -> `b64dec: invalid
character '!'`; wrong length -> `b64dec: bad padding`; `hexdec` with odd
length or non-hex digit -> error row; input over `MaxInput` -> `input too
large`; `b64enc` of empty string yields empty string (valid, not an error).

**Implementation notes.** 64-entry alphabet table + 4->3 / 3->4 loops; URL
encoder with a `unreserved[256]` bool table + `sprintf %02X`; decode buffers
are `static char g_out[MAXINPUT*4]` sized by the INI. Same clipboard/popup
pattern as units (§1). No threads, no windows.

**Host support:** none (same keep-console-visible benefit as §1).

---

## 3. vol - volume control (`vol.dll`)
> **Status: shipped in v0.14.0** — implemented as `examples/plugin_vol/vol.c`.

| Field | Value |
|---|---|
| DLL | `Plugins\vol.dll` |
| Triggers | `vol +` · `vol -` · `vol mute` · `vol <1..10>` · `vol` |
| Build | `gcc -shared -O2 -static -o vol.dll vol.c -luser32` |
| LOC | ~120 |

**TInfo**

| Field | Value |
|---|---|
| name | `Volume` |
| version | `1.0` |
| description | `vol +/-/mute or vol 1..10 - media-key volume stepping` |

**What it does.** Sends simulated media volume keys with `SendInput`
(`VK_VOLUME_UP` / `VK_VOLUME_DOWN` / `VK_VOLUME_MUTE`), which every Windows
10/11 mixer honors. `vol +`/`vol -` send one system step (default 2 %);
`vol <1..10>` sends N*5 presses so `vol 5` means "roughly half" from wherever
you are; `vol mute` toggles mute. This deliberately avoids Core Audio: precise
per-endpoint control needs `ole32`/`MMDeviceAPI`, which is outside the allowed
library set.

**Config**

| `[Vol]` key | Meaning | Default |
|---|---|---|
| `Step` | presses per `+`/`-` | `1` |
| `Beeps` | `1` = also `MessageBeep` after set | `0` |

**GetCount/GetString**: `1` — `"vol"`.

**Example dropdown rows** (typed word -> rows):

- `vo` -> `vol +`, `vol -`, `vol mute`, `vol 0` ... `vol 10` (11 rows; host caps/Alt+1..9 covers the useful prefix)

**Edge cases / failure behavior.** Unknown verb -> usage row `vol: use + - mute or 0..10`;
no audio device: media keys are silently swallowed by the OS — the plugin
cannot detect this, so it reports success anyway (documented in the help row);
`SendInput` returning 0 (blocked by a secure desktop) -> `vol: input blocked`.
Plugin never reads volume, so it never shows a stale level.

**Implementation notes.** One helper: `press(BYTE vk)` builds two
`INPUT` records (`KEYEVENTF_KEYUP` after key-down) with `wVk=vk` and calls
`SendInput`; everything else is `_stricmp` verb dispatch. `con_text` set to a
hint like `vol 5` (harmless today, useful after the keep-visible fix).

**Host support:** exact set/get level requires Core Audio (`ole32` +
`mmdeviceapi`) — mark this plugin "level readback: needs host to allow
`-lole32`", otherwise media-key stepping is the shipped behavior.

---

## 4. cliphist — clipboard history (`cliphist.dll`)

> **Status: shipped in v0.16.0** — implemented as `examples/plugin_cliphist/cliphist.c`
> (console-first: with `[Clipboard] Popup=0` the list answers inside the TypeBox console,
> never a blocking dialog; `[Clipboard] Persist=1` keeps history across runs in
> `Plugins\cliphist.dat`).

| Field | Value |
|---|---|
| DLL | `Plugins\cliphist.dll` |
| Triggers | `clip` (list) · `clip <n>` (re-copy entry n) · `clip clear` · `clip pin <n>` |
| Build | `gcc -shared -O2 -static -o cliphist.dll cliphist.c -luser32` |
| LOC | ~260 |

**TInfo**

| Field | Value |
|---|---|
| name | `Clipboard History` |
| version | `1.0` |
| description | `clip <n> - re-copy the n-th copied text (last 20 kept in RAM)` |

**What it does.** Captures every new `CF_UNICODETEXT` clipboard change by
polling `GetClipboardSequenceNumber()` at 500 ms from a hidden message-only
window created in `Load(HWND)`. Typing `clip` lists the newest entries as
drop-down rows (`clip 3: first line of that entry...`); picking a row (or
typing `clip 3`) puts that entry back on the clipboard, ready to Ctrl+V.
Entries are stored as `wchar_t*` internally and kept only in RAM by default.

**Config**

| `[Clipboard]` key | Meaning | Default |
|---|---|---|
| `Max` | entries kept (1..50) | `20` |
| `PollMs` | capture poll interval | `500` |
| `RowChars` | row preview length | `48` |
| `Persist` | `1` = save/load `Plugins\cliphist.dat` across runs | `0` |
| `Balloons` | tray balloon after re-copy | `0` |

**GetCount/GetString**: `1` — `"clip"`.

**Example dropdown rows** (typed word -> rows; rows must start with the typed
word in prefix mode — they all start with `clip`):

- `cli` -> `clip 1: Copy that invoice`, `clip 2: C:\Reports\nov.pdf`, `clip 3: +380 67 123 45 67`, `clip clear`
- `clip c` -> `clip clear`

**RunString contract.** `RunString` parses `clip <integer-prefix>`: it accepts
the exact row text (row suffix after the number is ignored, which is what lets
rows carry previews). No argument -> copies nothing, lists via a `MessageBoxW`
numbered list (fallback UI) and copies the whole list to the clipboard.

**Edge cases / failure behavior.** Empty history -> `clip: nothing captured
yet`; `clip 9` with 3 entries -> `clip: only 3 entries (1..3)`; entry equal to
top entry is skipped; copies over 32 KB and non-text formats are ignored
(we keep only `CF_UNICODETEXT`); clipboard locked by another app
(`OpenClipboard` FALSE) -> retry next tick, never block; `Unload` destroys the
message window (kills the timer) and frees all entries.

**Implementation notes.** `CreateWindowExW(..., HWND_MESSAGE, ...)` with a
tiny `WNDPROC`; `SetTimer(hwnd, 1, PollMs, NULL)`; on `WM_TIMER`:
`GetClipboardSequenceNumber()` change -> `OpenClipboard(NULL)` +
`GetClipboardData(CF_UNICODETEXT)` + `GlobalLock` copy. Re-copy:
`OpenClipboard` -> `EmptyClipboard` -> `SetClipboardData(CF_UNICODETEXT,
GlobalAlloc+memcpy)` -> `CloseClipboard` (keep the copy in a `static` so the
clipboard owns a valid block). Sanitize preview: replace `\r\n\t` with space,
truncate at `RowChars`. Balloon via `Shell_NotifyIconW` (shell32) with its own
`uID`, added in `Load`, removed in `Unload`.

**Host support:** benefits from the keep-console-visible fix (`clip 3` could
confirm right in the console); context-aware ET (full-line variant) would let
rows also work when the user adds arguments after `clip` — not required today.

---

## 5. taskman — process list & kill (`taskman.dll`)

> **Status: shipped in v0.16.0** — implemented as `examples/plugin_taskman/taskman.c`
> (console-first: results are console lines; the only popup is the one kill confirmation,
> and `[Taskman] Confirm=0` removes it too — automation-safe).

| Field | Value |
|---|---|
| DLL | `Plugins\taskman.dll` |
| Triggers | `kill <name>` (terminate) · `task <substr>` (list) |
| Build | `gcc -shared -O2 -static -o taskman.dll taskman.c -lkernel32 -luser32` |
| LOC | ~300 |

**TInfo**

| Field | Value |
|---|---|
| name | `Task Manager Lite` |
| version | `1.0` |
| description | `kill <proc> / task <filter> - list processes and kill by name` |

**What it does.** `kill notepad` terminates every process whose image name
matches (exact, case-insensitive, `.exe` optional) after one `MB_YESNO`
confirmation naming the executable and PID count; `task chrome` opens a small
top-level `LISTBOX` window with the matching processes (`1234  chrome.exe`) —
read-only, closed with Esc/Enter or the X. The process snapshot is cached and
refreshed on a 2 s `WM_TIMER` from a message-only window so keystroke-path ET
never walks the toolhelp snapshot.

**Config**

| `[Taskman]` key | Meaning | Default |
|---|---|---|
| `CompleteArgs` | `1` = offer bare process names as argument ET rows (see two-mode caveat) | `1` |
| `Confirm` | `0` = kill without the Yes/No popup (power users) | `1` |
| `ListMax` | max rows in the `task` window | `300` |

**GetCount/GetString**: `2` — `"kill"`, `"task"`.

**Example dropdown rows** (two-mode, because ET only sees the caret word):

- typing the command word `kil` -> `kill notepad.exe`, `kill chrome.exe`, `kill cmd.exe` (top candidates from the cached snapshot, most recently seen first)
- typing an argument after `kill `, word `note` -> `notepad.exe`, `notepad++.exe` (bare names; the host replaces just the caret word, so the line lands as `kill notepad.exe`)
- `tas` -> `task `, `task chrome`

**Edge cases / failure behavior.** Match on the protected set
(`csrss, wininit, winlogon, services, lsass, smss, System, Registry`) ->
`kill: refusing <name> (system process)`; `OpenProcess` failure -> `kill:
access denied for <name> (PID 1234)` (elevated hosts kill elevated procs;
non-elevated simply cannot — no UAC prompt is attempted, documented); no
match -> `kill: no process 'foo'`; zero visible processes for the list ->
list window with `task: nothing matched 'xy'`. The plugin itself is killable
via the host? Not our problem — but `kill TypeBox`/`kill TypeBox.exe` gets an
extra `MB_ICONWARNING` wording ("this ends the launcher").

**Implementation notes.** Snapshot: `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)`
+ `Process32FirstW/NextW` into a static `struct { DWORD pid; wchar_t name[MAX_PATH]; }`
array; terminate: `OpenProcess(PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION)`
+ `QueryFullProcessImageNameW` (for the confirm text) + `TerminateProcess(h, 0)`.
List window: register one `WNDCLASSW` in `Load`, `CreateWindowExW` top-level +
`CreateWindowExW("LISTBOX", ...)`, fill with `LB_ADDSTRING`, handle `WM_CLOSE`
/`WM_KEYDOWN VK_ESCAPE`; keep a single instance and just `SetForegroundWindow`
if already open. All calls on the host thread — the snapshot walk is
sub-millisecond, but ET still serves from the timer-refreshed cache only.

**Host support:** ET cache (roadmap #45) would let us drop the self-timer for
`snapshot`; full-line ET would make argument mode unambiguous (rows only when
the first word is `kill`/`task`).

---

## 6. winlist — window switcher (`winlist.dll`)

> **Status: shipped in v0.17.0** — implemented as `examples/plugin_winlist/winlist.c`
> (console-first: activation, `flashed` fallback and every error are console lines,
> zero dialogs; `[Winlist] AltTrick/Titles` configure the foreground-lock trick
> and the list cap).

| Field | Value |
|---|---|
| DLL | `Plugins\winlist.dll` |
| Triggers | `win <substr>` (activate) · `win <n>` (pick from last filter) · `win` (list) |
| Build | `gcc -shared -O2 -static -o winlist.dll winlist.c -luser32` |
| LOC | ~220 |

**TInfo**

| Field | Value |
|---|---|
| name | `Window Switcher` |
| version | `1.0` |
| description | `win <title> - activate a visible window by title substring` |

**What it does.** Enumerates top-level visible windows (z-order, ownerless,
non-empty title) and activates the best match for a title substring: `win
Term` raises the topmost window whose title contains "Term". Because ET rows
replace the caret word, the switcher keeps the last filter's match list and
offers numbered rows `win 1..9`; `win 3` activates the third of them.
A single match activates immediately (restoring minimized windows); zero
matches reports.

**Config**

| `[Winlist]` key | Meaning | Default |
|---|---|---|
| `AltTrick` | `1` = on `SetForegroundWindow` failure, tap `VK_MENU` via `SendInput` and retry once (beats the foreground lock) | `1` |
| `Titles` | max remembered matches for `win <n>` | `9` |

**GetCount/GetString**: `1` — `"win"`.

**Example dropdown rows** (typed word -> rows):

- `wi` -> `win ` (bare command, opens the picker), plus static `win 1` ... `win 9` (last matches, empty when none)
- argument mode, word `term` (e.g. after `win `) -> `Terminal`, `Documents - Notepad` — bare title snippets starting with the typed word; replacing the caret word yields `win Terminal`

**Edge cases / failure behavior.** No match -> `win: no window like 'xy'`;
activation blocked by foreground lock and the Alt-tap retry still fails ->
`FlashWindowEx` on the target (blinks, does not steal) and report `win:
flashed 'x' - click it`; minimized window -> `IsIconic` -> `ShowWindow(SW_RESTORE)`
before `SetForegroundWindow`; titles with newlines (rare chat apps) sanitized
to one line for rows; `win 4` without a previous filter -> `win: no filter yet`.

**Implementation notes.** `EnumWindows` callback filters: `IsWindowVisible`,
`GetWindowTextLengthW > 0`, `GetWindow(GW_OWNER) == NULL`, skip our own list
windows by class name; store `struct { HWND hwnd; wchar_t title[128]; } g_last[9]`.
Activation sequence as in the spec. Rows come from `g_last` (cheap, no
enumerate on keystroke); the first-word rows `win 1..9` reflect the previous
session's filter — acceptable staleness, `win <substr>` always re-enumerates
inside `RunString` (sub-millisecond).

**Host support:** none required; would feel better with the keep-console-visible
fix (confirmation `win: activated 'x'` staying in the console) and with
full-line ET (first-word vs argument mode).

---

## 7. timer — timers & stopwatch (`timer.dll`)

| Field | Value |
|---|---|
| DLL | `Plugins\timer.dll` |
| Triggers | `timer <dur> [label]` (e.g. `timer 5m tea`) · `timer stop [label]` · `sw` (start/stop) · `sw reset` |
| Build | `gcc -shared -O2 -static -o timer.dll timer.c -luser32 -lwinmm -lshell32` |
| LOC | ~200 |

**TInfo**

| Field | Value |
|---|---|
| name | `Timer` |
| version | `1.0` |
| description | `timer 5m tea + stopwatch sw - countdowns with tray alert` |

**What it does.** Countdown timers with a tray balloon + `MessageBeep` when
due, plus a start/stop/reset stopwatch whose elapsed time `sw` reports on
demand. State lives in a static array (up to 8 timers, one stopwatch); a
message-only window created in `Load(HWND)` ticks `WM_TIMER` every 1 s to arm
alerts and update the optional mini-window. `timer 25m` covers the everyday
pomodoro case; `sw` answers `0:07:23` without ever owning a console.

**Config**

| `[Timer]` key | Meaning | Default |
|---|---|---|
| `MiniWindow` | `1` = stopwatch shows a topmost mini label window while running | `1` |
| `Sound` | `0` = balloon without `PlaySoundW` | `1` |
| `Max` | simultaneous timers (1..8) | `8` |

**GetCount/GetString**: `2` — `"timer"`, `"sw"`.

**Example dropdown rows** (typed word -> rows):

- `tim` -> `timer 5m`, `timer 10m`, `timer 25m`, `timer stop`
- `sw` -> `sw`, `sw reset`

**Edge cases / failure behavior.** Unparseable duration (`timer xyz`) ->
`timer: use 90, 5m, 1h30m, 90s`; `timer 0` -> error; more than `Max` running
timers -> `timer: 8 timers already running (timer stop first)`; `timer stop`
with none running -> `timer: nothing running`; `sw` toggles (start when idle,
stop when running); due-timer balloon uses its own `Shell_NotifyIconW` `uID`,
added in `Load`, deleted in `Unload` (no ghost icon after `/reread`).
`MessageBeep` + optional `PlaySoundW(L"SystemExclamation", NULL,
SND_ASYNC|SND_ALIAS)` (winmm) fire from the timer window proc, not from
`RunString`.

**Implementation notes.** Duration parser: scan `<num><suffix>` runs (`s`,`m`,
`h`) into seconds, label = remainder of the line verbatim (ASCII, stored in
the slot). Tick loop compares `GetTickCount64()` deadlines. Alert: set
`NOTIFYICONDATAW` (shell32) `NIF_INFO` with the label + `" - time!"`;
`FlashWindowEx(mainWnd)` too, since the host window may be open. Mini-window:
one topmost borderless `STATIC`-class window with `SetWindowTextW` per tick;
destroyed when the stopwatch stops. All timing on the plugin's own timer
window — nothing on the host thread except state reads.

**Host support:** none required. (A host `ShowBox` broker would let an alert
reopen the console — nice-to-have, not needed.)

---

## 8. sysmon — CPU / RAM / battery / disk (`sysmon.dll`)

| Field | Value |
|---|---|
| DLL | `Plugins\sysmon.dll` |
| Triggers | `cpu` · `ram` · `batt` · `disk` · `sys` (all four combined) |
| Build | `gcc -shared -O2 -static -o sysmon.dll sysmon.c -lkernel32 -luser32` |
| LOC | ~220 |

**TInfo**

| Field | Value |
|---|---|
| name | `System Monitor` |
| version | `1.0` |
| description | `cpu ram batt disk sys - live status, no admin, no tasks bar needed` |

**What it does.** Prints a one-glance health readout: CPU % since the last
2 s sample (`GetSystemTimes` deltas, refreshed by the plugin's own `WM_TIMER`,
so the very first call reports `warming up` and the second is exact), RAM as
`used/total (load %)` via `GlobalMemoryStatusEx`, battery state/percent via
`GetSystemPowerStatus` (`no battery` on desktops), and free/total GB per fixed
drive via `GetLogicalDrives` + `GetDriveTypeW` + `GetDiskFreeSpaceExW`. `sys`
returns all of it as one text block, copied to the clipboard and shown in a
`MessageBoxW`.

**Config**

| `[Sysmon]` key | Meaning | Default |
|---|---|---|
| `SampleMs` | CPU sampler interval | `2000` |
| `DiskMinGB` | hide drives with more free space than N (0 = show all fixed) | `0` |

**GetCount/GetString**: `5` — `"cpu"`, `"ram"`, `"batt"`, `"disk"`, `"sys"`.

**Example dropdown rows** (typed word -> rows):

- `s` -> `sys`, `sw ...` (other plugins' rows interleave — host de-dupes/filters)
- `sy` -> `sys`
- `d` -> `disk`

**Edge cases / failure behavior.** First `cpu`/`sys` before the first tick ->
`cpu: sampling (ask again in 2 s)`; no battery -> `batt: no battery detected`;
a drive that vanishes mid-walk (USB pulled) -> `GetDiskFreeSpaceExW` fails,
that drive is skipped silently; huge numbers formatted GB with 1 decimal
(`137.2/476.9 GB`), never raw bytes overflow; clipboard/popup failures are
non-fatal.

**Implementation notes.** All APIs are kernel32: `GetSystemTimes` (FILETIME
arithmetic via `ULARGE_INTEGER`), `GlobalMemoryStatusEx` (set `dwLength`!),
`GetSystemPowerStatus`, `GetLogicalDrives`, `GetDriveTypeW`,
`GetDiskFreeSpaceExW`, `GetTickCount64` for uptime in `sys`. Format into one
`static char` buffer with `wsprintfA` (like calc.c) then `MultiByteToWideChar`
for the MessageBox. Message-only window + `SetTimer(SampleMs)` caches the
latest CPU triple.

**Host support:** with the keep-console-visible fix, `sys` output would stay in
the console instead of needing the MessageBox; nothing hard-required. (GPU /
network-rate tiles would need DWM/PSAPI libs we are not allowed to link — out
of scope.)

---

## 9. pwgen — password generator (`pwgen.dll`)

| Field | Value |
|---|---|
| DLL | `Plugins\pwgen.dll` |
| Triggers | `pw [len] [flags]` e.g. `pw 16`, `pw 20 n`, `pw 8 nsp` |
| Build | `gcc -shared -O2 -static -o pwgen.dll pwgen.c -ladvapi32 -luser32` |
| LOC | ~140 |

**TInfo**

| Field | Value |
|---|---|
| name | `Password Generator` |
| version | `1.0` |
| description | `pw 16 - crypto-random password to clipboard (n=digits s=symbols)` |

**What it does.** Generates a cryptographically random password with
`CryptAcquireContext(CRYPT_VERIFYCONTEXT)` + `CryptGenRandom` (advapi32) and
places it on the clipboard; length defaults to 16 (min 4, max 128). Flag
letters tune the charset: default = letters+digits, `n` = digits only, `s` =
add symbols, `p` = pronounceable-ish CV- alternation. The generated password
is shown once in a modal `MessageBoxW` for the user to eyeball — and the
typed line should not linger in history (see the `[PluginState]` note).

**Config**

| `[Pwgen]` key | Meaning | Default |
|---|---|---|
| `Len` | default length | `16` |
| `Symbols` | symbol alphabet | `` !@#$%^&*()-_=+[]{}?`` (ASCII set) |
| `Popup` | `0` = clipboard only | `1` |

**GetCount/GetString**: `1` — `"pw"`.

**Example dropdown rows** (typed word -> rows) — **deliberately templates
only, never a real password**:

- `p` -> `pw 8`, `pw 12`, `pw 16`, `pw 24`
- `pw ` -> `pw 16`

**Edge cases / failure behavior.** `pw 3` -> `pw: length 4..128`; `pw 999` ->
capped to 128 with a warning row; unknown flag letters -> usage;
`CryptGenRandom` failure (ancient OS / broken CSP) -> `pw: RNG unavailable`
and the clipboard is left untouched (never emit a weak fallback password
silently); clipboard failure -> still show the popup so the user at least sees
the password.

**Implementation notes.** Rejection sampling (`rand % N` bias avoided):
generate 1 byte, accept when `< (256/N)*N`. Charset built once from INI.
Copy-once: after showing the popup, optionally `SetClipboardData(CF_TEXT,...)`.
Suggest documenting in CONFIG.md: `[PluginState] pwgen_SaveHistory=0` so
`pw ...` lines stay out of the history file (the line itself only contains
length/flags — still, less is more).

**Host support:** none required (history suppression is already a host feature
via `[PluginState]`).

---

## 10. nettools — weather, public & local IP, DNS (`nettools.dll`)

| Field | Value |
|---|---|
| DLL | `Plugins\nettools.dll` |
| Triggers | `weather <city>` (alias `w <city>`) · `ip` (public + local) · `dns <host>` |
| Build | `gcc -shared -O2 -static -o nettools.dll nettools.c -lws2_32 -luser32 -lshell32` |
| LOC | ~320 |

**TInfo**

| Field | Value |
|---|---|
| name | `Network Tools` |
| version | `1.0` |
| description | `weather <city> / ip / dns <host> - plain-HTTP one-liners, 2 s budget` |

**What it does.** Three tiny network lookups sharing one hand-rolled
blocking-with-timeout HTTP helper over `ws2_32`: `weather Paris` fetches
`http://wttr.in/Paris?format=3` (plain HTTP, `User-Agent: curl/8` so wttr
returns a one-line text body) and shows e.g. `Paris: +8C, cloudy`; `ip`
queries `http://checkip.amazonaws.com/` for the public IPv4 and lists local
addresses via `getaddrinfo` on the hostname; `dns example.com` resolves and
lists all `A`/`AAAA` records. Results go to the clipboard and a popup; the
whole command is budgeted at ~2.5 s worst case so the console never freezes.

**Config**

| `[NetTools]` key | Meaning | Default |
|---|---|---|
| `TimeoutMs` | connect+recv budget per attempt | `1500` |
| `WeatherURL` | custom endpoint template (`%s` = encoded city) | `http://wttr.in/%s?format=3` |
| `CacheSec` | reuse `ip`/`weather` answer within N seconds | `60` |

**GetCount/GetString**: `4` — `"weather"`, `"w"`, `"ip"`, `"dns"`.

**Example dropdown rows** (typed word -> rows):

- `wea` -> `weather ` , `weather Kyiv`
- `w` -> `w `, `w Kyiv`, `win ...` (other plugin rows interleave; `w` row wins prefix)
- `d` -> `dns ` , `dns example.com`

**Edge cases / failure behavior.** Offline / blocked -> `weather: no route to
host (offline?)`, `dns: lookup failed for 'x'` — always inside the timeout,
never a UI freeze (non-blocking `connect` + `select` with `TimeoutMs`;
`SO_RCVTIMEO` for the read). HTTP status != 200 -> `weather: server said 404`;
short/oversized body (cap 4 KB) -> `weather: unexpected reply`; city with
spaces URL-encoded (`New%20York`); `dns` of a literal IP echoes it back
(getaddrinfo handles that); IPv6-only network -> `ip` prints the IPv6
public address if the v4 endpoint is unreachable (second attempt to
`http://checkipv6.amazonaws.com/`, same helper). Responses are UTF-8 ->
`MultiByteToWideChar(CP_UTF8, ...)` before the popup (wttr can include emoji).

**Implementation notes.** `Load`: `WSAStartup(0x202, ...)`; `Unload`:
`WSACleanup` (refcounted, safe even if Load never ran). Helper:
`http_get(const char* host, const char* path, char* buf, int cap)` —
`getaddrinfo(host, "80")` loop over results, non-blocking connect + `select`
(write, TimeoutMs), send `GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent:
curl/8\r\nConnection: close\r\n\r\n`, read till close/4 KB cap, split headers
at the blank line; on failure return code, no retry loop longer than two
addresses. **Plain HTTP only**: TLS is not in the allowed library set (see the
gaps table) — do not point `WeatherURL` at an https endpoint unless the host
toolchain gate later blesses `wininet.lib`/`winhttp.lib`. Because ET can be
called every keystroke, no network from ET rows — the city rows are static
examples + the last-used city cached in a `static`.

**Host support:** HTTPS needs the host build policy to allow
`-lwininet`/`-lwinhttp` (system libs, but currently outside the set) — mark
this plugin **"HTTPS: needs host toolchain decision"**; everything works
today over plain HTTP. Keep-console-visible fix would let `ip`/`dns` output
stay in the console.

---

## Backlog ideas (not specced here)

Shortlist for a future gallery revision, each validated as ABI-feasible but
either bigger or dependent on a host decision:

- **recycle.dll** — `rb list/empty` via `SHQueryRecycleBinW` +
  `SHEmptyRecycleBinW` (shell32); ~120 LOC, needs care: no per-item restore
  without shell COM (`ole32` - not allowed).
- **off.dll** — scheduled shutdown: `off 30` -> `WinExec("shutdown /s /t 1800")`,
  `off cancel` -> `/a` (kernel32); ~110 LOC; `sleep` needs `rundll32
  powrprof.dll,SetSuspendState` dance (no powrprof link).
- **find.dll** — file search with `FindFirstFileW` walk of INI-listed roots,
  budgeted walk + cached index; needs full-line ET to complete arguments well.
- **re.dll** — regex tester with a ~150-line backtracking engine
  (`. * + ? [] ^ $ \d`); risky to get right, medium value.
- **todo.dll** — `todo add/done/list` scratchpad in `Plugins\todo.txt`
  (kernel32 file IO + the cliphist row pattern); great fit, ~220 LOC.
- **wifi.dll** — SSID list + saved-key reveal by spawning `netsh wlan
  show profiles` with redirected stdout and parsing ANSI output; CP_ACP
  parsing is brittle for non-Latin SSIDs.

## Top 5 to implement first

| Rank | Plugin | User value | LOC | Risk | Why first |
|---|---|---|---|---|---|
| 1 | `units` | high (daily math/base conversions) | ~260 | very low - pure C, no APIs | proves the gallery pattern; zero failure surface |
| 2 | `codec` | high for devs (paste-decode loop) | ~220 | very low - pure C | pairs with units as the "silent workhorses" |
| 3 | `vol` | high (one-line volume, no tray) | ~120 | low - `SendInput` media keys, no readback | cheapest "wow" per LOC; tests ET rows-as-commands |
| 4 | `cliphist` | very high (the killer launcher feature) | ~260 | medium - clipboard polling | validates timer-window + tray patterns the rest reuse |
| 5 | `taskman` | high (kill stuck app fast) | ~300 | medium - privileges, confirm UX | validates ET cache + two-mode completion UX |

**Suggested v0.14.0 batch (2-3 plugins):** `units` + `codec` + `vol` —
~600 LOC total, zero network code, at most one link flag, no host changes
required, and together they put a visible result inside the console (via the
keep-visible fix) *and* on the clipboard. Next wave (v0.14.x/v0.15):
`cliphist`, `taskman`, `timer`; ship `sysmon`, `winlist`, `pwgen` after;
hold `nettools` until the plain-HTTP policy (or `wininet` decision) is settled.

*All specs above were verified against `src/main.cpp` (plugin loader, ET feed,
drop-down filter/replace, `[Exec] Plugins`, `[PluginState]`) and follow the
idioms of `examples/plugin_calc/calc.c`. ABI reference:
[docs/PLUGIN_API.md](PLUGIN_API.md).*
