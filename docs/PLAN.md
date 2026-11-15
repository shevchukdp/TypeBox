# TypeBox — plan / roadmap

## Concept
Universal hotkey tool for Windows: **hotkey → type → execute** (launcher + console/bash),
with plugins, optional info display, **INI settings, portability, import/export**.

Rooted in **TypeAndRun** (galancev/typeandrun, GPL-3.0) — its command/alias engine and
INI schema are continued here. UX modeled on Microsoft PowerToys Run / Flow Launcher /
Wox / Raycast; console/bash mode on drop-down terminals (Guake/Yakuake/Drop);
portable INI config on ConEmu.

## Phase 1 — MVP (current)
1. Hotkey registration (`RegisterHotKey`, modifiers + key) ✅
2. Single-line console popup (top-most, centered, edit control) ✅
3. Execution engine: aliases (`%s`, `%1..%n`, env vars), internal commands, shell ✅ (scaffold)
4. INI read/write next to the exe; portable folder ✅
5. History / undo / sounds ⏳

## Phase 2 — Plugins
- Native DLL ABI from TypeAndRun: `Load(HWND)`, `Unload`, `GetInfo`, `GetCount`,
  `GetString`, `GetCountET`, `GetStringET`, `RunString → TExec` (see PLUGIN_API.md).
- Plugin aliases participate in EasyType completion.

## Phase 3 — Info display
- PowerToys Run-style modules: calculator, web search results, files, system status
  (reinterpreting legacy `tar_math` / `tar_system` / `tar_clip` plugins).

## Phase 4 — Console / bash mode
- Drop-down terminal (ConPTY) or embedded shell session, Guake/Yakuake/Drop style.

## Stack
- Primary: **C++** (Win32; Qt optional) — keeps plugin ABI and INI/portability natural.
- Alternative: Rust (Tauri / egui, `#[repr(C)]` plugins), C# (.NET, GUI-fast, plugin ABI harder).

## Differentiators vs modern launchers
| | TypeBox | PowerToys Run | Flow Launcher |
|---|---|---|---|
| INI config, portable | ✅ | ❌ (JSON in `%LOCALAPPDATA%`) | ❌ |
| Import/export settings | ✅ | partial | ❌ |
| Aliases with params/env | ✅ | ❌ | ❌ |
| Plugin ecosystem | ✅ (native ABI) | ✅ | ✅ |
