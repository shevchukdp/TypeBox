# TypeBox — research-backed feature backlog (50 ideas)

Research date: 2026-11-15 · Baseline: **v0.10.0** (verified against [FEATURES.md](FEATURES.md),
[CONFIG.md](CONFIG.md), [PLUGIN_API.md](PLUGIN_API.md), CHANGELOG.md) ·
Constraints honored: single-file C++/Win32 host, no `.rc` resources, no external deps in the core
exe (plugins may add them), everything in portable `TypeBox.ini`, registry only behind explicit
opt-in, GPL-3.0, offline-first (network only in user-invoked features).

## Method

Websearch queries (2025–2026 landscape) covered: PowerToys / Command Palette release blogs
(0.90→0.102) and "Replace PowerToys Run with Command Palette" epic + linked feature-request issues
(#38683, #38423, #39399, #43174, #45817, #46096, #50698); Flow Launcher README/releases/plugin store
and issue threads (#2963 clipboard clear, #2069 browser-tab search, #3326 Everything bookmarks,
#2253 Window Walker); Wox v2 changelog (beta.1→v2.4.9: fzf fuzzy, Quick Select, MRU start page,
double-modifier hotkey, MCP server, Raycast import); Raycast-for-Windows blog + changelog
(v0.24→v0.41: Run replacement, snippet auto-expansion, per-command hotkeys, Games, window
management, custom fallback list, NTFS fast scanner); Listary 6.3 changelog/docs (QuickSwitch,
type-to-search, double-Ctrl, custom commands/actions); Alfred help (Snippets, dynamic placeholders,
Clipboard History); Keypirinha site/FAQ/community reviews (file-driven INI, machine/user split
configs, utility commands, abandonment); drop-down terminal heritage (Windows Terminal quake,
ConEmu, Guake/Yakuake, WTQ multi-monitor behavior); TypeAndRun ancestry (totalcmd.net changelog,
codeleventh mirror, Russian user reviews — autostart, "Add to TypeAndRun" shell menu, hint/dropdown,
width-in-percent); r/commandline and r/Windows11 launcher threads (lightweight/no-index asks,
fzf-style ideas, ueli workflows, QuickChat LLM launcher). Every idea below was checked against
FEATURES.md to confirm it is absent in v0.10.0; items marked *(planned)* extend the existing
roadmap in FEATURES.md/PLAN.md rather than inventing new scope.

## Landscape notes (short)

- **PowerToys** renamed Run → **Command Palette** (0.90+) and is shipping hard: extension gallery,
  a Dock with pinnable commands + quick-access shelf (Alt+1…9), calculator history, new fzf-style
  fuzzy matcher (0.95), per-utility CLI automation (0.102), PowerDisplay/Light Switch/Peek. But
  the community loudly dislikes it vs old Run: files become "second-class fallback" (#46096, #38683),
  exact matches rank below web search, window can't be dragged, ranking doesn't learn from usage
  (#43174), users beg for history of last queries and pinnable file types (#45817), portable-app
  folders aren't indexed (#39399), and a "minimal, search-box-only mode" (#38423). TypeBox's single
  line + dropdown IS the minimal mode — lean into that.
- **Microsoft is reviving Win+R itself** (2026 Insider "new Run dialog": 94 ms show time, `~\`
  directory navigation). Signals demand for a better Run box; Raycast shipped its own "Run"
  replacement command (v0.41) with browsable verbs + history.
- **Flow Launcher** (v2.1.4, 2026-09) differentiates with F1 preview panel, markdown previews,
  theme builder, VirusTotal-scanned plugin store, `pm install`, hotkeys per plugin/mode, shell-as-admin,
  Alt+number select, Ctrl+C/Ctrl+Shift+C copy path, Ctrl+Enter open-containing-folder, Game Mode.
  Most-requested: browser-tab search (delivered as plugin in 2025), clipboard management, Everything
  bookmarks.
- **Wox v2** (v2.4.9, 2026-10) = the "fast OSS Raycast": GPU rendering, fzf algorithm, Quick Select
  digits, MRU results on open, double-modifier hotkey (double-tap Ctrl), MCP server for plugin dev,
  AI chat plugin, theme store, and it imports Raycast settings/plugins — importers are a proven
  growth lever.
- **Raycast for Windows** (public beta 2025-11, ~monthly releases): per-command hotkeys/aliases set
  from the result row (Ctrl+K action panel), snippet auto-expansion with dynamic placeholders,
  clipboard history, custom fallback command list, window management, Games (Steam/Epic/GOG/…),
  ranking that learns which command you open with which search term (v0.30).
- **Listary 6.3** keeps the two killer UX ideas no launcher on Windows otherwise has: **QuickSwitch**
  (type a filename in any open/save dialog) and **type-to-search in Explorer**, plus double-Ctrl summon,
  Ctrl+number select, custom commands with parameters, custom per-filetype actions with hotkeys.
- **Keypirinha** (abandoned since v2.26) is the patron saint of this project's philosophy — INI-driven,
  portable, zero-CPU-idle, machine-vs-user config files — and its audience is actively migrating;
  its utility commands (hash, base64/UUID, base conversion, env-var lookup) are cheap for TypeBox to
  absorb.
- **Drop-down console heritage**: WT quake mode / ConEmu / Guake / WTQ — expectations are slide
  animation with configurable time, auto-hide on focus loss (we have it), full-width top geometry,
  and "appear on the monitor where the mouse/cursor is".
- **r/commandline** leans toward no-index, lightweight, offline tools — a privacy/drift differentiator
  TypeBox can market against indexer-heavy rivals.

## Top 10 priority table (value ÷ effort)

| Rank | # | Title | Value | Effort | Ver |
|---|---|---|---|---|---|
| 1 | 3 | Numbered quick-select rows (Alt+1…9) | high | S | v0.11.x |
| 2 | 14 | History prefix search (bash-style) | high | S | v0.11.x |
| 3 | 12 | Double-tap-modifier summon | high | S | v0.11.x |
| 4 | 29 | Copy path / open containing folder | high | S | v0.11.x |
| 5 | 27 | Opt-in autostart (tray + settings) | high | S | v0.11.x |
| 6 | 43 | Machine/user overlay config files | high | S | v0.11.x |
| 7 | 48 | Danger confirmation gate | med-high | S | v0.11.x |
| 8 | 4 | Web-search quicklinks `[WebSearch]` | high | S–M | v0.11.x |
| 9 | 1 | Fuzzy match engine (fzf-style) | high | M | v0.11.x |
| 10 | 5 | `~run` — Win+R/Settings verb catalog | high | M | v0.12.x |

The first seven are all single-day-class wins that close parity gaps with Flow/Wox/Raycast in one
release; i18n keys must be appended at the end of `src/i18n.h` `enum StrId` / `kKeyNames` / `kEn`
for every idea flagged **(i18n)**.

## All 50 ideas

Effort: S ≈ ≤2 days · M ≈ ≤1 week · L ≈ multi-week · XL ≈ XL (flagged per item).
**(i18n)** = needs new key(s) appended at end of `src/i18n.h` (`enum StrId`, `kKeyNames`, `kEn`).

### Search & launch

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 1 | **Fuzzy match engine** *(planned)* | fzf-style subsequence matcher scoring prefix/word-start/camelCase, shared by dropdown, Tab completion and alias lookup; `[Type] Fuzzy=1` toggle, exact matches always keep priority (lesson from CmdPal #46096 backlash). Pure code, no deps. | PowerToys 0.95 fuzzy matcher; Wox v2 fzf algorithm; Flow/Wox/Alfred fuzzy UX | high | M | v0.11.x |
| 2 | **Adaptive usage ranking** | Store `query→result` counts in portable `Stats.dat`; boost previously-chosen result for a similar query (decay over time). `[History] Adaptive=1`, `/stats` reset. | CmdPal #43174 (no learning), Raycast v0.30 term-memory, Listary habit ranking | high | M | v0.11.x |
| 3 | **Numbered quick-select** | First 9 dropdown rows show `1..9` in accent; **Alt+1…9** (or Ctrl) runs instantly, matches Flow/Listary muscle memory. Draw digits in existing row painter. | Flow Alt+number; Listary Ctrl+number; Wox v2.4 Quick Select; CmdPal shelf Alt+1…9 | high | S | v0.11.x |
| 4 | **Web-search quicklinks** | `[WebSearch] g=https://google.com/search?q=%s` keyword templates; `g foo bar` opens default browser. Optional `{clipboard}` token. Ship sensible defaults (g, yt, gh). | Raycast Quicklinks; Listary `g …`; Keypirinha site search; Flow WebSearch | high | S–M | v0.11.x **(i18n)** |
| 5 | **`~run` verb catalog** | `~run` (and bare `run ` prefix) lists classic Win+R verbs + `ms-settings:` deep links with friendly localized descriptions and run-count ordering; pure table + registry *read* of installed apps' `OpenWithProgids`. | Raycast "Run" command (v0.41); Microsoft's new Run dialog 2026; Flow WindowsSettings; Wox settings search | high | M | v0.12.x **(i18n)** |
| 6 | **App catalog scan** | Index Start-Menu shortcuts + recursive `exe` scan of `[Path]` folders (incremental via USN or file-time cache in `Apps.dat`) so portable-app exes launch by bare name — the Launchy/Keypirinha use case CmdPal lost. | CmdPal #39399 (LiberKey request); Launchy legacy; Keypirinha catalog | med | M | v0.12.x |
| 7 | **Window switcher** | `~w <word>` lists open windows (EnumWindows, titles) across virtual desktops; Enter = SetForegroundWindow (+ IVirtualDesktopManager switch), Alt+Enter closes. Rows reuse dropdown painter. | Flow Window Walker; PowerToys Window Hopper; Raycast Window Management; Keypirinha app-switch | high | L | v0.13.x **(i18n)** |
| 8 | **Clipboard history** | Opt-in `[Clipboard] Enable=0` watcher (AddClipboardFormatListener): text + images in portable `Clipboard.dat`; `cb ` prefix searches rows, Enter copies, Ctrl+Enter pastes via SendInput. Per-type retention days, exclude-secure-window rule (Alfred's practice), `/clearclipboard`. | Alfred Clipboard History; Raycast Clipboard History; Flow Clipboard+ (#2963); Wox clipboard | high | L | v1.x **(i18n)** |
| 9 | **Snippets + expansion** | `[Snippets] ds=,ds|{isodate}` rows (name|keyword|text) with placeholders `{date} {time} {clipboard} {cursor}`; `snip name` copies, and optional global auto-expansion (WH_KEYBOARD_LL) guarded by secure-field detection and app-exclude list mirroring Ignores.ini syntax. | Alfred Snippets + auto-expansion; Raycast snippet expansion (v0.38); ueli workflows | high | L | v0.12.x **(i18n)** |
| 10 | **Offline calculator + conversions** | Inline numeric detection: expression evaluated in console (result shown in status strip + copied by Ctrl+Shift+C), plus `0x1f→bin/hex`, case-convert, base64 — the abandoned-Keypirinha utility set, no deps (BCrypt hash can ride in a sample plugin, #38). | CmdPal #38528 unit converter request; Keypirinha hash/base-conv; Raycast calc; PowerToys Run calc history | med | M | v0.13.x |
| 11 | **Bookmarks & open-tab search** | Core-parse Chrome/Edge/Firefox `Bookmarks` JSON (offline file read) under `b ` keyword; open-tab search via UI Automation as an example *plugin* (keeps UIA COM out of core). | Flow #2069/#3326 (most-loved-requested), Flow BrowserTabs plugin 2025, Wox bookmarks | med | M (core) / L (tabs) | v0.13.x **(i18n)** |

### Console & input

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 12 | **Double-tap modifier summon** | Optional second summon mode: double-tap Ctrl/Shift/Alt within N ms (`[HotKey] DoubleTap=ctrl`), low-level hook timer, coexists with RegisterHotKey combo. | Listary double-Ctrl; Wox v2 beta.2; Raycast double-tap shortcuts (v0.33) | high | S | v0.11.x **(i18n)** |
| 13 | **Per-command hotkeys & second hotkey** | Allow alias/sysalias/`/cmd` lines to carry a `hotkey=` field (tray re-registers all); second global hotkey for "show + paste-and-run". | Raycast per-command hotkeys (v0.36); Flow hotkey tab; Listary action hotkeys | high | M | v0.12.x **(i18n)** |
| 14 | **History prefix search** | ↑/↓ filter history to lines starting with (or fuzzy-containing) the current prefix — bash history-search; `Ctrl+R` opens dedicated filtered mode with count `3/17` in strip. | bash/zsh; Raycast ArrowUp root-search recall; Listary recent-first habits | high | S | v0.11.x |
| 15 | **Inline ghost hint** | Dim "ghost text" of best completion after caret (Tab/End accepts) — cheaper than the full Hint popup, keeps single-line purity. `[Type] GhostHint=1`. | Executor/Wox hint popups; CmdPal minimal-mode sentiment (#38423) | med | M | v0.12.x |
| 16 | **Command chaining & worksets** | `a && b ; c` executes sequentially in one line; `&workname` saved sequence in `[Worksets]` ("open all work apps"), editable in a small settings page. | ueli Workflows; Listary custom commands; AHK macros | med | M | v0.13.x **(i18n)** |
| 17 | **Drag-to-move + position memory** | Drag console anywhere (WM_NCHITTEST→HTCAPTION on static areas), snap to edges, save per-monitor geometry in `[Coordinates]` (replaces the 📋 "can't move popup" complaint users have against CmdPal). | CmdPal can't-drag complaint (issue #46096 review); ConEmu/WT quake geometry | med | S | v0.11.x |
| 18 | **Monitor choice** | `[Show] Monitor=cursor|primary|last|N` — show on the monitor under the caret like quake terminals do. | WTQ multi-monitor behavior; CmdPal multi-monitor Dock | med | S | v0.11.x |
| 19 | **Run as administrator** | **Ctrl+Enter** on any line re-runs it via `ShellExecuteEx` `runas` verb (UAC prompt); dropdown row action menu offers it too; `[Exec] AllowElevate` toggle. | Flow shell-as-admin; PowerToys Run "Create and run as administrator"; Keypirinha runas | med-high | S–M | v0.12.x **(i18n)** |
| 20 | **Offline paste transforms** | `p ` mini-Advanced-Paste: clipboard → strip formatting / trim / URL-decode / JSON-format / UPPER / snake_case, result copies to clipboard + optionally to console. Pure C++, per-action `[Paste]` toggles. | PowerToys Advanced Paste (top utility 2025-26) minus AI | med | M | v0.13.x **(i18n)** |

### Visual

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 21 | **Quake presets + slide animation** | `[Show] Edge=top|bottom|center`, `WidthPercent` *(closes planned 📋 ConsoleWidth-Percent)*, full-width Guake-style preset, slide-in animation with `[Show] AnimMs=120` (frame-timer SetWindowPos) and `ReduceMotion=1` opt-out. | Guake/Yakuake/ConEmu quake + animation time; WT quake mode; Flow animation-speed setting | high | M | v0.12.x **(i18n)** |
| 22 | **Minimal mode** | Search-box-only chrome until typing starts (dropdown collapsed, strip hidden), Ctrl+\ toggles; the "thin bar that grows" CmdPal users begged for (#38423). | CmdPal #38423; Keypirinha minimalism; r/commandline lightweight asks | med | S | v0.12.x |
| 23 | **Row type icons** | GDI-drawn per-class glyphs (alias `»`, `~` diamond, exe gear, URL globe, file page) left of dropdown text — "visual anchors" extended from lamps to rows; no bitmaps. | Raycast/Flow/Wox icon-first results; Alfred default results | med | M | v0.12.x |
| 24 | **Auto theme switch** | `[Theme] Auto=time|sun|system` — swap preset by clock range (or sunrise/sunset offset table) or registry *read* of `AppsUseLightTheme`; pairs with cozy branding, zero UI cost. | PowerToys Light Switch (0.95 new utility); Wox Omarchy auto-theme | med | S–M | v0.13.x **(i18n)** |
| 25 | **Shareable themes** | `themes\<name>.theme.ini` (colors + font + strip chips) loaded at scan; **Save current as…** button on Theme page; zip-installable like Flow themes. | Flow Theme Builder; Wox theme store; Alfred themes; Keypirinha skins | med | S | v0.12.x **(i18n)** |
| 26 | **Accessibility pack** | `high-contrast` + `large-text` presets (compiled into the 7+2 scheme list), reduced-motion switch, larger click targets on strip; document keyboard-only path in /help. | PowerToys accessibility push (Mouse Button Lock, CursorHide 0.102) | med-low | S | v0.12.x **(i18n)** |

### System integration

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 27 | **Opt-in autostart** *(planned)* | Tray checkbox + Appearance toggle writing one HKCU `...\Run` value (explicit opt-in per registry policy, status shown by `/checkconfig`). | TypeAndRun autostart; every launcher has it; CmdPal "disable hotkey in fullscreen" companion ask | high | S | v0.11.x **(i18n)** |
| 28 | **"Add to TypeBox" shell menu** *(planned)* | Opt-in HKCU\*\shell\`Add to TypeBox` for files/folders: appends an alias row via `TypeBox.exe --add "<path>"` to `[Aliases]` (comments survive). Revives TypeAndRun's ShellInt. | TypeAndRun right-click "Add to TypeAndRun"; Listary custom actions | med | M | v0.13.x **(i18n)** |
| 29 | **Copy path / open folder** | With a dropdown row selected: **Ctrl+C** copy name, **Ctrl+Shift+C** copy full path, **Ctrl+Enter** open containing folder (explorer `/select`). Zero new UI. | Flow hotkeys (Ctrl+C/Ctrl+Shift+C/Ctrl+Enter); Listary Ctrl+O actions | high | S | v0.11.x |
| 30 | **System command pack** | New built-in `~sysaliases`: `~volume(±)`, `~mute`, `~brightness ±` (WMI/DDC read-only fallback), `~wifi`, `~bt`, `~dnd`, `~recycle`, `~hideclips`, `~wallpaper` — each ≤30 lines of Win32 or a `ms-settings:` jump; table extensible by users. | Flow System Commands; Listary system cmds; PowerToys PowerDisplay/Light Switch; Raycast system ext | med | M | v0.12.x **(i18n)** |
| 31 | **Game mode** | Auto-suppress the hotkey when a fullscreen/exclusive app is foreground (monitor `FS_` states) + tray **Game mode** toggle + `[Show] SuppressFullscreen=1`. | Flow Game Mode (Ctrl+F12); Listary fullscreen-disable option; CmdPal fullscreen ask (#38423 thread) | med | S | v0.12.x **(i18n)** |
| 32 | **Process killer** | `kill <name>` lists running processes (Toolhelp32 snapshot) in dropdown; Enter terminates (tree-aware, asks via danger-gate #48 for protected names), row shows CPU/mem basics. | Flow Process plugin; Task Manager shortcut workflows; Listary "end process" cmd | med | M | v0.13.x **(i18n)** |
| 33 | **File-dialog QuickSwitch** | While a standard open/save dialog is foreground, type hotkey → console filters filenames in dialog's current folder (UIA read), Enter inserts the full path. The Listary killer feature, absent from every OSS launcher. | Listary QuickSwitch (its #1 selling point in 2025-26 reviews) | high (wow-factor) | XL | v1.x **(i18n)** |
| 34 | **`/checkupdate`** | Explicit user-typed command fetches GitHub latest-release JSON (WinHTTP), logs version + shows balloon with browser link; never auto-runs (offline-first policy), `[Update] Check=manual|startup-ask`. | PowerToys/Flow/Wox/Raycast all ship updaters; community expectation | med | S | v0.12.x **(i18n)** |

### Plugins & extensibility

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 35 | **Unicode plugin ABI v2** *(planned)* | Host advertises `HOST_FLAGS_UNICODE`; optional `RunStringW/GetInfoW/GetStringETW` (UTF-16) exports preferred over ANSI; version handshake logged at `/load` (`plugin calc: abi v2 utf-16`). Unblocks CJK-ful plugins. | Already promised in PLUGIN_API.md; Wox/Flow Unicode-native plugins | high | M | v0.12.x |
| 36 | **Plugin config bridge** | Host exports calls `TbGetIniInt/Str`/`TbSetIniStr` (passed in a v2 `Load2(HWND, hostapi)`), persisting plugin options into `[Plugin:<name>]` in TypeBox.ini — portable per-plugin settings without each DLL writing its own file. | Flow JSON-RPC SettingsTemplate; Keypirinha package INI | med | M | v0.12.x |
| 37 | **Plugin trust manifest** | Optional `plugins\<dll>.ini` manifest (name/version/publisher/sha256/permissions); host prompts on first use of a new or changed DLL, records decision in `[Trust]`; `/checkconfig` lists untrusted. No network — pins are compared offline. | Flow VirusTotal store policy; Wox store vetting conversations | med | M | v0.13.x **(i18n)** |
| 38 | **First-party example plugin pack** | Ship 4 small GPL examples next to calc: `hash` (MD5/SHA via bcrypt.lib — a *plugin* dep, allowed), `convert` (base64/base/hex), `uuid+pwgen`, `emoji` (compiled table, copies to clipboard). Doubles as ABI docs. | Keypirinha built-ins (its most-loved features post-abandonment); Flow Emoji/Fancy-Emoji | med | M | v0.12.x **(i18n)** (console strings only via returned text) |
| 39 | **Importer from other launchers** | `/import-raycast <rayconfig>`, `/import-flow <settings.json>` → converts Quicklinks→`[WebSearch]`, snippets→`[Snippets]`, app hotkeys→#13; report per-object mapping in log. | Wox v2.4.9 "Import Raycast settings"; Raycast export/import; migration = growth lever | med | M | v0.13.x **(i18n)** |
| 40 | **MCP bridge (optional plugin)** | Localhost-only named-pipe/HTTP MCP server plugin exposing `launch`, `run_alias`, `list_aliases` tools so AI agents/IDEs can drive TypeBox; off by default, `[MCP]` section, loopback only. | Wox MCP server (2025 flagship feature); AI-tooling momentum | low-med | L | v1.x |
| 41 | **Row action framework** | Right-click / **Ctrl+K** on any dropdown row opens a themed action list (Run, Run as admin #19, Copy path #29, Open folder, Pin to top `[Pins]`, Delete from history); plugins can contribute actions via optional `GetRowActions`. | Raycast Action Panel (Ctrl+K); Listary Actions menu; Flow preview actions | med | L | v0.13.x **(i18n)** |

### Settings & portability

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 42 | **Profiles** *(planned)* | `[Profiles] Active=work` + per-profile sub-files `profiles\work.ini` merged over TypeBox.ini (aliases/history/dropdown/theme override); tray submenu + `/profile work` switch live. | FEATURES "beyond" list; ConEmu task profiles; ueli per-machine flows | high | M | v0.12.x **(i18n)** |
| 43 | **Machine/user overlay config** | Auto-load optional `TypeBox.<PCNAME>.ini` and/or `TypeBox.<user>.ini` next to the main INI (same schema, wins on conflict) — one synced folder, per-machine hotkeys/paths, no secret leakage. Keypirinha's best idea. | Keypirinha machine/user-specific configs | high | S | v0.11.x |
| 44 | **`/checkconfig` v2** | Schema-aware lint: unknown/misplaced keys (edit-distance typo suggestions), bad COLORREF/VK values, dead `[Path]` dirs, duplicate aliases — output to console + log with line numbers. | Keypirinha users' "INI is unforgiving" reviews; PowerToys settings search/validation ethos | med | S–M | v0.11.x **(i18n)** |
| 45 | **Settings search** | Type-to-filter box atop the 14-page dialog mapping to localized page/control labels; Enter jumps to page. Uses existing i18n strings as the index. | PowerToys Settings search (now CLI-scriptable); Wox/Flow settings search | med | M | v0.13.x **(i18n)** |
| 46 | **Portable bundle export** | `/export all <zip>` packs TypeBox.ini + History + lang + Sounds + Plugins manifests (+Ignores); `/import all` restores with backup + per-file report. Uses `zipfldr`/`Shell.Application` automation — no zlib dep in core. | ConEmu portable culture; Flow/Wox single-click migration; PowerToys DSC export trend (0.102) | med | M | v0.12.x **(i18n)** |
| 47 | **INI file watcher** | `FindNextChangeNotification` (or debounced timestamp poll) on TypeBox.ini → auto-`/reread` after 1 s quiet — hands-free with Dropbox/Syncthing folders; `[Config] WatchFile=1`. | ConEmu/Keypirinha file-driven configs; users edit INI externally today (tests do it!) | med | S | v0.12.x |

### Safety

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 48 | **Danger confirmation gate** | `[Safety] Confirm=~reboot,~logoff,kill,*del*; destructive lines (red traffic light + pattern list) show an inline "Enter to confirm / Esc to cancel" row instead of firing; Ctrl+Enter always bypasses. | PowerToys Run/File Explorer confirm patterns; Listary shutdown-cmd caution; offline analog of browser `about:blank#blocked` safety | med-high | S | v0.11.x **(i18n)** |
| 49 | **History secret redaction** | `Redact.ini` (Ignores-style wildcards + regex-lite): any executed line matching is stored as `alias ****`; live line unaffected. Covers `pw=…` args and token-looking strings (`[A-Za-z0-9]{32,}`). | Launchy/clipboard tools' password gaps; Alfred secure-field exclusion philosophy; privacy-first r/commandline sentiment | med | S | v0.12.x |
| 50 | **INI auto-backup + `/restore`** | Before every OK/Apply and `/import`, snapshot TypeBox.ini to `Backups\TypeBox-YYYYMMDD-HHMMSS-N.ini` (keep `[Config] Backups=10`); `/restore N` lists and rolls back. No more one-bad-Apply heartbreak. | Flow 1.20.0 config-breaking incidents (#3643); git-like safety; ConEmu backup-file habit | med | S | v0.11.x **(i18n)** |

Also considered and deliberately dropped as near-duplicates: shell denylist (subsumed by #48's
pattern gate), standalone row-action copy-path commands (folded into #41 as built-ins), status-strip
system widgets vs `/sys` (keep `/sys`), hint popup vs ghost hint (#15 chosen as the cheaper single-line
form), AI/LLM box (QuickChat-style) — deferred to a plugin per offline-first policy.

### i18n policy reminder

Every idea flagged **(i18n)** adds user-visible strings (settings controls, tray items, confirmation
and dropdown labels). Per project policy, append the new keys at the **end** of `src/i18n.h`
`enum StrId` (before `STR_LAST`), `kKeyNames[]`, and `kEn` — never insert mid-enum; non-English
packs fall back per-string and community packs override via `lang\<code>.ini`.
