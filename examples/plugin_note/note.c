/* note.dll - TypeBox quick-capture plugin (v0.25.0, user request).
 *
 *   note                 open the capture editor (clipboard text prefilled)
 *   note <text>          save one markdown note right now
 *   note + <text>        append "- [HH:mm] text" to today's journal
 *   note list            today's notes in the folder
 *   note open            open the notes folder in Explorer
 *
 * Conventions borrowed from the quick-capture world (studied before writing):
 * Obsidian Daily Notes / QuickAdd: journal file YYYY-MM-DD.md, captures land
 * as "- HH:mm text" bullets, "capture first, organize later"; timestamp-based
 * unique filenames for nameless captures (Easy Link to Daily Note); Joplin:
 * single flat folder, created on demand, date-first names sort naturally.
 * Everything is a plain .md file in [Notes] Folder - your vault, not ours.
 *
 * Offline-first: the plugin NEVER touches the network. Files live next to the
 * exe by default (<exe>\Notes) or wherever [Notes] Folder points.
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NOTE_BUILDING_DLL
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API
#endif

typedef struct {
    int size;
    const char* name;
    const char* version;
    const char* description;
    const char* author;
    const char* copyright;
    const char* homepage;
} TInfo;

typedef struct {
    int size;
    int run;
    const char* con_text;
    int con_sel_start;
    int con_sel_length;
} TExec;

#define NOTE_CLS L"TypeBoxNote"

static HWND   g_mainWnd = NULL;     /* host console window                     */
static HWND   g_noteWnd = NULL, g_noteEdit = NULL;
static WNDPROC g_oldEditProc = NULL;
static wchar_t g_folder[MAX_PATH] = L"";
static BOOL   g_prefill = TRUE;     /* [Notes] Prefill: seed editor from clipboard */

/* ------------------------------- clipboard ----------------------------------- */

/* v0.24 lesson kept: OpenClipboard against a thread-owned msg-only window,
 * never NULL - a wedged chain then fails fast instead of blocking the host. */
static HWND ClipWnd(void) {
    static HWND h;
    if (!h) {
        WNDCLASSW wc;
        HMODULE self = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&ClipWnd, &self);
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = DefWindowProcW;
        wc.lpszClassName = L"TypeBoxNoteClip";
        wc.hInstance     = self;
        if (RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
            h = CreateWindowExW(0, wc.lpszClassName, L"note", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, self, NULL);
    }
    return h;
}

static wchar_t* clipGet(void) {                 /* malloc'd or NULL */
    wchar_t* out = NULL;
    if (!OpenClipboard(ClipWnd())) return NULL;
    {
        HANDLE d = GetClipboardData(CF_UNICODETEXT);
        if (d) {
            wchar_t* src = (wchar_t*)GlobalLock(d);
            if (src) {
                size_t n = (lstrlenW(src) + 1) * sizeof(wchar_t);
                if (n <= 256 * 1024) {
                    out = (wchar_t*)malloc(n);
                    if (out) lstrcpyW(out, src);
                }
                GlobalUnlock(src);
            }
        }
    }
    CloseClipboard();
    return out;
}

static void clipPut(const wchar_t* w) {
    if (!OpenClipboard(ClipWnd())) return;
    EmptyClipboard();
    {
        size_t n = (lstrlenW(w) + 1) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
        if (h) {
            wchar_t* p = (wchar_t*)GlobalLock(h);
            if (p) {
                lstrcpyW(p, w);
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);
            } else GlobalFree(h);
        }
    }
    CloseClipboard();
}

/* --------------------------------- config ------------------------------------ */

