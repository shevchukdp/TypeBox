// =================================================================== i18n
//
// TypeBox localization. All user-visible strings go through L(STR_x).
//  - English is the compiled-in fallback; other languages are compiled-in
//    packs, optionally overridable by the portable  lang\<code>.ini
//    (key=value, UTF-8/UTF-16, ';' comments) for community translations.
//  - [UI] Language = auto | code   (see kLangs). Auto detects the UI LANGID.
//  - Arabic switches windows to right-to-left (WS_EX_LAYOUTRTL).
//  - Console font auto-falls back to a script-capable face when the user
//    never changed the default Courier New.
//
// Languages: en ru uk de fr es pt it | zh ja ko ar tr pl vi hi id sw af am
// (European packs are complete; the others cover the core set — anything
//  missing transparently falls back to English.)

#ifndef TYPEBOX_I18N_H
#define TYPEBOX_I18N_H

#include <map>
#include <string>
#include <vector>

enum StrId {
    STR_OK = 0, STR_APPLY, STR_CANCEL,
    STR_TRAY_SHOW, STR_TRAY_SETTINGS, STR_TRAY_CONFIG, STR_TRAY_EXIT, STR_BALLOON_START,
    STR_DLG_TITLE,
    STR_PG_APPEARANCE, STR_PG_CONSOLE, STR_PG_FONT, STR_PG_HOTKEY, STR_PG_EXEC,
    STR_PG_EASYTYPE, STR_PG_HISTORY, STR_PG_LOG, STR_PG_ALIASES, STR_PG_SOUNDS,
    STR_PG_PLUGINS, STR_PG_ENV, STR_PG_LANG,
    STR_AP_START, STR_AP_HIDEFOCUS, STR_AP_TRAY, STR_AP_BALLOONS, STR_AP_AUTOHIDE,
    STR_AP_IDLESEC, STR_AP_PRIORITY,
    STR_CO_WIDTH, STR_CO_HEIGHT, STR_CO_X, STR_CO_Y, STR_CO_CX, STR_CO_CY,
    STR_CO_ALPHA, STR_CO_BG, STR_CO_FG,
    STR_FO_NAME, STR_FO_SIZE, STR_FO_BOLD, STR_FO_APPLY_EN, STR_FO_CHOOSE,
    STR_HK_HINT, STR_HK_CURRENT, STR_HK_VK,
    STR_EX_INTERNAL, STR_EX_ALIASES, STR_EX_WWW, STR_EX_EMAIL, STR_EX_FILES,
    STR_EX_SHELL, STR_EX_PLUGINS,
    STR_ET_MODE, STR_ET_OFF, STR_ET_EASY, STR_ET_LINUX, STR_ET_INT, STR_ET_AL,
    STR_ET_SYS, STR_ET_HIST,
    STR_HI_MAX, STR_HI_UNDO, STR_HI_INT, STR_HI_AL, STR_HI_WWW, STR_HI_MAIL,
    STR_HI_PATH, STR_HI_PLG, STR_HI_OTHER, STR_HI_CLEAR,
    STR_LO_ENABLE, STR_LO_FILE, STR_LO_MAXKB, STR_LO_OVERFLOW, STR_LO_KEEP,
    STR_AL_NAME, STR_AL_ACTION, STR_AL_DIR, STR_AL_WINDOW, STR_AL_ADD,
    STR_AL_UPDATE, STR_AL_DELETE, STR_AL_HINT,
    STR_SO_PLAY, STR_SO_INTERNAL, STR_SO_ALIAS, STR_SO_SYS, STR_SO_URL,
    STR_SO_MAIL, STR_SO_FILE, STR_SO_FOLDER, STR_SO_PLUGIN, STR_SO_SHELL,
    STR_PL_LOAD, STR_PL_HIST, STR_PL_RESCAN, STR_PL_UNLOAD, STR_PL_ABOUT,
    STR_PL_OPTIONS, STR_PL_HINT,
    STR_EN_PATHHINT, STR_EN_VARSHINT, STR_EN_ADDVAR, STR_EN_UPDVAR, STR_EN_DELVAR,
    STR_EN_NOTE,
    STR_LA_LABEL, STR_ABOUT_TEXT, STR_HELP_HEAD,
    STR_EN_VALUE, STR_LA_AUTO,
    STR_AL_WN_NORMAL, STR_AL_WN_MAX, STR_AL_WN_MIN, STR_AL_WN_HIDDEN,
    STR_LA_HINT,
    STR_PG_THEME, STR_TH_SCHEME, STR_TH_HINT,
    STR_AN_PATH, STR_AN_UNKNOWN, STR_AN_OFF,
    STR_DD_SHOW, STR_DD_ROWS, STR_DD_EMPTY, STR_FT_FUZZY,
    STR_CUE, STR_AN_WEB,
    // v0.14: right-click menu on the console itself (TypeAndRun had one too).
    // Translations live in kCtxItems below, English is the fallback.
    STR_CTX_UNDO, STR_CTX_CUT, STR_CTX_COPY, STR_CTX_PASTE, STR_CTX_SELALL,
    STR_CTX_SETTINGS, STR_CTX_ALIASES, STR_CTX_PLUGINS, STR_CTX_THEME, STR_CTX_LANG,
    STR_CTX_RELOAD, STR_CTX_INIDIR, STR_CTX_ABOUT, STR_CTX_HELP, STR_CTX_HIDE,
    STR_CTX_EXIT,
    // v0.15: theme studio — custom themes (colors, border, alpha, font, size).
    STR_TH_BG, STR_TH_TEXT, STR_TH_ACCENT, STR_TH_BORDER, STR_TH_BORDERW,
    STR_TH_NAMELBL, STR_TH_SAVEAS, STR_TH_DEL, STR_TH_PORTNOTE,
    STR_LAST
};

static const char* const kKeyNames[STR_LAST] = {
    "ok", "apply", "cancel",
    "tray_show", "tray_settings", "tray_config", "tray_exit", "balloon_start",
    "dlg_title",
    "pg_appearance", "pg_console", "pg_font", "pg_hotkey", "pg_exec",
    "pg_easytype", "pg_history", "pg_log", "pg_aliases", "pg_sounds",
    "pg_plugins", "pg_env", "pg_lang",
    "ap_start", "ap_hidefocus", "ap_tray", "ap_balloons", "ap_autohide",
    "ap_idlesec", "ap_priority",
    "co_width", "co_height", "co_x", "co_y", "co_cx", "co_cy",
    "co_alpha", "co_bg", "co_fg",
    "fo_name", "fo_size", "fo_bold", "fo_apply_en", "fo_choose",
    "hk_hint", "hk_current", "hk_vk",
    "ex_internal", "ex_aliases", "ex_www", "ex_email", "ex_files",
    "ex_shell", "ex_plugins",
    "et_mode", "et_off", "et_easy", "et_linux", "et_int", "et_al",
    "et_sys", "et_hist",
    "hi_max", "hi_undo", "hi_int", "hi_al", "hi_www", "hi_mail",
    "hi_path", "hi_plg", "hi_other", "hi_clear",
    "lo_enable", "lo_file", "lo_maxkb", "lo_overflow", "lo_keep",
    "al_name", "al_action", "al_dir", "al_window", "al_add",
    "al_update", "al_delete", "al_hint",
    "so_play", "so_internal", "so_alias", "so_sys", "so_url",
    "so_mail", "so_file", "so_folder", "so_plugin", "so_shell",
    "pl_load", "pl_hist", "pl_rescan", "pl_unload", "pl_about",
    "pl_options", "pl_hint",
    "en_pathhint", "en_varshint", "en_addvar", "en_updvar", "en_delvar",
    "en_note",
    "la_label", "about_text", "help_head",
    "en_value", "la_auto",
    "al_wn_normal", "al_wn_max", "al_wn_min", "al_wn_hidden",
    "la_hint",
    "pg_theme", "th_scheme", "th_hint",
    "an_path", "an_unknown", "an_off",
    "dd_show", "dd_rows", "dd_empty", "ft_fuzzy", "cue", "an_web",
    "ctx_undo", "ctx_cut", "ctx_copy", "ctx_paste", "ctx_selall",
    "ctx_settings", "ctx_aliases", "ctx_plugins", "ctx_theme", "ctx_lang",
    "ctx_reload", "ctx_inidir", "ctx_about", "ctx_help", "ctx_hide", "ctx_exit",
    "th_bg", "th_text", "th_accent", "th_border", "th_borderw",
    "th_name", "th_saveas", "th_del", "th_portnote"
};

struct LangInfo { const wchar_t* code; const wchar_t* native; WORD langid; bool rtl; };
// order == pack order (kPacks) == enum LangIdx below
enum LangIdx { LANG_EN = 0, LANG_RU, LANG_UK, LANG_DE, LANG_FR, LANG_ES, LANG_PT,
               LANG_IT, LANG_ZH, LANG_JA, LANG_KO, LANG_AR, LANG_TR, LANG_PL,
               LANG_VI, LANG_HI, LANG_ID, LANG_SW, LANG_AF, LANG_AM, LANG_COUNT };
static const LangInfo kLangs[LANG_COUNT] = {
    { L"en", L"English",        0x09, false },
    { L"ru", L"Русский",        0x19, false },
    { L"uk", L"Українська",     0x22, false },
    { L"de", L"Deutsch",        0x07, false },
    { L"fr", L"Français",       0x0C, false },
    { L"es", L"Español",        0x0A, false },
    { L"pt", L"Português",      0x16, false },
    { L"it", L"Italiano",       0x10, false },
    { L"zh", L"中文",           0x04, false },   // zh-Hans
    { L"ja", L"日本語",         0x11, false },
    { L"ko", L"한국어",         0x12, false },
    { L"ar", L"العربية",        0x01, true  },
    { L"tr", L"Türkçe",         0x1F, false },
    { L"pl", L"Polski",         0x15, false },
    { L"vi", L"Tiếng Việt",     0x2A, false },
    { L"hi", L"हिन्दी",   0x39, false },
    { L"id", L"Bahasa Indonesia", 0x21, false },
    { L"sw", L"Kiswahili",      0x41, false },
    { L"af", L"Afrikaans",      0x36, false },
    { L"am", L"አማርኛ",  0x45, false },
};

