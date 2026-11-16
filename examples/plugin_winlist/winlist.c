/*
 * TypeBox plugin: window switcher — "win <substr>", "win <n>", "win".
 * Spec: docs/PLUGIN_GALLERY.md #6, console-first: no list window, NEVER a
 * blocking MessageBox, every answer lands in TExec.con_text with run=1.
 * Zero dependencies beyond user32.
 *
 * Build (MinGW):  gcc -shared -O2 -DWINLIST_BUILDING_DLL winlist.c -o winlist.dll -luser32
 * Build (MSVC):   cl /LD /DWINLIST_BUILDING_DLL /Fe:winlist.dll winlist.c user32.lib
 * Licensed under GPL-3.0.
 *
 * Config (TypeBox.ini next to the host exe, DLL lives in Plugins\ so ini = dllpath\..\TypeBox.ini):
 *   [Winlist]
 *     AltTrick  1 = on SetForegroundWindow failure tap VK_MENU via SendInput
 *               and retry once (beats the foreground lock)        (default 1)
 *     Titles    max remembered matches for "win <n>" (1..20)      (default 9)
 *
 * "win <substr>" re-enumerates inside RunString (sub-millisecond) and stores
 * the matches newest-z-order-first in g_last[] — EnumWindows walks top-down,
 * so the first hit is the topmost window and is the one activated. ET serves
 * from that cache only, never enumerates per keystroke.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>

#ifdef WINLIST_BUILDING_DLL
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

#define MAX_TITLES 20                      /* hard cap for [Winlist] Titles   */
#define TITLE_LEN  128                     /* spec: wchar_t title[128]        */

typedef struct { HWND hwnd; wchar_t title[TITLE_LEN]; } WIN_ENT;

static HWND     g_mainWnd = NULL;
static WIN_ENT  g_last[MAX_TITLES];
static int      g_lastCount = 0;
static int      g_altTrick = 1, g_titles = 9;

static char g_result[2048];

/* ------------------------------- config ------------------------------------- */
/* units.c style: locate our own DLL, step out of Plugins\, read TypeBox.ini
 * that sits next to the host exe. */
static void loadConfig(void) {
    wchar_t mod[MAX_PATH] = {}, ini[MAX_PATH] = {};
    HMODULE self = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&loadConfig, &self) &&
        GetModuleFileNameW(self, mod, MAX_PATH)) {
        lstrcpyW(ini, mod);
        wchar_t* slash = wcsrchr(ini, L'\\');
        if (slash) { *slash = 0;                      /* drop winlist.dll   */
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
    g_altTrick = (int)GetPrivateProfileIntW(L"Winlist", L"AltTrick", 1, ini);
    g_titles   = (int)GetPrivateProfileIntW(L"Winlist", L"Titles", 9, ini);
    if (g_titles < 1) g_titles = 1;
    if (g_titles > MAX_TITLES) g_titles = MAX_TITLES;
}

/* ------------------------------ wide helpers -------------------------------- */

static int wideToUtf8(const wchar_t* w, char* a, int ac) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, a, ac, NULL, NULL);
    return (n > 0) ? n - 1 : 0;
}

static void lowerW(wchar_t* s) { for (; *s; ++s) *s = (wchar_t)towlower(*s); }

/* titles with newlines/tabs (rare chat apps) become one line for rows and
 * for the console answer */
static void sanitizeW(const wchar_t* src, wchar_t* dst, int cap) {
    int i;
    for (i = 0; src[i] && i < cap - 1; ++i) dst[i] = iswcntrl(src[i]) ? L' ' : src[i];
    dst[i] = 0;
}

static void answer(const wchar_t* msg) { wideToUtf8(msg, g_result, sizeof(g_result)); }

/* ------------------------------ enumeration --------------------------------- */

static const wchar_t* kSkipClass[] = {
    L"TypeBoxMain", L"TypeBoxDropDown", L"TypeBoxSettings",
    L"TypeBoxWinlist"                     /* our own dialog class, reserved  */
};

static WIN_ENT g_hits[MAX_TITLES];
static int     g_nhits, g_total, g_cap;
static wchar_t g_needle[MAX_PATH];        /* lowercased substring, empty = all */

