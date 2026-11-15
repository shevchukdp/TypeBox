# TypeBox — research-backed feature backlog #2 (ideas #101–200)

Research date: 2026-11-15 · Baseline: **v0.12.0** (verified against [FEATURES.md](FEATURES.md),
[PLAN.md](PLAN.md), [ROADMAP-50.md](ROADMAP-50.md), [IDEAS-RESEARCH.md](IDEAS-RESEARCH.md),
[UX-REVIEW.md](UX-REVIEW.md), [CONFIG.md](CONFIG.md), [CHANGELOG.md](CHANGELOG.md),
`src/main.cpp`, `src/i18n.h`).

This file continues the numbering of [IDEAS-RESEARCH.md](IDEAS-RESEARCH.md) (#1–50) and
[ROADMAP-50.md](ROADMAP-50.md) (#1–70). **Every idea below (#101–200) was grep-checked against those
four docs + the shipped v0.12.0 feature set and is a genuine non-duplicate.** Where an idea complements an
existing roadmap item it is stated explicitly (e.g. "beyond #41") so the maintainer can merge cleanly.

**Constraints honored throughout:** single-file C++/Win32 host; no external dependency in the core exe
(anything heavier rides in an *optional plugin* or shells out to a tool the user already has, user-invoked
only); everything persisted in portable `TypeBox.ini` / `TypeBox.local.ini`; **registry only behind explicit
opt-in**; **offline-first** (network only in a user-typed command); i18n keys appended at the **end** of
`src/i18n.h` (`enum StrId` before `STR_LAST`, `kKeyNames[]`, `kEn`). Rows marked **(i18n)** add new
user-visible strings.

Effort: **S** ≤ 2 days · **M** ≤ 1 week · **L** multi-week · **XL** research-heavy. Value: **H/M/L**.

## Method — what the 2026 research surfaced (so ideas aren't invented from thin air)

- **PowerToys → Command Palette (0.90→0.97+)** now ships: fallback ranking (drag to reorder), control
  PowerToys from the palette, Peek file/folder preview, a Personalization page (background image + color
  tint), a "lightning-fast" no-animation mode, a compact search box + **quick-access shelf** (pinned +
  recent, Alt+1…9), a **WinGet** extension (search/install apps from winget/Store), **Manage Windows
  Services**, and `>` command-prefix. Community still dislikes file fallback ranking & non-learning results.
- **Flow Launcher** most-requested/ported: browser-tab + bookmarks search, clipboard manager, **Window
  Walker**, zoxide, Everything bookmarks, per-app volume (**FlowTrumpet**), Translate/Currency/Weather/
  Thesaurus plugins, ToggleCase, FancyEmoji, "show plugin action keywords when reactivated" (#3123), and a
  **configurable private plugin-store manifest URL** (#2178).
- **Wox v2** (v2.4.9→v2.5.0): GPU render, fzf, Quick Select digits, MRU start page, double-modifier hotkey,
  **MCP server**, **Omarchy auto-theme** (follows desktop + searches Omarchy menu), **imports Raycast
  exports**, and (2.5.0) plugins opening native windows, Timer/Dictation as store plugins.
- **Raycast for Windows** (v0.4x→2.0): Run replacement, per-command hotkeys/aliases from the result row
  (Ctrl+K action panel), snippet auto-expansion + dynamic placeholders, **window management** (per-monitor
  Show Desktop, Minimize/Close Others, Create Layout From Current Windows, launch-with-args), Clipboard
  History, Games, dictation, MCP, Quick Fix (grammar), Hyper Key, file search typo/non-Latin improvements.
- **Listary 6.3** signature asks still unmatched by OSS: **QuickSwitch** in open/save dialogs, **type-to-
  search in Explorer**, double-Ctrl, Ctrl+number, **custom Actions** with `{action_path}` params + per-action
  hotkeys, `folder:/file:/doc:/pic:` filters, **Pinyin-initials** fuzzy, network-drive indexing, "cmd in
  current folder", `mkdir`/`touch` from the launcher.
- **Keypirinha** (abandoned since v2.26; #553 "call for help", active refugee wave) is the patron saint of
  this project's philosophy. Its built-in utility set is cheap to absorb: **PuTTY/WinSCP/FileZilla session
  launch, registry-key launch, translation, math eval, base/hash, UUID/password/byte generation, dictionary
  search**, machine-vs-user config files.
- **Drop-down console heritage** (WT quake megathread #8888 / #653, ConEmu, Guake/WTQ/QuakeShell): slide
  animation with configurable duration, auto-hide on focus loss (have it), **appear on the monitor the mouse
  is on**, and the perennial ask for **a separate drop-down terminal per monitor / per virtual desktop**.
- **r/commandline / AHK / power-user forums**: privacy/no-index/lightweight sentiment (a TypeBox marketing
  edge), **hotstring→run command**, Static Hands, text-replacement, "which simple AHK scripts convert
  office workers"; **proper sudo on Windows** still a dream (HN); virtual-desktop "embarrassingly horrible"
  rants (no wheel-switch, no Ctrl+Win+#); portable-apps platform (PortableApps 30.6.2) now ships a
  **cascading tray app menu** + always-ready search box.
- **Accessibility (NVDA/Narrator)**: WT's own Settings + Command Palette are reported *unusable with screen
  readers* (#20194, "cannot type into the palette"). A hand-drawn GDI launcher has **zero** UIA — exposing
  the list/strip to a screen reader is the single highest-value a11y move.
- **Security (RunMRU / ClickFix, 2025-26)**: Splunk/Huntress/Elastic all hunt malicious commands pasted into
  the Run dialog (ClickFix), and RunMRU records the exact pasted payload. TypeBox already never writes
  RunMRU; a paste-run **payload guard** is a natural, differentiating safety feature.
- **Utilities to lift**: Everything (content search, filters, bookmarks, macros, **find duplicates**, regex),
  **PowerToys Text Extractor / Capture2Text** OCR, **gquick `tr:` translate**, **Media Controls for
  Command Palette** (play/pause/next/prev), tiling-WM users (komorebi/Astur: launcher + `Alt+Shift+Space`
  power menu), WSL (`wsl -d distro -- cmd`, WSLg, wsl-windows-toolbar-launcher).

---

## Search & launch

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 101 | Explicit mode prefixes | Reserve leading chars to force a class, like CmdPal's `>`: `>` = shell (cmd /k), `!` = only plugin commands, `#` = only `[WebSearch]`. Lets a user bypass fuzzy mis-ranking that PowerToys users complain about. Pure classifier branch before execution. | PowerToys CmdPal `> cmd`; Flow/Launchy action words | M | S | v0.12.x **(i18n)** |
| 102 | CJK phonetic search (pinyin / romaji / jamo) | Build a romanization key per alias/app/start-menu name so `azb`→安装包, `weixin`→微信, romaji→日本語, jamo-초성→한국어. Reuse existing fuzzy scorer on the phonetic key. Ship a compact compiled pinyin/romaji table (no dep). | Listary Pinyin-initials; yuru (pinyin/romaji Rust finder); gquick pinyin; tinycast #119 | H | L | v1.x **(i18n)** |
| 103 | Everything bridge (optional) | If `es.exe`/Everything is present, `es <query>` (and a `[Search] Everything=auto` default-off) issues a fast name search over its CLI/IPC and offers results as drop-down rows (Enter opens file, Ctrl+Enter opens folder). No bundled dep — pure shell-out, user-installed tool. | voidtools cult following (HN); Wox+Everything; Flow Everything plugin; Keypirinha file search | H | M | v0.13.x |
| 104 | Registry-key launcher (`reg:`) | `reg HKCU\Software\…` lists child keys/values (RegEnumKeyEx) and Enter opens the key in `regedit /s` at that path. Read-only browse; no writes. Keypirinha's documented launch target, absent from every modern OSS launcher. | Keypirinha registry-key launch | M | M | v1.x |
| 105 | SSH/FTP client session launcher | Parse PuTTY `.reg`/sessions, WinSCP & FileZilla config (offline file read) into drop-down rows under `ssh `/`scp `; Enter launches the saved session. Extensible via `[Sessions] folder=`. | Keypirinha PuTTY/WinSCP/FileZilla sessions; Flow PuTTY plugin | M | M | v1.x |
| 106 | winget install / update / search | `winget search <p>`, `winget install <p>`, and `~winget-upgrade` (runs `winget upgrade --all`) shell out to the winget the user already has; results in the list, confirm before install. Never auto-runs (offline-first). | PowerToys CmdPal WinGet extension; devs | M | M | v1.x **(i18n)** |
| 107 | Open terminal in the last folder | `cmd`/`wt`/`ps` with no arg opens the shell in the last-typed path, the current Explorer folder (UIA), or `[Show] LastDir`; a right-click-on-folder alias variant too. Kills the "cd twice then run" friction. | Listary "cmd in current folder"; WT open-here menu; QuakeShell Explorer integration | M | S | v0.13.x **(i18n)** |
| 108 | Windows Services control (`~svc`) | `~svc <name>` lists services (EnumServicesStatus), Enter starts/stops/restarts with a danger-gate on stop of protected names, row shows state. | CmdPal "Manage Windows Services"; Flow Services; ops people | M | M | v1.x **(i18n)** |
| 109 | Project launcher (`proj <name>`) | `[Projects] name=dir|editor|terminal|startupcmd`; `proj name` opens the folder in the editor + a terminal + runs the startup line in one Enter. Small INI table + a "recent projects" recency order. | Astur (`Alt+Shift+Space` power menu); QuickShell; GNOME Project-Quick-Switcher | M | M | v1.x **(i18n)** |
| 110 | Launch-to-monitor & window-state per alias | Add optional `monitor=` and `state=` to alias/sysalias rows so a given app always opens maximized on a chosen display (MonitorFromPoint/EnumMonitor placement + SetWindowPos after launch). | WT `globalSummon monitor:toMouse`; Raycast per-monitor Show Desktop; multi-monitor users | M | M | v1.x |
| 111 | Recent files grouped per application | `recent notepad` lists only that app's recent docs (re-resolve `%APPDATA%\…\Recent` *.lnk filtered by target exe). Global recent is roadmap #4; this is the per-app slice. | Listary recent folders; Windows Recent; CmdPal #45817 pinnable file types | M | M | v1.x |
| 112 | Quick-access shelf / pinned grid | A persistent "shelf" (a `[Shelf]` INI of favorite apps/URLs/commands) that renders as the drop-down on an empty line (replacing plain history) — a grid you can Alt+1…9 into without typing. Complements #28 (which pins a transient row to the top). | CmdPal quick-access shelf + persistent Dock; Wox MRU start page | M | M | v1.x **(i18n)** |
| 113 | App-row quick actions | On Start-menu/app rows add **Uninstall**, **Homepage**, **Open file location** as extra row actions (contributed into the #41 action framework via optional `GetRowActions`). Built-ins of #41 are generic (run/admin/copy/pin); these are app-specific. | Raycast/Flow app action rows; CmdPal app actions | M | M | v1.x **(i18n)** |

## Console & input

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 114 | Full readline / Vim key set in the edit | Beyond the Ctrl+A/PgUp/PgDn already added (#60): Ctrl+E/A/K/U/W/D, Alt+B/F/D, Ctrl+←/→ word-jump, Ctrl+L clear. Bash muscle memory for the console/bash persona. | bash/zsh/readline; r/commandline crowd | M | S | v0.12.x |
| 115 | Multi-line expand for long commands | Ctrl+M (or a `[Show] Multiline=1`) grows the popup to a multi-line edit for pasted scripts/long pipes; Enter still runs the joined line. | ConEmu/WT multiline; users pasting multi-line one-liners | M | M | v0.13.x |
| 116 | Inline `%1..%n` argument-slot hints | When the caret is on a parameterized alias, show the arg names under the caret in the strip (from alias comment or an `args=` field) so users see what each slot means. | Listary `{action_path}` params; Alfred `{query}` | M | M | v1.x **(i18n)** |
| 117 | Redo + `!!` re-run last command | Keep a small executed-command stack: Ctrl+Y / `/redo` walks forward, and a bare `!!` re-runs (and can edit) the previous command; complements single-level Ctrl+Z (#23). | bash `!!`/Ctrl+Y; PowerShell history replay | M | S | v0.13.x |
| 118 | Voice dictation (offline speech, opt-in) | `dictate` (or a mic hotkey) starts Windows Speech Recognition into the edit via SAPI/`System.Speech`; off by default, on-device only (offline-first). | Raycast/Wox dictation (2026); "type with your voice" | M | L | v1.x **(i18n)** |
| 119 | Per-monitor independent consoles, own hotkey | Allow N concurrent console instances, each pinned to a monitor with its own hotkey (`F1`/`F2`/`F3`), like tilda/Guake multi-monitor setups. Distinct from single-instance #18 monitor choice. | WT issue #653 "separate hotkey terminal for each monitor"; Tilda F1/F2/F3 | M | L | v1.x |
| 120 | Stay-on-top toggle (un-topmost) | Ctrl+Shift+T toggles WS_EX_TOPMOST off so the console sits in normal Z-order for recordings/presentations; tray check + `[Show] Topmost=`. | CmdPal "can't move / always above" gripes; streamers | L | S | v0.12.x **(i18n)** |
| 121 | Font zoom (Ctrl+= / Ctrl+- / reset) | Ctrl+=/- steps `[Font] Size` for the current session (Ctrl+0 restores INI); Ctrl+wheel too. Browser zoom habit for big screens / demos. | WT/ConEmu Ctrl+wheel font size | M | S | v0.12.x |
| 122 | Typed-anywhere command hotstrings | A `[Hotstrings]` table: typing a keyword + trigger **in any app** runs a command/alias (not a text insert — distinct from snippet auto-expand #59), guarded by Ignores.ini + secure-field detection. | AutoHotkey hotstrings→run; awesome-AutoHotkey; the-automator hotstring threads | H | M | v1.x **(i18n)** |
| 123 | Suggest clipboard as `%s` when an alias needs an arg | If a parameterized alias is selected and its arg is empty, show a dim "Enter = paste clipboard as arg" chip; one Enter pastes the clipboard into `%s` before running. | Alfred `{clipboard}`; Flow "paste as plain"; `notepad %1` habit | M | S | v0.12.x **(i18n)** |
| 124 | Dry-run preview (`dry ` prefix) | `dry <line>` prints, in the log/strip, exactly what Enter would resolve + run (class, target, expanded args) without executing — pairs with the danger gate. | safety-conscious r/commandline; complement to danger gate #48/#57 | M | S | v0.13.x **(i18n)** |

## Visual

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 125 | Token colourizer in the edit | Subclass the edit (WM_PAINT/EN change) to colour the typed line: alias/`~sys`/`/cmd` in accent, args dim, path/URL amber, unknown red — mirrors the traffic-light classes, but per-token in the text. | TypeAndRun colored hint (FEATURES 📋); editors' syntax highlight; Raycast accented fuzzy hits | M | M | v0.13.x |
| 126 | Dim-on-blur opacity | When the console loses focus (and isn't hidden), ramp layered alpha to a dimmed value then back on focus; `[Show] DimOnBlur=0..255`. | Guake/Yakuake transparency; CmdPal tint personalization | L | S | v0.13.x |
| 127 | Acrylic / blurred backdrop (opt-in) | `DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE)` behind the console card for a Win11 acrylic look; no-op on Win10; auto-off with `[Animations] Enable=0` or low-power. | CmdPal Personalization background+tint (0.97); Windows acrylic | M | M | v0.13.x |
| 128 | Persistent floating summon button | An always-on-top 40 px circle (draggable, `[Show] FloatingButton=`) the mouse user clicks to summon — fixes "how do I open it without learning the hotkey". | PortableApps always-ready search box; macOS drag handle; discoverability | M | M | v0.13.x |
| 129 | Full theme editor in Settings | A "Theme" sub-page editing every token (bg/fg/accent/lamps/chips/strip) live, **Save as…** to `themes\<name>.theme.ini` (#25 is save-only). | Flow Theme Builder; Wox theme store; CmdPal Personalization | M | L | v1.x **(i18n)** |
| 130 | Two-line dropdown rows (name + description) | Optional second dim line under each row for the alias comment / app publisher / URL host (from alias comments or plugin subtitle). Gated by `[DropDown] TwoLine=`; keeps single-line default. | Raycast/Flow subtitle rows; Listary result hints | M | M | v1.x |
| 131 | Custom / per-theme tray icon | Load `icons\<theme>.ico` if present (GDI fallback otherwise) so the tray badge matches the chosen cozy theme; a Theme-page "Choose tray icon…". | Flow theme icons; Listary tray; PortableApps | L | S | v0.12.x |
| 132 | Cascadia-style ligatures in the console font | Request `GCP/LIGATURE` + FF_DONTCARE so `!=`, `->`, `>=` ligate in Cascadia Code/JetBrains Mono when the user picks them; harmless fallback otherwise. | Windows Terminal Cascadia ligatures; dev aesthetic | L | S | v0.13.x |
| 133 | Colour the console frame by the traffic-light class | Extend the anchors to the whole 1 px border: green/amber/red frame tint per the classified class (a peripheral "what Enter does" cue without moving the eye to the lamps). | own traffic-light concept; CmdPal result-category coloring | M | S | v0.13.x |
| 134 | Compact density toggle | `[UI] Density=cozy|compact|roomy` scales row height, chip padding and strip height from font metrics (one `S()` multiplier) — denser for 4K, airier for touch. | Listary V6 denser list; power-user "make it smaller" | M | M | v0.13.x |
| 135 | Custom console background image + tint | `[Theme] BgImage=<path>` + `BgTint=` draws a dimmed bitmap behind the edit (GDI StretchBlt), staying legible via the contrast guard; opt-in, ships with none. | CmdPal Personalization background image (0.97) | L | M | v1.x **(i18n)** |

## Tray & system

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 136 | Media transport + now-playing tray | Read the SMTC active-session title/artist (ISessionManager) into a tray submenu + strip chip; `play/pause/next/prev/mute` commands (also as `~play …`) drive the active player via media keys. | Media Controls for Command Palette; pleiades play/pause/next; r/windowsapps radial media | M | M | v0.13.x **(i18n)** |
| 137 | Per-app volume mixer quick control | `vol <app>` lists running apps with per-app volume sliders (IAudioSessionManager2); `vol app 40` sets it. Distinct from the global `~volume` in #30. | Flow FlowTrumpet; Sound Volume Hotkeys | M | M | v1.x |
| 138 | Monitor layout switch | `~display clone|extend|second|main` toggles Win+P projection state (via DisplayConfig), and lists connected monitors with a "move TypeBox here" action. | Win+P; multi-monitor power users; WT multi-monitor threads | M | M | v1.x **(i18n)** |
| 139 | System command pack II (extra `~` commands) | Additional table-driven sysaliases beyond #30: `~nightlight`, `~hdr`, `~showdesktop`, `~minimizeall`, `~rotate`, `~vpn`, `~brightness ±` (WMI/ICC read-fallback), each ≤30 lines or an `ms-settings` jump. | Flow System Commands; PowerToys Light Switch/PowerDisplay; Raycast system ext | M | M | v0.13.x **(i18n)** |
| 140 | Power plan quick switch | `~power best|balanced|saver|ultimate` swaps the active power scheme (powercfg/ GUID list read); tray submenu entry; laptop users. | PowerToys; laptop forums; gaming/power users | L | M | v0.13.x **(i18n)** |
| 141 | Tray cascading favorites / recent apps submenu | A tray **Apps ▸** submenu that cascades pinned (`[Shelf]`) + most-frequent apps (from `Usage.dat`), mirroring the PortableApps "cascading tray app menu". Mouse-first discoverability. | PortableApps Platform 30.6.2 cascading tray menu | M | M | v0.12.x **(i18n)** |
| 142 | `/hotkeys` diagnostic command | `/hotkeys` lists the main hotkey + per-command hotkeys, whether each is currently registered (live RegisterHotKey probe on a temp HWND), and which held it — fixes the invisible registration failure UX-REVIEW B6 flags. | UX-REVIEW B6; users whose hotkey is silently stolen | M | S | v0.12.x **(i18n)** |
| 143 | Panic: kill all TypeBox-spawned children | `~killall` terminates every process TypeBox launched this session (track child PIDs/job object), beyond #23 Shift+Enter's last-only kill. | Shift+Enter only kills last; HN runaway-children gripes | M | M | v1.x |
| 144 | Elevated "sudo" helper (opt-in) | A one-time opt-in scheduled task (`RunLevel HighestAvailable`) or tiny privileged stub lets whitelisted aliases launch elevated **without** a UAC prompt each time; everything stays INI-configurable and off by default. | HN "unless we could have proper sudo on Windows"; sudo-for-Windows demand | M | L | v1.x |
| 145 | Auto-rescan app index on new install | Register for folder/USN change on Start-Menu dirs → debounced re-scan of the app index (roadmap #3 is scan-once) so newly installed apps appear without `/reread`. | Fluent Search "new apps take time to appear"; stale-app-list complaints | M | M | v1.x |
| 146 | Screen-share / presentation auto-silence | Detect a screen-share or fullscreen foreground window and auto-suppress sounds + tray balloon + hotkey for its duration (a smart companion to #31 game mode / #32 DND). | CmdPal "disable hotkey in fullscreen"; streamers/teachers | M | M | v0.13.x |
| 147 | Tray profile switcher submenu | A tray **Profile ▸** submenu (radio list of `[Profiles]`) to hot-swap the active profile from the tray, not only via `/profile` (profiles = #42). Mouse access to profile switching. | ConEmu task profiles; ueli per-machine flows | L | S | v1.x **(i18n)** |

## File ops & utilities

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 148 | Mini file verbs | Safe built-ins from the console: `mkdir X`, `touch X`, `rm X` (→ Recycle Bin via `SHFileOperation`, danger-gated), `cp/mv/ren a b`, `zip X`. Each echoes what it did to the log. | Listary `mkdir`/`touch` in the launcher; AHK file ops | M | M | v1.x **(i18n)** |
| 149 | Create shortcut (.lnk) to a target | `lnk <target> [name]` writes a `.lnk` (IShellLink) into a chosen folder/Desktop; optional "Add to shelf". Portable-users making links without Explorer. | Listary Actions; Windows "Send to"; AHK FileCreateShortcut | M | S | v1.x **(i18n)** |
| 150 | Create symlink / junction / hardlink | `link`, `junction`, `hardlink <target> <link>` (CreateSymbolicLink/DeviceIoControl), elevation-aware + gated. For dotfiles, git worktrees, portable data dirs. | "mklink" power users; devs (dotfiles, worktrees) | M | M | v1.x |
| 151 | Folder size report (`du`) | `du <folder>` walks sizes (recursive, with a cancel on Esc) and shows total + top-N subfolders in the strip/log — a `du`/TreeSize-lite without a real tool. | du/disk-usage habit; Everything size column | L | M | v1.x |
| 152 | Smart directory jump (zoxide-style, `z <name>`) | Track visited folders (from launched aliases/`cd`), rank by frecency, and `z name` jumps Explorer / opens a terminal in the best match. Offline, portable (`Dirs.dat`). | zoxide (40k stars) fzf crowd; Flow zoxide plugin | M | M | v1.x |
| 153 | Open-with an arbitrary installed app | `open <file-or-folder> with <app>` resolves `<app>` via the app index and launches it with the path as arg (Listary `{action_path}` actions, but typed ad-hoc rather than pre-configured). | Listary Actions "Edit with VS Code"; Windows Open With | M | M | v1.x **(i18n)** |
| 154 | Portable-apps scanner | Scan a `PortableApps\` root (and TypeBox's own folder) for launcher exes + `AppInfo.ini`/`_NAME` advertising, auto-registering each by its advertised name (revives LiberKey/PortableApps start-menu UX without their platform). | PortableApps.com / LiberKey; USB-drive users | M | M | v1.x **(i18n)** |
| 155 | Windows Sandbox profile launch (`~sandbox`) | `~sandbox [name]` generates a `.wsb` from `[Sandbox] name=Folders=…;Networking=…` rows and launches it — run sketchy installers isolated, config portable & offline. | security-minded running of untrusted installers; Windows Sandbox | M | M | v1.x **(i18n)** |
| 156 | WSL quick launch | Enumerate distros (`wsl -l -q`); `wsl <name>` opens it, `~wsl <cmd>` runs a one-off in the default distro, and WSLg GUI apps appear as rows (`wsl -d <d> -- app`). | wsl-windows-toolbar-launcher; WSLg GUI apps; devs `wsl -d distro -- cmd` | M | M | v1.x **(i18n)** |
| 157 | OCR region capture to clipboard (`ocr `) | `ocr` drags a screen region → extract text with **Windows.Media.Ocr** (on-device, system language) → copy to clipboard, with `;paste` to insert it directly. Fully offline. | PowerToys Text Extractor / Capture2Text; Snipping OCR | M | M | v1.x **(i18n)** |
| 158 | Translate typed text (`tr: <text>`) | `tr:<lang> text` (and `tr:` auto-detect) returns a translation. **Offline-first:** uses the Windows translation API only when the user invokes it; caches last-N results in `Trans.dat`. | Keypirinha GoogleTranslate pkg; gquick `tr:`; Flow Translate plugins (top-requested) | M | M | v1.x **(i18n)** |

## Plugins & scripting

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 159 | Scripts folder auto-register | Every `Scripts\*.cmd|bat|ps1|ahk|vbs` becomes an alias named by filename (leading `%s` = rest of the line), Tab-completed and `[Scripts]` gated. No DLL build needed for "custom commands" — the Listary/smolcast sweet spot. | Listary custom commands; smolcast tracked script files; AHK users | H | M | v0.13.x |
| 160 | Richer plugin result API | Add optional `GetString2` returning structured rows (title, subtitle, icon-glyph id, class, custom action verbs) so plugins can render two-line rows (#130) + their own Ctrl+K actions without string hacks. | Raycast/Flow rich rows; Wox native-window plugins (2.5) | M | L | v1.x |
| 161 | Plugin-contributed status-strip chip | A plugin export `GetStripChip(wchar_t* buf, COLORREF*)` so an extension can paint one themed chip (e.g. "●VPN on", "3 downloads") next to the clock. Small ABI add-on. | CmdPal extension widgets; Flow status plugins | L | M | v1.x |
| 162 | Per-alias icon + description fields | Extend `[Aliases]` with optional `icon=` and `desc=` so the drop-down/rows can show a glyph + subtitle and `/checkconfig` can validate them; feeds #130/#125. | Listary custom-command icon/hotkey fields; Alfred workflow icons | L | S | v1.x **(i18n)** |
| 163 | `typebox://` URI protocol handler | Opt-in HKCU protocol `typebox://run?cmd=…` (and `typebox://add?alias=…`) so web pages/scripts can invoke a TypeBox alias — revives Keypirinha/Launchy `kepirinha://` launchers. Explicit opt-in per registry policy. | Keypirinha/Launchy URI launchers; `wt`/`listary://` schemes | M | M | v1.x |
| 164 | Context-conditional aliases | An alias row can carry `when=app:code.exe | file:C:\flag` so it only appears/runs in that context (like AHK `#HotIf`); otherwise hidden from the drop-down. | AutoHotkey `#HotIf`; context-aware launchers | M | M | v1.x |
| 165 | Plugin modes with a prefix | A plugin declares a `mode=` prefix char; typing it enters an interactive plugin mode where subsequent rows/keys are routed to the plugin (multi-step flows), `Esc` exits. | Flow action keywords (#3123); Keypirinha plugin modes; Alfred workflow input modes | M | L | v1.x **(i18n)** |
| 166 | Named-pipe / CLI control API | Formalize automation: `TypeBox.exe --run '<cmd>'` (over the existing single-instance WM_COPYDATA) plus a localhost named pipe exposing `run`, `list`, `status` for scripts/IDEs — deliberately simpler than the #40 MCP bridge. | IDE/CI scripting; complement to MCP bridge #40 | L | M | v1.x |
| 167 | Plugin manifest declares hotkey & mode prefix | The `[Trust]`-manifest (#37) can advertise per-command hotkeys and a mode prefix; the host auto-registers/unregisters on load/unload — hotkeys without editing the main `[HotKey]`. | Raycast per-command hotkeys (#13); Flow hotkey tab | M | M | v1.x |

## Safety & privacy

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 168 | ClickFix / paste-run payload guard | When a line was **pasted** (Ctrl+V) rather than typed and matches remote-exec patterns (`curl …|sh`, `certutil -decode`, `mshta`, `powershell -enc`, `bitsadmin`, long base64), block auto-run, show a **decoded preview**, require an explicit typed "run". Goes far beyond the `del /s`/`format` danger gate (#48/#57). TypeBox never writes RunMRU — the one safe Run box. | Splunk/Huntress/Elastic ClickFix + RunMRU hunts (2025-26); security forums | H | M | v0.13.x **(i18n)** |
| 169 | Private session toggle (don't record this) | A tray/toggle "Private" (or leading `-` before a line) that suppresses History.dat/Usage.dat writes for the session only — the launcher equivalent of incognito. | privacy-first r/commandline; Alfred clipboard off-by-default ethos | M | S | v0.12.x **(i18n)** |
| 170 | Structured execution audit (`/audit`) | Keep a rolling `Audit.dat` (timestamp, class, target, exit code from the tracked handle) and `/audit` prints recent rows — who-ran-what forensics beyond the plain text log. | auditors; power users debugging failing aliases | L | M | v1.x |
| 171 | Clipboard secret auto-purge | For the (opt-in) clipboard history, drop items matching secret shapes (JWT, `AKIA…`, `-----BEGIN`, `[A-Za-z0-9]{40,}`) from memory after `[Clipboard] SecretTtl=60s` and never persist them to disk. | clipboard-manager leak reports; Alfred secure-field exclusion | M | M | v1.x |
| 172 | Config integrity pin (anti-tamper) | `[Config] IntegrityPin=<sha256>` computed at first run (opt-in); on each load, hash `TypeBox.ini` and warn (tray dot + log + inline banner) if it changed unexpectedly — catches drive-by config edits / malicious "settings" downloads. | config-drift/tamper anxiety; Flow #3643 config-breaking incidents | M | M | v1.x **(i18n)** |
| 173 | Allowlist-only execution mode (kiosk) | `[Exec] AllowlistOnly=1` runs **only** known aliases/sysaliases/`[Path]` programs/quicklinks and refuses shell catch-all + arbitrary paths — a locked-down mode for shared/kiosk machines. | locked-down/lab machines; enterprise power users | L | M | v1.x **(i18n)** |
| 174 | Time-based history auto-purge | `[History] MaxAgeDays=90` drops history/Usage rows older than N days on load (in addition to `Max` count), so stale entries fade out and sensitive old commands don't linger. | history-hygiene; privacy users | L | S | v0.12.x |
| 175 | DPAPI-encrypted secrets in TypeBox.local.ini | Opt-in `setsecret NAME <value>` stores a `%NAME%` under `[Secrets]` encrypted with the current user's DPAPI blob (never the portable main INI); aliases use `%NAME%` transparently. Keeps tokens off disk in plain text. | users storing API keys in aliases; portable-app privacy | M | M | v1.x **(i18n)** |

## Settings & portability

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 176 | `/reset` factory-defaults wizard | `/reset [section]` snapshots the INI (backing up per #50), restores chosen sections to compiled defaults, and re-applies live — escape hatch when a config goes sideways. | "bricked my config" support threads; ConEmu reset habit | M | S | v0.12.x **(i18n)** |
| 177 | Selective export/import (by section) | `/export aliases|themes|snippets|quicklinks <path>` exports only that section (and `/import` merges), so sharing a theme/alias pack doesn't drag your history or hotkeys. | Flow/Wox partial-import requests; "just share my aliases" | L | M | v0.13.x **(i18n)** |
| 178 | Drag-and-drop INI onto tray to import | Dropping a `.ini`/`.zip`/`Scripts\` onto the tray icon (or the console) triggers `/import` with a confirm + per-object report — no command typing. | PortableApps drop-in; ConEmu folder drag | M | M | v0.13.x **(i18n)** |
| 179 | Forward config schema migration (`/migrate`) | Stamp `TypeBox.ini` with a `SchemaVersion`; on a newer build, `/migrate` runs registered step-up transform funcs (backup first) so renamed/relocated keys move automatically across releases. | config-rot across versions; PowerToys DSC migration ethos | L | M | v1.x |
| 180 | Config doctor: one-click auto-fix | `/checkconfig` (#46) already reports; add `[Enter] fix it` actions per finding (repoint dead alias paths, drop duplicate aliases, fix bad COLORREF/VK) with an undo snapshot (#50). | "INI is unforgiving" Keypirinha reviews; PowerToys validate ethos | M | M | v1.x **(i18n)** |
| 181 | Roaming / installed install mode (opt-in) | `[Install] Mode=portable|roaming`: roaming keeps `TypeBox.ini` under `%APPDATA%\TypeBox` (still **no registry**) for multi-machine via OneDrive, with portable as the untouched default. | users wanting roaming config; multi-PC power users | L | M | v1.x **(i18n)** |
| 182 | Command-scoped help | `/help alias`, `/help websearch`, `/help hotkey`, `/help plugin` print just that topic's INI syntax to the console (a help index keyed by `#101` prefixes) instead of one big `/help`. | `tldr`-style docs habit; man-page-on-tap | M | S | v0.13.x **(i18n)** |
| 183 | "Import pack" with per-object preview | A single shareable pack (aliases + themes + quicklinks + snippets) previews each object (add/override/skip) before merge — safer than the blunt `/import`. | Raycast/Wox import flows (2.4.9); "importer = growth lever" | M | M | v1.x **(i18n)** |

## Accessibility & i18n

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 184 | UI Automation provider for the drop-down + status strip | Expose the GDI drop-down as a UIA list (items announce "1 of 8, calc, alias") and the strip as a text element, and the edit as a labelled edit — so NVDA/Narrator can read it at all (the biggest a11y gap for a hand-drawn UI). | WT Settings/Palette unusable with NVDA (#20194); NVDA elements-list discoverability (#14720) | H | L | v1.x **(i18n)** |
| 185 | TTS announce selected row / execution result | Opt-in (`[A11y] Speak=1`) speaks the selected row on ↑/↓ and the outcome ("launched calc", "not found") via `System.Speech`/SAPI on-device; respects speech rate + mute. | screen-reader users; "hear what Enter did" | M | M | v1.x **(i18n)** |
| 186 | Locale-aware number / date / currency formatting | Use `GetLocaleInfoEx`/`GetNumberFormat` so the calculator, clock and snippet `{date}` honor the user's decimal/thousands/date separators per `[UI] Language` rather than a hard-coded en-US. | non-EN users seeing `1.234,56` mismatches; i18n polish | M | M | v1.x |
| 187 | Access keys + full keyboard tab-order in Settings | Give every hand-built control an `&Access` mnemonic + a correct Tab/Shift+Tab chain (and a visible focus rectangle) so the 14-page dialog is fully keyboard-operable — WT's SR keyboard-trap bug (#20194) is exactly this class. | keyboard-only users; SR keyboard-trap bugs | M | M | v1.x |
| 188 | Dyslexia-friendly font + line-spacing option | `[A11y] Dyslexic=1` swaps to a dyslexia-friendly face (Atkinson Hyperlegible/OpenDyslexic, if installed, else a spaced fallback) and raises line spacing in the edit + list. | dyslexic users; PowerToys accessibility push | L | S | v1.x **(i18n)** |
| 189 | Sticky-Keys-friendly single-key / modifier-only summon | Honor Windows Sticky Keys: allow a modifier-only or single-key summon with a "stuck modifier" confirmation chip, so users who can't hold chords can still open TypeBox. | motor-impairment users; Sticky-Keys forums | M | S | v1.x **(i18n)** |
| 190 | `/langcheck` + `/langexport` translator tools | `/langcheck <code>` lists missing/fuzzy strings (per-string fallback count) in the log; `/langexport <code>` writes a `lang\<code>.ini` template pre-filled with untranslated `kKeyNames` for community packs. | community translators; LANGUAGE.md contributor workflow | M | S | v0.13.x |
| 191 | Follow Windows High Contrast / transparency automatically | Auto-switch to the `high-contrast` preset when the OS High-Contrast theme is active, and drop transparency/acrylic when `[Settings] TransparencyEffects=off`, then revert (complements the #26 presets). | HC-theme users; PowerToys accessibility auto-detect | M | S | v0.13.x **(i18n)** |

## Fun / easter

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 192 | CRT / amber-terminal retro theme + typing sounds | A novelty `[Theme] Name=crt-amber` with phosphor palette + optional GDI scanline overlay + per-key click sounds (`Sounds\`); off the cozy line, purely for fun. | retro-terminal nostalgia; ConEmu skins; r/unixporn crowd | L | M | v1.x **(i18n)** |
| 193 | Konami code + secret hidden commands | Typing the Konami sequence in the console flips the tray badge to rainbow for 60 s and unlocks `~nerve`, `~matrix`, `~spin` joke commands. | classic easter eggs; launcher community memes | L | S | v1.x |
| 194 | `/motd` daily tip + "command of the day" | `/motd` shows one localized tip (rotated by date from a compiled list); "command of the day" surfaces a user alias they rarely use (from `Usage.dat`) to drive discovery. | CmdPal extension-discovery ethos; help discoverability (UX B1) | L | S | v0.13.x **(i18n)** |
| 195 | Animated ASCII logo in `/about` | `/about` paints a small GDI-drawn ASCII/`>_` logo that re-colours to the theme and cycles a couple of frames; version + the #70 idle-CPU proof sit under it. | retro `about` screens; app personality | L | S | v0.13.x |
| 196 | Achievement milestones / command streaks | `/stats` (planned) extended with lighthearted milestones (1000th command, fastest run, week streak) shown once in the log — pure flavour, no nagging. | gamified-power-user culture; Wox/Raycast store badges | L | S | v1.x **(i18n)** |
| 197 | `~dice` / `~coin` / `~random` toy commands | `~coin` → heads/tails, `~dice 2d6` → rolls, `~random N-M` → number; the results copy to clipboard (the Keypirinha random-utility set, minus the serious pwgen which is #38). | Keypirinha random gen; quick pickers | L | S | v0.12.x |
| 198 | Theme roulette + `/shuffle` surprise accent | `/shuffle` picks a random cozy theme (or a hue-shifted accent through the contrast guard) for the session; `~roulette` same at launch with a `[Fun]` opt-in — makes theming a game. | theme-hopping community; Wox theme store | L | S | v0.13.x **(i18n)** |
| 199 | Reactive tray-badge mascot | The GDI tray badge grows tiny "eyes" whose state tracks the app (idle = calm, console shown = wide, error = sweat, child running = working), all GDI-drawn per-size; ties #70/A7 to personality. | app-mascot charm; desktop pets; tray-first apps | L | M | v1.x |
| 200 | "Boss key" gag overlay | `~boss` (or a configured panic hotkey) instantly hides the console and opens a convincing static "spreadsheet/README" PNG the user can drop via `[Fun] BossImage=`; second press restores. | the classic "boss key" gag; AHK panic scripts | L | S | v1.x **(i18n)** |

---

## Top 15 value/effort

Ranked by value ÷ effort, favouring cheap high-value wins that also close parity gaps or reinforce
TypeBox's privacy / offline / safety identity.

| Rank | # | Title | Value | Effort | Ver |
|---|---|---|---|---|---|
| 1 | 168 | ClickFix / paste-run payload guard | H | M | v0.13.x |
| 2 | 159 | Scripts folder auto-register | H | M | v0.13.x |
| 3 | 103 | Everything bridge (optional) | H | M | v0.13.x |
| 4 | 152 | Smart directory jump (zoxide-style) | M | M | v1.x |
| 5 | 107 | Open terminal in the last folder | M | S | v0.13.x |
| 6 | 114 | Full readline / Vim key set | M | S | v0.12.x |
| 7 | 122 | Typed-anywhere command hotstrings | H | M | v1.x |
| 8 | 108 | Windows Services control (`~svc`) | M | M | v1.x |
| 9 | 112 | Quick-access shelf / pinned grid | M | M | v1.x |
| 10 | 136 | Media transport + now-playing tray | M | M | v0.13.x |
| 11 | 142 | `/hotkeys` diagnostic command | M | S | v0.12.x |
| 12 | 124 | Dry-run preview (`dry ` prefix) | M | S | v0.13.x |
| 13 | 148 | Mini file verbs | M | M | v1.x |
| 14 | 158 | Translate typed text (`tr:`) | M | M | v1.x |
| 15 | 184 | UIA provider for drop-down + strip | H | L | v1.x |

## Next 3 releases — picks

Three cohesive, shippable batches. "(i18n)" = needs new keys appended at the end of `src/i18n.h`.

### v0.13 — "Muscle memory & safe hands" (headline: #168 ClickFix / paste-run payload guard)
A safety + input-persona release that leans into TypeBox's "the only safe Run box" story and bash muscle memory.
- **#168** ClickFix / paste-run payload guard *(i18n)* — the headline; decoded preview + confirm for pasted remote-exec payloads.
- **#124** `dry ` dry-run preview *(i18n)* — pairs with the gate; shows what Enter would resolve.
- **#114** Full readline / Vim key set — no strings, pure input win.
- **#117** Redo + `!!` re-run — console/bash credibility.
- **#107** Open terminal in the last folder *(i18n)* — everyday dev win.
- **#142** `/hotkeys` diagnostic *(i18n)* — closes UX-REVIEW B6's invisible-registration bug.
Needs new i18n keys for the guard's confirm/decoded-preview strings, the `dry` output label, terminal-verb
confirmations, and the `/hotkeys` report.

### v0.14 — "Dev & reach" (headline: #159 Scripts folder auto-register + #103 Everything bridge)
Makes TypeBox the developer's launcher without adding a bundled dependency — everything shells out to tools
the user already has, user-invoked only.
- **#159** Scripts folder auto-register — headline; drop a script in, get a command.
- **#103** Everything bridge (optional) — headline; fast file search when Everything is installed.
- **#108** Windows Services control (`~svc`) *(i18n)* — ops/dev control.
- **#152** Smart directory jump (`z`) — zoxide-style frecency, offline `Dirs.dat`.
- **#156** WSL quick launch *(i18n)* — distros, run-in-distro, WSLg GUI.
- **#106** winget install / update / search *(i18n)* — user-invoked package flows.
Needs i18n keys for `~svc`, `~wsl`, winget confirm/results labels and the jump-to-dir row wording.

### v1.0 — "Reach & care" (headline: #184 UIA screen-reader provider)
The "polished, everyone can use it, worldwide" release that turns the 20-language, offline launcher into an
accessible one and completes the community-reach features.
- **#184** UIA provider for the drop-down + status strip *(i18n)* — headline; NVDA/Narrator can finally read the list.
- **#185** TTS announce selected row / result *(i18n)* — audible feedback, on-device.
- **#102** CJK phonetic search (pinyin/romaji/jamo) *(i18n)* — unlocks the Listary/Keypirinha CJK audience.
- **#186** Locale-aware number/date/currency formatting — calc/clock/snippets follow the locale.
- **#136** Media transport + now-playing tray *(i18n)* — the "control center" daily driver.
- **#158** Translate typed text (`tr:`) *(i18n)* — the top-requested plugin utility, offline-first via the OS API.
Heaviest i18n batch (screen-reader/announce strings, media transport menu, translate results UI, CJK help
text); all appended at the end of `src/i18n.h`, non-EN packs fall back per string, community `lang\<code>.ini` overrides via #190.
