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
`Files`, `Shell`.

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
| `Priority` | process class: `idle` · `normal` · `high` · `realtime` | `normal` |

### `[Type]` (EasyType completion)
| Key | Meaning | Default |
|---|---|---|
| `EasyType` | `0` off · `1` easy · `2` linux-style (Tab completes, repeat Tab cycles) | `2` |
| `UseInternalET` / `UseAliasET` / `UseHistoryET` / `UseSysAliasET` | completion sources | `1` |
| `Fuzzy` | `1` subsequence scoring — `cl` finds `calc` (Tab + drop-down, hits accented); `0` = TypeAndRun prefix mode (TypeBox 0.11.0) | `1` |

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

Settings → **EasyType** page: *Show suggestions drop-down*, *Visible rows*, *History when the
line is empty* (log at start: `dropdown: on (8 rows)`). When the candidate list is longer than
`Lines`, **↑/↓** and the **mouse wheel** scroll it; rows carry `1…9` and **Alt+1…9** runs one
instantly. Hovering with the mouse highlights a row but **Enter** still runs the typed line —
only a keyboard selection or click runs.

### `[WebSearch]` — quicklinks (TypeBox 0.12.0)
Raycast-style: a keyword + query opens the default browser. Built-in keywords **g** (Google),
**yt** (YouTube), **gh** (GitHub) ship in the exe; rows here add or override by name. `%s` is
replaced by the URL-encoded rest of the line (no `%s` ⇒ the query is appended). A quicklink run is
logged (`websearch: g -> …`), recorded as EXC_URL, and previews amber in the anchor;
`[Exec] WWW=0` blocks it.

| Key | Meaning | Default |
|---|---|---|
| `Enable` | register quicklinks at all | `1` |
| `g`,`yt`,`gh`,… | `name=https://…%s` | (3 built-ins) |

Example: `so=https://stackoverflow.com/search?q=%s` makes `so hello world` open a search.

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
- `/export <path>` — saves `TypeBox.ini` to `<path>` (take it anywhere).
- `/import <path>` — restores it (current config kept as `TypeBox.ini.bak`),
  then reloads settings, aliases and hotkey.

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
queries plugins after aliases; `/load` rescans, `/unload` calls their `Unload`.
`[Exec] Plugins=0` disables the whole stage. See
[PLUGIN_API.md](PLUGIN_API.md) and [`examples/plugin_calc`](../examples/plugin_calc/calc.c).

**Bundled since v0.14.0** (specs: [PLUGIN_GALLERY.md](PLUGIN_GALLERY.md)):

| Plugin | Commands | Own settings |
|---|---|---|
| `units` | `conv 10 km mi` · `hex 255` → `FF` · `bin 0xFF` · `dec 0xFF` · `oct 777` | `[Units] Popup=1 CopyResult=1` |
| `codec` | `b64enc/b64dec` · `urlenc/urldec` · `hexenc/hexdec` | `[Codec] Popup=1 CopyResult=1` |
| `vol` | `vol +` · `vol -` · `vol mute` · `vol 3` | `[Vol] Step=1 Beeps=0` |

`Popup=0` = no MessageBox, the answer goes to the console + clipboard only;
`CopyResult=0` = nothing lands on the clipboard; `[Vol] Step` = media-key taps per
`+`/`-` (1..5), `Beeps=1` confirms with a beep. Answers stay selected in the console
so Ctrl+C copies the formatted line; the clipboard gets the raw value.

### `[PluginState]` — per-plugin switches (TypeBox extension)
Written by the **Plugins** page of the settings dialog, read at startup. Keys are the
dll base name (no extension):
`<name>_Load=1|0` — whether `Load()` runs at start and commands are routed to the
plugin; `<name>_SaveHistory=1|0` — whether its commands go to the history.
Example: `calc_Load=0` keeps `calc.dll` unloaded until re-enabled. The section is
rewritten on every OK/Apply (like `[Aliases]`), so keep manual notes elsewhere.

### `[Sounds]`
`EnableSounds=1` plus per-execution-type files (TypeAndRun parity):
`ExecuteInternalSound`, `ExecuteAliasSound`, `ExecuteSysAliasSound`, `ExecuteURLSound`,
`ExecuteEmailSound`, `ExecuteFileSound`, `ExecuteFolderSound`, `ExecutePluginSound`,
`ExecuteShellSound`. Relative paths resolve against the portable `Sounds\` folder;
`none` disables one sound.

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

### `[Theme]` — cozy color schemes (TypeBox 0.9.1)
```ini
[Theme]
Name = cozy-dark   ; cozy-dark | paper-light | midnight-blue | nord-frost
                   ; | solarized-dark | solarized-light | forest-night
```
A preset paints the console, the status strip and the tray badge in one go
(the accent tints the badge rim and the chips). **Theme** page of the settings
dialog has a live preview; **Apply** re-skins live (log `theme: …`). Explicit
`[Color] ConsoleBG/ConsoleFG` values always override the preset.

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
