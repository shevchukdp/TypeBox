# TypeBox — UI/UX & Visual-Design Review (v0.10.0)

Reviewed: `src/main.cpp` (PaintStatusBar, PaintDropDown/kDDClass, MakeCozyIcon, ApplyThemeVisual/kThemes,
SettingsCreate + page builders + Mk*/NumB/TxB/CkB, WM_DRAWITEM theme card, EditProc, ShowBox/HideBox,
WndProc, wWinMain) and the 11 screenshots in `docs/screenshots/`.
Evidence: `console.png` is the exact 720×56 client (pixel-probed), `dropdown.png` is 720×115 (= 3·37+4 → rowH = 37),
settings shots are the 640×460 dialog. Contrast ratios computed from `kThemes` (main.cpp:1478) with the sRGB WCAG
formula, reproducing `MixColor()` (main.cpp:1495) exactly. Line numbers refer to `src/main.cpp` unless noted.

**Verdict:** the bones are good — traffic-light anchor lamps, chip language, and the owner-drawn list are a real
identity. The problems are: (1) hand-tuned pixel constants that don't survive font metrics or light themes,
(2) several "first-run blindness" gaps (tray badge, unlit lamps, hotkey hint), (3) clipped statics on 4 settings
pages, and (4) fixed-px layout under `SetProcessDPIAware()`. All cheap to fix.

---

## A. Visual polish (graphics)

### A1. Status-strip baseline sits low vs lamps and chips — **visible in console.png (pixel-probed)**
Probe of the strip (window y 36–55): lamp ink spans y 42–50 (center 46), chip border spans 38–52, but the
Segoe UI −11 text cell is drawn at `y = rc.top + 3` (main.cpp:1620, 1644, 1651, 1658, 1675, 1685) and its actual
ink lands at **y 42–51** — i.e. ~1.5 px below lamp center, and the chip's 15 px height makes the top padding
(4 px) read larger than the bottom (1 px). The strip "sags".
**Fix:** replace all six hard-coded `y` with one `int ty = rc.top + (SB_H - tmHeight(g_sbFont))/2 - (tmExternalLeading>0?0:0)` —
or simplest: draw every strip string with `DrawTextW(..., DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX)` into a per-item
rect `{x, rc.top, x+w, rc.bottom}` and delete `y` entirely. If keeping TextOut, use `y = rc.top + 2` and `SB_H = 22`
(main.cpp:1603) so the 19 px text cell is truly centered.

### A2. Unlit lamps are ~invisible — kills the traffic-light idea
Probe: unlit amber = `#473B2D`, unlit red = `#47312F` on strip bg `#26221E` → **≈1.4:1**; the "ring" pen
`Mix(bg, white, 10)` (main.cpp:1630) is `#403B38`, also invisible. In console.png only the lit green reads;
a first-time user cannot see that there *are* three lamps. Unlit mix is 16 % (line 1629).
**Fix:** unlit fill `Mix(bg, lit[i], 32)`, ring `Mix(bg, white, 25)`, and keep lit at 100 % — the lit/unlit
duality then reads at a glance (target ≥ 2.5:1 for the unlit body so the shape is legible, not readable text).

### A3. Edit-box border is a hard system gray — theme seam
Probe: the edit's `WS_BORDER` (created at main.cpp:3151) renders a 1 px **`#646464`** line that ignores the theme
(muddy box in cozy-dark, harsh scratch on paper-light). Combined with the 2 px window margin and the 1 px
accent separator at strip top (probed `#AA7D4E`, correct per line 1611–1614), the console reads as *three
different rectangles* instead of one card.
**Fix:** drop `WS_BORDER`; in `WM_PAINT` (main.cpp:3171) stroke `Rectangle()` of the edit rect with a new
`g_line = Mix(g_effBg, g_effFg, 22)` pen (create alongside `g_bgBrush` in `ApplyThemeVisual`, main.cpp:1591).
While there, draw a 1 px `Mix(bg, black, 20)` hairline just above the separator to kill the edit/strip color seam.

