# TypeBox — 50 feature candidates (roadmap backlog)

Companion to [PLAN.md](PLAN.md). Status: 📋 planned · 🔨 in progress · ✅ done.
Effort: S ≈ <1 day, M ≈ 1–2 days, L ≈ a week (single-file C++/Win32 reality check included).
Priority: P1 next releases, P2 v1.x, P3 backlog/wildcard.

## Search & launch
| # | Feature | Spec | Effort | Pri | Ver |
|---|---|---|---|---|---|
| 1 | ~~Fuzzy matching~~ **done v0.11.0** | Subsequence scoring for Tab-completion + drop-down (prefix > word-start > consecutive bonuses, gap penalty); `[Type] Fuzzy=1`, off keeps TypeAndRun prefix mode | M | P1 | v0.11 ✅ |
| 2 | Frecency ranking | Per-item usage counters (count + lastUsed) persisted in `Usage.dat`; drop-down sorts fuzzy ties by frecency | M | P1 | v0.11.x |
| 3 | Start-Menu app index | Scan `%ProgramData%\...\Start Menu` + user's on first show (async, cached); launch any installed app by name; `[Apps] Enable/Lines` | L | P1 | v0.12 |
| 4 | Recent docs | `%APPDATA%\Microsoft\Windows\Recent` *.lnk re-resolve, offered as `recent ` candidates | S | P2 | v0.12.x |
| 5 | Web-search keywords | `g query` → Google, `w query` → Wikipedia, `yt query` → YouTube; `[Web]` section (name=url template), i18n labels | S | P1 | v0.12 |
| 6 | Calculator+ | Expression parser: `2+2*3`, `sqrt(144)`, `5 km in mi`, `100 USD -> EUR` (offline rates file + `/rates-update` opt-in); anchor amber | L | P1 | v0.13 |
| 7 | File search fallback | Windows Search (ISearchQueryLanguage/OLE DB `Search.Sql`) when no other match; `[Search] Enable=0` default off | L | P2 | v1.x |
| 8 | Usage badges | Drop-down rows show `×N` run count (from Usage.dat) right-aligned dim | S | P2 | v0.12.x |
| 9 | Alias arguments memory | Complete `%s` slot from same-alias previous args | S | P2 | v0.11.x |
| 10 | Quick web-tabs opener | `/tabs url1 url2 …` opens list in default browser (single Enter, batch) | S | P3 | backlog |

## Console & input
| # | Feature | Spec | Effort | Pri | Ver |
|---|---|---|---|---|---|
| 11 | Ghost-text completion | Inline dim suggestion after caret, End accepts (only when unique) | M | P2 | v0.13 |
| 12 | Ctrl+R history search | Reverse-i-search mode over History.dat (like bash) | M | P1 | v0.12 |
| 13 | Spell correction | "clalc" → "calc" hint via edit-distance ≤2 against aliases/apps | S | P2 | v0.12.x |
| 14 | Command chaining | `a ; b` runs both (each classified), `a && b` stops on failure; sound once | M | P2 | v0.13 |
| 15 | Timed commands | `in 10m ~sleep`, `at 18:30 cmd` — mini scheduler, tray balloon on fire, `/timers` list, `/cancel N` | L | P2 | v1.x |
| 16 | Macros | `/macro save name` records N executions; `name` alias-runs them with 300 ms spacing; `[Macros]` INI | M | P2 | v1.x |
| 17 | Snippets | `[Snippets] sig=...` typed `sig ` expands via %s engine; Snippets settings page | S | P2 | v0.13 |
| 18 | Ctrl+Enter = keep open | Execute without hiding (today Enter hides per TypeAndRun; add mode) | S | P1 | v0.11.x |
| 19 | Alt+Enter = run as admin | Elevated launch of the resolved command (ShellExecute "runas") | S | P1 | v0.12 |
| 20 | Clipboard pick | `clip ` + drop-down from own clipboard history (last 30 text copies, opt-in, memory-only unless `[Clipboard] Save=1`) | L | P2 | v1.x |

