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

## Settings dialog (TypeAndRun `frmSettingsUnit` parity)
| Page | Covers | Status |
|---|---|---|
| Appearance | start/hide/tray/balloons/AutoHide/Priority | ✅ |
| Console | size/position/centering/alpha/colors | ✅ |
| Font | name/size/bold + native Choose-font dialog + ApplyEnglish + preview | ✅ |
| Hotkey | THotKey-style capture widget + VK display | ✅ |
| Execution | 7 exec toggles | ✅ |
| EasyType | mode + 4 completion sources | ✅ |
| History | max + per-type toggles + Clear history | ✅ |
| Log | enabled/file/max KB/rewrite/keep | ✅ |
| Aliases | editor: list + name/action/dir/state, Add/Update/Delete | ✅ |
| Sounds | enable + 9 per-type WAV fields (`fraSounds`) | ✅ |
| Plugins | list + info (GetInfo) + Load-on-start/SaveHistory + Rescan/Unload/About/Options (`fraPlugins`) | ✅ |
| Environment | own PATH folders + %variables% editor (TypeBox addition, OS-style) | ✅ |
| Hint / Registry / dead-alias checker | — | 📋 Phase 2 |
| DropDown suggestions popup | `[DropDown]` | ✅ 0.10.0 |
| Language 20 UI languages + RTL + portable packs | `[UI] Language`, `lang\<code>.ini` | ✅ 0.9.0 |

Scratch-copy editing (Cancel discards), OK/Apply → per-key INI write (comments
survive) → live apply (hotkey re-register, font, geometry, alpha, priority, tray,
timer, brush) — ✅; `[Coordinates]` remembers dialog position — ✅.

## Console & Show  `[Show]`
| TypeAndRun option | TypeBox |
|---|---|
| ShowConsoleOnStart, HideConsole (hide on Enter), TriggerShow (toggle by hotkey) | ✅ |
| CenterScreenX / CenterScreenY, ConsoleX / ConsoleY | ✅ |
| ConsoleWidth (+Percent), ConsoleHeight | ✅ (Percent 📋) |
| Transparent (per-pixel alpha), BorderSize | ✅ transparent · 📋 border |
| DestroyForm (recreate on show to avoid focus bugs) | 📋 |
| TrayIcon, BalloonTips | ✅ (context menu: show · config · exit) |
| AutoHide + AutoHideValue (seconds) | ✅ |
| Priority (process class of TypeBox itself) | ✅ `[Show] Priority=idle\|normal\|high\|realtime` |
| PutMouseIntoConsole | 📋 |
| Auto-hide on deactivate | ✅ |

## Hotkey  `[HotKey]`
| Option | Status |
|---|---|
| Key, AltKey, CtrlKey, ShiftKey, WinKey (RegisterHotKey) | ✅ |
| RereadHotKey (reload hotkey before each show) | 📋 |
| Ignores.ini — do not show over listed apps (BadApp) | ✅ process name / window class, wildcards |

## Font  `[Font]`
Name, Size, Bold — ✅. ApplyEnglish (force EN layout on input) — ✅ `[Font] ApplyEnglish`.
Italic, StrikeOut, UnderLine, Align (L/C/R), AutoSize,
LockEnglish. → 📋

## EasyType / completion  `[Type]`
EasyType modes: Off / Easy / Linux-style — ✅ `Tab` completes the word at the caret from
internal commands + aliases + history + sys aliases (`UseSysAliasET`), repeated `Tab` cycles matches
(`[Type] EasyType`, `UseInternalET/UseAliasET/UseHistoryET`); AdvancedBackspace (word-wise ✅).
Fuzzy matching (`[Type] Fuzzy=1`, TypeBox 0.11.0): Tab and the drop-down rank subsequence hits
(prefix > word-start > consecutive); `Fuzzy=0` keeps the strict prefix behaviour.
CaseSensitivity; AdvancedSpace; DeleteDuplicates; colored hint per mode → 📋 (popup, Phase 2)

## Execution  `[Exec]`
Internal commands · Aliases · SystemAliases · WWW · EMail · Folders · Files · Plugins ·
Shell (cmd /c and /k) · DefaultAction (runas etc.). → ✅ for Internal/Aliases/SysAliases/
WWW/EMail/Files/Shell/Plugins with `[Exec]` toggles · 📋 DefaultAction

## Aliases (`Config.ini` → TypeBox `[Aliases]`)
- Fields: `alias | action | param | dir | state | priori` (+ comment) — ✅ (dir, state applied)
- `%s` and `%1..%n` parameter substitution — ✅
- `%ENVVAR%` and `%TARDIR%`/`%TYPEBOXDIR%` expansion — ✅
- working dir, window state — ✅ · process priority, runas/default action — 📋
- MoveTop on use, AutoSort, editor UI — 📋
- Shift+Enter kills the process started by the last alias — ✅ (ShellExecuteEx handle + TerminateProcess)
- Dead-alias check (CheckDeadAliases/ReturnDeadAliases, backup file, confirmations) — 📋

## System aliases
`[SysAliases]` name=action table (defaults: `~lock ~logoff ~sleep ~screensave ~reboot`),
Tab-completed, Shift+Enter-killable — ✅

