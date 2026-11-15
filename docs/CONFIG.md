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

### `[Color]` (scheme 0 — console)
COLORREF integers (`0x00BBGGRR` as decimal); `-1` = system default:
`ConsoleBG`, `ConsoleFG`. (Hint/DropDown colors and schemes 1–2 land with EasyType popups.)

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
