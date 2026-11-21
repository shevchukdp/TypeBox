/*
 * TypeBox plugin: clipboard history — "clip", "clip <n>", "clip pin <n>",
 * "clip unpin <n>", "clip paste <n>", "clip type <n>", "clip ring",
 * "clip clear".
 * Spec: docs/PLUGIN_GALLERY.md #4 (+ backlog RI #79: pin & paste-at-caret,
 * RI3 #260 ring window + #261 forced typing).
 * Zero dependencies beyond user32/gdi32/shell32/kernel32.
 *
 * Build (MinGW):  gcc -shared -O2 -DCLIPHIST_BUILDING_DLL cliphist.c -o cliphist.dll -luser32 -lgdi32 -lshell32
 * Build (MSVC):   cl /LD /DCLIPHIST_BUILDING_DLL /Fe:cliphist.dll cliphist.c user32.lib gdi32.lib shell32.lib
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
 *     TypeDelayMs    pacing between injected characters (RI3 #261, clamped
 *                    0..50, default 5)
 *     Ring           global hotkey opening the ring picker (RI3 #260),
 *                    "Ctrl+Alt+V", "0" disables (default Ctrl+Alt+V)
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
#define RING_CLS    L"TypeBoxClipRing"    /* RI3 #260 picker window class    */
#define RING_HOTID  991                   /* our RegisterHotKey id            */

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
static int g_typeDelay = 5;                       /* RI3 #261 TypeDelayMs     */
static UINT  g_ringMods = MOD_CONTROL | MOD_ALT;  /* RI3 #260 ring hotkey     */
static UINT  g_ringVk   = 'V';
static BOOL  g_ringHotOn = FALSE;                 /* RegisterHotKey succeeded */
static HWND  g_ringWnd = NULL, g_ringList = NULL;
static HBRUSH g_ringClsBrush = NULL;            /* class bg brush, one per Load cycle */
static DWORD g_ringShown = 0;                   /* tick: WA_INACTIVE grace     */
static WNDPROC g_listOld = NULL;                  /* listbox subclass backup  */
static HFONT   g_ringFont = NULL;
/* v0.29 DPI wave: the ring keeps its own dpi (WM_DPICHANGED wParam drives it,
 * which is also how the suites exercise the math on a 96-DPI machine).
 * All ring geometry below is 96-logical and scaled through Scp(). */
static int     g_ringDpi = 96;
typedef UINT (WINAPI *RingGDFW)(HWND);
static int DpiOf(HWND h) {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    RingGDFW f = u ? (RingGDFW)(void*)GetProcAddress(u, "GetDpiForWindow") : NULL;
    int d = f && h ? (int)f(h) : 96;
    return (d >= 96 && d <= 480) ? d : 96;
}
static int Scp(int dpi, int v) { return MulDiv(v, dpi >= 96 ? dpi : 96, 96); }
static HBRUSH  g_ringBgBrush = NULL;              /* cozy-dark list background*/

static void openRing(void);                       /* defined with the ring    */
static void closeRing(void);
static HMODULE SelfModule(void);

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

static void loadConfig(void);

/* RI3 #260: parse "Ctrl+Alt+V" style hotkeys for the ring picker:
 * modifier tokens + one final A-Z/0-9/F1-F12 key. "0" = disabled.
 * Hand-rolled tokenizer: MinGW's wcstok is the two-argument C89 flavour. */
