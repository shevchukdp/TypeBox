// TypeBox — universal hotkey console + bash + launcher for Windows.
// Early Phase-1 scaffold. Licensed under GPL-3.0 (see LICENSE / docs/PLAN.md).
//
// Roots: TypeAndRun by Evgeniy Galantsev (github.com/galancev/typeandrun, GPL-3.0)
// — command/alias engine and INI schema continue that project.
// Inspired by: Microsoft PowerToys Run, Flow Launcher, Wox, Raycast, Alfred,
// ConEmu (portable INI config), Guake/Yakuake/Drop (console/bash mode).
//
// Build (MSVC):
//   cl /EHsc /std:c++17 /DUNICODE /D_UNICODE src\main.cpp /Fe:TypeBox.exe ^
//      /link user32.lib shell32.lib shlwapi.lib gdi32.lib comctl32.lib
// Build (CMake): cmake -S . -B build && cmake --build build --config Release

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

static const wchar_t* kClassName = L"TypeBoxMain";
static const wchar_t* kEditName  = L"TypeBoxEdit";
static const int      kHotkeyId  = 666; // same lineage as TypeAndRun HotKeyId=666

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

static Hotkey                    g_hotkey;
static bool                      g_visible = false;
static HWND                      g_edit    = nullptr;
static std::wstring              g_iniPath;
static std::vector<std::wstring> g_history;
static size_t                    g_histPos = 0;

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

static std::wstring Trim(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return L"";
    size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::wstring FirstWord(const std::wstring& s) {
    size_t sp = s.find_first_of(L" \t");
    return sp == std::wstring::npos ? s : s.substr(0, sp);
}

static std::wstring RestWords(const std::wstring& s) {
    size_t sp = s.find_first_of(L" \t");
    return sp == std::wstring::npos ? L"" : Trim(s.substr(sp + 1));
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
                    val = d.substr(0, d.size() - 1); // strip trailing backslash, as %TARDIR% did
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
    std::vector<std::wstring> words;
    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && (input[i] == L' ' || input[i] == L'\t')) ++i;
        size_t b = i;
        while (i < input.size() && input[i] != L' ' && input[i] != L'\t') ++i;
        if (i > b) words.push_back(input.substr(b, i - b));
    }
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
    return at != std::wstring::npos && at > 0 && s.find(L'.', at) != std::wstring::npos;
}

static std::wstring GetEditText(HWND edit) {
    int len = GetWindowTextLengthW(edit);
    std::wstring s(len, L'\0');
    GetWindowTextW(edit, &s[0], len + 1);
    return Trim(s);
}

// ---------------------------------------------------------------- execution

