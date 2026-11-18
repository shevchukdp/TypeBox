/*
 * TypeBox plugin: timers & stopwatch - "timer 10m tea" + "sw".
 * Spec: docs/PLUGIN_GALLERY.md #7. Console-first: every command answers
 * INSIDE the console (con_text); the tray balloon + beep fire when a timer
 * is due, the stopwatch optionally shows a topmost mini label.
 *
 * Build (MinGW):  gcc -shared -O2 -DTIMER_BUILDING_DLL timer.c -o timer.dll -luser32 -lwinmm -lshell32 -lgdi32
 * Build (MSVC):   cl /LD /DTIMER_BUILDING_DLL /Fe:timer.dll timer.c user32.lib winmm.lib shell32.lib gdi32.lib
 * Licensed under GPL-3.0.
 *
 * Usage:
 *   timer 5m tea        -> balloon "tea - time!" + beep after 5 minutes
 *   timer 90            -> plain numbers are seconds
 *   timer 1h30m         -> <num><suffix> runs: s, m, h
 *   timer               -> list running timers (also "timer list")
 *   timer stop [label]  -> stop the matching timer, or the soonest one
 *   sw                  -> start the stopwatch / report and stop it
 *   sw reset            -> stop and zero the stopwatch
 *
 * Config (TypeBox.ini next to the host exe, DLL lives in Plugins\):
 *   [Timer]
 *     MiniWindow  1 = stopwatch shows a topmost mini label while running (default 1)
 *     Sound       0 = balloon without beep/PlaySound                   (default 1)
 *     Max         simultaneous timers, 1..8                             (default 8)
 */
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <wchar.h>

#ifdef TIMER_BUILDING_DLL
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

#define MAX_TIMERS   8
#define TRAY_UID     0x071Du                 /* our own Shell_NotifyIcon uID  */
#define TICK_ID      1                        /* plugin WM_TIMER id            */
#define MAX_SECONDS  (24 * 3600)              /* one day is timer enough       */

typedef struct {
    int       active;
    ULONGLONG deadline;                        /* GetTickCount64() ms           */
    wchar_t   label[64];
    char      labelA[64];                      /* same label, console-friendly  */
} Timer;

static HWND    g_mainWnd = NULL;
static HWND    g_msgWnd  = NULL;
static HWND    g_miniWnd = NULL;
static int     g_trayOn  = 0;
static int     g_tickOn  = 0;

static Timer   g_t[MAX_TIMERS];
static int     g_max = MAX_TIMERS;
static int     g_sound = 1;
static int     g_mini  = 1;
/* v0.29 DPI wave: the mini window keeps its own dpi (WM_DPICHANGED wParam),
 * its 190x46 + 12px font are 96-logical and scaled through Scp(). */
static int     g_miniDpi = 96;
static HFONT   g_miniFont = NULL;
typedef UINT (WINAPI *MiniGDFW)(HWND);
static int DpiOf(HWND h) {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    MiniGDFW f = u ? (MiniGDFW)(void*)GetProcAddress(u, "GetDpiForWindow") : NULL;
    int d = f && h ? (int)f(h) : 96;
    return (d >= 96 && d <= 480) ? d : 96;
}
static int Scp(int dpi, int v) { return MulDiv(v, dpi >= 96 ? dpi : 96, 96); }
static void miniApplyFont(void) {
    HFONT nf = CreateFontW(-Scp(g_miniDpi, 12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           VARIABLE_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!nf) return;
    if (g_miniFont) DeleteObject(g_miniFont);
    g_miniFont = nf;
}

static int     g_swRunning = 0;
static ULONGLONG g_swStartMs = 0;
static ULONGLONG g_swElapsed = 0;              /* accumulated while stopped    */

static char    g_result[4096];                 /* con_text payload             */

/* --------------------------------- config ---------------------------------- */

static void loadConfig(void) {
    wchar_t mod[MAX_PATH] = {}, ini[MAX_PATH] = {};
    HMODULE self = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&loadConfig, &self) &&
        GetModuleFileNameW(self, mod, MAX_PATH)) {
        lstrcpyW(ini, mod);
        wchar_t* slash = wcsrchr(ini, L'\\');
        if (slash) { *slash = 0;
            lstrcatW(ini, L"\\TypeBox.ini");
            if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
                *wcsrchr(ini, L'\\') = 0;       /* drop TypeBox.ini            */
                *wcsrchr(ini, L'\\') = 0;       /* drop Plugins\               */
                lstrcatW(ini, L"\\TypeBox.ini");
            }
        } else lstrcpyW(ini, L"TypeBox.ini");
    } else lstrcpyW(ini, L"TypeBox.ini");
    g_max   = (int)GetPrivateProfileIntW(L"Timer", L"Max", MAX_TIMERS, ini);
    g_sound = (int)GetPrivateProfileIntW(L"Timer", L"Sound", 1, ini);
    g_mini  = (int)GetPrivateProfileIntW(L"Timer", L"MiniWindow", 1, ini);
    if (g_max < 1) g_max = 1;
    if (g_max > MAX_TIMERS) g_max = MAX_TIMERS;
}

