// TypeBox — universal hotkey console + bash + launcher for Windows.
// Licensed under GPL-3.0 (see LICENSE / docs/PLAN.md).
//
// Roots: TypeAndRun by Evgeniy Galantsev (github.com/galancev/typeandrun, GPL-3.0)
// — command/alias engine, INI schema, internal commands and plugin ABI continue
// that project.
// Inspired by: Microsoft PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,
// ConEmu (portable INI config), Guake/Yakuake/Drop (console/bash mode).
//
// Build (MSVC):  cmake -S . -B build && cmake --build build --config Release
// Build (MSVC):  cmake -S . -B build && cmake --build build --config Release
// Build (MinGW): g++ -std=c++17 -DUNICODE -D_UNICODE -O2 src/main.cpp
//                -o TypeBox.exe -municode -mwindows
//                -luser32 -lshell32 -lshlwapi -lgdi32 -lcomctl32 -lwinmm

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cstring>
#include <cwctype>
#include <functional>

#ifndef TYPEBOX_VERSION
#define TYPEBOX_VERSION 0.0.0-dev
#endif
#define TB_STR_(x) #x
#define TB_STR(x) TB_STR_(x)

static const wchar_t* kClassName  = L"TypeBoxMain";
static const int      kHotkeyId   = 666; // same lineage as TypeAndRun HotKeyId=666
static const UINT     kTrayMsg    = WM_APP + 1;
static const UINT     kTrayId     = 1;

// ------------------------------------------------------------------ state

struct Hotkey {
    bool alt = false, ctrl = false, shift = false, win = false;
    WORD key = VK_F2;
    WORD mods() const {
        WORD m = 0;
        if (alt)   m |= MOD_ALT;
        if (ctrl)  m |= MOD_CONTROL;
        if (shift) m |= MOD_SHIFT;
        if (win)   m |= MOD_WIN;
        return m;
    }
};

struct Alias {
    std::wstring name;
    std::wstring action;
    std::wstring dir;          // working directory (pipe form field 3)
    int          state = SW_SHOWNORMAL; // window state (pipe form field 4)
};

// TypeAndRun system aliases: ~lock, ~logoff, ... (name starts with '~')
struct SysAlias {
    std::wstring name;
    std::wstring action;
};

struct Settings {
    // [Show]
    int  width = 720, height = 56, x = -1, y = -1;   // 56 = line + status strip
    bool centerX = true, centerY = true;
    bool hideOnDeactivate = true, trayIcon = true, balloonTips = true;
    bool showOnStart = true;
    int  transparent = 255;    // 0..255 alpha
    bool autoHide = false;     // hide after AutoHideValue seconds of idleness
    int  autoHideValue = 30;
    // [Exec]
    bool execInternal = true, execAliases = true, execWww = true,
         execEmail = true, execFiles = true, execShell = true, execPlugins = true;
    // [Font]
    std::wstring fontName = L"Courier New";
    int  fontSize = 18;
    bool fontBold = true;
    bool applyEnglish = false;   // force US-English layout when the console opens
    // [Show] Priority of the TypeBox process: idle|normal|high|realtime
    std::wstring priority = L"normal";
    // [UI] Language: auto | en ru uk de fr es pt it zh ja ko ar tr pl vi hi id sw af am
    std::wstring langCode = L"auto";
    // [Theme] Name — cozy color scheme: cozy-dark paper-light midnight-blue
    // nord-frost solarized-dark solarized-light forest-night
    std::wstring theme = L"cozy-dark";
    // [Type] EasyType completion
    int  easyType = 2;         // 0=off, 1=easy, 2=linux-style (Tab completes first word)
    bool etInternal = true, etAlias = true, etHistory = true, etSysAlias = true;
    // [Color] scheme 0 (COLORREF integers, -1 = system default)
    int  consoleBg = -1, consoleFg = -1;
    // [History] [Undo]
    int  historyMax = 200;
    bool useUndo = true;
    bool saveInternal = true, saveAlias = true, saveWww = true, saveEmail = true,
         savePath = true, savePlugins = true, saveUnknown = true;
    // [Sounds]
    bool enableSounds = true;
    std::wstring sound[9];     // per-type wav paths (see kSoundKeys), "" = none
    // [Log] — portable error log
    bool  logEnabled = true;
    std::wstring logFile = L"TypeBox.log";
    int   logMaxKB = 512;      // size cap
    int   logRewrite = 0;      // 0 rotate (.1 .2 ...), 1 truncate, 2 timestamp, 3 keep (stop)
    int   logKeep = 3;         // rotated copies to keep
};

// Execution result types (map to [History]/[Sounds] keys, like TypeAndRun).
enum ExecType {
    EXC_NONE = 0, EXC_INTERNAL, EXC_ALIAS, EXC_SYSALIAS, EXC_URL,
    EXC_EMAIL, EXC_FILE, EXC_FOLDER, EXC_PLUGIN, EXC_SHELL
};

static Hotkey                    g_hotkey;
static Settings                  g_set;
static bool                      g_visible = false;
static HWND                      g_edit    = nullptr;
static bool                      g_testSettings = false;
static std::wstring              g_iniPath;
static std::map<std::wstring, int> g_pluginState; // "Name_Load"/"Name_SaveHistory" -> 0/1
static std::vector<std::wstring> g_history;
static size_t                    g_histPos = 0;
static std::vector<Alias>        g_aliases;
static std::vector<SysAlias>     g_sysAliases;
static std::vector<std::wstring> g_ignores;    // Ignores.ini entries (BadApp)
static HANDLE                    g_aliasProc = nullptr; // last alias process
static std::wstring              g_aliasName;
static NOTIFYICONDATAW           g_nid     = {};
static std::wstring              g_lastRun; // for Undo
// EasyType completion state
static std::vector<std::wstring> g_etList;
static size_t                    g_etPos    = 0;
static std::wstring              g_etPrefix;
static HBRUSH                    g_bgBrush  = nullptr;

// ---------------------------------------------------------------- utilities

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    std::wstring dir(buf);
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';
    return dir;
}

#include "i18n.h"   // localization: L(STR_*), language packs, RTL

static std::wstring ReadIniStr(const wchar_t* sec, const wchar_t* key, const wchar_t* def) {
    wchar_t buf[4096] = {};
    GetPrivateProfileStringW(sec, key, def, buf, 4096, g_iniPath.c_str());
    return buf;
}

static int ReadIniInt(const wchar_t* sec, const wchar_t* key, int def) {
    return GetPrivateProfileIntW(sec, key, def, g_iniPath.c_str());
}

static bool ReadIniBool(const wchar_t* sec, const wchar_t* key, bool def) {
    return ReadIniInt(sec, key, def ? 1 : 0) != 0;
}

static std::wstring Trim(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return L"";
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::wstring Lower(const std::wstring& s) {
    std::wstring o = s;
    for (auto& c : o) c = (wchar_t)towlower(c);
    return o;
}

static std::wstring FirstWord(const std::wstring& s) {
    size_t sp = s.find_first_of(L" \t");
    return sp == std::wstring::npos ? s : s.substr(0, sp);
}

static std::wstring RestWords(const std::wstring& s) {
    size_t sp = s.find_first_of(L" \t");
    return sp == std::wstring::npos ? L"" : Trim(s.substr(sp + 1));
}

static std::vector<std::wstring> SplitWords(const std::wstring& s) {
    std::vector<std::wstring> words;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
        size_t b = i;
        while (i < s.size() && s[i] != L' ' && s[i] != L'\t') ++i;
        if (i > b) words.push_back(s.substr(b, i - b));
    }
    return words;
}

static std::vector<std::wstring> SplitPipe(const std::wstring& s) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (true) {
        size_t p = s.find(L'|', start);
        if (p == std::wstring::npos) { parts.push_back(Trim(s.substr(start))); break; }
        parts.push_back(Trim(s.substr(start, p - start)));
        start = p + 1;
    }
    return parts;
}

// TypeBox's own environment, mirroring the Windows "Environment Variables"
// dialog (System Properties -> Advanced): [Path] folders are prepended to the
// process PATH (aliases, shell lines and every child process pick them up),
// [Variables] define custom %NAME% substitutions.
struct EnvVar { std::wstring name, value, raw; }; // raw = as written in the INI
static std::vector<EnvVar>       g_envVars;
static std::vector<std::wstring> g_pathDirs;   // expanded
static std::vector<std::wstring> g_pathRaw;    // as written in the INI (for GUI round-trip)
static std::wstring              g_basePath;   // PATH as it was when the process started

// Prepend our folders to PATH. Called at startup and whenever settings apply.
static void ApplyEnvPath() {
    if (g_basePath.empty()) {
        std::vector<wchar_t> buf(64 * 1024);
        DWORD n = GetEnvironmentVariableW(L"PATH", buf.data(), (DWORD)buf.size());
        g_basePath = (n > 0 && n < buf.size()) ? buf.data() : L"";
    }
    std::wstring merged;
    for (auto& d : g_pathDirs) { if (d.empty()) continue; merged += d; merged += L';'; }
    merged += g_basePath;
    SetEnvironmentVariableW(L"PATH", merged.c_str());
}

// Replace %VAR% (any env var, case-insensitive) and %TYPEBOXDIR%.
// Custom [Variables] from TypeBox.ini (TypeBox's own "user variables", OS-style)
// take precedence over the process environment.
static std::wstring ExpandEnv(const std::wstring& s) {
    std::wstring out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == L'%') {
            size_t close = s.find(L'%', i + 1);
            if (close != std::wstring::npos) {
                std::wstring name = s.substr(i + 1, close - i - 1);
                std::wstring val;
                if (_wcsicmp(name.c_str(), L"TYPEBOXDIR") == 0) {
                    std::wstring d = ExeDir();
                    val = d.substr(0, d.size() - 1); // like legacy %TARDIR%
                } else {
                    bool custom = false;
                    for (auto& v : g_envVars)
                        if (_wcsicmp(v.name.c_str(), name.c_str()) == 0) {
                            val = v.value; custom = true; break;
                        }
                    if (!custom) {
                        wchar_t buf[4096] = {};
                        DWORD n = GetEnvironmentVariableW(name.c_str(), buf, 4096);
                        if (n > 0 && n < 4096) val = buf;
                    }
                }
                out += val; // unknown vars expand to empty (like TypeAndRun)
                i = close + 1;
                continue;
            }
        }
        out += s[i++];
    }
    return out;
}

// Replace %s and %1..%n with words of the typed remainder (TypeAndRun semantics).
static std::wstring Substitute(const std::wstring& action, const std::wstring& input) {
    std::vector<std::wstring> words = SplitWords(input);
    std::wstring out = action;
    size_t pos;
    while ((pos = out.find(L"%s")) != std::wstring::npos)
        out.replace(pos, 2, input);
    for (int n = 1; n <= 9; ++n) {
        std::wstring tok = L"%" + std::to_wstring(n);
        std::wstring val = (size_t)n <= words.size() ? words[n - 1] : L"";
        while ((pos = out.find(tok)) != std::wstring::npos)
            out.replace(pos, tok.size(), val);
    }
    return out;
}

static bool LooksLikeUrl(const std::wstring& s) {
    return (_wcsnicmp(s.c_str(), L"http://", 7) == 0 ||
            _wcsnicmp(s.c_str(), L"https://", 8) == 0 ||
            _wcsnicmp(s.c_str(), L"www.", 4) == 0);
}

static bool LooksLikeEmail(const std::wstring& s) {
    size_t at = s.find(L'@');
    return at != std::wstring::npos && at > 0 && s.find(L'.', at) != std::wstring::npos &&
           s.find_first_of(L" \t") == std::wstring::npos;
}

static std::wstring GetEditText(HWND edit) {
    int len = GetWindowTextLengthW(edit);
    std::wstring s(len, L'\0');
    GetWindowTextW(edit, &s[0], len + 1);
    return Trim(s);
}

static std::string ToUtf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::wstring WVersion() {
    static std::wstring v = FromUtf8(TB_STR(TYPEBOX_VERSION));
    return v;
}

// ------------------------------------------------------------------- logging
//
// Portable error log (no Event Viewer, no %LOCALAPPDATA%): everything TypeBox
// cannot do itself is appended to [Log] File. When the file exceeds
// [Log] MaxSizeKB the rewrite method from [Log] Rewrite kicks in:
//   rotate    — file -> file.1 -> file.2 ... (Keep copies kept)      [default]
//   truncate  — keep only the newest half
//   timestamp — rename to file-YYYYMMDD-HHMMSS.log, start a new one
//   keep      — never rewrite; further lines are dropped

static std::wstring LogPath() {
    std::wstring f = g_set.logFile;
    if (f.empty()) return L"";
    return PathIsRelativeW(f.c_str()) ? ExeDir() + f : f;
}

static void LogWrite(const wchar_t* level, const std::wstring& msg) {
    if (!g_set.logEnabled) return;
    std::wstring path = LogPath();
    if (path.empty()) return;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
        ULARGE_INTEGER sz;
        sz.HighPart = fad.nFileSizeHigh; sz.LowPart = fad.nFileSizeLow;
        if (sz.QuadPart > (ULONGLONG)g_set.logMaxKB * 1024) {
            switch (g_set.logRewrite) {
            case 3: // keep: stop writing, preserve the file as-is
                return;
            case 2: { // timestamp: seal the old file, start fresh
                SYSTEMTIME st; GetLocalTime(&st);
                std::wstring stem = path, ext;
                size_t dot = path.find_last_of(L'.');
                if (dot != std::wstring::npos && dot > path.find_last_of(L'\\')) {
                    stem = path.substr(0, dot); ext = path.substr(dot);
                }
                wchar_t stamp[64];
                wsprintfW(stamp, L"-%04d%02d%02d-%02d%02d%02d",
                          st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                if (!MoveFileExW(path.c_str(), (stem + stamp + ext).c_str(),
                                 MOVEFILE_REPLACE_EXISTING)) {
                    // file busy? degrade to truncate so the log still stays capped
                    FILE* w = _wfopen(path.c_str(), L"wb");
                    if (w) fclose(w);
                }
                break;
            }
            case 1: { // truncate: keep the newest half of the file
                long half = (g_set.logMaxKB * 1024) / 2;
                std::string tail;
                if (FILE* r = _wfopen(path.c_str(), L"rb")) {
                    _fseeki64(r, -(long long)half, SEEK_END);
                    char buf[8192]; size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), r)) > 0) tail.append(buf, n);
                    fclose(r);
                }
                FILE* w = _wfopen(path.c_str(), L"wb");
                if (w) {
                    const char* mark = "--- log truncated (old entries dropped) ---\n";
                    fwrite(mark, 1, strlen(mark), w);
                    if (!tail.empty()) fwrite(tail.data(), 1, tail.size(), w);
                    fclose(w);
                }
                break;
            }
            default: { // rotate: file -> file.1 -> ... -> file.Keep
                for (int i = g_set.logKeep; i >= 1; --i) {
                    std::wstring from = path + (i == 1 ? L"" : L"." + std::to_wstring(i - 1));
                    std::wstring to   = path + L"." + std::to_wstring(i);
                    if (i == 1) MoveFileExW(path.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING);
                    else        MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING);
                }
                break;
            }
            }
        }
    }

    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t head[64];
    wsprintfW(head, L"[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, level);
    std::string line = ToUtf8(head) + ToUtf8(msg) + "\n";
    if (FILE* f = _wfopen(path.c_str(), L"ab")) {
        fwrite(line.data(), 1, line.size(), f);
        fclose(f);
    }
}

static void SetTrayVisual();
static bool g_trayErr = false;   // set by LogError, cleared on Apply
static void LogError(const std::wstring& msg) {
    LogWrite(L"ERROR", msg);
    g_trayErr = true;   // cozy badge turns red until the next Apply
    SetTrayVisual();
}
static void LogInfo(const std::wstring& msg) { LogWrite(L"INFO", msg); }

// ---------------------------------------------------------------- settings

static void LoadSysAliases();
static void LoadIgnores();

// The 9 per-execution-type sounds of TypeAndRun, in [Sounds] key order.
static const wchar_t* kSoundKeys[9] = {
    L"ExecuteInternalSound", L"ExecuteAliasSound", L"ExecuteSysAliasSound",
    L"ExecuteURLSound", L"ExecuteEmailSound", L"ExecuteFileSound",
    L"ExecuteFolderSound", L"ExecutePluginSound", L"ExecuteShellSound" };
// [Sounds] per-type labels moved to i18n (STR_SO_INTERNAL + i).

// [Path] one folder per key (Dir1=.., 2=..) or semicolons in "Extra=..";
// [Variables] name=value. %VARS% are expanded, comments ignored.
static void LoadEnvironment() {
    g_pathDirs.clear();
    g_envVars.clear();

    auto parseSection = [&](const wchar_t* section,
                            std::function<void(const std::wstring&, const std::wstring&)> on) {
        std::vector<wchar_t> buf(64 * 1024);
        DWORD n = GetPrivateProfileSectionW(section, buf.data(), (DWORD)buf.size(), g_iniPath.c_str());
        for (wchar_t* q = buf.data(); q < buf.data() + n; ) {
            std::wstring ln(q);
            q += ln.size() + 1;
            if (ln.empty() || ln[0] == L';') continue;
            size_t eq = ln.find(L'=');
            if (eq == std::wstring::npos) continue;
            on(Trim(ln.substr(0, eq)), Trim(ln.substr(eq + 1)));
        }
    };

    // variables first: [Path] entries may reference %THEM%
    parseSection(L"Variables", [&](const std::wstring& key, const std::wstring& value) {
        if (key.empty() || value.empty()) return;
        EnvVar v; v.name = key; v.raw = value; v.value = ExpandEnv(value);
        g_envVars.push_back(v);
    });
    parseSection(L"Path", [&](const std::wstring&, const std::wstring& value) {
        // split on ';' so both "Extra=a;b" and one-dir-per-key work
        size_t start = 0;
        while (start <= value.size()) {
            size_t sc = value.find(L';', start);
            std::wstring raw = Trim(value.substr(start, sc == std::wstring::npos
                                                       ? std::wstring::npos : sc - start));
            if (!raw.empty()) {
                std::wstring d = ExpandEnv(raw);
                if (d.back() == L'\\') d.pop_back();
                if (!d.empty()) { g_pathDirs.push_back(d); g_pathRaw.push_back(raw); }
            }
            if (sc == std::wstring::npos) break;
            start = sc + 1;
        }
    });
}

