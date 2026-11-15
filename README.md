# TypeBox

**Universal hotkey console + bash + launcher for Windows.** Press a shortcut, type, hit Enter — TypeBox runs what you mean: apps, aliases, commands, URLs, files, plugins.

One **portable folder** with an **INI file** you can carry on a USB stick, keep in Git, or **export/import** between machines — the feature modern launchers forgot.

> ### Roots & inspiration
> **TypeBox grows out of [TypeAndRun](https://github.com/galancev/typeandrun)** by Evgeniy Galantsev (2001–2004, GPL-3.0) — the **direct ancestor** whose idea, command/alias engine and INI config schema this project continues.
> The UX and feature set draw inspiration from:
> **[Microsoft PowerToys Run](https://learn.microsoft.com/windows/powertoys/run)** (quick-launch UX), **[Flow Launcher](https://www.flowlauncher.com/)** (open plugin ecosystem), **[Wox](https://github.com/Wox-launcher/Wox)** (GPL-3.0 ancestor of PowerToys Run), **[Raycast](https://www.raycast.com/)** (rich display, extensions), **[Alfred](https://www.alfredapp.com/)** / **Quicksilver** (historical launchers), **[ConEmu](https://conemu.github.io/)** (proof that a portable INI config works for Windows tools), and drop-down terminals **[Guake](https://github.com/Guake/guake)** / **[Yakuake](https://kde.org/applications/utilities/org.kde.yakuake)** / **[Drop](https://github.com/marcinwicherek/drop)** (the console/bash mode UX).

> **Корни и вдохновение.** TypeBox вырос из **TypeAndRun** (Evgeniy Galantsev, [galancev/typeandrun](https://github.com/galancev/typeandrun), GPL-3.0): идея, движок команд/алиасов и INI-схема конфига продолжаются напрямую из него. UX и набор фич вдохновлены Microsoft PowerToys Run, Flow Launcher, Wox, Raycast, Alfred, а режим консоли/bash — drop-down терминалами Guake / Yakuake / Drop. Портативный INI-конфиг — по образцу ConEmu.

## Why TypeBox

| | **TypeBox** | TypeAndRun | PowerToys Run | Flow Launcher |
|---|---|---|---|---|
| Hotkey → type → run | ✅ | ✅ | ✅ | ✅ |
| Aliases with `%s`/`%1..%n` + env vars | ✅ | ✅ | ❌ | ❌ |
| Config format | **INI** | INI | JSON (`%LOCALAPPDATA%`) | JSON |
| Portable (folder moves with config) | ✅ | ✅ | ❌ | ❌ |
| Settings import / export | ✅ | manual | partial (JSON, since 0.64) | ❌ |
| Plugins | ✅ (planned, native DLL ABI of TypeAndRun) | ✅ | ✅ | ✅ |
| Console / bash mode | ✅ (planned) | partly | ❌ | ❌ |

TypeBox keeps the soul of TypeAndRun (aliases, `%s`, `%TYPEBOXDIR%`, history, undo, sounds, INI) and takes the quick-launch UX from PowerToys Run / Flow Launcher / Raycast.

## Features

MVP (Phase 1, scaffold in `src/`):
- ✅ Global hotkey (`[HotKey] Key / AltKey / CtrlKey / ShiftKey / WinKey`)
- ✅ Console-like popup (single-line edit, top-most, center screen)
- ✅ Command engine: aliases with `%s` / `%1..%n`, env vars (`%VAR%`, `%TYPEBOXDIR%`), URLs, `mailto:`, files/folders, shell (`cmd.exe /c`)
- ✅ Internal commands (`/help`, `/hide`, `/config`, `/exit`)
- ✅ INI config next to the exe — portable
- 🚧 History navigation (↑/↓)

Planned (see [docs/PLAN.md](docs/PLAN.md)):
- Plugin ABI from TypeAndRun (`Load/Unload/GetInfo/GetCount/GetString/RunString` → `TExec`) — [docs/PLUGIN_API.md](docs/PLUGIN_API.md)
- Info display (calculator, web results, files, system status — like PowerToys Run modules)
- Drop-down terminal / bash mode (Guake/Yakuake/Drop style)
- Import/export settings (copy the INI, plus a UI action), migration from old `TypeAndRun.ini`
- Sounds, undo, tray icon, balloon hints, per-theme colors

## Everything TypeAndRun had — and more

The goal is **full feature parity with the ancestor, plus modern additions**. The complete
checklist — every option of `Settings.pas`/`ForAll.pas` (`[Show] [HotKey] [Font] [Type] [Exec]
[History] [Path] [Registry] [Language] [Undo] [Sounds] [Config] [Coordinates] [Hint] [DropDown] [Color]`,
system aliases, EasyType completion, 14 internal commands, plugin ABI, single-instance IPC)
with status and the "beyond TypeAndRun" list (portable INI, import/export, Unicode, fuzzy search,
info modules, ConPTY terminal mode, profiles) — is in **[docs/FEATURES.md](docs/FEATURES.md)**.

## Docs

- [docs/FEATURES.md](docs/FEATURES.md) — parity checklist vs TypeAndRun + additions
- [docs/PLAN.md](docs/PLAN.md) — roadmap (MVP → plugins → info → bash/terminal)
- [docs/CONFIG.md](docs/CONFIG.md) — INI schema, portability, import/export, migration
- [docs/PLUGIN_API.md](docs/PLUGIN_API.md) — TypeAndRun-compatible plugin ABI

## Config & portability

All settings live in **`TypeBox.ini`** next to the executable — nothing in the registry, nothing in `%APPDATA%`. Copy the folder → your setup moves with you. "Import/export" = copy/swap the file (a UI action will wrap this).

The schema is compatible with the legacy `TypeAndRun.ini` (sections `[HotKey]`, `[Show]`, `[Exec]`, `[History]`, `[Sounds]`, `[Config]`, `[Hint]`, `[DropDown]`, `[Color]`) so old configs can be imported. See [docs/CONFIG.md](docs/CONFIG.md).

## Building

MSVC:
```
cl /EHsc /std:c++17 /DUNICODE /D_UNICODE src\main.cpp /Fe:TypeBox.exe ^
   /link user32.lib shell32.lib shlwapi.lib gdi32.lib comctl32.lib
```
CMake:
```
cmake -S . -B build && cmake --build build --config Release
```

## License

GPL-3.0 — the same license as the ancestor project, TypeAndRun. See [LICENSE](LICENSE).
