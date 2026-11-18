/* off.dll - TypeBox power plugin (v0.30.0 wave, gallery #16).
 *
 *   off                schedule shutdown in [Off] DefaultMin minutes
 *   off <min>          schedule shutdown in <min> minutes (1..1440)
 *   off now            shutdown immediately
 *   off cancel         cancel a scheduled shutdown (shutdown /a)
 *   restart [min]     same for reboot
 *   off sleep          suspend to RAM (SetSuspendState)
 *   off hibernate    write hiberfil and power off (needs powercfg /h on)
 *   off hybrid       hybrid shutdown (shutdown /hy /f)
 *   off logoff       end the session (ExitWindowsEx)
 *
 * SAFETY CONTRACT: with TYPEBOX_HEADLESS=1 or [UI] Headless=1 every power
 * action becomes a dry-run answer "off: [headless] would run <cmd>" - no
 * InitiateSystemShutdownExW, SetSuspendState, ExitWindowsEx or shutdown.exe
 * is ever invoked. The test suite runs in headless and asserts this.
 *
 * With [Off] Confirm=1 a schedule needs the SAME command twice within
 * [Off] WarnSec seconds - a stray Enter never schedules a shutdown.
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
#include <windows.h>
#include <powrprof.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef OFF_BUILDING_DLL
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

static HWND  g_mainWnd = NULL;
static int   g_headless = 0;      /* THE SAFETY SWITCH, see header */
static int   g_confirm = 1;       /* [Off] Confirm                 */
static int   g_warnSec = 60;      /* confirm window, [Off] WarnSec */
static int   g_defMin = 30;       /* [Off] DefaultMin              */

/* pending confirmation for scheduled shutdown/restart */
static int       g_pendKind = 0;  /* 0 none, 1 shutdown, 2 restart */
static int       g_pendMin = 0;
static ULONGLONG g_pendTick = 0;

static wchar_t g_wout[512];
static char    g_out[2048];

static void putLine(const wchar_t* w) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, g_out, sizeof(g_out), NULL, NULL);
}

static void loadConfig(void) {
    wchar_t ini[MAX_PATH] = {}, dir[MAX_PATH] = {};
    wchar_t hb[8];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    { wchar_t* bs = wcsrchr(dir, L'\\'); if (bs) bs[1] = 0; }
    wsprintfW(ini, L"%sTypeBox.ini", dir);
    g_confirm = GetPrivateProfileIntW(L"Off", L"Confirm", 1, ini);
    g_warnSec = GetPrivateProfileIntW(L"Off", L"WarnSec", 60, ini);
    if (g_warnSec < 5) g_warnSec = 5;
    if (g_warnSec > 300) g_warnSec = 300;
    g_defMin = GetPrivateProfileIntW(L"Off", L"DefaultMin", 30, ini);
    if (g_defMin < 1) g_defMin = 1;
    if (g_defMin > 1440) g_defMin = 1440;
    GetPrivateProfileStringW(L"UI", L"Headless", L"0", hb, 8, ini);
    if (hb[0] == L'1') g_headless = 1;
    {   /* the env var set by the harness is the primary contract */
        wchar_t e[8] = L"";
        if (GetEnvironmentVariableW(L"TYPEBOX_HEADLESS", e, 8) > 0 && e[0] == L'1')
            g_headless = 1;
    }
}

static int enablePriv(const wchar_t* nameW) {
    HANDLE tok = NULL;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
        return 0;
    if (!LookupPrivilegeValueW(NULL, nameW, &luid)) { CloseHandle(tok); return 0; }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL);
    CloseHandle(tok);
    return GetLastError() == ERROR_SUCCESS;
}

/* run a console tool hidden and wait; returns exit code, -1 on spawn fail */
static long runTool(const wchar_t* exe, const wchar_t* args) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t line[512];
    DWORD code = (DWORD)-1;
    wsprintfW(line, L"%s %s", exe, args);
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, 10000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (long)code;
}

/* ------------------------------ commands ---------------------------------- */

