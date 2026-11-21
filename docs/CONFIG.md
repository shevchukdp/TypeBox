# TypeBox configuration

All settings live in **`TypeBox.ini`** located next to `TypeBox.exe` (portable).
Nothing is written to the registry or `%APPDATA%`.

## Portability & import/export
- **Portable**: copy the whole folder → runs as-is; config travels with the program.
- **Export** = copy `TypeBox.ini`. **Import** = drop a `TypeBox.ini` next to the exe.
  A "Save to… / Load from…" action will wrap this later.
- **Migration**: the schema is compatible with legacy `TypeAndRun.ini` — copy the file,
  rename, done (see migration map below).

## Sections

### `[HotKey]`
| Key | Meaning | Example |
|---|---|---|
| `Key` | virtual-key code of the hotkey | `113` (F2) |
| `AltKey` / `CtrlKey` / `ShiftKey` / `WinKey` | `0`/`1` modifiers | `CtrlKey=1` |

**Unrestricted hotkeys (v0.13).** Any key is allowed: single keys with no modifiers (`Key=81`
for plain **Q**), media keys (`VolUp`, `PlayPause`), `Esc`, F13–F24, numpad and OEM symbols.
TypeBox registers with `RegisterHotKey` first (adds `MOD_NOREPEAT`, so holding the key does not
auto-repeat the toggle). When that API refuses — a bare single key, another app already owns the
combo, or an extended key it cannot express — TypeBox transparently falls back to a
**low-level keyboard hook** that matches the exact modifier state. The hook consumes the key only
while the console is hidden (so a single-letter hotkey does not leak into the app behind and does
not eat your typing inside the console); while the console is open the keys pass through untouched.
The startup log shows which mode is active: `hotkey: … [RegisterHotKey]` or `… [low-level hook]`.

### `[Aliases]`
```ini
[Aliases]
notepad = notepad.exe %s
gh      = https://github.com/search?q=%s
cmd     = cmd.exe /k %s
; legacy pipe form: action|param|dir|state|priori|comment
myide   = "C:\tools\ide.exe"|"%1"|C:\tools|3||open in my IDE
```
- First token of the typed line selects the alias; the rest is available as `%s` and
  `%1..%n` (same semantics as TypeAndRun).
- `VAR` expansion: `%WINDIR%`, `%TYPEBOXDIR%` (folder of the exe), any env var.
- A legacy `Config.ini` next to the exe is also read (`[Aliases]`, pipe form) —
  drop it in to import old TypeAndRun alias tables.
- `state`: SW_ number (`1` normal, `3` maximized, `2` minimized).

### `[Exec]`
Toggles (`0`/`1`) mirroring TypeAndRun: `Internal`, `Aliases`, `WWW`, `EMail`,
`Files`, `Shell`, plus `Scripts` (v0.16) — whether the portable `Scripts\`
folder works as a command set.

### `Scripts\` folder — scripts are commands (TypeBox 0.16.0)

Any `*.bat` · `*.cmd` · `*.ps1` · `*.py` · `*.vbs` · `*.vbe` dropped into the
portable `Scripts\` folder next to the exe becomes a command by its file name:

```
Scripts\hello.bat        ->  type: hello [any arguments]
Scripts\backup.ps1       ->  type: backup C:\Data
```

Arguments are passed through (`cmd /c` for bat/cmd, `powershell -File` for ps1,
`py` for python, shell association for vbs). The traffic light shows a yellow
**script** chip before Enter. Re-scan: `/reread` or Apply in settings
(startup logs `scripts: N registered`).

### `[Security]` — the safe Run box (TypeBox 0.16.0, danger-confirm 0.17.0, su 0.18.0)
| Key | Meaning | Default |
|---|---|---|
| `ConfirmSuspicious` | `1` = confirm before running lines that look like encoded payloads/fileless one-liners (`powershell -enc`, base64 blobs, `certutil`/`bitsadmin`/`mshta`, curl-pipe-shell, …) — the ClickFix lesson | `1` |
| `ConfirmDanger` | `1` = confirm destructive commands (`del`/`erase`/`rd`/`format`/`diskpart`/`shutdown`/`reg delete`/`net user`/`vssadmin delete`/`cipher /w`/`rm -r`/`Remove-Item -Recurse -Force`/`dd if=`/…) — token-based, so paths like `C:\del\report.pdf` stay quiet | `1` |
| `Elevate` | `1` = `su …` really requests UAC elevation (`runas`). `0` runs su-prefixed lines normally — the automation/CI switch (no interactive consent prompt) | `1` |

Both tripwires cover the shell catch-all, script arguments **and** alias expansions
(an alias that expands to `del …` is just as dangerous). Answer **No** and nothing
runs; every decision is logged (`security: blocked/confirmed … command`).

### `su` — run as administrator (TypeBox 0.18.0)

Type `su` (or `/su`, `sudo`) before anything else you would type:

```
su taskmgr              ->  elevated Task Manager (UAC prompt)
/su regedit             ->  elevated Registry Editor
sudo netstat -an        ->  elevated  cmd /c netstat -an
su backup               ->  your elevated Scripts\backup.bat
su sutest               ->  your alias, launched with runas
```

The prefix is stripped, the rest of the line runs through the **exact same
stages** as without it (alias → plugin → script → file → shell), only the final
launch uses the ShellExecute `runas` verb — so aliases, `Scripts\` scripts,
files and shell commands can all be elevated, while the status strip shows an
**admin** chip while you type. URLs, e-mails and web quicklinks stay at user
level (runas on a mailto handler is meaningless). Bare `su` answers with a usage
hint instead of launching anything. History keeps the typed line (re-runnable);
frecency/`Usage.dat` records the real command word, never `su`. Every elevation
is logged (`elevate: <line>`); cancelling the UAC prompt simply fails that one
launch (`GetLastError=1223` in the log) and TypeBox keeps running. Set
`[Security] Elevate=0` (or uncheck it on the Execution settings page) to make
su-prefixed lines run without UAC — intended for headless automation.

### `/update` — check for a new version & install helper (TypeBox 0.20.0)

Type `/update` and TypeBox asks the release API **once** — that one command is
the only place the host ever talks to the network, and it never happens unless
you type it (offline-first rule). What you get:

```
/update            ->  "You are running the latest version (0.20.0)" - or -
                        "TypeBox 0.20.1 is available (you are on 0.20.0, 970 KB).
                         Download and prepare the update?"  [Yes/No]
