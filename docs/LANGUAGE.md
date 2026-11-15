# TypeBox Localization

Everything user-visible goes through one accessor (`L(STR_x)` in
[`src/i18n.h`](../src/i18n.h)), so the whole UI can change language at once:
tray menu + balloon tip, `/help`, `/about`, and every label, checkbox, button and
page name of the 13-page settings dialog.

## Choosing a language

```ini
[UI]
Language = auto     ; or: en ru uk de fr es pt it zh ja ko ar tr pl vi hi id sw af am
```

- `auto` (default) uses the Windows UI language
  (`GetUserDefaultUILanguage`, with `GetUserDefaultLangID` as fallback);
  region suffixes are accepted (`ru-RU`, `zh-CN` …).
- Set it in the INI, or use **Settings → Language** — the combo lists all 20
  languages in their native names; **Apply** switches immediately.
- Language is also logged at startup: `language: ru (auto)`.

## Built-in coverage

| Tier | Languages | Coverage |
|---|---|---|
| Complete | English, Русский, Українська, Deutsch, Français, Español, Português, Italiano | every string |
| Core | 中文, 日本語, 한국어, العربية, Türkçe, Polski, Tiếng Việt, हिन्दी, Bahasa Indonesia, Kiswahili, Afrikaans, አማርኛ | main UI; anything missing falls back to English |

The fallback chain for every string: `lang\<code>.ini` override → compiled pack → English.

## Right-to-left & fonts

- **العربية** flips the drop-down to RTL (`SetProcessDefaultLayout(LAYOUT_RTL)` before
  window creation + `WS_EX_LAYOUTRTL`), and Apply toggles it live.
- When the console font is still the default *Courier New*, TypeBox suggests a
  script-capable face per language: Microsoft YaHei UI (zh), Yu Gothic UI (ja),
  Malgun Gothic (ko), Segoe UI (ar), Nirmala UI (hi, am). Pick any font explicitly on
  the Font page and your choice always wins.

## Community translation packs (portable)

Drop `lang\<code>.ini` next to `TypeBox.exe` — it overrides the compiled pack:

```ini
; lang\ru.ini — UTF-8 or UTF-16, ';' or '#' comments
pg_console = Консоль
ok         = ОК
tray_show  = Показать / Скрыть
```

Key names are the `kKeyNames` array in [`src/i18n.h`](../src/i18n.h) (one per UI
string, in `StrId` order). Unknown keys are ignored; empty values fall back.

## Adding strings (contributors)

1. Append a `STR_*` id **at the end** of the `StrId` enum and a matching key name to
   `kKeyNames` (older packs zero-init the tail → English fallback, never broken).
2. Add the English text at the end of `kEn`; other packs may stay empty.
3. Never compare localized text in code logic — the settings dialog dispatches buttons
   by the same `L()` strings it created them with, keep it that way.
