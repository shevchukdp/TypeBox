# Changelog

All notable changes to TypeBox are documented here.
Version source of truth: the `VERSION` file; a git tag `vX.Y.Z` must match it (enforced in CI).

## [0.5.0] - 2026-11-15

### Added
- **System aliases (`[SysAliases]`)** — TypeAndRun-style `~name = action` commands
  with safe built-in defaults `~lock · ~logoff · ~sleep · ~screensave · ~reboot`;
  user section replaces the table; `%s`/`%1..%n` supported; Tab-completed
  (`[Type] UseSysAliasET`); logged on failure (`sysalias failed: ...`).
- **Ignores.ini (BadApp parity)** — portable file of process-name / window-class
  patterns (wildcards `*` `?`); while the foreground window matches, the hotkey
  keeps the console hidden.
- **Shift+Enter kills the last alias process** — `ShellExecuteExW`
  (`SEE_MASK_NOCLOSEPROCESS`) keeps the handle, `TerminateProcess` on demand;
  works for aliases and sys aliases; result visible in the log.
- **`[Show] Priority`** — process class of TypeBox itself:
  `idle · normal · high · realtime` (`SetPriorityClass`).
- **`[Font] ApplyEnglish`** — force US-English keyboard layout when the console
  opens (TypeAndRun parity, no more typing commands in the wrong layout).
- **`/sys` command** — RAM/CPU/uptime/battery snapshot to the portable log and a
  tray balloon (non-blocking, PowerToys-Run-style info line).
- `/checkconfig` now also reports sys aliases and Ignores counts.

### Verified
- End-to-end harness (C helper posts WM_HOTKEY / WM_KEYDOWN): hotkey toggle,
  Ignores suppression over the foreground app, sys alias launch, alias launch
  with handle tracking, Shift+Enter kill — all confirmed through the portable log
  (`alias started: cmd (handle ok)` → `Shift+Enter: killed alias process cmd`).

## [0.4.0] - 2026-11-15