/* --------------------------------- helpers ---------------------------------- */

static int anyActive(void) {
    int i;
    for (i = 0; i < g_max; ++i) if (g_t[i].active) return 1;
    return g_swRunning;
}

static void ensureTick(void) {
    if (!g_msgWnd) return;
    if (anyActive()) {
        if (!g_tickOn) { SetTimer(g_msgWnd, TICK_ID, 1000, NULL); g_tickOn = 1; }
    } else if (g_tickOn) {
        KillTimer(g_msgWnd, TICK_ID); g_tickOn = 0;
    }
}

/* "1:30:00" or "5:00" (h:mm:ss / m:ss) */
static void fmtClock(ULONGLONG secs, wchar_t* out) {
    ULONGLONG h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    wsprintfW(out, L"%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
}

static void fmtClockA(ULONGLONG secs, char* out) {
    ULONGLONG h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    if (h) wsprintfA(out, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    else   wsprintfA(out, "%u:%02u", (unsigned)m, (unsigned)s);
}

/* -------------------------------- tray balloon ------------------------------- */

static void trayAdd(void) {
    NOTIFYICONDATAW nid;
    if (g_trayOn || !g_msgWnd) return;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hWnd = g_msgWnd;
    nid.uID = (UINT)TRAY_UID;
    nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    lstrcpyW(nid.szTip, L"TypeBox timers");
    if (Shell_NotifyIconW(NIM_ADD, &nid)) g_trayOn = 1;
}

static void trayRemove(void) {
    NOTIFYICONDATAW nid;
    if (!g_trayOn || !g_msgWnd) return;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_msgWnd;
    nid.uID = (UINT)TRAY_UID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_trayOn = 0;
}

static void fireAlert(Timer* t) {
    NOTIFYICONDATAW nid;
    wchar_t text[128] = {};
    if (t->label[0])
        wsprintfW(text, L"%s - time!", t->label);
    else
        lstrcpyW(text, L"Timer - time!");
    if (g_trayOn && g_msgWnd) {
        ZeroMemory(&nid, sizeof(nid));
        nid.cbSize = sizeof(nid);
        nid.hWnd = g_msgWnd;
        nid.uID = (UINT)TRAY_UID;
        nid.uFlags = NIF_INFO;
        lstrcpynW(nid.szInfo, text, 255);
        lstrcpyW(nid.szInfoTitle, L"TypeBox");
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
    if (g_sound) {
        MessageBeep(MB_ICONEXCLAMATION);
        PlaySoundW(L"SystemExclamation", NULL, SND_ASYNC | SND_ALIAS);
    }
    if (g_mainWnd && IsWindow(g_mainWnd)) {
        FLASHWINFO fi;
        fi.cbSize = sizeof(fi);
        fi.hwnd = g_mainWnd;
        fi.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
        fi.uCount = 3;
        fi.dwTimeout = 0;
        FlashWindowEx(&fi);
    }
}

/* ------------------------------- mini window -------------------------------- */

static LRESULT CALLBACK MiniProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        HBRUSH bg = CreateSolidBrush(RGB(30, 30, 30));
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, bg); DeleteObject(bg);
        wchar_t txt[64] = {};
        GetWindowTextW(h, txt, 63);
        SetTextColor(dc, RGB(240, 240, 240));
        SetBkMode(dc, TRANSPARENT);
        HFONT f = g_miniFont ? g_miniFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT old = (HFONT)SelectObject(dc, f);
        DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, old);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_DPICHANGED) {                    /* v0.29 PMv2 wave */
        int d = (int)LOWORD(w);
        RECT* pr = (RECT*)l;
        g_miniDpi = (d >= 96 && d <= 480) ? d : 96;
        miniApplyFont();
        if (pr) SetWindowPos(h, HWND_TOPMOST, pr->left, pr->top,
                             pr->right - pr->left, pr->bottom - pr->top, SWP_NOACTIVATE);
        InvalidateRect(h, NULL, TRUE);
        return 0;
    }
    if (m == WM_DESTROY) {
        if (g_miniFont) { DeleteObject(g_miniFont); g_miniFont = NULL; }
        return 0;
    }
    if (m == WM_NCLBUTTONDOWN) return 0;       /* no title bar, don't drag     */
    return DefWindowProcW(h, m, w, l);
}

