# Changelog

All notable changes to TypeBox are documented here.

## [0.1.0] - 2026-11-15

Project start. Docs, plan, config schema and an early Phase-1 scaffold.

### Added
- README with concept, positioning table vs. TypeAndRun / PowerToys Run / Flow Launcher,
  and the **roots & inspiration** section (TypeAndRun as the root ancestor;
  PowerToys Run, Flow Launcher, Wox, Raycast, Alfred, ConEmu, Guake/Yakuake/Drop as inspirations).
- `docs/PLAN.md` — roadmap (MVP → plugins → info modules → bash/terminal mode).
- `docs/CONFIG.md` — `TypeBox.ini` schema, portable layout, import/export semantics,
  migration map from legacy `TypeAndRun.ini`.
- `docs/PLUGIN_API.md` — plugin ABI carried over from TypeAndRun
  (`Load/Unload/GetInfo/GetCount/GetString/RunString` → `TExec`).
- `docs/FEATURES.md` — **full parity checklist** with TypeAndRun
  (every settings group, system aliases, EasyType, 14 internal commands, plugin ABI,
  single-instance IPC) **and the "beyond TypeAndRun" list** (portable INI, import/export,
  Unicode, fuzzy search, info modules, ConPTY terminal mode, profiles).
- `src/main.cpp` — Win32 C++ scaffold: global hotkey, console popup, command executor
  (aliases with `%s`/`%1..%n`, env vars, URLs, mailto, files/folders, shell mode).
- `CMakeLists.txt` and MSVC build command.
- `TypeBox.ini` — portable sample configuration (next to the exe).
- `LICENSE` — GPL-3.0 (same as TypeAndRun).
- `CHANGELOG.md`, `.gitignore`.

### Known limitations (tracked in docs/PLAN.md)
- Tray icon, plugin loader, sounds, undo, info display, terminal mode: not yet implemented.
- Import/export is "copy the INI" for now; a UI action is planned.