static BOOL parseHotkey(const wchar_t* s, UINT* mods, UINT* vk) {
    wchar_t buf[64], *tok, *next;
    BOOL haveKey = FALSE;
    *mods = 0; *vk = 0;
    lstrcpynW(buf, s, 64);
    next = buf;
    for (;;) {
        while (*next == L' ' || *next == L'+' || *next == L'\t') ++next;
        if (!*next) break;
        tok = next;
        while (*next && *next != L' ' && *next != L'+' && *next != L'\t') ++next;
        if (*next) { *next = 0; ++next; }
        if      (!_wcsicmp(tok, L"CTRL"))  *mods |= MOD_CONTROL;
        else if (!_wcsicmp(tok, L"ALT"))   *mods |= MOD_ALT;
        else if (!_wcsicmp(tok, L"SHIFT")) *mods |= MOD_SHIFT;
        else if (!_wcsicmp(tok, L"WIN") || !_wcsicmp(tok, L"WINDOWS")) *mods |= MOD_WIN;
        else if (!_wcsicmp(tok, L"0"))     return FALSE;
        else if ((tok[0] == L'f' || tok[0] == L'F') && tok[1]) {
            long f = 0;
            const wchar_t* d;
            for (d = tok + 1; *d; ++d) {
                if (*d < L'0' || *d > L'9') { f = 99; break; }
                f = f * 10 + (*d - L'0');
            }
            if (f < 1 || f > 12) return FALSE;
            *vk = (UINT)(VK_F1 + f - 1); haveKey = TRUE;
        } else {
            wchar_t c = tok[0];
            size_t n = lstrlenW(tok);
            if (n != 1) return FALSE;
            if (c >= L'a' && c <= L'z') c = (wchar_t)(c - 32);
            if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
                *vk = (UINT)c; haveKey = TRUE;
            } else return FALSE;
        }
    }
    return haveKey && (*mods & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN)) != 0;
}

static int g_headless = 0;  /* v0.27: TYPEBOX_HEADLESS=1 / [UI] Headless=1 - the
                             * suite must never touch the human's keyboard, focus
                             * or screen. Set in loadConfig. */

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
    g_typeDelay = (int)GetPrivateProfileIntW(L"Clipboard", L"TypeDelayMs", 5, ini);   /* RI3 #261 */
    { wchar_t ev[8]; ev[0] = 0;
      GetEnvironmentVariableW(L"TYPEBOX_HEADLESS", ev, 7);
      g_headless = (ev[0] == L'1') ||
                   (int)GetPrivateProfileIntW(L"UI", L"Headless", 0, ini); }
    if (g_typeDelay < 0)  g_typeDelay = 0;
    if (g_typeDelay > 50) g_typeDelay = 50;
    {   /* RI3 #260: [Clipboard] Ring=Ctrl+Alt+V ("0" disables the global hotkey) */
        wchar_t hk[64] = {};
        GetPrivateProfileStringW(L"Clipboard", L"Ring", L"Ctrl+Alt+V", hk, 64, ini);
        if (!parseHotkey(hk, &g_ringMods, &g_ringVk)) { g_ringVk = 0; }
    }
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
    /* v0.24: open against our msg-only window, not NULL - a wedged clipboard
     * chain makes OpenClipboard(NULL) BLOCK the host thread; a real thread
     * window fails fast instead (retry next tick). */
    if (!OpenClipboard(g_msgWnd ? g_msgWnd : NULL)) return;   /* locked: retry next tick */
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
    if (g_headless) return;   /* headless: no tray noise for the human */
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
    if (!OpenClipboard(g_msgWnd ? g_msgWnd : NULL)) return -1;  /* caller still echoes */
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
    if (m == WM_HOTKEY && w == RING_HOTID) { openRing(); return 0; }   /* RI3 #260 */
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
    if (g_headless) {                    /* only inject while the TEST pad owns
                                          * the foreground - never the human's app */
        HWND fg = GetForegroundWindow();
        wchar_t cls[32] = {};
        if (fg) GetClassNameW(fg, cls, 31);
        if (!fg || lstrcmpiW(cls, L"TypeBoxTestPad") != 0) return;
    }
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

/* RI3 #261: per-character SendInput (KEYEVENTF_UNICODE) at the caret, for the
 * terminals and apps where Ctrl+V is intercepted or mapped to something else.
 * \r\n\t and other control chars are dropped - the bracketed-paste guard:
 * a stray newline in a terminal would RUN things. */