static void loadConfig(void) {
    wchar_t ini[MAX_PATH] = {};
    /* The HOST exe is the anchor: [Notes] Folder lives with TypeBox.ini and the
     * default Notes folder sits next to TypeBox.exe - not next to the dll. */
    GetModuleFileNameW(NULL, ini, MAX_PATH);
    { wchar_t* bs = wcsrchr(ini, L'\\'); if (bs) bs[1] = 0; }
    {
        wchar_t iniPath[MAX_PATH];
        wsprintfW(iniPath, L"%sTypeBox.ini", ini);
        GetPrivateProfileStringW(L"Notes", L"Folder", L"", g_folder, MAX_PATH, iniPath);
        if (!g_folder[0]) wsprintfW(g_folder, L"%sNotes", ini);   /* portable default */
        g_prefill = (BOOL)GetPrivateProfileIntW(L"Notes", L"Prefill", 1, iniPath);
    }
    SHCreateDirectoryExW(NULL, g_folder, NULL);          /* Joplin: create on demand */
}

/* ------------------------------- file helpers -------------------------------- */

static int toWide(const char* a, wchar_t* w, int cap) {
    return MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap);
}

/* ASCII slug for FILENAMEs only (kebab-case); empty result -> "note". */
static void fileSlug(const wchar_t* src, wchar_t* dst, size_t cap) {
    size_t n = 0;
    BOOL lastDash = TRUE;                       /* trim a leading '-' */
    for (; *src && n + 1 < cap; ++src) {
        wchar_t c = *src;
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
        if ((c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9')) {
            dst[n++] = c; lastDash = FALSE;
        } else if (!lastDash && n + 1 < cap) {
            dst[n++] = L'-'; lastDash = TRUE;
        }
    }
    while (n && dst[n - 1] == L'-') --n;         /* no trailing dash */
    dst[n] = 0;
    if (!n) lstrcpyW(dst, L"note");
}

static void stamp(wchar_t* date, size_t dcap, wchar_t* hm, size_t hcap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    if (date) wsprintfW(date, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    if (hm)   wsprintfW(hm,   L"%02d:%02d", st.wHour, st.wMinute);
}

static void wideToAcp(const wchar_t* w, char* a, int cap) {
    if (WideCharToMultiByte(CP_ACP, 0, w, -1, a, cap, NULL, NULL) <= 0) a[0] = 0;
}

static void wideToUtf8(const wchar_t* w, char* a, int cap) {
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, a, cap, NULL, NULL) <= 0) a[0] = 0;
}

/* Write UTF-8 file; append=1 adds at the end. Returns 1 on success. */
static int writeUtf8(const wchar_t* path, const char* utf8, int append) {
    HANDLE f = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                           append ? OPEN_ALWAYS : CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD done = 0;
    if (f == INVALID_HANDLE_VALUE) return 0;
    WriteFile(f, utf8, (DWORD)lstrlenA(utf8), &done, NULL);
    CloseHandle(f);
    return 1;
}

/* Console echo for the async editor save: write straight into the host edit
 * (the plugin already returned its "editor opened" answer - same trick the
 * ring picker uses for its copy confirmation). */
static void reportWide(const wchar_t* msg) {
    if (g_mainWnd) {
        HWND e = FindWindowExW(g_mainWnd, NULL, L"EDIT", NULL);
        if (e) SetWindowTextW(e, msg);
    }
}

