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
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <string>
#include <vector>
#include <fstream>

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

struct Settings {
    // [Show]
    int  width = 720, height = 40, x = -1, y = -1;
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
    int  fontSize = 18, fontBold = 1;
    // [Type] EasyType completion
    int  easyType = 2;         // 0=off, 1=easy, 2=linux-style (Tab completes first word)
    bool etInternal = true, etAlias = true, etHistory = true;
    // [Color] scheme 0 (COLORREF integers, -1 = system default)
    int  consoleBg = -1, consoleFg = -1;
    // [History] [Undo]
    int  historyMax = 200;
    bool useUndo = true;
    bool saveInternal = true, saveAlias = true, saveWww = true, saveEmail = true,
         savePath = true, savePlugins = true, saveUnknown = true;
    // [Sounds]
    bool enableSounds = true;
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
static std::wstring              g_iniPath;
static std::vector<std::wstring> g_history;
static size_t                    g_histPos = 0;
static std::vector<Alias>        g_aliases;
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

// Replace %VAR% (any env var, case-insensitive) and %TYPEBOXDIR%.
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
                    wchar_t buf[4096] = {};
                    DWORD n = GetEnvironmentVariableW(name.c_str(), buf, 4096);
                    if (n > 0 && n < 4096) val = buf;
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

// ---------------------------------------------------------------- settings

static void LoadSettings() {
    g_set.width             = ReadIniInt(L"Show", L"Width", 720);
    g_set.height            = ReadIniInt(L"Show", L"Height", 40);
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

    g_set.easyType   = ReadIniInt(L"Type", L"EasyType", 2);
    g_set.etInternal = ReadIniBool(L"Type", L"UseInternalET", true);
    g_set.etAlias    = ReadIniBool(L"Type", L"UseAliasET", true);
    g_set.etHistory  = ReadIniBool(L"Type", L"UseHistoryET", true);

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

    g_hotkey.key    = (WORD)ReadIniInt(L"HotKey", L"Key", VK_F2);
    g_hotkey.alt    = ReadIniBool(L"HotKey", L"AltKey",   false);
    g_hotkey.ctrl   = ReadIniBool(L"HotKey", L"CtrlKey",  false);
    g_hotkey.shift  = ReadIniBool(L"HotKey", L"ShiftKey", false);
    g_hotkey.win    = ReadIniBool(L"HotKey", L"WinKey",   false);
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

// Play the per-execution-type sound from [Sounds] (TypeAndRun parity).
// Values may be absolute or relative to the TypeBox folder (bundled Sounds\).
static void PlayTypeSound(const wchar_t* key, const wchar_t* fallback) {
    if (!g_set.enableSounds) return;
    std::wstring s = ReadIniStr(L"Sounds", key, fallback);
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
        lstrcpynW(g_nid.szInfo,
                  L"Press the hotkey to open. Right-click the icon for the menu.",
                  ARRAYSIZE(g_nid.szInfo));
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

template <typename T>
static T GetProc(HMODULE h, const char* name) {
    return reinterpret_cast<T>(reinterpret_cast<uintptr_t>(GetProcAddress(h, name)));
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
        if (!h) return;
        Plugin p;
        p.file = file;
        p.hmod = h;
        BindPlugin(p);
        if (!p.RunString && !p.GetInfo) { FreeLibrary(h); return; } // not a TypeBox plugin
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
        if (p.Load && !p.loaded) { p.Load(mainWnd); p.loaded = true; }
}

static void CallPluginsUnload() {
    for (auto& p : g_plugins)
        if (p.Unload && p.loaded) { p.Unload(); p.loaded = false; }
}

// Ask plugins to handle the line (first plugin that returns run != 0 wins).
static ExecType RunPlugins(const std::wstring& line, HWND mainWnd) {
    std::string ansi = ToAnsi(line);
    for (auto& p : g_plugins) {
        if (!p.RunString) continue;
        if (!p.loaded && p.Load) { p.Load(mainWnd); p.loaded = true; }
        TExec ex = p.RunString(ansi.c_str());
        if (ex.size == (int)sizeof(TExec) && ex.run) {
            std::wstring text = FromAnsi(ex.con_text);
            if (!text.empty()) {
                SetWindowTextW(g_edit, text.c_str());
                SendMessageW(g_edit, EM_SETSEL, ex.con_sel_start,
                             ex.con_sel_start + (ex.con_sel_length < 0 ? 0 : ex.con_sel_length));
            }
            PlayTypeSound(L"ExecutePluginSound", L"Sounds\\plugin.wav");
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

// ---------------------------------------------------------------- internal commands
//
// The 14 internal commands of TypeAndRun (Static.pas), kept 1:1 by name.

static void RunInternal(const std::wstring& line, HWND mainWnd) {
    std::wstring c = Lower(FirstWord(line));

    if (c == L"/help") {
        std::wstring msg =
            L"TypeBox " + WVersion() + L" — universal hotkey console + launcher.\n\n"
            L"Type an alias, app name, URL, e-mail, file or command and press Enter.\n"
            L"Up/Down browse history, Esc hides, Ctrl+Z restores the last command.\n\n"
            L"Internal commands:\n"
            L"/about /checkconfig /config /exit /help /hide /homedir\n"
            L"/inidir /inifiles /load /reread /settings /show /unload\n"
            L"TypeBox additions: /export <file>  /import <file>  /undo";
        MessageBoxW(mainWnd, msg.c_str(), L"TypeBox — help", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/about") {
        std::wstring msg =
            L"TypeBox " + WVersion() + L"\n\n"
            L"Universal hotkey console + bash + launcher for Windows.\n"
            L"Portable INI config, aliases with %s/%1..%n, plugins.\n\n"
            L"Roots: continues TypeAndRun by Evgeniy Galantsev\n"
            L"(github.com/galancev/typeandrun, GPL-3.0).\n"
            L"Inspired by PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,\n"
            L"ConEmu and drop-down terminals (Guake/Yakuake/Drop).";
        MessageBoxW(mainWnd, msg.c_str(), L"About TypeBox", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/checkconfig") {
        bool iniOk = PathFileExistsW(g_iniPath.c_str());
        wchar_t msg[512];
        wsprintfW(msg,
            L"TypeBox.ini: %s\nAliases loaded: %d\nHistory items: %d\nHotkey registered: %s",
            iniOk ? L"found" : L"MISSING (using defaults)",
            (int)g_aliases.size(), (int)g_history.size(),
            g_hotkey.key ? L"yes" : L"no");
        MessageBoxW(mainWnd, msg, L"TypeBox — check config", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/config" || c == L"/settings") {
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
            }
        }
    } else if (c == L"/undo") {
        if (g_set.useUndo && !g_lastRun.empty()) {
            ShowBox(mainWnd);
            SetWindowTextW(g_edit, g_lastRun.c_str());
            SendMessageW(g_edit, EM_SETSEL, -1, 0);
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
    case EXC_PLUGIN:   return g_set.savePlugins;
    default:           return g_set.saveUnknown;
    }
}

static int RunLine(const std::wstring& line, HWND mainWnd) {
    if (line.empty()) return EXC_NONE;

    if (line[0] == L'/' && g_set.execInternal) {
        PlayTypeSound(L"ExecuteInternalSound", L"Sounds\\internal.wav");
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
            if (!ShellExecuteW(mainWnd, L"open", prog.c_str(),
                               args.empty() ? nullptr : args.c_str(),
                               a->dir.empty() ? nullptr : a->dir.c_str(),
                               a->state)) {
                // fall through to shell for shell built-ins
            }
            PlayTypeSound(L"ExecuteAliasSound", L"Sounds\\alias.wav");
            return EXC_ALIAS;
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
        PlayTypeSound(L"ExecuteURLSound", L"Sounds\\url.wav");
        return EXC_URL;
    }

    // 3) email
    if (g_set.execEmail && LooksLikeEmail(expanded)) {
        std::wstring mail = expanded;
        size_t sp;
        while ((sp = mail.find(L' ')) != std::wstring::npos) mail.replace(sp, 1, L"%20");
        ShellExecuteW(mainWnd, L"open", (L"mailto:" + mail).c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
        PlayTypeSound(L"ExecuteEmailSound", L"Sounds\\email.wav");
        return EXC_EMAIL;
    }

    // 4) file or folder
    if (g_set.execFiles) {
        DWORD attr = GetFileAttributesW(expanded.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            bool dir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
            ShellExecuteW(mainWnd, L"open", expanded.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            PlayTypeSound(dir ? L"ExecuteFolderSound" : L"ExecuteFileSound",
                          dir ? L"Sounds\\folder.wav" : L"Sounds\\file.wav");
            return dir ? EXC_FOLDER : EXC_FILE;
        }
    }

    // 5) shell mode ("bash-ish"): cmd.exe /c <line>
    if (g_set.execShell) {
        std::wstring shellCmd  = ReadIniStr(L"Shell", L"Command", L"cmd.exe");
        std::wstring shellArgs = ReadIniStr(L"Shell", L"Args", L"/c");
        ShellExecuteW(mainWnd, L"open", shellCmd.c_str(),
                      (shellArgs + L" " + expanded).c_str(), nullptr, SW_SHOWNORMAL);
        PlayTypeSound(L"ExecuteShellSound", L"Sounds\\shell.wav");
        return EXC_SHELL;
    }

    return EXC_NONE;
}

// ---------------------------------------------------------------- UI

static void ApplyFont(HWND edit) {
    HFONT f = CreateFontW(-MulDiv(g_set.fontSize, 96, 72), 0, 0, 0,
        g_set.fontBold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, g_set.fontName.c_str());
    SendMessageW(edit, WM_SETFONT, (WPARAM)f, TRUE);
}

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
    SendMessageW(g_edit, EM_SETSEL, 0, -1);
    g_visible = true;
    g_histPos = g_history.size();
    ResetET();
    if (g_set.autoHide && g_set.autoHideValue > 0)
        SetTimer(mainWnd, 1, (UINT)g_set.autoHideValue * 1000, nullptr);
}

static void HideBox(HWND mainWnd) {
    ShowWindow(mainWnd, SW_HIDE);
    SetWindowTextW(g_edit, L"");
    g_visible = false;
    KillTimer(mainWnd, 1);
    ResetET();
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
                     L"/inidir", L"/inifiles", L"/load", L"/reread", L"/settings",
                     L"/show", L"/unload", L"/undo" })
                    add(cmd);
            }
            if (g_set.etAlias)
                for (const auto& a : g_aliases) add(a.name);
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
    AppendMenuW(menu, MF_STRING, 1, L"Show / Hide");
    AppendMenuW(menu, MF_STRING, 2, L"Config (TypeBox.ini)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"Exit");
    SetForegroundWindow(hWnd); // so the menu dismisses on click-away
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(menu);
}

static LRESULT CALLBACK EditProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp,
                                 UINT_PTR id, DWORD_PTR ref) {
    HWND mainWnd = (HWND)ref;
    (void)id;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    switch (msg) {
    case WM_KEYDOWN:
        switch (wp) {
        case VK_RETURN: Submit(mainWnd);  return 0;
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
    RegisterHotKey(mainWnd, kHotkeyId, g_hotkey.mods(), g_hotkey.key);
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
    case WM_SIZE:
        MoveWindow(g_edit, 2, (HIWORD(lp) - 24) / 2, LOWORD(lp) - 4, 24, TRUE);
        return 0;
    case WM_TIMER:
        if (wp == 1 && g_visible) HideBox(hWnd); // [Show] AutoHide
        return 0;
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
            if (g_set.consoleBg >= 0) SetBkColor(dc, (COLORREF)g_set.consoleBg);
            if (g_set.consoleFg >= 0) SetTextColor(dc, (COLORREF)g_set.consoleFg);
            return (LRESULT)g_bgBrush;
        }
        break;
    case WM_HOTKEY:
        if (wp == kHotkeyId) g_visible ? HideBox(hWnd) : ShowBox(hWnd);
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
        if (LOWORD(wp) == 1)      g_visible ? HideBox(hWnd) : ShowBox(hWnd);
        else if (LOWORD(wp) == 2) ShellExecuteW(hWnd, L"open", g_iniPath.c_str(),
                                                nullptr, nullptr, SW_SHOWNORMAL);
        else if (LOWORD(wp) == 3) DestroyWindow(hWnd);
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
    LoadAliases();
    LoadHistory();
    DiscoverPlugins();
    if (g_set.consoleBg >= 0)
        g_bgBrush = CreateSolidBrush((COLORREF)g_set.consoleBg);

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kClassName;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND mainWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kClassName, L"TypeBox", WS_POPUP,
        0, 0, g_set.width, g_set.height, nullptr, nullptr, hInst, nullptr);
    if (!mainWnd) return 1;

    ApplyFont(g_edit);
    CallPluginsLoad(mainWnd);

    if (!RegisterHotKey(mainWnd, kHotkeyId, g_hotkey.mods(), g_hotkey.key)) {
        MessageBoxW(mainWnd,
            L"TypeBox: failed to register the hotkey.\n"
            L"Check [HotKey] in TypeBox.ini, then run /reread.",
            L"TypeBox", MB_OK | MB_ICONERROR);
    }

    AddTrayIcon(mainWnd, true);
    if (g_set.showOnStart) ShowBox(mainWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
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