```

On **Yes** the release zip is downloaded to `%TEMP%\TypeBox-update\`, its
**SHA-256 is verified against the digest published with the release** before
anything is unpacked (mismatch = canceled, never "install anyway"); a release
without a published digest asks you explicitly. The verified zip is unpacked to
a staging folder and `install.bat` is prepared. With the default
`[Update] AutoRun=1` you get one last "Restart and install?" — **Yes** and the
host exits, the helper waits for the process to vanish, copies the staged tree
over the app folder and relaunches TypeBox. Your `TypeBox.ini` is **never**
overwritten: `.ini` files are excluded from the copy.

| `[Update]` key | Meaning | Default |
|---|---|---|
| `ApiUrl` | release API to query (mirror/self-host override; plain `http://` is accepted **only** for `127.0.0.1`/`localhost`) | official GitHub Releases API |
| `AutoRun` | `1` = offer "restart & install" and run `install.bat`; `0` = only stage the files and show the folder | `1` |
| `CheckOnStart` | `1` = silent background check at startup; a newer release becomes a tray balloon, nothing else | `0` |
| `Latest` / `LastCheck` | cache of the last known newest tag and check time (shown when the check fails offline) | — |

Every step is logged (`/update: <tag> available…`, `digest verified`, `staged`,
`installing via install.bat`), so a support request can be answered from the log
alone.

### `pos` + `stayawake` — positioning & caffeine (TypeBox 0.23.0)

Two host-side utility words (they run before aliases, obey `[Exec] Internal=0`):

- `pos left|right|top|bottom|tl|tr|bl|br|center|q1..q4` — sizes/places the console into a
  slot of the **current** monitor's work area (taskbar-aware), FancyZones vocabulary.
  `center` keeps the current width/height; `q1..q4` are fourths of the row.
- `pos <name>` — a named preset from the portable `[Positions]` section:

  ```ini
  [Positions]
  mycorner = 10,20,300,120          ; x,y,w,h relative to the work area
  rightmon = 80,160,900,200,2       ; + monitor index (0-based, wraps)
  ```

- The spot lives **in memory only** and belongs to the slot itself: since 0.27.1 a `pos`
  call never inflates the `[Show]` size — hide/show re-applies the slot's own rect, a
  manual drag releases the active slot (your window management wins), and
  `pos reset` returns to the `[Show]` home explicitly. A restart always returns to
  `[Show]`/`X`/`Y` (your INI is never rewritten).
- Under `TYPEBOX_HEADLESS=1` the same slot math runs, but the rect is applied 32000px
  off both axes — automated suites verify geometry without ever surfacing a window
  on the human's screen.
- `stayawake [min]` — keeps display+system awake (`SetThreadExecutionState`), bare = indefinite,
  `1..1440` minutes rides a timer and clears itself, `stayawake off` clears now. No process or
  service is started; the state dies with TypeBox, so an exit is always a clean `off`.

Both answer in the console (`pos: right -> 960,0 960x1032`, `stayawake: on for 30 min`) and
log to `/log`; `pos` and `stayawake` are EasyType candidates like every other command.

### `in` + `at` — scheduled commands (TypeBox 0.34.0)

