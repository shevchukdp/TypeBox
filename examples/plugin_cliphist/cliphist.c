/*
 * TypeBox plugin: clipboard history — "clip", "clip <n>", "clip pin <n>",
 * "clip unpin <n>", "clip paste <n>", "clip clear".
 * Spec: docs/PLUGIN_GALLERY.md #4 (+ backlog RI #79: pin & paste-at-caret).
 * Zero dependencies beyond user32/shell32/kernel32.
 *
 * Build (MinGW):  gcc -shared -O2 -DCLIPHIST_BUILDING_DLL cliphist.c -o cliphist.dll -luser32 -lshell32
 * Build (MSVC):   cl /LD /DCLIPHIST_BUILDING_DLL /Fe:cliphist.dll cliphist.c user32.lib shell32.lib
 * Licensed under GPL-3.0.
 *
 * Config (TypeBox.ini next to the host exe, DLL lives in Plugins\ so ini = dllpath\..\TypeBox.ini):
 *   [Clipboard]
 *     Max            entries kept, clamped 1..50      (default 20)
 *     PollMs         capture poll interval, ms         (default 500)
 *     RowChars       row preview length                (default 48)
 *     Persist        1 = save/load Plugins\cliphist.dat (default 0)
 *     Balloons       1 = tray balloon after re-copy    (default 0)
 *     Popup          1 = "clip" (no arg) shows a MessageBox (default 1);
 *                    0 = never blocks, listing goes to con_text instead.
 *     PasteOnSelect  1 = "clip <n>" also hides the console and types the
 *                    entry at the caret (Ctrl+V injection)  (default 0)
 *
 * Pinned entries (* in the list) survive trimming and Persist reloads; the
 * newest copies always sit above them, so the top of the list stays yours.
 */
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef CLIPHIST_BUILDING_DLL
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

#define MAX_ENTRIES 50
#define MAX_BYTES   (32 * 1024)          /* skip clipboard text over 32 KB  */
#define TRAY_UID    0x04CBu              /* our own Shell_NotifyIcon uID    */
#define TIMER_ID    1
#define CLS_NAME    L"TypeBoxClipHist"
#define DAT_NAME    L"cliphist.dat"

static HWND     g_mainWnd = NULL;
static HWND     g_msgWnd  = NULL;
static wchar_t* g_entries[MAX_ENTRIES];  /* newest first, pinned block on top */
static BOOL     g_pinned[MAX_ENTRIES];   /* RI #79: survive trim + reload     */
static int      g_count   = 0;
static DWORD    g_lastSeq = 0;
static BOOL     g_trayOn  = FALSE;

static int g_max = 20, g_pollMs = 500, g_rowChars = 48;
static int g_persist = 0, g_balloons = 0, g_popup = 1;
static int g_pasteOnSelect = 0;

static char g_result[8192];

/* ------------------------------- config + paths ------------------------------ */
/* Same trick as units.c: find our own DLL path, step out of Plugins\, read the
 * TypeBox.ini that sits next to the host exe. */
static void moduleDir(wchar_t* dir) {
    wchar_t mod[MAX_PATH] = {};
    HMODULE self = NULL;
    dir[0] = 0;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&moduleDir, &self) &&
        GetModuleFileNameW(self, mod, MAX_PATH)) {
        lstrcpyW(dir, mod);
        wchar_t* slash = wcsrchr(dir, L'\\');
        if (slash) *slash = 0;            /* drop cliphist.dll, keep Plugins\ */
        else       dir[0] = 0;
    }
}

