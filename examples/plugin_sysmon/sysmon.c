/*
 * TypeBox plugin: system monitor - "cpu / ram / batt / disk / sys".
 * Spec: docs/PLUGIN_GALLERY.md #8. Console-first: every command answers
 * INSIDE the console (con_text); "sys" also lands on the clipboard, and
 * [Sysmon] Popup=1 adds the MessageBox the original spec asked for.
 *
 * Build (MinGW):  gcc -shared -O2 -DSYSMON_BUILDING_DLL sysmon.c -o sysmon.dll -luser32
 * Build (MSVC):   cl /LD /DSYSMON_BUILDING_DLL /Fe:sysmon.dll sysmon.c user32.lib
 * Licensed under GPL-3.0.
 *
 * Usage:
 *   cpu   -> "cpu: 17% (8 cores)"          (delta of two GetSystemTimes samples)
 *   ram   -> "ram: 9.8/15.4 GB (63%)"      (GlobalMemoryStatusEx)
 *   batt  -> "batt: 76%, charging"         ("no battery detected" on desktops)
 *   disk  -> "disk: C: 137.2/476.9 GB | D: 89.1/931.5 GB"   (fixed drives)
 *   sys   -> cpu + ram + batt + disk + uptime as one line, copied to clipboard
 *
 * The CPU percent is the delta between two GetSystemTimes snapshots kept by
 * the plugin's own message-only window (WM_TIMER every [Sysmon] SampleMs ms).
 * The FIRST call before the first tick answers "cpu: sampling (ask again in
 * 2 s)" - never a stale 0%.
 *
 * Config (TypeBox.ini next to the host exe, DLL lives in Plugins\):
 *   [Sysmon]
 *     SampleMs  CPU sampler interval, 500..60000   (default 2000)
 *     DiskMinGB hide drives whose TOTAL is below N GB (0 = show all fixed)  (default 0)
 *     Popup     1 = "sys" also shows a MessageBox                             (default 0; console-first)
 */
#include <windows.h>
#include <wchar.h>

#ifdef SYSMON_BUILDING_DLL
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

static HWND g_mainWnd = NULL;
static HWND g_msgWnd  = NULL;
static int  g_sampleMs = 2000;
static int  g_diskMinGB = 0;
static int  g_popup = 0;
static int  g_headless = 0;  /* v0.27: TYPEBOX_HEADLESS=1 / [UI] Headless=1 -
                              * the sys MessageBox is muted for quiet suites */

static ULARGE_INTEGER g_idle, g_kern, g_user;
static int      g_cpuInit = 0;
static int      g_cpuReady = 0;
static int      g_cpuPct = 0;
static int      g_cores = 1;

static char     g_result[4096];

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
    g_sampleMs  = (int)GetPrivateProfileIntW(L"Sysmon", L"SampleMs", 2000, ini);
    g_diskMinGB = (int)GetPrivateProfileIntW(L"Sysmon", L"DiskMinGB", 0, ini);
    g_popup     = (int)GetPrivateProfileIntW(L"Sysmon", L"Popup", 0, ini);
    { wchar_t ev[8]; ev[0] = 0;
      GetEnvironmentVariableW(L"TYPEBOX_HEADLESS", ev, 7);
      g_headless = (ev[0] == L'1') ||
                   (int)GetPrivateProfileIntW(L"UI", L"Headless", 0, ini); }
    if (g_sampleMs < 500) g_sampleMs = 500;
    if (g_sampleMs > 60000) g_sampleMs = 60000;
}

/* --------------------------------- helpers ---------------------------------- */

/* one decimal GB from bytes: 137.2 - via integer math only, no CRT format */
static void fmtGB(ULONGLONG bytes, char* out) {
    ULONGLONG v10 = bytes / 107374182ULL;          /* 0.1 GB units             */
    wsprintfA(out, "%u.%u GB", (unsigned)(v10 / 10), (unsigned)(v10 % 10));
}

static void ftToULARGE(const FILETIME* ft, ULARGE_INTEGER* u) {
    u->LowPart = ft->dwLowDateTime; u->HighPart = ft->dwHighDateTime;
}