/* Build + save a standalone note file: 2026-11-17-1423-privet-mir.md */
static int saveNoteFile(const wchar_t* body, wchar_t* pathOut, size_t cap) {
    wchar_t date[16], hm[8], title[200], slug[64], name[192], path[MAX_PATH];
    const wchar_t* p = body;
    char* u8;
    wchar_t* md;
    size_t need;
    /* title = first non-empty line */
    while (*p == L' ' || *p == L'\r' || *p == L'\n') ++p;
    {
        const wchar_t* e = p;
        size_t n = 0;
        while (*e && *e != L'\r' && *e != L'\n' && n + 1 < sizeof(title) / sizeof(wchar_t))
            title[n++] = *e++;
        title[n] = 0;
    }
    if (title[0] == L'#') title[0] = 0;          /* body carries its own heading */
    fileSlug(title, slug, 64);
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wsprintfW(date, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
        wsprintfW(hm,   L"%02d:%02d", st.wHour, st.wMinute);
        /* filename must not contain ':' - NTFS would read it as an ADS stream */
        wsprintfW(name, L"%s-%02d%02d-%s.md", date, st.wHour, st.wMinute, slug);
    }
    wsprintfW(path, L"%s\\%s", g_folder, name);

    need = (lstrlenW(title) + lstrlenW(body) + 80) * sizeof(wchar_t);
    md = (wchar_t*)malloc(need);
    if (!md) return 0;
    if (title[0]) wsprintfW(md, L"# %s\n\n_Captured %s %s._\n\n%s\n", title, date, hm, body);
    else          wsprintfW(md, L"_Captured %s %s._\n\n%s\n", date, hm, body);
    u8 = (char*)malloc(need + 1);
    if (!u8) { free(md); return 0; }
    wideToUtf8(md, u8, (int)(need + 1));
    free(md);
    if (!writeUtf8(path, u8, 0)) { free(u8); return 0; }
    free(u8);
    lstrcpynW(pathOut, path, (int)cap);
    return 1;
}

/* Append "- [HH:mm] text" to <folder>\<YYYY-MM-DD>.md (QuickAdd bullet). */
static int appendJournal(const wchar_t* text, wchar_t* pathOut, size_t cap) {
    wchar_t date[16], hm[8], path[MAX_PATH];
    wchar_t* line;
    char* u8;
    size_t need;
    stamp(date, 16, hm, 8);
    wsprintfW(path, L"%s\\%s.md", g_folder, date);
    {   /* create with the daily heading when missing */
        DWORD a = GetFileAttributesW(path);
        if (a == INVALID_FILE_ATTRIBUTES) {
            wchar_t head[128];
            char h8[512];
            wsprintfW(head, L"# %s\n\n", date);
            wideToUtf8(head, h8, sizeof(h8));
            writeUtf8(path, h8, 0);
        }
    }
    need = (lstrlenW(text) + 48) * sizeof(wchar_t);
    line = (wchar_t*)malloc(need);
    if (!line) return 0;
    wsprintfW(line, L"- [%s] %s\n", hm, text);
    u8 = (char*)malloc(need + 1);
    if (!u8) { free(line); return 0; }
    wideToUtf8(line, u8, (int)(need + 1));
    free(line);
    if (!writeUtf8(path, u8, 1)) { free(u8); return 0; }
    free(u8);
    lstrcpynW(pathOut, path, (int)cap);
    return 1;
}

/* ------------------------------- editor window -------------------------------- */

static void closeEditor(void) {
    if (g_noteWnd) { DestroyWindow(g_noteWnd); g_noteWnd = NULL; g_noteEdit = NULL; }
}

static void saveEditor(void) {
    wchar_t* text = (wchar_t*)malloc(256 * 1024);
    wchar_t path[MAX_PATH];
    if (!text) { closeEditor(); return; }
    GetWindowTextW(g_noteEdit, text, 256 * 1024);
    {   /* trim: nothing typed -> nothing saved */
        wchar_t* s = text;
        size_t n;
        while (*s == L' ' || *s == L'\r' || *s == L'\n') ++s;
        n = lstrlenW(s);
        while (n && (s[n - 1] == L' ' || s[n - 1] == L'\r' || s[n - 1] == L'\n')) --n;
        if (!n) { free(text); closeEditor(); return; }
        if (s != text) lstrcpyW(text, s);
        text[n] = 0;
    }
    if (saveNoteFile(text, path, MAX_PATH)) {
        wchar_t a[MAX_PATH + 40];
        wsprintfW(a, L"note: saved %s", path);
        clipPut(path);                                   /* path on the clipboard */
        reportWide(a);
    } else {
        reportWide(L"note: save FAILED - check [Notes] Folder");
    }
    free(text);
    closeEditor();
}