struct Pack { const wchar_t* items[STR_LAST]; };

// ---------------------------------------------------------------- English (source)
static const Pack kEn = {
L"OK", L"Apply", L"Cancel",
L"Show / Hide", L"Settings...", L"Config (TypeBox.ini)", L"Exit",
L"Press the hotkey to open. Right-click the icon for the menu.",
L"TypeBox Settings",
L"Appearance", L"Console", L"Font", L"Hotkey", L"Execution",
L"EasyType", L"History", L"Log", L"Aliases", L"Sounds",
L"Plugins", L"Environment", L"Language",
L"Show console on start", L"Hide console when focus is lost", L"Tray icon",
L"Balloon tips", L"Auto-hide after idle", L"Idle seconds", L"Process priority",
L"Width (px)", L"Height (px)", L"X (-1 = auto)", L"Y (-1 = auto)",
L"Center horizontally", L"Center vertically", L"Transparency 0..255",
L"BG color (COLORREF)", L"FG color (COLORREF)",
L"Font name", L"Size", L"Bold", L"Apply English layout on open", L"Choose...",
L"Click the box and press any key or combination - single keys work too:", L"Current hotkey: ",
L"VK code (advanced):",
L"Internal commands (/...)", L"Aliases and ~system aliases", L"Websites (www. / http)",
L"E-mail (mailto)", L"Files and folders", L"Shell commands (cmd /c)", L"Plugins",
L"Completion mode", L"off", L"easy", L"linux (Tab cycles)",
L"Complete internal commands", L"Complete aliases",
L"Complete system aliases (~)", L"Complete from history",
L"Max history items", L"Ctrl+Z undo", L"Save internal commands", L"Save aliases",
L"Save websites", L"Save e-mails", L"Save paths", L"Save plugin commands",
L"Save other commands", L"Clear history",
L"Enable error log", L"File", L"Max size KB", L"On overflow", L"Keep rotated copies",
L"Name", L"Action", L"Work dir", L"Window", L"Add", L"Update", L"Delete",
L"Action supports %s and %1..%n parameters.",
L"Play sounds on execution", L"Internal commands", L"Aliases", L"System aliases",
L"Websites", L"E-mail", L"Files", L"Folders", L"Plugins", L"Shell commands",
L"Load on start", L"Save to history", L"Rescan", L"Unload all", L"About",
L"Options", L"Dlls live in the portable Plugins\\ folder.",
L"Own PATH folders (one per line, %VAR% allowed):",
L"Variables (typed %NAME% expands to the value):",
L"Add var", L"Update var", L"Delete var",
L"Folders extend PATH for aliases, shell lines and every started program.",
L"UI language (Apply takes effect immediately):",
L"Universal hotkey console + bash + launcher for Windows.\n"
L"Portable INI config, aliases with %s/%1..%n, plugins.\n\n"
L"Roots: continues TypeAndRun by Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
L"Inspired by PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu and drop-down terminals (Guake/Yakuake/Drop).",
L"Internal commands:",
L"Value", L"(Auto)",
L"Normal", L"Maximized", L"Minimized", L"Hidden",
L"lang\\<code>.ini adds portable overrides (key=value, UTF-8).",
L"Theme", L"Color scheme",
L"Warm presets for the console and tray icon; explicit [Color] values win.",
L"from PATH", L"unknown command", L"disabled by [Exec]",
L"Show suggestions drop-down", L"Visible rows", L"History when the line is empty",
L"Fuzzy matching",
L"Type a command — Tab completes · Enter runs · Esc hides", L"web search" };

// ---------------------------------------------------------------- Russian (full)
static const Pack kRu = {
L"OK", L"Применить", L"Отмена",
L"Показать / Скрыть", L"Настройки...", L"Конфиг (TypeBox.ini)", L"Выход",
L"Нажмите горячую клавишу, чтобы открыть. Правый клик по значку — меню.",
L"TypeBox — Настройки",
L"Внешний вид", L"Консоль", L"Шрифт", L"Горячая клавиша", L"Выполнение",
L"EasyType", L"История", L"Журнал", L"Алиасы", L"Звуки",
L"Плагины", L"Окружение", L"Язык",
L"Показывать консоль при запуске", L"Скрывать консоль при потере фокуса",
L"Значок в трее", L"Всплывающие подсказки", L"Скрывать после простоя",
L"Секунды простоя", L"Приоритет процесса",
L"Ширина (px)", L"Высота (px)", L"X (-1 = авто)", L"Y (-1 = авто)",
L"Центрировать по горизонтали", L"Центрировать по вертикали",
L"Прозрачность 0..255", L"Цвет фона (COLORREF)", L"Цвет текста (COLORREF)",
L"Имя шрифта", L"Размер", L"Полужирный", L"Включать английскую раскладку при открытии",
L"Выбрать...",
L"Щёлкните в поле и нажмите любую клавишу или комбинацию — одиночные тоже работают:", L"Текущая клавиша: ",
L"Код VK (для опытных):",
L"Внутренние команды (/...)", L"Алиасы и ~системные алиасы", L"Сайты (www. / http)",
L"E-mail (mailto)", L"Файлы и папки", L"Команды оболочки (cmd /c)", L"Плагины",
L"Режим дополнения", L"выкл", L"простой", L"linux (Tab по кругу)",
L"Дополнять внутренние команды", L"Дополнять алиасы",
L"Дополнять системные алиасы (~)", L"Дополнять из истории",
L"Максимум записей истории", L"Отмена Ctrl+Z", L"Сохранять внутренние команды",
L"Сохранять алиасы", L"Сохранять сайты", L"Сохранять e-mail", L"Сохранять пути",
L"Сохранять команды плагинов", L"Сохранять прочие команды", L"Очистить историю",
L"Включить журнал ошибок", L"Файл", L"Макс. размер КБ", L"При переполнении",
L"Хранить повёрнутых копий",
L"Имя", L"Действие", L"Рабочая папка", L"Окно", L"Добавить", L"Обновить", L"Удалить",
L"Действие поддерживает %s и %1..%n.",
L"Проигрывать звуки при выполнении", L"Внутренние команды", L"Алиасы",
L"Системные алиасы", L"Сайты", L"E-mail", L"Файлы", L"Папки", L"Плагины",
L"Команды оболочки",
L"Загружать при старте", L"Сохранять в историю", L"Пересканировать",
L"Выгрузить все", L"О плагине", L"Параметры",
L"DLL лежат в портативной папке Plugins\\.",
L"Свои папки PATH (по одной в строке, %VAR% можно):",
L"Переменные (набранный %NAME% раскрывается в значение):",
L"Добавить пер.", L"Обновить пер.", L"Удалить пер.",
L"Папки расширяют PATH для алиасов, shell-строк и всех запускаемых программ.",
L"Язык интерфейса (Применить действует сразу):",
L"Универсальная хоткей-консоль + bash + запускальник для Windows.\n"
L"Портативный INI-конфиг, алиасы с %s/%1..%n, плагины.\n\n"
L"Корни: продолжает TypeAndRun Евгения Галанцева\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
L"Вдохновлён PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu и выпадающими терминалами (Guake/Yakuake/Drop).",
L"Внутренние команды:",
L"Значение", L"(Авто)", L"Обычное", L"Развёрнутое", L"Свёрнутое", L"Скрытое",
L"lang\\<code>.ini добавляет портативные переводы (key=value, UTF-8).",
L"Тема", L"Цветовая схема",
L"Тёплые пресеты консоли и трей-иконки; явные значения [Color] важнее.",
L"из PATH", L"неизвестная команда", L"запрещено в [Exec]",
L"Показывать список подсказок", L"Видимых строк", L"История при пустой строке",
L"Нечёткий поиск",
L"Введите команду — Tab дополняет · Enter запускает · Esc скрывает", L"веб-поиск" };

// ---------------------------------------------------------------- Ukrainian (full)
static const Pack kUk = {
L"OK", L"Застосувати", L"Скасувати",
L"Показати / Приховати", L"Налаштування...", L"Конфіг (TypeBox.ini)", L"Вийти",
L"Натисніть гарячу клавішу, щоб відкрити. Правий клік по значку — меню.",
L"TypeBox — Налаштування",
L"Вигляд", L"Консоль", L"Шрифт", L"Гаряча клавіша", L"Виконання",
L"EasyType", L"Історія", L"Журнал", L"Аліаси", L"Звуки",
L"Плагіни", L"Середовище", L"Мова",
L"Показувати консоль на старті", L"Ховати консоль при втраті фокусу",
L"Значок у треї", L"Спливаючі підказки", L"Ховати після простою",
L"Секунд простою", L"Пріоритет процесу",
L"Ширина (px)", L"Висота (px)", L"X (-1 = авто)", L"Y (-1 = авто)",
L"Центрувати по горизонталі", L"Центрувати по вертикалі",
L"Прозорість 0..255", L"Колір фону (COLORREF)", L"Колір тексту (COLORREF)",
L"Назва шрифту", L"Розмір", L"Напівжирний", L"Вмирати англійську розкладку при відкритті",
L"Вибрати...",
L"Клацніть у поле та натисніть будь-яку клавішу або комбінацію — одиночні також працюють:", L"Поточна клавіша: ",
L"Код VK (для досвідчених):",
L"Внутрішні команди (/...)", L"Аліаси та ~системні аліаси", L"Сайти (www. / http)",
L"E-mail (mailto)", L"Файли та папки", L"Команди оболонки (cmd /c)", L"Плагіни",
L"Режим доповнення", L"вимк", L"простий", L"linux (Tab по колу)",
L"Доповнювати внутрішні команди", L"Доповнювати аліаси",
L"Доповнювати системні аліаси (~)", L"Доповнювати з історії",
L"Максимум записів історії", L"Скасування Ctrl+Z", L"Зберігати внутрішні команди",
L"Зберігати аліаси", L"Зберігати сайти", L"Зберігати e-mail", L"Зберігати шляхи",
L"Зберігати команди плагінів", L"Зберігати інші команди", L"Очистити історію",
L"Увімкнути журнал помилок", L"Файл", L"Макс. розмір КБ", L"При переповненні",
L"Зберігати обернутих копій",
L"Ім'я", L"Дія", L"Робоча папка", L"Вікно", L"Додати", L"Оновити", L"Видалити",
L"Дія підтримує %s та %1..%n.",
L"Відтворювати звуки при виконанні", L"Внутрішні команди", L"Аліаси",
L"Системні аліаси", L"Сайти", L"E-mail", L"Файли", L"Папки", L"Плагіни",
L"Команди оболонки",
L"Завантажувати на старті", L"Зберігати в історію", L"Сканувати знову",
L"Вивантажити всі", L"Про плагін", L"Параметри",
L"DLL лежать у портативній папці Plugins\\.",
L"Власні папки PATH (по одній у рядку, %VAR% можна):",
L"Змінні (набраний %NAME% розкривається у значення):",
L"Додати зм.", L"Оновити зм.", L"Видалити зм.",
L"Папки розширюють PATH для аліасів, shell-рядків і всіх запусканих програм.",
L"Мова інтерфейсу (Застосувати діє одразу):",
L"Універсальна хоткей-консоль + bash + запускач для Windows.\n"
L"Портативний INI-конфіг, аліаси з %s/%1..%n, плагіни.\n\n"
L"Корені: продовжує TypeAndRun Євгена Галанцева\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
L"Натхненний PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu та випадаючими терміналами (Guake/Yakuake/Drop).",
L"Внутрішні команди:",
L"Значення", L"(Авто)", L"Звичайне", L"Розгорнуте", L"Згорнуте", L"Приховане",
L"lang\\<code>.ini додає портативні переклади (key=value, UTF-8)." };