static void sampleTick(void) {
    FILETIME ftIdle, ftK, ftU;
    ULARGE_INTEGER i, k, u;
    if (!GetSystemTimes(&ftIdle, &ftK, &ftU)) return;
    ftToULARGE(&ftIdle, &i); ftToULARGE(&ftK, &k); ftToULARGE(&ftU, &u);
    if (g_cpuInit) {
        ULONGLONG di = i.QuadPart - g_idle.QuadPart;
        ULONGLONG dk = k.QuadPart - g_kern.QuadPart;
        ULONGLONG du = u.QuadPart - g_user.QuadPart;
        ULONGLONG tot = di + dk + du;
        if (tot) g_cpuPct = (int)((100 * (tot - di) + tot / 2) / tot);
        if (g_cpuPct < 0) g_cpuPct = 0;
        if (g_cpuPct > 100) g_cpuPct = 100;
        g_cpuReady = 1;
    }
    g_idle = i; g_kern = k; g_user = u;
    g_cpuInit = 1;
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TIMER) { sampleTick(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

/* --------------------------------- readouts --------------------------------- */

static int buildCpu(char* out, int cap) {
    if (!g_cpuReady) return wsprintfA(out, "cpu: sampling (ask again in %d s)",
                                      g_sampleMs / 1000);
    return wsprintfA(out, "cpu: %d%% (%d cores)", g_cpuPct, g_cores);
}

static int buildRam(char* out, int cap) {
    MEMORYSTATUSEX ms;
    char used[32], total[32];
    (void)cap;
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return wsprintfA(out, "ram: unavailable");
    fmtGB(ms.ullTotalPhys - ms.ullAvailPhys, used);
    fmtGB(ms.ullTotalPhys, total);
    return wsprintfA(out, "ram: %s/%s (%u%%)", used, total, (unsigned)ms.dwMemoryLoad);
}

static int buildBatt(char* out) {
    SYSTEM_POWER_STATUS sp;
    if (!GetSystemPowerStatus(&sp)) return wsprintfA(out, "batt: unavailable");
    if (sp.BatteryFlag == 128 || (sp.BatteryFlag == 0 && sp.BatteryLifePercent == 0 &&
                                  sp.SystemStatusFlag == 0))
        return wsprintfA(out, "batt: no battery detected");
    if (sp.BatteryFlag & 128) return wsprintfA(out, "batt: no battery detected");
    {
        unsigned pct = sp.BatteryLifePercent;         /* 255 = unknown         */
        if (pct == 255) return wsprintfA(out, "batt: percent unknown");
        if (sp.BatteryFlag & 8)  return wsprintfA(out, "batt: %u%%, charging", pct);
        if (sp.SystemStatusFlag & 1) return wsprintfA(out, "batt: %u%%, plugged in", pct);
        return wsprintfA(out, "batt: %u%%, on battery", pct);
    }
}

static int buildDisk(char* out, int cap) {
    DWORD mask = GetLogicalDrives();
    int n = 0, i, used;
    lstrcpyA(out, "disk: ");
    used = (int)lstrlenA(out);
    if (!mask) return wsprintfA(out, "disk: unavailable");
    for (i = 0; i < 26 && used < cap - 48; ++i) {
        wchar_t root[4] = { L'A' + (wchar_t)i, L':', L'\\', 0 };
        ULARGE_INTEGER freeB, totalB;
        char f[32], t[32];
        if (!(mask & (1u << i))) continue;
        if (GetDriveTypeW(root) != DRIVE_FIXED) continue;
        if (!GetDiskFreeSpaceExW(root, &freeB, &totalB, NULL)) continue;  /* vanished */
        if (g_diskMinGB > 0 && totalB.QuadPart / 1073741824ULL < (ULONGLONG)g_diskMinGB)
            continue;
        fmtGB(freeB.QuadPart, f);
        fmtGB(totalB.QuadPart, t);
        used += wsprintfA(out + used, n ? " | %c: %s/%s" : "%c: %s/%s",
                          'A' + i, f, t);
        ++n;
    }
    if (!n) return wsprintfA(out, "disk: no fixed drives matched");
    return used;
}

static int buildSys(char* out, int cap) {
    int used = 0;
    char cpu[64], ram[96], batt[64], disk[1024], up[48];
    ULONGLONG secs = GetTickCount64() / 1000;
    buildCpu(cpu, (int)sizeof(cpu));
    buildRam(ram, (int)sizeof(ram));
    buildBatt(batt);
    buildDisk(disk, (int)sizeof(disk));
    if (secs >= 86400) wsprintfA(up, "up %u d %u h", (unsigned)(secs / 86400), (unsigned)((secs / 3600) % 24));
    else if (secs >= 3600) wsprintfA(up, "up %u h %u m", (unsigned)(secs / 3600), (unsigned)((secs / 60) % 60));
    else wsprintfA(up, "up %u m", (unsigned)(secs / 60));
    used  = wsprintfA(out, "%s | %s | %s", cpu, ram, batt);
    used += wsprintfA(out + used, " | %s | %s", disk, up);
    (void)cap;
    return used;
}

/* v0.24 hardening: OpenClipboard needs a thread window - NULL can BLOCK the
 * host thread on a wedged clipboard chain; a real msg-only hwnd fails fast. */
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
        wc.lpszClassName = L"TypeBoxSysmonClip";
        wc.hInstance     = self;
        if (RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
            h = CreateWindowExW(0, wc.lpszClassName, L"clip", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, self, NULL);
    }
    return h;
}

static void copyToClipboard(const char* text) {
    int i;
    for (i = 0; i < 5; ++i) {
        if (OpenClipboard(ClipWnd())) {
            HGLOBAL h;
            wchar_t w[2048];
            int   wl;
            EmptyClipboard();
            MultiByteToWideChar(CP_UTF8, 0, text, -1, w, 2048);
            wl = (int)lstrlenW(w) + 1;
            h = GlobalAlloc(GMEM_MOVEABLE, wl * sizeof(wchar_t));
            if (h) {
                wchar_t* p = (wchar_t*)GlobalLock(h);
                if (p) { lstrcpyW(p, w); GlobalUnlock(h);
                         if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h); }
                else GlobalFree(h);
            }
            CloseClipboard();
            return;
        }
        Sleep(120);
    }                                                /* busy: console answer stays */
}

