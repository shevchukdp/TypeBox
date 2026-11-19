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
//                -luser32 -lshell32 -lshlwapi -lgdi32 -lcomctl32 -lcomdlg32
//                -lwinmm -lwinhttp -lbcrypt

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cstring>
#include <cwctype>
#include <cmath>
#include <algorithm>
#include <functional>
#include <ctime>

#ifndef TYPEBOX_VERSION
#define TYPEBOX_VERSION 0.0.0-dev
#endif
#define TB_STR_(x) #x
#define TB_STR(x) TB_STR_(x)

static const wchar_t* kClassName  = L"TypeBoxMain";
static const int      kHotkeyId   = 666; // same lineage as TypeAndRun HotKeyId=666
static const UINT     kTrayMsg    = WM_APP + 1;
static const UINT     kTrayId     = 1;
static const UINT     kUpdateMsg  = WM_APP + 2;   // v0.20: opt-in startup update note
static const wchar_t* kUpdateApiDefault =
    L"https://api.github.com/repos/shevchukdp/TypeBox/releases/latest";

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
    bool dragMove = true;      // [Show] DragMove: status strip doubles as drag handle (v0.19)
    // [Exec]
    bool execInternal = true, execAliases = true, execWww = true,
         execEmail = true, execFiles = true, execShell = true, execPlugins = true;
    bool execScripts = true;         // [Exec] Scripts: portable Scripts\ folder (v0.16)
    bool confirmSuspicious = true;   // [Security] ConfirmSuspicious: safe Run box (v0.16)
    bool confirmDanger = true;       // [Security] ConfirmDanger: destructive commands (v0.17)
    bool elevate = true;             // [Security] Elevate: su actually uses runas (v0.18)
    bool frecency = true;            // [Type] FrecencyRanking: most-used wins ties (v0.17)
    // [Update] (v0.20): /update release check + install helper. Offline-first:
    // network only when the user typed /update, or with explicit CheckOnStart.
    std::wstring updApi;             // release API ("" = official GitHub releases)
    bool updAutoRun = true;          // launch install helper right after verification
    bool updCheckOnStart = false;    // opt-in silent startup check + tray balloon
    std::wstring updLatest;          // cache: last known latest tag
    std::wstring updLastCheck;       // cache: unix time of the last successful check
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
    bool fuzzy = true;         // [Type] Fuzzy: subsequence scoring (0 = TypeAndRun prefix mode)
    bool layoutFallback = true;// [Type] LayoutFallback (v0.32): also match the other
                               // keyboard layout (QWERTY <-> ЙЦУКЕН) in ET + aliases
    // [DropDown] suggestion list (TypeAndRun DropDown parity)
    bool dropDown = true;
    int  ddLines = 8;          // visible rows before scrolling ends
    bool ddOnEmpty = true;     // empty line shows the history (TypeAndRun behaviour)
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
// v0.27 (RI3 #12): Ctrl+R bash-style reverse history search. While active the
// drop-down lists only history rows containing the typed query (newest first);
// Enter runs the highlighted row, Esc restores the line you were typing.
static bool                      g_hsrch = false;
static std::wstring              g_hsrchSaved;
static bool                      g_hsrchEatChar = false;   // swallow the translated WM_CHAR
// v0.27: /day + /tabs answer in the console line and keep it open, like the
// bare `su` hint does (EXC_PLUGIN behaves the same way via con_text).
static bool                      g_internalAnswer = false;
// v0.27: HEADLESS TEST MODE - so a running suite cannot disturb a human at the
// keyboard. Three switches, any one is enough: env TYPEBOX_HEADLESS=1 (what the
// test suites use), the --headless-tests command line, or [UI] Headless=1 in
// the INI. Effects: console/settings/ring windows live off-screen at -32000,
// no SetForegroundWindow/SetFocus ever happens, sounds and tray balloons are
// muted, no OS hotkey is registered and no low-level keyboard hook is installed
// (probes post WM_HOTKEY instead), blocking dialogs are suppressed, and URL
// opens go to a minimized inactive window. Posted messages keep every feature
// fully testable - nothing is faked, only the user-visible side is parked.
static bool                      g_headless = false;
static void LogInfo(const std::wstring&);       // defined with the logging section
static void DetectHeadless(void) {
    wchar_t ev[8] = {};
    if (GetEnvironmentVariableW(L"TYPEBOX_HEADLESS", ev, 7) && ev[0] == L'1')
        g_headless = true;
    if (StrStrIW(GetCommandLineW(), L"--headless-tests")) g_headless = true;
    wchar_t ini[MAX_PATH] = {};
    GetModuleFileNameW(NULL, ini, MAX_PATH);
    { wchar_t* bs = wcsrchr(ini, L'\\'); if (bs) bs[1] = 0; }
    wcscat_s(ini, L"TypeBox.ini");
    if (GetPrivateProfileIntW(L"UI", L"Headless", 0, ini) != 0) g_headless = true;
    if (g_headless) LogInfo(L"headless test mode ON (off-screen, quiet, no hooks)");
}

// One choke point for every modal dialog: in headless test mode a MessageBox
// would block the main thread waiting on a human, so it is suppressed, logged,
// and the least-destructive button answer is returned (denials for questions).
static int TbMessageBox(HWND owner, LPCWSTR text, LPCWSTR cap, UINT type) {
    if (g_headless) {
        std::wstring t = cap ? cap : L"";
        std::wstring b = text ? text : L"";
        if (b.size() > 140) b = b.substr(0, 140) + L"...";
        LogInfo(L"[headless] dialog suppressed: [" + t + L"] " + b);
        UINT kind = type & MB_TYPEMASK;
        if (kind == MB_YESNO || kind == MB_YESNOCANCEL) return (int)IDNO;
        if (kind == MB_OKCANCEL || kind == MB_RETRYCANCEL ||
            kind == MB_CANCELTRYCONTINUE)               return (int)IDCANCEL;
        return (int)IDOK;
    }
    return (int)::MessageBoxW(owner, text, cap, type);
}
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
// v0.32 native typing + perf track (ROADMAP-V1 §4b): per-row layout-hit flag
// and pick text live here; the cache lets an unchanged keystroke skip the
// whole re-scan (plugin GetCountET calls are the expensive part); the scan
// counter rides the drop-down title in headless mode for suite asserts.
static std::vector<unsigned char> g_ddLayout;
static std::vector<std::wstring>  g_ddSub;
static std::wstring               g_ddCacheText;
static size_t                     g_ddCacheB = 0, g_ddCacheE = 0;
static bool                       g_ddCacheHsrch = false;
static int                        g_etScanCount = 0;
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

// v0.17: TypeBox.local.ini — per-machine overrides merged over TypeBox.ini.
// Exists => every ReadIni* consults it first; settings UI still WRITES the main
// ini only (a roaming folder keeps the team defaults, the local file the host).
static std::wstring g_localIni;   // empty = absent

static std::wstring ReadIniStr(const wchar_t* sec, const wchar_t* key, const wchar_t* def) {
    if (!g_localIni.empty()) {
        wchar_t buf[4096] = {};
        if (GetPrivateProfileStringW(sec, key, L"", buf, 4096, g_localIni.c_str()) > 0)
            return buf;
    }
    wchar_t buf[4096] = {};
    GetPrivateProfileStringW(sec, key, def, buf, 4096, g_iniPath.c_str());
    return buf;
}

static int ReadIniInt(const wchar_t* sec, const wchar_t* key, int def) {
    if (!g_localIni.empty() &&
        GetPrivateProfileIntW(sec, key, -2147483647 - 1, g_localIni.c_str()) != -2147483647 - 1)
        return GetPrivateProfileIntW(sec, key, def, g_localIni.c_str());
    return GetPrivateProfileIntW(sec, key, def, g_iniPath.c_str());
}

static bool ReadIniBool(const wchar_t* sec, const wchar_t* key, bool def) {
    return ReadIniInt(sec, key, def ? 1 : 0) != 0;
}

// ---------------------------------------------------------------- web search (v0.12)
// Raycast-style quicklinks: [WebSearch] g=https://www.google.com/search?q=%s
// Built-in g/yt/gh ship with the exe; user rows override them by name. The
// keyword + query executes as EXC_URL (history/sounds follow the WWW rules),
// and the anchor previews it in amber.
static std::wstring Trim(const std::wstring& s);
struct WebLink { std::wstring name, tmpl; };
static std::vector<WebLink> g_webLinks;

static std::wstring WebEncode(const std::wstring& q) {
    std::wstring o;
    for (wchar_t c : q) {
        switch (c) {
        case L' ': o += L'+';    break;
        case L'%': o += L"%25";  break;
        case L'&': o += L"%26";  break;
        case L'#': o += L"%23";  break;
        case L'+': o += L"%2B";  break;
        default:   o += c;
        }
    }
    return o;
}

