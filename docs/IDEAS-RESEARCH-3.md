# TypeBox - research-backed feature backlog #3 (ideas #201-300)

Research date: 2026-11-16 · Baseline: **v0.19.1** (verified against [FEATURES.md](FEATURES.md),
[PLAN.md](PLAN.md), [ROADMAP-50.md](ROADMAP-50.md), [IDEAS-RESEARCH.md](IDEAS-RESEARCH.md),
[IDEAS-RESEARCH-2.md](IDEAS-RESEARCH-2.md), [PLUGIN_GALLERY.md](PLUGIN_GALLERY.md),
[UX-REVIEW.md](UX-REVIEW.md), [CONFIG.md](CONFIG.md), `CHANGELOG.md`,
`src/main.cpp`, `src/i18n.h`, and the actual v0.19.1 release artifact).

This file continues the numbering of [IDEAS-RESEARCH.md](IDEAS-RESEARCH.md) (#1-50) and
[IDEAS-RESEARCH-2.md](IDEAS-RESEARCH-2.md) (#101-200). **Every idea below (#201-300) was grep-checked
against those two docs + [ROADMAP-50.md](ROADMAP-50.md) (#1-82) + the shipped v0.19.1 feature set
(fuzzy, frecency, app index, web quicklinks, calc+, scripts folder, danger gate, `su`/`sudo`, themes &
font studio, context menu, and the eight shipped plugins `calc units codec vol cliphist taskman winlist pwgen`)
and is a genuine non-duplicate.** Where an idea complements an existing item it says so explicitly
(e.g. "beyond ROADMAP #16", "beyond RI2 #168") so the maintainer can merge cleanly. Ideas that merely
restate shipped behavior were cut during dedupe (hash plugin → verify-a-checksum-file; case-convert →
slugify+translit; sandbox-open → dropped, RI2 #155).

**Constraints honored throughout:** single-file C++/Win32 host; no external dependency in the core exe
(anything heavier rides in an *optional plugin* or shells out to a tool the user already has, user-invoked
only); everything persisted in portable `TypeBox.ini` / `TypeBox.local.ini`; **registry only behind explicit
opt-in**; **offline-first** (network only in a user-typed command); i18n keys appended at the **end** of
`src/i18n.h` (`enum StrId` before `STR_LAST`, `kKeyNames[]`, `kEn`). Rows marked **(i18n)** add new
user-visible strings.

Effort: **S** ≤ 2 days · **M** ≤ 1 week · **L** multi-week · **XL** research-heavy. Value: **H/M/L**.

## Method - what the 2026 research surfaced (so ideas aren't invented from thin air)

- **PowerToys → Command Palette** (successor to PowerToys Run) keeps getting asks the TypeBox host
  can answer cheaply: run results *as admin* per-target, window positioning presets, per-desktop
  behavior, and a proper history search mode. Its plugin store normalized the "row actions + declare
  your hotkey in the manifest" pattern TypeBox already half-has (#13/#41/#167) - the gap is the
  developer experience around it (debug view, hot-reload, collision resolution), which is what #266-#275 attack.
- **Flow Launcher / Wox / ueli** plugin-dev threads: the recurring complaints are *silent* plugin crashes,
  no way to see what a plugin is answering, DLLs locked between edits, and no data folder convention -
  → #267 hot-reload, #270 plugdebug, #288 minidumps, #275 PluginsData.
- **Raycast extension store** made clipboard-manager-as-a-first-class-window the expected UX; TypeBox
  cliphist 1.1 shipped the list, so the ring-picker hotkey window and forced *typing* (for apps that
  block programmatic paste) are the natural next cm (#260-#262). Its "Quick Launcher + args memory"
  behaviors also map to #203/#204.
- **Listary reviews** (the feature TypeAndRun's audience actually misses): dialog-aware file find and
  per-folder terminal - already claimed by RI1 #33 / RI2 #107; the remaining Listary-adjacent gaps
  people name are clipboard quick-paste windows and bulk rename - → #260, #231.
- **AutoHotkey / Wo-Launcher forums**: long-standing asks for macro *recording* (not hand-written macros),
  `at`-style persistent scheduling, stay-awake, Wake-on-LAN, and a PIN-lock for shared machines -
  → #202, #299, #230, #238, #284.
- **r/todayilearned-grade utility traffic** (the `~dice`/`~coin` RI2 toys got the best reactions):
  console-rendered QR codes, figlet banners, `jwt`, `cron 0 3 * * 1 → "Mondays 03:00"`, epoch
  converter - small offline plugins, zero deps, screenshot-friendly: #243-#251, #300.
- **Security blogs (ClickFix/AV-SCRIPT wave 2025-26)** confirmed the double-extension + RLO filename
  vector is under-addressed by launchers (TypeBox has payload-guarding via ROADMAP #72 but not the
  filename anatomy itself) - → #282, and the trusted-folder green light #283.

## Search, history & learning

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 201 | Ctrl+R history *mode* | Beyond ROADMAP #12 / RI1 #14's prefix filter: a dedicated modal search - fuzzy-scored over history, grouped by day with relative stamps ("today 14:02"), row actions (Enter = run, Ctrl+C copy, Tab = edit-as-new-line, Del = forget), `Ctrl+R` again cycles matches, Esc returns the typed text untouched. Reuses the dropdown painter + existing history store. *(i18n)* | bash/fish users; CmdPal history-search asks | H | M | v0.20.x |
| 202 | Macro **recorder** | `~macro rec <name>` starts capturing executed lines (alias/plugin/shell, not internal `/cmds`), `~macro stop` saves to `[Macros]`, `~macro run <name>` replays with a 150 ms gap and a progress line; `~macro del`. Complements ROADMAP #16's hand-written macros by *learning* them from what you actually ran. *(i18n)* | AutoHotkey crowd; "repeat my morning routine" | H | M | v0.21.x |
| 203 | Per-argument value memory | Each `%1..%n` slot of an alias remembers its last 8 values (`Usage.dat` key `alias#slot`); when the caret is on a `%n`, ↑/↓ cycles remembered values. Distinct from RI2 #116 (which only *hints* at slots) - this makes re-running `sync <dir>` with yesterday's dir one keystroke. | Raycast args memory; own ROADMAP #9 | H | S | v0.20.x |
| 204 | Guided `%n` wizard | Enter on an alias with unfilled `%1..%n` can switch to step mode (`[Type] GuidedArgs=0/1`): one clean prompt per parameter with its remembered values as dropdown rows, Back/Next, final assembled command previewed before run. No bash line-editing skills required - the TypeAndRun beginner's complaint. | TypeAndRun forum; Keypirinha's prompt mode | M | M | v0.21.x |
| 205 | Command notes | `note <cmd-or-alias> <text>` attaches a sticky note to a command (stored `[Notes]`); the note shows in the dropdown's second line and in `Ctrl+H` hover; searchable so "why do I run this with --no-verify" is findable. Distinct from snippets - notes describe, snippets expand. | personal wiki-in-terminal habit; Flow "notes plugin" | M | S | v0.22.x |
| 206 | History tags & bookmarks | `tag git <name>` / `tag rm` on the last executed line, `taglist`, and `git:`-style filtering (typing `tag:git` shows only tagged rows); bookmarked rows get a `★` chip and survive `Max` trimming. Beyond ROADMAP #28 pinned rows (which are positional, not semantic). *(i18n)* | taskwarrior-lite; Obsidian users' launcher | M | M | v0.22.x |
| 207 | History CSV export/import | `/export history <file.csv>` (timestamp, line, class, run-count) and `/import history <file.csv>` with dedupe + optional redaction pass via the RI1 #49 rules - migration between machines and spreadsheet triage without touching `History.dat` format internals. | ROADMAP #46 export family; analysts | M | S | v0.22.x |
| 208 | `/stats` dashboard | Read-only console report: launches/day sparkline for 14 days, top-10 commands with counts, class mix (alias/plugin/shell), median keystrokes-to-launch, plugin hit table from #270 counters. `[Stats]` retention, `/stats reset`. Complements RI1 #2 (which *uses* Stats.dat; this *shows* it) without stepping on RI2 #196 streaks. | Power User "how much do I actually use this" | M | S | v0.22.x |
| 209 | `/duplicates` alias merger | Scans `[Aliases]`, groups entries whose target (resolved exe/URL/script) is identical, lists them with run-counts and offers "keep N, archive rest" into `[Archived]` (comments survive). Beyond ROADMAP #46 dead-alias checker - same plumbing, opposite problem: aliases that are *too* alive. | config-rot on long-lived installs | M | S | v0.22.x |
| 210 | Alias groups | `[Groups] dev=git,github,vscode,wt` - typing `dev ` (or `Ctrl+G` to cycle) filters the dropdown to the group only; group rows show a `⌗` chip; `/groups` lists. Ad-hoc vs #223 stars: groups are declared+shareable, stars are personal+organic. | zellij/terminal profiles audience | M | S | v0.22.x |

## Console, dropdown & input

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 211 | Edge-dock reveal mode | `[Show] EdgeDock=top,bottom,left+AutoHideSecs=0`: console tucks off-screen with a 2 px sliver; mouse hover at the edge peeks it, summon (hotkey) fully opens; distinct from RI2 #128's floating button (no extra window, native move loop intact). | Guake/Yakuake muscle memory | M | M | v0.21.x |
| 212 | Idle auto-hide | `[Show] AutoHideSecs=30` hides the console when it loses focus and the timer expires (never while a dropdown action is mid-click or a drag is in progress - guard with the existing `g_dragged` state); Esc-to-blur cancels. The "it's still on my screen" fix. | Quake-console habit; CmdPal auto-hide requests | M | S | v0.20.x |
| 213 | Last-query restore | On restart the console optionally reopens with the *previous session's* typed line and selected row restored (`[Show] RestoreQuery=0` default), so an interrupted `sync <bigdir>` is one Enter away. Crash-recovery story pairs with #288. | Wox session-restore | L | S | v0.22.x |
| 214 | Multi-monitor follow-focus | Beyond RI2 #119 (independent consoles per monitor): `[Show] FollowFocus=1` - the single console's next summon appears centered on the monitor that owns the foreground window (MonitorFromWindow), keeping one identity. | dual-monitor Reddit launcher threads | M | S | v0.21.x |
| 215 | Status-strip layout editor | `[Strip] Items=clock,execclass,admin,plugins,count` reorderable + per-chip visibility, widths auto-fit; Settings Appearance page gets an Up/Down list. Beyond ROADMAP #29 clock-chip options and #37 battery/net chips - the *layout* was never exposed. | own shipped strip; Raycast menu-bar config | M | M | v0.21.x |
| 216 | DWM border & caption tint | Per-theme `BorderColor` applied via `DWMWA_BORDER_COLOR` and caption via `DWMWA_CAPTION_COLOR` on Win11 (no-op fallthrough on Win10), so the OS chrome matches the theme instead of defaulting to accent. Complements ROADMAP #21 rounded corners (same DWM attribute family). | own theme studio; Win11 aesthetic threads | M | S | v0.20.x |
| 217 | Font fallback cascade | `[Font] Cascade=cascadia.ttf;NotoSansCJK;tahoma` - per-codepoint fallback in the dropdown/console painter + a preview line in the Font studio showing "Привет 世界 🙂" rendered across the cascade. Current font studio sets one family; CJK rows still tofu. | own font studio gap vs #102 CJK search | M | L | v0.23.x |
| 218 | Low-vision console pack | `[Show] CaretWidth=2`, thicker selection highlight color token, "flash" the console border 3× on successful execution, larger strip font independent of dropdown. One Settings group, all appended to `[Show]`. | low-vision AHK users | M | S | backlog |
| 219 | Result earcons | Three tiny built-in waveforms (success/warn/danger) chosen by the traffic-light class on execution completion, `[Sounds] Earcons=1`; complements the shipped per-event Sounds - the *outcome*, not the event, becomes audible. | screen-reader adjunct to RI2 #185 | M | S | backlog |
| 220 | Sound quiet hours | `[Sounds] Quiet=22:00-07:30` mutes all playback (earcons, keystroke, exec) inside the window, tray tooltip shows the muted state; per-sound `!` prefix to exempt (exec alerts stay). | night-shift users | L | S | backlog |
| 221 | RTL layout + per-language fonts | `[Language] RTL` locales (ar, he) flip console/dropdown/Settings via `WS_EX_LAYOUTRTL` + mirrored painting; `[UI] Font.ar=` per-locale font override. First right-to-left pass TypeAndRun never had. *(i18n)* | AHK Arabic community; RI2 #190 tooling side | M | L | v1.0.x |
| 222 | Per-page language override | UI stays English while console/history/errors follow `[Language] Console=ru` (or reverse) - admins keep docs/screenshots EN, keep daily messages native. Same string tables, one more lookup scope. *(i18n)* | bilingual IT support desks | M | S | v0.23.x |
| 223 | Star rows | `Ctrl+S` on a dropdown row toggles a personal star; typing `favs ` (or a `★` trigger char) filters to starred only; order = manual. Ad-hoc complement to #210 declared groups and ROADMAP #28 pins (survives history trim, lives in `[Stars]`). | browser-bookmark muscle memory | M | S | v0.21.x |
| 224 | Multi-select actions | `Ctrl+Space`/Ctrl+Click toggles rows; footer chip "3 selected"; Enter = execute all sequentially (ad-hoc vs RI1 #16 saved worksets), Ctrl+C = copy all paths/values, Ctrl+P = pin all to `[Pins]`. Reuses the row painter's selected state. | Everything/Explorer multiselect expectation | M | M | v0.22.x |
| 225 | Dropdown section headers | Optional `[DropDown] Sections=1`: gray non-selectable headers ("plugins · 4", "aliases · 12", "history · 6") between result classes with `Tab` jumping to next section - keeps fuzzy cross-class ranking *and* scanability (the CmdPal "what am I looking at" complaint). | Flow/Wox sectioned lists | M | M | v0.21.x |

## Window & display

| # | Title | Spec | Inspiration | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 226 | Snap & position presets | `pos left\|right\|top\|bottom\|tl\|tr\|bl\|br\|center\|q1..q4` sizes/places the console on the current monitor (work-area aware), `[Positions] name=x,y,w,h,mon` named presets + `pos name`; next to the shipped drag-move (#82) so a misplaced window is one command, still INI-homed on restart. | PowerToys FancyZones vocabulary | H | S | v0.20.x |
| 227 | Mini HUD window | Borderless always-on-top 1-line HUD (`hud` toggles; `[Hud] Items=clock,timer,clipcount`): shows ticking timer from the timer plugin, clipboard count from cliphist, clock; click summons the console, Esc hides. Separate HWND, click-through option, topmost like the console. Beyond ROADMAP #29/#37 chips - visible *without* summoning. | Raycast HUD; streamer decks | M | M | v0.21.x |
| 228 | Per-desktop summon rules | `[Desktops] 2=Suppress,4=AutoHide=10` keyed to the virtual desktop index (IVirtualDesktopManager): console stays hidden on the meeting/desktop-2, auto-hides on the Kiosk desktop; tray badge explains why the hotkey is quiet. Distinct axis from RI2 #119 (monitors). | desk-jockey workflow threads | M | M | v0.23.x |
| 229 | Virtual-desktop *goto* | `winlist` extension: `wingoto <name>` - if the target window lives on another virtual desktop, switch to it (IVirtualDesktopManager::SwitchDesktop) before SetForegroundWindow; today winlist foregrounds within the current desktop only. Beyond shipped winlist plugin + RI1 #7. | Win11 multi-desktop pain | H | M | v0.20.x |
| 230 | Stay-awake toggle | `stayawake [min]` - `SetThreadExecutionState(ES_CONTINUOUS\|ES_SYSTEM_REQUIRED\|ES_DISPLAY_REQUIRED?)`, bare = indefinite with the countdown in the strip chip, `stayawake off` clears; survives hide, clears on exit. The `caffeine` everyone installs separately. | caffeine/amphetamine users | H | S | v0.20.x |

## File & system utilities

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 231 | Bulk rename | `ren2 <folder> <regex> -> <template>` lists old→new rows with collision detection, Enter applies (undo file `ren2.undo` written first); numbered `{$1}` templates, preview colors risky rows. Beyond RI2 #148 mini file verbs (single-file ops). | Directory Opus/LittleWizard users | H | M | v0.21.x |
| 232 | Duplicate-file finder | `dupes <folder>` - size buckets then SHA-256 verify, groups listed with dates, `keep 1` per group deletes rest via recycle-bin (SHFileOperation FO_DELETE|FOF_ALLOWUNDO) + danger gate. | declutter Reddit; own #151 du users | M | M | v0.23.x |
| 233 | Disk drill-in bars | Beyond RI2 #151 `du`: ranked horizontal bars (`▓▓▓▓▓░ 41% 12.3 GB node_modules`), Enter descends into the biggest child, Backspace goes up, `du --json file` export. Makes the folder-size question a conversation, not a report. | WinDirStat users wanting a keyboard path | H | M | v0.22.x |
| 234 | Temp cleaner | `clean temp` aggregates %TEMP%, `%LOCALAPPDATA%\Temp`, browser caches (path table), Recycle Bin (links to #30's verb); lists bytes per bucket, Enter deletes with in-use-skip, report line per bucket. Danger gate mandatory, nothing above the table is touched. | CCleaner refugees (portability angle) | M | S | v0.22.x |
| 235 | Startup manager | `startup` lists Run/RunOnce keys (read-only registry) + Startup folders with publisher/path columns; `startup off <n>` exports the value to `[StartupSaved]` before removing (one-click `startup restore <n>`); registry *writes* only per-item with confirm, matching the opt-in policy. | Autoruns' keyboard users | H | M | v0.21.x |
| 236 | Task Scheduler viewer | `tasks [filter]` shells `schtasks /fo csv /v` read-side into dropdown rows (name/state/last result/next run), Enter = `schtasks /run`, `tasks disable <n>`; never edits tasks, only runs/disables what exists (write-back = explicit confirm). | ops folks; complements RI2 #108 services | M | M | v0.22.x |
| 237 | Net toolkit | `netinfo` (adapter, IPv4/6, gateway, DNS from GetAdaptersInfo), `net flushdns` (elevate-aware via #81), `net ping <host>` mini-table (4 samples, ms column, colored thresholds) - all read-only API or bundled-tool output, console rows. Beyond ROADMAP #37's strip chip. | helpdesk one-liners | H | M | v0.21.x |
| 238 | Wake-on-LAN | `wol <name>` sends the magic packet (UDP broadcast 9/7, 6×FF + MAC×16) to machines in `[Wol] name=AA:BB:CC:DD:EE:FF,subnet`; confirms packet count; only new import is `ws2_32`, no third-party deps. | homelab users | M | S | v0.23.x |
| 239 | Eject volumes | `eject [d:]` - `FSCTL_LOCK_VOLUME` → `FSCTL_ALLOW_MEDIA_REMOVAL` off → `IOCTL_STORAGE_EJECT_MEDIA` fallback to unmount notify; per-drive rows, tray balloon on failure; `eject *` ejects removable only. | hot-swap HDD users | M | S | v0.22.x |
| 240 | Snip handoff | `snip` invokes the OS region clipper (`shell:AppsFolder\...ScreenClip` / Win+Shift+S via SendInput) then reports "captured → clipboard/Photos folder"; `snip file <path>` uses PrintWindow for a specific window. Zero capture code in-core. | Snipaste users who live in the launcher | M | S | v0.20.x |
| 241 | Color picker + formats | `color pick` = GetCursorPos + GetPixel → hex, `color #hex` renders a swatch row + copies hex/rgb/hsl/css vars selectable by ←/→; `[Color]` theme swatches in Settings reuse the painter. Not RI1 #38's numeric convert - color-aware math (rel-lum contrast readout). | dev/designer loop | M | M | v0.22.x |
| 242 | QR render + hand-off | `qr <text\|url\|clipboard>` renders a QR in the console with half-block chars (`▄█`) at correct aspect, plus `qr png <file>` via GDI DIB (plugin, ~250 LOC byte-mode encoder, MIT-reimplemented); copy-as-image Ctrl+Shift+C. | event badges, print labels - no network, unlike web QR tools | H | M | v0.21.x |
| 243 | Regex lab | `re <pattern>` tests against clipboard (or `re <pat> -- <text>`), highlights matches, lists capture groups as rows, `←` copies a group; PCRE-lite backtracking engine in a plugin, 100 ms cap to avoid pathological hangs. | devs' eternal regex tab | H | L | v0.23.x |
| 244 | JSON tools | `json fmt\|minify\|get <path>\|keys` on argument/clipboard (auto-detect), 5 MB cap, `$.a[0].b` dotted paths, error rows with offset+caret line. Beyond RI1 #20's single JSON-format paste action. | API/debug daily driver | H | M | v0.21.x |
| 245 | Console diff | `diff a.txt b.txt` (or `diff <app>` vs clipboard) - LCS line diff, +/- colored rows, `d` copies a hunk, `--stat` counts; 2 MB/line cap. The code-review-adjacent tool every launcher lacks. | beyond Beyond-Compare license-free need | M | M | v0.23.x |
| 246 | Checksum verify | Beyond RI1 #38's hash sample plugin: `sha256 /verify <checksums.txt> [base]` validates a whole set (parses `hash  filename`, GNU + `***SUM` flavors), rows green/red, `rescan` skips OK. The release-download ritual, one command. | Linux `sha256sum -c` expectation | H | S | v0.20.x |
| 247 | JWT inspector | `jwt <token\|clipboard>` splits header/payload, base64url-decodes pretty-printed, humanizes `exp/iat/nbf` ("expired 3 days ago"), red-flags `alg:none`; explicitly *no* verification claims - an inspector, not a validator (offline, no keys). | web devs pasting tokens everywhere | M | S | v0.21.x |
| 248 | Cron humanizer | `cron "0 3 * * 1"` → "Mondays 03:00 · next: Mon 11-23 03:00 · every 7 d" + validation of 5/6-field forms; `cron next <n>` lists upcoming runs. Table math only, no scheduler. | devs + homelab cron editors | M | S | v0.22.x |
| 249 | Lorem generator | `lorem [3 paras\|200w]` with a compiled word table, paragraph/sentence mode, `--html` wraps in `<p>`, copies to clipboard. Pure static data. | designers filling mockups | L | S | backlog |
| 250 | Timestamp converter | `ts` (now, both 10/13-digit + ISO), `ts 1731758400`, `ts 2026-11-16T08:00+03`, `ts+tokyo` offset columns, clipboard auto-detect. Units plugin does numbers; nobody does epochs. | devs logging in two timezones | H | S | v0.20.x |
| 251 | Slugify + translit | `slug Привет мир` → `privet-mir` - compiled RU/UA/BG→Latin table + diacritic stripping + URL/`snake`/`kebab`/`Pascal` modes, clipboard fallback. The filename/slug branch RI1 #20's case-convert never covered. | RU-speaking devs publishing filenames/slugs | M | S | v0.20.x |
| 252 | Password strength checker | `pwcheck` (clipboard) / `pwcheck <text>` - Shannon entropy + charset classes + top-2000-compiled-password blacklist + sequential/run penalties → verdict row ("fair 54 bits - add a symbol"); sister command to shipped `pwgen` (`pw`), same plugin DLL. | "is my generated one fine?" follow-up | M | S | v0.20.x |
| 253 | Notes quick-capture | `note <text>` appends timestamped line to portable `Notes\inbox.md`, `note last` copies the newest, `note open` opens in the default `.md` handler; `[Notes] Inbox` path. Capture without an app switch. | obsidian/inbox-zero users | M | S | v0.22.x |
| 254 | Todo capture + due parse | `todo buy milk tomorrow 18:00` → portable `todo.txt` row with ISO due (`due:2026-11-17 18:00`), `todo` lists open rows, `todo done 3`; tiny natural-date parser (day, weekday, +nd/d). Console-only, no sync. | todo.txt users wanting a launcher front | M | M | v0.23.x |
| 255 | Error-code lookup | After a failed launch (ShellExecute error / exit code ≥2 within 2 s), the console offers a row: "0x80070005 - Access denied → web lookup" (Enter opens the existing `[WebSearch]` template for the code - user-invoked handoff, nothing auto-opens). | helpdesk "what does that mean" | M | S | v0.21.x |
| 256 | Windows Update status | `wu` reports via WUA COM *read-only*: last search time, pending/reboot-needed counts, last N KB installs; `wu open` jumps `ms-settings:windowsupdate`. No install triggers - that stays a user decision. | sysadmins' daily "did it install" | M | M | v0.23.x |
| 257 | Low-disk tray alert | `[Disk] WarnGB=5` - hourly free-space check on fixed volumes, one tray balloon per volume per day, click opens `du` on the fattest volume (pairs #233). | the 3-AM "drive full" surprise | M | S | backlog |
| 258 | `lock` sysalias | `lock` = `LockWorkStation()` behind the existing traffic-light class green; joins RI1 #30's shipped `~sysaliases` family as the most-requested missing verb. | shared-office machines | L | S | v0.20.x |
| 259 | `untrash` restore-by-keyboard | Lists Recycle Bin contents via shell folder reads (original path, delete date, size), `untrash <filter>` narrows, Enter restores the selected item to its original folder, `untrash purge` empties behind the danger gate - the single-item rescue RI1 #30's `~recycle` (which only opens/empties the folder) and RI2 #148's verbs both leave uncovered. | "just one file back" every week | M | M | v0.22.x |

## Clipboard

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 260 | Clipboard ring window | Second global hotkey (`[HotKey] Ring`) opens a compact always-on-top picker *pre-loaded* with cliphist's ring (no summon, no typing): ↑/↓, Enter copies, Ctrl+Enter types via #261, Esc dismisses; visual parity with the dropdown theme. cliphist 1.1 shipped the list; this removes the two keystrokes in front of it. | Raycast Clipboard Manager | H | M | v0.20.x |
| 261 | Forced typing | `clip type <n>` sends the entry as per-character SendInput with a `[Clipboard] TypeDelayMs=5` pacing + bracketed-paste guard, for terminals/apps where Ctrl+V paste is intercepted or blocked; reuses cliphist 1.1's caret-paste plumbing, different injector. | terminal/tmux users | H | S | v0.20.x |
| 262 | `clip send <n> <window>` | Paste entry into a *named* window by title-match even if it isn't foreground: find via EnumWindows, SetForegroundWindow it, type, restore focus - with a `[Clipboard] SendExclude` table for password dialogs. | multi-monitor report writing | M | M | v0.22.x |
| 263 | Clip entry → file | `clip save <n> [file]` writes text entry to file (auto-named `clip-<n>-<stamp>.txt` in `[Clipboard] SaveDir`), `clip saveimg <n>` saves the image entries as PNG via the existing clipboard-image path; opens the folder on completion. | collecting screenshots without Paint | M | S | v0.21.x |
| 264 | `clip purge <pattern>` | On-demand, user-typed scrub: `clip purge token` drops ring entries matching the pattern (wildcards + a compiled secret-shape list), reports `removed 3 of 27`; complements RI2 #171's *automatic* rule engine - this is the manual panic eraser. | after pasting an API key "just this once" | M | S | v0.21.x |
| 265 | URL un-shorten | `unshort <url\|clipboard>` follows redirects via `WinHTTP` HEAD (user-typed only, 5-hop cap, timeout) and shows `t.co/xyz → https://real/page +host +title-ish`; also feeds #282's ClickFix-style check as a first-line "is this where you think you're going". | security-conscious link clicking | M | M | v0.21.x |

## Plugins & scripting

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 266 | Plugin sandbox runner | Optional per-plugin `[Plugin:<name>] Sandbox=1`: host loads the DLL into a restricted-token secondary process and marshals the ABI calls over a local named pipe (job object caps handles/memory; file/registry callbacks denied by default), console warns per-denied attempt. Turns RI1 #37's *policy* trust into *mechanical* isolation. | FEATURES "sandboxed plugins" doc bullet - finally real | H | XL | v1.0.x |
| 267 | Hot-reload with state handoff | Beyond ROADMAP #42: watch `Plugins\*.dll` (FindFirstChangeNotification like RI1 #47), on change `Unload`, rename-locked copy, `Load` new, and if the DLL exports optional `SaveState/LoadState` (v3 ABI), migrate its session data; `/reload <name>` manual trigger; failed reload keeps the old one. | plugin-dev threads: "restart host for every edit" | H | L | v0.23.x |
| 268 | ABI gate at load | `/load` validates the full export set + `HOST_FLAGS` before calling `Load`, reports the *exact* missing export (console + log, e.g. `taskman.dll: no GetStringET - host needs v2+`), keeps prior version loaded instead of crashing on first use. Beyond RI1 #35's handshake. | Flow/Wox silent-crash horror stories | H | S | v0.20.x |
| 269 | Gallery browser (offline) | `/plugins` lists shipped-but-disabled and repo-catalogued plugins (PLUGIN_GALLERY rendered from `Plugins.ini` catalog section) with per-plugin rows: version, size, last-load result, Enable/Disable (writes `[Plugins]`, hot-applies via /load /unload). No download - everything installable is already in the zip. | beyond ROADMAP #44's static gallery file | M | M | v0.20.x |
| 270 | `/plugdebug <dll>` inspector | Per-plugin counters (rows served, RunString hits, avg GetString time µs via QPC), last 20 strings/answers with their ET-cache state, and a live tail mode (`/plugdebug <dll> --tail`); doubles as #208's data source. | every "why is my plugin slow" thread | H | M | v0.21.x |
| 271 | Lua scripts plugin | Beyond ROADMAP #43 (which risked core bloat): `script.dll` embeds a small Lua (MIT) and registers commands declared in `Scripts\*.lua` (`tb.register("hello", function(args) ... tb.out(...) end)`) - host stays single-file, sandbox = own lua_State, `os.execute` gated behind the danger framework. | AutoHotkey crowd wanting modern scripting | M | L | v1.0.x |
| 272 | plugtest harness | `plugtest.exe <dll> <line> [...]` - headless host shim: loads DLL, feeds lines, prints rows/`TExec` per call, nonzero exit on crash; CI runs the suite for each example plugin so plugin regressions die in the PR, not in a user's console. | own CI; every plugin-dev README wish | H | M | v0.21.x |
| 273 | C#/Rust ABI bindings | `docs/PLUGIN_API` gains verified minimal bindings: `TypeBox.Plugin.csproj` template (one `[UnmanagedCallersOnly]`-free P/Invoke layer) + `tbplugin` Rust crate (staticlib, extern "C" fns + `#[no_mangle]`), both compiled and shipped in `examples/` so the ABI stays honest across refactors. | the plugin gallery's missing 2 languages | M | L | v1.0.x |
| 274 | Plugin hotkey arbitration | Beyond RI2 #167: when two plugins (or a plugin vs user alias, RI1 #13) claim the same global hotkey, first-loader wins deterministically, loser's row gets a `⚠ hotkey` chip + `/hotkeys` (RI2 #142) lists claims/owners, and Settings gains a per-claim rebind field instead of silent failure. | Wox forum "my hotkey vanished" | M | S | v0.22.x |
| 275 | `PluginsData\` convention | Host passes a per-plugin writable dir (`PluginsData\<name>\`) via the v2 host-API (#36 family), adds `/plugdata <name> [clear]` (lists files + sizes, confirm-clear), and unload-time `SHChangeNotify` refresh; plugins stop dropping crumbs next to the exe. | Flow plugin reviews re: portable litter | M | S | v0.21.x |
| 276 | Webhooks plugin | `hook <name> [extra]` POSTs/GETs the `[Webhooks]`-declared endpoint (url, method, headers, `{clipboard}`/`{args}` templating) with WinHTTP, 5 s cap, response first-line echoed to console; off until `[Webhooks] Enable=1`. User-typed-only network per the offline-first rule. | "deploy the thing" one-liners | M | M | v0.22.x |
| 277 | Env profiles | `[Env:ci] GH_TOKEN=…;PATH+=C:\tools` applied by `winenv ci <alias-or-cmd>` at `CreateProcessEnv` time for alias/script/PATH lines (URLs/shell-catch-all keep the parent env); never touches the host's own env; secrets in `TypeBox.local.ini` + DPAPI (RI2 #175 pattern) recommended. | the "which shell has which tokens" mess | M | M | v0.22.x |
| 278 | Single-shot `--run` CLI | `TypeBox.exe --run "<line>" [--quiet --timeout 5s --json]` classifies+executes via the normal chain and exits with the child's code (or row JSON), reusing single-instance IPC - batch files/Task Scheduler can drive the exact same alias table without a second tray. Beyond RI2 #166's pipe API (CLI ergonomics). | automation from Task Scheduler | H | M | v0.21.x |

## Safety & privacy

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 279 | Confirm mini-language | Beyond shipped danger gate (#77) + ROADMAP #72: `[Safety] Confirm` grows predicates - `name:taskkill args>1 regex:\\s\/[PF] re:\d{3,}` - AND/OR composition, negation, and a `confirm test <line>` dry-evaluator that explains *which* clause fired. Same UI, brains upgraded. | kiosk/sysadmin power users | H | M | v0.22.x |
| 280 | Auto-elevate table | `[Security] AlwaysElevate=taskmgr,regedit,netshare` - launches matching those targets get the runas verb automatically (#81's machinery, inverted from opt-in to pre-consented per *target*, with a first-time "always for regedit?" y/n row that writes the entry). | IT admins' fixed tool list | M | S | v0.21.x |
| 281 | Deny-list | `[Security] Deny=psexec.exe,cmd /c del*` - matched lines hard-stop with a localized refusal ("blocked by policy") + audit log entry (RI2 #170); unlike #173's allowlist mode it coexists with normal usage, and parents/kiosk owners get the 80% case. | shared PC, junior-dev guardrails | M | S | v0.21.x |
| 282 | Filename anatomy warnings | Before executing a file: flag double extensions (`pdf.exe`, `jpg.exe`), RLO/RTL or homoglyph characters in names, `.hta/.lnk/.url` from Mark-of-the-Web zones (NTFS stream read) - inline amber row, Ctrl+Enter still runs it. Complements ROADMAP #72's payload guard with the *filename* half of ClickFix defense. | security blogs 2025-26 wave | H | M | v0.20.x |
| 283 | Trusted-folder green | Lines whose resolved exe lives under `[Path]` or `[Scripts]` folders get the green traffic light without a danger-gate prompt even when patterns match (#279 clauses exempt them); anything outside known folders starts amber. Trust is *declared once*, not per-prompt fatigue. | power users drowning in confirms | M | S | v0.22.x |
| 284 | PIN-lock the console | `[Lock] Enable + PIN (DPAPI-hashed)` - summon shows the console but input stays dead until a 4-6 digit PIN (own numpad overlay, rate-limited 5 tries, 60 s lockout); `TypeBox.exe --unlock` for scripts. Different from RI2 #173 kiosk (that restricts *what runs*; this restricts *who summons*). | shared office laptops, family PCs | M | M | v0.22.x |
| 285 | Scheduled private hours | Beyond RI2 #169 manual private toggle + #174 timed purge: `[History] Private=22:00-07:30` suspends recording automatically (strip shows a moon chip), executed lines still run, nothing is written to History.dat - the trust-me-bro hours. | after-hours "don't log that" | M | S | v0.21.x |
| 286 | `/panic` diagnostic snapshot | One command (or tray item) writes `Diag\panic-<stamp>.zip`: redacted TypeBox.ini (via #49 Redact rules + a compiled secret-shape scrubber), last 200 history lines redacted the same way, `/checkconfig` output, plugin list+hashes, log tail, Windows build; opens the folder for review before attaching to an issue. | every bug-report thread ever | H | S | v0.20.x |
| 287 | Safe mode boot | `/safe` relaunches with no plugins, default theme, default hotkey, and `[Plugins]` ignored for that run (banner row: "safe mode"); if the next start *also* crashes, auto-offer safe mode after the #288 dump - the standard "was it you or a plugin" triage. | classic safe-mode expectation | M | S | v0.20.x |
| 288 | Crash minidumps | `SetUnhandledExceptionFilter` + `AddVectoredExceptionHandler` write `Diag\tb-<stamp>.dmp` (MiniDumpWriteDump) + a one-line "last command being classified" note, then restart the host with `--post-crash` (opens console + toast "crash dump saved"); gated by `[Diag] Minidump=1`. | Flow plugin-crash threads; own debug pain | H | M | v0.20.x |
| 289 | Log levels + `/log` | `[Log] Level=0..3` (errors/normal/diag/trace) applied across host+plugin-load paths, `/log` tails the last 50 in-console with `f <filter>` and `w file`, `/log level 2` live-changes; rotation keeps 3 files (mirrors release-archive discipline). | support triage without repro ping-pong | M | S | v0.21.x |

## Settings, portability & fun

| # | Title | Spec | Inspiration / who asks | Value | Effort | Ver |
|---|---|---|---|---|---|---|
| 290 | `/settings <page>` deep link | `settings appearance` (also `settings ap`, fuzzy-matched page names) opens Settings directly on that page, plus a `page=` field in the existing context menu → Settings items. Beyond RI2 #45's search box - keyboard/CLI direct access, scriptable. *(i18n)* | keyboard-first admins | M | S | v0.20.x |
| 291 | Onboarding that *does* things | Beyond ROADMAP #49's first-run intro: a 4-step interactive pass in the real console - press the hotkey → run the built-in `~dice` → generate `pw 16` → pick a theme from the studio, each with a one-line localized coach caption + skip; `[UI] Onboarded=1` stops repeats. | first-run churn (most launchers' funnel leak) | H | M | v0.21.x |
| 292 | Markdown cheatsheet | `/export cheatsheet <file.md>` - all aliases (+descriptions), web quicklinks, sysaliases, plugin commands with one-line help, hotkeys table, grouped, print-friendly; the doc you paste into the team wiki so muscle memory migrates. *(i18n)* | team rollouts; own README | M | S | v0.22.x |
| 293 | Offline docs viewer | `/docs [topic]` opens the shipped `docs\*.md` (PLAN/CONFIG/PLUGIN_API/…) through a tiny built-in reader window (RichEdit + the existing markdown-lite renderer used by README) with in-page find - no browser, works on a locked-down bench machine. | offline sysadmins; beyond RI2 #182 | M | M | v0.23.x |
| 294 | Portable font bundling | `[Font] File=CascadiaCode.ttf` loads a font shipped next to the exe (GDI `AddFontResourceExW FR_PRIVATE`, released on exit) - the whole TypeBox folder works on a locked-down kiosk with zero installs, completing the portability story (#90's USB angle). | portableapps.com crowd | M | S | v0.21.x |
| 295 | `--profile <dir>` instances | `TypeBox.exe --profile work` runs a fully isolated instance (own INI dir, own `History.dat`, own single-instance mutex, own tray icon tooltip) - a work console and a personal console coexist, distinct from RI2 #42's settings-file profiles. | dual-life laptops, testers | M | M | v0.22.x |
| 296 | Rich tray tooltip + click action | Tray tooltip = version + enabled plugins + history count + last command class; `[Tray] LeftClick=summon\|menu\|hud` configurable (default summon - today it opens the menu, a papercut both AHK and PowerToys threads cite). | papercut threads | M | S | v0.21.x |
| 297 | Tray "pause hotkeys" | Tray item + `[HotKey] Pause=1` auto-unregisters all global hotkeys (game-mode *by choice*, beyond RI1 #31's fullscreen auto-detect), badge overlay on the icon, auto-resume after `[HotKey] PauseMin=120` or manual - prevents the hotkey-steal-in-COD class of bug. | gamers; #146's sibling | M | S | v0.20.x |
| 298 | `/update` release check | User-typed `/update`: queries GitHub Releases API for newer-than-VERSION, shows tag + notes summary (cached offline for repeat views), `y` downloads the **signed zip** to `%TEMP%\TypeBox-update\`, verifies sha256 against the release asset digest, then prints the swap-in instructions (never self-replaces a running exe). Beyond ROADMAP #36 - pinned digest, explicit consent, no silent anything. | winget-era expectations | M | M | v0.23.x |
| 299 | `/at` persistent scheduler | `at 18:30 shutdown /s` / `at friday 09:00 sync` registers a *TypeBox-named* one-shot Task Scheduler job (schtasks, user-level) that re-runs through `--run` (#278); `at list`, `at cancel <n>`; survives reboot unlike ROADMAP #15's in-host timers, and everything is inspectable in the OS snap-in. | AutoHotkey `RunAt`; ops habit | H | M | v0.22.x |
| 300 | Console fun pack | `figlet <text>` compiled 5×7 banner font in console colors, `/fortune` rotating quote pack (17 languages, community-extensible via `lang\fortunes.<code>.ini`, RI2 #190 channel), and `sudo make me a sandwich` → "OK. ▄▄▄▄▄▄" xkcd #149 reply - three tiny morale wins that make screenshots round two. *(i18n)* | RI2 fun section's best engagement; xkcd | M | S | v0.22.x |

## Top 15 value/effort

1. **#226** Snap & position presets - the daily-driver positioning command *(i18n light)*
2. **#230** Stay-awake toggle - one API call, universal relief
3. **#246** Checksum verify (`sha256sum -c` muscle memory)
4. **#250** Timestamp converter - devs live in epochs
5. **#251** Slugify + translit - the RU→Latin branch nothing else ships
6. **#268** ABI gate at load - kills a whole bug class for S effort
7. **#286** `/panic` diagnostic snapshot - support triage superpower
8. **#288** Crash minidumps - debuggability, gated
9. **#201** Ctrl+R history mode - the history feature history deserved
10. **#212** Idle auto-hide - the "still on my screen" fix
11. **#237** Net toolkit - helpdesk one-liners
12. **#260** Clipboard ring window - cliphist's last missing piece
13. **#270** `/plugdebug` inspector - plugin observability at last
14. **#282** Filename anatomy warnings - closes the ClickFix filename hole
15. **#203** Per-argument memory - Raycast-grade ergonomics for S effort

## Next 3 releases - picks

### v0.20 - "Daily-driver plumbing" (headline: #226 pos presets + gallery #7 timer plugin already queued)
- **#226** `pos left|right|center|q1..q4` + named presets *(the drag-move sequel)*
- **#230** `stayawake` · **#250** `ts` · **#251** `slug`
- **#246** `sha256 /verify` · **#240** `snip` · **#258** `lock` · **#252** `pwcheck`
- **#268** ABI gate · **#216** DWM border tint · **#212** idle auto-hide
- (already queued on ROADMAP-50: gallery #7 **timer** plugin)

### v0.21 - "Clipboard muscle & eyes" (headline: #260 clipboard ring window; roadmap #7 sysmon plugin queued)
- **#260** ring window · **#261** `clip type` · **#263** `clip save`
- **#223** star rows · **#225** section headers · **#215** strip layout editor
- **#235** startup manager · **#237** net toolkit · **#229** virtual-desktop goto
- **#269** gallery browser · **#270** `/plugdebug` · **#285** scheduled private hours

### v0.22 - "Brains & paperwork" (headline: #279 confirm mini-language; pairs with #202 recorder)
- **#279** predicate mini-language · **#283** trusted-folder green · **#280/#281** elevate-table + deny-list
- **#202** macro recorder · **#206** history tags · **#208** `/stats` · **#209** `/duplicates`
- **#243/#244** regex lab + JSON tools · **#299** `/at` scheduler · **#292** cheatsheet export
- **#266** sandbox runner (XL) opens here and may land v1.0.x; **#284** PIN-lock as the safety headliner's sibling.