static const char* schedule(int kind, int min) { /* kind 1=shutdown 2=reboot */
    wchar_t out[320];
    const wchar_t* tag = (kind == 2) ? L"restart" : L"off";
    const wchar_t* flag = (kind == 2) ? L"/r" : L"/s";
    if (g_confirm && !(min == 0 && g_pendKind == kind && g_pendMin == 0)) {
        /* one-strike confirm for anything but an already-confirmed `now` */
        int fresh = !(g_pendKind == kind && g_pendMin == min &&
                      GetTickCount64() - g_pendTick < (ULONGLONG)g_warnSec * 1000ULL);
        if (fresh) {
            g_pendKind = kind; g_pendMin = min; g_pendTick = GetTickCount64();
            if (min == 0)
                wsprintfW(out, L"%s: press again within %d s: %s now", tag, g_warnSec, tag);
            else
                wsprintfW(out, L"%s: press again within %d s: %s %d", tag, g_warnSec, tag, min);
            putLine(out);
            return g_out;
        }
    }
    g_pendKind = 0;
    if (g_headless) {
        wsprintfW(out, L"%s: [headless] would run shutdown %s /t %d", tag, flag, min * 60);
        putLine(out);
        return g_out;
    }
    {
        enablePriv(SE_SHUTDOWN_NAME);
        if (InitiateSystemShutdownExW(NULL, (LPWSTR)L"TypeBox off.dll",
                                      (UINT)min * 60, TRUE, kind == 2,
                                      SHTDN_REASON_MAJOR_APPLICATION |
                                      SHTDN_REASON_MINOR_MAINTENANCE |
                                      SHTDN_REASON_FLAG_PLANNED)) {
            if (min == 0) wsprintfW(out, L"%s: shutting down now", tag);
            else wsprintfW(out, L"%s: %s in %d min (cancel: off cancel)", tag,
                           (kind == 2) ? L"restart" : L"shutdown", min);
            putLine(out);
            return g_out;
        }
        wsprintfW(out, L"%s: failed (err %u)", tag, GetLastError());
        putLine(out);
        return g_out;
    }
}

static const char* cancel(void) {
    wchar_t out[320];
    if (g_pendKind) {
        g_pendKind = 0;
        putLine(L"off: confirmation cleared - nothing was scheduled");
        return g_out;
    }
    if (g_headless) {
        putLine(L"off: [headless] would run shutdown /a");
        return g_out;
    }
    {
        long code = runTool(L"shutdown", L"/a");
        if (code == 0) putLine(L"off: shutdown cancelled");
        else if (code == 1116) putLine(L"off: nothing was scheduled");
        else if (code == -1) putLine(L"off: could not run shutdown /a");
        else wsprintfW(out, L"off: cancel failed (exit %d)", (int)code), putLine(out);
        return g_out;
    }
}

static const char* doSleep(void) {
    wchar_t out[320];
    if (g_headless) { putLine(L"off: [headless] would run SetSuspendState(sleep)"); return g_out; }
    enablePriv(SE_SHUTDOWN_NAME);
    putLine(L"off: suspending now");           /* shown right before the box sleeps */
    if (!SetSuspendState(FALSE, FALSE, FALSE)) {
        wsprintfW(out, L"off: sleep failed (err %u)", GetLastError());
        putLine(out);
    }
    return g_out;
}

static const char* doHibernate(void) {
    wchar_t out[320];
    if (g_headless) { putLine(L"off: [headless] would run SetSuspendState(hibernate)"); return g_out; }
    {
        /* mingw-w64 headers lack POWER_CAPABILITIES and the extended
         * SYSTEM_POWER_STATUS; the registry value is the same source of
         * truth powercfg /h on/off writes. */
        HKEY hk;
        DWORD v = 0, sz = sizeof(v);
        int known = 0, on = 0;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          L"SYSTEM\\CurrentControlSet\\Control\\Power",
                          0, KEY_READ, &hk) == ERROR_SUCCESS) {
            if (RegQueryValueExW(hk, L"HibernateEnabled", NULL, NULL,
                                 (BYTE*)&v, &sz) == ERROR_SUCCESS) {
                known = 1; on = (v != 0);
            }
            RegCloseKey(hk);
        }
        if (known && !on) {
            putLine(L"off: hibernate: disabled in system - enable with powercfg /h on");
            return g_out;
        }
    }
    enablePriv(SE_SHUTDOWN_NAME);
    putLine(L"off: hibernating now");
    if (!SetSuspendState(TRUE, FALSE, FALSE)) {
        /* failed after we already answered; second answer replaces it */
        wsprintfW(out, L"off: hibernate failed (err %u) - try powercfg /h on", GetLastError());
        putLine(out);
    }
    return g_out;
}

static const char* doHybrid(void) {
    wchar_t out[320];
    if (g_headless) { putLine(L"off: [headless] would run shutdown /hy /f"); return g_out; }
    enablePriv(SE_SHUTDOWN_NAME);
    {
        long code = runTool(L"shutdown", L"/hy /f");
        if (code == 0 || code == -1) { putLine(L"off: hybrid shutdown started"); return g_out; }
        wsprintfW(out, L"off: hybrid failed (exit %d)", (int)code);
        putLine(out);
        return g_out;
    }
}