### A4. Edit box clips the default 18 pt font (720×56 default is the problem child)
`WM_SIZE` hard-codes the edit to **24 px** tall (main.cpp:3160) and centers it in `above = H − SB_H`; at the
default `[Font] Size=18` the font height is `-MulDiv(18,96,72) = -24 px` → Consolas/Courier tmHeight ≈ **29–31 px**
> 24 px control. Ascenders/descenders and the caret bottom are clipped (visible in console.png: `*` in `12*3+40`
touches the cell top). The dropdown already does it right (`g_ddRowH = tm.tmHeight + 8`, main.cpp:1837) — the
edit does not.
**Fix:** in `WM_SIZE`, `int eh = tm.tmHeight + 6;` (via `GetTextMetricsW` with the applied font), `MoveWindow(g_edit, 2,
(above-eh)/2, W-4, eh, TRUE)`; raise default `height` to 60 (main.cpp:71) so `(36-37)/2` stays integral.

### A5. Chip padding / rhythm in PaintStatusBar
Chip rect is `x … x+cs.cx+10` with text at `x+5` (main.cpp:1671–1675) → **5 px side padding**, but the advance is
`x += cs.cx + 18` → **8 px inter-chip gap**: padding < gap, so chips look like they're floating away from their own
text (clear in console.png between "linux (Tab cycles)" and "cozy-dark"). The `>_"` advance is a magic `x += 17`
(line 1652) that silently breaks the moment `g_sbFont` changes, and lamp spacing is 12 px pitch + 4 px tail
(lines 1627–1639). Right cluster uses 8 px margin vs 7 px left (lines 1626, 1685).
**Fix:** define `SB_PAD=6, SB_GAP=10`; chip `w = cs.cx + 2*SB_PAD`, text at `x+SB_PAD`, advance `cs.cx + 2*SB_PAD + SB_GAP`;
replace the `17` with `SIZE gs; GetTextExtentPoint32W(dc, L">_", 2, &gs); x += gs.cx + SB_GAP;`; make both margins 8.

### A6. Dropdown: row height, left inset vs console, double separator, weak hover
- **rowH = 37 px** at default font (probed from dropdown.png: 115 = 3·37+4) — too tall next to a 56 px console;
  8 rows (ddLines=8) = 300 px of list. `rowH = tmHeight + 4` gives ~33 px, tighter leading, still touch-safe.
- Row text starts at `rr.left + 6` = window x **8** (main.cpp:1850, 1858), while console text starts at ≈x 4
  (edit at x=2 + 1 px border) → suggestions are visibly *indented past* the typed line. Use `+2` and align with
  the new custom hairline from A3.
- The list opens at `y = wr.bottom + 1` with a full-perimeter accent `FrameRect` `Mix(bg, accent, 55)`
  (main.cpp:1842–1845, 2001): console-bottom → 1 px gap → orange frame reads as a **double rule**. Suppress the
  top edge (three single `FillRect` edges) or frame only bottom/sides.
- Hover/selection: selected fill is `Mix(bg, accent, 24)` and text goes 72 % → 100 % (main.cpp:1853, 1863) —
  in dropdown.png the difference is a whisper. Use 32 % fill + left 3 px accent tick + caret-style `▸` on the
  selected row so keyboard and mouse selection are unmistakable.

### A7. Tray badge is illegible at 16 px — MakeCozyIcon
`MakeCozyIcon` draws one 32×32 frame (main.cpp:1509–1559): `>_` at `CreateFontW(-13 …)` and state dot
`Ellipse(20,1,31,12)`. The shell downscales to 16 (or 20/24 at 125/150 % DPI) → the glyph collapses to ~6 px
of antialiased mush, and the 11×11 dot smears into a 4 px blob clipped by the rim (its center (25.5, 6.5)
sits outside R=15 at that corner — visible bite-mark in the badge). Idle rim `Mix(card, accent, 45)` (line 1566)
is too dim on a dark taskbar.
**Fix:** draw per-size (add S=16 variant: rim pen 1, glyph `-8`, dot `(9,0,15,6)`; S=32 keeps current), assemble a
multi-image HICON, and raise idle rim to 60 %.

### A8. Theme preview card is a flat rectangle pretending to be the console
WM_DRAWITEM (main.cpp:2339–2366): 430×96, 4 px accent bar, one 16 px Consolas line at `top+24`; bottom half empty
(settings-theme.png). It previews none of what the theme actually changes — strip, lamps, chips, tray — and the
text baseline (ty=66) leaves 30 px of dead space under it.
**Fix:** draw a mini-console in the card: mock strip `bottom-20` with `Mix(t.bg,black,12)`, three 6 px lamps, a
mock chip `Mix(t.bg,accent,18)`, and a 24 px tray badge circle bottom-right using the same routine as
MakeCozyIcon. This turns the Theme page into a live "what am I buying" card.