static void miniShow(ULONGLONG secs) {
    wchar_t txt[64], clock[32];
    if (!g_mini) return;
    fmtClock(secs, clock);
    wsprintfW(txt, L"stopwatch  %s", clock);
    if (!g_miniWnd) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = MiniProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.lpszClassName = L"TypeBoxTimerMini";
        if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            int w, ht;
            g_miniDpi = DpiOf(g_mainWnd);        /* v0.29: start on the console's dpi */
            w = Scp(g_miniDpi, 190); ht = Scp(g_miniDpi, 46);
            miniApplyFont();
            g_miniWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                L"TypeBoxTimerMini", txt, WS_POPUP | WS_VISIBLE,
                GetSystemMetrics(SM_CXSCREEN) - w - Scp(g_miniDpi, 24), Scp(g_miniDpi, 44), w, ht,
                NULL, NULL, GetModuleHandleW(NULL), NULL);
        }
    } else {
        SetWindowTextW(g_miniWnd, txt);
        InvalidateRect(g_miniWnd, NULL, TRUE);
    }
}

static void miniHide(void) {
    if (g_miniWnd) { DestroyWindow(g_miniWnd); g_miniWnd = NULL; }
}

/* --------------------------------- ticking ---------------------------------- */

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TIMER && w == TICK_ID) {
        ULONGLONG now = GetTickCount64();
        int i;
        for (i = 0; i < g_max; ++i) {
            if (g_t[i].active && now >= g_t[i].deadline) {
                fireAlert(&g_t[i]);
                g_t[i].active = 0;
            }
        }
        if (g_swRunning) miniShow((now - g_swStartMs) / 1000 + g_swElapsed / 1000);
        ensureTick();                            /* stops when all idle         */
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ------------------------------- duration parse ----------------------------- */

/* "90" "90s" "5m" "1h30m" "1h30m45s" -> seconds; 0 on garbage or zero. */
static ULONGLONG parseDuration(const char* s) {
    ULONGLONG total = 0;
    int any = 0;
    while (*s == ' ') ++s;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;      /* must start with a digit    */
        long n = 0;
        while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); ++s; }
        long mult = 1;
        if (*s == 's' || *s == 'S') { ++s; }
        else if (*s == 'm' || *s == 'M') { mult = 60; ++s; }
        else if (*s == 'h' || *s == 'H') { mult = 3600; ++s; }
        total += (ULONGLONG)n * (ULONGLONG)mult;
        any = 1;
        while (*s == ' ' || *s == ':' || *s == '-') ++s;   /* tolerate "1h 30m" */
    }
    if (!any || total == 0 || total > MAX_SECONDS) return 0;
    return total;
}

/* --------------------------------- commands --------------------------------- */

static void answer(const char* fmtResult) {
    lstrcpynA(g_result, fmtResult, (int)sizeof(g_result));
}

