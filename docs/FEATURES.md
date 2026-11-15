# TypeBox — feature parity checklist

Goal: **everything TypeAndRun had — and more.**
Sources: `Settings.pas`, `ForAll.pas`, `Static.pas`, `Plugins.pas`, `TypeAndRun.ini`
of the ancestor project [galancev/typeandrun](https://github.com/galancev/typeandrun)
(GPL-3.0). Status: ✅ implemented · 🚧 in progress · 📋 planned (roadmap Phase 2–4).

## Roots & inspiration
- **Root:** TypeAndRun (Evgeniy Galantsev, 2001–2004) — command/alias engine, INI schema,
  plugin ABI, EasyType, history, sounds, tray. TypeBox is its direct successor.
- **Inspiration:** Microsoft PowerToys Run (quick-launch UX), Flow Launcher (plugin ecosystem),
  Wox (GPL ancestor of Run), Raycast / Alfred (rich info display), ConEmu (portable INI),
  Guake / Yakuake / Drop (drop-down console/bash mode).

## Console & Show  `[Show]`
| TypeAndRun option | TypeBox |
|---|---|
| ShowConsoleOnStart, HideConsole (hide on Enter), TriggerShow (toggle by hotkey) | 📋 |
| CenterScreenX / CenterScreenY, ConsoleX / ConsoleY | 🚧 (center Y done) |
| ConsoleWidth (+Percent), ConsoleHeight | 🚧 (width; height via dropdown) |
| Transparent (per-pixel alpha), BorderSize | 📋 |
| DestroyForm (recreate on show to avoid focus bugs) | 📋 |
| TrayIcon, BalloonTips | 📋 |
| AutoHide + AutoHideValue (seconds) | 📋 |
| Priority (process class of TypeBox itself) | 📋 |
| PutMouseIntoConsole | 📋 |
| Auto-hide on deactivate | 🚧 |

## Hotkey  `[HotKey]`
| Option | Status |
|---|---|
| Key, AltKey, CtrlKey, ShiftKey, WinKey (RegisterHotKey) | ✅ |
| RereadHotKey (reload hotkey before each show) | 📋 |
| Ignores.ini — do not show over listed apps (BadApp) | 📋 |

## Font  `[Font]`
Name, Size, AutoSize, Bold, Italic, StrikeOut, UnderLine, Align (L/C/R),
ApplyEnglish (force EN layout on input), LockEnglish. → 📋 (scaffold uses fixed Courier).

## EasyType / completion  `[Type]`
EasyType modes: Off / Easy / Linux-style; completion sources: Internal, Alias, SysAlias,
Path, Plugins, History; CaseSensitivity; Space; EnableET; AdvancedBackspace (word-wise);
AdvancedSpace; DeleteDuplicates; colored console per mode. → 📋 (history ↑/↓ 🚧)

## Execution  `[Exec]`
Internal commands · Aliases · SystemAliases · WWW · EMail · Folders · Files · Plugins ·
Shell (cmd /c and /k) · DefaultAction (runas etc.). → 🚧 core chain works (aliases/URL/mail/path/shell)

## Aliases (`Config.ini` → TypeBox `[Aliases]`)
- Fields: `alias | action | param | dir | state | priori` (+ comment)
- `%s` and `%1..%n` parameter substitution — ✅
- `%ENVVAR%` and `%TARDIR%`/`%TYPEBOXDIR%` expansion — ✅
- working dir, window state, process priority, runas/default action — 📋
- MoveTop on use, AutoSort, editor UI — 📋
- Shift+Enter kills the alias process (KillTask via Toolhelp32) — 📋
- Dead-alias check (CheckDeadAliases/ReturnDeadAliases, backup file, confirmations) — 📋

## System aliases
Name/Action/Path tables from config (system commands like `~lock`, `~logoff`, `~cdeject`) — 📋

## History  `[History]`
Per-type save toggles (Internal/Alias/SysAlias/WWW/EMail/Path/Plugins/Unknown/CommandLine/Message
+ params variants), MaxHistory, InvertScroll — 📋 (linear history 🚧)

## Undo
`[Undo] UseUndo` — command-line undo with caret restore — 📋

## Paths  `[Path]`
Browser, FileManager, Shell, ShellClose (`/c %s`), ShellNoClose (`/k %s`) — 🚧 (shell done)

## Hint & DropDown popups  `[Hint]` `[DropDown]`
ShowHint live-completion popup; DropDown LineNum; Transparent; BorderSize; DestroyForm — 📋

## Color themes  `[Color]`
3 schemes (No-ET / EasyType / Linux) × {console bg/fg/border, hint bg/fg/border, dropdown bg/fg/border} — 📋

## Sounds  `[Sounds]`
EnableSounds + 9 per-execution-type sounds (internal/alias/sysalias/url/email/folder/file/plugin/shell)
+ bundled WAV folder — 📋

## Language  `[Language]`
External `.lng` string tables, `.chm` help, 10 shipped translations — 📋

## Registry integration  `[Registry]`
AutoRun (HKLM/HKCU), "Add to context menu" (ShellInt), MultiUser — 📋
(⚠ TypeBox keeps ALL portable settings in INI; these opt-in entries are explicit-only.)

## Single instance & IPC
Semaphore single-instance; second instance forwards command via WM_COPYDATA; command-line
params (`ParseCmdLine`) to prefill/execute — 📋

## Internal commands (all 14)
`/about /checkconfig /config /exit /help /hide /homedir /inidir /inifiles /load /reread /settings /show /unload` — 🚧 (4 of 14 in scaffold)

## Plugins (ABI-compatible with TypeAndRun)
`Load(HWND) Unload GetInfo GetCount GetString GetCountET GetStringET RunString→TExec`;
`Plugins.ini` per-plugin: `Path`, `Load`, `SaveHistory`; load/unload at runtime — 📋
See [PLUGIN_API.md](PLUGIN_API.md).

## --- AND MORE (beyond TypeAndRun) ---
| Addition | Why |
|---|---|
| **INI + portable folder + import/export** | PowerToys Run keeps JSON in `%LOCALAPPDATA%`; TypeBox config moves with you |
| Migration from legacy `TypeAndRun.ini` / `Config.ini` | zero-friction upgrade path |
| Unicode (UTF-8) config & input | TypeAndRun was ANSI/OEM |
| Fuzzy search across aliases+apps (PowerToys Run-style) | modern launcher UX |
| Info display modules: calculator, web results, files, system status (like PowerToys Run / Raycast) | the "панель" part of the idea |
| Drop-down terminal / bash mode (ConPTY) — Guake/Yakuake/Drop style | the "консоль/баш" part |
| DPI-aware, top-most, per-monitor positioning | 2004-era VCL app didn't care |
| Optional plugin manifest + sandboxed plugins (modern ABI v2) | safety without breaking v1 |
| Profiles (multiple INI sets, switchable) | beyond anything in the original |