### A9. Settings dialog: column grid breaks on Aliases; several statics clip
- `NumB/TxB` use a disciplined grid: label `x..x+150`, control `x+155`, edits 80/260 wide (main.cpp:2178–2194).
  **Aliases ignores it**: labels at `ax=170` width 60, controls at `ax+55` (main.cpp:2697–2707) → 5 px collision —
  visible in settings-aliases.png: "Window" fuses into the combo ("Window▾Normal"). Move controls to `ax+65`,
  or `SS_RIGHT` the labels.
- Clipped one-line statics (screenshots prove all four):
  - plugins hint `STR_PL_HINT` "Dlls live in the portable Plugins\ folder." in 190 px (main.cpp:2738) —
    shot shows "…live in the portable" cut; give it `w=230, h=34` (two lines) or `SS_LEFTNOWORDWRAP|SS_EDITCONTROL`.
  - env note `STR_EN_NOTE` in 210 px at `vx` (main.cpp:2775) — cut at "folders extend PATH for aliases,";
    use `w=224, h=40` or full width `16..446` under the list.
  - theme hint `STR_TH_HINT` ~73 chars in 430 px (main.cpp:2794) — last words clipped; `h=36` for wrap.
  - plugin info box clips the Homepage URL mid-token (main.cpp:2732–2733, 260×150) — widen to 270 and make
    Homepage an `SYS_LINK` or SS_NOTIFY + `WM_LBUTTONUP` → ShellExecute.
- Button row: fixed `75 px` ×3 at 380/461/542 (main.cpp:2826–2828) — right edge 617 vs client ~634, so buttons
  are neither flush nor padded consistently, and long locales (DE "Übernehmen") clip. Size to text
  (`GetTextExtentPoint32` + 24) and right-align the row to client width − 12. (Positive note: `CkB` width 380
  at main.cpp:2174 gives a generous full-row click target — keep.)
- Page list 140 px (main.cpp:2551) is adequate for RU/UK but tight for some locales; test with the longest string.

### A10. Font page preview lies about the console font
The preview static is created with **`VARIABLE_PITCH`** and the raw `g_draft.fontName` (main.cpp:2613–2621), while
the console and dropdown use `FIXED_PITCH | FF_MODERN` + `ConsoleFontSuggestion()` (main.cpp:1420–1427, 1828–1831,
i18n.h:1003). settings-font.png shows a proportional UI-ish "Aa Bb Cc" for a monospace console. Also, in that same
screenshot **Font name shows `cozy-dark`** — the binding is correct in current code (`TxB(…, &Settings::fontName)`,
main.cpp:2608; INI key `[Font] Name`, main.cpp:513/2888), so this is either an old build or INI data corruption;
ship a `--test-settings` assertion that `[Font]Name` round-trips, and add `CF_FIXEDPITCHONLY` to the ChooseFont
flags (main.cpp:2406). Preview must call `ConsoleFontSuggestion` + FIXED_PITCH and show the *effective* face.

### A11. Tray/state dot, hotkey page polish
Hotkey page (main.cpp:2623–2634): a read-only edit + raw VK decimal edit reads like a debug form; nothing marks
"capture mode" (see B6). Delete the VK edit (power users have the INI), give the capture edit a themed accent
border + "press keys…" ghost text while focused.

### A12. Clock cluster micro-alignment + cryptic sound glyph
`♪` vs `×` (main.cpp:1681): `×` for "sound off" is unreadable at 11 px; use `♪̶`-style two-state
(`\u266A` + strike rect, or `♪ off` text). Right margin 8 vs left 7 (A5), and the clock should be drawn with the
same tabular figure advance (`GetTextExtentPoint32` of `"00:00"` once) so the cluster doesn't jitter on minute change.

---

## B. Interaction / UX