The time machine (ROADMAP #15). Two reserved words arm one-shot timers that run the very line
you would have typed, from a 1 s timer tick:

```
in 10m cmd /c copy log.zip \\nas\backups      ; 90s / 10m / 2h / 1d — a bare number means minutes
in 30 tea.bat                                  ; (30 minutes; the cap is 30 days)
at 18:30 backup.bat                            ; wall clock, 24h; a past time means TOMORROW
```

- Timers persist in `Timers.dat` next to the exe (TAB + UTF-8, same dialect as `Usage.dat`)
  and their numbers are never reused. A timer whose moment passed while TypeBox was closed is
  **missed, not run**: at boot it is logged (`scheduled: missed #77 …`) and scrubbed — the
  machine never wakes up to a blast of old commands.
- When a timer fires the human gets a tray balloon (`scheduled: fired #12: …`) and the log
  keeps the receipt; under `TYPEBOX_HEADLESS=1` nothing executes — the log answers
  `would run`. A dangerous line (the `/path` wiper tripwire) is skipped at fire time —
  even in a dry run: `skipped #N … (dangerous - run it yourself)`.
- `/timers` lists who is armed (`#12 in 7m (23:41) cmd …`, six rows plus an ellipsis),
  `/cancel <number>` disarms one, `/cancel all` clears the deck. Internal commands refuse to
  be scheduled (`in 10m /exit` — no), and so do other timers (no `at 07:00 in 5m calc`).
- `in` and `at` are reserved words: `snippet add` / `search add` refuse those names.

### Do Not Disturb — `/dnd` and `[Sounds] Quiet` (TypeBox 0.34.0)

`/dnd [min]` shushes the execution sounds and every tray balloon (startup, `/sys`, update
notice): bare = indefinite, `1..1440` minutes self-expires, `/dnd` again prints the status,
`/dnd off` ends it. `[Sounds] Quiet=22:00-07:30` arms the same silence nightly — a window
that wraps midnight is fine. The log still tells the truth: shushed sound paths answer
`sounds: suppressed (quiet)`. The tray menu carries the same switch as a checkable
**Do Not Disturb** item (right-click the tray icon). From v0.37 the menu also grew a checkable
**Fuzzy matching** toggle (`[Type] Fuzzy`) and a **Next theme** item (`[Theme] Name`) - both
persist to the INI and repaint the console immediately, with an open drop-down rescanned on the
next keystroke.

### Dashboards, favorites & autostart (TypeBox 0.35.0)

- **`/stats`** — one console line: the last 14 days as block-glyph sparkline
  (`▁▂▄▆█`), your top five commands with counts, and the plugin table. Everything
  comes from `Usage.dat` — the wave added no new store except the tiny favorites
  file, and it never touches the network.
- **`/plugdebug`** — every plugin, its hits and average milliseconds since the
  start; a plugin that faults is marked `FAULT` and skipped for the session
  (the host survives it), the log keeps `plugin: <dll> FAULTED in RunString`.
  Since v0.36 each entry also carries the ET round-trips and the drop-down
  cache: `<key> ok <hits>/<avg>ms et <calls>/<avg>ms c<hits>` (the console
  shows the first six, the log line keeps the whole roster).
- **`/reload <plugin>`** (v0.36) — hot-reload one plugin in place: guarded
  `Unload`, `FreeLibrary`, fresh `LoadLibrary`, manifest re-read, guarded
  `Load`, counters zeroed, ET caches dropped. Same via the **Reload** button
  on Settings > Plugins. Log: `plugin: reloaded <key> (Nms)`.
- **`/export history <file.csv> [redact]`** — BOM'd CSV (`seq,command` header,
  quoted cells, doubled `""` quotes) that Excel opens correctly. `redact` keeps
  every command word but drops every argument — safe for a screenshot. The old
  bare `/export <file>` INI copy behaves exactly as before.
- **`/import history <file.csv>`** — merges exported rows back: exact duplicates
  (case-insensitive) are skipped, the `[History] Max` cap is honored, nothing
  else is rewritten.
- **Favorites** — `Ctrl+S` stars/unstars the highlighted drop-down row, or the
  whole line when nothing is highlighted, into `Favorites.dat` (UTF-8, one line
  per favorite, cap 512, portable next to the exe). Type `favs ` and the
  drop-down becomes the starred board; with at least one star the empty console
  opens as a board with `* favorites` / `* history` section headers — arrows,
  Alt+digits and clicks never stop on a header, and without favorites the board
  is byte-for-byte the old list. `favs` / `favs <text>` list them, `favs clear`
  empties the file.