static int runningCount(void) {
    int i, n = 0;
    for (i = 0; i < g_max; ++i) if (g_t[i].active) ++n;
    return n;
}

static void doTimerList(char* out, int cap) {
    int n = runningCount(), i, used, first = 1;
    char clock[32];
    if (!n) { lstrcpyA(out, "timer: nothing running"); return; }
    lstrcpyA(out, "timer: ");
    used = (int)lstrlenA(out);
    for (i = 0; i < g_max && used < cap - 64; ++i) {
        ULONGLONG left = 0, now = GetTickCount64();
        if (!g_t[i].active) continue;
        if (g_t[i].deadline > now) left = (g_t[i].deadline - now + 999) / 1000;
        fmtClockA(left, clock);
        if (g_t[i].labelA[0])
            used += wsprintfA(out + used, first ? "%s (%s)" : ", %s (%s)", clock, g_t[i].labelA);
        else
            used += wsprintfA(out + used, first ? "%s" : ", %s", clock);
        first = 0;
    }
    lstrcatA(out, " left");
}

static const char* runTimer(const char* rest) {
    char line[512];
    const char* p = rest;
    g_result[0] = 0;
    while (*p == ' ') ++p;

    if (!*p || _strnicmp(p, "list", 4) == 0) {
        if (runningCount() || g_swRunning) {
            char list[384];
            doTimerList(list, (int)sizeof(list));
            answer(list);
        } else {
            answer("timer: nothing running");
        }
        return g_result;
    }
    if (_strnicmp(p, "stop", 4) == 0 && (p[4] == 0 || p[4] == ' ')) {
        p += 4;
        while (*p == ' ') ++p;
        int i, victim = -1;
        if (*p) {                                       /* stop by label       */
            wchar_t want[64];
            int k;
            for (k = 0; p[k] && k < 63; ++k) want[k] = (wchar_t)(unsigned char)p[k];
            want[k] = 0;
            for (i = 0; i < g_max; ++i)
                if (g_t[i].active && _wcsicmp(g_t[i].label, want) == 0) { victim = i; break; }
            if (victim < 0) { answer("timer: no running timer with that label"); return g_result; }
        } else {                                        /* stop the soonest    */
            ULONGLONG best = 0;
            for (i = 0; i < g_max; ++i)
                if (g_t[i].active && (!best || g_t[i].deadline < best)) { best = g_t[i].deadline; victim = i; }
            if (victim < 0) { answer("timer: nothing running"); return g_result; }
        }
        g_t[victim].active = 0;
        g_t[victim].label[0] = 0;
        g_t[victim].labelA[0] = 0;
        wsprintfA(line, "timer: stopped (%d running)", runningCount());
        answer(line);
        ensureTick();
        return g_result;
    }

    /* "<dur> [label]" */
    lstrcpynA(line, p, (int)sizeof(line));
    {   /* duration = leading run of digits/units, label = the rest */
        const char* q = line;
        char dur[32]; int d = 0;
        while (*q && d < 31) {
            if ((*q >= '0' && *q <= '9') || *q == 's' || *q == 'S' ||
                *q == 'm' || *q == 'M' || *q == 'h' || *q == 'H') { dur[d++] = *q; ++q; }
            else break;
        }
        dur[d] = 0;
        ULONGLONG secs = parseDuration(dur);
        if (!secs) { answer("timer: use 90, 5m, 1h30m, 90s"); return g_result; }
        int n = runningCount();
        if (n >= g_max) {
            wsprintfA(line, "timer: %d timers already running (timer stop first)", g_max);
            answer(line); return g_result;
        }
        int slot = 0;
        while (slot < g_max && g_t[slot].active) ++slot;
        g_t[slot].active = 1;
        g_t[slot].deadline = GetTickCount64() + secs * 1000;
        g_t[slot].label[0] = 0;
        g_t[slot].labelA[0] = 0;
        while (*q == ' ') ++q;
        if (*q) {
            int k;
            for (k = 0; q[k] && k < 63; ++k) {
                g_t[slot].labelA[k] = q[k];
                g_t[slot].label[k]  = (wchar_t)(unsigned char)q[k];
            }
            g_t[slot].labelA[k] = 0;
            g_t[slot].label[k]  = 0;
        }
        ensureTick();
        { char clock[32]; fmtClockA(secs, clock);
          if (g_t[slot].labelA[0])
              wsprintfA(line, "timer: started %s - %s (%d running)", clock,
                        g_t[slot].labelA, runningCount());
          else
              wsprintfA(line, "timer: started %s (%d running)", clock, runningCount());
        }
        answer(line);
        return g_result;
    }
}

