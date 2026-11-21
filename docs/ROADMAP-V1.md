# TypeBox — the road to v1.0 and v1.5

Research + master plan, 2026-11-18. Baseline: **v0.35.0 shipped 2026-11-20** (19 plugins,
scheduled commands `in`/`at` + `/timers` + DND; v0.33 «write less» shipped the day before).
Companion docs: [ROADMAP-50.md](ROADMAP-50.md) (item backlog #1–300),
[PLAN.md](PLAN.md) (original phases), [RESEARCH-PLUGINS-2026-11-18.md](RESEARCH-PLUGINS-2026-11-18.md),
[PLUGIN_GALLERY.md](PLUGIN_GALLERY.md), [FEATURES.md](FEATURES.md) (TypeAndRun parity).

Reading order: §1 where we are → §2 what the 2026 market says → §3 non-goals →
§4 the release ladder v0.32→v1.0 → §5 v1.1→v1.5 → §6 plugin roster → §7 new views →
§8 quality system → §9 risks → §10 sources.

---

## 1. Where we are (verified inventory)

**Host** — single `src/main.cpp` (~5.7k LOC) C++/Win32, zero runtime deps, portable
INI, 20 UI languages, 24 slash commands (`/about`…`/update`), per-monitor DPI v2,
theme studio (`themes\*.theme.ini`), drop-down suggestions + fuzzy + frecency
(`Usage.dat`), any-key hotkeys (RegisterHotKey + LL-hook), scripts-folder autovivification,
`su` elevation, danger-confirm + ClickFix payload guard, `pos` docking, `stayawake`,
headless test mode, `/update` with SHA-256 verification.

**Plugins** — 19 example plugins on the TypeAndRun-compatible ABI
(`calc units codec vol cliphist taskman winlist pwgen timer sysmon util note yt weather
nettools off todo json` + `wol` in v0.31).

**Confirmed absent in code** (grep-verified today, so the plan doesn't re-plan shipped things):
ConPTY/bash mode, macros, snippets, command chaining, ghost text, DnD in/out, UIA provider,
autostart entry, `[Pins]`/stars, double-tap-modifier, Everything bridge, Windows Search
file search, `/stats`, history export, DND mode, HUD window, winget/installer story.

**Non-negotiable laws** (every release re-proves them):
1. **Offline-first** — network only inside a user-typed line (`yt`, `weather`, `/update`);
   power/network plugins answer `would run/send …` under headless and NEVER touch the machine.
2. **Portable** — one folder, `TypeBox.ini` (+ `TypeBox.local.ini`) is the whole state;
   import/export must keep working.
3. **ABI v1** — the 8 cdecl exports stay compatible with TypeAndRun plugins.
4. **Quiet** — idle CPU < 0.1 %, no balloons/hotkey grabs in headless; tests never disturb the user.
5. **One new command word per wave**, global-uniqueness asserted (host + all plugins) by
   `audit-words` — the v0.30.0 shadowing bug is the founding lesson.
6. **Roots in the README/About** (TypeAndRun, galancev, GPL-3.0, Evgeniy Galantsev) + inspirations.

---

## 2. What the 2026 field research surfaced

Sources reviewed Nov 2026: PowerToys **Command Palette** (0.90→0.97 release notes +
devblogs), **Flow Launcher** plugin site (~1000 plugins), **Raycast 2.0** (Windows
release, store, "next generation" page), **Wox v2.5.0 full feature pass** (repo +
woxlauncher.com, Nov 2026), Keypirinha HN/Reddit threads,
windows-terminal-quake / `wt -w _quake` (Quake-mode demand), ConPTY docs.
Prior internal waves: RI1 (#1–50), RI2 (#101–200), RI3 (#201–300).

| Signal (what competitors shipped/ask for in 2026) | TypeBox answer |
|---|---|
| CmdPal 0.97: **drag-and-drop out of the palette** (files, clipboard rows), **quick-access shelf** pinned via Alt+1…9, custom search engines, Remote Desktop extension, **Pinyin** fuzzy | DnD out + shelf are real gaps → v0.37 / v1.5; Pinyin-class CJK → font cascade + CJK fuzzy (RI2 #102) in v0.40 |
| Flow's biggest plugin category is **AI** (Ollama/LM Studio/OpenAI/Gemini/Groq), but all of it is typed-query, user-key-based | keep offline-first: AI ships as an **opt-in typed-only plugin** with user-configured endpoint (Ollama LAN included), default OFF → v1.3 |
| Flow has **layout-fix / layout-fallback** ("`дфя` finds what `laz` finds") — RU users' loudest plugin | we have a RU/UA/UA audience (TypeAndRun heritage) → **layout-fallback fuzzy at the host ET layer** — nobody does it for aliases/commands/plugins → v0.32 marquee |
| Flow store staples: TOTP, port→process, QR, emoji, color picker, cron humanizer, `uuid`, temp cleaner, winget/scoop frontends, WebDAV settings backup | map 1:1 onto our plugin roster (§6) — all fit single-DLL C + offline-first |
| Raycast-on-Windows reviews praise **snippets, clipboard manager window, natural-language calc, window management**; store = extensions moat | snippets v0.33; clip ring already shipped (v0.24) → image rows later; gallery-with-download → v1.1 |
| Keypirinha HN/Reddit: the two things people miss = **clipboard history** and **universal actions** on results | clipboard ✓ shipped; **universal actions** (right-click row: open location / copy / admin / uninstall) → v1.5 |
| Quake terminals (WTQ, `wt -w _quake`) — sustained demand for drop-down console | our tagline *promises* "console + bash" (PLAN Phase 4) → **ConPTY bash mode** is the v1.2 flagship (with a `wt`-handoff fallback if scope explodes) |
| Everyone distributes via **winget** + installers; CmdPal ships inside PowerToys | v1.0 exit criteria: winget manifest PR + optional installer, portable zip stays canonical |
| Recurring plugin-dev complaints (Flow/Wox threads): silent crashes, locked DLLs, no plugin log, no data convention | **plugin DX wave** v0.36: hot-reload, `/plugdebug`, SEH guard, `Data\` convention, manifest with declared words |
| **Wox v2.5.0** (Nov 2026): fully native + GPU, ~150 MB resident; triple store (plugins/themes/AI commands); built-in **screenshot region**, **window layouts**, **browser bookmarks + open-tab jump**, **media-player control**, **hotkey overview**, floating notes; **AGENTS.md + "ask an AI agent to build a plugin"**; free code signing via SignPath | memory story validated — ours is stricter (40 MB budget, §4b) so no chase; roster gains `shot`/`snap`/`marks`/`media` (§6); `hotkeys` cheat-sheet line (parking lot); store-lite v1.1 gains a theme gallery; v0.36 SDK goes **agent-friendly** (AGENTS.md + reference-plugin skeleton — today's coding agents write DLLs too); **Authenticode lands in v1.0** — this week's Defender `Bearfoos.A!ml` FP on the unsigned MinGW build is the proof |

**Strategic synthesis — four bets to v1.0:**
1. **Distribution + first run** (installer/winget/onboarding/autostart) — biggest gap between
   "great tool" and "v1.0 product".
2. **Plugin platform DX** — turns 19 example DLLs into an ecosystem (SDK header, manifest,
   hot-reload, debug view, later gallery).
3. **Keyboard-native differentiation** — layout-fallback, spell-suggest, ghost text, snippets,
   macros: the TypeAndRun crowd's wants, cheap on our codebase.
4. **Finish the promises** — the README/PLAN advertise *bash mode* and *info display*; they
   become v1.2 (ConPTY) and are already half-true via plugins.

---

## 3. Non-goals (keep writing these down or they creep in)

- ❌ Telemetry, accounts, cloud sync *as a requirement* (WebDAV stays opt-in, user-typed).
- ❌ Mandatory network / silent background fetches of any kind (checks happen only after a
  typed consent command; `/update` remains typed).
- ❌ .NET / Electron / Qt / embedded interpreters **in the core exe** (Lua only as an optional
  plugin DLL; AI/OCR only as typed plugins).
- ❌ Breaking ABI v1 (a v2 manifest layer is additive only; ABI v2 = v2.0 conversation).
- ❌ "AI-first" repositioning — AI is one opt-in plugin, not the product.
- ❌ Registry writes without an explicit opt-in UI (autostart/trust live on a `[Registry]`-page,
  every entry individually togglable).
- ❌ Embedded browser pane — Wox opens sites *inside* the launcher; links hand off to the
  user's browser. The console stays a console.

---

## 4. Release ladder → v1.0

Protocol unchanged per wave: suite `test-vNNNN` → regression all suites → docs
(README "Working today" / CHANGELOG image-led / CONFIG) → screenshot composite → CI →
API tag → **downloaded-bytes gate** → permanent archive → playbook §10 → RU report.
Effort S ≈ ≤2 d, M ≤ 1 w (our real cadence: S-hours to days).

### In flight
| Ver | Codename | Items |
|---|---|---|
| **v0.31.0** | power-on + word-law | `wol` #19 (magic packet, headless dry-run, presets), taskman `restart`→`relaunch` (off keeps `restart`), `audit-words` global uniqueness as hard build gate — **✅ shipped 2026-11-18** (26/26 suite, 75/75 release-bytes gate) |
| **v0.32.0** | **native typing** *(marquee for our audience)* | layout-fallback fuzzy (QWERTY-typed-Cyrillic / vice-versa in ET rows & aliases, `[Type] LayoutFallback=1`, `~` hint), spell-suggest "did you mean" ≤2 edits (ROADMAP #13), `/checkconfig` v2 dead-alias badges + one-line fix (ROADMAP #46), ET keystroke debounce (perf §4b) — **✅ shipped 2026-11-19** (25/25 suite, release-bytes gate green) |
| **v0.33.0** | **write less** | ① ghost-text inline completion (unique match, End/Right accepts, ROADMAP #11); ② per-`%n` argument memory (`Usage.dat` `name#slot`, RI3 #203); ③ **Snippets** `[Snippets]` + `%s` engine + Settings page + `snippet add/list/del` (ROADMAP #17/#59); ④ **Search Engines Lite** §4a (`[WebSearch]` self-service, `name:q`/`!name q` forms, `search add/list/del/test`) — **✅ shipped 2026-11-19** (40/40 suite, release-bytes gate green) |
| **v0.34.0** | **time machine** | ① **timed commands** `in 10m <cmd>` / `at 18:30 <cmd>`, `/timers`, `/cancel N`, persistent `Timers.dat` (missed-not-run at boot, ids never reused), tray balloon + headless `would run`, danger-skip at fire (ROADMAP #15, RI3 #202-adjacent); ② DND `/dnd [min]` + `[Sounds] Quiet=22:00-07:30` gating sounds and every balloon (ROADMAP #32, RI3 #220) — **✅ shipped 2026-11-20** (50/50 suite, release-bytes gate green) |
| **v0.35.0** | **dashboards + plugin DX + boot** *(three planned waves combined)* | ① `/stats` — 14-day sparkline + top-5 + plugin table from the reserved `day#`/`plug#` rows (RI3 #208); ② history CSV `/export history` / `/import history` with redaction (RI3 #207); ③ favorites: `Ctrl+S`, `favs ` filter, `* favorites`/`* history` section headers (RI3 #223/#225); ④ `/plugdebug` + fault guard + `Data\<dll>\` convention (v0.36 items ④⑤⑥, RI3 #270/#275); ⑤ `autostart on|off|status` HKCU Run under the dry-run law + checkable tray **Do Not Disturb** (v0.37 items ③④, ROADMAP #31) — **✅ shipped 2026-11-20** (48/48 suite, release-bytes gate green) |
| **v0.36.0** | **plugin DX** *(v1.0 blocker)* | ① `include/typebox_plugin.h` SDK + PLUGIN_API.md + **AGENTS.md** + reference skeleton `examples/plugin_skeleton` (agent-generates-a-plugin, the Wox pattern, ROADMAP #41); ② sidecar manifest `*.tbplugin.json` — declared words/author/hostmin, word law enforced **at load** (audit-words moved into the host); ③ **hot-reload** `/reload <plugin>` + Settings Reload button (ROADMAP #42); ④ §4b perf: per-plugin ET counters in `/plugdebug` (append-only contract), per-plugin ET LRU cache (8 entries/8 s), `Load()` >20 ms audit log; ⑤ fault guard extended over `Load`/`Unload`; ⑥ fixed the reload landmine — cliphist/sysmon/timer window classes moved under their own module (+`UnregisterClassW`), killing the stale-`WndProc` 0xc0000005 after FreeLibrary — **✅ shipped 2026-11-21** (30/30 suite, release-bytes gate green) |
| **v0.37.0** | **drag, drop, boot** *(drop-out time-boxed per risk table)* | ① **drop-in**: files dragged onto the console insert quoted paths at the caret (`WM_DROPFILES`→`EM_REPLACESEL`, `[UI] DropIn`); ② **drop-out**: press-and-move on a file row starts a real OLE drag via a minimal `IDataObject`/`IDropSource` (`CF_HDROP`+`CF_UNICODETEXT`), `[DropDown] DragOut` — and the headless law holds: the OLE loop never runs, the log answers `would drag:` (CmdPal 0.97 parity signal); ③ tray menu expansion (ROADMAP #31): checkable **Fuzzy matching** + **Next theme**, both INI-backed (`[Type] Fuzzy`, `[Theme] Name`), console repaints via `ApplyThemeVisual`, drop-down rescans — **✅ shipped 2026-11-21** (25/25 suite, release-bytes gate green) |

### Planned waves (order = dependency order; swap freely, keep the ladder invariants)
| Ver | Codename | Items (RI refs) | Sizes |
|---|---|---|---|
| **v0.35.0** | — *(combined into the shipped v0.35.0 row above)* | — | — |
| **v0.36.0** | — *(combined into the shipped v0.36.0 row above)* | — | — |
| **v0.37.0** | — *(combined into the shipped v0.37.0 row above)* | — | — |
| **v0.38.0** | first run, find settings | ① **onboarding**: no-INI first start → generated starter INI + welcome line-set + sample alias + `/help` tour (+ confetti flourish on the first successful run — the Wox launch signal, cheap GDI) (ROADMAP #49); ② Settings search box (ROADMAP #50); ③ idle auto-hide (RI3 #212); ④ follow-focus per-monitor summon (RI3 #214); ⑤ DWM border/caption tint (RI3 #216) | M · M · S · S · S |
| **v0.39.0** | trust | ① filename hygiene — RLO + double-extension anatomy warning before shell-run (RI3 #282, extends the v0.16 guard); ② trusted folders — green lamp for `[Security] Trusted=` paths (#283); ③ danger-gate list refresh + `/checkconfig` wiring | M · S · S |
| **v0.40.0** | everyone | ① **UIA provider** so NVDA/Recorder read the painted dropdown (ROADMAP #76/RI2 #184) — prototype NVDA pass first; ② font fallback cascade (`[Font] Cascade=`, CJK tofu fix, RI3 #217); ③ per-page language override `[Language] Console=ru` (RI3 #222) | L · M · S |
| **v1.0.0** | **platform** | ABI-freeze declaration + VERSIONING policy (ABI v1 guaranteed until 2.0); **Authenticode code signing** — SignPath OSS or Azure Trusted Signing (the v0.35 Defender `Bearfoos.A!ml` FP on the unsigned build is the business case; CI signs the artifact, zip stays canonical); **installer story** — portable zip canonical + `winget` manifest PR + optional `install.ps1` (scoop/choco manifests optional follow-ups, the Wox distribution table); docs site pass (every feature one screenshot, CONFIG completeness audit); E2E matrix run (clean Win10/11, 96/150/200 % DPI, Win8.1 degrade check); `/update` weekly-consent check (typed once); **`/perf` report card green on the §4b budgets**; branding/release-notes polish pass; README comparison table gains CmdPal + Raycast columns | L |

**Ladder invariants:** ≥1 user-visible "screenshot moment" per wave · new words pass
`audit-words` · every power/network surface keeps the headless dry-run contract · suites copy
the FULL 19+ plugin roster (the v0.30.0 lesson) · no wave introduces a core-exe dependency ·
§4b perf budgets do not regress (measured, not claimed).

### 4a. Search Engines Lite (v0.33) — "add a site as a search engine", browser-style

> **Shipped 2026-11-19 in v0.33.0** — zero-DLL engines, `search add|list|del|test`, the three
> invoke forms, the Settings page and the audit-words extension all landed. **Not shipped:**
> `search import` (opensearch.xml / SearchScopes) — it needs the network, and a dead command
> is worse than an absent one; it rides to a later wave with a live fetch path.

The old Firefox/Chrome feeling: *name + URL template with the query string = a search engine
you can invoke by keyword*. TypeBox already ships `[WebSearch] g=…%s` quicklinks (v0.12) —
this upgrades them from INI rows to a first-class, self-service mini-plugin surface.

- **Zero-DLL plugins**: any `[WebSearch] name = https://…%s` row *is* a search plugin —
  auto-registered for ET (typing `gh pho…` Tab-completes), auto-listed in the drop-down.
- **Create without touching the INI** (host word `search`, pre-checked unique):
  `search add <name> <url-with-%s>` · `search list` · `search del <name>` ·
  `search test <name> <query>` (shows the assembled URL, runs only on Enter) ·
  validation: `%s` present-or-appended, name = `[A-Za-z0-9_-]{1,32}`, collides-with-nothing
  (the audit-words rules applied to user rows at runtime).
- **Invoke** three ways: `g query` · `g:query` · `!g query` (DDG-bang style,
  `[WebSearch] Bang=1`).
- **Import**: `search import <opensearch.xml-url>` — fetch + parse OSD v1.1 XML
  (`ShortName`, `Url template`), typed-only network; `search import ie` from the free
  HKCU `SearchScopes` registry (no SQLite anywhere). Bookmarks-HTML `<keyword>` import =
  best-effort stretch.
- **Settings page**: "Search engines" table (name/keyword/URL template, %s-highlight,
  Add/Update/Delete + live test) on the model of the Aliases page.
- **Offline-first**: network only on a typed invocation or typed `import`; the fetch reuses
  the yt/weather winhttp helper.
- i18n keys appended; `audit-words` extends to `[WebSearch]` names (they consume a command
  word just like plugins do).

### 4b. Performance & memory track (cross-cutting, v0.32 → v1.0)

**Rule: measure first, then optimize; budgets become CI gates.**

*Card:* new `/perf` console line — startup ms (hot/cold), WorkingSet + private bytes,
idle CPU % (measured 5 s), ET latency p50/p95, per-plugin load ms, dropdown repaint count.
`/about` prints the measured idle-CPU number (RI #70) instead of a claim.

| Budget (CI-enforced from v0.36, green required at v1.0) | Target |
|---|---|
| idle CPU (headless, 60 s sample) | < 0.1 % |
| working set, warm (startup + 100 queries) | < 40 MB |
| working-set growth after 1 000 dropdown/execute cycles (leak smoke, in suite) | < 2 MB |
| ET response p95 while typing (19 plugins loaded) | < 16 ms |
| startup: hot / cold | < 300 ms / < 800 ms |

*Track items (who lands what):*
- **v0.32** (lands with the ET work anyway): keystroke debounce — ET rescans only on
  caret/line-diff, not on every paint; stop re-converting unchanged wide→UTF-8 lines
  per keystroke (single cached conversion per state change). **✅ landed in v0.32.0**
  (drop-down diff-cache on text/span/mode + one `ToAnsi` per rebuild; scan counter
  exposed as `|s=N` in the drop-down title for suite asserts, headless only).
- **v0.36** ✅ *(shipped 2026-11-21)*: per-plugin **ms counters** for ET and RunString in
  `/plugdebug` (`et <calls>/<avg>ms c<hits>`, console flags faulted plugins as `FAULT`);
  **ET result cache** per plugin — LRU 8 line-keys, 8 s TTL, invalidated on `/reread`,
  `/reload` and rescan (ROADMAP #45); `Load()` sub-20 ms rule documented in PLUGIN_API.md
  with the host auditing violations into the log (`plugin: <key> Load took Nms`).
- **v0.40**: merged idle tick — collapse the WM_TIMER zoo into one 250 ms host tick
  (clock at 1 s derived), `WM_DPICHANGED` coalescing, dropdown double-buffer cache audit.
- **v1.0**: `/perf` card + budgets gate + leak smoke in suite; Usage.dat append-log with
  compaction on exit (no full rewrites per execution); `[Apps]` index incremental refresh;
  buffers malloc-once-and-reuse audit; static-arena review across the example plugins
  (each plugin's static RAM counted, plugins total < 4 MB).

### Parking lot (S-size, attach to any wave with slack)
`Alt+Enter`=run-as-admin · Ctrl+Enter keep-open mode · battery/net status chips (ROADMAP #37)
· clock chip options (#29) · mouse side-button hotkeys (#33) · double-tap-modifier summon
(RI #54) · `[Ignores]` inverse per-app list (#38) · `Win+R` App-Paths handoff (#34)
· console width auto-fit (#30) · history InvertScroll · result earcons (RI3 #219)
· `/profile` INI profiles switcher (RI #66) · file-dialog quick-jump (`jumpto`: fill the
active file dialog's path edit from the console — the Wox quick-jump signal).

---

## 5. v1.1 → v1.5 (the ecosystem half)

| Ver | Codename | Items | Notes / risk |
|---|---|---|---|
| **v1.1** | **store-lite** | ① plugin gallery: `[Plugins] Gallery=` JSON index (in-repo, hash-pinned), one-click **download to `Plugins\`** with digest check + confirm dialog; ② seed wave of community plugins (§6: `emoji qr port cron epoch httpcode du`); ③ manifest v2: gallery metadata = same sidecar file; ④ **theme gallery** — shareable `[Theme]` packs as one `.tbtheme.json` (Wox theme-store signal, same digest-pinned rails) | supply-chain discipline: hash pins + HTTPS + pinned digest list in repo |
| **v1.2** | **bash mode** *(fulfills the tagline)* | **ConPTY session view**: `/bash` or second hotkey opens the drop-down console in terminal mode (Guake-style slide, themed, `[Bash] Shell=pwsh|cmd`), VT subset renderer (SGR colors, alt-screen, scrollback 2000), history bridged into `History.dat`, `sudo !!`, copy on select | **XL — biggest risk on the board.** Time-boxed spike first; fallback plan (decided at spike end): `wt -w _quake` handoff alias + history bridge, revisit real renderer for v1.5 |
| **v1.3** | brain, off the leash | ① **`ai` plugin** (typed-only): `[AI] Engine=ollama\|openai-compat`, user endpoint/model, `[AI] Enabled=0` default OFF, answers stream into the console line, every answer tagged `[net]`; ② `tr <text>` translation plugin (user endpoint, e.g. self-hosted LibreTranslate — no hard-coded vendor) | offline-first preserved: enabled-by-off, network only on a typed line, endpoint is user-owned |
| **v1.4** | config everywhere | ① `/sync push|pull` **WebDAV** (typed-only, PBKDF2+AES via bcrypt, `.bak` chain, diff preview before apply) — completes the USB/Git story with phone-safe backup; ② config diff viewer `/config diff`; ③ profile hardening (`/profile work`, machine overlay lint) | crypto in-core = bcrypt only; no new exe deps |
| **v1.5** | shelf & scripts | ① **quick-access shelf** — pinned commands/aliases strip under the console, Alt+1…9 (CmdPal-parity signal, own #28 pins finally); ② **universal actions** — right-click any dropdown row: Open location · Copy path/URL · Run as admin · Uninstall · Pin (Keypirinha's "miss #2"); ③ **Lua plugin** `Scripts\*.lua` via optional `tb_lua.dll` (ROADMAP #43 — scripts ride in a plugin, never the core); ④ multi-select rows (RI3 #224) | Lua vendoring is compile-time only; core stays interpreter-free |

Post-1.5 parking (v1.6+ candidates, already specced in RI3): Windows OCR `ocr` plugin
(WinRT, typed region), `startup` editor UI, per-desktop summon rules (#228), mini HUD
(#227, v1.x anytime), RTL deep pass (#221), CJK pinyin fuzzy (RI2 #102).

---

## 6. Plugin roster (words pre-checked unique against all 19 + host + each other;
`audit-words` enforces at build)

| Word | Plugin | Deps | Offline? | Effort | Ref / competitor |
|---|---|---|---|---|---|
| `emoji` | name→emoji to clipboard, `emoji <name> [n]`, table ~1–2 k rows embedded | none | ✅ | M | Raycast/Flow staple |
| `qr` | QR in console half-blocks + `qr save f.png` | gdi32 | ✅ | M | RI3 #242 |
| `port` | `port [N]` TCP/UDP→owning PID (`GetExtendedTcpTable`), `port kill N` behind danger-gate | iphlpapi | ✅ | M | Flow `portviewer` |
| `cron` | `cron 0 3 * * 1` → "Mondays 03:00" + next-5 runs | none | ✅ | S | RI3 #248 |
| `epoch` | epoch↔date both ways, tz-offset arg | none | ✅ | S | RI3 #300 |
| `httpcode` | 4xx/5xx explainer by code or words | none | ✅ | S | Flow `http-status-codes` |
| `du` | `du <dir> [depth]` top-N sizes, honest progress | none | ✅ | M | RI3 #233 |
| `untrash` | list/recycle-bin `trash list`, `trash restore <n>`, `trash empty` w/ danger-gate | shell32 | ✅ | M | RI3 #259 |
| `startup` | list Run-keys + Startup folder; enable/disable via opt-in confirm | advapi32 | ✅ | M | RI3 #235 |
| `cleantemp` | temp inventory → sizes, wipe behind danger-gate | none | ✅ | S | RI3 #234 |
| `wifi` | `wifi` SSID/quality, `wifi profiles`, `wifi qr` (typed reveal, masked default) | netsh spawn | ✅ | S | RESEARCH-PLUGINS unshipped list |
| `totp` | `totp <name>` 6-digit code, `[Totp]` names with **DPAPI**-sealed secrets, `totp add <name> <base32>` | bcrypt, crypt32 | ✅ | M | Flow TOTP (2FA without phone) |
| `color` | `color` pick-from-screen crosshair → hex, `color ff8800` preview chips | gdi32 | ✅ | M | RI3 #241, Flow `flow-picker` |
| `pkg` | `pkg search/upgrade <x>` via winget (typed-only execution) | none | ❌ typed | S | Flow `flowget` |
| `cmdbook` | personal command vault `[CommandVault]` categories + templates + `%s` (network-cmds only if user typed them) | none | ✅ | M | Flow `command-vault` — fits alias culture |
| `tr` | translate, user endpoint, `[Translate] Enabled=0` default | winhttp | ❌ typed | S/M | v1.3 |
| `ai` | chat/summarize via user endpoint (Ollama LAN first-class) | winhttp | ❌ typed | M | v1.3 |
| `shot` | `shot` region capture (crosshair shared with `color`), `shot now` full-screen → PNG to `[Shot] Folder=` + clipboard | gdi32 | ✅ | M | Wox v2.5 screenshot |
| `snap` | `snap <window> left/right/corners/grid` presets; `snap save/apply <name>` layouts best-effort (window identity is the flake — time-boxed) | user32 | ✅ | M | Wox window layouts / Raycast WM |
| `marks` | `marks <text>` browser bookmarks: Chrome/Edge `Bookmarks` JSON + Firefox exported `bookmarks.html`, offline read, Enter opens | none | ✅ | M | Wox browser-bookmarks; open-tab jump = UIA stretch, time-boxed |
| `media` | `media play\|pause\|next\|prev\|vol N`, `media now` track info via SMTC over COM | ole32 | ✅ | M | Wox media-player plugin |

Policy: **one** of these per wave (word-law), headless dry-run for anything with side
effects, ET rows for discoverability, checklist row count grows with each (currently
fits up to 19 comfortably — re-verify layout at #20–21).

---

## 7. New views (surfaces, not just commands)

| View | What it is | Where it rides | Effort | Version |
|---|---|---|---|---|
| **Bash/drop-down terminal** | Guake-style ConPTY session, themed, scrollback | host (v1.2) | XL | v1.2 |
| **/plugdebug** | per-plugin: answered lines, ms, word table, load errors, reload buttons | host window section (v0.36) | M | v0.36 |
| **/stats dashboard** | 14-day sparkline, top-10, class mix, plugin hit table | console line mode | S | v0.35 |
| **Preview pane** | dim detail strip under dropdown: file size/date, URL host, alias target | dropdown | M | v1.0 candidate, else v0.40 |
| **Section headers** | gray non-selectable "plugins · 4 / aliases · 12" rows in the dropdown | dropdown | M | v0.35 |
| **Onboarding cards** | first-run generated INI + welcome line-set + tour | host first-run | M | v0.38 |
| **Settings search** | fuzzy filter across pages+control labels (i18n-aware) | settings dialog | M | v0.38 |
| **Quick-access shelf** | pinned rows strip + Alt+1…9, `[Shelf]` INI | under the console | M | v1.5 |
| **Universal actions menu** | right-click dropdown row (open location/copy/admin/uninstall/pin) | dropdown rows | M | v1.5 |
| **HUD window** | borderless always-on-top clock/timer/clip-count chip strip, `hud` toggle | separate HWND | M | parking lot (fits any wave) |
| **Clipboard ring images** | image rows in `clip ring` (thumb + paste via CF_BITMAP) | cliphist plugin | L | v1.x |
| **Earcons/low-vision pack** | outcome sounds, fat caret, border flash | host `[Show]`/`[Sounds]` | S | parking lot |
| **Hotkey overview** | `hotkeys` line: the registered summon, Alt+digit/click/Tab/T tray contract, console chords — a printable cheat sheet (Wox hotkey-overview) | console line mode | S | parking lot |

---

## 8. Quality system that must grow with the version

- **Suites**: keep `test-vNNNN` per wave + full serial regression (today v0.180→v0.310);
  suites copy the **entire** plugin roster; headless contract asserts stay sacred.
- **word-law**: `audit-words.ps1` = hard fail in plugin build + CI step + gate; from v0.36 the
  same rule enforced at runtime via manifests ✅ (loader refuses a colliding DLL, `hostmin` too).
- **Release bytes**: every tag's zip re-gated on downloaded bytes, archived forever
  (`TypeBox-releases\`).
- **Crash isolation** (v0.36 ✅): guard around every plugin export call — RunString/ET since
  v0.35, Load/Unload since v0.36; faulting plugin →
  auto-disable + log + tray dot; `/plugdebug` shows the corpse.
- **Perf budget**: the §4b table is the law — `/perf` card from v1.0, counters from v0.36;
  idle CPU < 0.1 % (measured number printed in `/about`, RI #70);
  dropdown latency budget for layout-fallback + spell (both must stay O(rows)).
- **A11y** (v0.40): UIA text patterns on console+dropdown; NVDA smoke checklist in the suite log.
- **Matrix**: Win10/11 × 96/150/200 % DPI on every 5th release; Win8.1 smoke at v1.0.
- **i18n discipline**: new StrIds append-only at end of `src/i18n.h`; `--check-i18n` CI gate
  stays red-proof.

---

## 9. Risks & mitigations

| Risk | Mitigation |
|---|---|
| **ConPTY terminal renderer scope explosion** (v1.2) | 2-day spike with a go/no-go; documented fallback = `wt -w _quake` handoff + history bridge; VT subset spec written before code |
| OLE drop-out complexity (v0.37) | time-box; ship drop-in first (S), drop-out behind a spike decision |
| UIA rabbit hole | prototype NVDA read-through before committing the wave |
| AI expectations pressure vs offline-first law | default OFF, `[net]` tags on every net answer, endpoint user-owned, README states the policy |
| Gallery supply-chain | hash-pinned index, in-repo digest list, HTTPS-only, per-plugin confirm dialog |
| ABI drift after v1.0 | manifests sidecar only; ABI freeze declared in VERSIONING.md; break = major 2.0 |
| Single-maintainer bandwidth | 70/25/5 S/M/L ratio per wave; parking lot absorbs drift; every wave independently shippable |
| Plugin shadowing regression class (founding bug) | full-roster suites + audit-words + runtime manifest refusal — three independent layers |

---

## 10. Sources (research, Nov 2026)

- PowerToys Command Palette: learn.microsoft.com overview; devblogs Microsoft v0.97
  (Pinyin, RDP extension, custom web search, **drag-and-drop**, clipboard-history DnD);
  VSMag 2026-10 (shelf Alt+1…9, automation); Neowin 0.96.
- Flow Launcher plugin catalog (~1005 entries): layout-fix/layout-fallback, command-vault,
  webdav-backup, TOTP/2FA ×3, portviewer, flow-picker, qr ×2, emoji ×4, temp-cleaner,
  flowget/scoop, AI category (ollama, lm-studio, gemini, groq, openrouter), smart-calc.
- Raycast 2.0 (raycast.com/new), Raycast for Windows (MS Store, PCWorld, XDA "4 things
  PowerToys Run still can't" — store/snippets/window mgmt), technical deep dive.
- Keypirinha HN #39022891 (clipboard history + universal actions are the miss),
  r/software Wox threads.
- Quake demand: flyingpie/windows-terminal-quake, `wt -w _quake` (MS docs), r/commandline.
- ConPTY: devblogs "Introducing the Windows Pseudo Console"; winpty legacy notes.
- Internal: IDEAS-RESEARCH 1/2/3 (#1–300), UX-REVIEW, RESEARCH-PLUGINS-2026-11-18,
  PLUGIN_GALLERY, FEATURES (TypeAndRun parity), CHANGELOG.

*Maintained in-repo; update §4/§5 rows when a wave ships and move specifics into
ROADMAP-50.md item rows. This doc answers "what and in what order"; the per-wave
ROADMAP-50 rows answer "exactly how".*