static LRESULT CALLBACK NoteEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
        saveEditor();                                    /* Ctrl+Enter = save   */
        return 0;
    }
    if (m == WM_KEYDOWN && w == VK_ESCAPE) {             /* Esc = cancel        */
        closeEditor();
        return 0;
    }
    if (m == WM_CHAR && w == 0x0D && (GetKeyState(VK_CONTROL) & 0x8000)) {
        saveEditor();
        return 0;
    }
    if (m == WM_CHAR && w == 0x1B) {                     /* Esc = cancel         */
        closeEditor();
        return 0;
    }
    return CallWindowProcW(g_oldEditProc, h, m, w, l);
}

static LRESULT CALLBACK NoteProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(w) == 1) { saveEditor(); return 0; }          /* Save button  */
        if (LOWORD(w) == 2) { closeEditor(); return 0; }         /* Cancel       */
        break;
    case WM_CLOSE:
        closeEditor();
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void openEditor(void) {
    if (g_noteWnd) {                                     /* already open: raise */
        ShowWindow(g_noteWnd, SW_SHOW);
        SetForegroundWindow(g_noteWnd);
        return;
    }
    {
        HMODULE self = NULL;
        WNDCLASSW wc;
        RECT rc;
        int x, y, W = 520, H = 360;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&openEditor, &self);
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = NoteProc;
        wc.lpszClassName = NOTE_CLS;
        wc.hInstance     = self;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        RegisterClassW(&wc);

        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0);
        x = (rc.right - W) / 2; y = (rc.bottom - H) / 3;

        g_noteWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_APPWINDOW, NOTE_CLS,
                                    L"TypeBox note - Ctrl+Enter saves, Esc cancels",
                                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                    WS_THICKFRAME | WS_MINIMIZEBOX,
                                    x, y, W, H, NULL, NULL, self, NULL);
        if (!g_noteWnd) return;
        g_noteEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                     ES_MULTILINE | ES_WANTRETURN |
                                     ES_AUTOHSCROLL | WS_TABSTOP,
                                     8, 8, W - 24, H - 92, g_noteWnd,
                                     (HMENU)(INT_PTR)3, self, NULL);
        if (!g_noteEdit) { DestroyWindow(g_noteWnd); g_noteWnd = NULL; return; }
        g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_noteEdit, GWLP_WNDPROC,
                                                   (LONG_PTR)NoteEditProc);
        {
            HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND b1, b2;
            SendMessageW(g_noteEdit, WM_SETFONT, (WPARAM)f, TRUE);
            b1 = CreateWindowExW(0, L"BUTTON", L"Save (Ctrl+Enter)",
                                 WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                 W - 200, H - 100, 180, 26, g_noteWnd,
                                 (HMENU)1, self, NULL);
            b2 = CreateWindowExW(0, L"BUTTON", L"Cancel (Esc)",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 W - 200, H - 66, 180, 26, g_noteWnd,
                                 (HMENU)2, self, NULL);
            if (b1) SendMessageW(b1, WM_SETFONT, (WPARAM)f, TRUE);
            if (b2) SendMessageW(b2, WM_SETFONT, (WPARAM)f, TRUE);
        }
        if (g_prefill) {                                 /* capture-first: clipboard seed */
            wchar_t* c = clipGet();
            if (c && c[0]) SetWindowTextW(g_noteEdit, c);
            if (c) free(c);
        }
        ShowWindow(g_noteWnd, SW_SHOW);
        SetForegroundWindow(g_noteWnd);
        SetFocus(g_noteEdit);
    }
}

/* ---------------------------------- commands ---------------------------------- */

/* static buffer for the ANSI (CP_ACP, per the plugin ABI) console answer */
static char g_answer[2048];
static const char* answerW(const wchar_t* w) { wideToAcp(w, g_answer, sizeof(g_answer)); return g_answer; }