/* --------------------------------- commands --------------------------------- */

static const char* runCmd(const char* word) {
    g_result[0] = 0;
    if (!word[0]) return NULL;
    if (!_stricmp(word, "cpu"))       { buildCpu(g_result, (int)sizeof(g_result)); return g_result; }
    if (!_stricmp(word, "ram"))       { buildRam(g_result, (int)sizeof(g_result)); return g_result; }
    if (!_stricmp(word, "batt"))      { buildBatt(g_result); return g_result; }
    if (!_stricmp(word, "disk"))      { buildDisk(g_result, (int)sizeof(g_result)); return g_result; }
    if (!_stricmp(word, "sys")) {
        buildSys(g_result, (int)sizeof(g_result));
        copyToClipboard(g_result);
        if (g_popup && !g_headless) {
            wchar_t w[2048];
            MultiByteToWideChar(CP_UTF8, 0, g_result, -1, w, 2048);
            MessageBoxW(g_mainWnd, w, L"TypeBox \x2014 system", MB_OK | MB_ICONINFORMATION);
            /* console-first: the answer STAYS in the console even with the
               popup on - and RunString still returns run=1, so the host never
               falls through to treating "sys" as an external command. */
        }
        return g_result;
    }
    return NULL;
}

/* ----------------------------------- ABI ------------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
    { SYSTEM_INFO si; GetSystemInfo(&si);
      g_cores = (int)si.dwNumberOfProcessors; if (g_cores < 1) g_cores = 1; }
    { WNDCLASSEXW wc;
      ZeroMemory(&wc, sizeof(wc));
      wc.cbSize = sizeof(wc);
      wc.lpfnWndProc = WndProc;
      wc.hInstance = GetModuleHandleW(NULL);
      wc.lpszClassName = L"TypeBoxSysmonWnd";
      if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
          g_msgWnd = CreateWindowExW(0, L"TypeBoxSysmonWnd", L"TypeBoxSysmon", 0,
                                     0, 0, 0, 0, HWND_MESSAGE, NULL,
                                     GetModuleHandleW(NULL), NULL);
      } }
    if (g_msgWnd) SetTimer(g_msgWnd, 1, (UINT)g_sampleMs, NULL);
    sampleTick();                                   /* seed; first delta at +SampleMs */
}

PLUGIN_API void __cdecl Unload(void) {
    if (g_msgWnd) { KillTimer(g_msgWnd, 1); DestroyWindow(g_msgWnd); g_msgWnd = NULL; }
    g_cpuInit = 0; g_cpuReady = 0; g_cpuPct = 0;
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "System Monitor";
    i.version = "1.0";
    i.description = "cpu ram batt disk sys - live status, no admin, no tasks bar needed";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 5; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[5] = { "cpu", "ram", "batt", "disk", "sys" };
    return (n >= 0 && n < 5) ? k[n] : NULL;
}

static char g_etRows[6][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 6) lstrcpynA(g_etRows[g_etCount++], r, 256); }

static int pfx(const char* in, const char* word) {
    int n = lstrlenA(in);
    if (n < 1 || n > (int)lstrlenA(word)) return 0;
    return _strnicmp(in, word, n) == 0;
}

PLUGIN_API int __cdecl GetCountET(const char* input) {
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    if (pfx(input, "cpu"))  etAdd("cpu");
    if (pfx(input, "ram"))  etAdd("ram");
    if (pfx(input, "batt")) etAdd("batt");
    if (pfx(input, "disk")) etAdd("disk");
    if (pfx(input, "sys"))  etAdd("sys");
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    const char* text;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    while (*str == ' ') ++str;
    text = runCmd(str);
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