## History  `[History]`
MaxHistory — ✅; persistence to portable `History.dat` (UTF-8) — ✅; ↑/↓ scroll — ✅;
per-type save toggles (Internal/Alias/SysAlias/WWW/EMail/Path/Plugins/Unknown) — ✅ ·
InvertScroll — 📋

## Undo
`[Undo] UseUndo` — `Ctrl+Z` restores the last executed command — ✅ (caret at end)

## Paths  `[Path]`
Browser, FileManager, Shell, ShellClose (`/c %s`), ShellNoClose (`/k %s`) — ✅ Shell
(`[Shell] Command/Args`) · 📋 Browser/FileManager overrides

## Hint & DropDown popups  `[Hint]` `[DropDown]`
TypeAndRun: ShowHint live-completion popup; DropDown LineNum, Transparent, BorderSize, DestroyForm.
TypeBox 0.10.0: **DropDown** — themed suggestion list under (or above) the console with the ET
sources of the caret word (aliases, ~sysaliases, /commands, plugin commands) and the history when
the line is empty; `[DropDown] Enable / Lines / ShowHistoryOnEmpty`, keyboard
(↑/↓ select, Tab accept, Enter run, Esc close) and mouse (hover + click) driven; colors follow the
theme, so the old DropDownColor/DropDownText/BorderSize trio is intentionally replaced by the
scheme ([Theme]). Hint popup — 📋
(tied to EasyType, Phase 2)

## Color themes  `[Color]`
Scheme 0 console `ConsoleBG`/`ConsoleFG` — ✅ · 3 schemes (No-ET / EasyType / Linux) ×
{console bg/fg/border, hint bg/fg/border, dropdown bg/fg/border} — 📋 (hint/dropdown with Phase 2)

## Sounds  `[Sounds]`
EnableSounds + 9 per-execution-type sounds (internal/alias/sysalias/url/email/folder/file/plugin/shell)
— ✅ (paths relative to portable `Sounds\` folder) · bundled WAV files — 📋

## Language  `[Language]` / `[UI]`
TypeAndRun shipped external `.lng` tables + `.chm` help for 10 languages. TypeBox 0.9.0:
20 languages compiled in (8 complete European + 12 core, English fallback per string),
`[UI] Language` + Language settings page, `auto` UI-language detection, Arabic RTL,
script-capable font suggestion; portable `lang\<code>.ini` overrides replace the `.lng`
idea without extra downloads — ✅ (`.chm` help — 📋, `/help` covers the basics)

## Cozy look  `[Theme]` / `[Color]`  (TypeBox beyond TypeAndRun)
7 warm presets (console + status strip + GDI-drawn tray badge as one, accent-linked),
live preview on the Theme page; status strip shows counts/chips/clock; **visual anchors**
— a traffic light classifies the typed line on every keystroke (green = alias/~/command,
amber = PATH/file/URL/plugin, red = shell catch-all/unknown/off) — ✅ 0.9.1

## Registry integration  `[Registry]`
AutoRun (HKLM/HKCU), "Add to context menu" (ShellInt), MultiUser — 📋
(⚠ TypeBox keeps ALL portable settings in INI; these opt-in entries are explicit-only.)

## Single instance & IPC
Semaphore single-instance; second instance forwards command via WM_COPYDATA; command-line
params (`ParseCmdLine`) to prefill/execute — ✅

## Internal commands (all 14)
`/about /checkconfig /config /exit /help /hide /homedir /inidir /inifiles /load /reread /settings /show /unload` — ✅
(`/load` `/unload` are stubs until the Phase 2 plugin host)

## Plugins (ABI-compatible with TypeAndRun)
`Load(HWND) Unload GetInfo GetCount GetString GetCountET GetStringET RunString→TExec` — ✅
(+ optional `GetPluginMenu/About/Options/FastConfig/HookKey` bound); discovery from
portable `Plugins\` + `[Plugins]` entries — ✅; load/unload at runtime (`/load`, `/unload`) — ✅;
plugin participation in Tab completion — ✅ · `Plugins.ini` per-plugin `SaveHistory` — 📋
See [PLUGIN_API.md](PLUGIN_API.md) and the working example
[`examples/plugin_calc`](../examples/plugin_calc/calc.c).

## --- AND MORE (beyond TypeAndRun) ---
| Addition | Why |
|---|---|
| **INI + portable folder + import/export** | PowerToys Run keeps JSON in `%LOCALAPPDATA%`; TypeBox config moves with you |
| Migration from legacy `TypeAndRun.ini` / `Config.ini` | zero-friction upgrade path |
| Unicode (UTF-8) config & input | TypeAndRun was ANSI/OEM — ✅ history file is UTF-8, UI is full Unicode |
| Fuzzy search across aliases+apps (PowerToys Run-style) | modern launcher UX |
| Info display modules: calculator, web results, files, system status (like PowerToys Run / Raycast) | the "панель" part of the idea |
| Drop-down terminal / bash mode (ConPTY) — Guake/Yakuake/Drop style | the "консоль/баш" part |
| DPI-aware, top-most, per-monitor positioning | ✅ DPI-aware (2004-era VCL app didn't care) |
| Optional plugin manifest + sandboxed plugins (modern ABI v2) | safety without breaking v1 |
| Profiles (multiple INI sets, switchable) | beyond anything in the original |