static BOOL CALLBACK collect(HWND hwnd, LPARAM) {
    wchar_t cls[96], title[TITLE_LEN], low[TITLE_LEN];
    int i;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextLengthW(hwnd) <= 0) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;
    if (!GetClassNameW(hwnd, cls, 96)) return TRUE;
    for (i = 0; i < (int)(sizeof(kSkipClass) / sizeof(kSkipClass[0])); ++i)
        if (_wcsicmp(cls, kSkipClass[i]) == 0) return TRUE;
    if (GetWindowTextW(hwnd, title, TITLE_LEN) <= 0) return TRUE;   /* race */
    if (g_needle[0]) {
        lstrcpynW(low, title, TITLE_LEN);
        lowerW(low);
        if (!wcsstr(low, g_needle)) return TRUE;
    }
    ++g_total;
    if (g_nhits < g_cap) {
        g_hits[g_nhits].hwnd = hwnd;
        lstrcpynW(g_hits[g_nhits].title, title, TITLE_LEN);
        ++g_nhits;
    }
    return TRUE;
}

/* fresh walk, newest-z-order-first; results land in g_hits/g_total */
static void enumerate(const wchar_t* needleLower, int cap) {
    g_nhits = 0; g_total = 0; g_cap = cap;
    lstrcpynW(g_needle, needleLower, MAX_PATH);
    EnumWindows(collect, 0);
}

/* -------------------------------- activation --------------------------------- */

/* beat the foreground lock once: tap ALT down+up via SendInput so the next
 * SetForegroundWindow is allowed (AltTrick=0 disables) */
static void tapAlt(void) {
    INPUT in[2];
    ZeroMemory(in, sizeof(in));
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_MENU;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = VK_MENU; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

static void activate(HWND h, const wchar_t* title) {
    wchar_t clean[TITLE_LEN], msg[256];
    FLASHWINFO fi;
    sanitizeW(title, clean, TITLE_LEN);
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
    if (SetForegroundWindow(h)) {
        wsprintfW(msg, L"win: activated '%s'", clean);
        answer(msg);
        return;
    }
    if (g_altTrick) {
        tapAlt();
        if (SetForegroundWindow(h)) {
            wsprintfW(msg, L"win: activated '%s'", clean);
            answer(msg);
            return;
        }
    }
    {   /* still blocked: blink the target, do NOT steal the foreground */
        fi.cbSize = sizeof(fi);
        fi.hwnd = h;
        fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
        fi.uCount = 3;
        fi.dwTimeout = 0;
        FlashWindowEx(&fi);
    }
    wsprintfW(msg, L"win: flashed '%s' - click it", clean);
    answer(msg);
}

/* --------------------------------- commands ---------------------------------- */

static void doFilter(const wchar_t* sub) {
    wchar_t needle[MAX_PATH], q[128], msg[256];
    int i;
    sanitizeW(sub, q, 100);                        /* display copy, one line */
    lstrcpynW(needle, sub, MAX_PATH);
    lowerW(needle);
    enumerate(needle, g_titles);
    if (!g_total) {
        g_lastCount = 0;
        wsprintfW(msg, L"win: no window like '%s'", q);
        answer(msg);
        return;
    }
    for (i = 0; i < g_nhits; ++i) {
        g_last[i].hwnd = g_hits[i].hwnd;
        lstrcpynW(g_last[i].title, g_hits[i].title, TITLE_LEN);
    }
    g_lastCount = g_nhits;
    activate(g_last[0].hwnd, g_last[0].title);     /* topmost match wins */
}

static void doIndex(int n) {
    wchar_t clean[TITLE_LEN], msg[256];
    if (!g_lastCount) { answer(L"win: no filter yet"); return; }
    if (n < 1 || n > g_lastCount) {
        wsprintfW(msg, L"win: no match %d - last filter had %d", n, g_lastCount);
        answer(msg);
        return;
    }
    if (!IsWindow(g_last[n - 1].hwnd)) {
        sanitizeW(g_last[n - 1].title, clean, TITLE_LEN);
        wsprintfW(msg, L"win: '%s' is gone", clean);
        answer(msg);
        return;
    }
    activate(g_last[n - 1].hwnd, g_last[n - 1].title);
}

/* last filter's matches (numbered so "win <n>" picks the same rows), or —
 * when nothing is cached — every qualifying top-level window */
static void doList(void) {
    wchar_t out[700], one[180], clean[TITLE_LEN], tail[48];
    int i, shown = 0, total = 0, useLast = 0;
    for (i = 0; i < g_lastCount; ++i) if (IsWindow(g_last[i].hwnd)) ++total;
    if (total) useLast = 1;
    if (!useLast) {
        enumerate(L"", g_titles);
        total = g_total;
    }
    if (!total) { answer(L"win: no windows"); return; }
    lstrcpyW(out, L"win: ");
    for (i = 0; i < (useLast ? g_lastCount : g_nhits); ++i) {
        if (!IsWindow((useLast ? g_last[i] : g_hits[i]).hwnd)) continue;
        if (lstrlenW(out) > 460 && shown < total) break;   /* ~500 char cap */
        ++shown;
        if (shown > 1) lstrcatW(out, L" | ");
        sanitizeW((useLast ? g_last[i] : g_hits[i]).title, clean, TITLE_LEN);
        wsprintfW(one, L"%d) %s", i + 1, clean);           /* index = "win <n>" */
        lstrcatW(out, one);
    }
    if (shown < total) {
        wsprintfW(tail, L" (+%d more)", total - shown);
        lstrcatW(out, tail);
    }
    answer(out);
}

/* ----------------------------------- ABI ------------------------------------ */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
}

