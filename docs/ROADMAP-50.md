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