static void LoadSettings() {
    g_set.width             = ReadIniInt(L"Show", L"Width", 720);
    g_set.height            = ReadIniInt(L"Show", L"Height", 56);
    g_set.x                 = ReadIniInt(L"Show", L"X", -1);
    g_set.y                 = ReadIniInt(L"Show", L"Y", -1);
    g_set.centerX           = ReadIniBool(L"Show", L"CenterScreenX", true);
    g_set.centerY           = ReadIniBool(L"Show", L"CenterScreenY", true);
    g_set.hideOnDeactivate  = ReadIniBool(L"Show", L"HideOnDeactivate", true);
    g_set.trayIcon          = ReadIniBool(L"Show", L"TrayIcon", true);
    g_set.balloonTips       = ReadIniBool(L"Show", L"BalloonTips", true);
    g_set.showOnStart       = ReadIniBool(L"Show", L"ShowConsoleOnStart", true);
    g_set.transparent       = ReadIniInt(L"Show", L"Transparent", 255);
    g_set.autoHide          = ReadIniBool(L"Show", L"AutoHide", false);
    g_set.autoHideValue     = ReadIniInt(L"Show", L"AutoHideValue", 30);

    g_set.execInternal = ReadIniBool(L"Exec", L"Internal", true);
    g_set.execAliases  = ReadIniBool(L"Exec", L"Aliases",  true);
    g_set.execWww      = ReadIniBool(L"Exec", L"WWW",      true);
    g_set.execEmail    = ReadIniBool(L"Exec", L"EMail",    true);
    g_set.execFiles    = ReadIniBool(L"Exec", L"Files",    true);
    g_set.execShell    = ReadIniBool(L"Exec", L"Shell",    true);
    g_set.execPlugins  = ReadIniBool(L"Exec", L"Plugins",  true);

    g_set.fontName = ReadIniStr(L"Font", L"Name", L"Courier New");
    g_set.fontSize = ReadIniInt(L"Font", L"Size", 18);
    g_set.fontBold = ReadIniInt(L"Font", L"Bold", 1);
    g_set.applyEnglish = ReadIniBool(L"Font", L"ApplyEnglish", false);
    g_set.priority = Lower(Trim(ReadIniStr(L"Show", L"Priority", L"normal")));
    g_set.langCode = Trim(ReadIniStr(L"UI", L"Language", L"auto"));
    g_set.theme    = Trim(ReadIniStr(L"Theme", L"Name", L"cozy-dark"));

    g_set.easyType   = ReadIniInt(L"Type", L"EasyType", 2);
    g_set.etInternal = ReadIniBool(L"Type", L"UseInternalET", true);
    g_set.etAlias    = ReadIniBool(L"Type", L"UseAliasET", true);
    g_set.etHistory  = ReadIniBool(L"Type", L"UseHistoryET", true);
    g_set.etSysAlias = ReadIniBool(L"Type", L"UseSysAliasET", true);

    g_set.consoleBg = ReadIniInt(L"Color", L"ConsoleBG", -1);
    g_set.consoleFg = ReadIniInt(L"Color", L"ConsoleFG", -1);

    g_set.historyMax = ReadIniInt(L"History", L"Max", 200);
    g_set.saveInternal = ReadIniBool(L"History", L"Internal", true);
    g_set.saveAlias    = ReadIniBool(L"History", L"Alias", true);
    g_set.saveWww      = ReadIniBool(L"History", L"WWW", true);
    g_set.saveEmail    = ReadIniBool(L"History", L"EMail", true);
    g_set.savePath     = ReadIniBool(L"History", L"Path", true);
    g_set.savePlugins  = ReadIniBool(L"History", L"Plugins", true);
    g_set.saveUnknown  = ReadIniBool(L"History", L"Unknown", true);
    g_set.useUndo    = ReadIniBool(L"Undo", L"UseUndo", true);
    g_set.enableSounds = ReadIniBool(L"Sounds", L"EnableSounds", true);
    for (int i = 0; i < 9; ++i) {
        std::wstring def = std::wstring(L"Sounds\\") +
            (i == 0 ? L"internal" : i == 1 ? L"alias" : i == 2 ? L"sysalias" :
             i == 3 ? L"url" : i == 4 ? L"email" : i == 5 ? L"file" :
             i == 6 ? L"folder" : i == 7 ? L"plugin" : L"shell") + L".wav";
        g_set.sound[i] = ReadIniStr(L"Sounds", kSoundKeys[i], def.c_str());
    }
    // [PluginState]: per-plugin "Name_Load" / "Name_SaveHistory" flags (0/1)
    {
        std::vector<wchar_t> buf(64 * 1024);
        DWORD n = GetPrivateProfileSectionW(L"PluginState", buf.data(), (DWORD)buf.size(), g_iniPath.c_str());
        g_pluginState.clear();
        for (wchar_t* q = buf.data(); q < buf.data() + n; ) {
            std::wstring ln(q);
            q += ln.size() + 1;
            if (ln.empty() || ln[0] == L';') continue;
            size_t eq = ln.find(L'=');
            if (eq == std::wstring::npos) continue;
            g_pluginState[Trim(ln.substr(0, eq))] = _wtoi(ln.substr(eq + 1).c_str());
        }
    }
    g_set.logEnabled = ReadIniBool(L"Log", L"Enabled", true);
    g_set.logFile    = ReadIniStr(L"Log", L"File", L"TypeBox.log");
    g_set.logMaxKB   = ReadIniInt(L"Log", L"MaxSizeKB", 512);
    g_set.logKeep    = ReadIniInt(L"Log", L"Keep", 3);
    std::wstring rw = Lower(ReadIniStr(L"Log", L"Rewrite", L"rotate"));
    g_set.logRewrite = rw == L"truncate" ? 1 : rw == L"timestamp" ? 2 :
                       rw == L"keep"     ? 3 : 0;

    g_hotkey.key    = (WORD)ReadIniInt(L"HotKey", L"Key", VK_F2);
    g_hotkey.alt    = ReadIniBool(L"HotKey", L"AltKey",   false);
    g_hotkey.ctrl   = ReadIniBool(L"HotKey", L"CtrlKey",  false);
    g_hotkey.shift  = ReadIniBool(L"HotKey", L"ShiftKey", false);
    g_hotkey.win    = ReadIniBool(L"HotKey", L"WinKey",   false);

    LoadEnvironment();
    ApplyEnvPath();                    // custom PATH takes effect immediately
    LoadSysAliases();
    LoadIgnores();
}

// Load aliases from [Aliases] of TypeBox.ini and, when present, from a
// legacy-style Config.ini next to the exe (pipe form:
//   alias = action|param|dir|state|priori|comment).
static void LoadAliases() {
    g_aliases.clear();

    auto loadSection = [&](const wchar_t* file, const wchar_t* section) {
        std::vector<wchar_t> buf(64 * 1024);
        DWORD n = GetPrivateProfileSectionW(section, buf.data(), (DWORD)buf.size(), file);
        for (wchar_t* p = buf.data(); p < buf.data() + n; ) {
            std::wstring line(p);
            p += line.size() + 1;
            if (line.empty() || line[0] == L';' || line[0] == L'[') continue;
            size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            Alias a;
            a.name = Trim(line.substr(0, eq));
            std::wstring value = Trim(line.substr(eq + 1));
            if (a.name.empty() || value.empty()) continue;
            if (value.find(L'|') != std::wstring::npos) { // legacy pipe form
                std::vector<std::wstring> f = SplitPipe(value);
                a.action = f.size() > 0 ? f[0] : L"";
                if (f.size() > 2 && !f[2].empty()) a.dir = ExpandEnv(f[2]);
                if (f.size() > 3 && !f[3].empty()) a.state = _wtoi(f[3].c_str());
                if (a.state < SW_HIDE || a.state > SW_SHOWMAXIMIZED) a.state = SW_SHOWNORMAL;
            } else {
                a.action = value;
            }
            if (!a.action.empty()) g_aliases.push_back(a);
        }
    };

    loadSection(g_iniPath.c_str(), L"Aliases");
    std::wstring legacy = ExeDir() + L"Config.ini";
    if (PathFileExistsW(legacy.c_str()))
        loadSection(legacy.c_str(), L"Aliases");
}

static const Alias* FindAlias(const std::wstring& word) {
    for (const auto& a : g_aliases)
        if (_wcsicmp(a.name.c_str(), word.c_str()) == 0) return &a;
    return nullptr;
}

// ------------------------------------------------- system aliases ([SysAliases])
//
// TypeAndRun had a system command table (~lock, ~logoff, ~cdeject ...).
// A user section overrides everything; otherwise the default table applies.

static void LoadSysAliases() {
    g_sysAliases.clear();
    std::vector<wchar_t> buf(64 * 1024);
    DWORD n = GetPrivateProfileSectionW(L"SysAliases", buf.data(), (DWORD)buf.size(), g_iniPath.c_str());
    for (wchar_t* p = buf.data(); p < buf.data() + n; ) {
        std::wstring line(p);
        p += line.size() + 1;
        if (line.empty() || line[0] == L';' || line[0] == L'[') continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        SysAlias sa;
        sa.name = Trim(line.substr(0, eq));
        sa.action = Trim(line.substr(eq + 1));
        if (!sa.name.empty() && !sa.action.empty()) g_sysAliases.push_back(sa);
    }
    if (g_sysAliases.empty()) { // TypeAndRun-style defaults
        const wchar_t* def[][2] = {
            { L"~lock",       L"rundll32.exe user32.dll,LockWorkStation" },
            { L"~logoff",     L"shutdown.exe /l" },
            { L"~sleep",      L" rundll32.exe powrprof.dll,SetSuspendState 0,1,0" },
            { L"~screensave", L"control.exe desk.cpl,,@screensaver" },
            { L"~reboot",     L"shutdown.exe /r /t 5" },
        };
        for (auto& d : def) g_sysAliases.push_back({ d[0], Trim(d[1]) });
    }
}

static const SysAlias* FindSysAlias(const std::wstring& word) {
    for (const auto& s : g_sysAliases)
        if (_wcsicmp(s.name.c_str(), word.c_str()) == 0) return &s;
    return nullptr;
}

// ------------------------------------------------------------- Ignores.ini
//
// TypeAndRun BadApp: do not pop the console over the listed applications.
// One pattern per line in the portable Ignores.ini (process file name or
// window class, wildcards * and ?, ';' starts a comment).

static bool WildMatch(const std::wstring& pat, const std::wstring& s) {
    size_t p = 0, i = 0, star = std::wstring::npos, mark = 0;
    while (i < s.size()) {
        if (p < pat.size() && (towlower(pat[p]) == towlower(s[i]) || pat[p] == L'?')) { ++p; ++i; }
        else if (p < pat.size() && pat[p] == L'*') { star = p++; mark = i; }
        else if (star != std::wstring::npos) { p = star + 1; i = ++mark; }
        else return false;
    }
    while (p < pat.size() && pat[p] == L'*') ++p;
    return p == pat.size();
}

static void LoadIgnores() {
    g_ignores.clear();
    std::ifstream f((ExeDir() + L"Ignores.ini").c_str(), std::ios::binary);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::wstring e = Trim(FromUtf8(line));
        if (e.empty() || e[0] == L';' || e[0] == L'#') continue;
        g_ignores.push_back(e);
    }
}

static bool AppIgnored() {
    if (g_ignores.empty()) return false;
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    std::wstring proc;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (h) {
            wchar_t buf[MAX_PATH] = {};
            DWORD n = MAX_PATH;
            if (QueryFullProcessImageNameW(h, 0, buf, &n))
                proc = PathFindFileNameW(buf);
            CloseHandle(h);
        }
    }
    wchar_t cls[128] = {};
    GetClassNameW(fg, cls, 128);
    for (const auto& pat : g_ignores) {
        if ((!proc.empty() && WildMatch(pat, proc)) || WildMatch(pat, cls))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------- history

static std::wstring HistoryFile() { return ExeDir() + L"History.dat"; }

static void LoadHistory() {
    g_history.clear();
    std::ifstream f(HistoryFile().c_str(), std::ios::binary);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        g_history.push_back(FromUtf8(line));
    }
    if ((int)g_history.size() > g_set.historyMax)
        g_history.erase(g_history.begin(),
                        g_history.end() - g_set.historyMax);
}