### B1. First-run discoverability is the biggest gap
Nothing in the runtime UI teaches Tab / ↑↓ / Esc / Shift+Enter. The balloon is generic
"Press the hotkey to open…" (i18n.h:132) — it doesn't even interpolate the *actual* hotkey; the cue is empty because
the edit has no placeholder.
**Fixes (all cheap):** `EM_SETCUEBANNER` on `g_edit` (after creation, main.cpp:3153): "Type a command — Tab completes,
Enter runs, Esc hides"; tray tip (main.cpp:774) and balloon become `"TypeBox — Ctrl+Shift+F2"`; add a one-line
footnote hint on the Appearance page. Shift+Enter (kill last alias, main.cpp:3068–3089) is completely invisible —
see B7.

### B2. Dropdown navigation goes blind past `ddLines`
`DdUpdate` sizes the window to `min(items, ddLines)` rows (main.cpp:1996–1998), but ↑/↓ in EditProc wrap over the
**full** `g_ddItems.size()` (main.cpp:3097–3113). With 12 matches and Lines=8, selection rows 8–11 have no visible
highlight — Enter then executes an invisible row. Add `g_ddTop` scroll offset (scroll selection into view) or cap
navigation to the visible window. Also: no mouse-wheel handler in DropDownProc (main.cpp:1871) — add
`WM_MOUSEWHEEL` → adjust `g_ddTop`.

### B3. Hover-then-Enter executes the hovered row (surprise execution)
`WM_MOUSEMOVE` sets `g_ddSel` (main.cpp:1887–1893); Enter with `g_ddSel >= 0` runs that row (main.cpp:3088). A
pointer left parked over the list makes Enter execute a *history item the user never chose to select*. Additionally
single `WM_LBUTTONUP` **runs** the command (main.cpp:1895–1898) — for a launcher that fires `cmd /c`, first click
should *complete* (like Tab), second click/double-click runs. And reset `g_ddSel = -1` when the mouse leaves
(`WM_MOUSELEAVE` with `TrackMouseEvent`) so keyboard state can't be contaminated by pointer history.

### B4. Missing edit keys
Standard EDIT has no Ctrl+A (select all) — users will press it; Ctrl+Z is remapped to last-run (main.cpp:3126) which
loses normal undo (acceptable, document it); PgUp/PgDn do nothing (history jump ×10 would shine with `historyMax=200`).
Add all three in EditProc's `WM_KEYDOWN`.

### B5. Theme combo doesn't live-update the preview card
There is **no `CBN_SELCHANGE` handler for `g_themeCombo`**; `WM_COMMAND` falls through to `return 0`
(main.cpp:2369–2496) and the only `InvalidateRect(g_themePrev…)` lives in the *storer* lambda, which runs only on
OK/Apply (main.cpp:2784–2790). Change scheme → card keeps the old theme. One case in SettingsProc fixes it.

### B6. Hotkey capture needs honesty
HkCapProc (main.cpp:2101–2122): Esc restores text (good), but there's no visual capture state, Win can be a modifier
but never the *trigger* key, modifier-only combos silently no-op, and — worst — **registration failure is invisible**:
`ReloadHotkey` just logs (main.cpp:3140–3145), dialog is already gone. Validate live on capture:
`RegisterHotKey(tmp…)/UnregisterHotKey`, and show "already taken / needs a modifier" in red under the box.

### B7. Shift+Enter kill has zero affordance
`g_aliasProc` running is invisible in the console. When `g_aliasProc` is alive, light an extra strip state:
e.g. the amber lamp pulses and a `⚡ running — Shift+Enter kills` micro-label appears (PaintStatusBar already has a
label slot, main.cpp:1640–1645). Without this the feature is dead code to users.

### B8. AutoHide counts from open, not from last keystroke
Timer 1 starts once in ShowBox (main.cpp:1459) and never resets while typing — "Idle seconds" isn't idle. Restart it
in the `EN_CHANGE` handler (main.cpp:3222). Also clock timer is 15 s (main.cpp:3400) → up to 59 s stale minutes; use
`SetTimer` at (60 − now%60)·1000 rescheduled each tick.

### B9. Tray menu too thin for a tray-first app
Only Show/Settings/INI/Exit (main.cpp:2010–2021). "Show" label never reflects state (handler toggles, main.cpp:3215).
Add: checked **Pause hotkey** (huge for presentation mode), **Reload config** (`/reread` exists already), **Open
folder**, and a submenu jumping to any settings page (OpenSettings takes a page arg, main.cpp:3026). Middle-click → toggle.