static void doTypeString(const wchar_t* w) {
    INPUT in[2];
    const wchar_t* p;
    if (!g_mainWnd) return;
    if (g_headless) {                    /* only type into the TEST pad, never
                                          * into whatever the human is using */
        HWND fg = GetForegroundWindow();
        wchar_t cls[32] = {};
        if (fg) GetClassNameW(fg, cls, 31);
        if (!fg || lstrcmpiW(cls, L"TypeBoxTestPad") != 0) return;
    }
    ShowWindow(g_mainWnd, SW_HIDE);
    Sleep(150);                                        /* focus handover */
    for (p = w; *p && (DWORD)(p - w) < 8192; ++p) {
        wchar_t c = *p;
        if (c < 0x20 || c == 0x7F) continue;
        ZeroMemory(in, sizeof(in));
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wScan = c;
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;
        in[1].type = INPUT_KEYBOARD;
        in[1].ki.wScan = c;
        in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        if (g_typeDelay) Sleep((DWORD)g_typeDelay);
    }
}

static void runType(int n) {
    wchar_t line[128];
    if (g_count == 0) { wideToUtf8(L"clip: nothing captured yet", g_result, sizeof(g_result)); return; }
    if (n < 1 || n > g_count) {
        wsprintfW(line, L"clip: type needs an entry 1..%d", g_count);
        wideToUtf8(line, g_result, sizeof(g_result));
        return;
    }
    doTypeString(g_entries[n - 1]);
    wsprintfW(line, L"clip: entry %d typed (%d chars)", n, (int)lstrlenW(g_entries[n - 1]));
    wideToUtf8(line, g_result, sizeof(g_result));
}

/* --------------------------- ring picker window (RI3 #260) ------------------ */
/* A compact always-on-top picker pre-loaded with the ring: arrows move, Enter
 * copies, Ctrl+Enter types (#261), Esc dismisses, focus-loss dismisses. Paint
 * matches the host drop-down (cozy-dark + the blue selection bar). */

static void closeRing(void) {
    if (g_ringList && g_listOld) {
        SetWindowLongPtrW(g_ringList, GWLP_WNDPROC, (LONG_PTR)g_listOld);
        g_listOld = NULL;
    }
    if (g_ringWnd) { DestroyWindow(g_ringWnd); g_ringWnd = NULL; }
    g_ringList = NULL;
}

static void ringPick(int ctrl) {
    int sel;
    if (!g_ringList) return;
    sel = (int)SendMessageW(g_ringList, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_count) { closeRing(); if (!g_headless) SetForegroundWindow(g_mainWnd); return; }
    if (ctrl) doTypeString(g_entries[sel]);
    else {
        putOnClipboard(g_entries[sel]);
        if (!g_headless) SetForegroundWindow(g_mainWnd);
    }
    closeRing();
}

static LRESULT CALLBACK RingListProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN) {
        if (w == VK_ESCAPE) { closeRing(); if (!g_headless) SetForegroundWindow(g_mainWnd); return 0; }
        if (w == VK_RETURN) { ringPick((GET_KEYSTATE_LPARAM(l) & MK_CONTROL) != 0); return 0; }
    }
    return CallWindowProcW(g_listOld, h, m, w, l);
}