## Visual & UX
| # | Feature | Spec | Effort | Pri | Ver |
|---|---|---|---|---|---|
| 21 | Win11 rounded corners | `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE)` on popup | S | P1 | v0.11.x |
| 22 | Result-type icons | GDI glyph chips in drop-down: 📁/🌐/✉/⚙/⌨ via Segoe UI Emoji fallback (no .rc) | M | P1 | v0.13 |
| 23 | Wallpaper accent | Auto-derive theme accent from desktop wallpaper average color; `[Theme] Accent=auto|hex|wallpaper` | M | P2 | v0.13 |
| 24 | Follow system light/dark | `[Theme] Name=auto` maps to cozy-dark/paper-light from AppsUseLightTheme | S | P1 | v0.12 |
| 25 | Drop-down fade-in | 120 ms alpha fade, disabled when `[Animations] Enable=0` | S | P2 | v0.13 |
| 26 | Preview pane | Tab-suspected row detail strip: file size/date, URL host, alias target (one line, dim) | M | P2 | v1.x |
| 27 | Drop-down context menu | Right-click row: Run, Run as admin, Open location, Copy path, Pin | M | P2 | v0.13 |
| 28 | Pinned rows | Ctrl+P pins a drop-down row to top with 📌; `[Pins]` INI | S | P2 | v0.12.x |
| 29 | Clock chip options | `[UI] Clock=off|12h|24h`, seconds toggle; date tooltip | S | P2 | v0.11.x |
| 30 | Console width auto-fit | Shrink-to-fit to longest suggestion (min 480 / max screen-64), `[Show] AutoWidth=0` | S | P3 | backlog |

## Tray & system integration
| # | Feature | Spec | Effort | Pri | Ver |
|---|---|---|---|---|---|
| 31 | Tray context expansion | Menu adds: Fuzzy on/off, Theme cycle, Language cycle, Do-Not-Disturb | S | P1 | v0.12 |
| 32 | DND mode | Suppress sounds + tray error dot for 1 h or until off (`/dnd`) | S | P2 | v0.12.x |
| 33 | Media/mouse-button hotkey | `MouseX1/MouseX2` as `[HotKey] Key` values (low-level mouse hook) | M | P2 | v0.13 |
| 34 | Win+R handoff | Offer (opt-in) HKCU App Paths shim so `typebox` runs from Win+R | S | P3 | backlog |
| 35 | Explorer bar prefix | Opt-in "tb " from Explorer address bar via App Paths + IME note | M | P3 | backlog |
| 36 | Auto-update check | `/update` hits GitHub Releases API (portable zip), balloon "new version", manual download | M | P2 | v1.x |
| 37 | Battery/Net chip | Status-strip ♪-neighbor: `⚡87%` / `⚠offline` when offline (GetNetworkConnectivity) | S | P2 | v0.13 |
| 38 | Per-app hotkeys | `[Ignores]` inverse: list where hotkey is swallowed entirely (keep TypeAndRun BadApp default) | S | P3 | backlog |
| 39 | Run-as-admin host mode | `/elevate` self-relaunch; warning when target needs elevation but host can't | M | P2 | v0.12 |
| 40 | Session summary | `/day` prints today's executions grouped (uses Usage.dat) | S | P3 | backlog |