static void SaveHistory() {
    std::ofstream f(HistoryFile().c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return;
    for (const auto& h : g_history) f << ToUtf8(h) << '\n';
}

// ---------------------------------------------------------------- sounds

// The 9 per-execution-type sounds of TypeAndRun, in [Sounds] key order.

// Play the per-execution-type sound (TypeAndRun parity). Paths are absolute or
// relative to the portable TypeBox folder (bundled Sounds\).
static void PlayTypeSound(int idx) {
    if (!g_set.enableSounds || idx < 0 || idx > 8) return;
    std::wstring s = g_set.sound[idx];
    if (s.empty() || _wcsicmp(s.c_str(), L"none") == 0) return;
    std::wstring path = s;
    if (PathIsRelativeW(path.c_str())) path = ExeDir() + path;
    if (PathFileExistsW(path.c_str()))
        PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
}

// ---------------------------------------------------------------- tray

static void AddTrayIcon(HWND hWnd, bool balloon) {
    if (!g_set.trayIcon) return;
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hWnd;
    g_nid.uID              = kTrayId;
    g_nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = kTrayMsg;
    g_nid.hIcon            = LoadIconW(nullptr, IDI_APPLICATION);
    std::wstring tip = L"TypeBox " + WVersion();
    lstrcpynW(g_nid.szTip, tip.c_str(), ARRAYSIZE(g_nid.szTip));
    if (balloon && g_set.balloonTips) {
        g_nid.uFlags |= NIF_INFO;
        g_nid.dwInfoFlags = NIIF_INFO;
        lstrcpynW(g_nid.szInfoTitle, L"TypeBox", ARRAYSIZE(g_nid.szInfoTitle));
        lstrcpynW(g_nid.szInfo, L(STR_BALLOON_START), ARRAYSIZE(g_nid.szInfo));
    }
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void RemoveTrayIcon() {
    if (g_nid.hWnd) Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

// ---------------------------------------------------------------- plugins
//
// TypeAndRun-compatible native DLL ABI (see docs/PLUGIN_API.md):
//   Load(HWND) Unload GetInfo GetCount GetString GetCountET GetStringET RunString
//   GetPluginMenu/About/Options/FastConfig/HookKey — optional
// ANSI (CP_ACP) strings for 1:1 compatibility with the 2001-2004 plugins.

struct TInfo {
    int size;
    const char* name;
    const char* version;
    const char* description;
    const char* author;
    const char* copyright;
    const char* homepage;
};

struct TExec {
    int size;
    int run;
    const char* con_text;
    int con_sel_start;
    int con_sel_length;
};

typedef void      (__cdecl* PLoad)(HWND);
typedef void      (__cdecl* PUnload)();
typedef TInfo     (__cdecl* PGetInfo)();
typedef int       (__cdecl* PGetCount)();
typedef const char* (__cdecl* PGetString)(int);
typedef int       (__cdecl* PGetCountET)(const char*);
typedef const char* (__cdecl* PGetStringET)(int);
typedef TExec     (__cdecl* PRunString)(const char*);
typedef HMENU     (__cdecl* PGetPluginMenu)();
typedef void      (__cdecl* PAbout)();
typedef void      (__cdecl* POptions)();
typedef void      (__cdecl* PFastConfig)();
typedef int       (__cdecl* PHookKey)(HWND, WPARAM);

struct Plugin {
    HMODULE        hmod = nullptr;
    std::wstring   file;
    bool           loaded = false;   // Load(HWND) called
    bool           inHistory = false;
    PLoad          Load          = nullptr;
    PUnload        Unload        = nullptr;
    PGetInfo       GetInfo       = nullptr;
    PGetCount      GetCount      = nullptr;
    PGetString     GetString     = nullptr;
    PGetCountET    GetCountET    = nullptr;
    PGetStringET   GetStringET   = nullptr;
    PRunString     RunString     = nullptr;
    PGetPluginMenu GetPluginMenu = nullptr;
    PAbout         About         = nullptr;
    POptions       Options       = nullptr;
    PFastConfig    FastConfig    = nullptr;
    PHookKey       HookKey       = nullptr;
};

static std::vector<Plugin> g_plugins;
static std::wstring g_lastPluginName;             // plugin that handled the last line

static std::string ToAnsi(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring FromAnsi(const char* a) {
    if (!a) return L"";
    int n = MultiByteToWideChar(CP_ACP, 0, a, -1, nullptr, 0);
    std::wstring w(n ? n - 1 : 0, L'\0');
    MultiByteToWideChar(CP_ACP, 0, a, -1, &w[0], n);
    return w;
}

static FARPROC GetSym(HMODULE h, const char* name) {
    FARPROC p = GetProcAddress(h, name);
    if (!p) { // tolerate 32-bit cdecl "_Name" exports of legacy plugins
        char buf[260];
        wsprintfA(buf, "_%s", name);
        p = GetProcAddress(h, buf);
    }
    return p;
}

template <typename T>
static T GetProc(HMODULE h, const char* name) {
    return reinterpret_cast<T>(reinterpret_cast<uintptr_t>(GetSym(h, name)));
}

static void BindPlugin(Plugin& p) {
    p.Load          = GetProc<PLoad>(p.hmod, "Load");
    p.Unload        = GetProc<PUnload>(p.hmod, "Unload");
    p.GetInfo       = GetProc<PGetInfo>(p.hmod, "GetInfo");
    p.GetCount      = GetProc<PGetCount>(p.hmod, "GetCount");
    p.GetString     = GetProc<PGetString>(p.hmod, "GetString");
    p.GetCountET    = GetProc<PGetCountET>(p.hmod, "GetCountET");
    p.GetStringET   = GetProc<PGetStringET>(p.hmod, "GetStringET");
    p.RunString     = GetProc<PRunString>(p.hmod, "RunString");
    p.GetPluginMenu = GetProc<PGetPluginMenu>(p.hmod, "GetPluginMenu");
    p.About         = GetProc<PAbout>(p.hmod, "About");
    p.Options       = GetProc<POptions>(p.hmod, "Options");
    p.FastConfig    = GetProc<PFastConfig>(p.hmod, "FastConfig");
    p.HookKey       = GetProc<PHookKey>(p.hmod, "HookKey");
}

// Plugin identity = dll base name without extension (calc.dll -> "calc").
static std::wstring PluginKey(const Plugin& p) {
    std::wstring base = PathFindFileNameW(p.file.c_str());
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    return base;
}
static bool PluginStateOn(const std::wstring& key, bool def) {
    auto it = g_pluginState.find(key);
    return it == g_pluginState.end() ? def : it->second != 0;
}
static bool PluginLoadAllowed(const Plugin& p) {
    return PluginStateOn(PluginKey(p) + L"_Load", true);
}

// Scan the portable Plugins\ folder for *.dll. [Plugins] "Name"="path" entries
// and Plugins.ini (Path= / Load= / SaveHistory=) are honored too.
static void DiscoverPlugins() {
    for (auto& p : g_plugins)
        if (p.hmod && p.loaded && p.Unload) p.Unload();
    for (auto& p : g_plugins)
        if (p.hmod) FreeLibrary(p.hmod);
    g_plugins.clear();

    auto tryLoad = [&](const std::wstring& file) {
        HMODULE h = LoadLibraryW(file.c_str());
        if (!h) {
            LogError(L"plugin load failed: " + file +
                     L" (GetLastError=" + std::to_wstring(GetLastError()) + L")");
            return;
        }
        Plugin p;
        p.file = file;
        p.hmod = h;
        BindPlugin(p);
        if (!p.RunString && !p.GetInfo) {
            LogInfo(L"ignored non-plugin dll: " + file);
            FreeLibrary(h);
            return;
        }
        g_plugins.push_back(p);
    };

    WIN32_FIND_DATAW fd;
    std::wstring pattern = ExeDir() + L"Plugins\\*.dll";
    HANDLE hq = FindFirstFileW(pattern.c_str(), &fd);
    if (hq != INVALID_HANDLE_VALUE) {
        do { tryLoad(ExeDir() + L"Plugins\\" + fd.cFileName); } while (FindNextFileW(hq, &fd));
        FindClose(hq);
    }

    std::vector<wchar_t> buf(64 * 1024);
    DWORD n = GetPrivateProfileSectionW(L"Plugins", buf.data(), (DWORD)buf.size(), g_iniPath.c_str());
    for (wchar_t* q = buf.data(); q < buf.data() + n; ) {
        std::wstring line(q);
        q += line.size() + 1;
        if (line.empty() || line[0] == L';' || line[0] == L'[') continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring path = Trim(line.substr(eq + 1));
        if (path.empty()) continue;
        if (PathIsRelativeW(path.c_str())) path = ExeDir() + path;
        tryLoad(path);
    }
}

static void CallPluginsLoad(HWND mainWnd) {
    for (auto& p : g_plugins)
        if (p.Load && !p.loaded && PluginLoadAllowed(p)) { p.Load(mainWnd); p.loaded = true; }
}

static void CallPluginsUnload() {
    for (auto& p : g_plugins)
        if (p.Unload && p.loaded) { p.Unload(); p.loaded = false; }
}

// Ask plugins to handle the line (first plugin that returns run != 0 wins).
static ExecType RunPlugins(const std::wstring& line, HWND mainWnd) {
    std::string ansi = ToAnsi(line);
    for (auto& p : g_plugins) {
        if (!p.RunString || !PluginLoadAllowed(p)) continue;
        if (!p.loaded && p.Load) { p.Load(mainWnd); p.loaded = true; }
        TExec ex = p.RunString(ansi.c_str());
        if (ex.size == (int)sizeof(TExec) && ex.run) {
            std::wstring text = FromAnsi(ex.con_text);
            if (!text.empty()) {
                SetWindowTextW(g_edit, text.c_str());
                SendMessageW(g_edit, EM_SETSEL, ex.con_sel_start,
                             ex.con_sel_start + (ex.con_sel_length < 0 ? 0 : ex.con_sel_length));
            }
            g_lastPluginName = PluginKey(p);
            PlayTypeSound(7);
            return EXC_PLUGIN;
        }
    }
    return EXC_NONE;
}

// ---------------------------------------------------------------- forward decls

static void ShowBox(HWND mainWnd);
static void HideBox(HWND mainWnd);
static int  RunLine(const std::wstring& line, HWND mainWnd);
static void ReloadHotkey(HWND mainWnd);
static void ResetET();
static void OpenSettings(HWND mainWnd, int page);
enum { PG_APPEAR, PG_WINDOW, PG_FONT, PG_HOTKEY, PG_EXEC, PG_TYPE,
       PG_HISTORY, PG_LOG, PG_ALIASES, PG_SOUNDS, PG_PLUGINS, PG_ENV,
       PG_THEME, PG_LANG, PG_COUNT };

// ---------------------------------------------------------------- internal commands
//
// The 14 internal commands of TypeAndRun (Static.pas), kept 1:1 by name.

static void RunInternal(const std::wstring& line, HWND mainWnd) {
    std::wstring c = Lower(FirstWord(line));

    if (c == L"/help") {
        std::wstring msg =
            L"TypeBox " + WVersion() + L" — universal hotkey console + launcher.\n\n"
            L"Type an alias, app name, URL, e-mail, file or command and press Enter.\n"
            L"~commands run system actions (~lock ~logoff ~sleep ~screensave ~reboot).\n"
            L"Up/Down browse history, Esc hides, Ctrl+Z restores the last command.\n"
            L"Shift+Enter kills the process started by the last alias.\n\n";
        msg += std::wstring(L(STR_HELP_HEAD)) + L"\n"
            L"/about /checkconfig /config /exit /help /hide /homedir\n"
            L"/inidir /inifiles /load /reread /settings /show /unload\n"
            L"TypeBox additions: /export <file>  /import <file>  /undo  /log  /sys  /openconfig  /path\n"
            L"/settings opens the settings dialog, /config opens its alias editor.";
        MessageBoxW(mainWnd, msg.c_str(), L"TypeBox — help", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/about") {
        std::wstring msg =
            L"TypeBox " + WVersion() + L"\n\n" +
            std::wstring(L(STR_ABOUT_TEXT));
        MessageBoxW(mainWnd, msg.c_str(), L"About TypeBox", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/checkconfig") {
        bool iniOk = PathFileExistsW(g_iniPath.c_str());
        wchar_t msg[512];
        wsprintfW(msg,
            L"TypeBox.ini: %s\nAliases loaded: %d  SysAliases: %d\nIgnores (Ignores.ini): %d\n"
            L"History items: %d\nHotkey registered: %s",
            iniOk ? L"found" : L"MISSING (using defaults)",
            (int)g_aliases.size(), (int)g_sysAliases.size(), (int)g_ignores.size(),
            (int)g_history.size(),
            g_hotkey.key ? L"yes" : L"no");
        MessageBoxW(mainWnd, msg, L"TypeBox — check config", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/settings") {
        OpenSettings(mainWnd, PG_APPEAR);
    } else if (c == L"/config") {
        OpenSettings(mainWnd, PG_ALIASES);
    } else if (c == L"/openconfig") {
        ShellExecuteW(mainWnd, L"open", g_iniPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (c == L"/homedir" || c == L"/inidir") {
        ShellExecuteW(mainWnd, L"open", ExeDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (c == L"/inifiles") {
        std::wstring pattern = ExeDir() + L"*.ini";
        WIN32_FIND_DATAW fd;
        HANDLE hq = FindFirstFileW(pattern.c_str(), &fd);
        std::wstring list;
        if (hq != INVALID_HANDLE_VALUE) {
            do { list += fd.cFileName; list += L"\n"; } while (FindNextFileW(hq, &fd));
            FindClose(hq);
        }
        if (list.empty()) list = L"(no .ini files found)\n";
        MessageBoxW(mainWnd, list.c_str(), L"TypeBox — ini files", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/path") {
        // Show the own PATH folders and %variables% (OS Environment Variables style).
        std::wstring msg;
        for (auto& d : g_pathDirs) msg += d + L"\n";
        if (msg.empty()) msg = L"(no [Path] folders)\n";
        msg += L"\nVariables:\n";
        for (auto& v : g_envVars) msg += v.name + L" = " + v.value + L"\n";
        if (g_envVars.empty()) msg += L"(no [Variables])\n";
        MessageBoxW(mainWnd, msg.c_str(), L"TypeBox — own path & variables",
                    MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/reread") {
        LoadSettings();
        LoadAliases();
        ReloadHotkey(mainWnd);
    } else if (c == L"/load") {
        // (Re)scan Plugins\ and [Plugins]; call Load(HWND) on each.
        size_t before = g_plugins.size();
        CallPluginsUnload();
        DiscoverPlugins();
        CallPluginsLoad(mainWnd);
        std::wstring msg = L"Plugins loaded: " + std::to_wstring(g_plugins.size());
        if (g_plugins.empty() && before == 0)
            msg += L"\n(dlls in the portable Plugins\\ folder or [Plugins] entries are picked up)";
        MessageBoxW(mainWnd, msg.c_str(), L"TypeBox — plugins", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/unload") {
        CallPluginsUnload();
    } else if (c == L"/export") {
        // Export settings: copy the portable ini to the given file.
        std::wstring dest = RestWords(line);
        if (dest.empty()) {
            MessageBoxW(mainWnd, L"Usage: /export <path-to-file>",
                        L"TypeBox — export", MB_OK | MB_ICONINFORMATION);
        } else if (CopyFileW(g_iniPath.c_str(), dest.c_str(), FALSE)) {
            wchar_t msg[512];
            wsprintfW(msg, L"Settings exported to:\n%s", dest.c_str());
            MessageBoxW(mainWnd, msg, L"TypeBox — export", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(mainWnd, L"TypeBox: export failed (check the path).",
                        L"TypeBox — export", MB_OK | MB_ICONERROR);
            LogError(L"/export failed to: " + dest);
        }
    } else if (c == L"/import") {
        // Import settings: restore an exported file (current ini is kept as .bak).
        std::wstring src = RestWords(line);
        if (src.empty() || !PathFileExistsW(src.c_str())) {
            MessageBoxW(mainWnd, L"Usage: /import <path-to-file>\n(file not found)",
                        L"TypeBox — import", MB_OK | MB_ICONINFORMATION);
        } else {
            std::wstring bak = g_iniPath + L".bak";
            CopyFileW(g_iniPath.c_str(), bak.c_str(), FALSE);
            if (CopyFileW(src.c_str(), g_iniPath.c_str(), FALSE)) {
                LoadSettings();
                LoadAliases();
                ReloadHotkey(mainWnd);
                MessageBoxW(mainWnd,
                    L"Settings imported. Previous config kept as TypeBox.ini.bak",
                    L"TypeBox — import", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(mainWnd, L"TypeBox: import failed.",
                            L"TypeBox — import", MB_OK | MB_ICONERROR);
                LogError(L"/import failed from: " + src);
            }
        }
    } else if (c == L"/undo") {
        if (g_set.useUndo && !g_lastRun.empty()) {
            ShowBox(mainWnd);
            SetWindowTextW(g_edit, g_lastRun.c_str());
            SendMessageW(g_edit, EM_SETSEL, -1, 0);
        }
    } else if (c == L"/sys") {
        // Info display (PowerToys Run-style starter): RAM/CPU/uptime/battery to
        // the portable log + tray balloon — never blocks the console.
        MEMORYSTATUSEX ms = { sizeof(ms) };
        GlobalMemoryStatusEx(&ms);
        SYSTEM_POWER_STATUS ps = {};
        GetSystemPowerStatus(&ps);
        SYSTEM_INFO sysinf; GetSystemInfo(&sysinf);
        ULONGLONG upsec = GetTickCount64() / 1000;
        wchar_t bat[48] = L"battery: none";
        if (ps.BatteryFlag != 128)
            wsprintfW(bat, L"battery %u%%%s", (unsigned)ps.BatteryLifePercent,
                      (ps.BatteryFlag & 0x08) ? L" (charging)" : L"");
        wchar_t txt[512];
        wsprintfW(txt,
            L"RAM %u/%u MB free | CPU %u cores | uptime %u d %02u:%02u | %s",
            (unsigned)(ms.ullAvailPhys / 1048576), (unsigned)(ms.ullTotalPhys / 1048576),
            (unsigned)sysinf.dwNumberOfProcessors,
            (unsigned)(upsec / 86400), (unsigned)((upsec / 3600) % 24),
            (unsigned)((upsec / 60) % 60), bat);
        LogInfo(L"/sys: " + std::wstring(txt));
        if (g_nid.hWnd) {
            g_nid.uFlags = NIF_INFO;
            g_nid.dwInfoFlags = NIIF_INFO;
            lstrcpynW(g_nid.szInfoTitle, L"TypeBox — system", ARRAYSIZE(g_nid.szInfoTitle));
            lstrcpynW(g_nid.szInfo, txt, ARRAYSIZE(g_nid.szInfo));
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        }
    } else if (c == L"/log") {
        std::wstring p = LogPath();
        if (!p.empty() && PathFileExistsW(p.c_str()))
            ShellExecuteW(mainWnd, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        else
            MessageBoxW(mainWnd, L"Log is empty or not configured ([Log] File).",
                        L"TypeBox — log", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/hide") {
        HideBox(mainWnd);
    } else if (c == L"/show") {
        ShowBox(mainWnd);
    } else if (c == L"/exit") {
        DestroyWindow(mainWnd);
    }
}

// ---------------------------------------------------------------- execution
//
// Mirrors the TypeAndRun ParseConsole chain (Static.pas):
// internal → alias → URL → email → file/folder → shell, each toggleable in [Exec].

static bool HistoryAllows(ExecType t) {
    switch (t) {
    case EXC_INTERNAL: return g_set.saveInternal;
    case EXC_ALIAS:    return g_set.saveAlias;
    case EXC_SYSALIAS: return g_set.saveAlias;
    case EXC_URL:      return g_set.saveWww;
    case EXC_EMAIL:    return g_set.saveEmail;
    case EXC_FILE:
    case EXC_FOLDER:   return g_set.savePath;
    case EXC_PLUGIN:   return g_set.savePlugins &&
                                PluginStateOn(g_lastPluginName + L"_SaveHistory", true);
    default:           return g_set.saveUnknown;
    }
}

static int RunLine(const std::wstring& line, HWND mainWnd) {
    if (line.empty()) return EXC_NONE;

    if (line[0] == L'/' && g_set.execInternal) {
        PlayTypeSound(0);
        RunInternal(line, mainWnd);
        return EXC_INTERNAL;
    }

    std::wstring word = FirstWord(line);
    std::wstring rest = RestWords(line);

    // 1) alias from [Aliases] (TypeBox.ini or legacy Config.ini)
    if (g_set.execAliases) {
        if (const Alias* a = FindAlias(word)) {
            std::wstring cmd = ExpandEnv(Substitute(a->action, rest));
            std::wstring prog = FirstWord(cmd);
            std::wstring args = RestWords(cmd);
            // ShellExecuteEx with NOCLOSEPROCESS so Shift+Enter can kill the app;
            // FLAG_NO_UI: never pop the system "file not found" dialog — we log it.
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask       = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
            sei.hwnd        = mainWnd;
            sei.lpVerb      = L"open";
            sei.lpFile      = prog.c_str();
            sei.lpParameters = args.empty() ? nullptr : args.c_str();
            sei.lpDirectory = a->dir.empty() ? nullptr : a->dir.c_str();
            sei.nShow       = a->state;
            if (ShellExecuteExW(&sei)) {
                if (g_aliasProc) CloseHandle(g_aliasProc);
                g_aliasProc = sei.hProcess;   // may be null for shell verb relaunches
                g_aliasName = a->name;
                LogInfo(L"alias started: " + a->name + L" (handle " +
                        (sei.hProcess ? std::wstring(L"ok") : std::wstring(L"none")) + L")");
            } else {
                LogError(L"alias failed: " + a->name + L" -> " + cmd +
                         L" (GetLastError=" + std::to_wstring(GetLastError()) + L")");
            }
            PlayTypeSound(1);
            return EXC_ALIAS;
        }
    }

    // 1b) system alias (~lock, ~logoff, ...) — TypeAndRun system table
    if (g_set.execAliases && !word.empty() && word[0] == L'~') {
        if (const SysAlias* sa = FindSysAlias(word)) {
            std::wstring cmd = ExpandEnv(Substitute(sa->action, rest));
            std::wstring prog = FirstWord(cmd);
            std::wstring args = RestWords(cmd);
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
            sei.hwnd         = mainWnd;
            sei.lpVerb       = L"open";
            sei.lpFile       = prog.c_str();
            sei.lpParameters = args.empty() ? nullptr : args.c_str();
            sei.nShow        = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei)) {
                if (g_aliasProc) CloseHandle(g_aliasProc);
                g_aliasProc = sei.hProcess;
                g_aliasName = sa->name;
            } else {
                LogError(L"sysalias failed: " + sa->name + L" -> " + cmd +
                         L" (GetLastError=" + std::to_wstring(GetLastError()) + L")");
            }
            PlayTypeSound(2);
            return EXC_SYSALIAS;
        }
    }

    // 2) plugins (TypeAndRun ABI: RunString -> TExec)
    if (g_set.execPlugins) {
        ExecType pt = RunPlugins(line, mainWnd);
        if (pt != EXC_NONE) return pt;
    }

    // 3) URL
    std::wstring expanded = ExpandEnv(line);
    if (g_set.execWww && LooksLikeUrl(expanded)) {
        std::wstring url = expanded;
        if (_wcsnicmp(url.c_str(), L"www.", 4) == 0) url = L"http://" + url;
        ShellExecuteW(mainWnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        PlayTypeSound(3);
        return EXC_URL;
    }

    // 3) email
    if (g_set.execEmail && LooksLikeEmail(expanded)) {
        std::wstring mail = expanded;
        size_t sp;
        while ((sp = mail.find(L' ')) != std::wstring::npos) mail.replace(sp, 1, L"%20");
        ShellExecuteW(mainWnd, L"open", (L"mailto:" + mail).c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
        PlayTypeSound(4);
        return EXC_EMAIL;
    }

    // 4) file or folder
    if (g_set.execFiles) {
        DWORD attr = GetFileAttributesW(expanded.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            bool dir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
            ShellExecuteW(mainWnd, L"open", expanded.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            PlayTypeSound(dir ? 6 : 5);
            return dir ? EXC_FOLDER : EXC_FILE;
        }
    }

    // 4b) own PATH folders ([Path], OS-style): run a bare command name found
    //     in one of the user's folders. Direct launch keeps the process handle
    //     so Shift+Enter can kill it, and works even with [Shell] disabled.
    if (!word.empty() && !g_pathDirs.empty() &&
        word.find(L'\\') == std::wstring::npos && word.find(L'/') == std::wstring::npos) {
        static const wchar_t* kExts[] = { L".exe", L".bat", L".cmd", L".com" };
        for (auto& d : g_pathDirs) {
            for (auto ext : kExts) {
                std::wstring cand = d + L'\\' + word + ext;
                if (GetFileAttributesW(cand.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
                SHELLEXECUTEINFOW sei = { sizeof(sei) };
                sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
                sei.hwnd         = mainWnd;
                sei.lpVerb       = L"open";
                sei.lpFile       = cand.c_str();
                sei.lpParameters = rest.empty() ? nullptr : rest.c_str();
                sei.lpDirectory  = d.c_str();
                sei.nShow        = SW_SHOWNORMAL;
                if (ShellExecuteExW(&sei)) {
                    if (g_aliasProc) CloseHandle(g_aliasProc);
                    g_aliasProc = sei.hProcess;
                    g_aliasName = word;
                    LogInfo(L"path started: " + word + L" from " + d);
                } else {
                    LogError(L"path start failed: " + cand +
                             L" (GetLastError=" + std::to_wstring(GetLastError()) + L")");
                }
                PlayTypeSound(8);
                return EXC_SHELL;
            }
        }
    }

    // 5) shell mode ("bash-ish"): cmd.exe /c <line>
    if (g_set.execShell) {
        std::wstring shellCmd  = ReadIniStr(L"Shell", L"Command", L"cmd.exe");
        std::wstring shellArgs = ReadIniStr(L"Shell", L"Args", L"/c");
        ShellExecuteW(mainWnd, L"open", shellCmd.c_str(),
                      (shellArgs + L" " + expanded).c_str(), nullptr, SW_SHOWNORMAL);
        PlayTypeSound(8);
        return EXC_SHELL;
    }

    return EXC_NONE;
}

// ---------------------------------------------------------------- visual anchors
// A traffic light previews what Enter WILL do with the typed line:
//   green  — something you defined (alias, ~sysalias, known /command)
//   amber  — found outside (PATH program, file/folder, URL, mail, plugin)
//   red    — nothing is really known (shell catch-all, unknown, or the class
//            is switched off in [Exec])
// It re-classifies on every keystroke (EN_CHANGE) and is painted in the
// status strip next to three physical lamp dots.
static const wchar_t* const kInternalCmds[] = {
    L"/about", L"/checkconfig", L"/config", L"/exit", L"/export", L"/help",
    L"/hide", L"/homedir", L"/import", L"/inidir", L"/inifiles", L"/load",
    L"/log", L"/openconfig", L"/path", L"/reread", L"/settings", L"/show",
    L"/sys", L"/unload", L"/undo" };

struct Anchor { int lamp = -1; int labelId = -1; };   // -1 = lights off
static Anchor g_anchor;

static Anchor ClassifyAnchor(const std::wstring& lineRaw) {
    Anchor a;
    std::wstring line = Trim(lineRaw);
    if (line.empty()) return a;

    if (line[0] == L'/') {
        if (!g_set.execInternal) { a.lamp = 2; a.labelId = STR_AN_OFF; return a; }
        std::wstring w = FirstWord(line);
        for (auto c : kInternalCmds)
            if (_wcsicmp(w.c_str(), c) == 0) { a.lamp = 0; a.labelId = STR_SO_INTERNAL; return a; }
        a.lamp = 2; a.labelId = STR_AN_UNKNOWN; return a;
    }

    std::wstring word = FirstWord(line);
    if (g_set.execAliases) {
        if (word[0] != L'~' && FindAlias(word)) { a.lamp = 0; a.labelId = STR_SO_ALIAS; return a; }
        if (word[0] == L'~' && FindSysAlias(word)) { a.lamp = 0; a.labelId = STR_SO_SYS; return a; }
    }

    if (g_set.execPlugins)
        for (auto& p : g_plugins) {
            if (!p.GetCount || !p.GetString || !PluginLoadAllowed(p)) continue;
            for (int i = 0; i < p.GetCount(); ++i)
                if (_wcsicmp(word.c_str(), FromAnsi(p.GetString(i)).c_str()) == 0)
                    { a.lamp = 1; a.labelId = STR_SO_PLUGIN; return a; }
        }

    std::wstring expanded = ExpandEnv(line);
    if (LooksLikeUrl(expanded))
        { a.lamp = g_set.execWww   ? 1 : 2; a.labelId = g_set.execWww   ? STR_SO_URL  : STR_AN_OFF; return a; }
    if (LooksLikeEmail(expanded))
        { a.lamp = g_set.execEmail ? 1 : 2; a.labelId = g_set.execEmail ? STR_SO_MAIL : STR_AN_OFF; return a; }
    if (g_set.execFiles) {
        DWORD attr = GetFileAttributesW(expanded.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            a.lamp = 1;
            a.labelId = (attr & FILE_ATTRIBUTE_DIRECTORY) ? STR_SO_FOLDER : STR_SO_FILE;
            return a;
        }
    }
    if (!word.empty() && word.find(L'\\') == std::wstring::npos &&
        word.find(L'/') == std::wstring::npos) {
        static const wchar_t* kExts[] = { L".exe", L".bat", L".cmd", L".com" };
        for (auto& d : g_pathDirs)
            for (auto ext : kExts)
                if (GetFileAttributesW((d + L'\\' + word + ext).c_str())
                        != INVALID_FILE_ATTRIBUTES)
                    { a.lamp = 1; a.labelId = STR_AN_PATH; return a; }
    }
    if (g_set.execShell) { a.lamp = 2; a.labelId = STR_SO_SHELL; return a; }
    a.lamp = 2; a.labelId = STR_AN_UNKNOWN; return a;
}

// ---------------------------------------------------------------- UI

static void ApplyFont(HWND edit) {
    HFONT f = CreateFontW(-MulDiv(g_set.fontSize, 96, 72), 0, 0, 0,
        g_set.fontBold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN,
        ConsoleFontSuggestion(g_set.fontName.c_str()));
    SendMessageW(edit, WM_SETFONT, (WPARAM)f, TRUE);
}

static void SetTrayVisual();

static void ShowBox(HWND mainWnd) {
    RECT wr; GetWindowRect(GetDesktopWindow(), &wr);
    int x, y;
    if (g_set.x >= 0) x = g_set.x;
    else              x = (wr.right - g_set.width) / 2;
    if (g_set.y >= 0 && !g_set.centerY) y = g_set.y;
    else y = g_set.centerY ? (wr.bottom - g_set.height) / 3 : 100;
    if (!g_set.centerX) x = 0;

    SetWindowPos(mainWnd, HWND_TOPMOST, x, y, g_set.width, g_set.height, SWP_SHOWWINDOW);
    if (g_set.transparent < 255) {
        SetWindowLongW(mainWnd, GWL_EXSTYLE,
                       GetWindowLongW(mainWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(mainWnd, 0, (BYTE)g_set.transparent, LWA_ALPHA);
    }
    SetForegroundWindow(mainWnd);
    SetFocus(g_edit);
    if (g_set.applyEnglish) // [Font] ApplyEnglish: type commands in EN layout (TypeAndRun)
        ActivateKeyboardLayout((HKL)MAKELONG(0x409, 0x0409), 0);
    SendMessageW(g_edit, EM_SETSEL, 0, -1);
    g_visible = true;
    g_histPos = g_history.size();
    ResetET();
    g_anchor = Anchor{};             // lights off until the first keystroke
    SetTrayVisual();
    if (g_set.autoHide && g_set.autoHideValue > 0)
        SetTimer(mainWnd, 1, (UINT)g_set.autoHideValue * 1000, nullptr);
}

static void HideBox(HWND mainWnd) {
    ShowWindow(mainWnd, SW_HIDE);
    SetWindowTextW(g_edit, L"");
    g_visible = false;
    KillTimer(mainWnd, 1);
    ResetET();
    SetTrayVisual();
}

// ---------------------------------------------------------------- themes (cozy look)
// Seven warm presets paint the console, the status strip and the tray badge.
// Explicit [Color] ConsoleBG/ConsoleFG always win; the theme accent tints
// the tray rim and the status chips so tray and console match.
struct Theme { const wchar_t* name; COLORREF bg, fg, accent; };
static const Theme kThemes[] = {
    { L"cozy-dark",       RGB(0x2B, 0x26, 0x22), RGB(0xF5, 0xE9, 0xDC), RGB(0xFF, 0xB8, 0x6C) },
    { L"paper-light",     RGB(0xFB, 0xF5, 0xEA), RGB(0x3A, 0x33, 0x2B), RGB(0xC2, 0x66, 0x2D) },
    { L"midnight-blue",   RGB(0x1E, 0x24, 0x30), RGB(0xD8, 0xE2, 0xF0), RGB(0x7A, 0xA2, 0xF7) },
    { L"nord-frost",      RGB(0x2E, 0x34, 0x40), RGB(0xD8, 0xDE, 0xE9), RGB(0x88, 0xC0, 0xD0) },
    { L"solarized-dark",  RGB(0x00, 0x2B, 0x36), RGB(0x83, 0x94, 0x92), RGB(0xB5, 0x89, 0x00) },
    { L"solarized-light", RGB(0xFD, 0xF6, 0xE3), RGB(0x58, 0x6E, 0x75), RGB(0xB5, 0x89, 0x00) },
    { L"forest-night",    RGB(0x1F, 0x28, 0x23), RGB(0xDC, 0xE5, 0xDC), RGB(0xA3, 0xC9, 0xA8) },
};
static const int kThemeCount = (int)(sizeof(kThemes) / sizeof(kThemes[0]));

static int ThemeFromName(const std::wstring& name) {
    for (int i = 0; i < kThemeCount; ++i)
        if (_wcsicmp(kThemes[i].name, name.c_str()) == 0) return i;
    return 0;   // cozy-dark
}

static COLORREF MixColor(COLORREF a, COLORREF b, int pct) {   // pct% of b
    auto mix = [pct](int x, int y) { return x + (y - x) * pct / 100; };
    return RGB(mix(GetRValue(a), GetRValue(b)),
               mix(GetGValue(a), GetGValue(b)),
               mix(GetBValue(a), GetBValue(b)));
}

static COLORREF g_effBg  = RGB(0x2B, 0x26, 0x22);
static COLORREF g_effFg  = RGB(0xF5, 0xE9, 0xDC);
static COLORREF g_accent = RGB(0xFF, 0xB8, 0x6C);

// Cozy tray badge: charcoal circle, accent rim, ">_" glyph, state dot.
// Drawn with GDI into a 32bpp DIB; alpha is the circle mask we compute
// ourselves (GDI never writes alpha).
static HICON MakeCozyIcon(COLORREF card, COLORREF rim, COLORREF dot) {
    const int S = 32, C = 16, R = 15;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = S; bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP old = (HBITMAP)SelectObject(mem, dib);
    HPEN   pen  = CreatePen(PS_SOLID, 2, rim);
    HBRUSH br   = CreateSolidBrush(card);
    HPEN   oldPen = (HPEN)SelectObject(mem, pen);
    HBRUSH oldBr  = (HBRUSH)SelectObject(mem, br);
    Ellipse(mem, C - R, C - R, C + R, C + R);
    HFONT f = CreateFontW(-13, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                          FIXED_PITCH | FF_MODERN, L"Consolas");
    HFONT oldF = (HFONT)SelectObject(mem, f);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(0xF5, 0xE9, 0xDC));
    RECT tr = { C - R, C - R - 1, C + R, C + R + 2 };
    DrawTextW(mem, L">_", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (dot) {
        HBRUSH db = CreateSolidBrush(dot);
        HPEN   dp = CreatePen(PS_SOLID, 1, card);
        SelectObject(mem, dp); SelectObject(mem, db);
        Ellipse(mem, 20, 1, 31, 12);
        DeleteObject(db); DeleteObject(dp);
    }
    if (bits) {
        unsigned char* px = (unsigned char*)bits;
        for (int yy = 0; yy < S; ++yy)
            for (int xx = 0; xx < S; ++xx) {
                int dx = xx - C + 1, dy = yy - C;
                px[4 * (yy * S + xx) + 3] =
                    (dx * dx + dy * dy <= (R + 1) * (R + 1)) ? 0xFF : 0x00;
            }
    }
    SelectObject(mem, oldF); SelectObject(mem, oldBr); SelectObject(mem, oldPen);
    SelectObject(mem, old);
    DeleteDC(mem); ReleaseDC(nullptr, dc);
    DeleteObject(pen); DeleteObject(br); DeleteObject(f);
    ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmColor = dib;
    ii.hbmMask = CreateBitmap(S, S, 1, 1, nullptr);
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(ii.hbmMask); DeleteObject(dib);
    return ic;
}

static HICON g_icoIdle = nullptr, g_icoShown = nullptr, g_icoErr = nullptr;

static void EnsureTrayIcons() {
    if (g_icoIdle) return;
    const COLORREF card = RGB(0x2E, 0x2A, 0x27);
    g_icoIdle  = MakeCozyIcon(card, MixColor(card, g_accent, 45), 0);
    g_icoShown = MakeCozyIcon(card, g_accent, g_accent);
    g_icoErr   = MakeCozyIcon(card, g_accent, RGB(0xE0, 0x6C, 0x75));
    if (!g_icoIdle || !g_icoShown || !g_icoErr) {
        // never ship an invisible tray icon: fall back to the system one
        HICON def = LoadIconW(nullptr, IDI_APPLICATION);
        if (!g_icoIdle)  g_icoIdle  = CopyIcon(def);
        if (!g_icoShown) g_icoShown = CopyIcon(def);
        if (!g_icoErr)   g_icoErr   = CopyIcon(def);
    }
}
static void FreeTrayIcons() {
    if (g_icoIdle)  { DestroyIcon(g_icoIdle);  g_icoIdle  = nullptr; }
    if (g_icoShown) { DestroyIcon(g_icoShown); g_icoShown = nullptr; }
    if (g_icoErr)   { DestroyIcon(g_icoErr);   g_icoErr   = nullptr; }
}
static void SetTrayVisual() {
    if (!g_nid.hWnd) return;
    EnsureTrayIcons();
    g_nid.hIcon  = g_visible ? g_icoShown : (g_trayErr ? g_icoErr : g_icoIdle);
    g_nid.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// Effective console colors: explicit [Color] values win, else the theme.
static void ApplyThemeVisual() {
    const Theme& t = kThemes[ThemeFromName(g_set.theme)];
    g_effBg  = g_set.consoleBg >= 0 ? (COLORREF)g_set.consoleBg : t.bg;
    g_effFg  = g_set.consoleFg >= 0 ? (COLORREF)g_set.consoleFg : t.fg;
    g_accent = t.accent;
    if (g_bgBrush) { DeleteObject(g_bgBrush); g_bgBrush = nullptr; }
    g_bgBrush = CreateSolidBrush(g_effBg);
}

// ---------------------------------------------------------------- status strip
// A 20px cozy strip at the bottom of the drop-down: mini ">_", alias/plugin
// counts, EasyType + theme chips, sound / language / clock on the right.
static const int SB_H = 20;
static HFONT g_sbFont = nullptr;

static void PaintStatusBar(HDC dc, RECT rc) {
    rc.top = rc.bottom - SB_H;
    HBRUSH b = CreateSolidBrush(MixColor(g_effBg, RGB(0, 0, 0), 12));
    FillRect(dc, &rc, b);
    DeleteObject(b);
    HPEN   pen    = CreatePen(PS_SOLID, 1, MixColor(g_effBg, g_accent, 60));
    HPEN   oldPen = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, rc.left, rc.top, nullptr);
    LineTo(dc, rc.right, rc.top);
    SelectObject(dc, oldPen);
    DeleteObject(pen);

    HFONT oldF = (HFONT)SelectObject(dc, g_sbFont);
    SetBkMode(dc, TRANSPARENT);
    int y = rc.top + 3;

    // visual anchors: three physical lamps previewing what Enter will do
    static const COLORREF lit[3] = { RGB(0x98, 0xC9, 0x74),   // green: your own
                                     RGB(0xE0, 0xAF, 0x68),   // amber: found outside
                                     RGB(0xE0, 0x6C, 0x75) };  // red: unknown/shell
    int x = rc.left + 7;
    for (int i = 0; i < 3; ++i) {
        bool on = (g_anchor.lamp == i);
        HBRUSH b2 = CreateSolidBrush(on ? lit[i] : MixColor(g_effBg, lit[i], 16));
        HPEN   p2 = CreatePen(PS_SOLID, 1, MixColor(g_effBg, RGB(0xFF, 0xFF, 0xFF),
                                                    on ? 45 : 10));
        HPEN o2 = (HPEN)SelectObject(dc, p2);
        HBRUSH o3 = (HBRUSH)SelectObject(dc, b2);
        Ellipse(dc, x, rc.top + 6, x + 8, rc.top + 14);
        SelectObject(dc, o2); SelectObject(dc, o3);
        DeleteObject(b2); DeleteObject(p2);
        x += 12;
    }
    x += 4;
    if (g_anchor.lamp >= 0 && g_anchor.labelId >= 0 && g_anchor.lamp < 3) {
        const wchar_t* lbl = L(g_anchor.labelId);
        SIZE ls; GetTextExtentPoint32W(dc, lbl, (int)wcslen(lbl), &ls);
        SetTextColor(dc, MixColor(g_effBg, lit[g_anchor.lamp], 85));
        TextOutW(dc, x, y, lbl, (int)wcslen(lbl));
        x += ls.cx + 14;
    } else {
        x += 6;
    }

    SetTextColor(dc, g_accent);
    TextOutW(dc, x, y, L">_", 2);
    x += 17;
    size_t plg = 0, plgOn = 0;
    for (auto& p : g_plugins) { ++plg; if (p.loaded) ++plgOn; }
    std::wstring left = std::to_wstring(g_aliases.size()) + L" al   \u25CF" +
                        std::to_wstring(plgOn) + L"/" + std::to_wstring(plg);
    SetTextColor(dc, MixColor(g_effBg, g_effFg, 62));
    TextOutW(dc, x, y, left.c_str(), (int)left.size());
    SIZE sz; GetTextExtentPoint32W(dc, left.c_str(), (int)left.size(), &sz);
    x += sz.cx + 12;

    const wchar_t* mode = g_set.easyType == 0 ? L(STR_ET_OFF)
                        : g_set.easyType == 1 ? L(STR_ET_EASY) : L(STR_ET_LINUX);
    for (int pass = 0; pass < 2; ++pass) {
        const wchar_t* chip = pass == 0 ? mode : kThemes[ThemeFromName(g_set.theme)].name;
        SIZE cs; GetTextExtentPoint32W(dc, chip, (int)wcslen(chip), &cs);
        HBRUSH cb = CreateSolidBrush(MixColor(g_effBg, g_accent, 18));
        HPEN   cp = CreatePen(PS_SOLID, 1, MixColor(g_effBg, g_accent, 55));
        HPEN   o2 = (HPEN)SelectObject(dc, cp);
        HBRUSH o3 = (HBRUSH)SelectObject(dc, cb);
        RoundRect(dc, x, rc.top + 2, x + cs.cx + 10, rc.top + SB_H - 3, 7, 7);
        SelectObject(dc, o2); SelectObject(dc, o3);
        DeleteObject(cb); DeleteObject(cp);
        SetTextColor(dc, MixColor(g_effBg, g_accent, 85));
        TextOutW(dc, x + 5, y, chip, (int)wcslen(chip));
        x += cs.cx + 18;
    }

    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t clk[8]; wsprintfW(clk, L"%02d:%02d", (int)st.wHour, (int)st.wMinute);
    std::wstring right = std::wstring(g_set.enableSounds ? L"\u266A " : L"\u00D7 ") +
                         std::wstring(kLangs[g_langIdx].code) + L"  " + clk;
    SIZE rs; GetTextExtentPoint32W(dc, right.c_str(), (int)right.size(), &rs);
    SetTextColor(dc, MixColor(g_effBg, g_effFg, 62));
    TextOutW(dc, rc.right - rs.cx - 8, y, right.c_str(), (int)right.size());

    SelectObject(dc, oldF);
}

static void Submit(HWND mainWnd) {
    std::wstring line = GetEditText(g_edit);
    if (line.empty()) return;
    g_lastRun = line;
    ExecType t = (ExecType)RunLine(line, mainWnd);
    if (t != EXC_NONE && HistoryAllows(t)) {
        g_history.push_back(line);
        if ((int)g_history.size() > g_set.historyMax)
            g_history.erase(g_history.begin());
        SaveHistory();
    }
    SetWindowTextW(g_edit, L"");
    HideBox(mainWnd);
}

static void SetHistory(size_t idx) {
    if (idx < g_history.size()) SetWindowTextW(g_edit, g_history[idx].c_str());
    else                        SetWindowTextW(g_edit, L"");
    SendMessageW(g_edit, EM_SETSEL, -1, 0); // caret to end
    ResetET();
}

// ---------------------------------------------------------------- EasyType
//
// TypeAndRun's [Type] completion: Tab completes the word at the caret from
// internal commands, aliases and history; repeated Tab cycles the matches.

static void ResetET() {
    g_etList.clear();
    g_etPos = 0;
    g_etPrefix.clear();
}

static bool StartsWithNoCase(const std::wstring& s, const std::wstring& prefix) {
    if (s.size() < prefix.size()) return false;
    return _wcsnicmp(s.c_str(), prefix.c_str(), prefix.size()) == 0;
}

static void TabComplete(HWND edit) {
    if (g_set.easyType == 0) return;
    std::wstring text = GetEditText(edit);
    // GetEditText trims; use raw text for accurate caret mapping
    int len = GetWindowTextLengthW(edit);
    text.resize(len);
    GetWindowTextW(edit, &text[0], len + 1);

    DWORD sel = (DWORD)SendMessageW(edit, EM_GETSEL, 0, 0);
    size_t caret = LOWORD(sel);
    if (caret > text.size()) caret = text.size();

    size_t b = caret, e = caret;
    while (b > 0 && text[b-1] != L' ' && text[b-1] != L'\t') --b;
    while (e < text.size() && text[e] != L' ' && text[e] != L'\t') ++e;
    std::wstring word = text.substr(b, e - b);

    if (g_etList.empty() || !StartsWithNoCase(word, g_etPrefix)) {
        // (re)build the candidate list for this prefix
        ResetET();
        g_etPrefix = word;
        auto add = [&](const std::wstring& cand) {
            if (!StartsWithNoCase(cand, word) || cand.empty()) return;
            for (const auto& c : g_etList)
                if (_wcsicmp(c.c_str(), cand.c_str()) == 0) return;
            g_etList.push_back(cand);
        };
        bool firstWord = (b == 0);
        if (firstWord) {
            if (g_set.etInternal) {
                for (const wchar_t* cmd : { L"/about", L"/checkconfig", L"/config",
                     L"/exit", L"/export", L"/help", L"/hide", L"/homedir", L"/import",
                     L"/inidir", L"/inifiles", L"/load", L"/log", L"/openconfig", L"/path", L"/reread", L"/settings",
                     L"/show", L"/sys", L"/unload", L"/undo" })
                    add(cmd);
            }
            if (g_set.etAlias)
                for (const auto& a : g_aliases) add(a.name);
            if (g_set.etSysAlias)
                for (const auto& s : g_sysAliases) add(s.name);
            if (g_set.execPlugins)
                for (auto& p : g_plugins)
                    if (p.GetCount && p.GetString)
                        for (int i = 0; i < p.GetCount(); ++i)
                            add(FromAnsi(p.GetString(i)));
        }
        if (g_set.etHistory && word.find(L'/') != 0)
            for (size_t i = g_history.size(); i-- > 0; ) add(g_history[i]);
        if (g_set.execPlugins && word.find(L'/') != 0)
            for (auto& p : g_plugins)
                if (p.GetCountET && p.GetStringET) {
                    std::string a = ToAnsi(word);
                    int cnt = p.GetCountET(a.c_str());
                    for (int i = 0; i < cnt; ++i) add(FromAnsi(p.GetStringET(i)));
                }
        g_etPos = 0;
    }

    if (!g_etList.empty()) {
        std::wstring repl = g_etList[g_etPos % g_etList.size()];
        ++g_etPos;
        std::wstring nt = text.substr(0, b) + repl + text.substr(e);
        SetWindowTextW(edit, nt.c_str());
        SendMessageW(edit, EM_SETSEL, (WPARAM)(b + repl.size()), (LPARAM)(b + repl.size()));
    }
}

static void ShowTrayMenu(HWND hWnd) {
    POINT pt; GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L(STR_TRAY_SHOW));
    AppendMenuW(menu, MF_STRING, 4, L(STR_TRAY_SETTINGS));
    AppendMenuW(menu, MF_STRING, 2, L(STR_TRAY_CONFIG));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L(STR_TRAY_EXIT));
    SetForegroundWindow(hWnd); // so the menu dismisses on click-away
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(menu);
}

// =================================================================== settings dialog
//
// TypeAndRun-style settings window (frmSettingsUnit): left page list, controls
// bound to a scratch copy of the settings (Cancel discards), OK/Apply writes
// TypeBox.ini key-by-key (comments/unknown keys survive) and live-applies:
// font, geometry, alpha, hotkey re-registration, priority, tray, timer, brush.

static const int kPageStr[PG_COUNT] = {
    STR_PG_APPEARANCE, STR_PG_CONSOLE, STR_PG_FONT, STR_PG_HOTKEY, STR_PG_EXEC,
    STR_PG_EASYTYPE, STR_PG_HISTORY, STR_PG_LOG, STR_PG_ALIASES, STR_PG_SOUNDS,
    STR_PG_PLUGINS, STR_PG_ENV, STR_PG_THEME, STR_PG_LANG };

static HWND  g_setWnd = nullptr;
static HWND  g_setList = nullptr;
static HWND  g_setPage[PG_COUNT] = {};
static HFONT g_uiFont = nullptr;
static std::vector<HWND> g_setAll;
static std::vector<std::function<void()>> g_loaders, g_storers;

// scratch state
static Settings  g_draft;
static Hotkey    g_draftHk;
static std::vector<Alias> g_draftAliases;
static int       g_aliasSel = -1;

// controls that need direct access
static HWND g_hkEdit = nullptr, g_hkLabel = nullptr, g_hkVk = nullptr;
static HWND g_alLb = nullptr, g_alName = nullptr, g_alAction = nullptr,
      g_alDir = nullptr, g_alState = nullptr, g_fontPreview = nullptr;
static HWND g_plLb = nullptr, g_plLoadChk = nullptr, g_plHistChk = nullptr,
      g_plInfo = nullptr;
static HWND g_langCombo = nullptr;
static HWND g_themeCombo = nullptr, g_themePrev = nullptr;
struct PluginRow { std::wstring name; int load = 1, saveHist = 1; };
static std::vector<PluginRow> g_plRows;
static int g_plSel = -1;
static HWND g_envPathEdit = nullptr, g_varLb = nullptr,
      g_varName = nullptr, g_varValue = nullptr;
static std::vector<std::wstring> g_draftPath;   // raw folder lines
static std::vector<EnvVar>       g_draftVars;   // name/value/raw
static int g_varSel = -1;

static std::wstring HotkeyText(const Hotkey& hk) {
    std::wstring s;
    if (hk.ctrl)  s += L"Ctrl+";
    if (hk.alt)   s += L"Alt+";
    if (hk.shift) s += L"Shift+";
    if (hk.win)   s += L"Win+";
    wchar_t buf[32] = {};
    int c = MapVirtualKeyW(hk.key, MAPVK_VK_TO_CHAR);
    if (hk.key >= VK_F1 && hk.key <= VK_F24)
        wsprintfW(buf, L"F%u", hk.key - VK_F1 + 1);
    else if (c >= 0x20 && iswalnum((wchar_t)c))
        buf[0] = (wchar_t)towupper(c);
    else switch (hk.key) {
        case VK_SPACE: wcscpy(buf, L"Space"); break;
        case VK_UP:    wcscpy(buf, L"Up"); break;
        case VK_DOWN:  wcscpy(buf, L"Down"); break;
        case VK_LEFT:  wcscpy(buf, L"Left"); break;
        case VK_RIGHT: wcscpy(buf, L"Right"); break;
        case VK_INSERT:wcscpy(buf, L"Insert"); break;
        case VK_HOME:  wcscpy(buf, L"Home"); break;
        case VK_END:   wcscpy(buf, L"End"); break;
        case VK_NEXT:  wcscpy(buf, L"PgUp"); break;
        case VK_PRIOR: wcscpy(buf, L"PgDn"); break;
        default: wsprintfW(buf, L"VK%u", hk.key);
    }
    return s + buf;
}

static void HkRefresh() {
    std::wstring t = HotkeyText(g_draftHk);
    if (g_hkEdit)   SetWindowTextW(g_hkEdit, t.c_str());
    if (g_hkLabel)  SetWindowTextW(g_hkLabel,
        (std::wstring(L(STR_HK_CURRENT)) + t).c_str());
    if (g_hkVk) { wchar_t b[16]; wsprintfW(b, L"%u", g_draftHk.key); SetWindowTextW(g_hkVk, b); }
}

static LRESULT CALLBACK HkCapProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        WORD vk = (WORD)w;
        bool pure = vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU;
        g_draftHk.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        g_draftHk.shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
        g_draftHk.alt   = (GetKeyState(VK_MENU)    & 0x8000) != 0;
        g_draftHk.win   = (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                          (GetAsyncKeyState(VK_RWIN) & 0x8000);
        if (!pure) {
            if (vk == VK_ESCAPE) { HkRefresh(); return 0; }  // cancel capture
            g_draftHk.key = vk;
        }
        HkRefresh();
        return 0;
    }
    if (m == WM_KEYUP && (w == VK_SHIFT || w == VK_CONTROL || w == VK_MENU))
        return DefSubclassProc(h, m, w, l);
    if (m == WM_KEYUP) return 0;
    if (m == WM_CHAR)  return 0;
    return DefSubclassProc(h, m, w, l);
}

// ---------------------------------------------------------------- control makers
static HWND MkOn(HWND host, const wchar_t* cls, const wchar_t* text, DWORD style,
                 int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, host, nullptr,
                             (HINSTANCE)(LONG_PTR)GetWindowLongPtrW(host, GWLP_HINSTANCE), nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
    return c;
}
static HWND MkLabel(HWND host, const wchar_t* t, int x, int y, int w = 150) {
    return MkOn(host, L"STATIC", t, SS_LEFT, x, y, w, 18);
}
static HWND MkCheck(HWND host, const wchar_t* t, int x, int y, int w) {
    return MkOn(host, L"BUTTON", t, BS_AUTOCHECKBOX, x, y, w, 20);
}
static HWND MkEdit(HWND host, const wchar_t* t, int x, int y, int w, bool num = false) {
    return MkOn(host, L"EDIT", t, WS_BORDER | ES_AUTOHSCROLL | (num ? ES_NUMBER : 0),
                x, y, w, 22);
}
static HWND MkCombo(HWND host, int x, int y, int w, const wchar_t** items, int n) {
    HWND c = MkOn(host, L"COMBOBOX", L"", WS_VSCROLL | CBS_DROPDOWNLIST, x, y, w, 200);
    for (int i = 0; i < n; ++i) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)items[i]);
    SendMessageW(c, CB_SETCURSEL, 0, 0);
    return c;
}
static HWND MkButton(HWND host, const wchar_t* t, int x, int y, int w, UINT id = 0) {
    HWND c = MkOn(host, L"BUTTON", t, BS_PUSHBUTTON, x, y, w, 24);
    if (id) SetWindowLongPtrW(c, GWLP_ID, (LONG_PTR)id);   // language-independent dispatch
    return c;
}

// Dialog button ids (GWLP_ID) — dispatch must never depend on localized text.
enum { IDB_OK = 101, IDB_APPLY, IDB_CANCEL,
       IDB_AL_ADD, IDB_AL_UPDATE, IDB_AL_DELETE,
       IDB_FONT_CHOOSE, IDB_HI_CLEAR,
       IDB_PL_RESCAN, IDB_PL_UNLOAD, IDB_PL_ABOUT, IDB_PL_OPTIONS,
       IDB_EN_ADDVAR, IDB_EN_UPDATEVAR, IDB_EN_DELETEVAR };

static std::wstring GetText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    return s;
}
static void Bind(std::function<void()> load, std::function<void()> store) {
    g_loaders.push_back(load); g_storers.push_back(store);
}

// checkbox bound to a Settings bool member
static void CkB(HWND host, const wchar_t* label, int x, int y, bool Settings::*mem) {
    HWND c = MkCheck(host, label, x, y, 380);
    Bind([c, mem]{ SendMessageW(c, BM_SETCHECK, (g_draft.*mem) ? BST_CHECKED : BST_UNCHECKED, 0); },
         [c, mem]{ g_draft.*mem = SendMessageW(c, BM_GETCHECK, 0, 0) == BST_CHECKED; });
}
static void NumB(HWND host, const wchar_t* label, int x, int y, int Settings::*mem,
                 int minv, int maxv, bool allowNeg = false) {
    MkLabel(host, label, x, y + 2, 150);
    HWND e = MkEdit(host, L"", x + 155, y, 80, !allowNeg);
    Bind([e, mem]{ wchar_t b[24]; wsprintfW(b, L"%d", g_draft.*mem); SetWindowTextW(e, b); },
         [e, mem, minv, maxv]{
             int v = GetWindowTextLengthW(e) ? _wtoi(GetText(e).c_str()) : g_draft.*mem;
             if (v < minv) v = minv; if (v > maxv) v = maxv;
             g_draft.*mem = v;
         });
}
static void TxB(HWND host, const wchar_t* label, int x, int y, std::wstring Settings::*mem) {
    MkLabel(host, label, x, y + 2, 150);
    HWND e = MkEdit(host, L"", x + 155, y, 260);
    Bind([e, mem]{ SetWindowTextW(e, (g_draft.*mem).c_str()); },
         [e, mem]{ g_draft.*mem = Trim(GetText(e)); });
}

// ---------------------------------------------------------------- aliases page
static void AliasesFillList() {
    if (!g_alLb) return;
    SendMessageW(g_alLb, LB_RESETCONTENT, 0, 0);
    for (auto& a : g_draftAliases)
        SendMessageW(g_alLb, LB_ADDSTRING, 0, (LPARAM)a.name.c_str());
}
static void AliasesEditRow(int idx) {
    g_aliasSel = idx;
    if (idx < 0 || idx >= (int)g_draftAliases.size()) return;
    Alias& a = g_draftAliases[idx];
    SetWindowTextW(g_alName,   a.name.c_str());
    SetWindowTextW(g_alAction, a.action.c_str());
    SetWindowTextW(g_alDir,    a.dir.c_str());
    int sel = 0;
    if (a.state == SW_SHOWMAXIMIZED) sel = 1;
    else if (a.state == SW_SHOWMINIMIZED) sel = 2;
    else if (a.state == SW_HIDE) sel = 3;
    SendMessageW(g_alState, CB_SETCURSEL, sel, 0);
}
static void AliasesReadRow(int idx) {
    if (idx < 0 || idx >= (int)g_draftAliases.size()) return;
    Alias a;
    a.name   = Trim(GetText(g_alName));
    a.action = Trim(GetText(g_alAction));
    a.dir    = Trim(GetText(g_alDir));
    int sel = (int)SendMessageW(g_alState, CB_GETCURSEL, 0, 0);
    a.state = sel == 1 ? SW_SHOWMAXIMIZED : sel == 2 ? SW_SHOWMINIMIZED
            : sel == 3 ? SW_HIDE : SW_SHOWNORMAL;
    if (a.name.empty() || a.action.empty()) return;
    g_draftAliases[idx] = a;
    AliasesFillList();
    SendMessageW(g_alLb, LB_SETCURSEL, idx, 0);
}
// ---------------------------------------------------------------- plugins page
static void PluginsFillList() {
    if (!g_plLb) return;
    SendMessageW(g_plLb, LB_RESETCONTENT, 0, 0);
    for (auto& r : g_plRows)
        SendMessageW(g_plLb, LB_ADDSTRING, 0, (LPARAM)r.name.c_str());
    if (g_plSel >= 0 && g_plSel < (int)g_plRows.size())
        SendMessageW(g_plLb, LB_SETCURSEL, g_plSel, 0);
}
static void PluginsEditRow(int idx) {
    g_plSel = idx;
    if (idx < 0 || idx >= (int)g_plRows.size()) return;
    PluginRow& r = g_plRows[idx];
    SendMessageW(g_plLoadChk, BM_SETCHECK, r.load ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_plHistChk, BM_SETCHECK, r.saveHist ? BST_CHECKED : BST_UNCHECKED, 0);
    std::wstring info;
    for (auto& p : g_plugins)
        if (PluginKey(p) == r.name && p.GetInfo) {
            TInfo ti = p.GetInfo();
            auto add = [&info](const wchar_t* label, const char* v) {
                if (v && *v) info += std::wstring(label) + L": " + FromAnsi(v) + L"\n";
            };
            add(L"Name", ti.name); add(L"Version", ti.version);
            add(L"Description", ti.description); add(L"Author", ti.author);
            add(L"Copyright", ti.copyright); add(L"Homepage", ti.homepage);
            info += PluginLoadAllowed(p) ? L"Status: loaded" : L"Status: disabled";
            break;
        }
    if (info.empty()) info = L"(plugin not present on disk)";
    SetWindowTextW(g_plInfo, info.c_str());
}
static void PluginsReadRow() {
    if (g_plSel < 0 || g_plSel >= (int)g_plRows.size()) return;
    PluginRow& r = g_plRows[g_plSel];
    r.load     = SendMessageW(g_plLoadChk, BM_GETCHECK, 0, 0) == BST_CHECKED;
    r.saveHist = SendMessageW(g_plHistChk, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

// ---------------------------------------------------------------- environment page
static void VarsFillList() {
    if (!g_varLb) return;
    SendMessageW(g_varLb, LB_RESETCONTENT, 0, 0);
    for (auto& v : g_draftVars) {
        std::wstring line = v.name + L" = " + v.raw;
        SendMessageW(g_varLb, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
    if (g_varSel >= 0 && g_varSel < (int)g_draftVars.size())
        SendMessageW(g_varLb, LB_SETCURSEL, g_varSel, 0);
}
static void VarsEditRow(int idx) {
    g_varSel = idx;
    if (idx < 0 || idx >= (int)g_draftVars.size()) return;
    SetWindowTextW(g_varName,  g_draftVars[idx].name.c_str());
    SetWindowTextW(g_varValue, g_draftVars[idx].raw.c_str());
}
static bool VarsReadInto(EnvVar& v) {
    v.name = Trim(GetText(g_varName));
    v.raw  = Trim(GetText(g_varValue));
    v.value = v.raw;                       // expanded again on next load
    return !v.name.empty() && !v.raw.empty() && v.name.find(L'%') == std::wstring::npos;
}
static void VarsUpdate() {
    if (g_varSel < 0 || g_varSel >= (int)g_draftVars.size()) return;
    EnvVar v;
    if (!VarsReadInto(v)) return;
    g_draftVars[g_varSel] = v;
    VarsFillList();
    SendMessageW(g_varLb, LB_SETCURSEL, g_varSel, 0);
}
static void VarsAdd() {
    EnvVar v;
    if (!VarsReadInto(v)) return;
    for (auto& e : g_draftVars)
        if (_wcsicmp(e.name.c_str(), v.name.c_str()) == 0) return;   // duplicate
    g_draftVars.push_back(v);
    g_varSel = (int)g_draftVars.size() - 1;
    VarsFillList();
    SendMessageW(g_varLb, LB_SETCURSEL, g_varSel, 0);
}

static void AliasesAdd() {
    size_t before = g_draftAliases.size();
    g_draftAliases.push_back({});
    AliasesReadRow((int)before);
    if (g_draftAliases.size() == before) g_draftAliases.pop_back(); // rejected
    g_aliasSel = (int)before;
    AliasesFillList();
    SendMessageW(g_alLb, LB_SETCURSEL, g_aliasSel, 0);
}

// ---------------------------------------------------------------- build dialog
static void UiLoad() {
    for (auto& f : g_loaders) f();
    HkRefresh();
}
static void UiCollect() {
    for (auto& f : g_storers) f();
}

static void WriteDraftIni();
static void LiveApply(HWND mainWnd);

static void SettingsShowPage(int p) {
    for (int i = 0; i < PG_COUNT; ++i)
        ShowWindow(g_setPage[i], i == p ? SW_SHOW : SW_HIDE);
}

static LRESULT CALLBACK SettingsProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)l;
        if (g_themePrev && di->hwndItem == g_themePrev) {   // theme preview card
            const Theme& t = kThemes[ThemeFromName(g_draft.theme)];
            HBRUSH b = CreateSolidBrush(t.bg);
            FillRect(di->hDC, &di->rcItem, b);
            DeleteObject(b);
            RECT bar = di->rcItem;
            bar.bottom = bar.top + 4;
            HBRUSH ab = CreateSolidBrush(t.accent);
            FillRect(di->hDC, &bar, ab);
            FrameRect(di->hDC, &di->rcItem, ab);
            DeleteObject(ab);
            HFONT f = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                  OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  FIXED_PITCH | FF_MODERN, L"Consolas");
            HFONT oldF = (HFONT)SelectObject(di->hDC, f);
            SetBkMode(di->hDC, TRANSPARENT);
            int tx = di->rcItem.left + 12, ty = di->rcItem.top + 24;
            SetTextColor(di->hDC, t.accent);
            TextOutW(di->hDC, tx, ty, L">_", 2);
            std::wstring s = std::wstring(L" TypeBox \u2014 ") + t.name;
            SetTextColor(di->hDC, t.fg);
            TextOutW(di->hDC, tx + 22, ty, s.c_str(), (int)s.size());
            SelectObject(di->hDC, oldF);
            DeleteObject(f);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        HWND src = (HWND)l;
        int code = HIWORD(w);
        if (src == g_setList && code == LBN_SELCHANGE) {
            SettingsShowPage((int)SendMessageW(src, LB_GETCURSEL, 0, 0));
            return 0;
        }
        if (code == BN_CLICKED) {
            if (src == g_alLb) return 0;
            if (src) {
                int bid = (int)GetWindowLongW(src, GWLP_ID);   // locale-proof dispatch
                if (bid == IDB_OK || bid == IDB_APPLY) {
                    UiCollect();
                    WriteDraftIni();
                    g_set = g_draft; g_hotkey = g_draftHk; g_aliases = g_draftAliases;
                    LiveApply((HWND)GetWindowLongPtrW(h, GWLP_HWNDPARENT));
                    if (bid == IDB_OK) DestroyWindow(h);
                    else UiLoad();
                    return 0;
                }
                if (bid == IDB_CANCEL) { DestroyWindow(h); return 0; }
                if (bid == IDB_AL_ADD)     { AliasesAdd(); return 0; }
                if (bid == IDB_AL_UPDATE)  { AliasesReadRow(g_aliasSel); return 0; }
                if (bid == IDB_AL_DELETE) {
                    if (g_aliasSel >= 0 && g_aliasSel < (int)g_draftAliases.size()) {
                        g_draftAliases.erase(g_draftAliases.begin() + g_aliasSel);
                        g_aliasSel = -1;
                        AliasesFillList();
                    }
                    return 0;
                }
                if (bid == IDB_FONT_CHOOSE) {
                    CHOOSEFONTW cf = { sizeof(cf) };
                    LOGFONTW lf = {};
                    lstrcpynW(lf.lfFaceName, g_draft.fontName.c_str(), LF_FACESIZE);
                    lf.lfHeight = -MulDiv(g_draft.fontSize, 96, 72);
                    lf.lfWeight = g_draft.fontBold ? FW_BOLD : FW_NORMAL;
                    cf.lpLogFont = &lf; cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
                    cf.hwndOwner = h;
                    if (ChooseFontW(&cf)) {
                        g_draft.fontName = lf.lfFaceName;
                        g_draft.fontSize = MulDiv(-lf.lfHeight, 72, 96);
                        if (g_draft.fontSize < 6) g_draft.fontSize = 6;
                        if (g_draft.fontSize > 72) g_draft.fontSize = 72;
                        g_draft.fontBold = lf.lfWeight >= FW_BOLD;
                        UiLoad();
                    }
                    return 0;
                }
                if (bid == IDB_EN_ADDVAR)     { VarsAdd();    return 0; }
                if (bid == IDB_EN_UPDATEVAR)  { VarsUpdate(); return 0; }
                if (bid == IDB_EN_DELETEVAR) {
                    if (g_varSel >= 0 && g_varSel < (int)g_draftVars.size()) {
                        g_draftVars.erase(g_draftVars.begin() + g_varSel);
                        g_varSel = -1;
                        VarsFillList();
                    }
                    return 0;
                }
                if (bid == IDB_HI_CLEAR) {
                    g_history.clear();
                    SaveHistory();
                    LogInfo(L"settings: history cleared");
                    return 0;
                }
                HWND parent = (HWND)GetWindowLongPtrW(h, GWLP_HWNDPARENT);
                if (bid == IDB_PL_RESCAN) {
                    PluginsReadRow();
                    CallPluginsUnload();
                    DiscoverPlugins();
                    CallPluginsLoad(parent);
                    g_plRows.clear();
                    for (auto& p : g_plugins) {
                        PluginRow r;
                        r.name     = PluginKey(p);
                        r.load     = PluginStateOn(r.name + L"_Load", true) ? 1 : 0;
                        r.saveHist = PluginStateOn(r.name + L"_SaveHistory", true) ? 1 : 0;
                        g_plRows.push_back(r);
                    }
                    g_plSel = 0;
                    PluginsFillList();
                    PluginsEditRow(0);
                    LogInfo(L"settings: plugins rescanned, " +
                            std::to_wstring(g_plugins.size()) + L" found");
                    return 0;
                }
                if (bid == IDB_PL_UNLOAD) {
                    PluginsReadRow();
                    CallPluginsUnload();
                    LogInfo(L"settings: all plugins unloaded");
                    return 0;
                }
                if (bid == IDB_PL_ABOUT || bid == IDB_PL_OPTIONS) {
                    PluginsReadRow();
                    if (g_plSel >= 0 && g_plSel < (int)g_plRows.size()) {
                        for (auto& p : g_plugins)
                            if (PluginKey(p) == g_plRows[g_plSel].name) {
                                if (bid == IDB_PL_ABOUT) {
                                    if (p.About) p.About();
                                    else LogInfo(L"plugin has no About: " + g_plRows[g_plSel].name);
                                } else {
                                    if (p.Options) p.Options();
                                    else LogInfo(L"plugin has no Options: " + g_plRows[g_plSel].name);
                                }
                                break;
                            }
                    }
                    return 0;
                }
            }
        }
        if (code == LBN_SELCHANGE && src == g_alLb) {
            int s = (int)SendMessageW(src, LB_GETCURSEL, 0, 0);
            if (s >= 0) AliasesEditRow(s);
            return 0;
        }
        if (code == LBN_SELCHANGE && src == g_plLb) {
            PluginsReadRow();
            int s = (int)SendMessageW(src, LB_GETCURSEL, 0, 0);
            if (s >= 0) PluginsEditRow(s);
            return 0;
        }
        if (code == LBN_SELCHANGE && src == g_varLb) {
            int s = (int)SendMessageW(src, LB_GETCURSEL, 0, 0);
            if (s >= 0) VarsEditRow(s);
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        for (HWND c : g_setAll) DestroyWindow(c);
        g_setAll.clear();
        g_loaders.clear(); g_storers.clear();
        if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = nullptr; }
        g_setWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// Page windows are DefWindowProc containers; forward their child notifications
// (BN_CLICKED, LBN_SELCHANGE, EN_CHANGE...) to the settings window proc.
static LRESULT CALLBACK PageProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_COMMAND || m == WM_DRAWITEM)   // owner-draw controls live on the pages
        return SendMessageW(GetParent(h), m, w, l);
    return DefSubclassProc(h, m, w, l);
}

static void SettingsCreate(HWND mainWnd, int startPage) {
    HINSTANCE hInst = (HINSTANCE)(LONG_PTR)GetWindowLongPtrW(mainWnd, GWLP_HINSTANCE);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TypeBoxSettings";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);
    wc.lpszClassName = L"TypeBoxPage";
    wc.lpfnWndProc = DefWindowProcW;
    RegisterClassW(&wc);

    g_draft = g_set; g_draftHk = g_hotkey; g_draftAliases = g_aliases;
    g_aliasSel = 0;

    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g_uiFont = CreateFontIndirectW(&ncm.lfMessageFont);

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int W = 640, H = 460;
    int sx = GetPrivateProfileIntW(L"Coordinates", L"SettingsX", -1, g_iniPath.c_str());
    int sy = GetPrivateProfileIntW(L"Coordinates", L"SettingsY", -1, g_iniPath.c_str());
    if (sx < 0) sx = (wa.right - W) / 2;
    if (sy < 0) sy = (wa.bottom - H) / 2;

    g_setWnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"TypeBoxSettings", (std::wstring(L(STR_DLG_TITLE)) + L" — " + WVersion()).c_str(),
        WS_CAPTION | WS_SYSMENU, sx, sy, W, H, mainWnd, nullptr, hInst, nullptr);

    g_setList = MkOn(g_setWnd, L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                     8, 8, 140, 360);
    for (int i = 0; i < PG_COUNT; ++i)
        SendMessageW(g_setList, LB_ADDSTRING, 0, (LPARAM)L(kPageStr[i]));
    SendMessageW(g_setList, LB_SETCURSEL, startPage, 0);

    for (int i = 0; i < PG_COUNT; ++i) {
        g_setPage[i] = CreateWindowExW(0, L"TypeBoxPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 156, 8, 470, 360, g_setWnd, nullptr, hInst, nullptr);
        SendMessageW(g_setPage[i], WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        SetWindowSubclass(g_setPage[i], PageProc, 6, 0);
    }
    g_setAll.push_back(g_setList);
    for (int i = 0; i < PG_COUNT; ++i) g_setAll.push_back(g_setPage[i]);

    int y;
    // --- Appearance
    y = 12;
    CkB(g_setPage[PG_APPEAR], L(STR_AP_START),  16, y, &Settings::showOnStart);        y += 26;
    CkB(g_setPage[PG_APPEAR], L(STR_AP_HIDEFOCUS), 16, y, &Settings::hideOnDeactivate); y += 26;
    CkB(g_setPage[PG_APPEAR], L(STR_AP_TRAY),   16, y, &Settings::trayIcon);           y += 26;
    CkB(g_setPage[PG_APPEAR], L(STR_AP_BALLOONS), 16, y, &Settings::balloonTips);      y += 26;
    CkB(g_setPage[PG_APPEAR], L(STR_AP_AUTOHIDE), 16, y, &Settings::autoHide);         y += 30;
    NumB(g_setPage[PG_APPEAR], L(STR_AP_IDLESEC), 16, y, &Settings::autoHideValue, 1, 3600); y += 32;
    MkLabel(g_setPage[PG_APPEAR], L(STR_AP_PRIORITY), 16, y + 2);
    {
        const wchar_t* it[] = { L"idle", L"normal", L"high", L"realtime" };
        HWND c = MkCombo(g_setPage[PG_APPEAR], 175, y, 110, it, 4);
        Bind([c]{ int s = 0;
                  if (g_draft.priority == L"high") s = 2;
                  else if (g_draft.priority == L"realtime") s = 3;
                  else if (g_draft.priority == L"idle") s = 0;
                  else s = 1;
                  SendMessageW(c, CB_SETCURSEL, s, 0); },
             [c]{ int s = (int)SendMessageW(c, CB_GETCURSEL, 0, 0);
                  g_draft.priority = s == 3 ? L"realtime" : s == 2 ? L"high"
                                 : s == 0 ? L"idle" : L"normal"; });
    }

    // --- Console
    y = 12;
    NumB(g_setPage[PG_WINDOW], L(STR_CO_WIDTH),  16, y, &Settings::width, 200, 7680);  y += 28;
    NumB(g_setPage[PG_WINDOW], L(STR_CO_HEIGHT), 16, y, &Settings::height, 24, 2160);  y += 28;
    NumB(g_setPage[PG_WINDOW], L(STR_CO_X), 16, y, &Settings::x, -1, 7680, true);      y += 28;
    NumB(g_setPage[PG_WINDOW], L(STR_CO_Y), 16, y, &Settings::y, -1, 2160, true);      y += 28;
    CkB(g_setPage[PG_WINDOW], L(STR_CO_CX), 16, y, &Settings::centerX);                y += 26;
    CkB(g_setPage[PG_WINDOW], L(STR_CO_CY),   16, y, &Settings::centerY);              y += 30;
    NumB(g_setPage[PG_WINDOW], L(STR_CO_ALPHA), 16, y, &Settings::transparent, 30, 255); y += 32;
    NumB(g_setPage[PG_WINDOW], L(STR_CO_BG), 16, y, &Settings::consoleBg, -1, 0x00FFFFFF, true); y += 28;
    NumB(g_setPage[PG_WINDOW], L(STR_CO_FG), 16, y, &Settings::consoleFg, -1, 0x00FFFFFF, true);

    // --- Font
    y = 12;
    MkLabel(g_setPage[PG_FONT], L"Preview", 16, y, 430);
    g_fontPreview = MkOn(g_setPage[PG_FONT], L"STATIC", L"Aa Bb Cc 0123456789",
                         SS_LEFT, 16, y + 20, 430, 40);
    y += 70;
    TxB(g_setPage[PG_FONT], L(STR_FO_NAME), 16, y, &Settings::fontName);                y += 28;
    NumB(g_setPage[PG_FONT], L(STR_FO_SIZE), 16, y, &Settings::fontSize, 6, 72);        y += 28;
    CkB(g_setPage[PG_FONT], L(STR_FO_BOLD), 16, y, &Settings::fontBold);                y += 26;
    CkB(g_setPage[PG_FONT], L(STR_FO_APPLY_EN), 16, y, &Settings::applyEnglish);        y += 30;
    MkButton(g_setPage[PG_FONT], L(STR_FO_CHOOSE), 16, y, 110, IDB_FONT_CHOOSE);
    Bind([]{
        static HFONT prev = nullptr;
        if (prev) { DeleteObject(prev); prev = nullptr; }
        HFONT pf = CreateFontW(-MulDiv(g_draft.fontSize, 96, 72), 0, 0, 0,
            g_draft.fontBold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
            VARIABLE_PITCH, g_draft.fontName.c_str());
        prev = pf;
        SendMessageW(g_fontPreview, WM_SETFONT, (WPARAM)pf, TRUE); }, []{});

    // --- Hotkey
    y = 12;
    MkLabel(g_setPage[PG_HOTKEY], L(STR_HK_HINT), 16, y, 430); y += 24;
    g_hkEdit = MkEdit(g_setPage[PG_HOTKEY], L"", 16, y, 300);
    SetWindowSubclass(g_hkEdit, HkCapProc, 777, 0);
    SetWindowLongW(g_hkEdit, GWL_STYLE, GetWindowLongW(g_hkEdit, GWL_STYLE) | ES_READONLY);
    y += 32;
    g_hkLabel = MkLabel(g_setPage[PG_HOTKEY], L(STR_HK_CURRENT), 16, y, 430); y += 30;
    MkLabel(g_setPage[PG_HOTKEY], L(STR_HK_VK), 16, y + 2, 120);
    g_hkVk = MkEdit(g_setPage[PG_HOTKEY], L"", 140, y, 70, true);
    Bind([]{ wchar_t b[16]; wsprintfW(b, L"%u", g_draftHk.key);
             SetWindowTextW(g_hkVk, b); }, []{});

    // --- Execution
    y = 12;
    CkB(g_setPage[PG_EXEC], L(STR_EX_INTERNAL),   16, y, &Settings::execInternal); y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_EX_ALIASES),    16, y, &Settings::execAliases);  y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_EX_WWW),        16, y, &Settings::execWww);     y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_EX_EMAIL),      16, y, &Settings::execEmail);   y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_EX_FILES),      16, y, &Settings::execFiles);   y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_EX_SHELL),      16, y, &Settings::execShell);   y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_EX_PLUGINS),    16, y, &Settings::execPlugins);

    // --- EasyType
    y = 12;
    MkLabel(g_setPage[PG_TYPE], L(STR_ET_MODE), 16, y + 2);
    {
        const wchar_t* it[] = { L(STR_ET_OFF), L(STR_ET_EASY), L(STR_ET_LINUX) };
        HWND c = MkCombo(g_setPage[PG_TYPE], 175, y, 180, it, 3);
        Bind([c]{ SendMessageW(c, CB_SETCURSEL,
                 g_draft.easyType == 0 ? 0 : g_draft.easyType == 1 ? 1 : 2, 0); },
             [c]{ g_draft.easyType = (int)SendMessageW(c, CB_GETCURSEL, 0, 0); });
    }
    y += 34;
    CkB(g_setPage[PG_TYPE], L(STR_ET_INT),     16, y, &Settings::etInternal); y += 26;
    CkB(g_setPage[PG_TYPE], L(STR_ET_AL),      16, y, &Settings::etAlias);    y += 26;
    CkB(g_setPage[PG_TYPE], L(STR_ET_SYS),     16, y, &Settings::etSysAlias); y += 26;
    CkB(g_setPage[PG_TYPE], L(STR_ET_HIST),    16, y, &Settings::etHistory);

    // --- History
    y = 12;
    NumB(g_setPage[PG_HISTORY], L(STR_HI_MAX), 16, y, &Settings::historyMax, 10, 10000); y += 32;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_UNDO),  16, y, &Settings::useUndo);       y += 26;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_INT),   16, y, &Settings::saveInternal);  y += 26;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_AL),    16, y, &Settings::saveAlias);     y += 26;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_WWW),   16, y, &Settings::saveWww);       y += 26;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_MAIL),  16, y, &Settings::saveEmail);     y += 26;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_PATH),  16, y, &Settings::savePath);      y += 26;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_PLG),   16, y, &Settings::savePlugins);   y += 26;
    CkB(g_setPage[PG_HISTORY], L(STR_HI_OTHER), 16, y, &Settings::saveUnknown);   y += 30;
    MkButton(g_setPage[PG_HISTORY], L(STR_HI_CLEAR), 16, y, 130, IDB_HI_CLEAR);

    // --- Log
    y = 12;
    CkB(g_setPage[PG_LOG], L(STR_LO_ENABLE), 16, y, &Settings::logEnabled); y += 28;
    TxB(g_setPage[PG_LOG], L(STR_LO_FILE), 16, y, &Settings::logFile);      y += 28;
    NumB(g_setPage[PG_LOG], L(STR_LO_MAXKB), 16, y, &Settings::logMaxKB, 16, 65536); y += 32;
    MkLabel(g_setPage[PG_LOG], L(STR_LO_OVERFLOW), 16, y + 2);
    {
        const wchar_t* it[] = { L"rotate", L"truncate", L"timestamp", L"keep" };
        HWND c = MkCombo(g_setPage[PG_LOG], 175, y, 130, it, 4);
        Bind([c]{ SendMessageW(c, CB_SETCURSEL, g_draft.logRewrite, 0); },
             [c]{ g_draft.logRewrite = (int)SendMessageW(c, CB_GETCURSEL, 0, 0); });
    }
    y += 32;
    NumB(g_setPage[PG_LOG], L(STR_LO_KEEP), 16, y, &Settings::logKeep, 1, 99);

    // --- Aliases
    g_alLb = MkOn(g_setPage[PG_ALIASES], L"LISTBOX", L"",
                  WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 8, 8, 150, 330);
    int ax = 170;
    MkLabel(g_setPage[PG_ALIASES], L(STR_AL_NAME),   ax, 10, 60);
    g_alName   = MkEdit(g_setPage[PG_ALIASES], L"", ax + 55, 8, 230);
    MkLabel(g_setPage[PG_ALIASES], L(STR_AL_ACTION), ax, 40, 60);
    g_alAction = MkEdit(g_setPage[PG_ALIASES], L"", ax + 55, 38, 230);
    MkLabel(g_setPage[PG_ALIASES], L(STR_AL_DIR),    ax, 70, 60);
    g_alDir    = MkEdit(g_setPage[PG_ALIASES], L"", ax + 55, 68, 230);
    MkLabel(g_setPage[PG_ALIASES], L(STR_AL_WINDOW), ax, 100, 60);
    {
        const wchar_t* it[] = { L(STR_AL_WN_NORMAL), L(STR_AL_WN_MAX),
                                L(STR_AL_WN_MIN), L(STR_AL_WN_HIDDEN) };
        g_alState = MkCombo(g_setPage[PG_ALIASES], ax + 55, 98, 130, it, 4);
    }
    MkButton(g_setPage[PG_ALIASES], L(STR_AL_ADD),    ax, 132, 72, IDB_AL_ADD);
    MkButton(g_setPage[PG_ALIASES], L(STR_AL_UPDATE), ax + 80, 132, 72, IDB_AL_UPDATE);
    MkButton(g_setPage[PG_ALIASES], L(STR_AL_DELETE), ax + 160, 132, 72, IDB_AL_DELETE);
    MkLabel(g_setPage[PG_ALIASES], L(STR_AL_HINT), ax, 168, 290);

    // --- Sounds
    y = 12;
    CkB(g_setPage[PG_SOUNDS], L(STR_SO_PLAY), 16, y, &Settings::enableSounds);
    y += 30;
    for (int i = 0; i < 9; ++i) {
        int yy = y + i * 26;
        MkLabel(g_setPage[PG_SOUNDS], L(STR_SO_INTERNAL + i), 16, yy + 2, 130);
        HWND e = MkEdit(g_setPage[PG_SOUNDS], L"", 150, yy, 300);
        Bind([e, i]{ SetWindowTextW(e, g_draft.sound[i].c_str()); },
             [e, i]{ g_draft.sound[i] = Trim(GetText(e)); });
    }

    // --- Plugins
    g_plLb = MkOn(g_setPage[PG_PLUGINS], L"LISTBOX", L"",
                  WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 8, 8, 180, 250);
    int px = 200;
    g_plLoadChk = MkCheck(g_setPage[PG_PLUGINS], L(STR_PL_LOAD), px, 12, 180);
    g_plHistChk = MkCheck(g_setPage[PG_PLUGINS], L(STR_PL_HIST), px, 38, 180);
    g_plInfo = MkOn(g_setPage[PG_PLUGINS], L"STATIC", L"", SS_LEFT | SS_EDITCONTROL,
                    px, 70, 260, 150);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_RESCAN),  px, 240, 80, IDB_PL_RESCAN);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_UNLOAD),  px + 90, 240, 80, IDB_PL_UNLOAD);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_ABOUT),   px, 270, 80, IDB_PL_ABOUT);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_OPTIONS), px + 90, 270, 80, IDB_PL_OPTIONS);
    MkLabel(g_setPage[PG_PLUGINS], L(STR_PL_HINT), 8, 268, 190);

    // --- Environment (own PATH folders + %variables%, like the OS dialog)
    MkLabel(g_setPage[PG_ENV], L(STR_EN_PATHHINT), 16, 10, 430);
    g_envPathEdit = MkOn(g_setPage[PG_ENV], L"EDIT", L"",
                         WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOHSCROLL,
                         16, 30, 430, 100);
    Bind([]{
        std::wstring joined;
        for (auto& d : g_draftPath) { joined += d; joined += L"\r\n"; }
        SetWindowTextW(g_envPathEdit, joined.c_str());
    }, []{
        std::vector<wchar_t> buf(8192);
        GetWindowTextW(g_envPathEdit, buf.data(), 8191);
        g_draftPath.clear();
        std::wstring text(buf.data()), cur;
        for (wchar_t ch : text) {
            if (ch == L'\r' || ch == L'\n') {
                std::wstring t = Trim(cur);
                if (!t.empty()) g_draftPath.push_back(t);
                cur.clear();
            } else cur += ch;
        }
        std::wstring t = Trim(cur);
        if (!t.empty()) g_draftPath.push_back(t);
    });
    MkLabel(g_setPage[PG_ENV], L(STR_EN_VARSHINT), 16, 140, 430);
    g_varLb = MkOn(g_setPage[PG_ENV], L"LISTBOX", L"",
                   WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 16, 160, 200, 170);
    int vx = 226;
    MkLabel(g_setPage[PG_ENV], L(STR_AL_NAME),  vx, 162, 50);
    g_varName  = MkEdit(g_setPage[PG_ENV], L"", vx + 45, 160, 175);
    MkLabel(g_setPage[PG_ENV], L(STR_EN_VALUE), vx, 192, 50);
    g_varValue = MkEdit(g_setPage[PG_ENV], L"", vx + 45, 190, 175);
    MkButton(g_setPage[PG_ENV], L(STR_EN_ADDVAR), vx, 222, 84, IDB_EN_ADDVAR);
    MkButton(g_setPage[PG_ENV], L(STR_EN_UPDVAR), vx + 90, 222, 84, IDB_EN_UPDATEVAR);
    MkButton(g_setPage[PG_ENV], L(STR_EN_DELVAR), vx, 252, 84, IDB_EN_DELETEVAR);
    MkLabel(g_setPage[PG_ENV], L(STR_EN_NOTE), vx, 286, 210);

    // --- Theme (cozy look: console + status strip + tray badge colors)
    y = 12;
    MkLabel(g_setPage[PG_THEME], L(STR_TH_SCHEME), 16, y + 2, 110);
    {
        const wchar_t* it[kThemeCount];
        for (int i = 0; i < kThemeCount; ++i) it[i] = kThemes[i].name;
        g_themeCombo = MkCombo(g_setPage[PG_THEME], 130, y, 180, it, kThemeCount);
        Bind([]{ SendMessageW(g_themeCombo, CB_SETCURSEL,
                              ThemeFromName(g_draft.theme), 0);
                 if (g_themePrev) InvalidateRect(g_themePrev, nullptr, TRUE); },
             []{ int sel = (int)SendMessageW(g_themeCombo, CB_GETCURSEL, 0, 0);
                 if (sel < 0) sel = 0;
                 g_draft.theme = kThemes[sel].name;
                 if (g_themePrev) InvalidateRect(g_themePrev, nullptr, TRUE); });
    }
    y += 30;
    g_themePrev = MkOn(g_setPage[PG_THEME], L"STATIC", L"", SS_OWNERDRAW, 16, y, 430, 96);
    MkLabel(g_setPage[PG_THEME], L(STR_TH_HINT), 16, y + 102, 430);

    // --- Language (20 UI languages; Apply switches immediately, Arabic RTL)
    MkLabel(g_setPage[PG_LANG], L(STR_LA_LABEL), 16, 14, 430);
    {
        const wchar_t* it[LANG_COUNT + 1];
        it[0] = L(STR_LA_AUTO);
        for (int i = 0; i < LANG_COUNT; ++i) it[i + 1] = kLangs[i].native;
        g_langCombo = MkCombo(g_setPage[PG_LANG], 16, 38, 220, it, LANG_COUNT + 1);
        Bind([]{
            int sel = 0;
            std::wstring c = g_draft.langCode;
            if (!c.empty() && _wcsicmp(c.c_str(), L"auto") != 0) {
                size_t dash = c.find(L'-');
                if (dash != std::wstring::npos) c = c.substr(0, dash);
                for (int i = 0; i < LANG_COUNT; ++i)
                    if (_wcsicmp(c.c_str(), kLangs[i].code) == 0) { sel = i + 1; break; }
            }
            SendMessageW(g_langCombo, CB_SETCURSEL, sel, 0);
        }, []{
            int sel = (int)SendMessageW(g_langCombo, CB_GETCURSEL, 0, 0);
            g_draft.langCode = (sel <= 0) ? L"auto" : kLangs[sel - 1].code;
        });
    }
    MkOn(g_setPage[PG_LANG], L"STATIC",
            L"English · Русский · Українська · Deutsch · Français ·\n"
            L"Español · Português · Italiano · 中文 · 日本語 · 한국어 ·\n"
            L"العربية · Türkçe · Polski · Tiếng Việt · हिन्दी ·\n"
            L"Bahasa Indonesia · Kiswahili · Afrikaans · አማርኛ",
            SS_LEFT, 16, 76, 430, 90);
    MkLabel(g_setPage[PG_LANG], L(STR_LA_HINT), 16, 172, 430);

    MkButton(g_setWnd, L(STR_OK),     380, 380, 75, IDB_OK);
    MkButton(g_setWnd, L(STR_APPLY),  461, 380, 75, IDB_APPLY);
    MkButton(g_setWnd, L(STR_CANCEL), 542, 380, 75, IDB_CANCEL);

    // plugin rows from the discovered set + [PluginState] overrides
    g_plRows.clear();
    g_plSel = 0;
    for (auto& p : g_plugins) {
        PluginRow r;
        r.name     = PluginKey(p);
        r.load     = PluginStateOn(r.name + L"_Load", true) ? 1 : 0;
        r.saveHist = PluginStateOn(r.name + L"_SaveHistory", true) ? 1 : 0;
        g_plRows.push_back(r);
    }
    PluginsFillList();
    PluginsEditRow(g_plSel);

    // environment page drafts (raw, so %VARS% survive the round-trip)
    g_draftPath = g_pathRaw;
    g_draftVars = g_envVars;
    g_varSel = g_draftVars.empty() ? -1 : 0;
    VarsFillList();
    VarsEditRow(g_varSel);

    UiLoad();
    SettingsShowPage(startPage);
    ShowWindow(g_setWnd, SW_SHOW);
    UpdateWindow(g_setWnd);
    SetForegroundWindow(g_setWnd);
}