// ---------------------------------------------------------------- German (full)
static const Pack kDe = {
L"OK", L"Übernehmen", L"Abbrechen",
L"Anzeigen / Ausblenden", L"Einstellungen...", L"Konfig (TypeBox.ini)", L"Beenden",
L"Hotkey zum Öffnen drücken. Rechtsklick auf das Symbol öffnet das Menü.",
L"TypeBox — Einstellungen",
L"Darstellung", L"Konsole", L"Schrift", L"Hotkey", L"Ausführung",
L"EasyType", L"Verlauf", L"Protokoll", L"Aliasse", L"Geräusche",
L"Plugins", L"Umgebung", L"Sprache",
L"Konsole beim Start anzeigen", L"Konsole bei Fokusverlust ausblenden",
L"Symbol im Infobereich", L"Pop-up-Hinweise", L"Nach Inaktivität ausblenden",
L"Sekunden Inaktivität", L"Prozesspriorität",
L"Breite (px)", L"Höhe (px)", L"X (-1 = auto)", L"Y (-1 = auto)",
L"Horizontal zentrieren", L"Vertikal zentrieren",
L"Transparenz 0..255", L"Hintergrundfarbe (COLORREF)", L"Textfarbe (COLORREF)",
L"Schriftname", L"Größe", L"Fett", L"Beim Öffnen englisches Layout aktivieren",
L"Auswählen...",
L"Klicken Sie ins Feld und drücken Sie die Kombination (Esc = Abbruch):",
L"Aktueller Hotkey: ", L"VK-Code (erweitert):",
L"Interne Befehle (/...)", L"Aliasse und ~Systemaliasse", L"Websites (www. / http)",
L"E-Mail (mailto)", L"Dateien und Ordner", L"Shell-Befehle (cmd /c)", L"Plugins",
L"Vervollständigungsmodus", L"aus", L"einfach", L"linux (Tab wechselt)",
L"Interne Befehle vervollständigen", L"Aliasse vervollständigen",
L"Systemaliasse (~) vervollständigen", L"Aus Verlauf vervollständigen",
L"Max. Verlaufseinträge", L"Ctrl+Z Rückgängig", L"Interne Befehle speichern",
L"Aliasse speichern", L"Websites speichern", L"E-Mails speichern", L"Pfade speichern",
L"Plugin-Befehle speichern", L"Sonstige Befehle speichern", L"Verlauf löschen",
L"Fehlerprotokoll aktivieren", L"Datei", L"Max. Größe KB", L"Bei Überlauf",
L"Behaltene Drehkopien",
L"Name", L"Aktion", L"Arbeitsordner", L"Fenster", L"Hinzufügen", L"Aktualisieren",
L"Löschen", L"Aktion unterstützt %s und %1..%n.",
L"Geräusche bei Ausführung abspielen", L"Interne Befehle", L"Aliasse",
L"Systemaliasse", L"Websites", L"E-Mail", L"Dateien", L"Ordner", L"Plugins",
L"Shell-Befehle",
L"Beim Start laden", L"In Verlauf speichern", L"Neu einlesen", L"Alle entladen",
L"Über", L"Optionen", L"DLLs liegen im portablen Ordner Plugins\\.",
L"Eigene PATH-Ordner (einer pro Zeile, %VAR% erlaubt):",
L"Variablen (getipptes %NAME% wird zum Wert):",
L"Variable hinzufügen", L"Variable aktualisieren", L"Variable löschen",
L"Ordner erweitern PATH für Aliasse, Shell-Zeilen und alle gestarteten Programme.",
L"Oberflächensprache (Übernehmen wirkt sofort):",
L"Universelle Hotkey-Konsole + Bash + Launcher für Windows.\n"
L"Portable INI-Konfiguration, Aliasse mit %s/%1..%n, Plugins.\n\n"
L"Wurzeln: führt TypeAndRun von Evgeniy Galantsev fort\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
L"Inspiriert von PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu und Drop-down-Terminals (Guake/Yakuake/Drop).",
L"Interne Befehle:" };

// ---------------------------------------------------------------- French (full)
static const Pack kFr = {
L"OK", L"Appliquer", L"Annuler",
L"Afficher / Masquer", L"Paramètres...", L"Config (TypeBox.ini)", L"Quitter",
L"Appuyez sur la touche rapide pour ouvrir. Clic droit sur l'icône : menu.",
L"TypeBox — Paramètres",
L"Apparence", L"Console", L"Police", L"Raccourci", L"Exécution",
L"EasyType", L"Historique", L"Journal", L"Alias", L"Sons",
L"Plugins", L"Environnement", L"Langue",
L"Afficher la console au démarrage", L"Masquer la console si le focus est perdu",
L"Icône dans la zone de notification", L"Infobulles", L"Masquer après inactivité",
L"Secondes d'inactivité", L"Priorité du processus",
L"Largeur (px)", L"Hauteur (px)", L"X (-1 = auto)", L"Y (-1 = auto)",
L"Centrer horizontalement", L"Centrer verticalement",
L"Transparence 0..255", L"Couleur fond (COLORREF)", L"Couleur texte (COLORREF)",
L"Nom de la police", L"Taille", L"Gras", L"Activer le clavier anglais à l'ouverture",
L"Choisir...",
L"Cliquez dans la zone puis appuyez sur la combinaison (Échap : annuler) :",
L"Raccourci actuel : ", L"Code VK (avancé) :",
L"Commandes internes (/...)", L"Alias et alias système (~)", L"Sites web (www. / http)",
L"E-mail (mailto)", L"Fichiers et dossiers", L"Commandes shell (cmd /c)", L"Plugins",
L"Mode de complétion", L"off", L"simple", L"linux (Tab cyclique)",
L"Compléter les commandes internes", L"Compléter les alias",
L"Compléter les alias système (~)", L"Compléter depuis l'historique",
L"Entrées max d'historique", L"Annuler avec Ctrl+Z", L"Enregistrer les commandes internes",
L"Enregistrer les alias", L"Enregistrer les sites", L"Enregistrer les e-mails",
L"Enregistrer les chemins", L"Enregistrer les commandes de plugins",
L"Enregistrer les autres commandes", L"Effacer l'historique",
L"Activer le journal d'erreurs", L"Fichier", L"Taille max Ko", L"En cas de saturation",
L"Copies pivotées conservées",
L"Nom", L"Action", L"Dossier de travail", L"Fenêtre", L"Ajouter", L"Mettre à jour",
L"Supprimer", L"L'action accepte %s et %1..%n.",
L"Jouer des sons à l'exécution", L"Commandes internes", L"Alias", L"Alias système",
L"Sites web", L"E-mail", L"Fichiers", L"Dossiers", L"Plugins", L"Commandes shell",
L"Charger au démarrage", L"Sauvegarder dans l'historique", L"Rescanner",
L"Décharger tout", L"À propos", L"Options",
L"Les DLLs sont dans le dossier portable Plugins\\.",
L"Dossiers PATH perso (un par ligne, %VAR% autorisé) :",
L"Variables (%NAME% tapé se développe en valeur) :",
L"Ajouter var", L"MaJ var", L"Suppr var",
L"Les dossiers étendent le PATH pour les alias, lignes shell et tous les programmes.",
L"Langue de l'interface (Appliquer agit aussitôt) :",
L"Console + bash + lanceur universel à raccourci pour Windows.\n"
L"Config INI portable, alias avec %s/%1..%n, plugins.\n\n"
L"Racines : succède à TypeAndRun d'Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
L"Inspiré de PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu et des terminaux déroulants (Guake/Yakuake/Drop).",
L"Commandes internes :" };