## Plugins & extensibility
| # | Feature | Spec | Effort | Pri | Ver |
|---|---|---|---|---|---|
| 41 | Plugin SDK v1 | `typebox_plugin.h` single header + sysinfo example + docs/PLUGIN_API.md examples tab | M | P1 | v1.0-rc |
| 42 | Plugin hot-reload | Plugins page **Reload** button: FreeLibrary/LoadLibrary round-trip | S | P2 | v1.x |
| 43 | Lua scripts plugin | Built-in `Scripts\*.lua` host (tiny embeddable Lua compiled into optional `tb_lua.dll` plugin, not core) | XL | P3 | v1.x |
| 44 | Plugin gallery file | `[Plugins] Gallery=https://…/gallery.json` list w/ one-click download to `Plugins\` (hash-pinned, opt-in) | M | P2 | v1.x |
| 45 | Plugin ET cache | Cache GetStringET results per session (perf for slow plugins), invalidate on /reread | S | P2 | v0.12.x |

## Settings, safety, portability
| # | Feature | Spec | Effort | Pri | Ver |
|---|---|---|---|---|---|
| 46 | Dead-alias checker | On start (opt-in) verify alias programs exist (`where`/SearchPath); report page badge + `/checkconfig` section; suggest fixes | M | P1 | v0.12 |
| 47 | Config diff & rollback | `/undo` extended: keep last 5 INI copies `Config\`, `/revert` to previous, diff in log | M | P1 | v0.13 |
| 48 | Per-machine override | `TypeBox.local.ini` loaded after main INI (machine-specific paths/hotkeys stay portable) | S | P1 | v0.12 |
| 49 | First-run onboarding | No-INI first start → friendly TypeBox.ini generated + balloon + `welcome` one-shot line set | M | P2 | v1.0 |
| 50 | Settings search box | Filter text on top of page list: matches page + control labels via i18n strings | M | P2 | v1.x |

**Policy reminders** for every feature: i18n keys appended at the end of `src/i18n.h`; INI keys documented in `docs/CONFIG.md`; CHANGELOG entries are image-led (screenshot of what changed) with a short bullet; releases must pass CI + local asset run gate; tests live in `C:\temp\opencode\test-vNNN.ps1`.

---

## Research merge (2026-11-15) — from [IDEAS-RESEARCH.md](IDEAS-RESEARCH.md) + [UX-REVIEW.md](UX-REVIEW.md)

Both subagent reports reviewed. Adopted into this roadmap under my numbering; the reports keep the
full specs and sources (PowerToys Command Palette backlash #46096/#43174, Flow 2.1.4, Wox v2.4.9,
Raycast-Win v0.41, Listary 6.3, Keypirinha migration wave).

### v0.12.0 batch — keyboard muscle memory + UX quick wins (**shipped 2026-11-15**)
| Mine | Adopted | Item | Source | Status |
|---|---|---|---|---|
| #51 | R#3 | **Alt+1…9 quick-select** in the drop-down (digits painted on rows) | Flow/Listary/Wox; UX-TOp | ✅ v0.12.0 |
| #52 | R#4 | **Web-search quicklinks** `[WebSearch] g=…%s` (defaults g/yt/gh, i18n anchor) | Raycast Quicklinks, Listary | ✅ v0.12.0 |
| — | U-D8/B2 | **Drop-down scroll** (`g_ddTop` + wheel) — rows past `Lines` selectable/visible | UX-REVIEW | ✅ v0.12.0 |
| — | U-B3 | Hover-execute guard (Enter runs only keyboard-selected rows) | UX-REVIEW | ✅ v0.12.0 |
| — | U-Top1/B10 | **Contrast guard** strip text ≥4.5:1 (Mix% auto-raised on light themes) | UX-REVIEW | ✅ v0.12.0 |
| — | U-Top2/B1 | Cue banner hint in the edit (Tab/Enter/Esc discoverability, i18n) | UX-REVIEW | ✅ v0.12.0 |
| — | U-A4 | Edit height = tmHeight+6 (stops 18pt clipping) | UX-REVIEW | ✅ v0.12.0 |
| — | U-A2 | Unlit traffic-light lamps visible (Mix 32% + ring) | UX-REVIEW | ✅ v0.12.0 |
| — | U-B1 | Tray tip + balloon show the actual hotkey | UX-REVIEW | ✅ v0.12.0 |
| — | U-run-up | AutoHide timer resets on typing | UX-REVIEW | ✅ v0.12.0 |
| — | U-defect | calc.c `GetInfo` → ASCII (settings-plugins garbled `в Б`) | UX-REVIEW | ✅ v0.12.0 |

### v0.12.x–v0.13.x adoption queue (next after v0.12.0)
| Mine | Adopted | Item | Effort |
|---|---|---|---|
| #53 | R#14 | History prefix search (typed word + ↑ cycles matches only) | S |
| #54 | R#12 | Double-tap-modifier summon (double-Ctrl, opt-in) | S |
| #55 | R#27 | Opt-in autostart (HKCU Run, tray + settings checkbox) | S |
| #56 | R#43 | Machine/user overlay config = my #48 `TypeBox.local.ini` | S |
| #57 | R#48 | Danger confirmation gate (`rm -rf`, `del /s`, format → [Enter] confirms) | S |
| #58 | R#5 | `~run` verb catalog + ms-settings deep links | M |
| #2  | R#2 | Frecency / adaptive usage ranking (`Usage.dat`) | M |
| #59 | R#9 | Snippets `[Snippets]` (expansion via %s engine, my #17 superset) | M |
| #60 | U-B4/D10 | Ctrl+A/PgUp/PgDn in edit + OK default button | S |
| #61 | U-B12 | Per-Monitor V2 DPI + MonitorFromPoint centering | M |
| #62 | U-D9 | 1px themed hairline instead of WS_BORDER on the edit | S |
| #63 | R#7 | Window switcher `~w` (EnumWindows) | L |
| #64 | R#8 | Clipboard history (opt-in, privacy-gated) | L |
| #65 | R#10 | Offline calculator + conversions (my #6 superset) | M |

Own extra ideas beyond both reports: **#66** INI profile switcher (`TypeBox --profile work`),
**#67** per-monitor console geometry, **#68** "open containing folder" (Ctrl+Enter on a file row,
Explorer), **#69** export/import as a single Base64 blob line for chat-transfer, **#70** idle-CPU
proof badge in `/about` (< 0.1 % claim with measured number).



---

## Shipped

- **v0.13.0 — hotkey freedom (user priority).** Any key assignable (single keys, media, Esc,
  F13–F24, numpad, OEM); RegisterHotKey + low-level hook fallback with exact-modifier
  matching; MOD_NOREPEAT; human key names (VolUp, Num7, WebFav…); Esc is a key, not a
  cancel; font picker combo; live theme/font previews; About credits the author.
  Also shipped with it: docs/PLUGIN_GALLERY.md (10 specced plugins) + docs/IDEAS-RESEARCH-2.md
  (ideas #101–200, wave 2).
- **v0.13.1 — release-encoding hotfix.** MSVC lacked /utf-8: 19 of 20 language packs shipped
  as mojibake in released exes. CMake fixed + permanent in-process CI gate --check-i18n.

## Next releases (merge queue after v0.13)

| Own# | Source | Feature | Size |
|---|---|---|---|
| #71 | GALLERY Top-5 | v0.14.0 = first real plugins from PLUGIN_GALLERY: **units + codec + vol** (zero-dep C, ~600 LOC, host untouched) | M |
| #72 | RI2 #168 | ClickFix/paste-run payload guard (highlight suspicious Base64/one-liners before Enter) — "the only safe Run box" | M |
| #73 | RI2 #159 | Scripts-folder auto-register (every script in [Scripts] Dir becomes an alias) | S |
| #74 | RI2 #103 | Everything bridge (ES_LIST_SOCKET IPC when everything.exe runs, else graceful) | M |
| #75 | #1–#4 queue | Frecency Usage.dat, TypeBox.local.ini overlay, danger-confirm gate, history-prefix Up | M |
| #76 | RI2 #184 | UIA provider so Narrator/NVDA can read the hand-drawn drop-down | L |
| #77 | RI2 #102 | CJK phonetic search (pinyin/kana fuzzy on the same FuzzyScore spine) | L |

GALLERY backlog: cliphist/taskman/timer next wave; nettools awaits the wininet-vs-winhttp policy call.

- **v0.14.0 — plugins + console context menu (user request).** units/codec/vol shipped;
  Submit keeps the console open for plugin answers; `plugin: … handled` log lines;
  right-click menu on the console (TypeAndRun-style, 20 languages).

## v0.15.0 — Theme studio (user request: "create your own themes") — **shipped**

| # | Feature | Size |
|---|---|---|
| 78 | ✅ `themes\<name>.theme.ini` portable presets: bg/fg/accent/border colors (ChooseColorW pickers), border thickness px, alpha, font family/size/bold, console W/H — Save as…/Delete on the Theme page; combo lists built-ins + customs; Apply pushes font/alpha/geometry to INI, colors live in the theme file with `[Color]` explicit-wins kept | L |

## v0.16.0 — second plugin wave + quality backlog — **shipped**

| # | Feature | Size |
|---|---|---|
| 4 | ✅ cliphist — clipboard history plugin (gallery #4): `clip 1..N` rows, `[Clipboard] Max/Popup/Persist` | M |
| 5 | ✅ taskman — process control plugin (gallery #5): `task firefox`, `kill firefox`, `[Taskman] Confirm` | M |
| 72 | ✅ ClickFix-style payload guard — confirm encoded one-liners before the shell runs them | S |
| 73 | ✅ `Scripts\` folder auto-registration (any `.bat/.ps1/.py` becomes a command) | S |

## v0.17.0 — ranking & trust backlog

| # | Feature | Size |
|---|---|---|
| 75 | frecency ranking in the drop-down (count + lastUsed in `Usage.dat`) | M |
| 76 | `TypeBox.local.ini` — per-machine override merged over `TypeBox.ini` (portable + roaming) | S |
| 77 | danger-confirm for destructive aliases (`rm`, `del`, `format` patterns) reusing the v0.16 guard UI | S |
| 79 | cliphist pin-to-keep + `clip <n>` also pastes at caret via Ctrl+V emulation | S |
| 80 | taskman `restart <name>` (wait + relaunch main window) and CPU column in `task` output | S |

