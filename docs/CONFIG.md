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

### `[Type]` (EasyType completion)
| Key | Meaning | Default |
|---|---|---|
| `EasyType` | `0` off · `1` easy · `2` linux-style (Tab completes, repeat Tab cycles) | `2` |
| `UseInternalET` / `UseAliasET` / `UseHistoryET` | completion sources | `1` |

### `[Color]` (scheme 0 — console)
COLORREF integers (`0x00BBGGRR` as decimal); `-1` = system default:
`ConsoleBG`, `ConsoleFG`. (Hint/DropDown colors and schemes 1–2 land with EasyType popups.)

### `[Font]`
`Name` (default `Courier New`), `Size` (`18`), `Bold` (`1`).

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

### `[Sounds]`
`EnableSounds=1` plus per-execution-type files (TypeAndRun parity):
`ExecuteInternalSound`, `ExecuteAliasSound`, `ExecuteSysAliasSound`, `ExecuteURLSound`,
`ExecuteEmailSound`, `ExecuteFileSound`, `ExecuteFolderSound`, `ExecutePluginSound`,
`ExecuteShellSound`. Relative paths resolve against the portable `Sounds\` folder;
`none` disables one sound.

### Other sections
`[Shell] Command / Args` (default `cmd.exe` `/c`), `[Config]`, `[Hint]`,
`[DropDown]`, `[Color]` — inherited from the legacy schema (see `TypeAndRun.ini`
in galancev/typeandrun) and activated as features land.

## Migration from TypeAndRun
| Legacy | TypeBox |
|---|---|
| `TypeAndRun.ini [HotKey]` | `TypeBox.ini [HotKey]` |
| `Config.ini` alias lines `alias\|action\|...` | `[Aliases] alias = action` |
| `%TARDIR%` | `%TYPEBOXDIR%` |