### Added
- **Plugin host with TypeAndRun-compatible ABI** (`docs/PLUGIN_API.md`):
  `Load(HWND) / Unload / GetInfo / GetCount / GetString / GetCountET / GetStringET /
  RunString → TExec`, optional `GetPluginMenu / About / Options / FastConfig / HookKey`.
  ANSI (CP_ACP) strings for 1:1 compatibility with 2001–2004 plugins.
  Plugins are discovered from the portable `Plugins\` folder and `[Plugins]` ini entries;
  `/load` rescans at runtime, `/unload` deactivates them.
- **Execution chain consults plugins** (after aliases, before URL): the first plugin
  returning `TExec.run != 0` consumes the input; `con_text` is placed into the console
  with selection — exactly like TypeAndRun.
- **Plugin completion sources** for `Tab` (static `GetString` list + dynamic `GetCountET`).
- `[Exec] Plugins` toggle (default on).
- **`examples/plugin_calc/calc.c`** — calculator plugin (`calc 2+3*4`) in the original ABI,
  buildable with both MSVC and MinGW; doubles as a template for porting legacy
  `tar_math`-style plugins.
- **Portable error log with configurable size & rewrite method** — `[Log] Enabled /
  File / MaxSizeKB / Rewrite=rotate|truncate|timestamp|keep / Keep`; failed hotkeys,
  plugin load errors, failing aliases and import/export errors land there;
  open with `/log`.
- **`TypeBox.exe --version`** console flag + CI **smoke gate**: a build that cannot
  start, reports a wrong version, or fails to load the example plugin never reaches a release.
- **MSVC release is fully static** (`/MT`) — release zip needs no VC++ redistributable;
  MinGW build documented with `-static` (no libgcc/libstdc++ DLL dependencies).

### Fixed
- **Example `calc.dll` now exports the ABI under MSVC too** (`__declspec(dllexport)` via
  `CALC_BUILDING_DLL`) — the first published zip carried a host-invisible calc.dll
  (MSVC exports no symbols without dllexport; MinGW exported them implicitly).
  The host also tolerates legacy 32-bit `_Name` cdecl exports.

### Verified
- End-to-end ABI smoke test: host discovers a DLL in `Plugins\`, calls `Load`, routes a
  line through `RunString`, applies `TExec` — against MinGW g++ 13.2 (`-Wall -Wextra`, clean).
- Every published asset is downloaded and executed on a real machine
  (`--version` exit code, GUI liveness, startup log content).

## [0.3.0] - 2026-11-15

### Added
- **EasyType completion** (`[Type]`) — `Tab` completes the word at the caret from
  internal commands, aliases and history; repeated `Tab` cycles matches.
  Sources toggleable: `UseInternalET` / `UseAliasET` / `UseHistoryET`.
- **`/export <path>` and `/import <path>`** — one-command settings portability
  (import backs the current config up as `TypeBox.ini.bak` and live-reloads it).
- **`/undo`** command in addition to `Ctrl+Z`.
- **AutoHide** — `[Show] AutoHide=1`, `AutoHideValue=<seconds>` hides an idle console.
- **Console colors** — `[Color] ConsoleBG / ConsoleFG` (COLORREF integers).
- **Per-type history toggles** — `[History] Internal/Alias/WWW/EMail/Path/Plugins/Unknown`
  (TypeAndRun parity).

### Fixed
- Single `[History]` section in the sample config.

### Verified
- CI smoke gate additionally requires the example plugin to load through the ABI
  (`startup log: N plugins`, no "plugin load failed"/"ignored non-plugin dll").
- History candidate list rebuilds correctly when typing between `Tab` presses.

## [0.2.0] - 2026-11-15

First functional build. CI compiles with MSVC on every push; tagged releases ship a
portable zip with `TypeBox.exe` + `TypeBox.ini`.

### Added
- **CI/CD** — `.github/workflows/build.yml`: MSVC (Ninja) build on `windows-latest`,
  `VERSION`-vs-tag consistency check, artifact zip `TypeBox-<ver>-windows-x64.zip`,
  GitHub release on tag with binaries and roots/inspiration notes.
- `VERSION` file — single source of truth for the version; injected into the binary
  via CMake (`/about`, tray tooltip show it).
- **All 14 TypeAndRun internal commands** —
  `/about /checkconfig /config /exit /help /hide /homedir /inidir /inifiles /load
  /reread /settings /show /unload` (`/load`–`/unload` become functional with Phase 2 plugins).
- **Tray icon** with context menu (Show/Hide · Config · Exit) and startup balloon tip
  (`[Show] TrayIcon`, `BalloonTips`).
- **Transparency** — `[Show] Transparent=0..255` (layered window).
- **Font from INI** — `[Font] Name / Size / Bold`.
- **`[Exec]` toggles** — Internal / Aliases / WWW / EMail / Files / Shell now all honored.
- **Sounds** — `[Sounds] EnableSounds` + 9 per-execution-type sounds (internal, alias,
  sysalias, url, email, file, folder, plugin, shell), paths relative to the portable
  `Sounds\` folder (TypeAndRun parity).
- **Undo** — `[Undo] UseUndo`, `Ctrl+Z` restores the last executed command.
- **Persistent history** — saved to portable `History.dat` next to the exe (UTF-8),
  `[History] Max`, loaded at startup; `↑/↓` navigation; Ctrl+Backspace deletes a word.
- **Legacy pipe-form aliases** — `alias = action|param|dir|state|priori|comment`
  (working directory and window state applied), read from `[Aliases]` of `TypeBox.ini`
  or a legacy `Config.ini` next to the exe.
- **Single instance + IPC** — a second launch forwards its command line to the running
  instance via `WM_COPYDATA` and exits (TypeAndRun parity).
- **DPI-aware** manifest call (`SetProcessDPIAware`).
- `/reread` reloads hotkey, aliases and settings without restart.

### Fixed / improved
- Alias execution uses `ShellExecuteExW`-style dir/state parameters.
- History caret lands at end of line; history survives restarts.

### Verified
- Compiles clean (MinGW g++ 13.2 `-Wall -Wextra`, zero warnings) and runs:
  hotkey registration, tray icon, popup, execution chain. MSVC verification in CI.

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
- Plugin loader, info display, terminal mode: not yet implemented.
- Import/export is "copy the INI" for now; a UI action is planned.