### B10. Contrast — measured, several failures vs 4.5:1
sRGB ratios computed from `kThemes` + `MixColor` semantics (strip = `Mix(bg,black,12)`; chips: bg 18 %, border 55 %,
text 85 % of accent; secondary `Mix(bg, fg, 62)`):

| theme | fg/console | chip text/bg | secondary (counts · ♪ · lang · clock) | accent `>_` on strip |
|---|---|---|---|---|
| cozy-dark | 12.5 ✓ | **4.46 ✗(borderline)** | 6.1 ✓ | 9.4 ✓ |
| paper-light | 11.5 ✓ | **2.43 ✗** | **2.87 ✗** | **2.83 ✗** |
| midnight-blue | 11.9 ✓ | **3.50 ✗** | 5.8 ✓ | 6.5 ✓ |
| nord-frost | 9.3 ✓ | **3.47 ✗** | 5.0 ✓ | 6.8 ✓ |
| solarized-dark | 4.73 ✓ | **2.98 ✗** | **2.82 ✗** | 5.0 ✓ |
| solarized-light | 4.99 ✓(borderline) | **2.08 ✗** | **1.85 ✗✗** | **2.26 ✗** |
| forest-night | 11.8 ✓ | **4.28 ✗** | 5.8 ✓ | 8.8 ✓ |

The `MixColor(bg, fg, 62)` secondary (counts, ♪, language, **the clock**) is *information*, not decoration, and it
fails on paper-light, solarized-light, solarized-dark. Chip text fails on **5 of 7** themes.
**Fix:** bump secondary to `Mix(bg, fg, 82)` on light themes (pick by `lum(bg) > 0.4`), and for chip text use
`Mix(bg, accent, 85)` only when it clears ≥ 4.5:1 against `chipBg`, else clamp toward `bg`/`accent` automatically —
one 10-line contrast-guard helper next to `MixColor` (main.cpp:1495) fixes all 7 themes forever.

### B11. RTL is half-wired
`SetProcessDefaultLayout` + `WS_EX_LAYOUTRTL` on the main window (main.cpp:3338, 2984–2992) mirror the child edit,
but `PaintStatusBar`/`PaintDropDown` paint fixed LTR (`x = rc.left + 7` … right cluster at `rc.right − w − 8`). In
Arabic you get a right-aligned caret over a left-aligned strip — the clock ends up farthest from the reading start,
chips read in the wrong order. Either mirror the strip painting when `LangRtl()` (build the item list, then draw
right-to-left) or pin the strip to LTR *by design* and document it. Also: the live language switch doesn't re-mirror
an already-open settings dialog, and the language list static mixes `العربية`/`हिन्दी` inline between `·`
separators — bidi reordering mangles the separators (visible risk in settings-language.png); one name per row or
`\u200E` isolation marks fix it.

### B12. DPI: fixed pixels vs System-DPI-Aware is a guaranteed breakage at ≥150 %
`SetProcessDPIAware()` (main.cpp:3287) is *system*-aware; every constant is raw pixels: 720×56, SB_H=20, edit 24,
dialog 640×460 + all page coordinates, chip 15 px, tray 32 px, and `-MulDiv(size, 96, 72)` hard-codes 96 DPI
(main.cpp:1421, 1828). On a 4K laptop at 200 % the console is a 360×28-pt postage stamp and the settings dialog
clips its button row. **Fix:** `SetProcessDpiAwarenessContext(DPI_AWARE_PER_MONITOR_AWARE_V2)` (dwmapi-free, Win10+)
plus a `S(px)` macro = `MulDiv(px, GetDpiForWindow(w), 96)` applied to the strip/dialog geometry at create/resize.