// ---------------------------------------------------------------- Spanish (full)
static const Pack kEs = {
L"OK", L"Aplicar", L"Cancelar",
L"Mostrar / Ocultar", L"Opciones...", L"Config (TypeBox.ini)", L"Salir",
L"Pulsa la tecla rápida para abrir. Clic derecho en el icono: menú.",
L"TypeBox — Opciones",
L"Apariencia", L"Consola", L"Fuente", L"Tecla rápida", L"Ejecución",
L"EasyType", L"Historial", L"Registro", L"Alias", L"Sonidos",
L"Complementos", L"Entorno", L"Idioma",
L"Mostrar consola al iniciar", L"Ocultar consola al perder el foco",
L"Icono en la bandeja", L"Conseños emergentes", L"Ocultar tras inactividad",
L"Segundos de inactividad", L"Prioridad del proceso",
L"Ancho (px)", L"Alto (px)", L"X (-1 = auto)", L"Y (-1 = auto)",
L"Centrar horizontalmente", L"Centrar verticalmente",
L"Transparencia 0..255", L"Color fondo (COLORREF)", L"Color texto (COLORREF)",
L"Nombre de fuente", L"Tamaño", L"Negrita", L"Activar teclado inglés al abrir",
L"Elegir...",
L"Haz clic en el campo y pulsa la combinación (Esc cancela):",
L"Tecla actual: ", L"Código VK (avanzado):",
L"Comandos internos (/...)", L"Alias y alias de sistema (~)", L"Sitios web (www. / http)",
L"Correo (mailto)", L"Archivos y carpetas", L"Comandos de shell (cmd /c)",
L"Complementos",
L"Modo de autocompletado", L"apagado", L"fácil", L"linux (Tab cicla)",
L"Completar comandos internos", L"Completar alias",
L"Completar alias de sistema (~)", L"Completar desde el historial",
L"Máx. elementos del historial", L"Deshacer con Ctrl+Z", L"Guardar comandos internos",
L"Guardar alias", L"Guardar sitios", L"Guardar correos", L"Guardar rutas",
L"Guardar comandos de complementos", L"Guardar otros comandos", L"Vaciar historial",
L"Activar registro de errores", L"Archivo", L"Tamaño máx KB", L"Al llenarse",
L"Copias rotatorias guardadas",
L"Nombre", L"Acción", L"Carpeta de trabajo", L"Ventana", L"Añadir", L"Actualizar",
L"Eliminar", L"La acción admite %s y %1..%n.",
L"Reproducir sonidos al ejecutar", L"Comandos internos", L"Alias", L"Alias de sistema",
L"Sitios web", L"Correo", L"Archivos", L"Carpetas", L"Complementos", L"Comandos de shell",
L"Cargar al iniciar", L"Guardar en historial", L"Reescanear", L"Descargar todo",
L"Acerca de", L"Opciones", L"Los DLL están en la carpeta portable Plugins\\.",
L"Carpetas PATH propias (una por línea, %VAR% permitido):",
L"Variables (%NAME% escrito se expande al valor):",
L"Añadir var", L"Actualizar var", L"Eliminar var",
L"Las carpetas amplían el PATH para alias, líneas shell y todo programa iniciado.",
L"Idioma de interfaz (Aplicar actúa al instante):",
L"Consola universal con tecla rápida + bash + lanzador para Windows.\n"
L"Config INI portable, alias con %s/%1..%n, complementos.\n\n"
L"Raíces: continúa TypeAndRun de Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
L"Inspirado en PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu y terminales desplegables (Guake/Yakuake/Drop).",
L"Comandos internos:" };

// ---------------------------------------------------------------- Portuguese (full)
static const Pack kPt = {
L"OK", L"Aplicar", L"Cancelar",
L"Mostrar / Ocultar", L"Configurações...", L"Config (TypeBox.ini)", L"Sair",
L"Pressione a tecla de atalho para abrir. Botão direito no ícone: menu.",
L"TypeBox — Configurações",
L"Aparência", L"Console", L"Fonte", L"Atalho", L"Execução",
L"EasyType", L"Histórico", L"Registro", L"Aliases", L"Sons",
L"Plugins", L"Ambiente", L"Idioma",
L"Mostrar console ao iniciar", L"Ocultar console ao perder o foco",
L"Ícone na bandeja", L"Dicas de ferramenta", L"Ocultar após inatividade",
L"Segundos de inatividade", L"Prioridade do processo",
L"Largura (px)", L"Altura (px)", L"X (-1 = auto)", L"Y (-1 = auto)",
L"Centralizar horizontalmente", L"Centralizar verticalmente",
L"Transparência 0..255", L"Cor de fundo (COLORREF)", L"Cor do texto (COLORREF)",
L"Nome da fonte", L"Tamanho", L"Negrito", L"Ativar teclado inglês ao abrir",
L"Escolher...",
L"Clique no campo e pressione a combinação (Esc cancela):",
L"Atalho atual: ", L"Código VK (avançado):",
L"Comandos internos (/...)", L"Aliases e aliases de sistema (~)", L"Sites (www. / http)",
L"E-mail (mailto)", L"Arquivos e pastas", L"Comandos de shell (cmd /c)", L"Plugins",
L"Modo de autocompletar", L"desligado", L"fácil", L"linux (Tab cicla)",
L"Completar comandos internos", L"Completar aliases",
L"Completar aliases de sistema (~)", L"Completar do histórico",
L"Máx. itens do histórico", L"Desfazer com Ctrl+Z", L"Salvar comandos internos",
L"Salvar aliases", L"Salvar sites", L"Salvar e-mails", L"Salvar caminhos",
L"Salvar comandos de plugins", L"Salvar outros comandos", L"Limpar histórico",
L"Ativar registro de erros", L"Arquivo", L"Tamanho máx KB", L"Ao encher",
L"Cópias rotativas mantidas",
L"Nome", L"Ação", L"Pasta de trabalho", L"Janela", L"Adicionar", L"Atualizar",
L"Excluir", L"A ação aceita %s e %1..%n.",
L"Tocar sons ao executar", L"Comandos internos", L"Aliases", L"Aliases de sistema",
L"Sites", L"E-mail", L"Arquivos", L"Pastas", L"Plugins", L"Comandos de shell",
L"Carregar ao iniciar", L"Salvar no histórico", L"Reescanear", L"Descarregar tudo",
L"Sobre", L"Opções", L"Os DLLs ficam na pasta portable Plugins\\.",
L"Pastas PATH próprias (uma por linha, %VAR% permitido):",
L"Variáveis (%NAME% digitado expande para o valor):",
L"Adicionar var", L"Atualizar var", L"Excluir var",
L"As pastas ampliam o PATH para aliases, linhas shell e todo programa iniciado.",
L"Idioma da interface (Aplicar age na hora):",
L"Console universal com atalho + bash + lançador para Windows.\n"
L"Config INI portable, aliases com %s/%1..%n, plugins.\n\n"
L"Raízes: continua o TypeAndRun de Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
L"Inspirado em PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu e terminais suspensos (Guake/Yakuake/Drop).",
L"Comandos internos:" };

// ---------------------------------------------------------------- Italian (full)
static const Pack kIt = {
L"OK", L"Applica", L"Annulla",
L"Mostra / Nascondi", L"Impostazioni...", L"Config (TypeBox.ini)", L"Esci",
L"Premi il tasto di scelta rapida per aprire. Clic destro sull'icona: menu.",
L"TypeBox — Impostazioni",
L"Aspetto", L"Console", L"Carattere", L"Scorciatoia", L"Esecuzione",
L"EasyType", L"Cronologia", L"Registro", L"Alias", L"Suoni",
L"Plugin", L"Ambiente", L"Lingua",
L"Mostra la console all'avvio", L"Nascondi la console se perde il focus",
L"Icona nella barra", L"Suggerimenti a comparsa", L"Nascondi dopo inattività",
L"Secondi di inattività", L"Priorità di processo",
L"Larghezza (px)", L"Altezza (px)", L"X (-1 = auto)", L"Y (-1 = auto)",
L"Centra orizzontalmente", L"Centra verticalmente",
L"Trasparenza 0..255", L"Colore sfondo (COLORREF)", L"Colore testo (COLORREF)",
L"Nome carattere", L"Dimensione", L"Grassetto", L"Attiva tastiera inglese all'apertura",
L"Scegli...",
L"Clicca nel campo e premi la combinazione (Esc annulla):",
L"Scorciatoia attuale: ", L"Codice VK (avanzato):",
L"Comandi interni (/...)", L"Alias e alias di sistema (~)", L"Siti web (www. / http)",
L"E-mail (mailto)", L"File e cartelle", L"Comandi shell (cmd /c)", L"Plugin",
L"Modalità completamento", L"off", L"semplice", L"linux (Tab cicla)",
L"Completa comandi interni", L"Completa alias",
L"Completa alias di sistema (~)", L"Completa dalla cronologia",
L"Max voci di cronologia", L"Annulla con Ctrl+Z", L"Salva comandi interni",
L"Salva alias", L"Salva siti", L"Salva e-mail", L"Salva percorsi",
L"Salva comandi dei plugin", L"Salva altri comandi", L"Svuota cronologia",
L"Abilita registro errori", L"File", L"Dimensione max KB", L"A saturazione",
L"Copie rotate conservate",
L"Nome", L"Azione", L"Cartella di lavoro", L"Finestra", L"Aggiungi", L"Aggiorna",
L"Elimina", L"L'azione supporta %s e %1..%n.",
L"Riproduci suoni all'esecuzione", L"Comandi interni", L"Alias", L"Alias di sistema",
L"Siti web", L"E-mail", L"File", L"Cartelle", L"Plugin", L"Comandi shell",
L"Carica all'avvio", L"Salva nella cronologia", L"Riscansiona", L"Scarica tutti",
L"Informazioni", L"Opzioni", L"I DLL sono nella cartella portable Plugins\\.",
L"Cartelle PATH proprie (una per riga, %VAR% consentito):",
L"Variabili (%NAME% digitato si espande nel valore):",
L"Aggiungi var", L"Aggiorna var", L"Elimina var",
L"Le cartelle estendono il PATH per alias, righe shell e ogni programma avviato.",
L"Lingua dell'interfaccia (Applica agisce subito):",
L"Console universale con scorciatoia + bash + launcher per Windows.\n"
L"Config INI portable, alias con %s/%1..%n, plugin.\n\n"
L"Radici: continua TypeAndRun di Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).\n"
"L'ispirazione viene da PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
L"ConEmu e terminali a discesa (Guake/Yakuake/Drop).",
L"Comandi interni:" };

// ---------------------------------------------------------------- core packs
// Non-European packs translate the core UI set (indices 0..28, 46..52, 76..82,
// 94..97, 107..108); empty strings fall back to English.
static const wchar_t* const C = L"";   // shortcut: fall back to English