// ---------------------------------------------------------------- INI + apply
static void WInt(const wchar_t* sec, const wchar_t* key, long v) {
    wchar_t b[24]; wsprintfW(b, L"%ld", v);
    WritePrivateProfileStringW(sec, key, b, g_iniPath.c_str());
}
static void WB(const wchar_t* sec, const wchar_t* key, bool v) {
    WritePrivateProfileStringW(sec, key, v ? L"1" : L"0", g_iniPath.c_str());
}
static void WS_(const wchar_t* sec, const wchar_t* key, const std::wstring& v) {
    WritePrivateProfileStringW(sec, key, v.c_str(), g_iniPath.c_str());
}

static void WriteDraftIni() {
    Settings& s = g_draft;
    WB(L"Show", L"ShowConsoleOnStart", s.showOnStart);
    WB(L"Show", L"HideOnDeactivate",   s.hideOnDeactivate);
    WB(L"Show", L"TrayIcon",           s.trayIcon);
    WB(L"Show", L"BalloonTips",        s.balloonTips);
    WB(L"Show", L"AutoHide",           s.autoHide);
    WInt(L"Show", L"AutoHideValue",    s.autoHideValue);
    WInt(L"Show", L"Width",            s.width);
    WInt(L"Show", L"Height",           s.height);
    WInt(L"Show", L"X",                s.x);
    WInt(L"Show", L"Y",                s.y);
    WB(L"Show", L"CenterScreenX",      s.centerX);
    WB(L"Show", L"CenterScreenY",      s.centerY);
    WInt(L"Show", L"Transparent",      s.transparent);
    WS_(L"Show", L"Priority",          s.priority);
    WS_(L"UI", L"Language",            s.langCode);
    WS_(L"Theme", L"Name",             s.theme);

    WS_(L"Font", L"Name",              s.fontName);
    WInt(L"Font", L"Size",             s.fontSize);
    WB(L"Font", L"Bold",               s.fontBold != 0);
    WB(L"Font", L"ApplyEnglish",       s.applyEnglish);

    WInt(L"HotKey", L"Key",            g_draftHk.key);
    WB(L"HotKey", L"AltKey",           g_draftHk.alt);
    WB(L"HotKey", L"CtrlKey",          g_draftHk.ctrl);
    WB(L"HotKey", L"ShiftKey",         g_draftHk.shift);
    WB(L"HotKey", L"WinKey",           g_draftHk.win);

    WB(L"Exec", L"Internal", s.execInternal);
    WB(L"Exec", L"Aliases",  s.execAliases);
    WB(L"Exec", L"WWW",      s.execWww);
    WB(L"Exec", L"EMail",    s.execEmail);
    WB(L"Exec", L"Files",    s.execFiles);
    WB(L"Exec", L"Shell",    s.execShell);
    WB(L"Exec", L"Plugins",  s.execPlugins);

    WInt(L"Type", L"EasyType",     s.easyType);
    WB(L"Type", L"UseInternalET",  s.etInternal);
    WB(L"Type", L"UseAliasET",     s.etAlias);
    WB(L"Type", L"UseHistoryET",   s.etHistory);
    WB(L"Type", L"UseSysAliasET",  s.etSysAlias);

    WInt(L"History", L"Max",        s.historyMax);
    WB(L"History", L"Internal",     s.saveInternal);
    WB(L"History", L"Alias",        s.saveAlias);
    WB(L"History", L"WWW",          s.saveWww);
    WB(L"History", L"EMail",        s.saveEmail);
    WB(L"History", L"Path",         s.savePath);
    WB(L"History", L"Plugins",      s.savePlugins);
    WB(L"History", L"Unknown",      s.saveUnknown);
    WB(L"Undo", L"UseUndo",         s.useUndo);

    WB(L"Log", L"Enabled",          s.logEnabled);
    WS_(L"Log", L"File",            s.logFile);
    WInt(L"Log", L"MaxSizeKB",      s.logMaxKB);
    WS_(L"Log", L"Rewrite",         s.logRewrite == 1 ? L"truncate" :
                                    s.logRewrite == 2 ? L"timestamp" :
                                    s.logRewrite == 3 ? L"keep" : L"rotate");
    WInt(L"Log", L"Keep",           s.logKeep);

    if (s.consoleBg >= 0) WInt(L"Color", L"ConsoleBG", s.consoleBg);
    else WritePrivateProfileStringW(L"Color", L"ConsoleBG", nullptr, g_iniPath.c_str());
    if (s.consoleFg >= 0) WInt(L"Color", L"ConsoleFG", s.consoleFg);
    else WritePrivateProfileStringW(L"Color", L"ConsoleFG", nullptr, g_iniPath.c_str());

    WB(L"Sounds", L"EnableSounds",     s.enableSounds);
    for (int i = 0; i < 9; ++i)
        WS_(L"Sounds", kSoundKeys[i],  s.sound[i]);

    // [PluginState]: rewrite with the checkbox state of every known plugin
    PluginsReadRow();
    WritePrivateProfileStringW(L"PluginState", nullptr, nullptr, g_iniPath.c_str());
    for (auto& r : g_plRows) {
        WS_(L"PluginState", (r.name + L"_Load").c_str(),        r.load     ? L"1" : L"0");
        WS_(L"PluginState", (r.name + L"_SaveHistory").c_str(), r.saveHist ? L"1" : L"0");
    }

    // [Path] + [Variables]: rewrite with the dialog's raw lines
    WritePrivateProfileStringW(L"Path", nullptr, nullptr, g_iniPath.c_str());
    for (size_t i = 0; i < g_draftPath.size(); ++i)
        WS_(L"Path", (L"Dir" + std::to_wstring(i + 1)).c_str(), g_draftPath[i]);
    WritePrivateProfileStringW(L"Variables", nullptr, nullptr, g_iniPath.c_str());
    for (auto& v : g_draftVars)
        WS_(L"Variables", v.name.c_str(), v.raw);

    // [Aliases]: rewrite section in canonical form
    WritePrivateProfileStringW(L"Aliases", nullptr, nullptr, g_iniPath.c_str());
    for (auto& a : g_draftAliases) {
        std::wstring value = a.action;
        if (!a.dir.empty() || a.state != SW_SHOWNORMAL) {
            wchar_t st[8]; wsprintfW(st, L"%d", a.state);
            value = a.action + L"|" + L"|" + a.dir + L"|" + st;
        }
        WS_(L"Aliases", a.name.c_str(), value);
    }

    // remember dialog position (TypeAndRun [Coordinates] parity)
    if (g_setWnd) {
        RECT r; GetWindowRect(g_setWnd, &r);
        WInt(L"Coordinates", L"SettingsX", r.left);
        WInt(L"Coordinates", L"SettingsY", r.top);
    }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, g_iniPath.c_str()); // flush
}