static void loadConfig(void) {
    wchar_t mod[MAX_PATH] = {}, ini[MAX_PATH] = {};
    HMODULE self = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&loadConfig, &self) &&
        GetModuleFileNameW(self, mod, MAX_PATH)) {
        lstrcpyW(ini, mod);
        wchar_t* slash = wcsrchr(ini, L'\\');
        if (slash) { *slash = 0;                      /* drop cliphist.dll  */
            /* If TypeBox.ini sits next to the DLL (probe layout: dll loose in
             * build\), use it. Otherwise shipped layout Plugins\<name>.dll:
             * one level up, next to the host exe. */
            lstrcatW(ini, L"\\TypeBox.ini");
            if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
                *wcsrchr(ini, L'\\') = 0;             /* drop TypeBox.ini   */
                *wcsrchr(ini, L'\\') = 0;             /* drop Plugins\      */
                lstrcatW(ini, L"\\TypeBox.ini");
            }
        } else lstrcpyW(ini, L"TypeBox.ini");
    } else lstrcpyW(ini, L"TypeBox.ini");
    g_max      = (int)GetPrivateProfileIntW(L"Clipboard", L"Max",      20, ini);
    g_pollMs   = (int)GetPrivateProfileIntW(L"Clipboard", L"PollMs",  500, ini);
    g_rowChars = (int)GetPrivateProfileIntW(L"Clipboard", L"RowChars", 48, ini);
    g_persist  = (int)GetPrivateProfileIntW(L"Clipboard", L"Persist",   0, ini);
    g_balloons = (int)GetPrivateProfileIntW(L"Clipboard", L"Balloons",  0, ini);
    g_popup    = (int)GetPrivateProfileIntW(L"Clipboard", L"Popup",     1, ini);
    g_pasteOnSelect = (int)GetPrivateProfileIntW(L"Clipboard", L"PasteOnSelect", 0, ini);
    if (g_max < 1) g_max = 1;
    if (g_max > MAX_ENTRIES) g_max = MAX_ENTRIES;
    if (g_pollMs < 50) g_pollMs = 50;
    if (g_rowChars < 8) g_rowChars = 8;
}

/* --------------------------------- helpers ---------------------------------- */

static void freeAll(void) {
    int i;
    for (i = 0; i < g_count; ++i) { free(g_entries[i]); g_entries[i] = NULL; g_pinned[i] = FALSE; }
    g_count = 0;
}

/* RI #79: keep the pinned block stable on top, newest-first inside it, then
 * the unpinned newest-first. Called after every structural change. */
static void normalizeOrder(void) {
    wchar_t* e[MAX_ENTRIES];
    BOOL     p[MAX_ENTRIES];
    int i, o = 0;
    for (i = 0; i < g_count; ++i) if (g_pinned[i]) { e[o] = g_entries[i]; p[o] = TRUE; ++o; }
    for (i = 0; i < g_count; ++i) if (!g_pinned[i]) { e[o] = g_entries[i]; p[o] = FALSE; ++o; }
    for (i = 0; i < g_count; ++i) { g_entries[i] = e[i]; g_pinned[i] = p[i]; }
}

/* preview of one entry: first line, \r\n\t -> space, truncated at RowChars */
static void preview(const wchar_t* e, wchar_t* out, int outc) {
    int i, o = 0;
    if (!e) { out[0] = 0; return; }
    for (i = 0; e[i] && o < outc - 1 && o < g_rowChars; ++i) {
        wchar_t c = e[i];
        if (c == L'\r' || c == L'\n') break;          /* first line only */
        if (c == L'\t') c = L' ';
        if (c < 32) c = L' ';
        out[o++] = c;
    }
    if (o >= g_rowChars && e[o] && e[o] != L'\r' && e[o] != L'\n') {
        if (o < outc - 2) { out[o++] = L'.'; out[o++] = L'.'; out[o++] = 0; return; }
    }
    out[o] = 0;
}

static int wideToUtf8(const wchar_t* w, char* a, int ac) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, a, ac, NULL, NULL);
    return (n > 0) ? n - 1 : 0;
}

/* insert entry newest-first, skipping dup of newest, deduping older copies,
 * trimming to g_max. Takes ownership of w (frees it when discarding).
 * Pinned entries are never evicted; if everything is pinned the list may sit
 * above Max (hard cap MAX_ENTRIES). */