static const Pack kZh = {
L"确定", L"应用", L"取消",
L"显示 / 隐藏", L"设置...", L"配置 (TypeBox.ini)", L"退出",
L"按热键打开。右键点击图标打开菜单。",
L"TypeBox 设置",
L"外观", L"控制台", L"字体", L"热键", L"执行",
L"EasyType", L"历史", L"日志", L"别名", L"声音",
L"插件", L"环境", L"语言",
L"启动时显示控制台", L"失去焦点时隐藏控制台", L"托盘图标", L"气泡提示",
L"空闲后自动隐藏", L"空闲秒数", L"进程优先级",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"内部命令 (/...)", L"别名和 ~系统别名", L"网站 (www. / http)",
L"电子邮件 (mailto)", L"文件和文件夹", L"Shell 命令 (cmd /c)", L"插件",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"清除历史",
C, C, C, C, C,
L"名称", L"操作", L"工作目录", L"窗口", L"添加", L"更新", L"删除", C,
C, C, C, C, C, C, C, C, C, C,
L"启动时加载", L"保存到历史", L"重新扫描", L"全部卸载", L"关于", L"选项", C,
C, C, C, C, C, C,
L"界面语言（立即生效）：",
L"Windows 通用热键控制台 + bash + 启动器。\n"
L"便携 INI 配置、带 %s/%1..%n 的别名、插件。\n\n"
L"根源：延续 Evgeniy Galantsev 的 TypeAndRun\n"
L"(github.com/galancev/typeandrun, GPL-3.0)。",
L"内部命令:" };

static const Pack kJa = {
L"OK", L"適用", L"キャンセル",
L"表示 / 非表示", L"設定...", L"設定ファイル (TypeBox.ini)", L"終了",
L"ホットキーで開きます。アイコンの右クリックでメニュー。",
L"TypeBox 設定",
L"外観", L"コンソール", L"フォント", L"ホットキー", L"実行",
L"EasyType", L"履歴", L"ログ", L"エイリアス", L"サウンド",
L"プラグイン", L"環境", L"言語",
L"起動時にコンソールを表示", L"フォーカスが外れたら非表示", L"トレイアイコン",
L"バルーンヒント", L"アイドル後に自動で非表示", L"アイドル秒数", L"プロセス優先度",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"内部コマンド (/...)", L"エイリアスと ~システムエイリアス", L"ウェブサイト (www. / http)",
L"メール (mailto)", L"ファイルとフォルダー", L"シェルコマンド (cmd /c)", L"プラグイン",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"履歴を消去",
C, C, C, C, C,
L"名前", L"アクション", L"作業フォルダー", L"ウィンドウ", L"追加", L"更新", L"削除", C,
C, C, C, C, C, C, C, C, C, C,
L"起動時に読み込む", L"履歴に保存", L"再スキャン", L"すべてアンロード", L"バージョン情報",
L"オプション", C,
C, C, C, C, C, C,
L"UI言語（適用で即反映）：",
L"Windows 用の汎用ホットキーコンソール + bash + ランチャー。\n"
L"ポータブル INI 設定、%s/%1..%n 付きエイリアス、プラグイン。\n\n"
L"起源：Evgeniy Galantsev の TypeAndRun を継承\n"
L"(github.com/galancev/typeandrun, GPL-3.0)。",
L"内部コマンド:" };