static LRESULT CALLBACK RingProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_DPICHANGED: {                                 /* v0.29 PMv2 wave */
        int d = (int)LOWORD(w);
        RECT* pr = (RECT*)l;
        g_ringDpi = (d >= 96 && d <= 480) ? d : 96;
        {   /* 96-logical 12px list font rebuilt at the new scale */
            HFONT nf = CreateFontW(-Scp(g_ringDpi, 12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            if (nf) {
                if (g_ringFont) DeleteObject(g_ringFont);
                g_ringFont = nf;
                if (g_ringList) SendMessageW(g_ringList, WM_SETFONT, (WPARAM)nf, TRUE);
            }
        }
        if (pr) SetWindowPos(h, HWND_TOPMOST, pr->left, pr->top,
                             pr->right - pr->left, pr->bottom - pr->top, SWP_NOACTIVATE);
        InvalidateRect(h, NULL, TRUE);
        return 0;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)l;
        mi->itemHeight = Scp(g_ringDpi, 24);
        return TRUE;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)l;
        RECT r = di->rcItem;
        wchar_t txt[300];
        HBRUSH bg;
        if (di->itemID < 0) break;
        if (di->itemState & ODS_SELECTED) {
            bg = CreateSolidBrush(RGB(0, 120, 215));
            SetTextColor(di->hDC, RGB(255, 255, 255));
        } else {
            bg = CreateSolidBrush(RGB(34, 29, 26));
            SetTextColor(di->hDC, RGB(240, 230, 210));
        }
        FillRect(di->hDC, &r, bg);
        DeleteObject(bg);
        txt[0] = 0;
        SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, (LPARAM)txt);
        r.left += Scp(g_ringDpi, 8);
        DrawTextW(di->hDC, txt, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_LEFT);
        return TRUE;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(240, 230, 210));
        SetBkColor(dc, RGB(34, 29, 26));
        if (!g_ringBgBrush) g_ringBgBrush = CreateSolidBrush(RGB(34, 29, 26));
        return (LRESULT)g_ringBgBrush;
    }
    case WM_CLOSE:
        closeRing();
        if (!g_headless) SetForegroundWindow(g_mainWnd);
        return 0;
    case WM_ACTIVATE:
        /* Grace: when "clip ring" runs from the console, the host re-activates
         * its own window to show the answer - that must not kill the picker.
         * After the grace a real focus-loss closes it (picker feel). */
        if (LOWORD(w) == WA_INACTIVE) {
            if (GetTickCount() - g_ringShown > 350) closeRing();
            return 0;
        }
        break;
    case WM_DESTROY:
        if (g_ringFont) { DeleteObject(g_ringFont); g_ringFont = NULL; }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void openRing(void) {
    RECT wa;
    int i, rows, wdt = 560, hgt;
    if (!g_msgWnd) return;
    closeRing();
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    g_ringDpi = DpiOf(g_mainWnd);              /* v0.29: start on the console's dpi */
    wdt = Scp(g_ringDpi, 560);
    rows = g_count ? g_count : 1;
    if (rows > 12) rows = 12;
    hgt = Scp(g_ringDpi, rows * 24 + 8);
    if (g_headless) {                       /* picker lives off-screen, silently */
        wa.left = -32000; wa.top = -32000;
        wa.right = -32000 + wdt; wa.bottom = -32000 + hgt;
    }
    g_ringWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, RING_CLS,
                                L"TypeBox \x2014 clip ring",
                                WS_POPUP | WS_VISIBLE,
                                wa.left + ((wa.right - wa.left) - wdt) / 2,
                                wa.top  + ((wa.bottom - wa.top) - hgt) / 3,
                                wdt, hgt, NULL, NULL,
                                SelfModule(), NULL);
    if (!g_ringWnd) return;
    g_ringList = CreateWindowExW(0, L"LISTBOX", NULL,
                                 WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                                 WS_TABSTOP | LBS_NOTIFY | LBS_OWNERDRAWFIXED |
                                 LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                                 2, 2, wdt - 4, hgt - 4, g_ringWnd, NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (!g_ringList) { DestroyWindow(g_ringWnd); g_ringWnd = NULL; return; }
    g_ringFont = CreateFontW(-Scp(g_ringDpi, 12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (g_ringFont) SendMessageW(g_ringList, WM_SETFONT, (WPARAM)g_ringFont, TRUE);
    for (i = 0; i < g_count; ++i) {
        wchar_t prev[256], row[300];
        preview(g_entries[i], prev, (int)(sizeof(prev) / sizeof(prev[0])));
        if (g_pinned[i]) wsprintfW(row, L"%d) *%s", i + 1, prev);
        else             wsprintfW(row, L"%d) %s",  i + 1, prev);
        SendMessageW(g_ringList, LB_ADDSTRING, 0, (LPARAM)row);
    }
    if (!g_count) SendMessageW(g_ringList, LB_ADDSTRING, 0, (LPARAM)L"(empty)");
    else          SendMessageW(g_ringList, LB_SETCURSEL, 0, 0);
    g_listOld = (WNDPROC)SetWindowLongPtrW(g_ringList, GWLP_WNDPROC, (LONG_PTR)RingListProc);
    if (!g_headless) {
        SetForegroundWindow(g_ringWnd);
        SetFocus(g_ringList);
    }
    g_ringShown = GetTickCount();
}

/* ----------------------------------- ABI ------------------------------------ */

/* v0.36 fix: classes must live under OUR module, not the exe - FreeLibrary
 * then drops them automatically, and a /load reload can never find a stale
 * class whose WndProc points into an unmapped image (0xc0000005). */
static HMODULE SelfModule(void) {
    HMODULE self = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&SelfModule, &self);
    return self;
}

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    WNDCLASSW wc;
    HMODULE self = SelfModule();
    g_mainWnd = mainWnd;
    loadConfig();
    g_lastSeq = 0;                      /* != any real seq: first check captures current clipboard */
    loadHistory();
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.lpszClassName = CLS_NAME;
    wc.hInstance     = self;
    if (RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        g_msgWnd = CreateWindowExW(0, CLS_NAME, L"TypeBoxClipHist", 0,
                                   0, 0, 0, 0, HWND_MESSAGE, NULL,
                                   wc.hInstance, NULL);
        if (g_msgWnd) SetTimer(g_msgWnd, TIMER_ID, (UINT)g_pollMs, NULL);
    }
    if (g_balloons && g_msgWnd) trayAdd();
    {   /* RI3 #260: ring window class + global hotkey [Clipboard] Ring */
        WNDCLASSW rw;
        ZeroMemory(&rw, sizeof(rw));
        rw.lpfnWndProc   = RingProc;
        rw.lpszClassName = RING_CLS;
        rw.hInstance     = self;
        rw.hbrBackground = g_ringClsBrush ? g_ringClsBrush
                           : (g_ringClsBrush = CreateSolidBrush(RGB(34, 29, 26)));
        rw.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        if (RegisterClassW(&rw) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            if (g_ringVk && g_msgWnd)
                g_ringHotOn = RegisterHotKey(g_msgWnd, RING_HOTID,
                                             g_ringMods | MOD_NOREPEAT, g_ringVk) != 0;
        }
    }
}