static const char* runMain(const char* rest) {
    wchar_t arg[8192], path[MAX_PATH];
    const char* p = rest;
    while (*p == ' ') ++p;
    if (!g_folder[0]) loadConfig();
    arg[0] = 0;
    if (!toWide(p, arg, 8192)) return "note: bad argument";
    {
        size_t n = lstrlenW(arg);
        while (n && arg[n - 1] == L' ') arg[--n] = 0;
    }

    if (!arg[0]) { openEditor(); return "note: editor opened - Ctrl+Enter saves, Esc cancels"; }

    if (lstrcmpiW(arg, L"list") == 0) {
        wchar_t date[16], pat[MAX_PATH], line[260], *out, *pre, *cat;
        WIN32_FIND_DATAW fd;
        HANDLE h;
        int n = 0, total = 0;
        stamp(date, 16, NULL, 0);
        out = (wchar_t*)malloc(32 * 1024);
        if (!out) return "note: out of memory";
        out[0] = 0;
        wsprintfW(pat, L"%s\\%s*.md", g_folder, date);
        h = FindFirstFileW(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            free(out);
            wsprintfW(line, L"note: no notes today (%s) in %s", date, g_folder);
            return answerW(line);
        }
        do {
            ++total;
            if (n < 12) { wsprintfW(line, L"%s  ", fd.cFileName); lstrcatW(out, line); ++n; }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        pre = (wchar_t*)malloc(512);
        cat = (wchar_t*)malloc(32 * 1024);
        if (pre && cat) {
            wsprintfW(pre, L"note: %d today: ", total);
            wsprintfW(cat, L"%s%s", pre, out);
            lstrcpyW(out, cat);
        }
        free(pre); free(cat);
        { const char* r = answerW(out); free(out); return r; }
    }

    if (lstrcmpiW(arg, L"open") == 0) {
        wchar_t msg[MAX_PATH + 40];
        if (ShellExecuteW(NULL, L"open", g_folder, NULL, NULL, SW_SHOWNORMAL) > (HINSTANCE)32) {
            wsprintfW(msg, L"note: folder opened: %s", g_folder);
            return answerW(msg);
        }
        return "note: could not open the folder";
    }

    if (arg[0] == L'+' && (arg[1] == L' ' || arg[1] == 0)) {
        const wchar_t* text = arg + 1;
        while (*text == L' ') ++text;
        if (!*text) return "note: note + <text> appends a timestamped bullet to today's journal";
        if (appendJournal(text, path, MAX_PATH)) {
            wchar_t msg[MAX_PATH + 40];
            wsprintfW(msg, L"note: appended to %s", path);
            clipPut(path);
            return answerW(msg);
        }
        return "note: append FAILED - check [Notes] Folder";
    }

    /* one-shot: note <text> */
    if (saveNoteFile(arg, path, MAX_PATH)) {
        wchar_t msg[MAX_PATH + 40];
        wsprintfW(msg, L"note: saved %s", path);
        clipPut(path);
        return answerW(msg);
    }
    return "note: save FAILED - check [Notes] Folder";
}

/* ----------------------------------- ABI --------------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
}
PLUGIN_API void __cdecl Unload(void) {
    if (g_noteWnd) { DestroyWindow(g_noteWnd); g_noteWnd = NULL; g_noteEdit = NULL; }
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Note";
    i.version = "1.0";
    i.description = "quick capture to markdown \u00b7 note / note <text> / note + <text> / note list / note open";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "note" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

static char g_etRows[4][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 4) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int len;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    len = lstrlenA(input);
    if (len <= 4 && _strnicmp(input, "note", len) == 0) {
        etAdd("note + "); etAdd("note list"); etAdd("note open");
    } else if (len >= 5 && _strnicmp(input, "note ", 5) == 0) {
        etAdd("note + "); etAdd("note list"); etAdd("note open");
    }
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    const char* text = NULL;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    while (*str == ' ') ++str;
    if (_strnicmp(str, "note", 4) == 0 && (str[4] == 0 || str[4] == ' '))
        text = runMain(str + 4);
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = text;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(text);
    return ex;
}