static void RunInternal(const std::wstring& line, HWND mainWnd) {
    std::wstring c = line;
    for (auto& ch : c) ch = (wchar_t)tolower(ch);
    if (c == L"/help") {
        MessageBoxW(mainWnd,
            L"TypeBox — universal hotkey console + launcher.\n\n"
            L"Type an alias, app name, URL, e-mail, file or command.\n"
            L"/help  /hide  /config  /exit",
            L"TypeBox", MB_OK | MB_ICONINFORMATION);
    } else if (c == L"/hide") {
        ShowWindow(mainWnd, SW_HIDE);
    } else if (c == L"/config") {
        ShellExecuteW(mainWnd, L"open", g_iniPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (c == L"/exit") {
        PostQuitMessage(0);
    }
}

// Execute one typed line: internal → alias → URL → email → path → shell.
// Mirrors the TypeAndRun ParseConsole chain (Static.pas), kept minimal here.
static bool RunLine(const std::wstring& line, HWND mainWnd) {
    if (line.empty()) return false;

    if (line[0] == L'/') { RunInternal(line, mainWnd); return true; }

    std::wstring word = FirstWord(line);
    std::wstring rest = RestWords(line);

    // 1) alias from [Aliases] in the portable ini
    std::wstring alias = ReadIniStr(L"Aliases", word.c_str(), L"");
    if (!alias.empty()) {
        std::wstring cmd = ExpandEnv(Substitute(alias, rest));
        std::wstring prog = FirstWord(cmd);
        std::wstring args = RestWords(cmd);
        ShellExecuteW(mainWnd, L"open", prog.c_str(),
                      args.empty() ? nullptr : args.c_str(), nullptr, SW_SHOWNORMAL);
        return true;
    }

    // 2) URL
    std::wstring expanded = ExpandEnv(line);
    if (LooksLikeUrl(expanded)) {
        std::wstring url = expanded;
        if (_wcsnicmp(url.c_str(), L"www.", 4) == 0) url = L"http://" + url;
        ShellExecuteW(mainWnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }

    // 3) email
    if (LooksLikeEmail(expanded)) {
        std::wstring mail = expanded;
        size_t sp;
        while ((sp = mail.find(L' ')) != std::wstring::npos) mail.replace(sp, 1, L"%20");
        ShellExecuteW(mainWnd, L"open", (L"mailto:" + mail).c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }

    // 4) file or folder
    if (PathFileExistsW(expanded.c_str())) {
        ShellExecuteW(mainWnd, L"open", expanded.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }

    // 5) shell mode ("bash-ish"): cmd.exe /c <line>
    std::wstring shellCmd  = ReadIniStr(L"Shell", L"Command", L"cmd.exe");
    std::wstring shellArgs = ReadIniStr(L"Shell", L"Args", L"/c");
    ShellExecuteW(mainWnd, L"open", shellCmd.c_str(),
                  (shellArgs + L" " + expanded).c_str(), nullptr, SW_SHOWNORMAL);
    return true;
}

// ---------------------------------------------------------------- UI

static void ShowBox(HWND mainWnd) {
    RECT wr; GetWindowRect(GetDesktopWindow(), &wr);
    int w = ReadIniInt(L"Show", L"Width", 720);
    int x = (wr.right - w) / 2;
    int y = ReadIniInt(L"Show", L"CenterScreenY", 1) ? (wr.bottom) / 3
                                                     : ReadIniInt(L"Show", L"Y", 100);
    SetWindowPos(mainWnd, HWND_TOPMOST, x, y, w, 40, SWP_SHOWWINDOW);
    SetForegroundWindow(mainWnd);
    SetFocus(g_edit);
    SendMessageW(g_edit, EM_SETSEL, 0, -1);
    g_visible  = true;
    g_histPos  = g_history.size();
}

static void HideBox(HWND mainWnd) {
    ShowWindow(mainWnd, SW_HIDE);
    SetWindowTextW(g_edit, L"");
    g_visible = false;
}

static void Submit(HWND mainWnd) {
    std::wstring line = GetEditText(g_edit);
    if (line.empty()) return;
    g_history.push_back(line);
    if (g_history.size() > (size_t)ReadIniInt(L"History", L"Max", 200))
        g_history.erase(g_history.begin());
    RunLine(line, mainWnd);
    SetWindowTextW(g_edit, L"");
    HideBox(mainWnd);
}

static void SetHistory(HWND mainWnd, size_t idx) {
    if (idx < g_history.size()) SetWindowTextW(g_edit, g_history[idx].c_str());
    else                         SetWindowTextW(g_edit, L"");
}

static LRESULT CALLBACK EditProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp,
                                 UINT_PTR id, DWORD_PTR ref) {
    HWND mainWnd = (HWND)ref;
    switch (msg) {
    case WM_KEYDOWN:
        switch (wp) {
        case VK_RETURN: Submit(mainWnd);          return 0;
        case VK_ESCAPE: HideBox(mainWnd);         return 0;
        case VK_UP:
            if (g_histPos > 0) SetHistory(mainWnd, --g_histPos);
            return 0;
        case VK_DOWN:
            if (g_histPos < g_history.size()) SetHistory(mainWnd, ++g_histPos);
            else { g_histPos = g_history.size(); SetHistory(mainWnd, g_histPos); }
            return 0;
        }
        break;
    }
    return DefSubclassProc(hWnd, msg, wp, lp);
}

static void LoadHotkey() {
    g_hotkey.key    = (WORD)ReadIniInt(L"HotKey", L"Key", VK_F2);
    g_hotkey.alt    = ReadIniInt(L"HotKey", L"AltKey", 0)   != 0;
    g_hotkey.ctrl   = ReadIniInt(L"HotKey", L"CtrlKey", 0)  != 0;
    g_hotkey.shift  = ReadIniInt(L"HotKey", L"ShiftKey", 0) != 0;
    g_hotkey.win    = ReadIniInt(L"HotKey", L"WinKey", 0)   != 0;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_edit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, hWnd, nullptr, nullptr, nullptr);
        SendMessageW(g_edit, WM_SETFONT,
            (WPARAM)CreateFontW(18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                FIXED_PITCH | FF_MODERN, L"Courier New"),
            TRUE);
        SetWindowSubclass(g_edit, EditProc, 1, (DWORD_PTR)hWnd);
        return 0;
    }
    case WM_SIZE:
        MoveWindow(g_edit, 2, 2, LOWORD(lp) - 4, 28, TRUE);
        return 0;
    case WM_HOTKEY:
        if (wp == kHotkeyId) g_visible ? HideBox(hWnd) : ShowBox(hWnd);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && g_visible &&
            ReadIniInt(L"Show", L"HideOnDeactivate", 1))
            HideBox(hWnd);
        return 0;
    case WM_DESTROY:
        UnregisterHotKey(hWnd, kHotkeyId);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

// ---------------------------------------------------------------- entry

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g_iniPath = ExeDir() + L"TypeBox.ini";
    InitCommonControls();

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kClassName;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND mainWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kClassName, L"TypeBox", WS_POPUP,
        0, 0, 720, 40, nullptr, nullptr, hInst, nullptr);
    if (!mainWnd) return 1;

    LoadHotkey();
    if (!RegisterHotKey(mainWnd, kHotkeyId, g_hotkey.mods(), g_hotkey.key)) {
        MessageBoxW(nullptr,
            L"TypeBox: failed to register the hotkey.\n"
            L"Check [HotKey] in TypeBox.ini.",
            L"TypeBox", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowBox(mainWnd); // start visible so the user can immediately type

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