- **`autostart on|off|status`** — adds/removes the `TypeBox` value under
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` (quoted exe path). The
  offline-first law covers the registry: under `TYPEBOX_HEADLESS` nothing is
  ever written — the answer is `would write` / `would delete`; `status` is a
  read and always answers truthfully.
- **`Data\<dll>\`** — the host pre-creates this folder next to the exe for every
  enabled plugin: the portable, writable home plugin authors should use instead
  of inventing `%APPDATA%` paths.

### `TypeBox.local.ini` — per-machine overrides (TypeBox 0.17.0)

Drop an INI named `TypeBox.local.ini` next to `TypeBox.ini` and it wins on every
key it contains — same sections, same keys. The classic use: a team-maintained
portable `TypeBox.ini` on a shared folder + a personal `TypeBox.local.ini` that
overrides position, theme or language on *this* machine:

```ini
; TypeBox.local.ini — only the keys you care about
[Show]
Width = 900
[UI]
Language = uk
```

The settings dialog keeps writing the main `TypeBox.ini` (defaults stay shared);
startup logs `local config: on (TypeBox.local.ini)`. Note: plugins read
`TypeBox.ini` directly, so plugin sections are not overridable by the local file.

### `[Show]`
| Key | Meaning | Default |
|---|---|---|
| `Width` / `Height` | popup size | `720` / `40` |
| `X` / `Y` | fixed position (`-1` = auto) | `-1` |
| `CenterScreenX` / `CenterScreenY` | centering | `1` / `1` |
| `HideOnDeactivate` | hide when focus lost | `1` |
| `ShowConsoleOnStart` | show popup at launch | `1` |
| `TrayIcon` / `BalloonTips` | tray presence / notifications | `1` / `1` |
| `Transparent` | window alpha `0..255` | `255` |
| `AutoHide` / `AutoHideValue` | auto-hide after N idle seconds | `0` / `30` |
| `DragMove` | `1` = press-and-drag the 20 px status strip to move the console; the spot is remembered for the session, the INI position is restored on every restart | `1` |
| `Priority` | process class: `idle` · `normal` · `high` · `realtime` | `normal` |
| `DropIn` | (0.37) `1` = files dragged onto the console insert quoted paths at the caret; `0` logs `drop-in: ignored` and never touches the line | `1` |

**Every pixel value in the INI is 96-DPI LOGICAL (since 0.29.0).** TypeBox runs
Per-Monitor V2 aware (Win10 1703+; Win8.1 → Per-Monitor; Win7/8 → system aware), and each
window scales the numbers it reads: `[Show] Width/Height`, `[Positions]` presets,
`[Font] Size`, the 20 px status strip, the 640×460 settings dialog, drop-down rows and
plugin windows all go through `MulDiv(v, windowDpi, 96)` at show time. So `Width=720`
means "720 logical px" — on a 150% display the console is physically 1080 px wide and
text stays sharp. You never have to multiply anything by your scale factor.

### `/checkconfig` — config audit (TypeBox 0.32.0)

`/checkconfig` reports what actually loaded - into the console (one line, selectable/copyable)
*and* the classic dialog: ini file, alias and sysalias counts, ignores, history, hotkey
registration. The v2 audit adds real findings: an alias whose target is found nowhere (as a
file, with `.exe`, in PATH over `.exe/.bat/.cmd/.com`, as a URL, or via a trusted shell host
like `cmd`/`powershell`) is flagged `DEAD: name -> target` with a one-line fix - `(maybe
'closest-word')` from the pool of alias names and plugin words - and every finding is logged
(`checkconfig: dead alias "name"`). Duplicate alias names are counted in the header.

### `[Type]` (EasyType completion)
| Key | Meaning | Default |
|---|---|---|
| `EasyType` | `0` off · `1` easy · `2` linux-style (Tab completes, repeat Tab cycles) | `2` |
| `UseInternalET` / `UseAliasET` / `UseHistoryET` / `UseSysAliasET` | completion sources | `1` |
| `Fuzzy` | `1` subsequence scoring — `cl` finds `calc` (Tab + drop-down, hits accented); `0` = TypeAndRun prefix mode (TypeBox 0.11.0) | `1` |
| `LayoutFallback` | `1` (TypeBox 0.32.0) — the word is also matched in the **other keyboard layout**: with ЙЦУКЕН on, the physical `l a z` keys print `дфя` and the alias `laz` is still found, Tab-completed and executed (both directions, physical-key map, `~`-marked rows, real hits win ties; Enter accepts only an exact other-layout hit); `0` disables | `1` |
| `FrecencyRanking` | `1` (TypeBox 0.17.0) — drop-down ties are won by the commands you use most: `Usage.dat` next to the exe keeps `word · count · last-used` per command; delete the file to reset the ranking | `1` |
| `Ghost` | `1` (TypeBox 0.33.0) — **ghost text**: when exactly one row continues the whole typed line, its tail paints dimmed after the caret; **End** (or **Right** at line end) accepts it, more typing narrows it. Uniqueness is the contract — an ambiguous prefix shows nothing; `~` layout and `≈` spell rows never ghost. `0` disables | `1` |
| `ArgMemory` | `1` (TypeBox 0.33.0) — **remembered arguments**: the words of an alias/snippet run stick per slot (`Usage.dat` keys `name#1..9`, field 6), so `ff x` fills `%2` from the last time; `0` disables capture and recall | `1` |

### `[DropDown]` — suggestions list (TypeBox 0.10.0, TypeAndRun DropDown parity)
A themed list snaps under the console (flips above at the screen edge): candidates for the word
at the caret (aliases, `~sysaliases`, `/commands`, plugin commands, history) — or the history
when the line is empty. **↑/↓** select · **Tab** accept · **Enter** run the row · first **Esc**
closes the list · mouse hover + click work too. Colors follow `[Theme]` (the old per-popup
DropDownColor trio is replaced by the scheme).

| Key | Meaning | Default |
|---|---|---|
| `Enable` | show the drop-down at all | `1` |
| `Lines` | visible rows (1..20) before the rest is clipped | `8` |
| `ShowHistoryOnEmpty` | empty line lists the history (TypeAndRun behaviour) | `1` |
| `DragOut` | (0.37) `1` = press-and-move on a row that *is* an existing file starts a real OLE drag (`CF_HDROP` + `CF_UNICODETEXT`), so the file lands in Explorer/chat/editors; `0` keeps rows click-only | `1` |