PLUGIN_API void __cdecl Unload(void) {}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Window Switcher";
    i.version = "1.0";
    i.description = "win <title> - activate a visible window by title substring";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "win" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

/* ET rows: serve from g_last only (never enumerate per keystroke), rows are
 * "win <sanitized title>", unique, always starting with the word "win " so
 * host completion keeps the command; gallery samples when nothing is cached. */
static char g_etRows[9][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 9) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    wchar_t seen[9][TITLE_LEN], clean[TITLE_LEN], row[176];
    char arow[256];
    int len, i, j, seenN = 0, dup;
    g_etCount = 0;
    if (!input) return 0;
    len = (int)lstrlenA(input);
    if (len > 3 || (len && _strnicmp(input, "win", len) != 0)) return 0;  /* prefix of "win", empty counts */
    for (i = 0; i < g_lastCount && g_etCount < 9; ++i) {
        if (!IsWindow(g_last[i].hwnd)) continue;
        dup = 0;
        for (j = 0; j < seenN; ++j) if (_wcsicmp(seen[j], g_last[i].title) == 0) { dup = 1; break; }
        if (dup) continue;
        if (seenN < 9) { lstrcpynW(seen[seenN], g_last[i].title, TITLE_LEN); ++seenN; }
        sanitizeW(g_last[i].title, clean, TITLE_LEN);
        wsprintfW(row, L"win %s", clean);
        wideToUtf8(row, arow, sizeof(arow));
        etAdd(arow);
    }
    if (!g_etCount) {                                  /* gallery examples */
        etAdd("win notepad");
        etAdd("win chrome");
    }
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    const char* p;
    wchar_t argW[MAX_PATH];
    char a[512];
    int n;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    if (_strnicmp(str, "win", 3) != 0) return ex;
    p = str + 3;
    while (*p == ' ') ++p;
    lstrcpynA(a, p, sizeof(a));
    {   /* trim trailing spaces */
        size_t n = strlen(a);
        while (n && isspace((unsigned char)a[n - 1])) a[--n] = 0;
    }
    MultiByteToWideChar(CP_UTF8, 0, a, -1, argW, MAX_PATH);
    g_result[0] = 0;
    if (!argW[0]) {
        doList();                                      /* bare "win" lists */
    } else {
        int isIdx = 1, i;
        if (lstrlenW(argW) > 4) isIdx = 0;            /* absurd run of digits = substring */
        for (i = 0; isIdx && argW[i]; ++i) if (!iswdigit(argW[i])) isIdx = 0;
        if (isIdx) {
            n = 0;
            for (i = 0; argW[i]; ++i) n = n * 10 + (int)(argW[i] - L'0');
            doIndex(n);
        } else {
            doFilter(argW);
        }
    }
    if (!g_result[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