static const char* runSw(const char* rest) {
    char line[128];
    g_result[0] = 0;
    while (*rest == ' ') ++rest;
    if (_strnicmp(rest, "reset", 5) == 0) {
        g_swRunning = 0; g_swElapsed = 0; g_swStartMs = 0;
        miniHide(); ensureTick();
        answer("stopwatch: reset");
        return g_result;
    }
    if (*rest) { answer("stopwatch: use sw or sw reset"); return g_result; }
    if (!g_swRunning) {
        g_swRunning = 1;
        g_swStartMs = GetTickCount64();
        ensureTick();
        miniShow(g_swElapsed / 1000);
        answer("stopwatch: started");
    } else {
        ULONGLONG secs = (GetTickCount64() - g_swStartMs) / 1000 + g_swElapsed / 1000;
        g_swElapsed = secs * 1000;
        g_swRunning = 0;
        miniHide(); ensureTick();
        { unsigned hh = (unsigned)(secs / 3600), mm = (unsigned)((secs % 3600) / 60),
                   ss = (unsigned)(secs % 60);
          wsprintfA(line, "stopwatch: %u:%02u:%02u", hh, mm, ss); }   // spec form 0:07:23
        answer(line);
    }
    return g_result;
}

/* ----------------------------------- ABI ------------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
    { WNDCLASSEXW wc;
      ZeroMemory(&wc, sizeof(wc));
      wc.cbSize = sizeof(wc);
      wc.lpfnWndProc = WndProc;
      wc.hInstance = GetModuleHandleW(NULL);
      wc.lpszClassName = L"TypeBoxTimerWnd";
      if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
          g_msgWnd = CreateWindowExW(0, L"TypeBoxTimerWnd", L"TypeBoxTimer", 0,
                                     0, 0, 0, 0, HWND_MESSAGE, NULL,
                                     GetModuleHandleW(NULL), NULL);
      } }
    trayAdd();
    ensureTick();                                 /* survives /reread reload   */
}

PLUGIN_API void __cdecl Unload(void) {
    int i;
    for (i = 0; i < MAX_TIMERS; ++i) g_t[i].active = 0;
    g_swRunning = 0; g_swElapsed = 0;
    miniHide();
    if (g_msgWnd && g_tickOn) KillTimer(g_msgWnd, TICK_ID);
    g_tickOn = 0;
    trayRemove();
    if (g_msgWnd) { DestroyWindow(g_msgWnd); g_msgWnd = NULL; }
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Timer";
    i.version = "1.0";
    i.description = "timer 5m tea + stopwatch sw - countdowns with tray alert";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 2; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[2] = { "timer", "sw" };
    return (n >= 0 && n < 2) ? k[n] : NULL;
}

static char g_etRows[6][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 6) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    g_etCount = 0;
    if (!input) return 0;
    int len = lstrlenA(input);
    if (len <= 3 && _strnicmp(input, "tim", len) == 0) {
        etAdd("timer 5m"); etAdd("timer 10m"); etAdd("timer 25m");
        etAdd("timer stop"); etAdd("timer list");
    } else if (len > 3 && _strnicmp(input, "tim", 3) == 0) {
        etAdd("timer stop");
    } else if (len <= 2 && _strnicmp(input, "sw", len) == 0) {
        etAdd("sw"); etAdd("sw reset");
    }
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    const char* text = NULL;
    if (_strnicmp(str, "timer", 5) == 0 && (str[5] == 0 || str[5] == ' '))
        text = runTimer(str + 5);
    else if (_strnicmp(str, "sw", 2) == 0 && (str[2] == 0 || str[2] == ' '))
        text = runSw(str + 2);
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