Settings → **EasyType** page: *Show suggestions drop-down*, *Visible rows*, *History when the
line is empty* (log at start: `dropdown: on (8 rows)`). When the candidate list is longer than
`Lines`, **↑/↓** and the **mouse wheel** scroll it; rows carry `1…9` and **Alt+1…9** runs one
instantly. Hovering with the mouse highlights a row but **Enter** still runs the typed line —
only a keyboard selection or click runs. Two v0.32 marks: a row ending in **`~`** matched only
through the *other keyboard layout* (see `LayoutFallback`), and rows starting with **`≈`** are
"did you mean" suggestions (≤2 edits) that appear when nothing else matched — picking one
writes the real word. From v0.33 the unique continuation also paints dimmed after the caret
(**ghost text**, `[Type] Ghost`) — **End**/**Right** accepts it without touching the list.
From v0.37 a row that *is* an existing file drags out: press and move over it and Explorer,
a chat window or an editor receive a genuine file drop (`CF_HDROP`, with the path as
`CF_UNICODETEXT` fallback). Headless test instances never enter the OLE drag loop - the log
answers `would drag: <path>` and nothing runs (the offline-first law covers drags too).

### `[Snippets]` — text expansion (TypeBox 0.33.0)
A row is `name = text`; typing `name [words]` (no other command claimed the word) expands the
text through the same engine as alias actions — `%s` = the typed remainder, `%1…%9` = the words,
empty `%n` falls back to the remembered one (see `[Type] ArgMemory`) — and pastes it into the
window that was behind the console (clipboard + one synthetic Ctrl+V). Headless test mode never
touches the clipboard or injects keys: the log answers `snippet: would paste N chars`.
Rows also complete in ET and can be managed from the line: `snippet add <name> <text with %s>`,
`snippet list`, `snippet del <name>` (names `[A-Za-z0-9_-]{1,32}`, collisions with aliases,
plugin words and reserved words are refused). Settings → **Snippets** edits the same rows.

| Key | Meaning | Default |
|---|---|---|
| `name` | `name=text with %s and %1..%9` | (none) |

Example: `sig=Best regards, %s` makes `sig John Doe` type itself into your mail draft.

### `[WebSearch]` — quicklinks & search engines (TypeBox 0.12.0, Lite since 0.33.0)
Raycast-style: a keyword + query opens the default browser. Built-in keywords **g** (Google),
**yt** (YouTube), **gh** (GitHub) ship in the exe; rows here add or override by name. `%s` is
replaced by the URL-encoded rest of the line (no `%s` ⇒ the query is appended). A quicklink run is
logged (`websearch: g -> …`), recorded as EXC_URL, and previews amber in the anchor;
`[Exec] WWW=0` blocks it.

**Search Engines Lite (v0.33)** — every row is a first-class engine with three invoke forms:
`g query` · browser-style `g:query` · and with `Bang=1` the DDG form `!g query`. Manage them
without touching the INI: `search add <name> <url-with-%s>` · `search list` · `search del <name>`
· `search test <name> <query>` (assembles the URL into the line — only Enter opens it, and under
headless `[Exec] WWW=0` it stays a preview). Names are `[A-Za-z0-9_-]{1,32}` and refuse to
collide with aliases, plugin words or the reserved words; `%s` is present-or-appended. The
Settings → **Search engines** page edits the same rows (the g/yt/gh defaults always come back —
`Enable=0` turns the family off). `search import <opensearch.xml>` is planned, not shipped.

| Key | Meaning | Default |
|---|---|---|
| `Enable` | register quicklinks at all | `1` |
| `Bang` | `1` (v0.33) — also accept the `!name query` form | `0` |
| `g`,`yt`,`gh`,… | `name=https://…%s` | (3 built-ins) |

Example: `so=https://stackoverflow.com/search?q=%s` makes `so hello world` (or `so:hello world`,
or `!so hello world` with `Bang=1`) open a search.

### `[Color]` (scheme 0 — console)
COLORREF integers (`0x00BBGGRR` as decimal); `-1` = system default:
`ConsoleBG`, `ConsoleFG`. (The drop-down and status strip follow `[Theme]`, see above.)

### `[Font]`
`Name` (default `Courier New`), `Size` (`18`), `Bold` (`1`),
`ApplyEnglish=1` — switch the keyboard layout to US-English while the console opens
(TypeAndRun parity, avoids typing commands in the wrong layout).

### `[History]` / `[Undo]`
`[History] Max=200` plus per-type save toggles (TypeAndRun parity):
`Internal`, `Alias`, `WWW`, `EMail`, `Path`, `Plugins`, `Unknown` (`0`/`1`).
History persists to portable **`History.dat`** (UTF-8, one command per line) next to the
exe. `[Undo] UseUndo=1` → `Ctrl+Z` or `/undo` restores the last executed command.

### Import / export
Internal commands wrap the portable file:
- `/export <path>` - saves `TypeBox.ini` to `<path>` (take it anywhere).
- `/import <path>` - restores it (current config kept as `TypeBox.ini.bak`),
  then reloads settings, aliases and hotkey.
- `/export history <file.csv> [redact]` / `/import history <file.csv>` -
  command history as CSV, see **Dashboards, favorites & autostart** above.

### `[Log]` — portable error log
Everything TypeBox cannot do itself (failed hotkey registration, plugin load errors,
failing aliases, import/export errors) is appended to a log **next to the exe** —
no Event Viewer, no `%LOCALAPPDATA%`. Open it with the internal `/log` command.

| Key | Meaning | Default |
|---|---|---|
| `Enabled` | master switch | `1` |
| `File` | log file (relative paths resolve to the portable folder) | `TypeBox.log` |
| `MaxSizeKB` | size cap before the rewrite method kicks in | `512` |
| `Rewrite` | `rotate` (file→file.1→file.N) · `truncate` (keep newest half) · `timestamp` (seal as `file-YYYYMMDD-HHMMSS.log`) · `keep` (stop writing, never touch) | `rotate` |
| `Keep` | rotated copies retained (`rotate`) | `3` |

### `[Plugins]`
```ini
[Plugins]
Calculator = Plugins\calc.dll
```
All `*.dll` in the portable `Plugins\` folder are picked up automatically;
`[Plugins]` entries add extra paths (relative to the exe). The execution chain
queries plugins after aliases; `/load` rescans, `/unload` calls their `Unload`,
`/reload <plugin>` swaps one dll in place (v0.36). Since v0.36 a dll may carry
a sidecar `<dllname>.tbplugin.json` (`words`, `author`, `hostmin`, `version`,
`description`): the host enforces the declared words against host commands and
other plugins and refuses to load below `hostmin`; no manifest = loads as
before. `[Exec] Plugins=0` disables the whole stage. See
[PLUGIN_API.md](PLUGIN_API.md), [../AGENTS.md](../AGENTS.md) (agent-authored
plugins) and [`examples/plugin_skeleton`](../examples/plugin_skeleton/skeleton.c)
(the reference plugin; [`examples/plugin_calc`](../examples/plugin_calc/calc.c)
is the classic minimal one).

**Bundled since v0.14.0** (specs: [PLUGIN_GALLERY.md](PLUGIN_GALLERY.md)):

| Plugin | Commands | Own settings |
|---|---|---|
| `units` | `conv 10 km mi` · `hex 255` → `FF` · `bin 0xFF` · `dec 0xFF` · `oct 777` | `[Units] Popup=1 CopyResult=1` |
| `codec` | `b64enc/b64dec` · `urlenc/urldec` · `hexenc/hexdec` | `[Codec] Popup=1 CopyResult=1` |
| `vol` | `vol +` · `vol -` · `vol mute` · `vol 3` | `[Vol] Step=1 Beeps=0` |
| `cliphist` | `clip` (list, `*` = pinned) · `clip <n>` re-copy · `clip pin/unpin <n>` · `clip paste <n>` (hide + Ctrl+V at caret) · `clip type <n>` (per-char SendInput, for apps that eat Ctrl+V) · `clip ring` (topmost picker; also the global `[Clipboard] Ring` hotkey) · `clip clear` | `[Clipboard] Max=20 PollMs=500 RowChars=48 Popup=1 Persist=0 Balloons=0 PasteOnSelect=0 TypeDelayMs=5 Ring=Ctrl+Alt+V` |
| `taskman` | `task <substr>` (list) · `kill <name|pid>` · `top [n|filter]` (CPU ranking) · `relaunch <name|pid>` (v0.31: was `restart` — that word belongs to `off`) | `[Taskman] Confirm=1 ListMax=300 TopN=8` |
| `winlist` | `win <title>` activate · `win` (list) · `win <n>` (pick last filter) | `[Winlist] AltTrick=1 Titles=9` |
| `pwgen` | `pw` · `pw 20` · `pw 20 3` · `pw 16 snp` (letters+digits / `n` digits / `s` symbols / `p` pronounceable) — first result copied to clipboard | `[Pwgen] Len=16 Symbols=!@#$%^&*()-_=+[]{}? Popup=0` |
| `timer` | `timer 5m tea` countdown with tray balloon · `timer` (list) · `timer stop [label]` (no label = soonest) · `sw` start/report/stop the stopwatch · `sw reset` | `[Timer] MiniWindow=1 Sound=1 Max=8` |
| `sysmon` | `cpu` % (delta of two samples, never a stale 0) · `ram` used/total (load %) · `batt` state+percent · `disk` free/total per fixed drive · `sys` all-in-one line, auto-copied to clipboard | `[Sysmon] SampleMs=2000 DiskMinGB=0 Popup=0` |
| `util` | `ts` now (epoch s/ms + ISO local/UTC) · `ts 1763385000` · `ts 2026-11-16T08:00+03:00` · `ts clip` · `slug Привет мир` → `privet-mir` (trailing `snake`/`pascal`/`camel`/`upper`, result copied) · `sha256 <file>` (digest + clipboard) · `sha256 /verify SUMS [base]` (GNU + BSD formats, `sha256sum -c` rules) | — |
| `note` | `note` (capture editor, clipboard prefilled, Ctrl+Enter saves) · `note <text>` (one-shot `YYYY-MM-DD-HHmm-title.md`) · `note + <text>` (`- [HH:mm] text` bullet in today's journal `YYYY-MM-DD.md`) · `note list` (today's files) · `note open` (folder in Explorer) | `[Notes] Folder=` (default `<exe>\Notes`), `Prefill=1` |
| `yt` | `yt <query>` (top videos in the console: `1) title (12:34) channel`) · `yt <n>` (opens result n in the browser) · `yt /clear` (drop the cache) — **the network is touched only on a typed `yt` line**: Data API v3 when a key is set, else public Piped/Invidious instances, else the results page opens in the browser. While the plugin is enabled it shadows the built-in `yt` quicklink; tick it off in Settings → Plugins to give the quicklink back | `[Youtube] ApiKey=` (optional, Data API v3), `Top=7` (3..12), `TimeoutMs=6000`, `Instances=` (comma-separated base URLs) |
| `weather` | `weather <city>` — one line: now (° + condition + feels + wind) plus **today and tomorrow** highs/lows · `weather` (cached place; else `[Weather] City`; else IP geolocation) · `weather /clear` (forget the cache) — **the network is touched only on a typed `weather` line** (keyless Open-Meteo forecast + geocoding, ipwho.is as last-resort geolocator; unreachable network answers honestly, never hangs) | `[Weather] City=` (default place), `Units=m` (\|`im` for °F/mph), `TimeoutMs=6000` |
| `nettools` (v0.30) | `ip` (public IP, cached) · `w <city>` (one short wttr.in line, last city remembered; the rich `weather` command stays with the `weather` plugin — v0.30.1 word split) · `dns <host>` (addresses; works offline for localhost/hosts names) · `netinfo` (adapter name + IPv4/mask/gateway) · `ping <host> [n]` (ICMP replies with min/avg/max ms) — **network only on typed lines**, every request capped by `TimeoutMs` | `[NetTools] TimeoutMs=3000` (800..15000), `CacheSec=60`, `PingCount=4` (1..8), `WeatherURL=https://wttr.in/%s?format=3`, `IpUrl=https://checkip.amazonaws.com/`, `LastCity=` (auto-written after a successful `w`) |
| `off` (v0.30) | `off` / `off <min>` / `off now` schedule a shutdown · `off cancel` · `restart [min]` reboot · `off sleep` · `off hibernate` (answers `enable with powercfg /h on` when disabled in the system) · `off hybrid` · `off logoff` — with `TYPEBOX_HEADLESS=1` (or `[UI] Headless=1`) every action is a dry-run `off: [headless] would run ...`, never a real power call | `[Off] Confirm=1` (same line twice within…), `WarnSec=60` (confirm window 5..300), `DefaultMin=30` |
| `todo` (v0.30) | `todo <text> [when]` (trailing `today` · `tomorrow` · `mon..sun` · `+Nd` · `+Nh` · `HH:MM` · `YYYY-MM-DD`, combinable: `todo call mom tomorrow 18:00`) · `todo` list (`!!` overdue, `!` due <24 h) · `todo today` · `todo done <n>` (archives to DoneFile) · `todo del <n>` | `[Todo] File=todo.txt` (relative to the exe), `DoneFile=todo-done.txt`, `MaxRows=20` (5..100), `DueWarn=1` |
| `json` (v0.30) | `json fmt <text>` pretty (2-space) · `json min <text>` · `json get a.b[2].c <text>` (string values raw) · `json keys <text>` · `jwt <token>` / `jwt decode <token>` (base64url header+payload, `iat`/`exp` in UTC with `expired` verdict) — pure computation, zero network | — |
| `wol` (v0.31) | `wol <MAC> [host [port]]` sends the magic packet (6×FF + 16×MAC, UDP broadcast) · `wol <name> [host [port]]` a named preset from `[Wol]` · `wol list` shows the configured names — MAC accepts `AA:BB:CC:DD:EE:FF`, `AA-BB-...`, `AA.BB.CC.DD.EE.FF` or `AABBCCDDEEFF`; per-row `host [port]` override the defaults; with `TYPEBOX_HEADLESS=1` (or `[UI] Headless=1`) **never** puts a packet on the wire — answers `wol: [headless] would send magic packet to ... via h:p (N packets)` | `[Wol] Host=255.255.255.255`, `Port=9`, `Repeat=3` (1..5); named rows `pc = AA:BB:CC:DD:EE:FF [host [port]]` |

`Popup=0` = no MessageBox, the answer goes to the console + clipboard only;
`CopyResult=0` = nothing lands on the clipboard; `[Vol] Step` = media-key taps per
`+`/`-` (1..5), `Beeps=1` confirms with a beep. Answers stay selected in the console
so Ctrl+C copies the formatted line; the clipboard gets the raw value.

### `[PluginState]` — per-plugin switches (TypeBox extension)
Written by the **Plugins** page of the settings dialog, read at startup. Since
v0.25 the page is a checklist: every discovered plugin is listed with a tick
box - click a row to enable/disable it (the *Load* checkbox mirrors the selected
row). Keys are the dll base name (no extension):
`<name>_Load=1|0` — whether `Load()` runs at start and commands are routed to the
plugin; `<name>_SaveHistory=1|0` — whether its commands go to the history.
Example: `calc_Load=0` keeps `calc.dll` unloaded until re-enabled. The section is
rewritten on every OK/Apply (like `[Aliases]`), so keep manual notes elsewhere.

### `[Sounds]`
`EnableSounds=1` plus per-execution-type files (TypeAndRun parity):
`ExecuteInternalSound`, `ExecuteAliasSound`, `ExecuteSysAliasSound`, `ExecuteURLSound`,
`ExecuteEmailSound`, `ExecuteFileSound`, `ExecuteFolderSound`, `ExecutePluginSound`,
`ExecuteShellSound`. Relative paths resolve against the portable `Sounds\` folder;
`none` disables one sound. `Quiet=22:00-07:30` (TypeBox 0.34.0) is the nightly do-not-disturb
window — sounds and tray balloons stay silent inside it (a window that wraps midnight is
fine); see **Do Not Disturb** above and the `/dnd` command for manual silence.

### `[Path]` + `[Variables]` — own environment (TypeBox 0.8.0)
Like the Windows *Environment Variables* dialog, but fully portable (lives in
TypeBox.ini, survives moves; edit via Settings → **Environment** or `/path` to inspect):
```ini
[Path]
Dir1 = C:\tools              ; one folder per key...
Extra = D:\bin;E:\scripts    ; ...or semicolons in "Extra"
Dir2 = %TYPEBOXDIR%\bin      ; %VARS% are expanded ([Variables] first)

[Variables]
MYDOCS = %USERPROFILE%\Documents   ; %MYDOCS% works in aliases, sysaliases, typed text
```
Typed words matching an executable (`exe|bat|cmd|com`) in a `[Path]` folder launch
directly — the process stays killable with Shift+Enter (log: `path started: name from dir`).
The folders are also prepended to the process PATH, so aliases, `cmd /c` shell lines
and every child program find your tools.

### `[UI]` — interface language (TypeBox 0.9.0)
```ini
[UI]
Language = auto   ; auto | en ru uk de fr es pt it zh ja ko ar tr pl vi hi id sw af am
```
`auto` detects the Windows UI language. The **Language** page of the settings dialog
writes this key; Apply switches the whole UI immediately (Arabic also mirrors the
drop-down RTL). Portable community overrides live in `lang\<code>.ini` next to the
exe — see [LANGUAGE.md](LANGUAGE.md).

`Headless = 1` turns on the **headless test mode** (TypeBox 0.27.0), so automated
suites can never disturb a human at the keyboard. Any one switch is enough:
`[UI] Headless=1` in the INI, the `--headless-tests` command line, or the
`TYPEBOX_HEADLESS=1` environment variable (what the test suites use — the host
passes nothing, plugins and probe children simply inherit it). Effects:
the console, Settings and the clip ring open off-screen at (-32000,-32000);
no `SetForegroundWindow`/`SetFocus` ever happens; sounds, tray balloons and
modal dialogs are muted (suppressed dialogs are written to the log instead);
the global hotkey is not registered and no low-level keyboard hook is installed
(probes post `WM_HOTKEY` to the window instead); `/tabs` and the `yt`/`note`
plugins open URLs in a minimized inactive window. Everything stays testable through posted window messages —
nothing is faked, only the user-visible disturbance is parked. The companion
probe tool uses `TB_QUIET=1` for the same vow on its side: right-clicks,
Ctrl+Enter and pad clicks are posted as messages, the physical-cursor path and
the “minimise the foreground thief” fallback are disabled.

### `[Theme]` — cozy color schemes + custom themes (TypeBox 0.9.1, studio in 0.15.0)
```ini
[Theme]
Name = cozy-dark   ; cozy-dark | paper-light | midnight-blue | nord-frost
                   ; | solarized-dark | solarized-light | forest-night
                   ; …or the name of your own themes\<name>.theme.ini
```
A preset paints the console, the status strip and the tray badge in one go
(the accent tints the badge rim and the chips). **Theme** page of the settings
dialog has a live preview; **Apply** re-skins live (log `theme: …`). Explicit
`[Color] ConsoleBG/ConsoleFG` values always override the preset.

**Your own themes** (v0.15 theme studio): press **Save as…** on the Theme page or drop a
file into the portable `themes\` folder — one INI per theme, listed in the scheme combo:

`themes\sunset-glow.theme.ini`
```ini
[Theme]
Bg=241028          ; RRGGBB (no 0x needed)
Fg=F2E9FF
Accent=FF7A45
Border=FF7A45      ; optional; unset = the accent
BorderWidth=3      ; 0..24 px frame around the console (built-ins: none)
Alpha=200          ; optional 30..255; pushed to [Show] Transparent when picked
FontName=Consolas  ; optional; pushed to [Font] Name when picked
FontSize=22        ; optional 6..72
FontBold=1         ; optional 0/1
Width=810          ; optional; pushed to [Show] Width when picked
Height=92          ; optional; pushed to [Show] Height when picked
```
Only `Bg/Fg/Accent` are required — the rest stay whatever the console currently is
(shared fields are one-time pushes on pick, persisted as ordinary `[Show]`/`[Font]`).
Built-ins are compiled in: they cannot be deleted, and a custom saving under a built-in
name gets a `-copy` suffix instead of shadowing it. `/reread` rescans `themes\` live.
Delete a custom theme with the **Delete** button (or just delete the file).

### `[SysAliases]` — system commands (TypeAndRun parity)

`~name = action` lines, e.g. `~notes = notepad.exe`. An empty/absent section loads
the built-in table: `~lock` · `~logoff` · `~sleep` · `~screensave` · `~reboot`.
They are Tab-completed (toggle `[Type] UseSysAliasET`), can use `%s`/`%1..%n`,
are gated by `[Exec] SysAlias` (alias toggle) and the last one can be killed with
**Shift+Enter** like a normal alias.

### `Ignores.ini` — stay hidden over apps (BadApp parity)
Separate portable file next to the exe (not in TypeBox.ini): one pattern per line,
process **file name** or **window class**, wildcards `*` `?`, `;`/`#` comments.
While the foreground window matches, the hotkey keeps the console hidden.

### Other sections
`[Shell] Command / Args` (default `cmd.exe` `/c`), `[Config]`, `[Hint]`,
`[DropDown]`, `[Color]` — inherited from the legacy schema (see `TypeAndRun.ini`
in galancev/typeandrun) and activated as features land.

## Settings dialog

`/settings` (or tray → **Settings...**) opens a portable settings window —
12 pages (Appearance, Console, Font, Hotkey, Execution, EasyType, History, Log,
Aliases, Sounds, Plugins, Environment). It edits a scratch copy: **Cancel discards**; **OK/Apply** write
`TypeBox.ini` key-by-key (your comments and unknown keys survive; `[Aliases]`
is rewritten in canonical form) and apply live — hotkey re-registration with a
log entry on conflict, font, geometry, alpha, priority, tray, AutoHide timer.
`/config` opens the alias editor page directly; `/openconfig` opens the raw file.
The dialog position is remembered in `[Coordinates] SettingsX/SettingsY`.
Everything remains pure INI — hand-editing the file and `/reread` still works.

## Migration from TypeAndRun
| Legacy | TypeBox |
|---|---|
| `TypeAndRun.ini [HotKey]` | `TypeBox.ini [HotKey]` |
| `Config.ini` alias lines `alias\|action\|...` | `[Aliases] alias = action` |
| `%TARDIR%` | `%TYPEBOXDIR%` |
