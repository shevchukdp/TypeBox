# TypeBox

[![build](https://github.com/shevchukdp/TypeBox/actions/workflows/build.yml/badge.svg)](https://github.com/shevchukdp/TypeBox/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/shevchukdp/TypeBox?include_prereleases)](https://github.com/shevchukdp/TypeBox/releases)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

**Universal hotkey console + bash + launcher for Windows.** Press a shortcut, type, hit Enter — TypeBox runs what you mean: apps, aliases, commands, URLs, files, plugins.

One **portable folder** with an **INI file** you can carry on a USB stick, keep in Git, or **export/import** between machines — the feature modern launchers forgot.

**Download:** [latest release](https://github.com/shevchukdp/TypeBox/releases) — unzip, run `TypeBox.exe`, hotkey `Ctrl+Shift+F2`.

## Screenshots

| The console (hotkey → type → Enter) | Drop-down suggestions (v0.10) | Settings → Environment: own PATH & variables |
|---|---|---|
| ![console](docs/screenshots/console.png) | ![dd](docs/screenshots/dropdown.png) | ![env](docs/screenshots/settings-environment.png) |

| Appearance | Font | Hotkey capture |
|---|---|---|
| ![a](docs/screenshots/settings-appearance.png) | ![f](docs/screenshots/settings-font.png) | ![h](docs/screenshots/settings-hotkey.png) |

| Alias editor | Plugin manager |
|---|---|
| ![al](docs/screenshots/settings-aliases.png) | ![p](docs/screenshots/settings-plugins.png) |

| Settings → Theme (cozy presets) | Settings → Language (20 UI languages) | The same page, Russian UI |
|---|---|---|
| ![theme](docs/screenshots/settings-theme.png) | ![lang](docs/screenshots/settings-language.png) | ![langru](docs/screenshots/settings-language-ru.png) |

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
| Plugins | ✅ (native DLL ABI compatible with TypeAndRun) | ✅ | ✅ | ✅ |
| Console / bash mode | ✅ (planned) | partly | ❌ | ❌ |

TypeBox keeps the soul of TypeAndRun (aliases, `%s`, `%TYPEBOXDIR%`, history, undo, sounds, INI) and takes the quick-launch UX from PowerToys Run / Flow Launcher / Raycast.

## Features

Working today (v0.11.0):
- ✅ **Fuzzy matching** — `[Type] Fuzzy=1` (default): Tab and the drop-down score subsequences,
  so `cl` finds `calc`; hit characters glow in the accent color, best score on top, `Fuzzy=0`
  restores the strict TypeAndRun prefix mode
- ✅ **Drop-down suggestions** — TypeAndRun's `frmTypeDropDown` reborn, themed by `[Theme]`:
  ET candidates for the word at the caret (aliases · `~sysalias` · `/commands` · plugins) or the
  history on an empty line; ↑/↓ · Tab accept · Enter run · Esc close · mouse click;
  `[DropDown] Enable/Lines/ShowHistoryOnEmpty`, EasyType page controls
- ✅ **Cozy themes + visual anchors** — 7 warm presets (`[Theme]`, Theme page, live
  preview), GDI-drawn tray badge (accent rim, state dot), console status strip
  (counts · chips · clock) and a traffic light previewing Enter: 🟢 your alias/~/cmd,
  🟡 PATH/file/URL/plugin, 🔴 shell catch-all — per keystroke, localized labels
- ✅ **Localization — 20 languages, everywhere** (en ru uk de fr es pt it complete +
  zh ja ko ar tr pl vi hi id sw af am core): tray, balloons, help/about, whole settings
  dialog; `[UI] Language`, Language page, Arabic RTL, CJK/RTL font auto-suggestion,
  portable `lang\<code>.ini` overrides — [docs/LANGUAGE.md](docs/LANGUAGE.md)
- ✅ **Own PATH & variables** — `[Path]` folders + `[Variables]` like the Windows
  *Environment Variables* dialog, but portable: typed names launch from your folders,
  PATH is injected for aliases/shell/children, `%NAME%` expands everywhere — `/path`,
  Settings → Environment
- ✅ **Settings dialog** — 14 pages (Appearance · Console · Font · Hotkey · Execution ·
  EasyType · History · Log · Aliases editor · Sounds · Plugin manager · Environment ·
  Theme · Language), scratch copy + Cancel,
  live-apply on OK/Apply (hotkey re-registration, font, geometry, alpha, priority, tray,
  plugin load state, UI language) — `/settings`, tray menu
- ✅ Global hotkey (`[HotKey] Key / AltKey / CtrlKey / ShiftKey / WinKey`)
- ✅ Console-like popup (top-most, centered, transparent, font from INI, DPI-aware, AutoHide)
- ✅ Command engine: aliases with `%s` / `%1..%n`, env vars (`%VAR%`, `%TYPEBOXDIR%`),
  legacy pipe form `action|dir|state`, URLs, `mailto:`, files/folders, shell (`cmd.exe /c`)
- ✅ **System aliases** (`[SysAliases]`): `~lock · ~logoff · ~sleep · ~screensave · ~reboot`
  defaults, user table overridable; **Shift+Enter** kills the last alias process
- ✅ **Ignores.ini (BadApp)** — stay hidden over listed apps (process/class, wildcards)
- ✅ **All 14 TypeAndRun internal commands** (`/about … /unload`) + `/export` `/import` `/undo` `/log` `/sys`
- ✅ **Plugin host — TypeAndRun ABI-compatible** (`Load/Unload/GetInfo/GetCount/GetString/RunString → TExec`):
  drop DLLs into the portable `Plugins\` folder; example with calculator in
  [`examples/plugin_calc`](examples/plugin_calc/calc.c) — [docs/PLUGIN_API.md](docs/PLUGIN_API.md)
- ✅ **EasyType completion** — `Tab` completes from commands/aliases/sys-aliases/plugins/history, cycles on repeat
- ✅ Tray icon + balloon tips · sounds per execution type · undo (`Ctrl+Z`)
- ✅ Persistent history (`↑/↓`, portable `History.dat`, per-type toggles) · single instance + `WM_COPYDATA` IPC
- ✅ INI config next to the exe — portable

Planned (see [docs/PLAN.md](docs/PLAN.md)):
- EasyType hint/dropdown popups + full 3-scheme colors
- Info display (calculator, web results, files, system status — like PowerToys Run modules)
- Drop-down terminal / bash mode (Guake/Yakuake/Drop style)

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
- [docs/LANGUAGE.md](docs/LANGUAGE.md) — 20 UI languages, RTL, portable translation packs
- [docs/PLUGIN_API.md](docs/PLUGIN_API.md) — TypeAndRun-compatible plugin ABI

## Config & portability

All settings live in **`TypeBox.ini`** next to the executable — nothing in the registry, nothing in `%APPDATA%`. Copy the folder → your setup moves with you. "Import/export" = copy/swap the file (a UI action will wrap this).

The schema is compatible with the legacy `TypeAndRun.ini` (sections `[HotKey]`, `[Show]`, `[Exec]`, `[History]`, `[Sounds]`, `[Config]`, `[Hint]`, `[DropDown]`, `[Color]`) so old configs can be imported. See [docs/CONFIG.md](docs/CONFIG.md).

## Building

CMake (MSVC or MinGW):
```
cmake -S . -B build && cmake --build build --config Release
```
MSVC directly:
```
cl /EHsc /std:c++17 /DUNICODE /D_UNICODE /DTYPEBOX_VERSION=0.0.0-dev src\main.cpp /Fe:TypeBox.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib shlwapi.lib gdi32.lib comctl32.lib winmm.lib
```
MinGW-w64 directly (add `-static` so the exe needs no MinGW runtime DLLs):
```
g++ -std=c++17 -DUNICODE -D_UNICODE -DTYPEBOX_VERSION=0.0.0-dev -O2 src/main.cpp -o TypeBox.exe ^
    -municode -mwindows -static -luser32 -lshell32 -lshlwapi -lgdi32 -lcomctl32 -lwinmm
```
The version comes from the `VERSION` file (CI refuses a tag that doesn't match it).

## License

GPL-3.0 — the same license as the ancestor project, TypeAndRun. See [LICENSE](LICENSE).
