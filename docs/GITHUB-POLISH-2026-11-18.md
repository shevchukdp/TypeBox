# GitHub polish — 2026-11-18 (draft, not applied)

Working notes for the repo owner (GitHub-side settings cannot be changed from the repo; README
changes were made directly in `README.md` and are summarized in §4).

## 1. GitHub «About» box (Settings → About)

**Description (90 chars, fits the ~100-char box):**

```
Hotkey launcher + bash-style console for Windows — portable INI, offline-first, 14 plugins
```

Fallbacks if the em dash renders oddly:

- `Hotkey launcher + bash-style console for Windows - portable INI, offline-first, 14 plugins` (89)
- `Portable hotkey launcher & bash-style console for Windows. Offline-first. TypeAndRun lineage.` (94)

**Topics (paste as space-separated; GitHub normalizes):**

```
windows launcher hotkey run-launcher alfred-alternative powertoys-run portable offline-first productivity console ini plugins c cpp typeandrun
```

(`alfred-alternative` and `powertoys-run` are how people actually search for this category;
`typeandrun` keeps the lineage discoverable.)

**Website / Homepage:** `https://github.com/shevchukdp/TypeBox#readme` — until a GitHub Pages
landing exists. If Pages is ever enabled (docs/ rendered), switch to `https://shevchukdp.github.io/TypeBox/`.

## 2. Quick wins for the repo owner (GitHub UI, ~5 min each)

1. **Social preview image** — Settings → Social preview: upload a 1200×630 PNG (suggest: the
   `console-ctrlr.png` shot on a dark theme + "Ctrl+Shift+F2 → type → run" tagline + GPL-3.0
   badge); this is what shows on every link share on GitHub, X, Telegram, Discord.
2. **Set the About box + topics** — copy from §1 above; today the repo has no description, which
   kills discovery via topic pages.
3. **Issue templates** — add `.github/ISSUE_TEMPLATE/` with `bug_report.md` (must include TypeBox
   version + `TypeBox.log` excerpt), `feature_request.md` (points at docs/ROADMAP-50.md so ideas
   get vetted there first) and a `config.yml` with a link to docs/PLUGIN_API.md.
4. **Enable Discussions** — for plugin ideas and "how do I alias X" questions; keeps the issue
   tracker for real bugs, and the winning ideas flow into ROADMAP-50 the way v0.25/v0.26 waves did.
5. **Attach screenshots to releases** — the README now embeds
   `…/releases/download/v0.27.1/console-pos.png`; make sure each release carries its screenshot
   assets (v0.27.0 already ships console-ctrlr/tabs/day). Keeps the first impression working even
   after docs/screenshots churn.

## 3. Truth check performed before shipping the README

> Updated in the v0.28.0 wave: VERSION = 0.28.0, top CHANGELOG entry [0.28.0], and the plugin
> list grew a 14th — `weather` (`calc, units, codec, vol, cliphist, taskman, winlist, pwgen,
> timer, sysmon, util, note, yt, weather`). The About draft in §1 carries the new count.

- `VERSION` = **0.27.1**, top CHANGELOG entry = **[0.27.1] - 2026-11-17** → README headline says
  "Working today (v0.27.1)"; every bullet re-checked against CHANGELOG.md / docs/CONFIG.md.
- 13 plugins: `calc, units, codec, vol, cliphist, taskman, winlist, pwgen, timer, sysmon, util,
  note, yt` — matches CMakeLists targets and the v0.26 gate ("version/13-plugins boot").
- Default hotkey `Ctrl+Shift+F2`: `TypeBox.ini` `[HotKey] Key=113` (VK_F2) + Ctrl + Shift. ✅
- All 19 `docs/screenshots/*.png` referenced by the README exist on disk (verified by path).
- Offline-first stated in the intro and in the `yt` / `/update` bullets: network only on a typed
  command (`/update` wording matches docs/CONFIG.md §"/update": one ask, SHA-256 digest verified,
  INI never overwritten).
- All pre-existing README links kept (screenshots, docs, examples, roots blockquotes, badges);
  RU «Корни и вдохновение» blockquote kept verbatim, nothing translated.

## 4. README claims fixed in this pass (why)

1. **Direct MSVC build line was broken** — it lacked `/utf-8` and the `comdlg32/winhttp/bcrypt`
   libs. `/utf-8` is mandatory (CI comment: without it the released exe garbles 19/20 languages —
   CHANGELOG v0.14.x); `winhttp`/`bcrypt`/`comdlg32` are linked in `CMakeLists.txt` (the `/update`
   downloader + file dialogs), so the old `cl` line would not link. Fixed both MSVC and MinGW lines.
2. **Building section now leads with the canonical CI line** — `cmake -S . -B build -G "Ninja"
   -DCMAKE_BUILD_TYPE=Release && cmake --build build`, taken verbatim from
   `.github/workflows/build.yml`, and notes it also builds all 13 plugin DLLs (the old README only
   showed a single-file host build, so plugin sources were never built by the documented flow).
3. **Comparison table cell `Console / bash mode | ✅ (planned)` was self-contradictory** — changed
   to `console ✅ · bash mode planned`: the console popup ships today, the bash/terminal mode is
   still roadmap (docs/ROADMAP-50.md), so a plain ✅ would overclaim.
4. **`/update` was buried** in the internal-commands list only — promoted to a headline bullet and
   an Install/Update section with its real guarantees (typed-command-only network, SHA-256 vs
   published digest, INI never overwritten) per docs/CONFIG.md.
5. **Headless test mode was missing from the README entirely** although it shipped in v0.27.0
   (CHANGELOG) — added to the power-user console block.
6. **`pos reset` (new in v0.27.1) was missing from the `pos` bullet** (mentioned only in a lead-in
   sentence) — now in the bullet with the geometry-fix truth: slots no longer stretch the console,
   manual drag releases the slot, INI never rewritten (per CHANGELOG 0.27.1).
7. **Import/export wording was stale** — "(a UI action will wrap this)" kept implying vaporware
   while `/export` / `/import` commands already ship; now states the commands + the
   `TypeBox.ini.bak` live-reload behavior (docs/CONFIG.md).
8. **Added the missing link to docs/PLUGIN_GALLERY.md** — the plugin-development teaser (one C
   file, six cdecl exports, `gcc -shared -O2 -static …`, skeleton in `examples/plugin_calc/calc.c`)
   pointed only at PLUGIN_API.md before; the gallery is the actual "start here" doc.
9. **Screenshot policy**: kept every existing local screenshot reference (all verified present);
   added one pos row pointing at the **release asset** `releases/download/v0.27.1/console-pos.png`
   because `docs/screenshots/console-pos.png` on disk depicts the pre-fix v0.23 behavior; all other
   v0.27 shots (ctrlr/tabs/day) already exist locally and stay on relative paths.
10. **Emoji noise trimmed** from the feature list (decorative ✅/🔍/📊/🗂 removed); kept only the
    semantic 🟢🟡🔴 traffic-light and the ✅/❌ comparison-table marks. Section order normalized to:
    Intro → Screenshots → Why → Working today (+Planned) → Install/Update → Configuration →
    Plugins → Building → Roots & License → Docs; the Roots & inspiration blockquotes (EN + RU) were
    moved below the fold into "Roots & License" verbatim, including TypeAndRun / Evgeniy Galantsev /
    GPL-3.0 and the full inspirations list.