static void addEntryEx(wchar_t* w, BOOL pin) {
    int i;
    if (!w || !w[0]) { free(w); return; }
    if (lstrlenW(w) * sizeof(wchar_t) > MAX_BYTES) { free(w); return; }
    if (g_count && lstrcmpW(g_entries[0], w) == 0) { free(w); return; }
    for (i = 0; i < g_count; ++i) {
        if (lstrcmpW(g_entries[i], w) == 0) {        /* duplicate: move to front */
            pin = g_pinned[i] || pin;                /* a re-copy keeps the pin  */
            free(g_entries[i]);
            for (; i > 0; --i) { g_entries[i] = g_entries[i - 1]; g_pinned[i] = g_pinned[i - 1]; }
            g_entries[0] = w;
            g_pinned[0]  = pin;
            normalizeOrder();
            return;
        }
    }
    for (i = g_count; i > 0; --i) { g_entries[i] = g_entries[i - 1]; g_pinned[i] = g_pinned[i - 1]; }
    g_entries[0] = w;
    g_pinned[0]  = pin;
    if (g_count < MAX_ENTRIES) ++g_count;
    normalizeOrder();
    while (g_count > g_max) {                        /* evict last UNPINNED only */
        int victim = -1;
        for (i = g_count - 1; i >= 0; --i) if (!g_pinned[i]) { victim = i; break; }
        if (victim < 0) break;                       /* all pinned: keep them    */
        free(g_entries[victim]);
        for (i = victim; i < g_count - 1; ++i) { g_entries[i] = g_entries[i + 1]; g_pinned[i] = g_pinned[i + 1]; }
        --g_count;
    }
}

static void addEntry(wchar_t* w) { addEntryEx(w, FALSE); }

/* -------------------------------- capture ----------------------------------- */