static const Pack kKo = {
L"확인", L"적용", L"취소",
L"보이기 / 숨기기", L"설정...", L"설정 파일 (TypeBox.ini)", L"끝내기",
L"단축키를 눌러 여세요. 아이콘 우클릭 시 메뉴가 열립니다.",
L"TypeBox 설정",
L"모양", L"콘솔", L"글꼴", L"단축키", L"실행",
L"EasyType", L"기록", L"로그", L"별칭", L"소리",
L"플러그인", L"환경", L"언어",
L"시작 시 콘솔 표시", L"포커스를 잃으면 숨기기", L"트레이 아이콘", L"풍선 도움말",
L"유휴 후 자동 숨기기", L"유휴 초", L"프로세스 우선순위",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"내부 명령 (/...)", L"별칭 및 ~시스템 별칭", L"웹사이트 (www. / http)",
L"이메일 (mailto)", L"파일 및 폴더", L"셸 명령 (cmd /c)", L"플러그인",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"기록 지우기",
C, C, C, C, C,
L"이름", L"동작", L"작업 폴더", L"창", L"추가", L"갱신", L"삭제", C,
C, C, C, C, C, C, C, C, C, C,
L"시작 시 로드", L"기록에 저장", L"다시 검색", L"모두 언로드", L"정보", L"옵션", C,
C, C, C, C, C, C,
L"UI 언어 (적용 즉시 반영):",
L"Windows용 범용 단축키 콘솔 + bash + 런처.\n"
L"포터블 INI 설정, %s/%1..%n 별칭, 플러그인.\n\n"
L"계보: Evgeniy Galantsev의 TypeAndRun을 계승\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"내부 명령:" };

static const Pack kAr = {
L"موافق", L"تطبيق", L"إلغاء",
L"إظهار / إخفاء", L"الإعدادات...", L"الملف الشخصي (TypeBox.ini)", L"خروج",
L"اضغط المفتاح السريع للفتح. انقر بزر الفأرة الأيمن على الرمز للقائمة.",
L"إعدادات TypeBox",
L"المظهر", L"الطرفية", L"الخط", L"مفتاح الاختصار", L"التنفيذ",
L"EasyType", L"السجل", L"السجل الخاطئ", L"الأسماء المستعارة", L"الأصوات",
L"الإضافات", L"البيئة", L"اللغة",
L"إظهار الطرفية عند البدء", L"إخفاء الطرفية عند فقدان التركيز", L"رمز في الدرج",
L"تلميحات منبثقة", L"إخفاء تلقائي بعد الخمول", L"ثواني الخمول", L"أولوية العملية",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"أوامر داخلية (/...)", L"أسماء مستعارة وأسماء النظام (~)", L"مواقع الويب (www. / http)",
L"البريد (mailto)", L"الملفات والمجلدات", L"أوامر الصدفة (cmd /c)", L"الإضافات",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"مسح السجل",
C, C, C, C, C,
L"الاسم", L"الإجراء", L"مجلد العمل", L"النافذة", L"إضافة", L"تحديث", L"حذف", C,
C, C, C, C, C, C, C, C, C, C,
L"تحميل عند البدء", L"حفظ في السجل", L"إعادة الفحص", L"إلغاء تحميل الكل", L"حول",
L"خيارات", C,
C, C, C, C, C, C,
L"لغة الواجهة (يطبق فورا):",
L"طرفية شاملة بمفتاح سريع + bash + مشغل لتطبيقات Windows.\n"
L"إعداد INI محمول، أسماء مستعارة مع %s/%1..%n، إضافات.\n\n"
L"الجذور: يواصل TypeAndRun من Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"أوامر داخلية:" };

static const Pack kTr = {
L"Tamam", L"Uygula", L"İptal",
L"Göster / Gizle", L"Ayarlar...", L"Yapılandırma (TypeBox.ini)", L"Çıkış",
L"Açmak için kısayol tuşuna basın. Simgeye sağ tık: menü.",
L"TypeBox Ayarları",
L"Görünüm", L"Konsol", L"Yazı Tipi", L"Kısayol", L"Çalıştırma",
L"EasyType", L"Geçmiş", L"Günlük", L"Takma Adlar", L"Sesler",
L"Eklentiler", L"Ortam", L"Dil",
L"Başlangıçta konsolu göster", L"Odak kaybolunca gizle", L"System tepsisi simgesi",
L"Balon ipuçları", L"Boşta kalınca gizle", L"Boşta saniye", L"İşlem önceliği",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"Dahili komutlar (/...)", L"Takma adlar ve ~sistem takma adları",
L"Web siteleri (www. / http)", L"E-posta (mailto)", L"Dosyalar ve klasörler",
L"Kabuk komutları (cmd /c)", L"Eklentiler",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"Geçmişi temizle",
C, C, C, C, C,
L"Ad", L"Eylem", L"Çalışma klasörü", L"Pencere", L"Ekle", L"Güncelle", L"Sil", C,
C, C, C, C, C, C, C, C, C, C,
L"Başlangıçta yükle", L"Geçmişe kaydet", L"Yeniden tara", L"Tümünü çıkar",
L"Hakkında", L"Seçenekler", C,
C, C, C, C, C, C,
L"Arayüz dili (Uygula anında etkilidir):",
L"Windows için evrensel kısayol konsolu + bash + başlatıcı.\n"
L"Taşınabilir INI yapılandırması, %s/%1..%n takma adlar, eklentiler.\n\n"
L"Kökler: Evgeniy Galantsev'in TypeAndRun projesini sürdürür\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"Dahili komutlar:" };

static const Pack kPl = {
L"OK", L"Zastosuj", L"Anuluj",
L"Pokaż / Ukryj", L"Ustawienia...", L"Konfiguracja (TypeBox.ini)", L"Zakończ",
L"Naciśnij klawisz skrótu, aby otworzyć. Kliknij prawym na ikonę: menu.",
L"TypeBox — Ustawienia",
L"Wygląd", L"Konsola", L"Czcionka", L"Skrót", L"Wykonanie",
L"EasyType", L"Historia", L"Dziennik", L"Aliasy", L"Dźwięki",
L"Wtyczki", L"Środowisko", L"Język",
L"Pokaż konsolę przy starcie", L"Ukryj konsolę po utracie fokusu", L"Ikona w zasobniku",
L"Dymki podpowiedzi", L"Ukryj po bezczynności", L"Sekundy bezczynności",
L"Priorytet procesu",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"Polecenia wewnętrzne (/...)", L"Aliasy i ~aliasy systemowe", L"Strony (www. / http)",
L"E-mail (mailto)", L"Pliki i foldery", L"Polecenia powłoki (cmd /c)", L"Wtyczki",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"Wyczyść historię",
C, C, C, C, C,
L"Nazwa", L"Akcja", L"Katalog roboczy", L"Okno", L"Dodaj", L"Zaktualizuj", L"Usuń", C,
C, C, C, C, C, C, C, C, C, C,
L"Wczytuj przy starcie", L"Zapisuj do historii", L"Przeskanuj ponownie",
L"Wyładuj wszystkie", L"O programie", L"Opcje", C,
C, C, C, C, C, C,
L"Język interfejsu (Zastosuj działa od razu):",
L"Uniwersalna konsola skrótów + bash + launcher dla Windows.\n"
L"Przenośna konfiguracja INI, aliasy z %s/%1..%n, wtyczki.\n\n"
L"Korzenie: kontynuuje TypeAndRun autorstwa Evgeniy Galantseva\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"Polecenia wewnętrzne:" };

static const Pack kVi = {
L"OK", L"Áp dụng", L"Hủy",
L"Hiện / Ẩn", L"Cài đặt...", L"Cấu hình (TypeBox.ini)", L"Thoát",
L"Nhấn phím nóng để mở. Chuột phải vào biểu tượng để mở menu.",
L"TypeBox — Cài đặt",
L"Giao diện", L"Console", L"Phông chữ", L"Phím nóng", L"Thực thi",
L"EasyType", L"Lịch sử", L"Nhật ký", L"Bí danh", L"Âm thanh",
L"Trình cắm thêm", L"Môi trường", L"Ngôn ngữ",
L"Hiện console khi khởi động", L"Ẩn console khi mất tiêu điểm", L"Biểu tượng khay",
L"Chú thích bong bóng", L"Tự ẩn sau khi nghỉ", L"Giây nghỉ", L"Độ ưu tiên tiến trình",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"Lệnh nội bộ (/...)", L"Bí danh và ~bí danh hệ thống", L"Website (www. / http)",
L"E-mail (mailto)", L"Tệp và thư mục", L"Lệnh shell (cmd /c)", L"Trình cắm thêm",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"Xóa lịch sử",
C, C, C, C, C,
L"Tên", L"Hành động", L"Thư mục làm việc", L"Cửa sổ", L"Thêm", L"Cập nhật", L"Xóa", C,
C, C, C, C, C, C, C, C, C, C,
L"Nạp khi khởi động", L"Lưu vào lịch sử", L"Quét lại", L"Bỏ nạp tất cả",
L"Giới thiệu", L"Tùy chọn", C,
C, C, C, C, C, C,
L"Ngôn ngữ giao diện (Áp dụng có ngay):",
L"Console phím nóng + bash + trình khởi chạy đa dụng cho Windows.\n"
L"Cấu hình INI xách tay, bí danh kèm %s/%1..%n, trình cắm thêm.\n\n"
L"Nguồn gốc: tiếp nối TypeAndRun của Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"Lệnh nội bộ:" };

static const Pack kHi = {
L"ठीक", L"लागू करें", L"रद्द करें",
L"दिखाएँ / छिपाएँ", L"सेटिंग्स...", L"कॉन्फ़िग (TypeBox.ini)", L"बाहर निकलें",
L"खोलने के लिए हॉटकी दबाएँ। आइकन पर राइट-क्लिक: मेनू।",
L"TypeBox सेटिंग्स",
L"रूप", L"कंसोल", L"फ़ॉन्ट", L"हॉटकी", L"कार्यान्वयन",
L"EasyType", L"इतिहास", L"लॉग", L"उपनाम", L"ध्वनियाँ",
L"प्लगइन", L"वातावरण", L"भाषा",
L"शुरू में कंसोल दिखाएँ", L"फ़ोकस खोने पर छिपाएँ", L"ट्रे आइकन",
L"बैलून सुझाव", L"निष्क्रियता के बाद स्वतः छिपाएँ", L"निष्क्रिय सेकंड",
L"प्रक्रिया प्राथमिकता",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"आंतरिक कमांड (/...)", L"उपनाम और ~सिस्टम उपनाम", L"वेबसाइट (www. / http)",
L"ईमेल (mailto)", L"फ़ाइलें और फ़ोल्डर", L"शेल कमांड (cmd /c)", L"प्लगइन",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"इतिहास साफ़ करें",
C, C, C, C, C,
L"नाम", L"क्रिया", L"कार्य फ़ोल्डर", L"विंडो", L"जोड़ें", L"अपडेट करें", L"हटाएँ", C,
C, C, C, C, C, C, C, C, C, C,
L"शुरू में लोड करें", L"इतिहास में सहेजें", L"फिर से स्कैन करें", L"सभी अनलोड करें",
L"परिचय", L"विकल्प", C,
C, C, C, C, C, C,
L"इंटरफ़ेस भाषा (लागू तुरंत असर करता है):",
L"Windows के लिए सार्वभौमिक हॉटकी कंसोल + bash + लॉन्चर।\n"
L"पोर्टेबल INI कॉन्फ़िग, %s/%1..%n वाले उपनाम, प्लगइन।\n\n"
L"मूल: Evgeniy Galantsev के TypeAndRun को आगे बढ़ाता है\n"
L"(github.com/galancev/typeandrun, GPL-3.0)।",
L"आंतरिक कमांड:" };

static const Pack kId = {
L"OK", L"Terapkan", L"Batal",
L"Tampilkan / Sembunyikan", L"Pengaturan...", L"Konfig (TypeBox.ini)", L"Keluar",
L"Tekan tombol pintas untuk membuka. Klik kanan ikon: menu.",
L"Pengaturan TypeBox",
L"Tampilan", L"Konsol", L"Fon", L"Tombol pintas", L"Eksekusi",
L"EasyType", L"Riwayat", L"Log", L"Alias", L"Suara",
L"Plugin", L"Lingkungan", L"Bahasa",
L"Tampilkan konsol saat mulai", L"Sembunyikan konsol saat fokus hilang",
L"Ikon baki", L"Tip balon", L"Sembunyikan otomatis setelah diam",
L"Detik menganggur", L"Prioritas proses",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"Perintah internal (/...)", L"Alias dan ~alias sistem", L"Situs web (www. / http)",
L"E-mail (mailto)", L"Berkas dan folder", L"Perintah shell (cmd /c)", L"Plugin",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"Bersihkan riwayat",
C, C, C, C, C,
L"Nama", L"Tindakan", L"Folder kerja", L"Jendela", L"Tambah", L"Perbarui", L"Hapus", C,
C, C, C, C, C, C, C, C, C, C,
L"Muat saat mulai", L"Simpan ke riwayat", L"Pindai ulang", L"Lepas semua",
L"Tentang", L"Opsi", C,
C, C, C, C, C, C,
L"Bahasa antarmuka (Terapkan langsung berlaku):",
L"Konsol tombol pintas universal + bash + peluncur untuk Windows.\n"
L"Konfigurasi INI portabel, alias dengan %s/%1..%n, plugin.\n\n"
L"Akar: meneruskan TypeAndRun karya Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"Perintah internal:" };

static const Pack kSw = {
L"Sawa", L"Tumkia", L"Ghairi",
L"Onyesha / Ficha", L"Mipangilio...", L"Weka (TypeBox.ini)", L"Funga",
L"Bonyeza kitufe cha mkato kufungua. Bofya kulia ikoni: menyu.",
L"Mipangilio ya TypeBox",
L"Muonekano", L"Konsoli", L"Fonti", L"Kitufe cha mkato", L"Utekelezaji",
L"EasyType", L"Historia", L"Kumbukumbu", L"Jina bandia", L"Sauti",
L"Plogini", L"Mazingira", L"Lugha",
L"Onyesha konsoli mwanzo", L"Ficha konsoli ukipoteza umakini", L"Ikoni ya kwenye tray",
L"Vidokezo vya balloon", L"Ficha baada ya utulivu", L"Sekunde za utulivu",
L"Kipaumbele cha mchakato",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"Amri za ndani (/...)", L"Majina bandia na ~majina ya mfumo",
L"Tovuti (www. / http)", L"Barua pepe (mailto)", L"Faili na folda",
L"Amri za shell (cmd /c)", L"Plogini",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"Futa historia",
C, C, C, C, C,
L"Jina", L"Kitendo", L"Folda ya kazi", L"Dirisha", L"Soma", L"Sasisha", L"Futa", C,
C, C, C, C, C, C, C, C, C, C,
L"Pakia mwanzoni", L"Hifadhi kwenye historia", L"Changanua upya", L"Ondoa zote",
L"Kuhusu", L"Chaguo", C,
C, C, C, C, C, C,
L"Lugha ya interface (Tumkia hufanya kazi mara)",
L"Konsoli ya kitufe cha mkato + bash + kizindua cha Windows.\n"
L"Weka ya INI ya kubebeka, majina bandia yenye %s/%1..%n, plogini.\n\n"
L"Mizizi: inaendelea TypeAndRun ya Evgeniy Galantsev\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"Amri za ndani:" };

static const Pack kAf = {
L"Goed", L"Toepas", L"Kanselleer",
L"Wys / Versteek", L"Instellings...", L"Konfig (TypeBox.ini)", L"Beëindig",
L"Druk die vortsleutel om te oopmaak. Regskliek op die ikoon: kieslys.",
L"TypeBox Instellings",
L"Voorkoms", L"Konsole", L"Lettertipe", L"Vortsleutel", L"Uitvoering",
L"EasyType", L"Geskiedenis", L"Logboek", L"Aliasse", L"Klanke",
L"Inproppe", L"Omgewing", L"Taal",
L"Wys konsole by begin", L"Versteek konsole wanneer fokus verloor", L"Strookie-ikoon",
L"Balloonwenke", L"Versteek outomaties na luier", L"Luier sekondes",
L"Prosesprioriteit",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"Interne bevele (/...)", L"Aliasse en ~stelselaliasses", L"Webwerwe (www. / http)",
L"E-pos (mailto)", L"Leërs en vouers", L"Shell-bevele (cmd /c)", L"Inproppe",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"Vee geskiedenis",
C, C, C, C, C,
L"Naam", L"Aksie", L"Werklader", L"Venster", L"Voeg by", L"Dateer op", L"Vee uit", C,
C, C, C, C, C, C, C, C, C, C,
L"Laai by begin", L"Berg in geskiedenis", L"Skandeer weer", L"Laai almal uit",
L"Aangaande", L"Opsies", C,
C, C, C, C, C, C,
L"Koppelvlaktaal (Toepas werk dadelik):",
L"Universele vortsleutel-konsole + bash + lanseerder vir Windows.\n"
L"Draagbare INI-konfigurasie, aliasse met %s/%1..%n, inproppe.\n\n"
L"Wortels: gaan TypeAndRun van Evgeniy Galantsev voort\n"
L"(github.com/galancev/typeandrun, GPL-3.0).",
L"Interne bevele:" };

static const Pack kAm = {
L"እሺ", L"ተግብር", L"ሰርዝ",
L"አሳይ / አደብቅ", L"ቅንብሮች...", L"ዕቅድ (TypeBox.ini)", L"ውጣ",
L"ለመክፈት አጭር ቁልብ ይጫኑ። አዶውን ቀኝ-ጠቅ ያድርጉ፦ ዝርዝር።",
L"TypeBox ቅንብሮች",
L"መልክ", L"ኮንሶል", L"ፊደል", L"አጭር ቁልብ", L"ትግበራ",
L"EasyType", L"ታሪክ", L"ሪፖርት", L"ቅጥያ ስሞች", L"ድምፆች",
L"ተደራጮች", L"አካባቢ", L"ቋንቋ",
L"በመጀመሪያ ኮንሶል አሳይ", L"ትኩረት ᲃላ ሲጠፋ አደብቅ", L"የስር ምልክት",
L"የአረንጓዴ ምክር", L"ከምሽቱ በኋላ አደብቅ", L"የምሽቱ ሰከንዶች", L"የሂደት ቅድሚያ",
C, C, C, C, C, C, C, C, C,
C, C, C, C, C,
C, C, C,
L"የውስጥ ትዕዛዞች (/...)", L"ቅጥያ ስሞች እና ~የስርቅተት ስሞች", L"ድረ-ገጾች (www. / http)",
L"ኢሜይል (mailto)", L"ፋይሎች እና አቃልያዎች", L"የኮሌት ትዕዛዞች (cmd /c)", L"ተደራጮች",
C, C, C, C, C, C, C, C,
C, C, C, C, C, C, C, C, C, L"ታሪክን አጽዳ",
C, C, C, C, C,
L"ስም", L"ትግበራ", L"የሥራ አቃልያ", L"መስኮት", L"ጨምር", L"አዘምን", L"አጥፋ", C,
C, C, C, C, C, C, C, C, C, C,
L"በመጀመሪያ ጫን", L"ወደ ታሪክ አስቀምጥ", L"እንደገና ፍለጋ", L"ሁሉንም አውጣ",
L"ስለ", L"አማራጮች", C,
C, C, C, C, C, C,
L"የመሳሪያ ቋንቋ (ተግብር ወዲያውኑ ይሠራል)፦",
L"ለዊንዶውስ አጠቃላይ አጭር ቁልብ ኮንሶል + bash + አስጀማሪ።\n"
L"ተንቀሳቃሽ INI ዕቅድ፣ ከ%s/%1..%n ጋር ቅጥያ ስሞች፣ ተደራጮች።\n\n"
L"መሠረት፦ የEvgeniy Galantsevን TypeAndRun ይቀጥላል\n"
L"(github.com/galancev/typeandrun, GPL-3.0)።",
L"የውስጥ ትዕዛዞች:" };

static const Pack* const kPacks[LANG_COUNT] = {
    &kEn, &kRu, &kUk, &kDe, &kFr, &kEs, &kPt, &kIt,
    &kZh, &kJa, &kKo, &kAr, &kTr, &kPl, &kVi, &kHi, &kId, &kSw, &kAf, &kAm };

// ------------------------------------------------------ console context menu strings
// Kept as one compact table instead of 20 pack-tail appends (v0.14). Order of
// columns == STR_CTX_UNDO .. STR_CTX_EXIT. C (=L"") falls back to English,
// exactly like the sparse tails of the smaller packs.
static const wchar_t* const kCtxItems[LANG_COUNT][16] = {
/* en */ { L"Undo", L"Cut", L"Copy", L"Paste", L"Select All",
           L"Settings...", L"Aliases...", L"Plugins...", L"Theme...", L"Language...",
           L"Reload config", L"Open config folder", L"About...", L"Help...",
           L"Hide", L"Exit" },
/* ru */ { L"Отменить", L"Вырезать", L"Копировать", L"Вставить", L"Выделить всё",
           L"Настройки…", L"Псевдонимы…", L"Плагины…", L"Тема…", L"Язык…",
           L"Перечитать конфиг", L"Открыть папку конфига", L"О программе…",
           L"Справка…", L"Скрыть", L"Выход" },
/* uk */ { L"Скасувати", L"Вирізати", L"Копіювати", L"Вставити", L"Виділити все",
           L"Параметри…", L"Псевдоніми…", L"Плагіни…", L"Тема…", L"Мова…",
           L"Перечитати конфіг", L"Відкрити теку конфіга", L"Про програму…",
           L"Довідка…", L"Приховати", L"Вихід" },
/* de */ { L"Rückgängig", L"Ausschneiden", L"Kopieren", L"Einfügen", L"Alles markieren",
           L"Einstellungen…", L"Aliase…", L"Plugins…", L"Design…", L"Sprache…",
           L"Konfig neu laden", L"Konfig-Ordner öffnen", L"Über…", L"Hilfe…",
           L"Ausblenden", L"Beenden" },
/* fr */ { L"Annuler", L"Couper", L"Copier", L"Coller", L"Tout sélectionner",
           L"Paramètres…", L"Alias…", L"Plugins…", L"Thème…", L"Langue…",
           L"Recharger la config", L"Ouvrir le dossier config", L"À propos…",
           L"Aide…", L"Masquer", L"Quitter" },
/* es */ { L"Deshacer", L"Cortar", L"Copiar", L"Pegar", L"Seleccionar todo",
           L"Opciones…", L"Aliases…", L"Complementos…", L"Tema…", L"Idioma…",
           L"Recargar la config", L"Abrir carpeta de config", L"Acerca de…",
           L"Ayuda…", L"Ocultar", L"Salir" },
/* pt */ { L"Desfazer", L"Recortar", L"Copiar", L"Colar", L"Selecionar tudo",
           L"Configurações…", L"Atalhos…", L"Plugins…", L"Tema…", L"Idioma…",
           L"Recarregar a config", L"Abrir pasta da config", L"Sobre…", L"Ajuda…",
           L"Ocultar", L"Sair" },
/* it */ { L"Annulla", L"Taglia", L"Copia", L"Incolla", L"Seleziona tutto",
           L"Impostazioni…", L"Alias…", L"Plugin…", L"Tema…", L"Lingua…",
           L"Ricarica la config", L"Apri cartella config", L"Informazioni…",
           L"Guida…", L"Nascondi", L"Esci" },
/* zh */ { L"撤销", L"剪切", L"复制", L"粘贴", L"全选",
           L"设置…", L"别名…", L"插件…", L"主题…", L"语言…",
           L"重新载入配置", L"打开配置文件夹", L"关于…", L"帮助…", L"隐藏", L"退出" },
/* ja */ { L"元に戻す", L"切り取り", L"コピー", L"貼り付け", L"すべて選択",
           L"設定…", L"エイリアス…", L"プラグイン…", L"テーマ…", L"言語…",
           L"設定を再読み込み", L"設定フォルダーを開く", L"バージョン情報…",
           L"ヘルプ…", L"隠す", L"終了" },
/* ko */ { L"실행 취소", L"잘라내기", L"복사", L"붙여넣기", L"전체 선택",
           L"설정…", L"별칭…", L"플러그인…", L"테마…", L"언어…",
           L"설정 다시 읽기", L"설정 폴더 열기", L"정보…", L"도움말…",
           L"숨기기", L"종료" },
/* ar */ { L"تراجع", L"قص", L"نسخ", L"لصق", L"تحديد الكل",
           L"إعدادات…", L"أسماء مستعارة…", L"إضافات…", L"سمة…", L"لغة…",
           L"إعادة تحميل الإعدادات", L"فتح مجلد الإعدادات", L"حول البرنامج…",
           L"مساعدة…", L"إخفاء", L"خروج" },
/* tr */ { L"Geri Al", L"Kes", L"Kopyala", L"Yapıştır", L"Tümünü Seç",
           L"Ayarlar…", L"Takma Adlar…", L"Eklentiler…", L"Tema…", L"Dil…",
           L"Yapılandırmayı yenile", L"Yapılandırma klasörünü aç", L"Hakkında…",
           L"Yardım…", L"Gizle", L"Çık" },
/* pl */ { L"Cofnij", L"Wytn", L"Kopiuj", L"Wklej", L"Zaznacz wszystko",
           L"Ustawienia…", L"Aliasy…", L"Wtyczki…", L"Motyw…", L"Język…",
           L"Przeładuj konfigurację", L"Otwórz folder konfiguracji",
           L"O programie…", L"Pomoc…", L"Ukryj", L"Wyjdź" },
/* vi */ { L"Hoàn tác", L"Cắt", L"Sao chép", L"Dán", L"Chọn tất cả",
           L"Cài đặt…", L"Bí danh…", L"Plugin…", L"Chủ đề…", L"Ngôn ngữ…",
           L"Nạp lại cấu hình", L"Mở thư mục cấu hình", L"Giới thiệu…",
           L"Trợ giúp…", L"Ẩn", L"Thoát" },
/* hi */ { L"पूर्ववत करें", L"काटें", L"कॉपी करें", L"पेस्ट करें", L"सब चुनें",
           L"सेटिंग्स…", L"उपनाम…", L"प्लगइन…", L"थीम…", L"भाषा…",
           L"कॉन्फ़िग फिर लोड करें", L"कॉन्फ़िग फ़ोल्डर खोलें", L"परिचय…",
           L"सहायता…", L"छिपाएँ", L"बाहर निकलें" },
/* id */ { L"Urungkan", L"Potong", L"Salin", L"Tempel", L"Pilih Semua",
           L"Pengaturan…", L"Alias…", L"Plugin…", L"Tema…", L"Bahasa…",
           L"Muat ulang konfigurasi", L"Buka folder konfigurasi", L"Tentang…",
           L"Bantuan…", L"Sembunyikan", L"Keluar" },
/* sw */ { C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C },
/* af */ { C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C },
/* am */ { C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C },
};

// v0.15 theme studio strings. Columns == STR_TH_BG .. STR_TH_PORTNOTE.
static const wchar_t* const kThItems[LANG_COUNT][9] = {
/* en */ { L"Background", L"Text", L"Accent", L"Border", L"Border width (px)",
           L"New theme name", L"Save as...", L"Delete",
           L"Custom themes live in themes\\<name>.theme.ini - portable with the folder." },
/* ru */ { L"Фон", L"Текст", L"Акцент", L"Рамка", L"Толщина рамки (px)",
           L"Имя новой темы", L"Сохранить как…", L"Удалить",
           L"Свои темы лежат в themes\\<имя>.theme.ini и переносятся вместе с папкой." },
/* uk */ { L"Фон", L"Текст", L"Акцент", L"Рамка", L"Товщина рамки (px)",
           L"Ім'я нової теми", L"Зберегти як…", L"Видалити",
           L"Власні теми лежать у themes\\<ім'я>.theme.ini й переносяться з папкою." },
/* de */ { L"Hintergrund", L"Text", L"Akzent", L"Rahmen", L"Rahmenbreite (px)",
           L"Neuer Theme-Name", L"Speichern unter…", L"Löschen",
           L"Eigene Themes liegen in themes\\<name>.theme.ini -portabel mit dem Ordner." },
/* fr */ { L"Arrière-plan", L"Texte", L"Accent", L"Bordure", L"Épaisseur de bordure (px)",
           L"Nom du nouveau thème", L"Enregistrer sous…", L"Supprimer",
           L"Les thèmes personnalisés sont dans themes\\<nom>.theme.ini — portables." },
/* es */ { L"Fondo", L"Texto", L"Acento", L"Borde", L"Grosor del borde (px)",
           L"Nombre del tema nuevo", L"Guardar como…", L"Eliminar",
           L"Los temas propios están en themes\\<nombre>.theme.ini - portables." },
/* pt */ { L"Fundo", L"Texto", L"Destaque", L"Borda", L"Espessura da borda (px)",
           L"Nome do novo tema", L"Salvar como…", L"Excluir",
           L"Temas personalizados ficam em themes\\<nome>.theme.ini - portáteis." },
/* it */ { L"Sfondo", L"Testo", L"Accento", L"Bordo", L"Spessore bordo (px)",
           L"Nome del nuovo tema", L"Salva come…", L"Elimina",
           L"i temi personalizzati sono in themes\\<nome>.theme.ini - portabili." },
/* zh */ { L"背景", L"文字", L"强调色", L"边框", L"边框宽度 (px)",
           L"新主题名称", L"另存为…", L"删除",
           L"自制主题保存在 themes\\<名称>.theme.ini，随文件夹一起携带。" },
/* ja */ { L"背景", L"文字", L"アクセント", L"枠線", L"枠の太さ (px)",
           L"新しいテーマ名", L"名前を付けて保存…", L"削除",
           L"自作テーマは themes\\<名前>.theme.ini に保存され、フォルダと一緒に持ち歩けます。" },
/* ko */ { L"배경", L"텍스트", L"강조색", L"테두리", L"테두리 두께(px)",
           L"새 테마 이름", L"다른 이름으로 저장…", L"삭제",
           L"직접 만든 테마는 themes\\<이름>.theme.ini에 저장되어 폴더와 함께 이동합니다." },
/* ar */ { L"خلفية", L"نص", L"لون مميز", L"حد", L"سماكة الحد (بكسل)",
           L"اسم السمة الجديد", L"حفظ باسم…", L"حذف",
           L"السمات المخصصة محفوظة في themes\\<name>.theme.ini وتنتقل مع المجلد." },
/* tr */ { L"Arka plan", L"Metin", L"Vurgu", L"Kenarlık", L"Kenarlık kalınlığı (px)",
           L"Yeni tema adı", L"Farklı kaydet…", L"Sil",
           L"Özel temalar themes\\<ad>.theme.ini içinde klasörle taşınır." },
/* pl */ { L"Tło", L"Tekst", L"Akcent", L"Obramowanie", L"Grubość obramowania (px)",
           L"Nazwa nowego motywu", L"Zapisz jako…", L"Usuń",
           L"Własne motywy są w themes\\<nazwa>.theme.ini - przenośne razem z folderem." },
/* vi */ { L"Nền", L"Văn bản", L"Nhấn", L"Viền", L"Độ dày viền (px)",
           L"Tên chủ đề mới", L"Lưu thành…", L"Xóa",
           L"Chủ đề tự tạo nằm trong themes\\<tên>.theme.ini - di chuyển cùng thư mục." },
/* hi */ { L"पृष्ठभूमि", L"टेक्स्ट", L"एक्सेंट", L"किनारा", L"किनारे की मोटाई (px)",
           L"नई थीम का नाम", L"के रूप में सहेजें…", L"हटाएँ",
           L"अपनी थीम themes\\<नाम>.theme.ini में रहती हैं - फ़ोल्डर के साथ पोर्टेबल।" },
/* id */ { L"Latar", L"Teks", L"Aksen", L"Bingkai", L"Ketebalan bingkai (px)",
           L"Nama tema baru", L"Simpan sebagai…", L"Hapus",
           L"Tema khusus ada di themes\\<nama>.theme.ini - portabel bersama folder." },
/* sw */ { C, C, C, C, C, C, C, C, C },
/* af */ { C, C, C, C, C, C, C, C, C },
/* am */ { C, C, C, C, C, C, C, C, C },
};

// ---------------------------------------------------------------- runtime state
static int g_langIdx = LANG_EN;
static std::map<std::wstring, std::wstring> g_i18nUser;   // key -> override

static std::wstring I18nFromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// lang\<code>.ini overrides (portable community translations); UTF-8 or UTF-16LE.
static void LoadLangOverrides(int idx) {
    g_i18nUser.clear();
    std::wstring path = ExeDir() + L"lang\\" + kLangs[idx].code + L".ini";
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    std::vector<char> raw;
    char buf[4096]; DWORD rd = 0;
    while (ReadFile(f, buf, sizeof(buf), &rd, nullptr) && rd > 0)
        raw.insert(raw.end(), buf, buf + rd);
    CloseHandle(f);
    std::wstring text;
    if (raw.size() >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE)
        text.assign((const wchar_t*)&raw[2], (raw.size() - 2) / sizeof(wchar_t));
    else if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF &&
             (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        text = I18nFromUtf8(std::string(&raw[3], raw.size() - 3));
    else
        text = I18nFromUtf8(std::string(raw.begin(), raw.end()));
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find(L'\n', pos);
        std::wstring line = text.substr(pos, nl == std::wstring::npos
                                               ? std::wstring::npos : nl - pos);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t eq = line.find(L'=');
        if (!line.empty() && line[0] != L';' && line[0] != L'#' &&
            line[0] != L'[' && eq != std::wstring::npos) {
            std::wstring k = line.substr(0, eq);
            std::wstring v = line.substr(eq + 1);
            while (!k.empty() && iswspace(k.back())) k.pop_back();
            size_t s = v.find_first_not_of(L" \t");
            v = s == std::wstring::npos ? std::wstring() : v.substr(s);
            g_i18nUser[k] = v;
        }
        if (nl == std::wstring::npos) break;
        pos = nl + 1;
    }
}

static int LangAutoDetect() {
    WORD prim = PRIMARYLANGID(GetUserDefaultUILanguage());
    for (int i = 0; i < LANG_COUNT; ++i)
        if (kLangs[i].langid == prim) return i;
    // fall back to user locale when the UI language is unknown
    WORD prim2 = PRIMARYLANGID(GetUserDefaultLangID());
    for (int i = 0; i < LANG_COUNT; ++i)
        if (kLangs[i].langid == prim2) return i;
    return LANG_EN;
}

// "auto" | "ru" | "ru-RU" | "zh-CN" -> index
static int LangFromCode(const std::wstring& code) {
    if (code.empty() || _wcsicmp(code.c_str(), L"auto") == 0)
        return LangAutoDetect();
    std::wstring c = code;
    size_t dash = c.find(L'-');
    if (dash != std::wstring::npos) c = c.substr(0, dash);
    for (int i = 0; i < LANG_COUNT; ++i)
        if (_wcsicmp(c.c_str(), kLangs[i].code) == 0) return i;
    return LangAutoDetect();
}

static void SetLanguage(int idx) {
    if (idx < 0 || idx >= LANG_COUNT) idx = LANG_EN;
    g_langIdx = idx;
    LoadLangOverrides(idx);
}

static const wchar_t* L(int id) {
    if (id < 0 || id >= STR_LAST) return L"?";
    // 1) portable user override  2) current pack  3) ctx table  4) English
    auto it = g_i18nUser.find(std::wstring(I18nFromUtf8(kKeyNames[id])));
    if (it != g_i18nUser.end() && !it->second.empty()) return it->second.c_str();
    const wchar_t* s = kPacks[g_langIdx]->items[id];
    if (s && *s) return s;
    if (id >= STR_CTX_UNDO && id <= STR_CTX_EXIT) {
        const wchar_t* x = kCtxItems[g_langIdx][id - STR_CTX_UNDO];
        if (x && *x) return x;
        x = kCtxItems[LANG_EN][id - STR_CTX_UNDO];   // packs carry no ctx tail
        if (x && *x) return x;
        return L"?";
    }
    if (id >= STR_TH_BG && id <= STR_TH_PORTNOTE) {
        const wchar_t* x = kThItems[g_langIdx][id - STR_TH_BG];
        if (x && *x) return x;
        x = kThItems[LANG_EN][id - STR_TH_BG];
        if (x && *x) return x;
        return L"?";
    }
    return kEn.items[id];
}

static bool LangRtl() { return kLangs[g_langIdx].rtl; }

// Script-capable console font when the user never touched the default.
static const wchar_t* ConsoleFontSuggestion(const wchar_t* chosen) {
    if (chosen && *chosen && _wcsicmp(chosen, L"Courier New") != 0) return chosen;
    switch (g_langIdx) {
        case LANG_ZH: return L"Microsoft YaHei UI";
        case LANG_JA: return L"Yu Gothic UI";
        case LANG_KO: return L"Malgun Gothic";
        case LANG_AR: return L"Segoe UI";
        case LANG_HI:
        case LANG_AM: return L"Nirmala UI";
        default:      return L"Courier New";
    }
}

#endif // TYPEBOX_I18N_H