static const char* doLogoff(void) {
    wchar_t out[320];
    if (g_headless) { putLine(L"off: [headless] would run ExitWindowsEx(EWX_LOGOFF)"); return g_out; }
    enablePriv(SE_SHUTDOWN_NAME);
    if (ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0)) { putLine(L"off: logging off"); return g_out; }
    wsprintfW(out, L"off: logoff failed (err %u)", GetLastError());
    putLine(out);
    return g_out;
}

/* ------------------------------- dispatch ---------------------------------- */

static int eqi(const char* a, const char* b) { return _stricmp(a, b) == 0; }

/* returns answer text or NULL when not ours */
static const char* dispatch(const char* line) {
    char a[64], b[64];
    /* "off" | "off <arg>" ; "restart" | "restart <arg>" */
    if (_strnicmp(line, "off", 3) == 0 && (line[3] == 0 || line[3] == ' ')) {
        const char* arg = line + 3;
        while (*arg == ' ') ++arg;
        if (!*arg) return schedule(1, g_defMin);
        lstrcpynA(a, arg, sizeof(a));
        { char* sp = strchr(a, ' '); if (sp) *sp = 0; }
        if (eqi(a, "cancel")) return cancel();
        if (eqi(a, "now"))    return schedule(1, 0);
        if (eqi(a, "sleep"))  return doSleep();
        if (eqi(a, "hibernate")) return doHibernate();
        if (eqi(a, "hybrid")) return doHybrid();
        if (eqi(a, "logoff") || eqi(a, "logout")) return doLogoff();
        {   /* pure number of minutes? */
            const char* p = a;
            if (*p) {
                while (*p && *p >= '0' && *p <= '9') ++p;
                if (!*p) {
                    int m = atoi(a);
                    if (m >= 1 && m <= 1440) return schedule(1, m);
                }
            }
        }
        putLine(L"usage: off [now|<min>|cancel|sleep|hibernate|hybrid|logoff]");
        return g_out;
    }
    if (_strnicmp(line, "restart", 7) == 0 && (line[7] == 0 || line[7] == ' ')) {
        const char* arg = line + 7;
        while (*arg == ' ') ++arg;
        if (!*arg) return schedule(2, g_defMin);
        lstrcpynA(b, arg, sizeof(b));
        { char* sp = strchr(b, ' '); if (sp) *sp = 0; }
        if (eqi(b, "cancel")) return cancel();
        if (eqi(b, "now"))    return schedule(2, 0);
        {
            const char* p = b;
            if (*p) {
                while (*p && *p >= '0' && *p <= '9') ++p;
                if (!*p) {
                    int m = atoi(b);
                    if (m >= 1 && m <= 1440) return schedule(2, m);
                }
            }
        }
        putLine(L"usage: restart [now|<min>]  (cancel: off cancel)");
        return g_out;
    }
    return NULL;
}

/* ----------------------------------- ABI ------------------------------------ */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
}

PLUGIN_API void __cdecl Unload(void) { }

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Off";
    i.version = "1.0";
    i.description = "off [now|<min>|cancel|sleep|hibernate|hybrid|logoff], restart [min] - headless = dry-run always";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 2; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[2] = { "off", "restart" };
    return (n >= 0 && n < 2) ? k[n] : NULL;
}

static char g_etRows[8][128];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 8) lstrcpynA(g_etRows[g_etCount++], r, 128); }

static int pref(const char* in, const char* pfx) {
    int pl = lstrlenA(pfx), il = lstrlenA(in);
    if (il > pl) return _strnicmp(in, pfx, (size_t)pl) == 0;
    return _strnicmp(in, pfx, (size_t)il) == 0;
}

PLUGIN_API int __cdecl GetCountET(const char* input) {
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    if (_strnicmp(input, "off", 3) == 0) {
        if (pref(input, "off")) etAdd("off now");
        if (pref(input, "off")) etAdd("off cancel");
        if (pref(input, "off")) etAdd("off sleep");
        if (pref(input, "off")) etAdd("off hibernate");
        if (pref(input, "off")) etAdd("off hybrid");
        if (pref(input, "off")) etAdd("off logoff");
    } else if (_strnicmp(input, "restart", 7) == 0) {
        etAdd("restart now");
        etAdd("restart 60");
    }
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
    text = dispatch(str);
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = text;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(text);
    return ex;
}