static void LoadWebSearch() {
    g_webLinks.clear();
    if (ReadIniInt(L"WebSearch", L"Enable", 1) == 0) return;
    auto add = [&](const std::wstring& n, const std::wstring& t) {
        if (n.empty() || t.empty()) return;
        bool known = _wcsnicmp(t.c_str(), L"http://", 7) == 0 ||
                     _wcsnicmp(t.c_str(), L"https://", 8) == 0;
        if (!known) return;                       // Enable=1 and comments are not links
        for (auto& w : g_webLinks)
            if (_wcsicmp(w.name.c_str(), n.c_str()) == 0) { w.tmpl = t; return; }
        g_webLinks.push_back({ n, t });
    };
    add(L"g",  L"https://www.google.com/search?q=%s");
    add(L"yt", L"https://www.youtube.com/results?search_query=%s");
    add(L"gh", L"https://github.com/search?q=%s");
    wchar_t buf[16384] = {};
    DWORD n = GetPrivateProfileSectionW(L"WebSearch", buf, ARRAYSIZE(buf), g_iniPath.c_str());
    for (DWORD i = 0; i < n; ) {
        std::wstring row(buf + i);
        i += (DWORD)row.size() + 1;
        size_t eq = row.find(L'=');
        if (eq == std::wstring::npos) continue;
        add(Trim(row.substr(0, eq)), Trim(row.substr(eq + 1)));
    }
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

// ---------------------------------------------------------------- Scripts\ folder (v0.16, RI2 #159)
// Every script in the portable Scripts\ directory becomes a command by its
// file name (args pass through) — no INI entry, no installer.
struct ScriptCmd { std::wstring name, path, ext; };
static std::vector<ScriptCmd> g_scripts;
static void LoadScripts() {
    g_scripts.clear();
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW((ExeDir() + L"Scripts\\*.*").c_str(), &fd);
    if (hf == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring fn = fd.cFileName;
        size_t dot = fn.find_last_of(L'.');
        if (dot == std::wstring::npos || dot == 0) continue;
        std::wstring ext = Lower(fn.substr(dot));
        static const wchar_t* kOk[] = { L".bat", L".cmd", L".ps1", L".py", L".vbs", L".vbe" };
        bool ok = false;
        for (auto e : kOk) if (ext == e) { ok = true; break; }
        if (!ok) continue;
        g_scripts.push_back({ fn.substr(0, dot), ExeDir() + L"Scripts\\" + fn, ext });
    } while (FindNextFileW(hf, &fd));
    FindClose(hf);
    std::sort(g_scripts.begin(), g_scripts.end(),
              [](const ScriptCmd& a, const ScriptCmd& b) {
                  return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
}
static const ScriptCmd* FindScript(const std::wstring& word) {
    for (auto& s : g_scripts)
        if (_wcsicmp(s.name.c_str(), word.c_str()) == 0) return &s;
    return nullptr;
}

// ---------------------------------------------------------------- safe Run box (v0.16, RI2 #168)
// ClickFix-style payload guard: encoded one-liners and fileless loaders are
// the classic paste-into-Run attack. Confirm them BEFORE the shell runs them.
static bool LooksSuspicious(const std::wstring& line) {
    std::wstring s = Lower(line);
    static const wchar_t* kSigs[] = {
        L"-enc ", L"-enc(", L"-enc:", L"encodedcommand", L"frombase64string",
        L"downloadstring", L"-wpercent", L"certutil -", L"bitsadmin",
        L"mshta http", L"mshta vbscript", L"regsvr32 /i:", L"rundll32 http",
        L"rundll32 javascript", L"msiexec /i http", L" | sh", L"|sh ",
        L" | bash", L"|bash ", L" | powershell", L"|powershell", L"| iex",
        L"iex ", L"invoke-expression", L"mimikatz", L"psexec", L"procdump",
        L"lazagne", L"netcat", L"nc.exe", L"base64 -d |", L"base64 --decode |",
    };
    for (auto sig : kSigs) if (s.find(sig) != std::wstring::npos) return true;
    int run = 0;                                  // long base64 blob?
    for (wchar_t c : s) {
        if (iswspace(c)) run = 0;
        else if (iswalnum(c) || c == L'+' || c == L'/' || c == L'=') {
            if (++run >= 80) return true;
        } else run = 0;
    }
    return false;
}

// v0.17: destructive-command tripwire. Token-based (not substring) so paths
// like C:\del\report.pdf stay quiet; covers the classic disk/user/boot wipers.
static bool LooksDangerous(const std::wstring& line) {
    std::wstring s = Lower(Trim(line));
    if (s.empty()) return false;
    std::vector<std::wstring> tok; std::wstring cur;
    for (wchar_t ch : s) {
        if (ch == L' ' || ch == L'\t') { if (!cur.empty()) { tok.push_back(cur); cur.clear(); } }
        else cur += ch;
    }
    if (!cur.empty()) tok.push_back(cur);
    if (tok.empty()) return false;
    auto has = [&](const wchar_t* t) {
        for (auto& x : tok) if (x == t) return true; return false; };
    const std::wstring& t0 = tok[0];
    if (t0 == L"del" || t0 == L"erase" || t0 == L"rd" || t0 == L"rmdir" ||
        t0 == L"format" || t0 == L"diskpart" || t0 == L"shutdown" || t0 == L"takeown" ||
        t0 == L"mkfs")
        return true;
    if (t0 == L"reg" && tok.size() > 1 && tok[1] == L"delete") return true;
    if (t0 == L"net" && tok.size() > 1 &&
        (tok[1] == L"user" || tok[1] == L"localgroup" || tok[1] == L"share")) return true;
    if (t0 == L"vssadmin" && has(L"delete")) return true;
    if (t0 == L"cipher" && has(L"/w")) return true;
    if (t0 == L"sc" && tok.size() > 1 && tok[1] == L"delete") return true;
    if (t0 == L"schtasks" && has(L"/delete")) return true;
    if (t0 == L"wmic" && s.find(L"shadowcopy") != std::wstring::npos && has(L"delete")) return true;
    if (t0 == L"icacls" && has(L"/reset")) return true;
    if (t0 == L"rm" && (has(L"-r") || has(L"-rf") || has(L"-fr") || has(L"--recursive")))
        return true;
    if (t0 == L"dd") { for (auto& x : tok) if (x.rfind(L"if=", 0) == 0) return true; }
    if ((t0 == L"powershell" || t0 == L"pwsh" || t0 == L"powershell.exe" || t0 == L"pwsh.exe") &&
        s.find(L"remove-item") != std::wstring::npos &&
        (s.find(L"-recurse") != std::wstring::npos || s.find(L"-r ") != std::wstring::npos) &&
        (s.find(L"-force") != std::wstring::npos || s.find(L"-f ") != std::wstring::npos))
        return true;
    return false;
}
// Shared YESNO for both security tripwires. true = user allowed, false = blocked.
static void LogInfo(const std::wstring&);       // defined with the logging section
static bool ConfirmDangerLine(const std::wstring& what, HWND owner) {
    std::wstring msg = L"TypeBox \u2014 danger check\n\nThis looks like a destructive command:\n\n"
                       + what + L"\n\nRun it anyway?";
    if (TbMessageBox(owner, msg.c_str(), L"TypeBox",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        LogInfo(L"security: blocked dangerous command");
        return false;
    }
    LogInfo(L"security: confirmed dangerous command");
    return true;
}

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
    g_set.dragMove          = ReadIniBool(L"Show", L"DragMove", true);
    g_set.transparent       = ReadIniInt(L"Show", L"Transparent", 255);
    g_set.autoHide          = ReadIniBool(L"Show", L"AutoHide", false);
    g_set.autoHideValue     = ReadIniInt(L"Show", L"AutoHideValue", 30);

    g_set.execInternal = ReadIniBool(L"Exec", L"Internal", true);
    g_set.execAliases  = ReadIniBool(L"Exec", L"Aliases",  true);
    g_set.execWww      = ReadIniBool(L"Exec", L"WWW",      true);
    g_set.execEmail    = ReadIniBool(L"Exec", L"EMail",    true);
    g_set.execFiles    = ReadIniBool(L"Exec", L"Files",    true);
    g_set.execShell    = ReadIniBool(L"Exec", L"Shell",    true);
    g_set.execScripts  = ReadIniBool(L"Exec", L"Scripts",  true);
    g_set.confirmSuspicious = ReadIniBool(L"Security", L"ConfirmSuspicious", true);
    g_set.confirmDanger     = ReadIniBool(L"Security", L"ConfirmDanger", true);
    g_set.elevate             = ReadIniBool(L"Security", L"Elevate", true);
    g_set.frecency          = ReadIniBool(L"Type", L"FrecencyRanking", true);
    g_set.execPlugins  = ReadIniBool(L"Exec", L"Plugins",  true);

    // [Update] (v0.20)
    g_set.updApi          = Trim(ReadIniStr(L"Update", L"ApiUrl", L""));
    if (g_set.updApi.empty()) g_set.updApi = kUpdateApiDefault;
    g_set.updAutoRun      = ReadIniBool(L"Update", L"AutoRun", true);
    g_set.updCheckOnStart = ReadIniBool(L"Update", L"CheckOnStart", false);
    g_set.updLatest       = ReadIniStr(L"Update", L"Latest", L"");
    g_set.updLastCheck    = ReadIniStr(L"Update", L"LastCheck", L"");

    g_set.fontName = ReadIniStr(L"Font", L"Name", L"Courier New");
    g_set.fontSize = ReadIniInt(L"Font", L"Size", 18);
    g_set.fontBold = ReadIniInt(L"Font", L"Bold", 1);
    g_set.applyEnglish = ReadIniBool(L"Font", L"ApplyEnglish", false);
    g_set.priority = Lower(Trim(ReadIniStr(L"Show", L"Priority", L"normal")));
    g_set.langCode = Trim(ReadIniStr(L"UI", L"Language", L"auto"));
    g_set.theme    = Trim(ReadIniStr(L"Theme", L"Name", L"cozy-dark"));

    g_set.easyType   = ReadIniInt(L"Type", L"EasyType", 2);
    g_set.fuzzy      = ReadIniInt(L"Type", L"Fuzzy", 1) != 0;
    g_set.layoutFallback = ReadIniInt(L"Type", L"LayoutFallback", 1) != 0;
    g_set.dropDown   = ReadIniInt(L"DropDown", L"Enable", 1) != 0;
    g_set.ddLines    = ReadIniInt(L"DropDown", L"Lines", 8);
    if (g_set.ddLines < 1) g_set.ddLines = 1;
    if (g_set.ddLines > 20) g_set.ddLines = 20;
    g_set.ddOnEmpty  = ReadIniInt(L"DropDown", L"ShowHistoryOnEmpty", 1) != 0;
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

// ------------------------------------------------------------ layout fallback
// v0.32 "native typing" (ROADMAP-V1 #4): a Russian user mid-layout thinks in
// both alphabets - typing `laz` with ЙЦУКЕН on prints `дфя`, and the launch
// should find `laz` anyway. Physical-key map of the two layouts (unshifted
// keys; 33 columns). Returns which conversion makes sense for a word:
//   0 = as typed (mixed scripts / no letters: never guess)
//   1 = pure Cyrillic  -> also match its QWERTY form
//   2 = pure latin     -> also match its ЙЦУКЕН form
static const wchar_t* const kLayoutLat = L"qwertyuiop[]asdfghjkl;'zxcvbnm,./";
static const wchar_t* const kLayoutCyr = L"йцукенгшщзхъфывапролджэячсмитьбю.";

static int LayoutDir(const std::wstring& w) {
    bool cyr = false, lat = false;
    for (wchar_t c : w) {
        if (c >= 0x400 && c <= 0x4FF) cyr = true;
        else if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) lat = true;
        else if (c == L' ' || c == L'\t') return 0;      // multi-word: no guess
    }
    if (cyr && !lat) return 1;
    if (lat && !cyr) return 2;
    return 0;
}

// dir 1 = Cyrillic->Latin, dir 2 = Latin->Cyrillic; unmapped chars pass
// through (digits are shared by both layouts).
static std::wstring LayoutConvert(const std::wstring& w, int dir) {
    std::wstring out = w;
    for (auto& ch : out) {
        wchar_t lc = towlower(ch);
        const wchar_t* from = dir == 1 ? kLayoutCyr : kLayoutLat;
        const wchar_t* to   = dir == 1 ? kLayoutLat : kLayoutCyr;
        const wchar_t* f = wcschr(from, lc);
        if (f && *f) ch = to[(size_t)(f - from)];
    }
    return out;
}

// alias lookup with the layout fallback (exact match in the other layout -
// names are unique, so the hit is unambiguous); used by the Enter chain and
// by the traffic-light anchor so both agree about what will run.
static const Alias* FindAliasLayout(const std::wstring& word) {
    const Alias* a = FindAlias(word);
    if (!a && g_set.layoutFallback && !word.empty() && word[0] != L'~') {
        int dir = LayoutDir(word);
        if (dir) a = FindAlias(LayoutConvert(word, dir));
    }
    return a;
}

// bounded Levenshtein (v0.32 spell-suggest + /checkconfig v2 "maybe you
// meant"). Returns maxDist+1 as soon as the distance cannot get any better,
// so callers just compare with their current best.
static int EditDistCapped(const std::wstring& a, const std::wstring& b, int maxDist) {
    size_t n = a.size(), m = b.size();
    if (n == 0) return (int)m;
    if (m == 0) return (int)n;
    if ((int)(n > m ? n - m : m - n) > maxDist) return maxDist + 1;   // length gate
    std::vector<int> prev(m + 1), cur(m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = (int)j;
    for (size_t i = 1; i <= n; ++i) {
        cur[0] = (int)i;
        int rowMin = cur[0];
        wchar_t ca = towlower(a[i - 1]);
        for (size_t j = 1; j <= m; ++j) {
            int cost = ca == towlower(b[j - 1]) ? 0 : 1;
            int d = prev[j - 1] + cost;
            if (prev[j] + 1 < d) d = prev[j] + 1;
            if (cur[j - 1] + 1 < d) d = cur[j - 1] + 1;
            cur[j] = d;
            if (d < rowMin) rowMin = d;
        }
        if (rowMin > maxDist) return maxDist + 1;
        prev.swap(cur);
    }
    return prev[m];
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

// ---------------------------------------------------------------- usage / frecency (v0.17)
// Usage.dat lines: word<TAB>count<TAB>unix-seconds (word = lower-cased command
// name, never a path or /command). The drop-down ranks fuzzy ties by this:
// the tools you actually use float to the top, quality still dominates.
struct UsageRec { int count = 0; long long last = 0; int day = 0; int today = 0; };
static std::map<std::wstring, UsageRec> g_usage;   // key: lower command word

static std::wstring UsageFile() { return ExeDir() + L"Usage.dat"; }

static void LoadUsage() {
    g_usage.clear();
    std::ifstream f(UsageFile().c_str(), std::ios::binary);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        size_t t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos) continue;
        UsageRec r;
        r.count = atoi(line.substr(t1 + 1, t2 - t1 - 1).c_str());
        size_t t3 = line.find('\t', t2 + 1);
        r.last  = atoll((t3 == std::string::npos ? line : line.substr(t2 + 1, t3 - t2 - 1)).c_str());
        if (t3 != std::string::npos) {                    // v0.27: /day fields
            size_t t4 = line.find('\t', t3 + 1);
            r.day = atoi(line.substr(t3 + 1, (t4 == std::string::npos ? line.size() : t4) - t3 - 1).c_str());
            if (t4 != std::string::npos) r.today = atoi(line.substr(t4 + 1).c_str());
        }
        if (r.count >= 1) g_usage[FromUtf8(line.substr(0, t1))] = r;
    }
}
static void SaveUsage() {
    std::ofstream f(UsageFile().c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return;
    for (auto& kv : g_usage)
        f << ToUtf8(kv.first) << '\t' << kv.second.count << '\t' << kv.second.last
          << '\t' << kv.second.day << '\t' << kv.second.today << '\n';
}
static int LocalYmd() {                        // v0.27: today's YYYYMMDD for /day
    SYSTEMTIME st; GetLocalTime(&st);
    return st.wYear * 10000 + st.wMonth * 100 + st.wDay;
}
static void RecordUsage(const std::wstring& line) {
    std::wstring w = Trim(line);
    size_t sp = w.find_first_of(L" \t");
    if (sp != std::wstring::npos) w = w.substr(0, sp);
    if (w.empty() || w[0] == L'/' || w[0] == L'\\') return;
    if (w.find_first_of(L"\\/:;|<>\"'=%") != std::wstring::npos) return;   // paths/url/vars
    bool wordish = false;
    for (wchar_t ch : w) if (iswalnum(ch)) { wordish = true; break; }
    if (!wordish || w.size() > 32) return;
    UsageRec& r = g_usage[Lower(w)];
    r.count += 1;
    r.last   = (long long)time(nullptr);
    int ymd = LocalYmd();                       // v0.27: per-day tally for /day
    if (r.day != ymd) { r.day = ymd; r.today = 1; }
    else              { r.today += 1; }
    SaveUsage();                              // tiny file; deterministic for tests
}
// 0..17 — smaller than any real fuzzy delta, still decides ties.
static int FrecencyBoost(const std::wstring& row) {
    if (!g_set.frecency) return 0;
    std::wstring w = row;
    size_t sp = w.find_first_of(L" \t");
    if (sp != std::wstring::npos) w = w.substr(0, sp);
    auto it = g_usage.find(Lower(w));
    if (it == g_usage.end()) return 0;
    int b = it->second.count < 5 ? it->second.count * 3 : 15;
    if ((long long)time(nullptr) - it->second.last < 86400) b += 2;
    return b;
}

// ---------------------------------------------------------------- sounds

// The 9 per-execution-type sounds of TypeAndRun, in [Sounds] key order.

// Play the per-execution-type sound (TypeAndRun parity). Paths are absolute or
// relative to the portable TypeBox folder (bundled Sounds\).
static void PlayTypeSound(int idx) {
    if (g_headless) return;                       // silence: no beeps at the user
    if (!g_set.enableSounds || idx < 0 || idx > 8) return;
    std::wstring s = g_set.sound[idx];
    if (s.empty() || _wcsicmp(s.c_str(), L"none") == 0) return;
    std::wstring path = s;
    if (PathIsRelativeW(path.c_str())) path = ExeDir() + path;
    if (PathFileExistsW(path.c_str()))
        PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
}

// ---------------------------------------------------------------- tray

static std::wstring HotkeyText(const Hotkey& hk);   // defined with the settings dialog
static void AddTrayIcon(HWND hWnd, bool balloon) {
    if (!g_set.trayIcon) return;
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hWnd;
    g_nid.uID              = kTrayId;
    g_nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = kTrayMsg;
    g_nid.hIcon            = LoadIconW(nullptr, IDI_APPLICATION);
    std::wstring tip = L"TypeBox " + WVersion() + L" — " + HotkeyText(g_hotkey);
    lstrcpynW(g_nid.szTip, tip.c_str(), ARRAYSIZE(g_nid.szTip));
    if (balloon && g_set.balloonTips && !g_headless) {   // headless: no balloons
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
static void DdClose();   // fwd (defined with the drop-down below)
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
            // v0.24: a plugin may hide the console itself (clip "paste/type at
            // caret" -> bare SW_HIDE so the target keeps the focus). Sync the
            // host's toggle, otherwise the next hotkey press "hides" an
            // already hidden box and the user needs two presses to show it.
            if (g_visible && mainWnd && !IsWindowVisible(mainWnd)) {
                g_visible = false;
                DdClose();
            }
            g_lastPluginName = PluginKey(p);
            LogInfo(L"plugin: " + g_lastPluginName + L" handled: " + line);
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
static int  RunUtilityCommands(const std::wstring& line, HWND mainWnd);   // v0.23
static void ReloadHotkey(HWND mainWnd);
static void LoadThemes();       // v0.15 (/reread uses these before their definitions)
static void ApplyThemeVisual();
static void ResetET();
static void OpenSettings(HWND mainWnd, int page);
enum { PG_APPEAR, PG_WINDOW, PG_FONT, PG_HOTKEY, PG_EXEC, PG_TYPE,
       PG_HISTORY, PG_LOG, PG_ALIASES, PG_SOUNDS, PG_PLUGINS, PG_ENV,
       PG_THEME, PG_LANG, PG_COUNT };

// ---------------------------------------------------------------- update (v0.20)
//
// `/update`: user-initiated release check + install helper (RI3 #298). The
// offline-first rule holds: nothing here touches the network unless the user
// typed /update (or opted into [Update] CheckOnStart). https only, with
// loopback http allowed for mirrors and for the test harness. The downloaded
// zip is sha256-verified against the digest published by the release API
// before anything is staged, and the running exe never replaces itself:
// install.bat waits for our pid to vanish, copies the staged tree (never
// *.ini - settings are the user's) over the app folder and restarts TypeBox.

struct UpdateInfo {
    std::wstring tag, asset, url;
    std::string  digest;             // "sha256:<hex>" when the API publishes one
    long long    size = 0;
};

static std::wstring AscW(const std::string& s) {
    std::wstring w; w.reserve(s.size());
    for (unsigned char c : s) w += (wchar_t)c;
    return w;
}

static bool HttpGet(const std::wstring& url, std::string& body,
                    const std::wstring* toFile, std::wstring& err) {
    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t host[256] = {}, path[2048] = {}, extra[512] = {};
    uc.lpszHostName = host;   uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = path;   uc.dwUrlPathLength  = 2047;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 511;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { err = L"bad url"; return false; }
    bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    std::wstring hostL = Lower(host);
    if (!secure && hostL != L"127.0.0.1" && hostL != L"localhost") {
        err = L"https required (loopback http only)"; return false;
    }
    bool ok = false;
    HINTERNET hs = WinHttpOpen(L"TypeBox update check", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hs) { err = L"winhttp open"; return false; }
    WinHttpSetTimeouts(hs, 8000, 8000, 10000, 15000);
    HINTERNET hc = WinHttpConnect(hs, host, uc.nPort, 0);
    if (hc) {
        std::wstring full = std::wstring(path) + extra;
        HINTERNET hq = WinHttpOpenRequest(hc, L"GET", full.c_str(), nullptr,
                                          WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          secure ? WINHTTP_FLAG_SECURE : 0);
        if (hq) {
            const wchar_t* hdrs =
                L"Accept: application/vnd.github+json\r\n"
                L"X-GitHub-Api-Version: 2022-11-28\r\n";
            WinHttpAddRequestHeaders(hq, hdrs, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hq, nullptr)) {
                DWORD status = 0, sz = sizeof(status);
                WinHttpQueryHeaders(hq,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
                if (status == 200) {
                    HANDLE hf = toFile ? CreateFileW(toFile->c_str(), GENERIC_WRITE, 0,
                                                     nullptr, CREATE_ALWAYS,
                                                     FILE_ATTRIBUTE_NORMAL, nullptr)
                                       : INVALID_HANDLE_VALUE;
                    if (toFile && hf == INVALID_HANDLE_VALUE) {
                        err = L"cannot write " + *toFile;
                    } else {
                        ok = true;
                        for (;;) {
                            DWORD avail = 0;
                            if (!WinHttpQueryDataAvailable(hq, &avail) || !avail) break;
                            char buf[32768];
                            if (avail > sizeof(buf)) avail = sizeof(buf);
                            DWORD rd = 0;
                            if (!WinHttpReadData(hq, buf, avail, &rd) || !rd) break;
                            if (toFile) {
                                DWORD wr = 0;
                                if (!WriteFile(hf, buf, rd, &wr, nullptr) || wr != rd)
                                    { ok = false; break; }
                            } else {
                                body.append(buf, rd);
                                if (body.size() > 16u * 1024 * 1024) { ok = false; break; }
                            }
                        }
                        if (toFile) CloseHandle(hf);
                        if (!toFile && body.empty()) ok = false;
                    }
                } else {
                    wchar_t st[48]; wsprintfW(st, L"http %u", status); err = st;
                }
            } else err = L"send failed";
            WinHttpCloseHandle(hq);
        } else err = L"request";
        WinHttpCloseHandle(hc);
    } else err = L"connect";
    WinHttpCloseHandle(hs);
    return ok;
}

// Minimal JSON field readers (the release payload is our own trust domain:
// GitHub's fixed schema, ASCII in every field we touch).
static bool JsonStr(const std::string& j, const std::string& key, std::string& out) {
    std::string pat = "\"" + key + "\":\"";
    size_t p = j.find(pat);
    if (p == std::string::npos) return false;
    p += pat.size();
    std::string v;
    while (p < j.size()) {
        char c = j[p];
        if (c == '\"') break;
        if (c == '\\' && p + 1 < j.size()) { v += j[p + 1]; p += 2; continue; }
        v += c; ++p;
    }
    out = v;
    return true;
}

static bool JsonLong(const std::string& j, const std::string& key, long long& out) {
    std::string pat = "\"" + key + "\":";
    size_t p = j.find(pat);
    if (p == std::string::npos) return false;
    p += pat.size();
    long long v = 0; bool any = false;
    while (p < j.size() && j[p] >= L'0' && j[p] <= L'9') {
        v = v * 10 + (j[p] - '0'); ++p; any = true;
    }
    out = v; return any;
}

// Top-level objects of "<key>":[ {..}, {..} ] - string-aware brace scan.
static std::vector<std::string> JsonObjects(const std::string& j, const std::string& key) {
    std::vector<std::string> out;
    std::string pat = "\"" + key + "\":[";
    size_t p = j.find(pat);
    if (p == std::string::npos) return out;
    p += pat.size();
    while (p < j.size()) {
        while (p < j.size() && j[p] != '{' && j[p] != ']') ++p;
        if (p >= j.size() || j[p] != '{') break;
        size_t start = p;
        int depth = 0; bool instr = false, esc = false;
        for (; p < j.size(); ++p) {
            char c = j[p];
            if (instr) { if (esc) esc = false; else if (c == '\\') esc = true;
                         else if (c == '\"') instr = false; }
            else if (c == '\"') instr = true;
            else if (c == '{') ++depth;
            else if (c == '}') { if (!--depth) { ++p; break; } }
        }
        out.push_back(j.substr(start, p - start));
    }
    return out;
}

// "0.19.1" / "v9.9.9" / "0.20.0-dev" -> 4 numeric slots for comparison.
static void VerNums(const std::wstring& v, int out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    int slot = 0; long long cur = 0; bool any = false;
    for (size_t i = 0; i <= v.size() && slot < 4; ++i) {
        wchar_t c = (i < v.size()) ? v[i] : L'.';
        if (c >= L'0' && c <= L'9') { cur = cur * 10 + (c - L'0'); any = true; }
        else if (c == L'.' || c == L'-' || c == L'+' || c == L'_') {
            out[slot++] = any ? (int)cur : 0; cur = 0; any = false;
        }
    }
}

static int VerCmp(const std::wstring& a, const std::wstring& b) {
    int va[4], vb[4]; VerNums(a, va); VerNums(b, vb);
    for (int i = 0; i < 4; ++i) if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
    return 0;
}

static bool Sha256File(const std::wstring& file, std::string& hex) {
    bool ok = false;
    BCRYPT_ALG_HANDLE ha = nullptr; BCRYPT_HASH_HANDLE hh = nullptr;
    if (BCryptOpenAlgorithmProvider(&ha, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(ha, &hh, nullptr, 0, nullptr, 0, 0) >= 0) {
        HANDLE hf = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE) {
            ok = true;
            char buf[65536]; DWORD rd = 0;
            while (ReadFile(hf, buf, sizeof(buf), &rd, nullptr) && rd)
                if (BCryptHashData(hh, (PUCHAR)buf, rd, 0) < 0) { ok = false; break; }
            CloseHandle(hf);
            unsigned char dig[32] = {};
            if (ok && BCryptFinishHash(hh, dig, 32, 0) >= 0) {
                static const char* kX = "0123456789abcdef";
                for (int i = 0; i < 32; ++i) { hex += kX[dig[i] >> 4]; hex += kX[dig[i] & 15]; }
            } else ok = false;
        }
        if (hh) BCryptDestroyHash(hh);
    }
    if (ha) BCryptCloseAlgorithmProvider(ha, 0);
    return ok;
}

static void DeleteDirTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    std::wstring pat = dir + L"\\*";
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        std::wstring p = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) DeleteDirTree(p);
        else SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL), DeleteFileW(p.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    RemoveDirectoryW(dir.c_str());
}

static std::wstring UpdateDir() {
    wchar_t tmp[MAX_PATH] = {}; GetTempPathW(MAX_PATH, tmp);
    return std::wstring(tmp) + L"TypeBox-update";
}

static std::wstring LastCheckText() {
    if (g_set.updLastCheck.empty()) return L"never";
    time_t tt = (time_t)_wtoi64(g_set.updLastCheck.c_str());
    struct tm* lt = localtime(&tt);
    if (!lt) return L"?";
    wchar_t buf[64];
    wsprintfW(buf, L"%04d-%02d-%02d %02d:%02d", lt->tm_year + 1900, lt->tm_mon + 1,
              lt->tm_mday, lt->tm_hour, lt->tm_min);
    return buf;
}

static void CacheUpdateResult(const std::wstring& tag) {
    g_set.updLatest = tag;
    g_set.updLastCheck = std::to_wstring((long long)time(nullptr));
    WritePrivateProfileStringW(L"Update", L"Latest", tag.c_str(), g_iniPath.c_str());
    WritePrivateProfileStringW(L"Update", L"LastCheck",
                               g_set.updLastCheck.c_str(), g_iniPath.c_str());
}

// Ask the configured API for the latest release. Returns false on network /
// schema failure (err = reason); on success fills ui and refreshes the cache.
static bool UpdateQuery(std::wstring& err, UpdateInfo& ui) {
    std::string body;
    if (!HttpGet(g_set.updApi, body, nullptr, err)) return false;
    std::string tag;
    if (!JsonStr(body, "tag_name", tag) || tag.empty()) { err = L"no tag_name"; return false; }
    std::wstring wtag = AscW(tag);
    while (!wtag.empty() && (wtag[0] == L'v' || wtag[0] == L'V')) wtag.erase(0, 1);
    ui.tag = wtag;
    for (auto& obj : JsonObjects(body, "assets")) {
        std::string name;
        if (!JsonStr(obj, "name", name)) continue;
        std::wstring wname = Lower(AscW(name));
        if (wname.size() < 4 || wname.substr(wname.size() - 4) != L".zip") continue;
        ui.asset = AscW(name);
        std::string du, dg;
        if (JsonStr(obj, "browser_download_url", du)) ui.url = AscW(du);
        if (JsonStr(obj, "digest", dg)) ui.digest = dg;
        JsonLong(obj, "size", ui.size);
        break;
    }
    if (ui.asset.empty() || ui.url.empty()) { err = L"no zip asset"; return false; }
    CacheUpdateResult(ui.tag);
    return true;
}

static void DoUpdate(HWND mainWnd) {
    UpdateInfo ui;
    std::wstring err;
    if (!UpdateQuery(err, ui)) {
        LogError(L"/update check failed: " + err);   // log first: the box is modal
        wchar_t msg[1024];
        wsprintfW(msg, L(STR_UP_ERR_API), err.c_str(),
                  LastCheckText().c_str(),
                  g_set.updLatest.empty() ? L"?" : g_set.updLatest.c_str());
        TbMessageBox(mainWnd, msg, L"TypeBox — update", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring cur = WVersion();
    if (VerCmp(ui.tag, cur) <= 0) {
        LogInfo(L"/update: already latest " + cur + L" (tag " + ui.tag + L")");
        wchar_t msg[512];
        wsprintfW(msg, L(STR_UP_UPTODATE), cur.c_str(), LastCheckText().c_str());
        TbMessageBox(mainWnd, msg, L"TypeBox — update", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t kb[32];
    wsprintfW(kb, L"%u KB", (unsigned)(ui.size / 1024));   // wsprintfW: no %lld
    LogInfo(L"/update: " + ui.tag + L" available, current " + cur + L", " +
            std::wstring(kb) + L", asset " + ui.asset);
    wchar_t ask[512];
    wsprintfW(ask, L(STR_UP_ASK_DL), ui.tag.c_str(), cur.c_str(), kb);
    if (TbMessageBox(mainWnd, ask, L"TypeBox — update",
                    MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    std::wstring dir = UpdateDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring zip = dir + L"\\" + ui.asset;
    DeleteFileW(zip.c_str());
    {
        std::string unused;
        if (!HttpGet(ui.url, unused, &zip, err)) {
            LogError(L"/update download failed: " + err);   // log before the modal box
            wchar_t msg[1024];
            wsprintfW(msg, L(STR_UP_DL_FAIL), L"download", err.c_str());
            TbMessageBox(mainWnd, msg, L"TypeBox — update", MB_OK | MB_ICONERROR);
            return;
        }
    }
    std::string hex;
    if (!Sha256File(zip, hex)) {
        wchar_t msg[1024];
        wsprintfW(msg, L(STR_UP_DL_FAIL), L"hash", err.c_str());
        TbMessageBox(mainWnd, msg, L"TypeBox — update", MB_OK | MB_ICONERROR);
        return;
    }
    if (!ui.digest.empty()) {
        std::string want = ui.digest;
        if (want.size() > 7 && want.compare(0, 7, "sha256:") == 0) want = want.substr(7);
        for (auto& c : want) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (want != hex) {
            LogError(L"/update digest mismatch: published=" + AscW(want) +
                     L" actual=" + AscW(hex));   // log before the modal box
            TbMessageBox(mainWnd, L(STR_UP_HASH_BAD), L"TypeBox — update",
                        MB_OK | MB_ICONERROR);
            return;
        }
        LogInfo(L"/update: digest verified sha256=" + AscW(hex));
    } else {
        LogInfo(L"/update: no published digest, sha256=" + AscW(hex));
        wchar_t warn[1024];
        wsprintfW(warn, L(STR_UP_HASH_WARN), AscW(hex).c_str());
        if (TbMessageBox(mainWnd, warn, L"TypeBox — update",
                        MB_YESNO | MB_ICONWARNING) != IDYES) return;
    }

    // Stage: unpack the zip, normalize the "TypeBox/" wrapper the packer adds.
    std::wstring stage = dir + L"\\stage";
    DeleteDirTree(stage);
    CreateDirectoryW(stage.c_str(), nullptr);
    bool unpacked = false;
    {
        std::wstring cmd = L"tar.exe -xf \"" + zip + L"\" -C \"" + stage + L"\"";
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 60000);
            DWORD ec = 1; GetExitCodeProcess(pi.hProcess, &ec);
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            unpacked = (ec == 0);
        }
    }
    std::wstring src = stage;
    if (PathFileExistsW((stage + L"\\TypeBox\\TypeBox.exe").c_str()))
        src = stage + L"\\TypeBox";
    if (!unpacked || !PathFileExistsW((src + L"\\TypeBox.exe").c_str())) {
        wchar_t msg[1024];
        wsprintfW(msg, L(STR_UP_DL_FAIL), L"unpack", L"tar");
        TbMessageBox(mainWnd, msg, L"TypeBox — update", MB_OK | MB_ICONERROR);
        return;
    }

    // install.bat: wait for our pid to vanish, mirror the staged tree (no .ini
    // files - those are the user's), restart, remove itself. ASCII only.
    std::wstring bat = dir + L"\\install.bat";
    {
        std::string srcA, appA;
        {
            std::wstring sw = src;
            srcA.assign(sw.begin(), sw.end());
            std::wstring aw = ExeDir();
            appA.assign(aw.begin(), aw.end());
            while (!appA.empty() && appA.back() == '\\') appA.pop_back();
        }
        std::string s;
        s += "@echo off\r\n";
        s += ":waitexit\r\n";
        s += "tasklist /fi \"pid eq %1\" | findstr /r \"[0-9]\" >nul 2>&1 && (\r\n";
        s += "  %SystemRoot%\\System32\\timeout.exe /t 1 /nobreak >nul\r\n";
        s += "  goto :waitexit\r\n";
        s += ")\r\n";
        s += "del /q \"" + srcA + "\\*.ini\" 2>nul\r\n";
        s += "xcopy /e /y /q /i \"" + srcA + "\\*\" \"" + appA + "\\\" >nul\r\n";
        s += "if errorlevel 1 (\r\n";
        s += "  echo TypeBox update: copy failed. Press a key...\r\n";
        s += "  pause >nul\r\n";
        s += ") else (\r\n";
        s += "  start \"\" \"" + appA + "\\TypeBox.exe\"\r\n";
        s += ")\r\n";
        s += "del \"%~f0\"\r\n";
        HANDLE hf = CreateFileW(bat.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            wchar_t msg[1024];
            wsprintfW(msg, L(STR_UP_DL_FAIL), L"bat", bat.c_str());
            TbMessageBox(mainWnd, msg, L"TypeBox — update", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD wr = 0;
        WriteFile(hf, s.data(), (DWORD)s.size(), &wr, nullptr);
        CloseHandle(hf);
    }
    LogInfo(L"/update staged " + ui.tag + L" from " + ui.asset);

    if (!g_set.updAutoRun) {
        wchar_t msg[1024];
        wsprintfW(msg, L(STR_UP_MANUAL), dir.c_str());
        TbMessageBox(mainWnd, msg, L"TypeBox — update", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t askr[512];
    wsprintfW(askr, L(STR_UP_RESTART_ASK), ui.tag.c_str());
    if (TbMessageBox(mainWnd, askr, L"TypeBox — update",
                    MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    ShowWindow(mainWnd, SW_HIDE);
    LogInfo(L"/update: installing via install.bat, pid " + std::to_wstring(GetCurrentProcessId()));
    std::wstring pid = std::to_wstring(GetCurrentProcessId());
    ShellExecuteW(mainWnd, L"open", bat.c_str(), pid.c_str(), dir.c_str(), SW_SHOWNORMAL);
    DestroyWindow(mainWnd);   // WM_DESTROY tears down cleanly; the bat takes over
}

static DWORD WINAPI UpdateCheckThread(LPVOID p) {
    HWND wnd = (HWND)p;
    UpdateInfo ui;
    std::wstring err;
    if (UpdateQuery(err, ui) && VerCmp(ui.tag, WVersion()) > 0)
        PostMessageW(wnd, kUpdateMsg, 0, (LPARAM)new std::wstring(ui.tag));
    return 0;
}

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
            L"Ctrl+R searches your command history: type to filter, Enter runs the match,\n"
            L"another Ctrl+R cycles, Esc restores the line you were typing.\n"
            L"Shift+Enter kills the process started by the last alias.\n\n";
        msg += std::wstring(L(STR_HELP_HEAD)) + L"\n"
            L"/about /checkconfig /config /exit /help /hide /homedir\n"
            L"/inidir /inifiles /load /reread /settings /show /unload\n"
            L"TypeBox additions: /export <file>  /import <file>  /undo  /log  /sys  /openconfig  /path\n"
            L"/day prints today's executions  /tabs url1 url2 ... opens them all in the browser\n"
            L"/update checks GitHub Releases for a newer version (user-initiated only)\n"
            L"Utilities (v0.23): pos right | pos <[Positions] name> | stayawake [min] | stayawake off\n"
            L"Plugin utilities: ts (epoch/ISO converter, ts clip) | slug text [snake|pascal] | sha256 <file> | sha256 /verify SHA256SUMS | cpu ram batt disk sys | clip type <n> | clip ring\n"
            L"/settings opens the settings dialog, /config opens its alias editor.";
        TbMessageBox(mainWnd, msg.c_str(), L"TypeBox — help", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/about") {
        std::wstring msg =
            L"TypeBox " + WVersion() + L"\n\n" +
            std::wstring(L(STR_ABOUT_TEXT)) +
            L"\n\nAuthor / Автор: Dima Shevchuk (D. Shevchuk)"
            L"\nhttps://github.com/shevchukdp/TypeBox"
            L"\nLicense / Лицензия: GPL-3.0";
        TbMessageBox(mainWnd, msg.c_str(), L"About TypeBox", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/checkconfig") {
        // v0.32 "checkconfig v2": the v1 counters plus a real audit - dead
        // aliases (target program cannot be found), duplicate alias names,
        // and a one-line "maybe you meant" fix per dead row. Answer lands in
        // the console (console-first, copyable) AND in the classic dialog.
        bool iniOk = PathFileExistsW(g_iniPath.c_str());
        int dead = 0, dup = 0;
        std::wstring fixes;                       // "  |  DEAD: name -> target"
        // a target resolves when: exists as-is / +.exe, or a PATH hit with a
        // runnable extension, or it is a URL, or a shell host we trust.
        auto targetFound = [&](const std::wstring& action) -> bool {
            std::wstring t = Trim(action);
            if (t.empty()) return false;
            if (LooksLikeUrl(ExpandEnv(t))) return true;
            std::wstring prog;
            if (t[0] == L'"') { size_t q = t.find(L'"', 1); prog = q == std::wstring::npos ? t : t.substr(1, q - 1); }
            else prog = FirstWord(t);
            if (prog.empty()) return false;
            static const wchar_t* const kHosts[] = { L"cmd", L"cmd.exe", L"powershell", L"powershell.exe",
                L"pwsh", L"pwsh.exe", L"wscript", L"wscript.exe", L"cscript", L"cscript.exe",
                L"rundll32", L"rundll32.exe", L"msiexec", L"msiexec.exe", L"explorer", L"explorer.exe",
                L"control", L"control.exe", L"taskkill", L"taskkill.exe" };
            for (auto h : kHosts) if (_wcsicmp(prog.c_str(), h) == 0) return true;
            std::wstring pe = ExpandEnv(prog);
            if (PathFileExistsW(pe.c_str())) return true;
            if (PathFileExistsW((pe + L".exe").c_str())) return true;
            if (pe.find(L'\\') != std::wstring::npos || pe.find(L'/') != std::wstring::npos) return false;
            static const wchar_t* const kExts[] = { L".exe", L".bat", L".cmd", L".com" };
            for (auto& d : g_pathDirs)
                for (auto e : kExts)
                    if (GetFileAttributesW((d + L'\\' + pe + e).c_str()) != INVALID_FILE_ATTRIBUTES)
                        return true;
            return false;
        };
        // fix pool: other alias names, alias targets, plugin command words
        std::vector<std::wstring> pool;
        for (auto& a : g_aliases) pool.push_back(a.name);
        for (auto& p : g_plugins)
            if (p.GetCount && p.GetString)
                for (int i = 0; i < p.GetCount(); ++i) pool.push_back(FromAnsi(p.GetString(i)));
        for (size_t i = 0; i < g_aliases.size(); ++i) {
            const Alias& a = g_aliases[i];
            for (size_t j = i + 1; j < g_aliases.size(); ++j)
                if (_wcsicmp(a.name.c_str(), g_aliases[j].name.c_str()) == 0) ++dup;
            bool ok = targetFound(a.action);
            if (ok) continue;
            ++dead;
            std::wstring line = L"DEAD: " + a.name + L" -> " + (Trim(a.action).empty() ? std::wstring(L"(empty)") : FirstWord(Trim(a.action)));
            int bestD = 3; std::wstring best;
            for (auto& cand : pool) {
                if (_wcsicmp(cand.c_str(), a.name.c_str()) == 0) continue;
                int d = EditDistCapped(cand, a.name, bestD);
                if (d >= 1 && d < bestD) { bestD = d; best = cand; }
            }
            if (!best.empty()) line += L" (maybe '" + best + L"')";
            LogInfo(L"checkconfig: dead alias \"" + a.name + L"\"" +
                    (best.empty() ? L"" : L" - maybe " + best));
            if (fixes.size() < 600) fixes += (fixes.empty() ? L"" : L"  |  ") + line;
        }
        wchar_t head[256];
        wsprintfW(head,
            L"checkconfig: ini %s | aliases %d (%d dead, %d dup) | sys %d | ignores %d | history %d | hotkey %s",
            iniOk ? L"found" : L"MISSING", (int)g_aliases.size(), dead, dup,
            (int)g_sysAliases.size(), (int)g_ignores.size(), (int)g_history.size(),
            g_hotkey.key ? L"yes" : L"no");
        std::wstring report = head;
        if (!fixes.empty()) report += L"  |  " + fixes;
        if (g_edit) {
            SetWindowTextW(g_edit, report.c_str());
            SendMessageW(g_edit, EM_SETSEL, 0, -1);
        }
        std::wstring dlg = report;
        for (auto& ch : dlg) if (ch == L'|') ch = L'\n';
        g_internalAnswer = true;              // keep the report in the console
        TbMessageBox(mainWnd, dlg.c_str(), L"TypeBox — check config", MB_OK | MB_ICONINFORMATION);
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
        TbMessageBox(mainWnd, list.c_str(), L"TypeBox — ini files", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/path") {
        // Show the own PATH folders and %variables% (OS Environment Variables style).
        std::wstring msg;
        for (auto& d : g_pathDirs) msg += d + L"\n";
        if (msg.empty()) msg = L"(no [Path] folders)\n";
        msg += L"\nVariables:\n";
        for (auto& v : g_envVars) msg += v.name + L" = " + v.value + L"\n";
        if (g_envVars.empty()) msg += L"(no [Variables])\n";
        TbMessageBox(mainWnd, msg.c_str(), L"TypeBox — own path & variables",
                    MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/reread") {
        LoadSettings();
        LoadAliases();
        LoadWebSearch();
        LoadScripts();              // v0.16: Scripts\ picked up live too
        ReloadHotkey(mainWnd);
        LoadThemes();               // v0.15: themes\*.theme.ini picked up too
        ApplyThemeVisual();
        if (mainWnd) { InvalidateRect(mainWnd, nullptr, TRUE);
                       RECT rc; GetClientRect(mainWnd, &rc);
                       SendMessageW(mainWnd, WM_SIZE, 0,
                                    MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top)); }
    } else if (c == L"/load") {
        // (Re)scan Plugins\ and [Plugins]; call Load(HWND) on each.
        size_t before = g_plugins.size();
        CallPluginsUnload();
        DiscoverPlugins();
        CallPluginsLoad(mainWnd);
        std::wstring msg = L"Plugins loaded: " + std::to_wstring(g_plugins.size());
        if (g_plugins.empty() && before == 0)
            msg += L"\n(dlls in the portable Plugins\\ folder or [Plugins] entries are picked up)";
        TbMessageBox(mainWnd, msg.c_str(), L"TypeBox — plugins", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/unload") {
        CallPluginsUnload();
    } else if (c == L"/export") {
        // Export settings: copy the portable ini to the given file.
        std::wstring dest = RestWords(line);
        if (dest.empty()) {
            TbMessageBox(mainWnd, L"Usage: /export <path-to-file>",
                        L"TypeBox — export", MB_OK | MB_ICONINFORMATION);
        } else if (CopyFileW(g_iniPath.c_str(), dest.c_str(), FALSE)) {
            wchar_t msg[512];
            wsprintfW(msg, L"Settings exported to:\n%s", dest.c_str());
            TbMessageBox(mainWnd, msg, L"TypeBox — export", MB_OK | MB_ICONINFORMATION);
        } else {
            TbMessageBox(mainWnd, L"TypeBox: export failed (check the path).",
                        L"TypeBox — export", MB_OK | MB_ICONERROR);
            LogError(L"/export failed to: " + dest);
        }
    } else if (c == L"/import") {
        // Import settings: restore an exported file (current ini is kept as .bak).
        std::wstring src = RestWords(line);
        if (src.empty() || !PathFileExistsW(src.c_str())) {
            TbMessageBox(mainWnd, L"Usage: /import <path-to-file>\n(file not found)",
                        L"TypeBox — import", MB_OK | MB_ICONINFORMATION);
        } else {
            std::wstring bak = g_iniPath + L".bak";
            CopyFileW(g_iniPath.c_str(), bak.c_str(), FALSE);
            if (CopyFileW(src.c_str(), g_iniPath.c_str(), FALSE)) {
                LoadSettings();
                LoadAliases();
                LoadWebSearch();
                LoadScripts();
                ReloadHotkey(mainWnd);
                TbMessageBox(mainWnd,
                    L"Settings imported. Previous config kept as TypeBox.ini.bak",
                    L"TypeBox — import", MB_OK | MB_ICONINFORMATION);
            } else {
                TbMessageBox(mainWnd, L"TypeBox: import failed.",
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
        if (g_nid.hWnd && !g_headless) {
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
            TbMessageBox(mainWnd, L"Log is empty or not configured ([Log] File).",
                        L"TypeBox — log", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/update") {
        DoUpdate(mainWnd);            // v0.20: release check + install helper
    } else if (c == L"/day") {
        // v0.27 (RI3 #40): today's executions, grouped, from Usage.dat.
        int ymd = LocalYmd();
        int cmds = 0, runs = 0;
        long long lastAny = 0;
        std::vector<std::pair<int, std::wstring>> v;
        for (auto& kv : g_usage)
            if (kv.second.day == ymd && kv.second.today > 0) {
                cmds++; runs += kv.second.today;
                v.push_back(std::make_pair(kv.second.today, kv.first));
                if (kv.second.last > lastAny) lastAny = kv.second.last;
            }
        std::wstring ans;
        if (v.empty()) ans = L(STR_DAY_NONE);
        else {
            std::sort(v.begin(), v.end(),
                      [](const std::pair<int, std::wstring>& a,
                         const std::pair<int, std::wstring>& b) { return a.first > b.first; });
            wchar_t buf[96];
            wsprintfW(buf, L"day: %d command%s, %d runs today | ",
                      cmds, cmds == 1 ? L"" : L"s", runs);
            ans = buf;
            int shown = 0;
            for (auto& p : v) {
                if (shown >= 8) { ans += L", ..."; break; }
                if (shown) ans += L", ";
                ans += p.second + L" " + std::to_wstring(p.first);
                ++shown;
            }
            if (lastAny) {
                time_t t = (time_t)lastAny; struct tm tb;
                if (localtime_s(&tb, &t) == 0) {
                    wsprintfW(buf, L" | last %02d:%02d", tb.tm_hour, tb.tm_min);
                    ans += buf;
                }
            }
        }
        LogInfo(L"/day: " + ans);
        SetWindowTextW(g_edit, ans.c_str());
        SendMessageW(g_edit, EM_SETSEL, 0, -1);
        g_internalAnswer = true;
    } else if (c == L"/tabs") {
        // v0.27 (RI3 #10): one Enter, a whole page of tabs (batch opener).
        size_t sp0 = line.find_first_of(L" \t");
        std::wstring rest = Trim(sp0 == std::wstring::npos ? L"" : line.substr(sp0 + 1));
        std::vector<std::wstring> urls;
        size_t i = 0;
        while (i < rest.size() && (int)urls.size() < 10) {
            while (i < rest.size() && (rest[i] == L' ' || rest[i] == L'\t')) ++i;
            size_t s = i;
            while (i < rest.size() && rest[i] != L' ' && rest[i] != L'\t') ++i;
            if (i > s) urls.push_back(rest.substr(s, i - s));
        }
        if (urls.empty()) {
            SetWindowTextW(g_edit, L(STR_TABS_NONE));
            SendMessageW(g_edit, EM_SETSEL, 0, -1);
            g_internalAnswer = true;
        } else {
            int opened = 0;
            for (auto& u0 : urls) {
                std::wstring u = u0;
                if (u.find(L"://") == std::wstring::npos) u = L"https://" + u;   // bare host gets a scheme
                if ((INT_PTR)ShellExecuteW(mainWnd, L"open", u.c_str(), nullptr, nullptr,
                                           g_headless ? SW_SHOWMINNOACTIVE
                                                      : SW_SHOWNORMAL) > 32) {
                    ++opened;
                    LogInfo(L"tabs: opened " + u);
                } else {
                    LogError(L"tabs: failed " + u);
                }
            }
            wchar_t ans[64];
            wsprintfW(ans, L(STR_TABS_OPENED), opened);
            SetWindowTextW(g_edit, ans);
            SendMessageW(g_edit, EM_SETSEL, 0, -1);
            PlayTypeSound(opened ? 8 : 2);
            g_internalAnswer = true;
        }
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

// v0.18: `su notepad` / `/su regedit` / `sudo netstat` run the resolved target
// elevated (ShellExecute "runas" → UAC). Submit strips the prefix, raises the
// one-shot flag and hands the rest of the line to the normal chain; the chip
// and the anchor classify the stripped command with an "admin" label.
// URLs/emails/quicklinks deliberately stay at user level — runas on a mailto
// handler is meaningless.
static bool g_elevateNext = false;

static bool SuSplit(const std::wstring& line, std::wstring& rest) {
    std::wstring t = Trim(line);
    if (t.empty()) return false;
    size_t i = 0;
    while (i < t.size() && t[i] != L' ' && t[i] != L'\t') ++i;
    std::wstring w = Lower(t.substr(0, i));
    if (w != L"su" && w != L"sudo" && w != L"/su" && w != L"/sudo") return false;
    rest = Trim(t.substr(i));
    return true;
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

    // v0.23: reserved utility words run ahead of aliases (RI3 #226 pos,
    // #230 stayawake; ts/slug are console-first plugin commands in util.dll).
    if (g_set.execInternal) {
        int u = RunUtilityCommands(line, mainWnd);
        if (u != EXC_NONE) return u;
    }

    // 1) alias from [Aliases] (TypeBox.ini or legacy Config.ini). v0.32: the
    // layout fallback may resolve a wrong-layout alias name (exact hit only).
    if (g_set.execAliases) {
        if (const Alias* a = FindAliasLayout(word)) {
            std::wstring cmd = ExpandEnv(Substitute(a->action, rest));
            // v0.17: an alias that expands to a wiper is still a wiper.
            if (g_set.confirmDanger && LooksDangerous(cmd) &&
                !ConfirmDangerLine(cmd, mainWnd)) return EXC_NONE;
            std::wstring prog = FirstWord(cmd);
            std::wstring args = RestWords(cmd);
            // ShellExecuteEx with NOCLOSEPROCESS so Shift+Enter can kill the app;
            // FLAG_NO_UI: never pop the system "file not found" dialog — we log it.
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask       = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
            sei.hwnd        = mainWnd;
            sei.lpVerb      = g_elevateNext ? L"runas" : L"open";   // v0.18 su
            sei.lpFile      = prog.c_str();
            sei.lpParameters = args.empty() ? nullptr : args.c_str();
            sei.lpDirectory = a->dir.empty() ? nullptr : a->dir.c_str();
            sei.nShow       = a->state;
            if (ShellExecuteExW(&sei)) {
                if (g_aliasProc) CloseHandle(g_aliasProc);
                g_aliasProc = sei.hProcess;   // may be null for shell verb relaunches
                g_aliasName = a->name;
                LogInfo(L"alias started: " + a->name +
                        (_wcsicmp(a->name.c_str(), word.c_str()) != 0
                             ? L" (layout: " + word + L")" : L"") +
                        L" (handle " +
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
            sei.lpVerb       = g_elevateNext ? L"runas" : L"open";   // v0.18 su
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

    // 2b) web-search quicklinks: "g some query" -> default browser ([WebSearch])
    if (!g_webLinks.empty() && !rest.empty()) {
        for (auto& wl : g_webLinks) {
            if (_wcsicmp(word.c_str(), wl.name.c_str()) != 0) continue;
            std::wstring url = wl.tmpl;
            size_t pos = url.find(L"%s");
            std::wstring q = WebEncode(rest);
            if (pos != std::wstring::npos) url.replace(pos, 2, q); else url += q;
            LogInfo(L"websearch: " + word + L" -> " + url);
            if (!g_set.execWww) { LogInfo(L"websearch: blocked by [Exec] WWW=0"); return EXC_NONE; }
            ShellExecuteW(mainWnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            PlayTypeSound(3);
            return EXC_URL;
        }
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
            ShellExecuteW(mainWnd, g_elevateNext ? L"runas" : L"open",
                          expanded.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
                sei.lpVerb       = g_elevateNext ? L"runas" : L"open";   // v0.18 su
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

    // 4c) Scripts\ folder (v0.16, RI2 #159): a script file name is a command.
    if (g_set.execScripts && !word.empty() && !g_scripts.empty() &&
        word.find(L'\\') == std::wstring::npos && word.find(L'/') == std::wstring::npos) {
        if (const ScriptCmd* sc = FindScript(word)) {
            if (g_set.confirmSuspicious && LooksSuspicious(rest)) {
                std::wstring msg = L"TypeBox \u2014 security check\n\nThe arguments to your script look like an encoded payload:\n\n"
                                   + rest + L"\n\nRun it anyway?";
                if (TbMessageBox(mainWnd, msg.c_str(), L"TypeBox",
                                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
                    LogInfo(L"security: blocked suspicious args for script: " + word);
                    return EXC_NONE;
                }
            }
            // v0.17: destructive arguments to a script are just as dangerous.
            if (g_set.confirmDanger && LooksDangerous(rest) &&
                !ConfirmDangerLine(rest, mainWnd)) return EXC_NONE;
            std::wstring file, args;
            if (sc->ext == L".ps1") {
                file = L"powershell.exe";
                args = L"-NoProfile -ExecutionPolicy Bypass -File \"" + sc->path + L"\"";
                if (!rest.empty()) args += L" " + rest;
            } else if (sc->ext == L".bat" || sc->ext == L".cmd") {
                file = L"cmd.exe";
                args = L"/c \"" + sc->path + L"\"";
                if (!rest.empty()) args += L" " + rest;
            } else if (sc->ext == L".py") {
                file = L"cmd.exe";
                args = L"/c py \"" + sc->path + L"\"";
                if (!rest.empty()) args += L" " + rest;
            } else {                       // .vbs/.vbe: shell association (wscript)
                file = sc->path;
                args = rest;
            }
            HINSTANCE r = ShellExecuteW(mainWnd, g_elevateNext ? L"runas" : L"open",
                                        file.c_str(),
                                        args.empty() ? nullptr : args.c_str(),
                                        nullptr, SW_SHOWNORMAL);
            if ((INT_PTR)r > 32) {
                LogInfo(L"script started: " + word + L" (" + sc->path + L")");
                PlayTypeSound(8);
                return EXC_SHELL;
            }
            LogError(L"script start failed: " + sc->path +
                     L" (ShellExecute=" + std::to_wstring((INT_PTR)r) + L")");
            return EXC_NONE;
        }
    }

    // 5) shell mode ("bash-ish"): cmd.exe /c <line>
    if (g_set.execShell) {
        // v0.16 (RI2 #168): the only safe Run box — confirm suspicious payloads.
        if (g_set.confirmSuspicious && LooksSuspicious(expanded)) {
            std::wstring msg = L"TypeBox \u2014 security check\n\nThis command looks like an encoded payload or fileless one-liner:\n\n"
                               + expanded + L"\n\nRun it anyway?";
            if (TbMessageBox(mainWnd, msg.c_str(), L"TypeBox",
                            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
                LogInfo(L"security: blocked suspicious command");
                return EXC_NONE;
            }
        }
        // v0.17: destructive commands (del/format/shutdown/reg delete …) confirm too.
        if (g_set.confirmDanger && LooksDangerous(expanded) &&
            !ConfirmDangerLine(expanded, mainWnd)) return EXC_NONE;
        std::wstring shellCmd  = ReadIniStr(L"Shell", L"Command", L"cmd.exe");
        std::wstring shellArgs = ReadIniStr(L"Shell", L"Args", L"/c");
        ShellExecuteW(mainWnd, g_elevateNext ? L"runas" : L"open", shellCmd.c_str(),
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
    L"/about", L"/checkconfig", L"/config", L"/day", L"/exit", L"/export", L"/help",
    L"/hide", L"/homedir", L"/import", L"/inidir", L"/inifiles", L"/load",
    L"/log", L"/openconfig", L"/path", L"/reread", L"/settings", L"/show",
    L"/sys", L"/tabs", L"/unload", L"/undo", L"/update" };

struct Anchor { int lamp = -1; int labelId = -1; };   // -1 = lights off
static Anchor g_anchor;

static Anchor ClassifyAnchor(const std::wstring& lineRaw) {
    Anchor a;
    std::wstring line = Trim(lineRaw);
    if (line.empty()) return a;

    // v0.18: su prefix — classify what actually runs, chip says "admin".
    {
        std::wstring suRest;
        if (SuSplit(line, suRest)) {
            if (suRest.empty()) { a.lamp = 0; a.labelId = STR_SO_ADMIN; return a; }
            Anchor b = ClassifyAnchor(suRest);
            if (b.labelId >= 0) b.labelId = STR_SO_ADMIN;
            return b;
        }
    }

    if (line[0] == L'/') {
        if (!g_set.execInternal) { a.lamp = 2; a.labelId = STR_AN_OFF; return a; }
        std::wstring w = FirstWord(line);
        for (auto c : kInternalCmds)
            if (_wcsicmp(w.c_str(), c) == 0) { a.lamp = 0; a.labelId = STR_SO_INTERNAL; return a; }
        a.lamp = 2; a.labelId = STR_AN_UNKNOWN; return a;
    }

    std::wstring word = FirstWord(line);
    if (g_set.execAliases) {
        if (word[0] != L'~' && FindAliasLayout(word)) { a.lamp = 0; a.labelId = STR_SO_ALIAS; return a; }
        if (word[0] == L'~' && FindSysAlias(word)) { a.lamp = 0; a.labelId = STR_SO_SYS; return a; }
    }

    if (g_set.execPlugins)
        for (auto& p : g_plugins) {
            if (!p.GetCount || !p.GetString || !PluginLoadAllowed(p)) continue;
            for (int i = 0; i < p.GetCount(); ++i)
                if (_wcsicmp(word.c_str(), FromAnsi(p.GetString(i)).c_str()) == 0)
                    { a.lamp = 1; a.labelId = STR_SO_PLUGIN; return a; }
        }

    if (!g_webLinks.empty() && !RestWords(line).empty())   // quicklink -> amber
        for (auto& wl : g_webLinks)
            if (_wcsicmp(word.c_str(), wl.name.c_str()) == 0)
                { a.lamp = 1; a.labelId = STR_AN_WEB; return a; }

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
    if (g_set.execScripts && !word.empty() && !g_scripts.empty() &&
        word.find(L'\\') == std::wstring::npos && word.find(L'/') == std::wstring::npos &&
        FindScript(word)) { a.lamp = 2; a.labelId = STR_SO_SCRIPT; return a; }
    if (g_set.execShell) { a.lamp = 2; a.labelId = STR_SO_SHELL; return a; }
    a.lamp = 2; a.labelId = STR_AN_UNKNOWN; return a;
}

// ---------------------------------------------------------------- Per-Monitor V2 DPI (v0.29)
// The contract (docs/CONFIG.md): every pixel value that lives in the INI -
// [Show] Width/Height, [Positions] presets, [Font] Size, the 20px strip - is
// 96-DPI LOGICAL. Each top-level window keeps its own dpi, initialized from
// GetDpiForWindow and updated from WM_DPICHANGED's wParam (the message, not
// a re-query, drives the number - which is also how the suites exercise the
// scaling math on a 96-DPI machine). On Win7/8/8.1 PMv2 is unavailable, the
// dpi stays 96 and every Sc() below is the identity: old behavior preserved.
static int g_mainDpi = 96, g_ddDpi = 96, g_setDpi = 96;
static const wchar_t* g_dpiMode = L"system aware";
typedef BOOL (WINAPI* SetDpiCtxFn)(HANDLE);        // DPI_AWARENESS_CONTEXT is a HANDLE
typedef UINT (WINAPI* GetDpiForWindow_t)(HWND);
static GetDpiForWindow_t pGetDpiForWindow = nullptr;
static void DpiInit() {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (u) pGetDpiForWindow = (GetDpiForWindow_t)GetProcAddress(u, "GetDpiForWindow");
}
static int WindowDpi(HWND h) {
    if (pGetDpiForWindow && h) { UINT d = pGetDpiForWindow(h); if (d >= 96 && d <= 480) return (int)d; }
    HDC dc = GetDC(h);
    int d = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(h, dc);
    return (d >= 96 && d <= 480) ? d : 96;
}
static int Sc(int dpi, int v) { return MulDiv(v, dpi >= 96 ? dpi : 96, 96); }

// ---------------------------------------------------------------- UI

static void ApplyFont(HWND edit) {
    HWND root = GetAncestor(edit, GA_ROOT);
    int d = (edit == g_edit) ? g_mainDpi : (root ? WindowDpi(root) : 96);
    HFONT f = CreateFontW(-MulDiv(MulDiv(g_set.fontSize, 96, 72), d, 96), 0, 0, 0,
        g_set.fontBold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN,
        ConsoleFontSuggestion(g_set.fontName.c_str()));
    SendMessageW(edit, WM_SETFONT, (WPARAM)f, TRUE);
    if (edit == g_edit && GetParent(edit)) {   // font changed -> redo layout
        RECT rc; GetClientRect(GetParent(edit), &rc);
        SendMessageW(GetParent(edit), WM_SIZE, 0,
                     MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
    }
}

static void SetTrayVisual();
static void DdClose();
static void DdUpdate(HWND mainWnd);

// v0.19: mouse-dragged position of the console. MEMORY ONLY - never written to
// the INI, so a restart always returns the window to its [Show] X/Y (or centered)
// home position, which is exactly what the "return to original on restart" ask means.
static int  g_dragX = -1, g_dragY = -1;
static bool g_dragged = false;
// v0.27: once `pos` has deliberately repositioned the console (headless suites
// depend on the geometry), a re-show must keep that spot, not re-park at -32000.
static bool g_posMoved = false;
// v0.27.1: the active `pos` slot keeps ITS OWN geometry (right/bottom are w/h,
// not edges). The old code wrote the slot straight into g_set, so a one-line
// console stayed "spread out" at half-screen until restart; and in headless
// mode the slot landed on the HUMAN's work area during test runs. [Show] now
// stays the restore source, `pos reset` releases the slot, and headless
// applies the rect shifted -32000px off both axes.
static RECT g_posRect = {};

static void ShowBox(HWND mainWnd) {
    // v0.29: [Show] px are 96-logical - scale them to THIS window's dpi
    const int lw = Sc(g_mainDpi, g_set.width), lh = Sc(g_mainDpi, g_set.height);
    RECT wr; GetWindowRect(GetDesktopWindow(), &wr);
    int x, y;
    if (g_set.x >= 0) x = g_set.x;
    else              x = (wr.right - lw) / 2;
    if (g_set.y >= 0 && !g_set.centerY) y = g_set.y;
    else y = g_set.centerY ? (wr.bottom - lh) / 3 : 100;
    if (!g_set.centerX) x = 0;
    if (g_set.dragMove && g_dragged) { x = g_dragX; y = g_dragY; }
    // v0.27.1: an active `pos` slot outranks everything (position AND size);
    // in headless mode the same rect is applied -32000 off both axes so the
    // human never sees it. Otherwise: [Show] geometry / the off-screen park.
    int ww = lw, hh = lh;
    if (g_posMoved) { x = g_posRect.left; y = g_posRect.top;
                      ww = g_posRect.right; hh = g_posRect.bottom; }

    if (g_headless) {
        // off-screen and non-activating: suites drive it by posted messages,
        // the human at the keyboard never sees it nor loses focus.
        int px = -32000, py = -32000;
        if (g_posMoved) { px = x - 32000; py = y - 32000; }
        SetWindowPos(mainWnd, nullptr, px, py, ww, hh,
                     SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
    } else {
        SetWindowPos(mainWnd, HWND_TOPMOST, x, y, ww, hh, SWP_SHOWWINDOW);
    }
    if (g_set.transparent < 255) {
        SetWindowLongW(mainWnd, GWL_EXSTYLE,
                       GetWindowLongW(mainWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(mainWnd, 0, (BYTE)g_set.transparent, LWA_ALPHA);
    }
    if (!g_headless) {
        SetForegroundWindow(mainWnd);
        SetFocus(g_edit);
        if (g_set.applyEnglish) // [Font] ApplyEnglish: type commands in EN layout (TypeAndRun)
            ActivateKeyboardLayout((HKL)MAKELONG(0x409, 0x0409), 0);
    }
    SendMessageW(g_edit, EM_SETSEL, 0, -1);
    g_visible = true;
    g_histPos = g_history.size();
    g_hsrch = false;                          // v0.27: every summon starts clean
    g_hsrchSaved.clear();
    ResetET();
    g_anchor = Anchor{};             // lights off until the first keystroke
    SetTrayVisual();
    DdUpdate(mainWnd);               // empty line -> history (parity)
    if (g_set.autoHide && g_set.autoHideValue > 0)
        SetTimer(mainWnd, 1, (UINT)g_set.autoHideValue * 1000, nullptr);
}

static void HideBox(HWND mainWnd) {
    ShowWindow(mainWnd, SW_HIDE);
    SetWindowTextW(g_edit, L"");
    g_visible = false;
    g_hsrch = false;                          // v0.27: mode never survives a hide
    g_hsrchSaved.clear();
    KillTimer(mainWnd, 1);
    ResetET();
    SetTrayVisual();
    DdClose();
}

// ---------------------------------------------------------------- utilities (v0.23)
//
// RI3 #226 `pos` + #230 `stayawake`. Host-side on purpose: ShowBox re-lays
// the window out from g_set on every show, so a plugin SetWindowPos would be
// undone by the next hotkey - pos updates g_set (memory only) so the spot
// survives hide/show for the session while a restart still returns the
// console to its INI home position. stayawake lives as long as the host
// process does: the thread execution state dies with the process, and the
// optional [min] expiry rides a callback timer on the main window.
// ts/slug (RI3 #250/#251) are console-first commands in util.dll instead.

#ifndef ES_CONTINUOUS
#define ES_CONTINUOUS        0x80000000
#define ES_SYSTEM_REQUIRED   0x00000001
#define ES_DISPLAY_REQUIRED  0x00000002
extern "C" DWORD WINAPI SetThreadExecutionState(DWORD esFlags);
#endif

static UINT_PTR g_awakeTimer = 0;

static VOID CALLBACK AwakeExpireCb(HWND h, UINT m, UINT_PTR id, DWORD t) {
    (void)m; (void)t;
    SetThreadExecutionState(ES_CONTINUOUS);
    KillTimer(h, id);
    g_awakeTimer = 0;
    LogInfo(L"stayawake: expired");
}

static void ShowAnswer(HWND mainWnd, const std::wstring& text) {
    (void)mainWnd;
    SetWindowTextW(g_edit, text.c_str());
    SendMessageW(g_edit, EM_SETSEL, 0, -1);
}

static bool ApplyPosRect(HWND w, int x, int y, int ww, int hh) {
    if (ww < 40 || hh < 20) return false;
    // v0.27.1: the slot is computed EXACTLY as the human would get it, but in
    // headless mode it is applied 32000px off both axes - suites must never
    // surface a window on the human's screen (that is what "the window keeps
    // spreading out during tests" really was). g_set stays untouched.
    g_posRect.left = x;  g_posRect.top = y;
    g_posRect.right = ww; g_posRect.bottom = hh;   // w/h, not edges
    int ax = g_headless ? x - 32000 : x;
    int ay = g_headless ? y - 32000 : y;
    if (!SetWindowPos(w, NULL, ax, ay, ww, hh,
                      SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW)) return false;
    return true;
}

// FancyZones-flavoured vocabulary on the CURRENT monitor's work area.
// `dpi` scales the two INI-fed geometries (the `center` slot reuses the
// 96-logical [Show] size); every other slot is pure work-area math.
static bool SlotRect(const RECT& wa, const wchar_t* slot, int dpi,
                     int* x, int* y, int* w, int* h) {
    int Lp = wa.left, T = wa.top;
    int W = wa.right - wa.left, H = wa.bottom - wa.top;
    if (!_wcsicmp(slot, L"left"))         { *x=Lp;       *y=T;      *w=W/2;     *h=H;     }
    else if (!_wcsicmp(slot, L"right"))   { *x=Lp+W/2;   *y=T;      *w=W-W/2;   *h=H;     }
    else if (!_wcsicmp(slot, L"top"))     { *x=Lp;       *y=T;      *w=W;       *h=H/2;   }
    else if (!_wcsicmp(slot, L"bottom"))  { *x=Lp;       *y=T+H/2;  *w=W;       *h=H-H/2; }
    else if (!_wcsicmp(slot, L"tl"))      { *x=Lp;       *y=T;      *w=W/2;     *h=H/2;   }
    else if (!_wcsicmp(slot, L"tr"))      { *x=Lp+W/2;   *y=T;      *w=W-W/2;   *h=H/2;   }
    else if (!_wcsicmp(slot, L"bl"))      { *x=Lp;       *y=T+H/2;  *w=W/2;     *h=H-H/2; }
    else if (!_wcsicmp(slot, L"br"))      { *x=Lp+W/2;   *y=T+H/2;  *w=W-W/2;   *h=H-H/2; }
    else if (!_wcsicmp(slot, L"center"))  { *w=Sc(dpi, g_set.width); *h=Sc(dpi, g_set.height);
                                           *x=Lp+(W-*w)/2; *y=T+(H-*h)/2; }
    else if (slot[0]==L'q' && slot[1]>=L'1' && slot[1]<=L'4' && !slot[1+1]) {
        int n = slot[1]-L'1'; *w = W/4; if (n==3) *w = W-3*(W/4);
        *x = Lp+n*(W/4); *y = T; *h = H;
    }
    else return false;
    return true;
}

static BOOL CALLBACK MonWorkCb(HMONITOR m, HDC, LPRECT, LPARAM l) {
    MONITORINFO mi; mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(m, &mi)) (*reinterpret_cast<std::vector<RECT>*>(l)).push_back(mi.rcWork);
    return TRUE;
}

static int ParseIntsCSV(wchar_t* s, int* out, int max) {
    int n = 0;
    while (*s && n < max) {
        while (*s == L' ' || *s == L',') ++s;
        if (!*s) break;
        bool neg = (*s == L'-'); if (neg || *s == L'+') ++s;
        if (*s < L'0' || *s > L'9') return -1;
        long v = 0;
        while (*s >= L'0' && *s <= L'9') { v = v*10 + (*s-L'0'); ++s; }
        out[n++] = (int)(neg ? -v : v);
    }
    return n;
}

static int RunUtilityCommands(const std::wstring& line, HWND mainWnd) {
    std::wstring w = Lower(FirstWord(line));
    std::wstring rest = Trim(RestWords(line));

    if (w == L"pos") {
        MONITORINFO mi; mi.cbSize = sizeof(mi);
        // v0.27 headless: the parked window sits at -32000, which would make
        // TONEAREST pick some far-off (often left-extended) monitor. The user's
        // pos contract is the PRIMARY work area, so pin the geometry there.
        HMONITOR hm = g_headless
            ? MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY)
            : MonitorFromWindow(mainWnd, MONITOR_DEFAULTTONEAREST);
        if (!hm || !GetMonitorInfoW(hm, &mi)) return EXC_NONE;
        if (rest.empty()) { ShowAnswer(mainWnd, L(STR_PO_USAGE)); return EXC_PLUGIN; }
        if (!_wcsicmp(rest.c_str(), L"reset")) {   // v0.27.1: release the slot
            g_posMoved = false;
            ShowAnswer(mainWnd, L"pos: back to [Show] geometry");
            LogInfo(L"pos: reset to [Show]");
            return EXC_PLUGIN;
        }
        int x, y, ww, hh;
        if (SlotRect(mi.rcWork, rest.c_str(), g_mainDpi, &x, &y, &ww, &hh)) {
            if (!ApplyPosRect(mainWnd, x, y, ww, hh))
                { ShowAnswer(mainWnd, L"pos: SetWindowPos failed"); return EXC_PLUGIN; }
            g_posMoved = true;
            wchar_t msg[160];
            wsprintfW(msg, L"pos: %s -> %d,%d %dx%d", rest.c_str(), x, y, ww, hh);
            LogInfo(L"pos: " + rest + L" applied");
            ShowAnswer(mainWnd, msg);
            return EXC_PLUGIN;
        }
        wchar_t val[128] = {};
        GetPrivateProfileStringW(L"Positions", rest.c_str(), L"", val, 127, g_iniPath.c_str());
        if (!val[0]) {
            wchar_t msg[256];
            wsprintfW(msg, L(STR_PO_NOPRESET), rest.c_str());
            ShowAnswer(mainWnd, msg);
            return EXC_PLUGIN;
        }
        int p[5] = {}, np = ParseIntsCSV(val, p, 5);
        if (np < 4) { ShowAnswer(mainWnd, L"pos: preset must be x,y,w,h[,mon]"); return EXC_PLUGIN; }
        RECT wa = mi.rcWork;
        if (np >= 5) {
            std::vector<RECT> mons;
            EnumDisplayMonitors(NULL, NULL, MonWorkCb, (LPARAM)&mons);
            if (!mons.empty()) wa = mons[p[4] % (int)mons.size()];
        }
        // [Positions] presets are 96-logical too (v0.29): scale x/y/w/h
        int sx = Sc(g_mainDpi, p[0]), sy = Sc(g_mainDpi, p[1]);
        int sw = Sc(g_mainDpi, p[2]), sh = Sc(g_mainDpi, p[3]);
        if (!ApplyPosRect(mainWnd, wa.left+sx, wa.top+sy, sw, sh))
            { ShowAnswer(mainWnd, L"pos: SetWindowPos failed"); return EXC_PLUGIN; }
        g_posMoved = true;
        wchar_t msg[160];
        wsprintfW(msg, L"pos: %s -> %d,%d %dx%d", rest.c_str(), wa.left+sx, wa.top+sy, sw, sh);
        LogInfo(L"pos: preset " + rest + L" applied");
        ShowAnswer(mainWnd, msg);
        return EXC_PLUGIN;
    }

    if (w == L"stayawake") {
        if (rest.empty()) {
            if (g_awakeTimer) { KillTimer(mainWnd, g_awakeTimer); g_awakeTimer = 0; }
            SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
            LogInfo(L"stayawake: on (indefinite)");
            ShowAnswer(mainWnd, L(STR_AW_ON));
            return EXC_PLUGIN;
        }
        if (_wcsicmp(rest.c_str(), L"off") == 0) {
            if (g_awakeTimer) { KillTimer(mainWnd, g_awakeTimer); g_awakeTimer = 0; }
            SetThreadExecutionState(ES_CONTINUOUS);
            LogInfo(L"stayawake: off");
            ShowAnswer(mainWnd, L(STR_AW_OFF));
            return EXC_PLUGIN;
        }
        const wchar_t* s = rest.c_str();
        if (*s < L'0' || *s > L'9') { ShowAnswer(mainWnd, L(STR_AW_BADMIN)); return EXC_PLUGIN; }
        int mins = 0;
        while (*s >= L'0' && *s <= L'9') { mins = mins*10 + (*s-L'0'); ++s; }
        while (*s == L' ') ++s;
        if (*s || mins < 1 || mins > 1440) { ShowAnswer(mainWnd, L(STR_AW_BADMIN)); return EXC_PLUGIN; }
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        if (g_awakeTimer) KillTimer(mainWnd, g_awakeTimer);
        g_awakeTimer = 7788;
        SetTimer(mainWnd, 7788, (UINT)mins * 60000u, AwakeExpireCb);
        wchar_t msg[96];
        wsprintfW(msg, L(STR_AW_ON_MIN), mins);
        LogInfo(L"stayawake: on for " + rest + L" min");
        ShowAnswer(mainWnd, msg);
        return EXC_PLUGIN;
    }
    return EXC_NONE;
}

// ---------------------------------------------------------------- themes (cozy look)
// Seven warm presets paint the console, the status strip and the tray badge.
// Explicit [Color] ConsoleBG/ConsoleFG always win; the theme accent tints
// the tray rim and the status chips so tray and console match.
// v0.15: users add their own — themes\<name>.theme.ini, portable with the folder.
// Shared fields (alpha/font/geometry) are a one-time push when the theme is
// picked; they persist as ordinary [Show]/[Font] keys afterwards.
struct Theme {
    std::wstring name;
    COLORREF bg     = RGB(0x2B, 0x26, 0x22);
    COLORREF fg     = RGB(0xF5, 0xE9, 0xDC);
    COLORREF accent = RGB(0xFF, 0xB8, 0x6C);
    COLORREF border = CLR_NONE;   // CLR_NONE = follow the accent
    int  borderW  = -1;           // -1 = unset (built-ins: frameless)
    int  alpha    = -1;           // [Show] Transparent push on pick, -1 = unset
    std::wstring fontName;        // "" = unset
    int  fontSize = -1, fontBold = -1;
    int  w = -1, h = -1;          // [Show] geometry push, -1 = unset
    bool custom   = false;        // loaded from themes\<name>.theme.ini
};
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
static std::vector<Theme> g_customThemes;      // themes\*.theme.ini (sorted by name)

static std::wstring ThemeDirW()  { return ExeDir() + L"themes\\"; }
static std::wstring ThemeFileW(const std::wstring& n) { return ThemeDirW() + n + L".theme.ini"; }

static COLORREF ParseThColor(const wchar_t* b) {   // "RRGGBB" / "0xRRGGBB" -> COLORREF
    COLORREF v = (COLORREF)wcstol(b, nullptr, 16);
    return RGB(GetBValue(v), GetGValue(v), GetRValue(v));   // file is human RRGGBB
}
static COLORREF ThColor(const wchar_t* key, COLORREF def, const std::wstring& f) {
    wchar_t b[32] = {};
    if (!GetPrivateProfileStringW(L"Theme", key, L"", b, 31, f.c_str()) || !b[0])
        return def;
    return ParseThColor(b);
}
static bool LoadCustomTheme(const std::wstring& name, Theme& t) {
    std::wstring f = ThemeFileW(name);
    if (!PathFileExistsW(f.c_str())) return false;
    t = Theme();
    t.name = name; t.custom = true;
    t.bg     = ThColor(L"Bg",     t.bg,     f);
    t.fg     = ThColor(L"Fg",     t.fg,     f);
    t.accent = ThColor(L"Accent", t.accent, f);
    wchar_t b[32] = {};
    if (GetPrivateProfileStringW(L"Theme", L"Border", L"", b, 31, f.c_str()) && b[0])
        t.border = ParseThColor(b);
    t.borderW  = (int)GetPrivateProfileIntW(L"Theme", L"BorderWidth", -1, f.c_str());
    t.alpha    = (int)GetPrivateProfileIntW(L"Theme", L"Alpha",    -1, f.c_str());
    wchar_t fn[LF_FACESIZE] = {};
    GetPrivateProfileStringW(L"Theme", L"FontName", L"", fn, LF_FACESIZE, f.c_str());
    if (fn[0]) t.fontName = fn;
    t.fontSize = (int)GetPrivateProfileIntW(L"Theme", L"FontSize", -1, f.c_str());
    t.fontBold = (int)GetPrivateProfileIntW(L"Theme", L"FontBold", -1, f.c_str());
    t.w        = (int)GetPrivateProfileIntW(L"Theme", L"Width",    -1, f.c_str());
    t.h        = (int)GetPrivateProfileIntW(L"Theme", L"Height",   -1, f.c_str());
    return true;
}
static void LoadThemes() {
    g_customThemes.clear();
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW((ThemeDirW() + L"*.theme.ini").c_str(), &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            std::wstring fn = fd.cFileName;               // "<name>.theme.ini"
            const size_t suffix = 10;                     // strlen(".theme.ini")
            if (fn.size() <= suffix) continue;
            Theme t;
            if (LoadCustomTheme(fn.substr(0, fn.size() - suffix), t))
                g_customThemes.push_back(t);
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    std::sort(g_customThemes.begin(), g_customThemes.end(),
              [](const Theme& a, const Theme& b) {
                  return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
}
static int ThemeCount() { return kThemeCount + (int)g_customThemes.size(); }
static const Theme& ThemeAt(int i) {
    if (i >= 0 && i < kThemeCount) return kThemes[i];
    int c = i - kThemeCount;
    if (c >= 0 && c < (int)g_customThemes.size()) return g_customThemes[c];
    return kThemes[0];
}
static int ThemeFromName(const std::wstring& name) {
    for (int i = 0; i < kThemeCount; ++i)
        if (_wcsicmp(kThemes[i].name.c_str(), name.c_str()) == 0) return i;
    for (int i = 0; i < (int)g_customThemes.size(); ++i)
        if (_wcsicmp(g_customThemes[i].name.c_str(), name.c_str()) == 0)
            return kThemeCount + i;
    return 0;   // cozy-dark
}

static COLORREF MixColor(COLORREF a, COLORREF b, int pct) {   // pct% of b
    auto mix = [pct](int x, int y) { return x + (y - x) * pct / 100; };
    return RGB(mix(GetRValue(a), GetRValue(b)),
               mix(GetGValue(a), GetGValue(b)),
               mix(GetBValue(a), GetBValue(b)));
}

// WCAG contrast helpers (v0.12, UX review B10): secondary strip text and chip
// labels used fixed Mix% values that fail on light themes — raise the mix
// until the pair reaches ~4.5:1 instead of trusting the percentage.
static double SrgbLuma(COLORREF c) {
    auto lin = [](int v) {
        double x = v / 255.0;
        return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(GetRValue(c)) + 0.7152 * lin(GetGValue(c)) +
           0.0722 * lin(GetBValue(c));
}
static double ContrastRatio(COLORREF a, COLORREF b) {
    double la = SrgbLuma(a), lb = SrgbLuma(b);
    if (la < lb) { double t = la; la = lb; lb = t; }
    return (la + 0.05) / (lb + 0.05);
}
static COLORREF ReadableMix(COLORREF bg, COLORREF fg, int startPct) {
    int pct = startPct < 40 ? 40 : startPct;
    for (; pct < 100; pct += 4)
        if (ContrastRatio(bg, MixColor(bg, fg, pct)) >= 4.5)
            return MixColor(bg, fg, pct);
    return MixColor(bg, fg, pct);
}

static COLORREF g_effBg  = RGB(0x2B, 0x26, 0x22);
static COLORREF g_effFg  = RGB(0xF5, 0xE9, 0xDC);
static COLORREF g_accent = RGB(0xFF, 0xB8, 0x6C);
static COLORREF g_effBorder = CLR_NONE;   // v0.15 theme studio: custom frame
static int      g_borderW   = 0;          // px; 0 = frameless (all built-ins)

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
// Custom themes may also carry a client-side frame (Border + BorderWidth).
static void ApplyThemeVisual() {
    const Theme& t = ThemeAt(ThemeFromName(g_set.theme));
    g_effBg  = g_set.consoleBg >= 0 ? (COLORREF)g_set.consoleBg : t.bg;
    g_effFg  = g_set.consoleFg >= 0 ? (COLORREF)g_set.consoleFg : t.fg;
    g_accent = t.accent;
    g_borderW = (t.borderW > 0) ? t.borderW : 0;
    g_effBorder = CLR_NONE;
    if (g_borderW) g_effBorder = (t.border != CLR_NONE) ? t.border : t.accent;
    if (g_bgBrush) { DeleteObject(g_bgBrush); g_bgBrush = nullptr; }
    g_bgBrush = CreateSolidBrush(g_effBg);
}

// N-px frame inside the client area (the window is WS_POPUP, no NC border).
static void PaintFrame(HDC dc, const RECT& rc) {
    if (g_borderW <= 0 || g_effBorder == CLR_NONE) return;
    HBRUSH b = CreateSolidBrush(g_effBorder);
    RECT r = rc;
    for (int i = 0; i < g_borderW; ++i) {
        if (r.left >= r.right - 1 || r.top >= r.bottom - 1) break;
        FrameRect(dc, &r, b);
        InflateRect(&r, -1, -1);
    }
    DeleteObject(b);
}

// ---------------------------------------------------------------- status strip
// A 20px cozy strip at the bottom of the drop-down: mini ">_", alias/plugin
// counts, EasyType + theme chips, sound / language / clock on the right.
static const int SB_H = 20;
// status strip height for the console (logical 20px scaled to the console dpi)
static int SbH() { return Sc(g_mainDpi, SB_H); }
static HFONT g_sbFont = nullptr;
// (re)build the 96-logical 11px strip font at the console's current dpi
static void RebuildSbFont() {
    if (g_sbFont) { DeleteObject(g_sbFont); g_sbFont = nullptr; }
    g_sbFont = CreateFontW(-Sc(g_mainDpi, 11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           VARIABLE_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void PaintStatusBar(HDC dc, RECT rc, int dpi) {
    rc.top = rc.bottom - Sc(dpi, SB_H);
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
    int y = rc.top + Sc(dpi, 3);

    // visual anchors: three physical lamps previewing what Enter will do
    static const COLORREF lit[3] = { RGB(0x98, 0xC9, 0x74),   // green: your own
                                     RGB(0xE0, 0xAF, 0x68),   // amber: found outside
                                     RGB(0xE0, 0x6C, 0x75) };  // red: unknown/shell
    int x = rc.left + 7;
    for (int i = 0; i < 3; ++i) {
        bool on = (g_anchor.lamp == i);
        // unlit lamps must stay visible too (UX A2): 32% fill + 25% ring
        HBRUSH b2 = CreateSolidBrush(on ? lit[i] : MixColor(g_effBg, lit[i], 32));
        HPEN   p2 = CreatePen(PS_SOLID, 1, MixColor(g_effBg, RGB(0xFF, 0xFF, 0xFF),
                                                    on ? 45 : 25));
        HPEN o2 = (HPEN)SelectObject(dc, p2);
        HBRUSH o3 = (HBRUSH)SelectObject(dc, b2);
        Ellipse(dc, x, rc.top + Sc(dpi, 6), x + Sc(dpi, 8), rc.top + Sc(dpi, 14));
        SelectObject(dc, o2); SelectObject(dc, o3);
        DeleteObject(b2); DeleteObject(p2);
        x += Sc(dpi, 12);
    }
    x += Sc(dpi, 4);
    if (g_anchor.lamp >= 0 && g_anchor.labelId >= 0 && g_anchor.lamp < 3) {
        const wchar_t* lbl = L(g_anchor.labelId);
        SIZE ls; GetTextExtentPoint32W(dc, lbl, (int)wcslen(lbl), &ls);
        SetTextColor(dc, MixColor(g_effBg, lit[g_anchor.lamp], 85));
        TextOutW(dc, x, y, lbl, (int)wcslen(lbl));
        x += ls.cx + Sc(dpi, 14);
    } else {
        x += Sc(dpi, 6);
    }

    SetTextColor(dc, g_accent);
    TextOutW(dc, x, y, L">_", 2);
    x += Sc(dpi, 17);
    size_t plg = 0, plgOn = 0;
    for (auto& p : g_plugins) { ++plg; if (p.loaded) ++plgOn; }
    std::wstring left = std::to_wstring(g_aliases.size()) + L" al   \u25CF" +
                        std::to_wstring(plgOn) + L"/" + std::to_wstring(plg);
    SetTextColor(dc, ReadableMix(g_effBg, g_effFg, 62));          // contrast guard
    TextOutW(dc, x, y, left.c_str(), (int)left.size());
    SIZE sz; GetTextExtentPoint32W(dc, left.c_str(), (int)left.size(), &sz);
    x += sz.cx + Sc(dpi, 12);

    const wchar_t* mode = g_set.easyType == 0 ? L(STR_ET_OFF)
                        : g_set.easyType == 1 ? L(STR_ET_EASY) : L(STR_ET_LINUX);
    std::wstring themeName = ThemeAt(ThemeFromName(g_set.theme)).name;   // v0.15: customs too
    for (int pass = 0; pass < 2; ++pass) {
        const wchar_t* chip = pass == 0 ? mode : themeName.c_str();
        SIZE cs; GetTextExtentPoint32W(dc, chip, (int)wcslen(chip), &cs);
        COLORREF chipBg = MixColor(g_effBg, g_accent, 18);
        HBRUSH cb = CreateSolidBrush(chipBg);
        HPEN   cp = CreatePen(PS_SOLID, 1, MixColor(g_effBg, g_accent, 55));
        HPEN   o2 = (HPEN)SelectObject(dc, cp);
        HBRUSH o3 = (HBRUSH)SelectObject(dc, cb);
        RoundRect(dc, x, rc.top + Sc(dpi, 2), x + cs.cx + Sc(dpi, 10),
                  rc.top + Sc(dpi, SB_H) - Sc(dpi, 3), Sc(dpi, 7), Sc(dpi, 7));
        SelectObject(dc, o2); SelectObject(dc, o3);
        DeleteObject(cb); DeleteObject(cp);
        SetTextColor(dc, ReadableMix(chipBg, g_accent, 85));      // contrast guard
        TextOutW(dc, x + Sc(dpi, 5), y, chip, (int)wcslen(chip));
        x += cs.cx + Sc(dpi, 18);
    }

    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t clk[8]; wsprintfW(clk, L"%02d:%02d", (int)st.wHour, (int)st.wMinute);
    std::wstring right = std::wstring(g_set.enableSounds ? L"\u266A " : L"\u00D7 ") +
                         std::wstring(kLangs[g_langIdx].code) + L"  " + clk;
    SIZE rs; GetTextExtentPoint32W(dc, right.c_str(), (int)right.size(), &rs);
    SetTextColor(dc, ReadableMix(g_effBg, g_effFg, 62));          // clock: contrast guard
    TextOutW(dc, rc.right - rs.cx - Sc(dpi, 8), y, right.c_str(), (int)right.size());

    SelectObject(dc, oldF);
}

static void Submit(HWND mainWnd) {
    std::wstring line = GetEditText(g_edit);
    if (line.empty()) return;
    DdClose();
    g_hsrch = false;                          // v0.27: Enter ends search mode
    g_internalAnswer = false;
    g_lastRun = line;
    // v0.18: su/sudo//su prefix -> run the rest elevated. History keeps the
    // typed line (re-runnable), Usage records the real command word.
    std::wstring run = line;
    {
        std::wstring suRest;
        if (SuSplit(line, suRest)) {
            if (suRest.empty()) {                       // bare `su`: console hint
                LogInfo(L"su: usage hint shown");
                SetWindowTextW(g_edit, L(STR_SU_HINT));
                SendMessageW(g_edit, EM_SETSEL, 0, -1);
                return;                                 // console stays open
            }
            run = suRest;
            if (g_set.elevate) {            // [Security] Elevate=0 runs it normally
                g_elevateNext = true;       // (headless automation), documented
                LogInfo(L"elevate: " + run);
            } else {
                LogInfo(L"elevate: " + run + L" (normal, [Security] Elevate=0)");
            }
        }
    }
    ExecType t = (ExecType)RunLine(run, mainWnd);
    g_elevateNext = false;
    if (t != EXC_NONE) RecordUsage(run);   // v0.17 frecency (independent of history rules)
    if (t != EXC_NONE && HistoryAllows(t)) {
        g_history.push_back(line);
        if ((int)g_history.size() > g_set.historyMax)
            g_history.erase(g_history.begin());
        SaveHistory();
    }
    // v0.14: plugin output lives in the edit (RunPlugins just filled it with
    // con_text + selection). Keep the console open so the result can be read
    // and copied - the original TypeAndRun behaved exactly like this. Esc or
    // the hotkey still hides; the next keystroke replaces the result.
    if (t == EXC_PLUGIN) return;
    if (g_internalAnswer) {                   // v0.27: /day /tabs answered in line
        g_internalAnswer = false;
        return;                               // console stays open, text selected
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

// Fuzzy subsequence scoring (v0.11, [Type] Fuzzy). Returns >0 when every
// pattern char appears in the text in order; `out` receives the matched
// offsets so the drop-down can accent exactly the hit characters. Bonuses:
// straight-prefix run, word-start hits, consecutive runs; penalties: gaps
// between hits and overall verbosity. 0 means "no match". With Fuzzy=0 the
// callers keep TypeAndRun's prefix behaviour (StartsWithNoCase).
static int FuzzyScore(const std::wstring& text, const std::wstring& pat,
                      std::vector<size_t>* out = nullptr) {
    if (pat.empty()) return 1;                       // empty pattern matches all
    if (text.size() < pat.size()) return 0;
    std::vector<size_t> hits;
    int score = 0;
    size_t p = 0;
    for (size_t i = 0; i < text.size() && p < pat.size(); ++i) {
        if (towlower(text[i]) != towlower(pat[p])) continue;
        int add = 1;
        if (i == 0) add += 10;                                       // straight prefix
        else {
            wchar_t c = text[i - 1];
            if (c == L' ' || c == L'-' || c == L'_' || c == L'.' ||
                c == L'\\' || c == L'/') add += 8;                   // word start
        }
        if (p && hits[p - 1] + 1 == i) add += 5;                     // consecutive run
        else if (p) score -= (int)(i - hits[p - 1] - 1);             // gap penalty
        score += add;
        hits.push_back(i);
        ++p;
    }
    if (p < pat.size()) return 0;                                    // not a subsequence
    score -= (int)(text.size() - pat.size()) / 4;                    // prefer shorter
    if (score < 1) score = 1;
    if (out) *out = hits;
    return score;
}

// v0.32 native typing: score `text` against the typed word AND - when dir
// says so - the same word in the other keyboard layout. A layout hit scores
// one below its raw value, so a real match always wins the tie; marks come
// from whichever form matched (they index into text either way).
static int FuzzyScoreLayout(const std::wstring& text, const std::wstring& pat,
                            int dir, std::vector<size_t>* out = nullptr,
                            bool* viaLayout = nullptr) {
    int s1 = FuzzyScore(text, pat, out);
    if (!dir) return s1;
    std::wstring alt = LayoutConvert(pat, dir);
    std::vector<size_t> mk2;
    int s2 = FuzzyScore(text, alt, &mk2);
    if (s2 <= 0) return s1;
    int s2b = s2 > 1 ? s2 - 1 : 1;
    if (s2b > s1) {
        if (out) *out = std::move(mk2);
        if (viaLayout) *viaLayout = true;
        return s2b;
    }
    return s1;
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

    bool listFresh = g_set.fuzzy ? (word == g_etPrefix)
                                 : StartsWithNoCase(word, g_etPrefix);
    if (g_etList.empty() || !listFresh) {
        // (re)build the candidate list for this prefix
        ResetET();
        g_etPrefix = word;
        const int dir = g_set.layoutFallback ? LayoutDir(word) : 0;   // v0.32
        auto add = [&](const std::wstring& cand) {
            if (cand.empty()) return;
            if (!word.empty()) {
                if (g_set.fuzzy) { if (FuzzyScoreLayout(cand, word, dir) <= 0) return; }
                else if (!StartsWithNoCase(cand, word) &&
                         !(dir && StartsWithNoCase(cand, LayoutConvert(word, dir)))) return;
            }
            for (const auto& c : g_etList)
                if (_wcsicmp(c.c_str(), cand.c_str()) == 0) return;
            g_etList.push_back(cand);
        };
        bool firstWord = (b == 0);
        if (firstWord) {
            if (g_set.etInternal) {
                for (const wchar_t* cmd : { L"/about", L"/checkconfig", L"/config",
                     L"/day", L"/exit", L"/export", L"/help", L"/hide", L"/homedir", L"/import",
                     L"/inidir", L"/inifiles", L"/load", L"/log", L"/openconfig", L"/path", L"/reread", L"/settings",
                     L"/show", L"/sys", L"/tabs", L"/unload", L"/undo", L"/update" })
                    add(cmd);
            }
            if (g_set.etAlias)
                for (const auto& a : g_aliases) add(a.name);
            if (g_set.etSysAlias)
                for (const auto& s : g_sysAliases) add(s.name);
            if (!g_webLinks.empty())
                for (auto& wl : g_webLinks) add(wl.name);   // quicklinks (g/yt/gh)
            if (g_set.execPlugins)
                for (auto& p : g_plugins)
                    if (p.GetCount && p.GetString)
                        for (int i = 0; i < p.GetCount(); ++i)
                            add(FromAnsi(p.GetString(i)));
        }
        if (g_set.etHistory && word.find(L'/') != 0)
            for (size_t i = g_history.size(); i-- > 0; ) add(g_history[i]);
        if (g_set.execPlugins && text.find(L'/') != 0)
            // v0.26: gate on the LINE start, not the caret word - "yt /cl"
            // must still reach the plugin templates (only leading "/" lines
            // are internal commands).
            // v0.32 perf: the wide->ANSI line conversion happens ONCE per
            // rebuild, not once per plugin.
            {
                std::string aLine = ToAnsi(text);
                for (auto& p : g_plugins)
                    if (p.GetCountET && p.GetStringET) {
                        // v0.24: whole line, not just the caret word - plugins key
                        // their subcommand templates on "cmd " (cliphist "clip ri").
                        int cnt = p.GetCountET(aLine.c_str());
                        for (int i = 0; i < cnt; ++i) add(FromAnsi(p.GetStringET(i)));
                    }
            }
        g_etPos = 0;
        if (g_set.fuzzy && !word.empty()) {          // best fuzzy match completes first
            // v0.32 perf: score each candidate ONCE (was twice per comparator
            // call) - FuzzyScoreLayout is no longer re-run inside the sort.
            std::vector<std::pair<int, size_t>> ord;
            ord.reserve(g_etList.size());
            for (size_t i = 0; i < g_etList.size(); ++i)
                ord.push_back({ FuzzyScoreLayout(g_etList[i], word, dir), i });
            std::stable_sort(ord.begin(), ord.end(),
                [](const std::pair<int, size_t>& a, const std::pair<int, size_t>& b) {
                    return a.first > b.first;
                });
            std::vector<std::wstring> sorted;
            sorted.reserve(ord.size());
            for (auto& kv : ord) sorted.push_back(g_etList[kv.second]);
            g_etList.swap(sorted);
        }
    }

    if (!g_etList.empty()) {
        std::wstring repl = g_etList[g_etPos % g_etList.size()];
        ++g_etPos;
        // Whole-line templates: a candidate that repeats everything left of the
        // caret ("clip ring" completed on the word "ri") replaces from 0.
        size_t start = b;
        if (b > 0 && StartsWithNoCase(repl, text.substr(0, b))) start = 0;
        std::wstring nt = text.substr(0, start) + repl + text.substr(e);
        SetWindowTextW(edit, nt.c_str());
        SendMessageW(edit, EM_SETSEL, (WPARAM)(start + repl.size()), (LPARAM)(start + repl.size()));
    }
}

// ---------------------------------------------------------------- drop-down (v0.10)
// TypeAndRun's frmTypeDropDown parity: a themed suggestion list glued under
// the console (it flips above when the screen runs out). Rows are the ET
// candidates for the word at the caret — aliases, ~sysaliases, /commands,
// plugin commands and history — or, when the line is empty, the history
// itself ([DropDown] ShowHistoryOnEmpty). The window is owned + NOACTIVATE:
// the caret never leaves the edit, keys stay in EditProc, and the mouse can
// still hover and click rows.
static const wchar_t* kDDClass = L"TypeBoxDropDown";
static HWND                      g_ddWnd = nullptr;
static HWND                      g_ddOwner = nullptr;
static HFONT                     g_ddFont = nullptr;
static std::vector<std::wstring> g_ddItems;
static std::vector<std::vector<size_t>> g_ddMarks;   // per-row matched offsets
static const std::vector<size_t> kNoMarks;           // for rows without a pattern
static int    g_ddSel = -1, g_ddRowH = 22;
static int    g_ddTop = 0;          // first visible row when the list overflows
static bool   g_ddSelKb = false;    // Enter runs only rows picked by keyboard
static size_t g_ddB = 0, g_ddE = 0;                  // word slot inside the edit

static bool DdVisible() { return g_ddWnd && IsWindowVisible(g_ddWnd); }

static void DdSyncTitle() {   // cheap state probe for the test harness
    if (!g_ddWnd) return;
    std::wstring top;                          // v0.17: top row too (frecency asserts)
    if (!g_ddItems.empty()) {
        top = g_ddItems[0];
        if (!g_ddLayout.empty() && g_ddLayout[0])   // v0.32: top row matched only
            top = L"~" + top;                       // via the layout fallback
        for (auto& ch : top) if (ch == L'|' || ch == L'\r' || ch == L'\n') ch = L' ';
        if (top.size() > 40) top = top.substr(0, 40);
    }
    SetWindowTextW(g_ddWnd, (std::wstring(L"TypeBoxDropDown|") +
        std::to_wstring(g_ddItems.size()) + L"|" + std::to_wstring(g_ddSel) +
        L"|" + std::to_wstring(g_ddTop) + L"|" + top +
        (g_hsrch ? L"|bck" : L"") +                        // v0.27: |bck = history search
        (g_headless ? L"|s=" + std::to_wstring(g_etScanCount) : L"")).c_str());
}

static int DdVisibleRows() {
    int vis = (int)g_ddItems.size();
    if (vis > g_set.ddLines) vis = g_set.ddLines;
    return vis < 0 ? 0 : vis;
}

static void DdKeepVisible() {       // arrows/wheel keep the selection on screen
    int vis = DdVisibleRows();
    if (vis <= 0) { g_ddTop = 0; return; }
    if (g_ddSel >= 0 && g_ddSel < g_ddTop) g_ddTop = g_ddSel;
    if (g_ddSel >= g_ddTop + vis) g_ddTop = g_ddSel - vis + 1;
    int maxTop = (int)g_ddItems.size() - vis; if (maxTop < 0) maxTop = 0;
    if (g_ddTop > maxTop) g_ddTop = maxTop;
    if (g_ddTop < 0) g_ddTop = 0;
}

static void DdClose() {
    if (g_ddWnd) ShowWindow(g_ddWnd, SW_HIDE);
    g_ddSel = -1;
    g_ddTop = 0;
    g_ddSelKb = false;
    g_ddItems.clear();
    g_ddMarks.clear();
    g_ddLayout.clear();
    g_ddSub.clear();
    g_ddCacheText.clear();                    // v0.32: force a fresh scan next time
    g_ddCacheB = g_ddCacheE = 0;
    g_ddCacheHsrch = false;
}

static void DdEnsureFont() {
    if (g_ddFont) return;
    // v0.29: the dd font follows g_ddDpi (row height is derived from its
    // metrics below, so the whole list scales with it)
    g_ddFont = CreateFontW(-MulDiv(MulDiv(g_set.fontSize, 96, 72) + 1, g_ddDpi, 96),
        0, 0, 0,
        g_set.fontBold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, ConsoleFontSuggestion(g_set.fontName.c_str()));
    HDC dc = GetDC(nullptr);
    HFONT old = (HFONT)SelectObject(dc, g_ddFont);
    TEXTMETRICW tm = {};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old); ReleaseDC(nullptr, dc);
    g_ddRowH = (int)tm.tmHeight + Sc(g_ddDpi, 8);   // v0.29: row padding rides the dpi too
}

static void PaintDropDown(HDC dc) {
    RECT rc; GetClientRect(g_ddWnd, &rc);
    HBRUSH b = CreateSolidBrush(MixColor(g_effBg, RGB(0, 0, 0), 10));
    FillRect(dc, &rc, b); DeleteObject(b);
    HBRUSH fb = CreateSolidBrush(MixColor(g_effBg, g_accent, 55));
    FrameRect(dc, &rc, fb); DeleteObject(fb);
    DdEnsureFont();
    HFONT oldF = (HFONT)SelectObject(dc, g_ddFont);
    SetBkMode(dc, TRANSPARENT);
    DdKeepVisible();
    int vis = DdVisibleRows();
    for (int i = 0; i < vis; ++i) {
        int di = g_ddTop + i;
        RECT rr = { Sc(g_ddDpi, 2), i * g_ddRowH + Sc(g_ddDpi, 2),
                    rc.right - Sc(g_ddDpi, 2), (i + 1) * g_ddRowH + Sc(g_ddDpi, 2) };
        if (rr.top >= rc.bottom) break;
        if (di == g_ddSel) {
            HBRUSH sb = CreateSolidBrush(MixColor(g_effBg, g_accent, 24));
            FillRect(dc, &rr, sb); DeleteObject(sb);
        }
        const std::wstring& it = g_ddItems[di];
        int px = rr.left + Sc(g_ddDpi, 6), py = rr.top + Sc(g_ddDpi, 3);
        if (i < 9) {                                   // Alt+1..9 gutter (Flow/Listary)
            wchar_t dg = (wchar_t)(L'1' + i);
            SetTextColor(dc, MixColor(g_effBg, g_accent, 70));
            TextOutW(dc, px, py, &dg, 1);
            SIZE ds = { 0, 0 }; GetTextExtentPoint32W(dc, &dg, 1, &ds);
            px += ds.cx + Sc(g_ddDpi, 6);
        }
        COLORREF hot = MixColor(g_effBg, g_accent, 88);           // matched chars
        COLORREF dim = MixColor(g_effBg, g_effFg, di == g_ddSel ? 100 : 72);
        const std::vector<size_t>& mk =
            (di < (int)g_ddMarks.size()) ? g_ddMarks[di] : kNoMarks;
        size_t h = 0;                                             // marks are ascending
        for (size_t ci = 0; ci < it.size(); ++ci) {
            bool on = h < mk.size() && mk[h] == ci;
            if (on) ++h;
            SetTextColor(dc, on ? hot : dim);
            TextOutW(dc, px, py, it.c_str() + ci, 1);
            SIZE s = { 0, 0 };
            GetTextExtentPoint32W(dc, it.c_str() + ci, 1, &s);
            px += s.cx;
        }
        if (di < (int)g_ddLayout.size() && g_ddLayout[di]) {   // v0.32: matched
            SetTextColor(dc, MixColor(g_effBg, g_accent, 88)); // via the layout
            TextOutW(dc, px + Sc(g_ddDpi, 4), py, L"~", 1);   // fallback - hint
        }
    }
    SelectObject(dc, oldF);
}

static void DdPick(bool run);   // used by the mouse handler below

static LRESULT CALLBACK DropDownProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND:  return 1;                       // all painting in WM_PAINT
    case WM_DPICHANGED: {                                 // v0.29 PMv2 wave
        g_ddDpi = LOWORD(w) >= 96 ? (int)LOWORD(w) : 96;
        if (g_ddFont) { DeleteObject(g_ddFont); g_ddFont = nullptr; }
        DdEnsureFont();                                  // row height rides the new metrics
        RECT* pr = (RECT*)l;
        if (pr) SetWindowPos(h, HWND_TOPMOST, pr->left, pr->top,
                             pr->right - pr->left, pr->bottom - pr->top,
                             SWP_NOACTIVATE);
        InvalidateRect(h, nullptr, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        PaintDropDown(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:                                  // PrintWindow/screenshots
        if ((HDC)w) PaintDropDown((HDC)w);
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    case WM_MOUSEMOVE: {
        int idx = g_ddTop + ((int)HIWORD(l) - Sc(g_ddDpi, 2)) / (g_ddRowH > 0 ? g_ddRowH : Sc(g_ddDpi, 22));
        if (idx >= 0 && idx < (int)g_ddItems.size() && idx != g_ddSel) {
            g_ddSel = idx; g_ddSelKb = false;     // hover alone must not arm Enter
            DdSyncTitle();
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int idx = g_ddTop + ((int)HIWORD(l) - Sc(g_ddDpi, 2)) / (g_ddRowH > 0 ? g_ddRowH : Sc(g_ddDpi, 22));
        if (idx >= 0 && idx < (int)g_ddItems.size()) { g_ddSel = idx; DdPick(true); }
        return 0;
    }
    case WM_MOUSEWHEEL: {                          // scroll past the [DropDown] Lines cap
        int steps = GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA;
        g_ddTop -= steps;
        DdKeepVisible();
        if (g_ddSel >= 0) DdSyncTitle();
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_DESTROY:
        if (g_ddFont) { DeleteObject(g_ddFont); g_ddFont = nullptr; }
        g_ddWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// Replace the caret word with the selected candidate; run=true submits too.
static void DdPick(bool run) {
    if (!g_edit || g_ddSel < 0 || g_ddSel >= (int)g_ddItems.size() || !g_ddOwner) return;
    // v0.32: "≈ candidate" spell rows carry their real text in g_ddSub.
    std::wstring item = (g_ddSel < (int)g_ddSub.size()) ? g_ddSub[g_ddSel]
                                                        : g_ddItems[g_ddSel];
    int len = GetWindowTextLengthW(g_edit);
    std::wstring text(len, L'\0');
    if (len) GetWindowTextW(g_edit, &text[0], len + 1);
    size_t b = g_ddB <= text.size() ? g_ddB : text.size();
    size_t e = g_ddE <= text.size() ? g_ddE : text.size();
    if (e < b) e = b;
    size_t start = b;                            // whole-line template? replace from 0
    if (b > 0 && StartsWithNoCase(item, text.substr(0, b))) start = 0;
    std::wstring nt = text.substr(0, start) + item + text.substr(e);
    SetWindowTextW(g_edit, nt.c_str());
    SendMessageW(g_edit, EM_SETSEL, (WPARAM)(start + item.size()), (LPARAM)(start + item.size()));
    HWND owner = g_ddOwner;
    DdClose();
    if (run) Submit(owner);
}

// Rebuild and (re)position the list for the word at the caret.
static void DdUpdate(HWND mainWnd) {
    if (!g_set.dropDown || !g_visible || !g_edit) { DdClose(); return; }
    int len = GetWindowTextLengthW(g_edit);
    std::wstring text(len, L'\0');
    if (len) GetWindowTextW(g_edit, &text[0], len + 1);
    DWORD sel = (DWORD)SendMessageW(g_edit, EM_GETSEL, 0, 0);
    size_t caret = LOWORD(sel) <= text.size() ? LOWORD(sel) : text.size();
    size_t b = caret, e = caret;
    while (b > 0 && text[b - 1] != L' ' && text[b - 1] != L'\t') --b;
    while (e < text.size() && text[e] != L' ' && text[e] != L'\t') ++e;
    std::wstring word = text.substr(b, e - b);

    // v0.32 perf (ROADMAP-V1 §4b): same text, same word span, same mode ->
    // what is on screen is still right; skip the whole rescan (the plugin
    // GetCountET() round-trips are the expensive part). DdClose invalidates.
    if (g_ddWnd && !g_hsrch && !g_ddCacheHsrch && !g_ddItems.empty() &&
        IsWindowVisible(g_ddWnd) && text == g_ddCacheText &&
        b == g_ddCacheB && e == g_ddCacheE) return;

    std::vector<std::wstring> items;
    std::vector<std::vector<size_t>> marks;
    std::vector<int> score;
    std::vector<unsigned char> lay;                       // v0.32: row matched via layout
    std::vector<std::wstring> sub;                        // v0.32: text a pick writes back
    const int dir = (g_set.layoutFallback && !g_hsrch) ? LayoutDir(word) : 0;
    auto add = [&](const std::wstring& raw) {
        std::wstring c = Trim(raw);                       // plugins may return "calc "
        if (c.empty() || (int)items.size() >= 100) return;
        std::vector<size_t> mk;
        int sc = 1;                                       // empty word: keep source order
        bool viaLay = false;
        if (!word.empty()) {
            if (g_set.fuzzy) {
                sc = FuzzyScoreLayout(c, word, dir, &mk, &viaLay);
                if (sc <= 0) return;
            } else {
                bool hit = StartsWithNoCase(c, word);     // prefix mode (Fuzzy=0)
                if (!hit && dir && StartsWithNoCase(c, LayoutConvert(word, dir))) {
                    hit = true; viaLay = true;
                }
                if (!hit) return;
                for (size_t i = 0; i < word.size(); ++i) mk.push_back(i);
            }
            sc += FrecencyBoost(c);                       // v0.17: used-more wins ties
        }
        for (auto& x : items)
            if (_wcsicmp(x.c_str(), c.c_str()) == 0) return;
        items.push_back(c); marks.push_back(std::move(mk)); score.push_back(sc);
        lay.push_back(viaLay ? 1 : 0); sub.push_back(c);
    };
    if (g_hsrch) {
        // v0.27 (RI3 #12): bck-i-search — rows = history containing the query,
        // newest first, marks on the match. Rebuilt on every keystroke, so the
        // newest match stays selected (0) exactly like bash.
        std::wstring q = Trim(text);
        for (size_t i = g_history.size(); i-- > 0; ) {
            const std::wstring& h = g_history[i];
            if (h.empty()) continue;
            std::vector<size_t> mk;
            if (!q.empty()) {
                const wchar_t* f = StrStrIW(h.c_str(), q.c_str());   // shlwapi
                if (!f) continue;
                size_t off = (size_t)(f - h.c_str());
                for (size_t k = 0; k < q.size(); ++k) mk.push_back(off + k);
            }
            bool dup = false;
            for (auto& x : items) if (_wcsicmp(x.c_str(), h.c_str()) == 0) { dup = true; break; }
            if (dup) continue;
            if ((int)items.size() >= 100) break;
            items.push_back(h); marks.push_back(std::move(mk)); score.push_back(1);
            lay.push_back(0); sub.push_back(h);
        }
        if (items.empty()) { DdClose(); return; }
    } else if (word.empty()) {
        if (g_set.ddOnEmpty)                        // empty console = history (parity)
            for (size_t i = g_history.size(); i-- > 0; ) add(g_history[i]);
    } else {
        bool firstWord = (b == 0);
        if (firstWord && g_set.etInternal) {
            for (auto c : kInternalCmds) add(c);
            add(L"pos"); add(L"stayawake");      // v0.23 host utility words
        }
        if (firstWord && g_set.etAlias)
            for (auto& a : g_aliases) add(a.name);
        if (firstWord && g_set.etSysAlias)
            for (auto& s : g_sysAliases) add(s.name);
        if (firstWord)
            for (auto& wl : g_webLinks) add(wl.name);   // quicklinks like "g"
        if (firstWord && g_set.execPlugins)
            for (auto& p : g_plugins)
                if (p.GetCount && p.GetString && PluginLoadAllowed(p))
                    for (int i = 0; i < p.GetCount(); ++i) add(FromAnsi(p.GetString(i)));
        if (g_set.etHistory && word[0] != L'/')
            for (size_t i = g_history.size(); i-- > 0; ) add(g_history[i]);
        if (g_set.execPlugins && word[0] != L'/') {
            std::string aLine = ToAnsi(text);   // v0.32 perf: convert the line ONCE
            for (auto& p : g_plugins)
                if (p.GetCountET && p.GetStringET && PluginLoadAllowed(p)) {
                    int cnt = p.GetCountET(aLine.c_str());  // v0.24: whole line
                    for (int i = 0; i < cnt; ++i) add(FromAnsi(p.GetStringET(i)));
                }
        }
    }
    ++g_etScanCount;                        // v0.32 perf probe (|s=N in the title)

    // v0.32 spell-suggest (ROADMAP #13): nothing survived the fuzzy pass ->
    // offer up to 3 "≈ candidate" rows within 2 edits of the typed word.
    // The ≈ is display-only: picking a row writes g_ddSub (the real word).
    if (!word.empty() && !g_hsrch && items.empty() && b == 0) {
        std::vector<std::pair<int, std::wstring>> sug;
        auto trySug = [&](const std::wstring& cand) {
            std::wstring c = Trim(cand);
            if (c.empty() || _wcsicmp(c.c_str(), word.c_str()) == 0) return;
            int d = EditDistCapped(c, word, 2);
            if (d >= 1 && d <= 2) sug.push_back({ d, c });
        };
        if (g_set.etInternal) {
            for (auto c : kInternalCmds) trySug(c);
            trySug(L"pos"); trySug(L"stayawake");
        }
        if (g_set.etAlias)
            for (auto& a : g_aliases) trySug(a.name);
        if (g_set.etSysAlias)
            for (auto& s : g_sysAliases) trySug(s.name);
        for (auto& wl : g_webLinks) trySug(wl.name);
        if (g_set.execPlugins)
            for (auto& p : g_plugins)
                if (p.GetCount && p.GetString && PluginLoadAllowed(p))
                    for (int i = 0; i < p.GetCount(); ++i) trySug(FromAnsi(p.GetString(i)));
        std::stable_sort(sug.begin(), sug.end(),
            [](const std::pair<int, std::wstring>& a, const std::pair<int, std::wstring>& b2) {
                return a.first < b2.first;
            });
        for (size_t i = 0; i < sug.size() && i < 3; ++i) {
            items.push_back(L"\u2248 " + sug[i].second);      // ≈ candidate
            marks.emplace_back();
            score.push_back(0);                               // always below real hits
            lay.push_back(0);
            sub.push_back(sug[i].second);
        }
    }
    if (items.empty()) { DdClose(); return; }
    if (!g_hsrch && !word.empty() && (g_set.fuzzy || g_set.frecency)) {   // best match on top, ties keep source order
        std::vector<size_t> ord(items.size());
        for (size_t i = 0; i < ord.size(); ++i) ord[i] = i;
        std::stable_sort(ord.begin(), ord.end(),
            [&](size_t a, size_t b) { return score[a] > score[b]; });
        std::vector<std::wstring> sItems;
        std::vector<std::vector<size_t>> sMarks;
        std::vector<unsigned char> sLay;
        std::vector<std::wstring> sSub;
        sItems.reserve(items.size()); sMarks.reserve(items.size());
        sLay.reserve(items.size()); sSub.reserve(items.size());
        for (size_t i : ord) {
            sItems.push_back(items[i]); sMarks.push_back(marks[i]);
            sLay.push_back(lay[i]);     sSub.push_back(sub[i]);
        }
        items = std::move(sItems); marks = std::move(sMarks);
        lay = std::move(sLay);     sub = std::move(sSub);
    }

    if (!g_ddWnd) {
        static bool reg = false;
        HINSTANCE hInst = (HINSTANCE)(LONG_PTR)GetWindowLongPtrW(mainWnd, GWLP_HINSTANCE);
        if (!reg) {
            WNDCLASSW wc = {};
            wc.lpfnWndProc = DropDownProc;
            wc.hInstance = hInst;
            wc.lpszClassName = kDDClass;
            RegisterClassW(&wc);
            reg = true;
        }
        g_ddWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kDDClass, kDDClass, WS_POPUP,
            0, 0, 10, 10, mainWnd, nullptr, hInst, nullptr);
        if (!g_ddWnd) { LogInfo(L"drop-down: create failed"); g_set.dropDown = false; return; }
    }
    g_ddOwner = mainWnd;
    g_ddDpi = g_mainDpi;          // v0.29: the dd rides right below the console
    g_ddItems = std::move(items);
    g_ddMarks = std::move(marks);
    g_ddLayout = std::move(lay);
    g_ddSub = std::move(sub);
    g_ddB = b; g_ddE = e; g_ddSel = g_hsrch ? 0 : -1;
    g_ddTop = 0; g_ddSelKb = g_hsrch;         // v0.27: Enter runs the newest match
    g_ddCacheText = text;                     // v0.32 debounce: an identical
    g_ddCacheB = b; g_ddCacheE = e;            // keystroke reuses this list
    g_ddCacheHsrch = g_hsrch;

    DdEnsureFont();
    int rows = (int)g_ddItems.size();
    if (rows > g_set.ddLines) rows = g_set.ddLines;
    int h = rows * g_ddRowH + Sc(g_ddDpi, 4);       // v0.29: dd total scales with rows
    RECT wr; GetWindowRect(mainWnd, &wr);
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = wr.left, y = wr.bottom + 1;
    if (y + h > wa.bottom) y = wr.top - h - 1;           // flip above (parity)
    SetWindowPos(g_ddWnd, HWND_TOPMOST, x, y, wr.right - wr.left, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    DdSyncTitle();
    InvalidateRect(g_ddWnd, nullptr, TRUE);
    UpdateWindow(g_ddWnd);
}

static void ShowTrayMenu(HWND hWnd) {
    POINT pt; GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L(STR_TRAY_SHOW));
    AppendMenuW(menu, MF_STRING, 4, L(STR_TRAY_SETTINGS));
    AppendMenuW(menu, MF_STRING, 2, L(STR_TRAY_CONFIG));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L(STR_TRAY_EXIT));
    if (!g_headless) SetForegroundWindow(hWnd); // so the menu dismisses on click-away
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(menu);
}

// ------------------------------------------------------- console context menu
// The original TypeAndRun popped a menu on the console itself (settings, etc).
// v0.14 brings it back and adds the standard edit operations. Items dispatch
// by ID in WM_COMMAND (they arrive even if TrackPopupMenu is still pumping).
enum { IDCT_UNDO = 9101, IDCT_CUT, IDCT_COPY, IDCT_PASTE, IDCT_SELALL,
       IDCT_SETTINGS, IDCT_ALIASES, IDCT_PLUGINS, IDCT_THEME, IDCT_LANG,
       IDCT_RELOAD, IDCT_INIDIR, IDCT_ABOUT, IDCT_HELP, IDCT_HIDE, IDCT_EXIT };

static void ShowConsoleMenu(HWND hWnd, POINT pt) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDCT_UNDO,    L(STR_CTX_UNDO));
    AppendMenuW(menu, MF_STRING, IDCT_CUT,     L(STR_CTX_CUT));
    AppendMenuW(menu, MF_STRING, IDCT_COPY,    L(STR_CTX_COPY));
    AppendMenuW(menu, MF_STRING, IDCT_PASTE,   L(STR_CTX_PASTE));
    AppendMenuW(menu, MF_STRING, IDCT_SELALL,  L(STR_CTX_SELALL));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDCT_SETTINGS, L(STR_CTX_SETTINGS));
    AppendMenuW(menu, MF_STRING, IDCT_ALIASES,  L(STR_CTX_ALIASES));
    AppendMenuW(menu, MF_STRING, IDCT_PLUGINS,  L(STR_CTX_PLUGINS));
    AppendMenuW(menu, MF_STRING, IDCT_THEME,    L(STR_CTX_THEME));
    AppendMenuW(menu, MF_STRING, IDCT_LANG,     L(STR_CTX_LANG));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDCT_RELOAD,   L(STR_CTX_RELOAD));
    AppendMenuW(menu, MF_STRING, IDCT_INIDIR,   L(STR_CTX_INIDIR));
    AppendMenuW(menu, MF_STRING, IDCT_ABOUT,    L(STR_CTX_ABOUT));
    AppendMenuW(menu, MF_STRING, IDCT_HELP,     L(STR_CTX_HELP));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDCT_HIDE,     L(STR_CTX_HIDE));
    AppendMenuW(menu, MF_STRING, IDCT_EXIT,     L(STR_CTX_EXIT));
    // enable/disable the edit block against the current state
    DWORD start = 0, end = 0;
    SendMessageW(g_edit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    bool hasSel = end > start;
    EnableMenuItem(menu, IDCT_UNDO,   MF_BYCOMMAND |
        (SendMessageW(g_edit, EM_CANUNDO, 0, 0) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, IDCT_CUT,    MF_BYCOMMAND | (hasSel ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, IDCT_COPY,   MF_BYCOMMAND | (hasSel ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, IDCT_PASTE,  MF_BYCOMMAND |
        (IsClipboardFormatAvailable(CF_TEXT) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, IDCT_SELALL, MF_BYCOMMAND |
        (GetWindowTextLengthW(g_edit) ? MF_ENABLED : MF_GRAYED));
    if (!g_headless) SetForegroundWindow(hWnd);   // click-away dismisses, like the tray menu
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
// v0.15 theme studio: edits the pending custom theme (saved to themes\<name>.theme.ini).
// Shared fields (alpha/font/geometry) live in g_draft — picked theme pushes them there.
static Theme g_draftTheme;
static HWND  g_thSw[4] = {};              // Bg / Fg / Accent / Border color patches
static HWND  g_thName = nullptr, g_thBorderW = nullptr, g_thFontCombo = nullptr;
static HWND  g_thAlpha = nullptr, g_thW = nullptr, g_thH = nullptr;
static HWND  g_thSize = nullptr, g_thBold = nullptr;
static HWND g_fontCombo = nullptr;   // v0.13: font is picked, not typed
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
        case VK_ESCAPE: wcscpy(buf, L"Esc"); break;      // v0.13: Esc is assignable
        case VK_RETURN: wcscpy(buf, L"Enter"); break;
        case VK_TAB:   wcscpy(buf, L"Tab"); break;
        case VK_CAPITAL: wcscpy(buf, L"Caps"); break;
        case VK_NUMLOCK: wcscpy(buf, L"NumLock"); break;
        case VK_SCROLL:  wcscpy(buf, L"Scroll"); break;
        case VK_UP:    wcscpy(buf, L"Up"); break;
        case VK_DOWN:  wcscpy(buf, L"Down"); break;
        case VK_LEFT:  wcscpy(buf, L"Left"); break;
        case VK_RIGHT: wcscpy(buf, L"Right"); break;
        case VK_INSERT:wcscpy(buf, L"Insert"); break;
        case VK_HOME:  wcscpy(buf, L"Home"); break;
        case VK_END:   wcscpy(buf, L"End"); break;
        case VK_NEXT:  wcscpy(buf, L"PgDn"); break;   // VK_NEXT = page down
        case VK_PRIOR: wcscpy(buf, L"PgUp"); break;   // VK_PRIOR = page up
        // v0.13: media / laptop / OEM keys get real names so the capture box
        // never shows a bare VK number - the user should see what they picked.
        case VK_VOLUME_MUTE:      wcscpy(buf, L"Mute"); break;
        case VK_VOLUME_DOWN:      wcscpy(buf, L"VolDown"); break;
        case VK_VOLUME_UP:        wcscpy(buf, L"VolUp"); break;
        case VK_MEDIA_NEXT_TRACK: wcscpy(buf, L"NextTrack"); break;
        case VK_MEDIA_PREV_TRACK: wcscpy(buf, L"PrevTrack"); break;
        case VK_MEDIA_STOP:       wcscpy(buf, L"Stop"); break;
        case VK_MEDIA_PLAY_PAUSE: wcscpy(buf, L"PlayPause"); break;
        case VK_APPS:    wcscpy(buf, L"Apps"); break;
        case VK_SLEEP:   wcscpy(buf, L"Sleep"); break;
        case VK_SNAPSHOT:wcscpy(buf, L"PrtSc"); break;
        case VK_CANCEL:  wcscpy(buf, L"Break"); break;
        case VK_CLEAR:   wcscpy(buf, L"Clear"); break;
        case VK_ADD:      wcscpy(buf, L"Num+"); break;
        case VK_SUBTRACT: wcscpy(buf, L"Num-"); break;
        case VK_MULTIPLY: wcscpy(buf, L"Num*"); break;
        case VK_DIVIDE:   wcscpy(buf, L"Num/"); break;
        case VK_DECIMAL:  wcscpy(buf, L"Num."); break;
        case VK_OEM_1:     wcscpy(buf, L";"); break;
        case VK_OEM_PLUS:  wcscpy(buf, L"+"); break;
        case VK_OEM_COMMA: wcscpy(buf, L","); break;
        case VK_OEM_MINUS: wcscpy(buf, L"-"); break;
        case VK_OEM_PERIOD:wcscpy(buf, L"."); break;
        case VK_OEM_2:  wcscpy(buf, L"/"); break;
        case VK_OEM_3:  wcscpy(buf, L"`"); break;
        case VK_OEM_4:  wcscpy(buf, L"["); break;
        case VK_OEM_5:  wcscpy(buf, L"\\"); break;
        case VK_OEM_6:  wcscpy(buf, L"]"); break;
        case VK_OEM_7:  wcscpy(buf, L"'"); break;
        default:
            if (hk.key >= VK_NUMPAD0 && hk.key <= VK_NUMPAD9)
                wsprintfW(buf, L"Num%d", (int)hk.key - VK_NUMPAD0);
            else if (hk.key >= VK_F13 && hk.key <= VK_F24)
                wsprintfW(buf, L"F%d", (int)hk.key - VK_F12);
            else if (hk.key >= VK_BROWSER_BACK && hk.key <= VK_BROWSER_FAVORITES) {
                static const wchar_t* bn[] = { L"WebBack", L"WebFwd", L"WebRefresh",
                    L"WebStop", L"WebSearch", L"WebHome", L"WebFav" };
                wcscpy(buf, bn[(int)hk.key - VK_BROWSER_BACK]);
            }
            else if (hk.key == VK_LWIN) wcscpy(buf, L"Win");
            else if (hk.key == VK_RWIN) wcscpy(buf, L"RWin");
            else if ((hk.key >= L'A' && hk.key <= L'Z') ||
                     (hk.key >= L'0' && hk.key <= L'9')) {   // v0.13 single keys
                buf[0] = (wchar_t)hk.key; buf[1] = 0;
            }
            else wsprintfW(buf, L"VK%u", hk.key);
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
            g_draftHk.key = vk;   // v0.13: truly any key, Esc included - the
                                  // dialog's own Cancel button discards drafts
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
// ------------------------------------------------ v0.13.0: font picker support
// The font NAME must be chosen from the list of installed families, not typed
// free-form (typos produced silently-falling-back fonts). Enumerated once.
static int CALLBACK EnumFontProc(ENUMLOGFONTEXW* elf, NEWTEXTMETRICEXW*, DWORD, LPARAM l) {
    auto* v = (std::vector<std::wstring>*)l;
    std::wstring f = elf->elfLogFont.lfFaceName;
    if (!f.empty() && f[0] != L'@')                 // skip vertical CJK variants
        v->push_back(f);
    return 1;
}
static const std::vector<std::wstring>& FontFamilies() {
    static std::vector<std::wstring> fams;
    if (fams.empty()) {
        HDC dc = GetDC(nullptr);
        LOGFONTW lf = {}; lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)EnumFontProc, (LPARAM)&fams, 0);
        ReleaseDC(nullptr, dc);
        std::sort(fams.begin(), fams.end(), [](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(a.c_str(), b.c_str()) < 0; });
        fams.erase(std::unique(fams.begin(), fams.end(),
                     [](const std::wstring& a, const std::wstring& b) {
                         return _wcsicmp(a.c_str(), b.c_str()) == 0; }), fams.end());
    }
    return fams;
}

// Shared by the Font page Bind and the combo CBN_SELCHANGE: the preview must
// follow g_draft instantly while the user tries schemes/fonts out.
static void UpdateFontPreview() {
    if (!g_fontPreview) return;
    static HFONT prev = nullptr;
    if (prev) { DeleteObject(prev); prev = nullptr; }
    prev = CreateFontW(-MulDiv(MulDiv(g_draft.fontSize, 96, 72), g_setDpi, 96), 0, 0, 0,
        g_draft.fontBold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        VARIABLE_PITCH, g_draft.fontName.c_str());
    SendMessageW(g_fontPreview, WM_SETFONT, (WPARAM)prev, TRUE);
    InvalidateRect(g_fontPreview, nullptr, TRUE);
}

static HWND MkOn(HWND host, const wchar_t* cls, const wchar_t* text, DWORD style,
                 int x, int y, int w, int h) {
    // v0.29: every caller speaks 96-logical px; this one place scales them to
    // the settings dialog's dpi (the only host Mk* builds into).
    int sx = Sc(g_setDpi, x), sy = Sc(g_setDpi, y);
    int sw = Sc(g_setDpi, w), sh = Sc(g_setDpi, h);
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             sx, sy, sw, sh, host, nullptr,
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
       IDB_EN_ADDVAR, IDB_EN_UPDATEVAR, IDB_EN_DELETEVAR,
       IDB_TH_SAVEAS, IDB_TH_DEL, IDT_TH_COMBO, IDT_TH_NAME };

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
static HWND CkB(HWND host, const wchar_t* label, int x, int y, bool Settings::*mem) {
    HWND c = MkCheck(host, label, x, y, 380);
    Bind([c, mem]{ SendMessageW(c, BM_SETCHECK, (g_draft.*mem) ? BST_CHECKED : BST_UNCHECKED, 0); },
         [c, mem]{ g_draft.*mem = SendMessageW(c, BM_GETCHECK, 0, 0) == BST_CHECKED; });
    return c;
}
// numeric bound to a Settings int member; returns the edit so pages can resync it
// (v0.15 theme studio pushes alpha/font/geometry into g_draft on theme pick)
static HWND NumB(HWND host, const wchar_t* label, int x, int y, int Settings::*mem,
                 int minv, int maxv, bool allowNeg = false, int labelW = 150, int editW = 80) {
    MkLabel(host, label, x, y + 2, labelW);
    HWND e = MkEdit(host, L"", x + labelW + 5, y, editW, !allowNeg);
    Bind([e, mem]{ wchar_t b[24]; wsprintfW(b, L"%d", g_draft.*mem); SetWindowTextW(e, b); },
         [e, mem, minv, maxv]{
             int v = GetWindowTextLengthW(e) ? _wtoi(GetText(e).c_str()) : g_draft.*mem;
             if (v < minv) v = minv; if (v > maxv) v = maxv;
             g_draft.*mem = v;
         });
    return e;
}
static void TxB(HWND host, const wchar_t* label, int x, int y, std::wstring Settings::*mem) {
    MkLabel(host, label, x, y + 2, 150);
    HWND e = MkEdit(host, L"", x + 155, y, 260);
    Bind([e, mem]{ SetWindowTextW(e, (g_draft.*mem).c_str()); },
         [e, mem]{ g_draft.*mem = Trim(GetText(e)); });
}

// numeric bound to a pending Theme field (theme file, not [Show]/[Font])
static HWND NumT(HWND host, const wchar_t* label, int x, int y, int Theme::*mem,
                 int minv, int maxv, int labelW = 150, int editW = 60) {
    MkLabel(host, label, x, y + 2, labelW);
    HWND e = MkEdit(host, L"", x + labelW + 5, y, editW, true);
    Bind([e, mem]{ wchar_t b[24]; wsprintfW(b, L"%d", (g_draftTheme.*mem > 0) ? g_draftTheme.*mem : 0);
                   SetWindowTextW(e, b); },
         [e, mem, minv, maxv]{
             int v = GetWindowTextLengthW(e) ? _wtoi(GetText(e).c_str()) : 0;
             if (v < minv) v = minv; if (v > maxv) v = maxv;
             g_draftTheme.*mem = v;
         });
    return e;
}

// ---------------------------------------------------------------- v0.15 theme studio
// Built-ins stay compiled-in; customs are themes\<name>.theme.ini (portable).
// Pick a theme -> colors/border go to g_draftTheme, and the shared look fields
// (alpha, font, geometry) push into g_draft so Apply persists them as [Show]/[Font].
static void ThFillCombo() {
    if (!g_themeCombo) return;
    SendMessageW(g_themeCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < ThemeCount(); ++i)
        SendMessageW(g_themeCombo, CB_ADDSTRING, 0, (LPARAM)ThemeAt(i).name.c_str());
    SendMessageW(g_themeCombo, CB_SETCURSEL, ThemeFromName(g_draft.theme), 0);
}
static void ThResync() {                       // push state into the studio controls
    if (!g_thName) return;
    SetWindowTextW(g_thName, g_draftTheme.name.c_str());
    if (g_thBorderW) { wchar_t b[16]; wsprintfW(b, L"%d", g_draftTheme.borderW > 0 ? g_draftTheme.borderW : 0);
                       SetWindowTextW(g_thBorderW, b); }
    if (g_thAlpha)   { wchar_t b[16]; wsprintfW(b, L"%d", g_draft.transparent); SetWindowTextW(g_thAlpha, b); }
    if (g_thW)       { wchar_t b[16]; wsprintfW(b, L"%d", g_draft.width);       SetWindowTextW(g_thW, b); }
    if (g_thH)       { wchar_t b[16]; wsprintfW(b, L"%d", g_draft.height);      SetWindowTextW(g_thH, b); }
    if (g_thSize)    { wchar_t b[16]; wsprintfW(b, L"%d", g_draft.fontSize);    SetWindowTextW(g_thSize, b); }
    if (g_thBold)    SendMessageW(g_thBold, BM_SETCHECK, g_draft.fontBold ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_thFontCombo) {   // keep a custom family visible, never fight the user
        int s = (int)SendMessageW(g_thFontCombo, CB_FINDSTRING, (WPARAM)-1,
                                  (LPARAM)g_draft.fontName.c_str());
        if (s != CB_ERR) SendMessageW(g_thFontCombo, CB_SETCURSEL, s, 0);
    }
    for (int i = 0; i < 4; ++i) if (g_thSw[i]) InvalidateRect(g_thSw[i], nullptr, TRUE);
    if (g_themePrev) InvalidateRect(g_themePrev, nullptr, TRUE);
}
static void ThPick(const Theme& t) {           // combo selection -> editor + shared push
    g_draftTheme = t;
    if (t.alpha >= 0)      g_draft.transparent = t.alpha;
    if (!t.fontName.empty()) g_draft.fontName = t.fontName;
    if (t.fontSize > 0)    g_draft.fontSize = t.fontSize;
    if (t.fontBold >= 0)   g_draft.fontBold = t.fontBold != 0;
    if (t.w > 0)           g_draft.width = t.w;
    if (t.h > 0)           g_draft.height = t.h;
    g_draft.theme = t.name;
    ThResync();
}
static void ThSaveAs(std::wstring name) {
    name = Trim(name);
    for (auto& c : name) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_';
    if (name.empty()) { MessageBeep(MB_ICONWARNING); return; }
    for (int i = 0; i < kThemeCount; ++i)      // never shadow a built-in
        if (_wcsicmp(kThemes[i].name.c_str(), name.c_str()) == 0) { name += L"-copy"; break; }
    CreateDirectoryW(ThemeDirW().c_str(), nullptr);
    std::wstring f = ThemeFileW(name);
    WritePrivateProfileStringW(L"Theme", nullptr, nullptr, f.c_str());   // clean slate
    auto W = [&](const wchar_t* k, const wchar_t* v) {
        WritePrivateProfileStringW(L"Theme", k, v, f.c_str()); };
    auto hex = [](COLORREF c) {                // static per call-site: fine here
        static wchar_t s[8][16]; static int n = 0; n = (n + 1) & 7;
        swprintf(s[n], 16, L"%06X", (unsigned)GetRValue(c) << 16 |
                                   (unsigned)GetGValue(c) << 8 | (unsigned)GetBValue(c));
        return s[n]; };
    W(L"Bg", hex(g_draftTheme.bg));
    W(L"Fg", hex(g_draftTheme.fg));
    W(L"Accent", hex(g_draftTheme.accent));
    if (g_draftTheme.border != CLR_NONE) W(L"Border", hex(g_draftTheme.border));
    wchar_t b[24];
    if (g_draftTheme.borderW >= 0) { swprintf(b, 24, L"%d", g_draftTheme.borderW); W(L"BorderWidth", b); }
    if (g_draftTheme.alpha   >= 0) { swprintf(b, 24, L"%d", g_draftTheme.alpha);   W(L"Alpha", b); }
    if (!g_draftTheme.fontName.empty()) W(L"FontName", g_draftTheme.fontName.c_str());
    if (g_draftTheme.fontSize  >  0) { swprintf(b, 24, L"%d", g_draftTheme.fontSize); W(L"FontSize", b); }
    if (g_draftTheme.fontBold  >= 0) { swprintf(b, 24, L"%d", g_draftTheme.fontBold); W(L"FontBold", b); }
    if (g_draftTheme.w         >  0) { swprintf(b, 24, L"%d", g_draftTheme.w); W(L"Width", b); }
    if (g_draftTheme.h         >  0) { swprintf(b, 24, L"%d", g_draftTheme.h); W(L"Height", b); }
    LoadThemes();
    g_draftTheme.name = name;
    g_draft.theme = name;
    ThFillCombo(); ThResync();
    LogInfo(L"theme: saved themes\\" + name + L".theme.ini");
}
static void ThDeleteSel() {
    int sel = (int)SendMessageW(g_themeCombo, CB_GETCURSEL, 0, 0);
    if (sel < kThemeCount) { MessageBeep(MB_ICONWARNING); return; }   // built-ins are permanent
    std::wstring name = ThemeAt(sel).name;
    DeleteFileW(ThemeFileW(name).c_str());
    LoadThemes();                                                      // file is re-creatable
    g_draftTheme = ThemeAt(0);
    g_draft.theme = kThemes[0].name;
    ThFillCombo(); ThResync();
    LogInfo(L"theme: deleted themes\\" + name + L".theme.ini");
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

// v0.25: the plugins list is a checklist - a click on a row toggles that
// plugin's enable state (the "Load" checkbox of the edit pane stays in sync).
static LRESULT CALLBACK PlChkProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    LRESULT r = DefSubclassProc(h, m, w, l);
    if (m == WM_LBUTTONUP) {
        int idx = (int)SendMessageW(h, LB_ITEMFROMPOINT, 0, MAKELPARAM(LOWORD(l), HIWORD(l)));
        if (LOWORD(idx) < (int)g_plRows.size()) {
            PluginsReadRow();                       // commit the pane being edited
            g_plSel = (int)LOWORD(idx);
            g_plRows[g_plSel].load = g_plRows[g_plSel].load ? 0 : 1;
            SendMessageW(g_plLoadChk, BM_SETCHECK,
                         g_plRows[g_plSel].load ? BST_CHECKED : BST_UNCHECKED, 0);
            InvalidateRect(h, NULL, TRUE);
        }
    }
    return r;
}

static BOOL CALLBACK DdpiFontCb(HWND c, LPARAM f) {
    SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    return TRUE;
}

static LRESULT CALLBACK SettingsProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_DPICHANGED: {                         // v0.29: dialog follows its monitor
        int d = LOWORD(w);
        g_setDpi = (d >= 96 && d <= 480) ? d : 96;
        RECT* pr = (RECT*)l;
        if (pr) SetWindowPos(h, NULL, pr->left, pr->top,
                             pr->right - pr->left, pr->bottom - pr->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
        if (g_uiFont) DeleteObject(g_uiFont);      // pending-delete while still selected
        NONCLIENTMETRICSW ncm = { sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        LOGFONTW lfUI = ncm.lfMessageFont;
        lfUI.lfHeight = MulDiv(lfUI.lfHeight, g_setDpi, 96);
        g_uiFont = CreateFontIndirectW(&lfUI);
        EnumChildWindows(h, DdpiFontCb, (LPARAM)g_uiFont);   // live controls get the new font;
                                                             // full re-layout comes with the next open (MkOn)
        InvalidateRect(h, nullptr, TRUE);
        wchar_t mm[96];
        wsprintfW(mm, L"dpi: settings -> %d (%d%%)", g_setDpi, MulDiv(g_setDpi, 100, 96));
        LogInfo(mm);
        return 0;
    }
    case WM_MEASUREITEM: {                          // plugin checklist row height
        LPMEASUREITEMSTRUCT mi = (LPMEASUREITEMSTRUCT)l;
        // comctl sends this ONCE at list creation (observed itemID 0, docs say
        // -1) BEFORE the scaled g_uiFont arrives - so the height must come
        // from the dpi, never from the not-yet-set font. Accept both ids.
        if (mi->itemID == (UINT)-1 || mi->itemID == 0) {
            mi->itemHeight = Sc(g_setDpi, 20);
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)l;
        if (g_plLb && di->hwndItem == g_plLb) {     // plugin enable checklist (v0.25)
            if (di->itemID >= 0 && di->itemID < (int)g_plRows.size()) {
                bool on = g_plRows[di->itemID].load != 0;
                FillRect(di->hDC, &di->rcItem,
                         GetSysColorBrush((di->itemState & ODS_SELECTED)
                                          ? COLOR_HIGHLIGHT : COLOR_WINDOW));
                {
                    RECT cb = di->rcItem;
                    cb.left += Sc(g_setDpi, 6);
                    cb.top += (cb.bottom - cb.top - Sc(g_setDpi, 13)) / 2;
                    cb.bottom = cb.top + Sc(g_setDpi, 13);
                    cb.right  = cb.left + Sc(g_setDpi, 13);
                    DrawFrameControl(di->hDC, &cb, DFC_BUTTON,
                                     DFCS_BUTTON3STATE | (on ? DFCS_CHECKED : 0));
                }
                {
                    wchar_t nm[128];
                    nm[0] = 0;
                    SendMessageW(g_plLb, LB_GETTEXT, di->itemID, (LPARAM)nm);
                    SetBkMode(di->hDC, TRANSPARENT);
                    SetTextColor(di->hDC,
                                 (di->itemState & ODS_SELECTED)
                                     ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                                     : (on ? GetSysColor(COLOR_WINDOWTEXT)
                                           : GetSysColor(COLOR_GRAYTEXT)));
                    TextOutW(di->hDC, di->rcItem.left + Sc(g_setDpi, 28),
                             di->rcItem.top + Sc(g_setDpi, 2),
                             nm, lstrlenW(nm));
                }
            }
            return TRUE;
        }
        if (g_themePrev && di->hwndItem == g_themePrev) {   // theme preview card
            const Theme& t = g_draftTheme;                 // v0.15: live studio draft
            HBRUSH b = CreateSolidBrush(t.bg);
            FillRect(di->hDC, &di->rcItem, b);
            DeleteObject(b);
            RECT bar = di->rcItem;
            bar.bottom = bar.top + Sc(g_setDpi, 4);
            HBRUSH ab = CreateSolidBrush(t.accent);
            FillRect(di->hDC, &bar, ab);
            DeleteObject(ab);
            if (t.borderW > 0) {                           // frame the theme asks for
                COLORREF bc = (t.border != CLR_NONE) ? t.border : t.accent;
                HBRUSH fb = CreateSolidBrush(bc);
                RECT fr = di->rcItem;
                int bw = t.borderW > 4 ? 4 : t.borderW;
                for (int i = 0; i < bw; ++i) { FrameRect(di->hDC, &fr, fb); InflateRect(&fr, -1, -1); }
                DeleteObject(fb);
            } else {
                FrameRect(di->hDC, &di->rcItem, ab);
            }
            const wchar_t* fam = t.fontName.empty() ? L"Consolas" : t.fontName.c_str();
            int px = 16;
            if (t.fontSize > 0) { px = MulDiv(t.fontSize, 96, 72); if (px > 30) px = 30; if (px < 10) px = 10; }
            HFONT f = CreateFontW(-Sc(g_setDpi, px), 0, 0, 0,
                                  (t.fontBold > 0) ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                  OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  FIXED_PITCH | FF_MODERN, fam);
            HFONT oldF = (HFONT)SelectObject(di->hDC, f);
            SetBkMode(di->hDC, TRANSPARENT);
            int tx = di->rcItem.left + Sc(g_setDpi, 12), ty = di->rcItem.top + Sc(g_setDpi, 24);
            SetTextColor(di->hDC, t.accent);
            TextOutW(di->hDC, tx, ty, L">_", 2);
            std::wstring s = std::wstring(L" TypeBox \u2014 ") + t.name;
            SetTextColor(di->hDC, t.fg);
            TextOutW(di->hDC, tx + Sc(g_setDpi, 22), ty, s.c_str(), (int)s.size());
            SelectObject(di->hDC, oldF);
            DeleteObject(f);
            return TRUE;
        }
        for (int i = 0; i < 4; ++i) {                       // color patch buttons
            if (!g_thSw[i] || di->hwndItem != g_thSw[i]) continue;
            COLORREF c = i == 0 ? g_draftTheme.bg : i == 1 ? g_draftTheme.fg :
                         i == 2 ? g_draftTheme.accent : g_draftTheme.border;
            if (c == CLR_NONE) c = RGB(0x80, 0x80, 0x80);   // "Border" unset = gray
            HBRUSH b = CreateSolidBrush(c);
            FillRect(di->hDC, &di->rcItem, b);
            DeleteObject(b);
            wchar_t hx[16];
            if (i == 3 && g_draftTheme.border == CLR_NONE) lstrcpyW(hx, L"\u2014");
            else swprintf(hx, 16, L"%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, ReadableMix(c, RGB(0, 0, 0), 88));
            SIZE sz; GetTextExtentPoint32W(di->hDC, hx, (int)wcslen(hx), &sz);
            TextOutW(di->hDC, di->rcItem.left + (di->rcItem.right - di->rcItem.left - sz.cx) / 2,
                     di->rcItem.top + (di->rcItem.bottom - di->rcItem.top - sz.cy) / 2,
                     hx, (int)wcslen(hx));
            FrameRect(di->hDC, &di->rcItem, (HBRUSH)GetStockObject(GRAY_BRUSH));
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
        // v0.13: previews must follow the combo instantly - the user tries
        // schemes/fonts out BEFORE pressing Apply (store() alone was too late).
        // v0.15: picking a theme also loads it into the studio (colors/border)
        // and pushes alpha/font/geometry into g_draft.
        if (src == g_themeCombo && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(src, CB_GETCURSEL, 0, 0);
            ThPick(ThemeAt(sel < 0 ? 0 : sel));
            return 0;
        }
        if (src == g_fontCombo && code == CBN_SELCHANGE) {
            wchar_t b[LF_FACESIZE] = {};
            int s = (int)SendMessageW(src, CB_GETCURSEL, 0, 0);
            if (s != CB_ERR && SendMessageW(src, CB_GETLBTEXT, s, (LPARAM)b) > 0) {
                g_draft.fontName = b;
                UpdateFontPreview();
            }
            return 0;
        }
        if (code == BN_CLICKED) {
            if (src == g_plLoadChk && g_plLb) {     // checklist glyph follows the
                PluginsReadRow();                   // per-row "Load" checkbox and
                InvalidateRect(g_plLb, NULL, TRUE); // the other way round as well
            }
            // v0.15 theme studio: click a color patch -> ChooseColorW
            for (int i = 0; i < 4; ++i) {
                if (src != g_thSw[i]) continue;
                COLORREF init = i == 0 ? g_draftTheme.bg : i == 1 ? g_draftTheme.fg :
                                i == 2 ? g_draftTheme.accent :
                                (g_draftTheme.border != CLR_NONE ? g_draftTheme.border : g_draftTheme.accent);
                static COLORREF cust[16] = {};
                CHOOSECOLORW cc = { sizeof(cc) };
                cc.hwndOwner = h;
                cc.Flags = CC_RGBINIT | CC_FULLOPEN;
                cc.lpCustColors = cust;
                cc.rgbResult = init;
                if (ChooseColorW(&cc)) {
                    if (i == 0) g_draftTheme.bg = cc.rgbResult;
                    else if (i == 1) g_draftTheme.fg = cc.rgbResult;
                    else if (i == 2) g_draftTheme.accent = cc.rgbResult;
                    else g_draftTheme.border = cc.rgbResult;
                    InvalidateRect(g_thSw[i], nullptr, TRUE);
                    if (g_themePrev) InvalidateRect(g_themePrev, nullptr, TRUE);
                }
                return 0;
            }
            if (src == g_alLb) return 0;
            if (src) {
                int bid = (int)GetWindowLongW(src, GWLP_ID);   // locale-proof dispatch
                if (bid == IDB_TH_SAVEAS) { ThSaveAs(g_thName ? GetText(g_thName) : L""); return 0; }
                if (bid == IDB_TH_DEL)    { ThDeleteSel(); return 0; }
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
                    lf.lfHeight = -Sc(g_setDpi, MulDiv(g_draft.fontSize, 96, 72));
                    lf.lfWeight = g_draft.fontBold ? FW_BOLD : FW_NORMAL;
                    cf.lpLogFont = &lf; cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
                    cf.hwndOwner = h;
                    if (ChooseFontW(&cf)) {
                        g_draft.fontName = lf.lfFaceName;
                        g_draft.fontSize = MulDiv(-lf.lfHeight, 72, g_setDpi >= 96 ? g_setDpi : 96);
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
    if (m == WM_COMMAND || m == WM_DRAWITEM || m == WM_MEASUREITEM)  // owner-draw controls live on the pages
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

    LoadThemes();   // v0.15: the studio combo must see fresh themes\*.theme.ini
    g_draft = g_set; g_draftHk = g_hotkey; g_draftAliases = g_aliases;
    g_draftTheme = ThemeAt(ThemeFromName(g_set.theme));
    g_aliasSel = 0;

    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    // v0.29: the settings dialog lives at the console's tracked dpi (its owner;
    // the system sends WM_DPICHANGED later if it lands on another monitor).
    // Tracked, not re-queried, is also what lets the suites drive real scaling
    // math on a 96-DPI machine via the synthetic dpichg path.
    // Its message font is scaled the same 96-logical way, controls are laid
    // out in logical px and scaled centrally in MkOn below.
    g_setDpi = g_mainDpi;
    LOGFONTW lfUI = ncm.lfMessageFont;
    lfUI.lfHeight = MulDiv(lfUI.lfHeight, g_setDpi, 96);
    g_uiFont = CreateFontIndirectW(&lfUI);

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int W = 640, H = 540;
    int sx = GetPrivateProfileIntW(L"Coordinates", L"SettingsX", -1, g_iniPath.c_str());
    int sy = GetPrivateProfileIntW(L"Coordinates", L"SettingsY", -1, g_iniPath.c_str());
    if (sx < 0) sx = (wa.right - W) / 2;
    if (sy < 0) sy = (wa.bottom - H) / 2;

    g_setWnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"TypeBoxSettings", (std::wstring(L(STR_DLG_TITLE)) + L" — " + WVersion()).c_str(),
        WS_CAPTION | WS_SYSMENU, sx, sy, Sc(g_setDpi, W), Sc(g_setDpi, H), mainWnd, nullptr, hInst, nullptr);

    g_setList = MkOn(g_setWnd, L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                     8, 8, 140, 360);
    for (int i = 0; i < PG_COUNT; ++i)
        SendMessageW(g_setList, LB_ADDSTRING, 0, (LPARAM)L(kPageStr[i]));
    SendMessageW(g_setList, LB_SETCURSEL, startPage, 0);

    for (int i = 0; i < PG_COUNT; ++i) {
        g_setPage[i] = CreateWindowExW(0, L"TypeBoxPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, Sc(g_setDpi, 156), Sc(g_setDpi, 8),
            Sc(g_setDpi, 470), Sc(g_setDpi, 440), g_setWnd, nullptr, hInst, nullptr);
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
    // v0.13: pick the font from the installed families - typing free text was
    // a typo factory (unknown name => silent fallback to the GDI default).
    MkLabel(g_setPage[PG_FONT], L(STR_FO_NAME), 16, y + 3, 150);
    g_fontCombo = MkOn(g_setPage[PG_FONT], L"COMBOBOX", L"",
                       CBS_DROPDOWNLIST | WS_VSCROLL, 175, y, 260, 320);
    Bind([=]{
        SendMessageW(g_fontCombo, CB_RESETCONTENT, 0, 0);
        for (auto& f : FontFamilies())
            SendMessageW(g_fontCombo, CB_ADDSTRING, 0, (LPARAM)f.c_str());
        int i = (int)SendMessageW(g_fontCombo, CB_FINDSTRINGEXACT, -1,
                                  (LPARAM)g_draft.fontName.c_str());
        if (i == CB_ERR)                        // custom INI value: keep it visible
            i = (int)SendMessageW(g_fontCombo, CB_ADDSTRING, 0,
                                  (LPARAM)g_draft.fontName.c_str());
        SendMessageW(g_fontCombo, CB_SETCURSEL, i, 0);
        UpdateFontPreview(); },
    [=]{ wchar_t b[LF_FACESIZE] = {};
         int s = (int)SendMessageW(g_fontCombo, CB_GETCURSEL, 0, 0);
         if (s != CB_ERR && SendMessageW(g_fontCombo, CB_GETLBTEXT, s, (LPARAM)b) > 0)
             g_draft.fontName = b; });
    y += 28;
    NumB(g_setPage[PG_FONT], L(STR_FO_SIZE), 16, y, &Settings::fontSize, 6, 72);        y += 28;
    CkB(g_setPage[PG_FONT], L(STR_FO_BOLD), 16, y, &Settings::fontBold);                y += 26;
    CkB(g_setPage[PG_FONT], L(STR_FO_APPLY_EN), 16, y, &Settings::applyEnglish);        y += 30;
    MkButton(g_setPage[PG_FONT], L(STR_FO_CHOOSE), 16, y, 110, IDB_FONT_CHOOSE);
    Bind([]{ UpdateFontPreview(); }, []{});

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
    CkB(g_setPage[PG_EXEC], L(STR_EX_PLUGINS),    16, y, &Settings::execPlugins); y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_EX_SCRIPTS),    16, y, &Settings::execScripts); y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_SE_SUSPICIOUS), 16, y, &Settings::confirmSuspicious); y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_SE_DANGER),     16, y, &Settings::confirmDanger);   y += 26;
    CkB(g_setPage[PG_EXEC], L(STR_SE_ELEVATE),    16, y, &Settings::elevate);

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
    CkB(g_setPage[PG_TYPE], L(STR_ET_HIST),    16, y, &Settings::etHistory); y += 30;
    CkB(g_setPage[PG_TYPE], L(STR_DD_SHOW),    16, y, &Settings::dropDown);  y += 26;
    NumB(g_setPage[PG_TYPE], L(STR_DD_ROWS),   16, y, &Settings::ddLines, 1, 20); y += 34;
    CkB(g_setPage[PG_TYPE], L(STR_DD_EMPTY),   16, y, &Settings::ddOnEmpty); y += 26;
    CkB(g_setPage[PG_TYPE], L(STR_FT_FUZZY),   16, y, &Settings::fuzzy);      y += 26;
    CkB(g_setPage[PG_TYPE], L(STR_TY_FRECENCY), 16, y, &Settings::frecency);  y += 26;
    CkB(g_setPage[PG_TYPE], L(STR_FT_LAYOUT),  16, y, &Settings::layoutFallback);

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
    // v0.29.1: rows became dpi-driven 20 logical px (the height v0.25 always
    // INTENDED - comctl silently measured the font instead), so 14 rows need
    // 320px to stay fully visible; the hint label moved below the list.
    // v0.30.0: 18 plugins = 360 rows px, list grew to 400 (hint y=412) so a
    // human sees every plugin at a glance - and tbtest pltoggle still does
    // LB_SETTOPINDEX first for the case rows ever outgrow the list again.
    g_plLb = MkOn(g_setPage[PG_PLUGINS], L"LISTBOX", L"",
                  WS_BORDER | WS_VSCROLL | LBS_NOTIFY |
                  LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 8, 8, 180, 400);
    SetWindowSubclass(g_plLb, PlChkProc, 8, 0);     // click = enable/disable
    int px = 200;
    g_plLoadChk = MkCheck(g_setPage[PG_PLUGINS], L(STR_PL_LOAD), px, 12, 180);
    g_plHistChk = MkCheck(g_setPage[PG_PLUGINS], L(STR_PL_HIST), px, 38, 180);
    g_plInfo = MkOn(g_setPage[PG_PLUGINS], L"STATIC", L"", SS_LEFT | SS_EDITCONTROL,
                    px, 70, 260, 150);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_RESCAN),  px, 240, 80, IDB_PL_RESCAN);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_UNLOAD),  px + 90, 240, 80, IDB_PL_UNLOAD);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_ABOUT),   px, 270, 80, IDB_PL_ABOUT);
    MkButton(g_setPage[PG_PLUGINS], L(STR_PL_OPTIONS), px + 90, 270, 80, IDB_PL_OPTIONS);
    MkLabel(g_setPage[PG_PLUGINS], L(STR_PL_HINT), 8, 412, 190);

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

    // --- Theme (v0.15 theme studio: colors, border thickness, alpha, font, size)
    y = 12;
    MkLabel(g_setPage[PG_THEME], L(STR_TH_SCHEME), 16, y + 2, 110);
    g_themeCombo = MkOn(g_setPage[PG_THEME], L"COMBOBOX", L"",
                        WS_VSCROLL | CBS_DROPDOWNLIST, 130, y, 150, 200);
    SetWindowLongPtrW(g_themeCombo, GWLP_ID, IDT_TH_COMBO);   // testable by id
    MkButton(g_setPage[PG_THEME], L(STR_TH_SAVEAS), 292, y, 104, IDB_TH_SAVEAS);
    MkButton(g_setPage[PG_THEME], L(STR_TH_DEL), 402, y, 52, IDB_TH_DEL);
    ThFillCombo();
    Bind([]{ ThFillCombo(); ThResync(); },
         []{ int sel = (int)SendMessageW(g_themeCombo, CB_GETCURSEL, 0, 0);
             if (sel < 0) sel = 0;
             g_draft.theme = ThemeAt(sel).name; });
    y += 30;
    g_themePrev = MkOn(g_setPage[PG_THEME], L"STATIC", L"", SS_OWNERDRAW, 16, y, 438, 56);
    y += 62;                                                     // 104
    MkLabel(g_setPage[PG_THEME], L(STR_TH_NAMELBL), 16, y + 2, 150);
    g_thName = MkEdit(g_setPage[PG_THEME], L"", 171, y, 150);    // "Save as..." target
    SetWindowLongPtrW(g_thName, GWLP_ID, IDT_TH_NAME);            // testable by id
    {
        const int rows[4] = { 130, 154, 178, 202 };              // color patches
        const StrId strs[4] = { STR_TH_BG, STR_TH_TEXT, STR_TH_ACCENT, STR_TH_BORDER };
        for (int i = 0; i < 4; ++i) {
            MkLabel(g_setPage[PG_THEME], L(strs[i]), 16, rows[i] + 2, 150);
            g_thSw[i] = MkOn(g_setPage[PG_THEME], L"STATIC", L"",
                             SS_OWNERDRAW | SS_NOTIFY, 175, rows[i], 110, 20);
        }
    }
    // border thickness belongs to the theme file; alpha/font/geometry are the
    // shared look fields — Apply persists them as plain [Show]/[Font] keys.
    g_thBorderW = NumT(g_setPage[PG_THEME], L(STR_TH_BORDERW), 16, 226, &Theme::borderW, 0, 24);
    g_thBold    = CkB(g_setPage[PG_THEME], L(STR_FO_BOLD), 262, 228, &Settings::fontBold);
    g_thAlpha   = NumB(g_setPage[PG_THEME], L(STR_CO_ALPHA), 16, 252, &Settings::transparent, 30, 255);
    g_thW       = NumB(g_setPage[PG_THEME], L(STR_CO_WIDTH), 262, 252, &Settings::width, 200, 7680, false, 90, 56);
    g_thH       = NumB(g_setPage[PG_THEME], L(STR_CO_HEIGHT), 16, 278, &Settings::height, 24, 2160, false, 90, 56);
    MkLabel(g_setPage[PG_THEME], L(STR_FO_NAME), 16, 302, 150);
    g_thFontCombo = MkOn(g_setPage[PG_THEME], L"COMBOBOX", L"",
                         CBS_DROPDOWNLIST | WS_VSCROLL, 171, 300, 150, 300);
    Bind([=]{
        SendMessageW(g_thFontCombo, CB_RESETCONTENT, 0, 0);
        for (auto& f : FontFamilies())
            SendMessageW(g_thFontCombo, CB_ADDSTRING, 0, (LPARAM)f.c_str());
        int i = (int)SendMessageW(g_thFontCombo, CB_FINDSTRINGEXACT, -1,
                                  (LPARAM)g_draft.fontName.c_str());
        if (i == CB_ERR)
            i = (int)SendMessageW(g_thFontCombo, CB_ADDSTRING, 0, (LPARAM)g_draft.fontName.c_str());
        SendMessageW(g_thFontCombo, CB_SETCURSEL, i, 0); },
    [=]{ wchar_t b[LF_FACESIZE] = {};
         int s = (int)SendMessageW(g_thFontCombo, CB_GETCURSEL, 0, 0);
         if (s != CB_ERR && SendMessageW(g_thFontCombo, CB_GETLBTEXT, s, (LPARAM)b) > 0)
             g_draft.fontName = b; });
    g_thSize = NumB(g_setPage[PG_THEME], L(STR_FO_SIZE), 330, 302, &Settings::fontSize, 6, 72, false, 60, 50);
    MkLabel(g_setPage[PG_THEME], L(STR_TH_PORTNOTE), 16, 330, 440);

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
    if (g_headless)
        SetWindowPos(g_setWnd, nullptr, -32000, -32000, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);   // off-screen, focus untouched
    else
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
    WB(L"Show", L"DragMove",           s.dragMove);
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
    WB(L"Exec", L"Scripts",  s.execScripts);
    WB(L"Security", L"ConfirmSuspicious", s.confirmSuspicious);
    WB(L"Security", L"ConfirmDanger",     s.confirmDanger);
    WB(L"Security", L"Elevate",           s.elevate);
    WB(L"Exec", L"Plugins",  s.execPlugins);

    // [Update] (v0.20): survives settings saves; official API written as empty.
    {
        std::wstring api = s.updApi;
        if (_wcsicmp(api.c_str(), kUpdateApiDefault) == 0) api.clear();
        WS_(L"Update", L"ApiUrl",       api);
        WB(L"Update",  L"AutoRun",      s.updAutoRun);
        WB(L"Update",  L"CheckOnStart", s.updCheckOnStart);
        WS_(L"Update", L"Latest",       s.updLatest);
        WS_(L"Update", L"LastCheck",    s.updLastCheck);
    }

    WInt(L"Type", L"EasyType",     s.easyType);
    WInt(L"Type", L"Fuzzy",        s.fuzzy ? 1 : 0);
    WInt(L"Type", L"LayoutFallback", s.layoutFallback ? 1 : 0);
    WInt(L"DropDown", L"Enable",              s.dropDown ? 1 : 0);
    WInt(L"DropDown", L"Lines",               s.ddLines);
    WInt(L"DropDown", L"ShowHistoryOnEmpty",  s.ddOnEmpty ? 1 : 0);
    WB(L"Type", L"UseInternalET",  s.etInternal);
    WB(L"Type", L"UseAliasET",     s.etAlias);
    WB(L"Type", L"UseHistoryET",   s.etHistory);
    WB(L"Type", L"UseSysAliasET",  s.etSysAlias);
    WB(L"Type", L"FrecencyRanking", s.frecency);

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
    LoadWebSearch();
    LoadScripts();                         // v0.16 Scripts\ follows Apply too
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
    if (mainWnd) {                         // v0.15: custom frame + edit inset redo
        InvalidateRect(mainWnd, nullptr, TRUE);
        RECT rc; GetClientRect(mainWnd, &rc);
        SendMessageW(mainWnd, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
    }
    g_trayErr = false;
    FreeTrayIcons();                       // badge follows the new accent
    SetTrayVisual();
    DdClose();                             // font/colors may have changed
    if (g_ddFont) { DeleteObject(g_ddFont); g_ddFont = nullptr; }
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
        if (!g_headless) SetForegroundWindow(g_setWnd);
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
    OpenSettings(mainWnd, PG_PLUGINS);    // v0.25: exercise the plugin checklist
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
    // hand-drawn cue banner: EM_SETCUEBANNER needs the v6 common-controls
    // manifest this .rc-less build carries no, and would not reach PrintWindow
    // screenshots either (UX review B1: the four keys must teach themselves)
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        LRESULT r = DefSubclassProc(hWnd, msg, wp, lp);
        HDC dc = (msg == WM_PAINT) ? GetDC(hWnd) : (HDC)wp;
        if (dc && GetWindowTextLengthW(hWnd) == 0) {
            HFONT f = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
            HFONT old = f ? (HFONT)SelectObject(dc, f) : nullptr;
            RECT rc; GetClientRect(hWnd, &rc);
            TEXTMETRICW tm = {}; GetTextMetricsW(dc, &tm);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, MixColor(g_effBg, g_effFg, 45));
            RECT tr = { rc.left + 3, (rc.top + rc.bottom - (int)tm.tmHeight) / 2,
                        rc.right - 2, rc.bottom };
            DrawTextW(dc, L(STR_CUE), -1, &tr, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
            if (old) SelectObject(dc, old);
        }
        if (msg == WM_PAINT && dc) ReleaseDC(hWnd, dc);
        return r;
    }
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    // v0.27: the Ctrl+R keydown is consumed by EditProc, but the message loop's
    // TranslateMessage already queued the matching WM_CHAR - swallow exactly one
    // so the search key never lands in the console (posted key events can carry
    // repeat bits in lParam, so the guard eats the whole char message).
    if (msg == WM_CHAR && (wp == L'r' || wp == L'R')) {
        if (g_hsrchEatChar) { g_hsrchEatChar = false; return 0; }
    } else if (msg == WM_CHAR && g_hsrchEatChar) {
        g_hsrchEatChar = false;               // any other char makes it stale
    } else if (msg == WM_KEYDOWN && g_hsrchEatChar && wp != VK_CONTROL &&
               wp != L'R' && wp != L'r') {
        g_hsrchEatChar = false;
    }
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:      // real Alt+digit arrives as WM_SYSKEYDOWN (Alt+1..9 rows)
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
            if (DdVisible() && g_ddSel >= 0 && g_ddSelKb) { DdPick(true); return 0; }  // run selected row
            Submit(mainWnd);  return 0;
        }
        case VK_ESCAPE:
            if (g_hsrch) {                    // v0.27: Esc leaves bck-i-search,
                g_hsrch = false;              // the line you typed comes back
                SetWindowTextW(hWnd, g_hsrchSaved.c_str());
                SendMessageW(hWnd, EM_SETSEL, -1, 0);
                DdClose();
                return 0;
            }
            if (DdVisible()) { DdClose(); return 0; }   // Esc #1: drop the list
            HideBox(mainWnd); return 0;
        case VK_TAB:
            if (g_hsrch) {                    // v0.27: Tab leaves search mode
                g_hsrch = false;              // and drops the row under the caret in
                if (DdVisible() && g_ddSel >= 0) DdPick(false);
                else DdClose();
                return 0;
            }
            if (DdVisible() && g_ddSel >= 0) { DdPick(false); return 0; }  // accept row
            TabComplete(hWnd);  return 0;
        case VK_UP:
            if (DdVisible() && !g_ddItems.empty()) {
                g_ddSel = g_ddSel <= 0 ? (int)g_ddItems.size() - 1 : g_ddSel - 1;
                g_ddSelKb = true; DdKeepVisible();
                DdSyncTitle(); InvalidateRect(g_ddWnd, nullptr, FALSE);
                return 0;
            }
            if (g_histPos > 0) SetHistory(--g_histPos);
            return 0;
        case VK_DOWN:
            if (DdVisible() && !g_ddItems.empty()) {
                g_ddSel = (g_ddSel + 1) % (int)g_ddItems.size();
                g_ddSelKb = true; DdKeepVisible();
                DdSyncTitle(); InvalidateRect(g_ddWnd, nullptr, FALSE);
                return 0;
            }
            if (g_histPos < g_history.size()) SetHistory(++g_histPos);
            else { g_histPos = g_history.size(); SetHistory(g_histPos); }
            return 0;
        case VK_HOME:
            if (DdVisible() && !g_ddItems.empty()) {
                g_ddSel = 0; g_ddTop = 0; g_ddSelKb = true;
                DdSyncTitle(); InvalidateRect(g_ddWnd, nullptr, FALSE);
                return 0;
            }
            break;
        case VK_END:
            if (DdVisible() && !g_ddItems.empty()) {
                g_ddSel = (int)g_ddItems.size() - 1; g_ddSelKb = true; DdKeepVisible();
                DdSyncTitle(); InvalidateRect(g_ddWnd, nullptr, FALSE);
                return 0;
            }
            break;
        case L'A': case L'a':
            if (ctrl) { SendMessageW(hWnd, EM_SETSEL, 0, -1); return 0; }   // Ctrl+A
            break;
        case L'R': case L'r':
            // v0.27 (RI3 #12): Ctrl+R = bash-style reverse history search.
            // First press: remember the line, the console becomes the query.
            // Every further press cycles the matches, like bash.
            if (ctrl || (GET_KEYSTATE_WPARAM(lp) & MK_CONTROL)) {
                if (!g_hsrch) {
                    g_hsrch = true;
                    int len = GetWindowTextLengthW(hWnd);
                    std::wstring raw(len, L'\0');
                    if (len) GetWindowTextW(hWnd, &raw[0], len + 1);
                    g_hsrchSaved = raw;
                    g_hsrchEatChar = true;
                    SetWindowTextW(hWnd, L"");   // EN_CHANGE -> DdUpdate lists history
                } else if (DdVisible() && !g_ddItems.empty()) {
                    g_ddSel = (g_ddSel + 1) % (int)g_ddItems.size();
                    g_ddSelKb = true;
                    g_hsrchEatChar = true;
                    DdKeepVisible(); DdSyncTitle();
                    InvalidateRect(g_ddWnd, nullptr, FALSE);
                }
                return 0;
            }
            break;
        default:
            if (((GetKeyState(VK_MENU) & 0x8000) || (lp & 0x100000)) &&   // Alt context bit
                wp >= L'1' && wp <= L'9' &&
                DdVisible() && !g_ddItems.empty()) {       // Alt+digit: run a visible row
                int row = g_ddTop + (int)wp - (int)L'1';
                if (row >= 0 && row < (int)g_ddItems.size()) { g_ddSel = row; DdPick(true); }
                return 0;
            }
            break;
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

// ------------------------------------------------ v0.13.0: unrestricted hotkeys
// RegisterHotKey cannot express every key (single keys without modifiers, some
// media/extended keys) and fails outright when another app already owns the
// combination. To honor "the user configures it, no limits", ANY VK can be a
// hotkey through a low-level keyboard hook that matches the exact modifier
// state. The hook only fires while the console is hidden and consumes the key,
// so a single-letter hotkey does not leak into the app behind and still lets
// the user type that letter inside the console.
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
static HHOOK g_hkHook = nullptr;
static HWND  g_hkWnd  = nullptr;     // WM_HOTKEY toggle messages go here
static bool  g_hkHookMode = false;   // hotkey currently served by the hook
static bool  g_hkEdge     = false;   // key-down edge guard (kills auto-repeat)

static LRESULT CALLBACK LLKbProc(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION) {
        KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)l;
        if ((w == WM_KEYDOWN || w == WM_SYSKEYDOWN) && !g_visible &&
            k->vkCode == g_hotkey.key) {
            bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
            bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
            bool win   = (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                         (GetAsyncKeyState(VK_RWIN) & 0x8000);
            if (ctrl == g_hotkey.ctrl && shift == g_hotkey.shift &&
                alt  == g_hotkey.alt  && win  == g_hotkey.win) {
                if (!g_hkEdge && g_hkWnd) {
                    g_hkEdge = true;
                    PostMessageW(g_hkWnd, WM_HOTKEY, kHotkeyId, 0);
                }
                return 1;                    // consume: do not leak to the app
            }
        } else if ((w == WM_KEYUP || w == WM_SYSKEYUP) &&
                   k->vkCode == g_hotkey.key) {
            g_hkEdge = false;
        }
    }
    return CallNextHookEx(g_hkHook, code, w, l);
}

static void ApplyHotkey(HWND mainWnd) {
    UnregisterHotKey(mainWnd, kHotkeyId);
    if (g_hkHook) { UnhookWindowsHookEx(g_hkHook); g_hkHook = nullptr; }
    g_hkHookMode = false;
    g_hkEdge = false;
    g_hkWnd  = mainWnd;

    if (g_headless) {
        // A suite must never steal keys from the human at the keyboard: no OS
        // hotkey, no low-level hook. Probes post WM_HOTKEY(kHotkeyId) instead.
        LogInfo(L"hotkey: " + HotkeyText(g_hotkey) + L" [headless: not registered]");
        return;
    }

    bool wantHook = (g_hotkey.mods() == 0);  // single key: RegisterHotKey would
                                             // eat it system-wide; hook is smarter
    if (!wantHook &&
        !RegisterHotKey(mainWnd, kHotkeyId, g_hotkey.mods() | MOD_NOREPEAT,
                        g_hotkey.key)) {
        wantHook = true;                     // occupied or unsupported: hook still works
        LogError(L"RegisterHotKey failed (key=" + std::to_wstring(g_hotkey.key) +
                 L", mods=" + std::to_wstring(g_hotkey.mods()) +
                 L"); falling back to low-level hook");
    }
    if (wantHook) {
        g_hkHook = SetWindowsHookExW(WH_KEYBOARD_LL, LLKbProc,
                                     (HINSTANCE)GetModuleHandleW(nullptr), 0);
        if (g_hkHook) g_hkHookMode = true;
        else LogError(L"SetWindowsHookEx(WH_KEYBOARD_LL) failed: hotkey disabled");
    }
    LogInfo(L"hotkey: " + HotkeyText(g_hotkey) +
            (g_hkHookMode ? L" [low-level hook]" : L" [RegisterHotKey]"));
}

static void ReloadHotkey(HWND mainWnd) {
    UnregisterHotKey(mainWnd, kHotkeyId);
    LoadSettings();
    ApplyHotkey(mainWnd);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_mainDpi = WindowDpi(hWnd);            // v0.29: this window's dpi (96 on Win7/8)
        g_edit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, hWnd, nullptr, nullptr, nullptr);
        SetWindowSubclass(g_edit, EditProc, 1, (DWORD_PTR)hWnd);
        return 0;
    }
    case WM_DPICHANGED: {                        // v0.29 PMv2: one console, any monitor
        int d = LOWORD(wp);
        g_mainDpi = (d >= 96 && d <= 480) ? d : 96;
        RECT* pr = (RECT*)lp;
        if (pr) SetWindowPos(hWnd, NULL, pr->left, pr->top,
                             pr->right - pr->left, pr->bottom - pr->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
        RebuildSbFont();                         // strip font follows
        ApplyFont(g_edit);                       // console font follows, relayout via WM_SIZE
        if (g_ddWnd) {                           // an open drop-down re-scales too
            g_ddDpi = g_mainDpi;
            if (g_ddFont) { DeleteObject(g_ddFont); g_ddFont = nullptr; }
            DdEnsureFont();
            InvalidateRect(g_ddWnd, nullptr, TRUE);
        }
        InvalidateRect(hWnd, nullptr, TRUE);
        wchar_t m[96];
        wsprintfW(m, L"dpi: -> %d (%d%%)", g_mainDpi, MulDiv(g_mainDpi, 100, 96));
        LogInfo(m);
        return 0;
    }
    // v0.29 suite hooks (tbtest dpichg/fontinfo). A 96-DPI machine cannot get
    // a real WM_DPICHANGED, so the suite injects the very same path: we build
    // the proposed rect the way the system would (origin kept, size MulDiv'd)
    // and re-enter the handler above. lParam is never dereferenced from
    // another process - only the integer wParam crosses the boundary.
    case WM_APP + 31: {
        int d = (int)wp;
        if (d < 96 || d > 480) return 0;
        RECT cur, pr;
        GetWindowRect(hWnd, &cur);
        pr.left = cur.left; pr.top = cur.top;
        // the system's proposed rect converts PHYSICAL size from the window's
        // CURRENT dpi to the new one - the suite chain 96->144->240->96 must
        // round-trip exactly, so the divisor is g_mainDpi, never 96.
        pr.right  = cur.left + MulDiv(cur.right  - cur.left, d, g_mainDpi);
        pr.bottom = cur.top  + MulDiv(cur.bottom - cur.top,  d, g_mainDpi);
        return SendMessageW(hWnd, WM_DPICHANGED, MAKEWPARAM(d, d), (LPARAM)&pr);
    }
    case WM_APP + 32: {
        HFONT f = g_edit ? (HFONT)SendMessageW(g_edit, WM_GETFONT, 0, 0) : nullptr;
        if (!f) return 0;
        HDC dc = GetDC(hWnd);
        HFONT old = (HFONT)SelectObject(dc, f);
        TEXTMETRICW tm = {};
        GetTextMetricsW(dc, &tm);
        SelectObject(dc, old);
        ReleaseDC(hWnd, dc);
        wchar_t m[128];
        wsprintfW(m, L"fontinfo: tmHeight=%d tmAscent=%d dpi=%d",
                  (int)tm.tmHeight, (int)tm.tmAscent, g_mainDpi);
        LogInfo(m);
        return 0;
    }
    case WM_SIZE: {
        // keep the edit centered in the space ABOVE the 20px status strip;
        // height follows the font (UX A4: a fixed 24 clipped the 18pt glyphs)
        // v0.15: a custom theme may draw a bw-px frame -> the edit sits inside it
        int bw = g_borderW > 0 ? g_borderW : 0;
        int above = (int)HIWORD(lp) - SbH() - bw;
        if (above < Sc(g_mainDpi, 16)) above = Sc(g_mainDpi, 16);
        int eh = Sc(g_mainDpi, 24);
        if (HFONT f = (HFONT)SendMessageW(g_edit, WM_GETFONT, 0, 0)) {
            HDC dc = GetDC(hWnd);
            HFONT old = (HFONT)SelectObject(dc, f);
            TEXTMETRICW tm = {}; GetTextMetricsW(dc, &tm);
            SelectObject(dc, old); ReleaseDC(hWnd, dc);
            eh = (int)tm.tmHeight + 6;
            if (eh > above) eh = above;
        }
        MoveWindow(g_edit, 2 + bw, bw + (above - eh) / 2, LOWORD(lp) - 4 - 2 * bw, eh, TRUE);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1 && g_visible) HideBox(hWnd);   // [Show] AutoHide
        else if (wp == 2) {                        // clock: refresh the strip
            RECT rc; GetClientRect(hWnd, &rc);
            rc.top = rc.bottom - SbH();
            if (g_visible) InvalidateRect(hWnd, &rc, TRUE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        if (g_bgBrush) FillRect(dc, &rc, g_bgBrush);
        if (g_sbFont) PaintStatusBar(dc, rc, g_mainDpi);
        PaintFrame(dc, rc);                     // v0.15 custom-theme frame on top
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_PRINTCLIENT: {           // PrintWindow/screenshot capture path
        HDC dc = (HDC)wp;
        RECT rc; GetClientRect(hWnd, &rc);
        if (g_bgBrush) FillRect(dc, &rc, g_bgBrush);
        if (g_sbFont) PaintStatusBar(dc, rc, g_mainDpi);
        PaintFrame(dc, rc);
        return 0;
    }
    case WM_ERASEBKGND:
        if (g_bgBrush) {
            RECT rc; GetClientRect(hWnd, &rc);
            FillRect((HDC)wp, &rc, g_bgBrush);
            PaintFrame((HDC)wp, rc);
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
    // v0.19: drag the console by its 20px status strip. Returning HTCAPTION
    // makes DefWindowProc run the standard system move loop; it ends with
    // WM_EXITSIZEMOVE, where we remember the spot (memory only, see g_dragX).
    // WM_NCHITTEST cannot tell WHICH button is down - and HTCAPTION silently
    // eats right-clicks (no WM_CONTEXTMENU, so no console menu: the v0.19.0
    // regression users hit). Only claim the strip while the LEFT button is
    // down; with the right button the strip behaves like plain client area.
    case WM_NCHITTEST: {
        if (g_set.dragMove && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) == 0) {
            POINT cp = { (int)(short)LOWORD(lp), (int)(short)HIWORD(lp) };
            ScreenToClient(hWnd, &cp);
            RECT rc; GetClientRect(hWnd, &rc);
            if (cp.x >= 0 && cp.x <= rc.right &&
                cp.y >= rc.bottom - SbH() && cp.y < rc.bottom)
                return (LRESULT)HTCAPTION;
        }
        break;
    }
    case WM_NCRBUTTONUP:
        // right-click on the strip (e.g. arriving as non-client after a hit-test
        // race): still open the console menu at the cursor, like everywhere else
        if (wp == HTCAPTION && g_visible) {
            POINT pt = { (int)(short)LOWORD(lp), (int)(short)HIWORD(lp) };
            ShowConsoleMenu(hWnd, pt);
            return 0;
        }
        break;
    case WM_NCLBUTTONDBLCLK:
        if (g_set.dragMove) return 0;   // double-click on strip must not maximize
        break;
    case WM_EXITSIZEMOVE:
        if (g_set.dragMove) {
            RECT rc; GetWindowRect(hWnd, &rc);
            g_dragX = rc.left; g_dragY = rc.top; g_dragged = true;
            g_posMoved = false;   // v0.27.1: a manual drag releases the pos slot
        }
        return 0;
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
    case WM_CONTEXTMENU: {
        // v0.14: right-click anywhere on the console (EDIT forwards it here)
        // opens the TypeAndRun-style popup: edit ops + settings/plugins/about.
        if (!g_visible) return 0;
        POINT pt;
        if ((int)(short)LOWORD(lp) == -1 && (int)(short)HIWORD(lp) == -1)
            GetCursorPos(&pt);                       // keyboard Menu key
        else { pt.x = (int)(short)LOWORD(lp); pt.y = (int)(short)HIWORD(lp); }
        ShowConsoleMenu(hWnd, pt);
        return 0;
    }
    case kTrayMsg: {
        UINT ev = LOWORD(lp);
        if (ev == WM_LBUTTONUP || ev == WM_LBUTTONDBLCLK)
            g_visible ? HideBox(hWnd) : ShowBox(hWnd);
        else if (ev == WM_RBUTTONUP)
            ShowTrayMenu(hWnd);
        return 0;
    }
    case kUpdateMsg: {   // v0.20: opt-in startup check result (thread -> UI)
        std::wstring* tag = (std::wstring*)lp;
        if (tag) {
            if (g_set.balloonTips && g_nid.hWnd && g_set.trayIcon && !g_headless) {
                g_nid.uFlags = NIF_INFO;
                g_nid.dwInfoFlags = NIIF_INFO;
                lstrcpynW(g_nid.szInfoTitle, L"TypeBox", ARRAYSIZE(g_nid.szInfoTitle));
                std::wstring txt = std::wstring(L(STR_UP_ASK_DL));
                // one %s: the tag; leave the rest as-is (informational balloon)
                size_t p = txt.find(L"%s");
                if (p != std::wstring::npos) txt.replace(p, 2, *tag);
                lstrcpynW(g_nid.szInfo, txt.c_str(), ARRAYSIZE(g_nid.szInfo));
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            }
            LogInfo(L"/update: newer release cached: " + *tag);
            delete tag;
        }
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
            DdUpdate(hWnd);                    // keep the suggestion list in sync
            if (g_visible && g_set.autoHide && g_set.autoHideValue > 0) {
                KillTimer(hWnd, 1);            // typing must not get hidden mid-keystroke
                SetTimer(hWnd, 1, (UINT)g_set.autoHideValue * 1000, nullptr);
            }
        }
        if (LOWORD(wp) == 1)      g_visible ? HideBox(hWnd) : ShowBox(hWnd);
        else if (LOWORD(wp) == 2) ShellExecuteW(hWnd, L"open", g_iniPath.c_str(),
                                                nullptr, nullptr, SW_SHOWNORMAL);
        else if (LOWORD(wp) == 3) DestroyWindow(hWnd);
        else if (LOWORD(wp) == 4) OpenSettings(hWnd, PG_APPEAR);
        else if (LOWORD(wp) >= IDCT_UNDO && LOWORD(wp) <= IDCT_EXIT) {
            // console context menu actions (v0.14)
            switch ((int)LOWORD(wp)) {
            case IDCT_UNDO:     SendMessageW(g_edit, EM_UNDO, 0, 0);        break;
            case IDCT_CUT:      SendMessageW(g_edit, WM_CLEAR, 0, 0);       break;
            case IDCT_COPY:     SendMessageW(g_edit, WM_COPY, 0, 0);        break;
            case IDCT_PASTE:    SendMessageW(g_edit, WM_PASTE, 0, 0);       break;
            case IDCT_SELALL:   SendMessageW(g_edit, EM_SETSEL, 0, -1);     break;
            case IDCT_SETTINGS: OpenSettings(hWnd, PG_APPEAR);              break;
            case IDCT_ALIASES:  OpenSettings(hWnd, PG_ALIASES);             break;
            case IDCT_PLUGINS:  OpenSettings(hWnd, PG_PLUGINS);             break;
            case IDCT_THEME:    OpenSettings(hWnd, PG_THEME);               break;
            case IDCT_LANG:     OpenSettings(hWnd, PG_LANG);                break;
            case IDCT_RELOAD:   RunInternal(L"/reread", hWnd);              break;
            case IDCT_INIDIR:   RunInternal(L"/inidir", hWnd);              break;
            case IDCT_ABOUT:    RunInternal(L"/about", hWnd);               break;
            case IDCT_HELP:     RunInternal(L"/help", hWnd);                break;
            case IDCT_HIDE:     HideBox(hWnd);                              break;
            case IDCT_EXIT:     DestroyWindow(hWnd);                        break;
            }
            if (g_visible && g_edit) SetFocus(g_edit);
            return 0;
        }
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
        if (g_hkHook) { UnhookWindowsHookEx(g_hkHook); g_hkHook = nullptr; }
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
    // v0.29 wave: Per-Monitor V2 (Win10 1703+), plain PM (Win8.1+), else the
    // old system awareness. Dynamic import on purpose: no manifest, and a
    // missing export simply falls through to the previous behavior.
    {
        HMODULE u = LoadLibraryW(L"user32.dll");
        SetDpiCtxFn f = u ? (SetDpiCtxFn)GetProcAddress(u, "SetProcessDpiAwarenessContext") : nullptr;
        bool pm2 = false, pm = false;
        if (f) {
            pm2 = f((HANDLE)-4) != 0;                       // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            if (!pm2) pm = f((HANDLE)-3) != 0;              // ..._PER_MONITOR_AWARE
        }
        if (pm2)       g_dpiMode = L"per-monitor v2";
        else if (pm)   g_dpiMode = L"per-monitor";
        else         { SetProcessDPIAware(); g_dpiMode = L"system aware"; }
        if (u) FreeLibrary(u);   // user32 stays loaded anyway (we use it forever)
    }
    DpiInit();
    DetectHeadless();   // v0.27: suites set TYPEBOX_HEADLESS=1 so nothing of this
                        // run can touch the human at the keyboard

    // Smoke-test / user friendly: `TypeBox.exe --version` prints and exits
    // (used by CI to refuse publishing a build that cannot start).
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        bool wantVersion = false;
        bool wantCheckI18n = false;
        bool wantDpiMath = false;
        for (int i = 1; i < argc; ++i)
            if (_wcsicmp(argv[i], L"--version") == 0 || _wcsicmp(argv[i], L"/version") == 0)
                wantVersion = true;
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--test-settings") == 0)
                g_testSettings = true;
            if (_wcsicmp(argv[i], L"--check-i18n") == 0)
                wantCheckI18n = true;
            if (_wcsicmp(argv[i], L"--dpi-math") == 0)
                wantDpiMath = true;
        }
        if (argv) LocalFree(argv);
        // CI gate (v0.13.1): dump the CURRENT language pack's compiled literals to
        // i18n-check.txt next to the exe. In-process on purpose: it proves the
        // compiler (MSVC /utf-8) kept the 20 language packs intact, without any
        // GUI window, IPC or message loop that a headless runner could starve.
        if (wantCheckI18n) {
            g_iniPath = ExeDir() + L"TypeBox.ini";
            SetLanguage(LangFromCode(Trim(ReadIniStr(L"UI", L"Language", L"auto"))));
            std::wstring out = std::wstring(L"lang=") + kLangs[g_langIdx].code + L"\r\n"
                + std::wstring(L(STR_APPLY)) + L"\r\n"
                + std::wstring(L(STR_PG_APPEARANCE)) + L"\r\n"
                + std::wstring(L(STR_PG_THEME)) + L"\r\n"
                + std::wstring(L(STR_SE_DANGER)) + L"\r\n"      // v0.17 tail (side table)
                + std::wstring(L(STR_TY_FRECENCY)) + L"\r\n"
                + std::wstring(L(STR_SO_ADMIN)) + L"\r\n"       // v0.18 su chip + hint
                + std::wstring(L(STR_SU_HINT)) + L"\r\n"
                + std::wstring(L(STR_SE_ELEVATE)) + L"\r\n"
                + std::wstring(L(STR_UP_UPTODATE)) + L"\r\n"    // v0.20 /update dialogs
                + std::wstring(L(STR_UP_ASK_DL)) + L"\r\n"
                + std::wstring(L(STR_HK_HINT));
            std::string bytes = ToUtf8(out);
            auto tryWrite = [&bytes](const std::wstring& p) -> bool {
                HANDLE hf = CreateFileW(p.c_str(), GENERIC_WRITE, 0,
                                        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hf == INVALID_HANDLE_VALUE) return false;
                DWORD wr = 0;
                WriteFile(hf, bytes.c_str(), (DWORD)bytes.size(), &wr, nullptr);
                CloseHandle(hf);
                return wr == bytes.size();
            };
            if (!tryWrite(ExeDir() + L"i18n-check.txt")) {   // fallback: caller's CWD
                wchar_t cwd[MAX_PATH] = {};
                GetCurrentDirectoryW(MAX_PATH, cwd);
                tryWrite(std::wstring(cwd) + L"\\i18n-check.txt");
            }
            return 0;
        }
        if (wantDpiMath) {
            // v0.29 CI/suite gate: proves the PMv2 scaling math (the very Sc()/
            // MulDiv() the window code uses) at every supported dpi, windowless
            // and deterministic - same in-process pattern as --check-i18n.
            g_iniPath = ExeDir() + L"TypeBox.ini";
            LoadSettings();
            static const int ds[] = { 96, 120, 144, 168, 192, 216, 240 };
            std::wstring out = L"dpi-math mode=" + std::wstring(g_dpiMode) + L"\r\n";
            for (int di = 0; di < 7; ++di) {
                int d = ds[di];
                out += L"dpi=" + std::to_wstring(d) +
                    L" show=" + std::to_wstring(Sc(d, 720)) + L"x" + std::to_wstring(Sc(d, 56)) +
                    L" strip=" + std::to_wstring(Sc(d, 20)) +
                    L" sbFont=" + std::to_wstring(MulDiv(11, d, 96)) +
                    L" font=" + std::to_wstring(MulDiv(MulDiv(g_set.fontSize, 96, 72), d, 96)) +
                    L"\r\n";
            }
            out += L"dpi-math OK\r\n";
            std::wstring p = ExeDir() + L"dpi-math.txt";
            HANDLE f = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f != INVALID_HANDLE_VALUE) {
                std::string u8 = ToUtf8(out);
                DWORD wr = 0;
                WriteFile(f, u8.data(), (DWORD)u8.size(), &wr, nullptr);
                CloseHandle(f);
            }
            LogInfo(L"dpi-math: table written (mode " + std::wstring(g_dpiMode) + L")");
            return 0;
        }
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
    {   // v0.17: optional per-machine override file next to the portable config
        std::wstring loc = ExeDir() + L"TypeBox.local.ini";
        if (GetFileAttributesW(loc.c_str()) != INVALID_FILE_ATTRIBUTES) g_localIni = loc;
    }
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
    LoadWebSearch();
    LoadScripts();      // v0.16: Scripts\ folder auto-registration
    LoadHistory();
    LoadUsage();        // v0.17: Usage.dat powers the drop-down's frecency ranking
    DiscoverPlugins();

    // [Show] Priority — process class of TypeBox itself
    DWORD pc = NORMAL_PRIORITY_CLASS;
    if      (g_set.priority == L"idle")     pc = IDLE_PRIORITY_CLASS;
    else if (g_set.priority == L"high")     pc = HIGH_PRIORITY_CLASS;
    else if (g_set.priority == L"realtime") pc = REALTIME_PRIORITY_CLASS;
    SetPriorityClass(GetCurrentProcess(), pc);
    LoadThemes();         // v0.15: themes\*.theme.ini (portable custom themes)
    ApplyThemeVisual();   // theme + [Color] -> g_effBg/g_effFg/g_accent + frame + g_bgBrush

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

    RebuildSbFont();                          // v0.29: 11 logical px @ console dpi
    ApplyThemeVisual();
    ApplyFont(g_edit);
    CallPluginsLoad(mainWnd);

    ApplyHotkey(mainWnd);   // v0.13: RegisterHotKey + low-level hook fallback

    {
        size_t enabled = 0;
        for (auto& p : g_plugins)
            if (PluginLoadAllowed(p)) ++enabled;
        LogInfo(L"TypeBox " + WVersion() + L" started; " +
                std::to_wstring(g_aliases.size()) + L" aliases, " +
                std::to_wstring(g_plugins.size()) + L" plugins, " +
                std::to_wstring(enabled) + L" enabled");
        LogInfo(L"dpi: " + std::wstring(g_dpiMode) + L", console " +
                std::to_wstring(g_mainDpi) + L" (window dpi)");
        if (!g_pathDirs.empty() || !g_envVars.empty())
            LogInfo(L"own PATH: " + std::to_wstring(g_pathDirs.size()) +
                    L" folders, variables: " + std::to_wstring(g_envVars.size()));
        LogInfo(L"language: " + std::wstring(kLangs[g_langIdx].code) +
                L" (" + (g_set.langCode.empty() ? L"auto" : g_set.langCode) + L")");
        LogInfo(L"theme: " + g_set.theme);
        LogInfo(L"dropdown: " + std::wstring(g_set.dropDown ?
                L"on (" + std::to_wstring(g_set.ddLines) + L" rows)" : L"off"));
        LogInfo(L"fuzzy: " + std::wstring(g_set.fuzzy ? L"on" : L"off (prefix mode)"));
        LogInfo(L"websearch: " + (g_webLinks.empty()
                 ? std::wstring(L"off")
                 : std::to_wstring(g_webLinks.size()) + L" keywords"));
        LogInfo(L"scripts: " + (g_scripts.empty()
                 ? std::wstring(L"none (Scripts\\ folder)")
                 : std::to_wstring(g_scripts.size()) + L" registered"));
        LogInfo(L"local config: " + (g_localIni.empty()
                 ? std::wstring(L"absent") : std::wstring(L"on (TypeBox.local.ini)")));
        if (!g_usage.empty())
            LogInfo(L"usage: " + std::to_wstring(g_usage.size()) + L" commands ranked");
    }

    AddTrayIcon(mainWnd, true);
    SetTrayVisual();                             // cozy badge replaces the default icon
    SetTimer(mainWnd, 2, 15000, nullptr);   // status bar clock refresh
    if (g_set.showOnStart) ShowBox(mainWnd);

    // v0.20: opt-in silent release check (default off - offline-first).
    if (g_set.updCheckOnStart && !g_testSettings)
        CreateThread(nullptr, 0, UpdateCheckThread, mainWnd, 0, nullptr);

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
        // Esc is handled in EditProc (dropdown first, then hide) - no pump hijack.
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