static void LiveApply(HWND mainWnd) {
    ReloadHotkey(mainWnd);                 // LoadSettings + RegisterHotKey
    LoadAliases();                         // LoadSettings covers sys-aliases/ignores
    // language: re-resolve pack + RTL before fonts (CJK/RTL suggestion)
    {
        bool wasRtl = (GetWindowLongW(mainWnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
        SetLanguage(LangFromCode(g_set.langCode));
        bool isRtl = LangRtl();
        SetProcessDefaultLayout(isRtl ? LAYOUT_RTL : 0);
        if (isRtl != wasRtl) {
            SetWindowLongW(mainWnd, GWL_EXSTYLE,
                           GetWindowLongW(mainWnd, GWL_EXSTYLE) ^ WS_EX_LAYOUTRTL);
            SetWindowPos(mainWnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        LogInfo(L"language: " + std::wstring(kLangs[g_langIdx].code) +
                (isRtl ? L" (rtl)" : L""));
    }
    // honour [PluginState] immediately: unload the ones just disabled
    for (auto& p : g_plugins)
        if (p.loaded && !PluginLoadAllowed(p)) {
            if (p.Unload) p.Unload();
            p.loaded = false;
        }
    ApplyFont(g_edit);
    DWORD pc = NORMAL_PRIORITY_CLASS;
    if      (g_set.priority == L"idle")     pc = IDLE_PRIORITY_CLASS;
    else if (g_set.priority == L"high")     pc = HIGH_PRIORITY_CLASS;
    else if (g_set.priority == L"realtime") pc = REALTIME_PRIORITY_CLASS;
    SetPriorityClass(GetCurrentProcess(), pc);
    ApplyThemeVisual();                    // effective bg/fg/accent from [Theme]/[Color]
    g_trayErr = false;
    FreeTrayIcons();                       // badge follows the new accent
    SetTrayVisual();
    LogInfo(L"theme: " + g_set.theme);
    if (g_set.trayIcon) { if (!g_nid.hWnd) AddTrayIcon(mainWnd, false); }
    else RemoveTrayIcon();
    KillTimer(mainWnd, 1);
    if (g_visible && g_set.autoHide && g_set.autoHideValue > 0)
        SetTimer(mainWnd, 1, (UINT)g_set.autoHideValue * 1000, nullptr);
    if (g_visible) ShowBox(mainWnd);       // reposition / re-alpha with new values
    else InvalidateRect(mainWnd, nullptr, TRUE);
    LogInfo(L"settings applied");
}

static void OpenSettings(HWND mainWnd, int page) {
    if (g_setWnd) {                        // already open: raise + switch page
        SendMessageW(g_setList, LB_SETCURSEL, page, 0);
        SettingsShowPage(page);
        SetForegroundWindow(g_setWnd);
        return;
    }
    SettingsCreate(mainWnd, page);
}

// test hook: create the dialog, pump, destroy (--test-settings)
static bool SettingsSelfTest(HWND mainWnd) {
    OpenSettings(mainWnd, 0);
    if (!g_setWnd) return false;
    MSG m;
    for (int i = 0; i < 20; ++i) {
        if (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(20);
    }
    bool ok = g_setWnd != nullptr;         // survived the pump
    if (g_setWnd) PostMessageW(g_setWnd, WM_CLOSE, 0, 0);
    for (int i = 0; i < 20; ++i) {
        if (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(20);
    }
    return ok && g_setWnd == nullptr;
}

static LRESULT CALLBACK EditProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp,
                                 UINT_PTR id, DWORD_PTR ref) {
    HWND mainWnd = (HWND)ref;
    (void)id;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    switch (msg) {
    case WM_KEYDOWN:
        switch (wp) {
        case VK_RETURN: {
            // Shift+Enter kills the process started by the last alias (TypeAndRun parity).
            // Physical shift lives in GetKeyState, not in WM_KEYDOWN lParam bits.
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ||
                         (GET_KEYSTATE_WPARAM(lp) & MK_SHIFT) != 0;
            if (shift && g_aliasProc) {
                DWORD w = WaitForSingleObject(g_aliasProc, 0);
                if (w == WAIT_TIMEOUT) {
                    TerminateProcess(g_aliasProc, 0);
                    LogInfo(L"Shift+Enter: killed alias process " + g_aliasName);
                } else {
                    LogInfo(L"Shift+Enter: nothing to kill (" + g_aliasName +
                            L", wait=" + std::to_wstring(w) +
                            L", gle=" + std::to_wstring(GetLastError()) + L")");
                }
                CloseHandle(g_aliasProc);
                g_aliasProc = nullptr;
                return 0;
            }
            if (shift) { LogInfo(L"Shift+Enter: no running alias process to kill"); return 0; }
            Submit(mainWnd);  return 0;
        }
        case VK_ESCAPE: HideBox(mainWnd); return 0;
        case VK_TAB: TabComplete(hWnd);  return 0;
        case VK_UP:
            if (g_histPos > 0) SetHistory(--g_histPos);
            return 0;
        case VK_DOWN:
            if (g_histPos < g_history.size()) SetHistory(++g_histPos);
            else { g_histPos = g_history.size(); SetHistory(g_histPos); }
            return 0;
        case VK_BACK: // word-wise backspace (TypeAndRun AdvancedBackspace)
            if (ctrl) {
                std::wstring t = GetEditText(hWnd);
                size_t e = t.size();
                while (e > 0 && (t[e-1] == L' ' || t[e-1] == L'\t')) --e;
                size_t b = e;
                while (b > 0 && t[b-1] != L' ' && t[b-1] != L'\t') --b;
                SetWindowTextW(hWnd, t.substr(0, b).c_str());
                SendMessageW(hWnd, EM_SETSEL, -1, 0);
                return 0;
            }
            break;
        case 'Z': // Undo: bring back the last executed command (TypeAndRun [Undo])
            if (ctrl && g_set.useUndo && !g_lastRun.empty()) {
                SetWindowTextW(hWnd, g_lastRun.c_str());
                SendMessageW(hWnd, EM_SETSEL, -1, 0);
                return 0;
            }
            break;
        }
        break;
    }
    if (msg == WM_KEYDOWN && wp != VK_TAB) ResetET(); // typing rebuilds candidates
    return DefSubclassProc(hWnd, msg, wp, lp);
}

static void ReloadHotkey(HWND mainWnd) {
    UnregisterHotKey(mainWnd, kHotkeyId);
    LoadSettings();
    if (!RegisterHotKey(mainWnd, kHotkeyId, g_hotkey.mods(), g_hotkey.key))
        LogError(L"RegisterHotKey(/reread) failed");
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_edit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, hWnd, nullptr, nullptr, nullptr);
        SetWindowSubclass(g_edit, EditProc, 1, (DWORD_PTR)hWnd);
        return 0;
    }
    case WM_SIZE: {
        // keep the edit centered in the space ABOVE the 20px status strip
        int above = (int)HIWORD(lp) - SB_H;
        if (above < 16) above = 16;
        MoveWindow(g_edit, 2, (above - 24) / 2, LOWORD(lp) - 4, 24, TRUE);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1 && g_visible) HideBox(hWnd);   // [Show] AutoHide
        else if (wp == 2) {                        // clock: refresh the strip
            RECT rc; GetClientRect(hWnd, &rc);
            rc.top = rc.bottom - SB_H;
            if (g_visible) InvalidateRect(hWnd, &rc, TRUE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        if (g_bgBrush) FillRect(dc, &rc, g_bgBrush);
        if (g_sbFont) PaintStatusBar(dc, rc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_PRINTCLIENT: {           // PrintWindow/screenshot capture path
        HDC dc = (HDC)wp;
        RECT rc; GetClientRect(hWnd, &rc);
        if (g_bgBrush) FillRect(dc, &rc, g_bgBrush);
        if (g_sbFont) PaintStatusBar(dc, rc);
        return 0;
    }
    case WM_ERASEBKGND:
        if (g_bgBrush) {
            RECT rc; GetClientRect(hWnd, &rc);
            FillRect((HDC)wp, &rc, g_bgBrush);
            return 1;
        }
        break;
    case WM_CTLCOLOREDIT:
        if (g_bgBrush) {
            HDC dc = (HDC)wp;
            SetBkColor(dc, g_effBg);
            SetTextColor(dc, g_effFg);
            return (LRESULT)g_bgBrush;
        }
        break;
    case WM_HOTKEY:
        if (wp == kHotkeyId) {
            // Ignores.ini (TypeAndRun BadApp): stay hidden over listed apps
            if (!g_visible && AppIgnored()) return 0;
            g_visible ? HideBox(hWnd) : ShowBox(hWnd);
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && g_visible && g_set.hideOnDeactivate)
            HideBox(hWnd);
        return 0;
    case kTrayMsg: {
        UINT ev = LOWORD(lp);
        if (ev == WM_LBUTTONUP || ev == WM_LBUTTONDBLCLK)
            g_visible ? HideBox(hWnd) : ShowBox(hWnd);
        else if (ev == WM_RBUTTONUP)
            ShowTrayMenu(hWnd);
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE && g_edit && (HWND)lp == g_edit) {
            // live traffic-light preview of what Enter will execute
            Anchor na = ClassifyAnchor(GetEditText(g_edit));
            if (na.lamp != g_anchor.lamp || na.labelId != g_anchor.labelId) {
                g_anchor = na;
                if (g_visible) {
                    RECT rc; GetClientRect(hWnd, &rc);
                    rc.top = rc.bottom - SB_H;
                    InvalidateRect(hWnd, &rc, TRUE);
                }
            }
        }
        if (LOWORD(wp) == 1)      g_visible ? HideBox(hWnd) : ShowBox(hWnd);
        else if (LOWORD(wp) == 2) ShellExecuteW(hWnd, L"open", g_iniPath.c_str(),
                                                nullptr, nullptr, SW_SHOWNORMAL);
        else if (LOWORD(wp) == 3) DestroyWindow(hWnd);
        else if (LOWORD(wp) == 4) OpenSettings(hWnd, PG_APPEAR);
        return 0;
    case WM_COPYDATA: {
        // Second instance forwards its command line for execution.
        auto cds = (PCOPYDATASTRUCT)lp;
        if (cds && cds->cbData >= sizeof(wchar_t)) {
            std::wstring cmd((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
            while (!cmd.empty() && cmd.back() == L'\0') cmd.pop_back();
            cmd = Trim(cmd);
            if (!cmd.empty()) {
                ShowBox(hWnd);
                SetWindowTextW(g_edit, cmd.c_str());
                Submit(hWnd);
            }
        }
        return TRUE;
    }
    case WM_DESTROY:
        UnregisterHotKey(hWnd, kHotkeyId);
        RemoveTrayIcon();
        if (g_aliasProc) { CloseHandle(g_aliasProc); g_aliasProc = nullptr; }
        CallPluginsUnload();
        for (auto& p : g_plugins)
            if (p.hmod) FreeLibrary(p.hmod);
        g_plugins.clear();
        SaveHistory();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

// ---------------------------------------------------------------- entry

// Args of a second instance, without the program name.
static std::wstring SecondInstanceArgs() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring out;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) out += L' ';
        out += argv[i];
    }
    if (argv) LocalFree(argv);
    return Trim(out);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    SetProcessDPIAware();

    // Smoke-test / user friendly: `TypeBox.exe --version` prints and exits
    // (used by CI to refuse publishing a build that cannot start).
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        bool wantVersion = false;
        for (int i = 1; i < argc; ++i)
            if (_wcsicmp(argv[i], L"--version") == 0 || _wcsicmp(argv[i], L"/version") == 0)
                wantVersion = true;
        for (int i = 1; i < argc; ++i)
            if (_wcsicmp(argv[i], L"--test-settings") == 0)
                g_testSettings = true;
        if (argv) LocalFree(argv);
        if (wantVersion) {
            if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                std::string s = ToUtf8(L"TypeBox " + WVersion() + L"\n");
                HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
                DWORD wr = 0;
                DWORD mode = 0;
                if (h && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
                    WriteFile(h, s.c_str(), (DWORD)s.size(), &wr, nullptr);
                else
                    OutputDebugStringA(s.c_str());
            }
            return 0;
        }
    }

    g_iniPath = ExeDir() + L"TypeBox.ini";
    InitCommonControls();

    // Single instance: forward the command line to the running one.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"TypeBox_SingleInstance");
    (void)mutex; // must stay open for the lifetime of the process
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(kClassName, nullptr);
        std::wstring args = SecondInstanceArgs();
        if (other && !args.empty()) {
            COPYDATASTRUCT cds = {};
            cds.dwData = 1;
            cds.cbData = (DWORD)((args.size() + 1) * sizeof(wchar_t));
            cds.lpData = (PVOID)args.c_str();
            SendMessageW(other, WM_COPYDATA, 0, (LPARAM)&cds);
        }
        return 0;
    }

    LoadSettings();
    SetLanguage(LangFromCode(g_set.langCode));   // L(STR_*) from here on
    SetProcessDefaultLayout(LangRtl() ? LAYOUT_RTL : 0);   // Arabic mirror (LTR = 0)
    LoadAliases();
    LoadHistory();
    DiscoverPlugins();

    // [Show] Priority — process class of TypeBox itself
    DWORD pc = NORMAL_PRIORITY_CLASS;
    if      (g_set.priority == L"idle")     pc = IDLE_PRIORITY_CLASS;
    else if (g_set.priority == L"high")     pc = HIGH_PRIORITY_CLASS;
    else if (g_set.priority == L"realtime") pc = REALTIME_PRIORITY_CLASS;
    SetPriorityClass(GetCurrentProcess(), pc);
    ApplyThemeVisual();   // theme + [Color] -> g_effBg/g_effFg/g_accent + g_bgBrush

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kClassName;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    HWND mainWnd = CreateWindowExW(exStyle,
        kClassName, L"TypeBox", WS_POPUP,
        0, 0, g_set.width, g_set.height, nullptr, nullptr, hInst, nullptr);
    if (!mainWnd) return 1;

    g_sbFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           VARIABLE_PITCH | FF_DONTCARE, L"Segoe UI");
    ApplyThemeVisual();
    ApplyFont(g_edit);
    CallPluginsLoad(mainWnd);

    if (!RegisterHotKey(mainWnd, kHotkeyId, g_hotkey.mods(), g_hotkey.key)) {
        LogError(L"RegisterHotKey failed (key=" + std::to_wstring(g_hotkey.key) +
                 L", mods=" + std::to_wstring(g_hotkey.mods()) + L")");
        MessageBoxW(mainWnd,
            L"TypeBox: failed to register the hotkey.\n"
            L"Check [HotKey] in TypeBox.ini, then run /reread.",
            L"TypeBox", MB_OK | MB_ICONERROR);
    }

    {
        size_t enabled = 0;
        for (auto& p : g_plugins)
            if (PluginLoadAllowed(p)) ++enabled;
        LogInfo(L"TypeBox " + WVersion() + L" started; " +
                std::to_wstring(g_aliases.size()) + L" aliases, " +
                std::to_wstring(g_plugins.size()) + L" plugins, " +
                std::to_wstring(enabled) + L" enabled");
        if (!g_pathDirs.empty() || !g_envVars.empty())
            LogInfo(L"own PATH: " + std::to_wstring(g_pathDirs.size()) +
                    L" folders, variables: " + std::to_wstring(g_envVars.size()));
        LogInfo(L"language: " + std::wstring(kLangs[g_langIdx].code) +
                L" (" + (g_set.langCode.empty() ? L"auto" : g_set.langCode) + L")");
        LogInfo(L"theme: " + g_set.theme);
    }

    AddTrayIcon(mainWnd, true);
    SetTrayVisual();                             // cozy badge replaces the default icon
    SetTimer(mainWnd, 2, 15000, nullptr);   // status bar clock refresh
    if (g_set.showOnStart) ShowBox(mainWnd);

    if (g_testSettings) {                  // CI gate: dialog must build, pump, close
        bool ok = SettingsSelfTest(mainWnd);
        LogInfo(ok ? L"settings self-test OK" : L"settings self-test FAILED");
        DestroyWindow(mainWnd);
        MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&m);
        return ok ? 0 : 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        // Settings dialog: tab/enter navigation + Esc closes it.
        if (g_setWnd && (GetAncestor(msg.hwnd, GA_ROOT) == g_setWnd)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
                msg.hwnd != g_hkEdit) {
                DestroyWindow(g_setWnd);
                continue;
            }
            if (IsDialogMessageW(g_setWnd, &msg)) continue;
        }
        // Let Esc close the box even when the edit has focus.
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
            msg.hwnd == g_edit && g_visible) {
            HideBox(mainWnd);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