PLUGIN_API void __cdecl Unload(void) {
    closeRing();                                  /* RI3 #260: never orphan it */
    if (g_msgWnd) {
        KillTimer(g_msgWnd, TIMER_ID);
        if (g_ringHotOn) { UnregisterHotKey(g_msgWnd, RING_HOTID); g_ringHotOn = FALSE; }
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
    UnregisterClassW(CLS_NAME, SelfModule());   /* reload gets fresh classes */
    UnregisterClassW(RING_CLS, SelfModule());
    if (g_ringClsBrush) { DeleteObject(g_ringClsBrush); g_ringClsBrush = NULL; }
    saveHistory();
    freeAll();
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Clipboard History";
    i.version = "1.2";
    i.description = "clip <n> re-copy \u00b7 clip type <n> forced typing \u00b7 clip ring picker (Ctrl+Alt+V) \u00b7 clip pin <n> keep (last 20 kept in RAM)";
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
        if (!la || _strnicmp(a, "type",  la) == 0) etAdd("clip type 1");     /* RI3 #261 */
        if (!la || _strnicmp(a, "ring",  la) == 0) etAdd("clip ring");       /* RI3 #260 */
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
    } else if (!_strnicmp(p, "type", 4) && (p[4] == 0 || isspace((unsigned char)p[4]))) {
        n = strtol(p + 4, &end, 10);                       /* RI3 #261: forced typing  */
        if (end == p + 4 || n < 1) return ex;
        runType((int)n);
    } else if (!_strnicmp(p, "ring", 4) && (p[4] == 0 || isspace((unsigned char)p[4]))) {
        openRing();                                        /* RI3 #260: ring picker    */
        if (g_ringHotOn)
            wideToUtf8(L"clip: ring opened - arrows, Enter copies, Ctrl+Enter types, Esc closes",
                       g_result, sizeof(g_result));
        else
            wideToUtf8(L"clip: ring opened (global hotkey not registered - check [Clipboard] Ring)",
                       g_result, sizeof(g_result));
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