static void captureIfChanged(void) {
    DWORD seq;
    HANDLE h;
    wchar_t* src, * copy;
    size_t bytes;
    seq = GetClipboardSequenceNumber();
    if (seq == g_lastSeq) return;
    if (!OpenClipboard(NULL)) return;                /* locked: retry next tick */
    g_lastSeq = seq;
    h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        src = (wchar_t*)GlobalLock(h);
        if (src) {
            bytes = (lstrlenW(src) + 1) * sizeof(wchar_t);
            if (bytes <= MAX_BYTES + sizeof(wchar_t)) {
                copy = (wchar_t*)malloc(bytes);
                if (copy) { lstrcpyW(copy, src); addEntry(copy); }
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}

/* ------------------------------- persistence -------------------------------- */
/* one entry per line; \n \r \t escaped so each entry stays on one physical line */

static void saveHistory(void) {
    wchar_t dir[MAX_PATH] = {}, path[MAX_PATH] = {};
    FILE* f;
    int i;
    if (!g_persist) return;
    moduleDir(dir);
    if (!dir[0]) return;
    lstrcpyW(path, dir); lstrcatW(path, L"\\"); lstrcatW(path, DAT_NAME);
    {   /* fopen wants a narrow path */
        char apath[MAX_PATH * 2];
        WideCharToMultiByte(CP_ACP, 0, path, -1, apath, sizeof(apath), NULL, NULL);
        f = fopen(apath, "wb");
    }
    if (!f) return;
    for (i = 0; i < g_count; ++i) {
        const wchar_t* e = g_entries[i];
        if (g_pinned[i]) fputs("\\p", f);              /* RI #79: pin marker */
        for (; *e; ++e) {
            switch (*e) {
            case L'\n': fputs("\\n", f); break;
            case L'\r': fputs("\\r", f); break;
            case L'\t': fputs("\\t", f); break;
            case L'\\': fputs("\\\\", f); break;
            default:
                if (*e >= 32) {
                    char u8[4];
                    int n = WideCharToMultiByte(CP_UTF8, 0, e, 1, u8, sizeof(u8), NULL, NULL);
                    if (n > 0) fwrite(u8, 1, (size_t)n, f);
                }
            }
        }
        fputc('\n', f);
    }
    fclose(f);
}

static void loadHistory(void) {
    wchar_t dir[MAX_PATH] = {}, path[MAX_PATH] = {};
    char apath[MAX_PATH * 2];
    FILE* f;
    char line[4096];
    if (!g_persist) return;
    moduleDir(dir);
    if (!dir[0]) return;
    lstrcpyW(path, dir); lstrcatW(path, L"\\"); lstrcatW(path, DAT_NAME);
    WideCharToMultiByte(CP_ACP, 0, path, -1, apath, sizeof(apath), NULL, NULL);
    f = fopen(apath, "rb");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char* p = line, * q;
        char* out;
        wchar_t* w;
        size_t o;
        BOOL pin = FALSE;
        q = p + strlen(p);
        while (q > p && (q[-1] == '\n' || q[-1] == '\r')) *--q = 0;
        if (!*p) continue;
        if (p[0] == '\\' && p[1] == 'p') { pin = TRUE; p += 2; }   /* pin marker */
        if (!*p) continue;
        out = (char*)malloc(strlen(p) * 3 + 1);       /* decoded utf8 bytes */
        if (!out) break;
        o = 0;
        while (*p) {
            if (p[0] == '\\' && p[1]) {
                switch (p[1]) {
                case 'n':  out[o++] = '\n'; p += 2; continue;
                case 'r':  out[o++] = '\r'; p += 2; continue;
                case 't':  out[o++] = '\t'; p += 2; continue;
                case '\\': out[o++] = '\\'; p += 2; continue;
                }
            }
            out[o++] = *p++;
        }
        out[o] = 0;
        {
            int wn = MultiByteToWideChar(CP_UTF8, 0, out, -1, NULL, 0);
            w = (wchar_t*)malloc((size_t)(wn > 0 ? wn : 1) * sizeof(wchar_t));
            if (w) {
                if (wn > 0) MultiByteToWideChar(CP_UTF8, 0, out, -1, w, wn);
                else w[0] = 0;
                addEntryEx(w, pin);                   /* oldest first, newest ends on top */
            }
        }
        free(out);
    }
    fclose(f);
}

/* --------------------------------- re-copy ---------------------------------- */

static void trayBalloon(const wchar_t* text) {
    NOTIFYICONDATAW nid;
    if (!g_trayOn) return;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uID = (UINT)TRAY_UID;
    nid.uFlags = NIF_INFO;
    lstrcpynW(nid.szInfo, text, 255);
    lstrcpyW(nid.szInfoTitle, L"TypeBox");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static int putOnClipboard(const wchar_t* w) {
    int ok = 0;
    if (!OpenClipboard(NULL)) return -1;               /* caller still echoes */
    EmptyClipboard();
    {
        size_t bytes = (lstrlenW(w) + 1) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (h) {
            wchar_t* p = (wchar_t*)GlobalLock(h);
            if (p) { lstrcpyW(p, w); GlobalUnlock(h);
                     if (SetClipboardData(CF_UNICODETEXT, h)) ok = 1;  /* clipboard owns h */
                     else GlobalFree(h); }
            else GlobalFree(h);
        }
    }
    CloseClipboard();
    return ok;
}

/* ------------------------------ message-only wnd ----------------------------- */

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TIMER && w == TIMER_ID) { captureIfChanged(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void trayAdd(void) {
    NOTIFYICONDATAW nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hWnd = g_msgWnd;
    nid.uID = (UINT)TRAY_UID;
    nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    lstrcpyW(nid.szTip, L"TypeBox clipboard history");
    if (Shell_NotifyIconW(NIM_ADD, &nid)) g_trayOn = TRUE;
}

/* --------------------------------- commands --------------------------------- */

static void runList(void) {
    wchar_t list[8192];
    int i;
    list[0] = 0;
    if (g_count == 0) {
        wideToUtf8(L"clip: nothing captured yet", g_result, sizeof(g_result));
        if (g_popup) MessageBoxW(NULL, L"clip: nothing captured yet",
                                 L"TypeBox \x2014 clipboard history", MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (i = 0; i < g_count; ++i) {
        wchar_t prev[256], head[32];
        preview(g_entries[i], prev, (int)(sizeof(prev) / sizeof(prev[0])));
        if (g_pinned[i]) wsprintfW(head, L"%d) *%s", i + 1, prev);   /* RI #79 */
        else             wsprintfW(head, L"%d) %s",  i + 1, prev);
        if (list[0] && lstrlenW(list) < 7800) lstrcatW(list, L"  ");
        lstrcatW(list, head);
    }
    if (g_popup) {
        MessageBoxW(NULL, list, L"TypeBox \x2014 clipboard history", MB_OK | MB_ICONINFORMATION);
        g_result[0] = 0;                                /* popup mode: console stays clean */
    } else {
        wideToUtf8(list, g_result, sizeof(g_result));   /* no popup: list goes to console  */
    }
}

/* RI #79: hide the console so focus returns to the window that held the
 * caret, then inject Ctrl+V there. Only used by "clip paste <n>" and by
 * "clip <n>" when [Clipboard] PasteOnSelect=1. */
static void doPasteCaret(void) {
    INPUT in[4];
    if (!g_mainWnd) return;
    ShowWindow(g_mainWnd, SW_HIDE);
    Sleep(150);                                        /* focus handover */
    ZeroMemory(in, sizeof(in));
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'V';
    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'V'; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

/* returns 1 when the entry was answered (n valid) */
static int runRecopy(int n, int paste) {
    wchar_t msg[320], one[300];
    if (g_count == 0) {
        wideToUtf8(L"clip: nothing captured yet", g_result, sizeof(g_result));
        return 1;
    }
    if (n < 1 || n > g_count) {
        wchar_t only[64];
        only[0] = 0;
        wsprintfW(only, L"clip: only %d entries (1..%d)", g_count, g_count);
        wideToUtf8(only, g_result, sizeof(g_result));
        return 1;
    }
    {
        int ok = putOnClipboard(g_entries[n - 1]);      /* -1 = clipboard locked, still echo */
        /* con_text = the entry itself; the host selects it in the console */
        wideToUtf8(g_entries[n - 1], g_result, sizeof(g_result));
        if (paste && ok > 0) doPasteCaret();
    }
    if (g_balloons) {
        preview(g_entries[n - 1], one, (int)(sizeof(one) / sizeof(one[0])));
        wsprintfW(msg, L"clip: entry %d on the clipboard: %s", n, one);
        trayBalloon(msg);
    }
    return 1;
}

/* RI #79: pin/unpin keep the pinned block on top after normalizing. */
static void runPin(int n, int pin) {
    wchar_t line[96];
    if (g_count == 0) { wideToUtf8(L"clip: nothing captured yet", g_result, sizeof(g_result)); return; }
    if (n < 1 || n > g_count) {
        wsprintfW(line, L"clip: %s needs an entry 1..%d", pin ? L"pin" : L"unpin", g_count);
        wideToUtf8(line, g_result, sizeof(g_result));
        return;
    }
    g_pinned[n - 1] = pin ? TRUE : FALSE;
    normalizeOrder();
    if (g_persist) saveHistory();                       /* pins must survive a reload */
    if (pin) wsprintfW(line, L"clip: entry %d pinned \x2014 kept when history trims", n);
    else     wsprintfW(line, L"clip: entry %d unpinned", n);
    wideToUtf8(line, g_result, sizeof(g_result));
}

/* ----------------------------------- ABI ------------------------------------ */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    WNDCLASSW wc;
    g_mainWnd = mainWnd;
    loadConfig();
    g_lastSeq = 0;                      /* != any real seq: first check captures current clipboard */
    loadHistory();
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.lpszClassName = CLS_NAME;
    wc.hInstance     = GetModuleHandleW(NULL);
    if (RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        g_msgWnd = CreateWindowExW(0, CLS_NAME, L"TypeBoxClipHist", 0,
                                   0, 0, 0, 0, HWND_MESSAGE, NULL,
                                   wc.hInstance, NULL);
        if (g_msgWnd) SetTimer(g_msgWnd, TIMER_ID, (UINT)g_pollMs, NULL);
    }
    if (g_balloons && g_msgWnd) trayAdd();
}

PLUGIN_API void __cdecl Unload(void) {
    if (g_msgWnd) {
        KillTimer(g_msgWnd, TIMER_ID);
        if (g_trayOn) {
            NOTIFYICONDATAW nid;
            ZeroMemory(&nid, sizeof(nid));
            nid.cbSize = sizeof(nid);
            nid.hWnd = g_msgWnd;
            nid.uID  = (UINT)TRAY_UID;
            Shell_NotifyIconW(NIM_DELETE, &nid);
            g_trayOn = FALSE;
        }
        DestroyWindow(g_msgWnd);
        g_msgWnd = NULL;
    }
    saveHistory();
    freeAll();
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Clipboard History";
    i.version = "1.1";
    i.description = "clip <n> re-copy \u00b7 clip pin <n> keep \u00b7 clip paste <n> type at caret (last 20 kept in RAM)";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "clip" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

/* ET rows: up to 8 numbered previews + "clip clear"; every row starts with the
 * typed prefix word "clip" so host completion works. */
static char g_etRows[9][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 9) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int i, n;
    wchar_t prev[256], row[256];
    char arow[256];
    g_etCount = 0;
    if (!input) return 0;
    captureIfChanged();                    /* lazy: ET may run before any RunString */
    if (lstrlenA(input) <= 4) {                        /* typed prefix of "clip" ("" counts) */
        if (_strnicmp(input, "clip", lstrlenA(input)) != 0) return 0;
    } else {                                           /* "clip c..." -> offer subcommands */
        const char* a;
        size_t la;
        if (_strnicmp(input, "clip ", 5) != 0) return 0;
        a = input + 5;
        la = (size_t)lstrlenA(a);
        if (!la || _strnicmp(a, "clear", la) == 0) etAdd("clip clear");
        if (!la || _strnicmp(a, "pin",   la) == 0) etAdd("clip pin 1");      /* RI #79 */
        if (!la || _strnicmp(a, "unpin", la) == 0) etAdd("clip unpin 1");    /* RI #79 */
        if (!la || _strnicmp(a, "paste", la) == 0) etAdd("clip paste 1");    /* RI #79 */
        return g_etCount;
    }
    n = g_count;
    if (n > 8) n = 8;
    for (i = 0; i < n; ++i) {
        preview(g_entries[i], prev, (int)(sizeof(prev) / sizeof(prev[0])));
        wsprintfW(row, L"clip %d: %s", i + 1, prev);
        wideToUtf8(row, arow, sizeof(arow));
        etAdd(arow);
    }
    etAdd("clip clear");
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    const char* p;
    char* end;
    long n;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    if (_strnicmp(str, "clip", 4) != 0) return ex;
    captureIfChanged();                    /* lazy one-shot: robust without the timer */
    p = str + 4;
    while (*p == ' ') ++p;
    g_result[0] = 0;
    if (!*p) {
        runList();
    } else if (!_strnicmp(p, "clear", 5) && (p[5] == 0 || isspace((unsigned char)p[5]))) {
        freeAll();
        wideToUtf8(L"clip: history cleared", g_result, sizeof(g_result));
    } else if (!_strnicmp(p, "pin", 3) && (p[3] == 0 || isspace((unsigned char)p[3]))) {
        n = strtol(p + 3, &end, 10);
        if (end == p + 3 || n < 1) return ex;              /* not ours */
        runPin((int)n, 1);                                 /* RI #79   */
    } else if (!_strnicmp(p, "unpin", 5) && (p[5] == 0 || isspace((unsigned char)p[5]))) {
        n = strtol(p + 5, &end, 10);
        if (end == p + 5 || n < 1) return ex;
        runPin((int)n, 0);                                 /* RI #79 */
    } else if (!_strnicmp(p, "paste", 5) && (p[5] == 0 || isspace((unsigned char)p[5]))) {
        n = strtol(p + 5, &end, 10);
        if (end == p + 5 || n < 1) return ex;
        runRecopy((int)n, 1);                              /* RI #79: Ctrl+V at caret */
    } else {
        n = strtol(p, &end, 10);
        if (end == p || n < 1) return ex;              /* not ours -> let others try */
        /* rows arrive as "clip <n>: preview ..." - everything after the first
         * space following the number is ignored: strtol stops at ':' and the
         * rest is never looked at by runRecopy */
        runRecopy((int)n, g_pasteOnSelect);
    }
    if (!g_result[0]) return ex;                       /* popup mode: nothing to show */
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