### B13. Multi-monitor centering straddles the seam
`ShowBox` centers on `GetWindowRect(GetDesktopWindow())` = the *virtual* desktop (main.cpp:1434–1440); with two
monitors of different sizes the box lands across the bezel. Use `MonitorFromPoint(GetCursorPos, MONITOR_DEFAULTTONEAREST)`
and center within its work area (also fixes B12's per-monitor awareness story).

### B14. Flicker & repaint churn
Every keystroke classifies the anchor and `InvalidateRect(…, TRUE)` erases + repaints the whole strip
(main.cpp:3225–3231), and clock tick does the same; `PaintStatusBar` fills/pens/brushes directly on the window DC.
Double-buffer the strip into a memory DC (build once per state change) and blit — also unlocks the C2 fade-in.

### B15. Settings findability & defaults
14 undecorated list entries; no filter box (type "sound" → jump). The Console page exposes raw decimal COLORREFs
(`ConsoleBG=3355443`-style, main.cpp:2599–2600) — replace with a "Use theme colors" checkbox + swatch. Defaults
review: 720×56 ✓ width, height must grow (A4); Font 18 ✓; hotkey `Ctrl+Shift+F2` — keep it (Ctrl+Space collides
with IMEs, Alt+Space with the window menu), but surface it everywhere (B1). `ShowConsoleOnStart=true` +
`HideOnDeactivate=true` ✓; empty-line dropdown = history ✓ nice.

---

## C. Cozy-look evolution (win / effort / risk)

| # | Proposal | Win | Effort | Risk |
|---|---|---|---|---|
| C1 | **Win11 rounded corners**: `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)` at startup (load dwmapi at runtime; no-op on Win10). Pair with `WM_NCCALCSIZE`-free borderless paint — the corner radius then matches native flyouts. The dropdown (also WS_POPUP) gets the same call so console + list read as one rounded card. | High — single biggest "modern" signal for pennies | Low (~15 lines) | Low; Win10 unaffected; verify with `transparent < 255` (layered) combos |
| C2 | **Fade/slide-in on ShowBox**: `AnimateWindow(g_edit-parent, 90, AW_BLEND \| AW_ACTIVATE)` — or, since the window may already be layered, ramp `SetLayeredWindowAttributes` 0→255 over 3 × 15 ms timers; 4 px slide for `centerY` boxes. Only on open, never on close (hide must stay instant). | Medium — feels premium, signals "app is alive" | Low (5–10 lines) | Low–med: must skip on RDP / `transparent<255` interplay; never delay focus |
| C3 | **Strip depth + seam**: 1 px vertical hairline `Mix(bg,black,25)` above the accent separator, separator itself a 2-stop horizontal gradient (accent 60 % → 25 % from center), and a 1 px bottom shadow `Mix(bg,black,25)` on the window. Removes the "three stacked rectangles" look from A3 in one move. | Medium — makes the flagship screenshot cohesive | Low (~10 lines in PaintStatusBar) | Low; test with explicit `[Color]` overrides |
| C4 | **Type-glyphs for dropdown rows & lamps** via Segoe UI Emoji/Segoe Fluent Icons: 🌐 URL, 📁/📄 files, ⚙ alias, 🔧 shell, 🕘 history, ▸ current — 16 px column at row left, and a 10 px glyph beside each lamp label. GDI `DrawTextW` gives monochrome outline glyphs (acceptable, cozy); color emoji needs DirectWrite — do monochrome first. | Medium-high — turns the list from text-wall into a scannable menu, reinforces lamp semantics | Medium (~60 lines: glyph map + hit-test inset) | Med: RTL + font fallback (some CJK systems lack the glyphs → fall back to bullet) |
| C5 | **Accent auto-derivation + WCAG palette tokens**: `Theme` gains `secondary/chip/chipText/sep` computed once in `ApplyThemeVisual` from `(bg, fg, accent)` with the contrast guard from B10; add theme `auto` = derive accent from the Windows accent color (`DWM::AccentColor` registry, fallback cozy amber) so TypeBox matches the user's wallpaper mood. | High — "cozy" becomes *personal*; kills the whole B10 bug class | Medium (~80 lines + Settings combo entry) | Med: OS-accent can be unreadable — always clamp through the guard; registry read can fail → default amber |

---

## D. Quick wins (ranked by impact ÷ effort)

1. **Cue-banner hint in the console** — `SendMessageW(g_edit, EM_SETCUEBANNER, TRUE, L"Type a command — Tab completes · Enter runs · Esc hides")` after main.cpp:3153. Fixes discoverability of Tab/Enter/Esc for free.
2. **Contrast guard for strip text** — in PaintStatusBar raise `Mix(bg,fg,62)` → theme-aware ≥ 4.5:1 (lines 1657, 1684) and clamp chip text (line 1674); saves paper-light/solarized-light from 1.85:1 illegibility (B10).
3. **Edit height = tmHeight + 6** in `WM_SIZE` (main.cpp:3160) instead of the fixed `24` — stops the 18 pt clipping and keeps default 720×56 honest (A4).
4. **Repaint the theme card on combo change** — add `CBN_SELCHANGE` for `g_themeCombo` in SettingsProc's `WM_COMMAND` (main.cpp:2369) calling `InvalidateRect(g_themePrev, …)` (B5, 3 lines).
5. **Make unlit lamps visible** — `Mix(bg, lit[i], 32)` + ring 25 % white (main.cpp:1629–1631); the traffic-light feature becomes usable (A2).
6. **Put the real hotkey in the tray tip & balloon** — `tip = "TypeBox " + WVersion() + " — " + HotkeyText(g_hotkey)` in `AddTrayIcon` (main.cpp:774–780); new users learn the shortcut from the OS UI itself (B1).
7. **Un-clip the four settings statics** — widen/wrap `STR_PL_HINT` (main.cpp:2738), `STR_EN_NOTE` (2775), `STR_TH_HINT` (2794), plugin Homepage (2732); each is a one-line geometry fix, and settings-aliases/env/plugins/theme shots all show truncation today (A9).
8. **Dropdown scroll offset + wheel** — add `g_ddTop`, scroll selection into view in ↑/↓ (main.cpp:3097–3113) and `WM_MOUSEWHEEL` in DropDownProc (1887): > 8 matches stop being invisible (B2).
9. **Custom 1 px themed hairline instead of `WS_BORDER` on the edit** (create at main.cpp:3151, stroke in WM_PAINT 3171) with `g_line = Mix(bg, fg, 22)` from `ApplyThemeVisual` — removes the gray `#646464` seam in every theme (A3).
10. **Ctrl+A / PgUp / PgDn in EditProc** (main.cpp:3066 switch) and `BS_DEFPUSHBUTTON`/`BS_DEFPUSHBUTTON|BS_ICON`-style default on OK + explicit Cancel default-key in `MkButton`/buttons (2149, 2826) so Enter/Esc do the expected thing in both windows (B4 + dialog polish).

*Close runner-up:* reset the AutoHide timer on `EN_CHANGE` (main.cpp:3222) — one line, stops "it vanished while I was typing".

---

## Top 5 do-next

| # | Item | Why now | Cost |
|---|---|---|---|
| 1 | Contrast guard for strip secondary text + chip text (B10 / D2) | Fails WCAG on 4–5 of 7 themes incl. clock; brand risk | ~20 lines |
| 2 | Edit height tmHeight+6 + cue banner + lamp visibility (A4/B1/A2 / D1,D3,D5) | Visible on every single open; default settings currently clip their own font | ~25 lines |
| 3 | Per-size tray badge (A7) | The tray is the app's home; the badge is mush today at 16 px | ~30 lines |
| 4 | Dropdown: hover-execute guard, scroll offset, tighter rowH & inset (B2/B3/A6) | Correctness (invisible Enter) + the feature's visual polish | ~40 lines |
| 5 | Per-Monitor V2 DPI + MonitorFromPoint centering + un-clip settings statics (B12/B13/A9) | High-DPI users get an unusable dialog; screenshots prove clipping | ~60 lines |

---

### Observed data/encoding defects (flag for owners, not design)
- settings-font.png shows **Font name = `cozy-dark`** — binding is correct in current code; add a settings round-trip
  self-test (`--test-settings` at main.cpp:3037 already exists — extend it) and audit any migration code touching `[Font] Name`.
- plugins page shows **garbled `в Б`** in calc's description — plugin ANSI `GetInfo` through `FromAnsi` (main.cpp:2249–2254) on
  a UTF-8-authored calc.c; standardize plugin metadata to UTF-8 (or CP_ACP in the sample).
- "Dils live in the portable…" clipping (i18n.h:159) — fix string + box (A9).

### What's already right (keep)
Row-wide 380 px checkbox hit targets (CkB, main.cpp:2174); dropdown `WS_EX_NOACTIVATE` ownership so the caret never
leaves the edit (main.cpp:1986); Esc-two-stage (list, then hide, main.cpp:3091–3093); Enter-selects-then-runs parity;
traffic-light *semantics* (own/found/unknown) are a genuinely good idea — A2 just makes them visible;
`IsDialogMessageW` + Esc guard for the capture edit (main.cpp:3414–3420).
