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
```
- First token of the typed line selects the alias; the rest is available as `%s` and
  `%1..%n` (same semantics as TypeAndRun).
- `VAR` expansion: `%WINDIR%`, `%TYPEBOXDIR%` (folder of the exe), any env var.

### `[Exec]`
Toggles (`0`/`1`) mirroring TypeAndRun: `Internal`, `Aliases`, `WWW`, `EMail`,
`Files`, `Shell`.

### Other sections
`[Show]` (position/size/top-most), `[History] Max`, `[Sounds]`, `[Config]`, `[Hint]`,
`[DropDown]`, `[Color]` — inherited from the legacy schema (see `TypeAndRun.ini`
in galancev/typeandrun) and activated as features land.

## Migration from TypeAndRun
| Legacy | TypeBox |
|---|---|
| `TypeAndRun.ini [HotKey]` | `TypeBox.ini [HotKey]` |
| `Config.ini` alias lines `alias\|action\|...` | `[Aliases] alias = action` |
| `%TARDIR%` | `%TYPEBOXDIR%` |
