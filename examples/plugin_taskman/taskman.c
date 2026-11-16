/*
 * TypeBox plugin: task manager lite — "kill <name|pid>", "task <filter>".
 * Spec: docs/PLUGIN_GALLERY.md #5, console-first: no modal list window, the
 * host is NEVER blocked except the optional kill confirmation (Confirm=0
 * disables even that). Zero dependencies beyond kernel32/user32.
 *
 * Build (MinGW):  gcc -shared -O2 -DTASKMAN_BUILDING_DLL taskman.c -o taskman.dll -lkernel32 -luser32
 * Build (MSVC):   cl /LD /DTASKMAN_BUILDING_DLL /Fe:taskman.dll taskman.c kernel32.lib user32.lib
 * Licensed under GPL-3.0.
 *
 * Config (TypeBox.ini next to the host exe, DLL lives in Plugins\ so ini = dllpath\..\TypeBox.ini):
 *   [Taskman]
 *     Confirm   0 = kill without the Yes/No popup                 (default 1)
 *     ListMax   max processes scanned into the cache per snapshot (default 300)
 *
 * Process snapshot: CreateToolhelp32Snapshot/Process32FirstW/NextW (kernel32,
 * no psapi). Refreshed at RunString time; the ET path serves from the cache
 * and refreshes only when it is older than 2000 ms.
 */
#include <windows.h>
#include <tlhelp32.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>

#ifdef TASKMAN_BUILDING_DLL
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

#define MAX_PROCS   1024
#define MAX_HITS    256
#define CACHE_MS    2000

typedef struct { DWORD pid; wchar_t name[MAX_PATH]; } PROC_ENT;

static HWND      g_mainWnd = NULL;
static PROC_ENT  g_procs[MAX_PROCS];
static int       g_nprocs = 0;
static DWORD     g_cacheTick = 0;
static int       g_haveCache = 0;
static int       g_confirm = 1, g_listMax = 300;

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
        if (slash) { *slash = 0;                      /* drop taskman.dll   */
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
    g_confirm = (int)GetPrivateProfileIntW(L"Taskman", L"Confirm", 1, ini);
    g_listMax = (int)GetPrivateProfileIntW(L"Taskman", L"ListMax", 300, ini);
    if (g_listMax < 1) g_listMax = 1;
    if (g_listMax > MAX_PROCS) g_listMax = MAX_PROCS;
}

/* ------------------------------ wide helpers -------------------------------- */

static int wideToUtf8(const wchar_t* w, char* a, int ac) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, a, ac, NULL, NULL);
    return (n > 0) ? n - 1 : 0;
}

static void lowerW(wchar_t* s) { for (; *s; ++s) *s = (wchar_t)towlower(*s); }

/* ------------------------------- snapshot ----------------------------------- */

static void refreshCache(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    int n = 0, total = 0, skip = 0;
    if (snap == INVALID_HANDLE_VALUE) { g_haveCache = 1; g_cacheTick = GetTickCount(); return; }
    pe.dwSize = sizeof(pe);
    /* count first: with >ListMax processes on the system, fresh ones sit at
     * the END of the toolhelp walk, so keep the LAST ListMax entries */
    if (Process32FirstW(snap, &pe)) { do { ++total; } while (Process32NextW(snap, &pe)); }
    if (total > g_listMax) skip = total - g_listMax;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (skip > 0) { --skip; continue; }
            if (n >= g_listMax) break;
            g_procs[n].pid = pe.th32ProcessID;
            lstrcpynW(g_procs[n].name, pe.szExeFile, MAX_PATH);
            ++n;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    g_nprocs = n;
    g_cacheTick = GetTickCount();
    g_haveCache = 1;
}

static void cacheIfStale(void) {
    if (!g_haveCache || (DWORD)(GetTickCount() - g_cacheTick) > CACHE_MS) refreshCache();
}

/* does image name (lowercased) contain the lowercased needle? */
static int matches(const wchar_t* name, const wchar_t* needleLower) {
    wchar_t low[MAX_PATH];
    lstrcpynW(low, name, MAX_PATH);
    lowerW(low);
    return wcsstr(low, needleLower) != NULL;
}

/* name match with optional ".exe": "notepad" matches "notepad.exe" */
static int nameMatch(const wchar_t* name, const wchar_t* want) {
    wchar_t withExe[MAX_PATH + 8];
    if (_wcsicmp(name, want) == 0) return 1;
    lstrcpyW(withExe, want);
    lstrcatW(withExe, L".exe");
    return _wcsicmp(name, withExe) == 0;
}

/* --------------------------------- task ------------------------------------- */

static void doTask(const wchar_t* arg) {
    wchar_t needle[MAX_PATH], out[700], one[128], cnt[48];
    int i, total = 0, shown = 0;
    lstrcpynW(needle, arg, MAX_PATH);
    lowerW(needle);
    for (i = 0; i < g_nprocs; ++i) if (matches(g_procs[i].name, needle)) ++total;
    if (!total) {
        wsprintfW(out, L"%s: no processes matched", arg);
        wideToUtf8(out, g_result, sizeof(g_result));
        return;
    }
    wsprintfW(out, L"%s: ", arg);
    for (i = 0; i < g_nprocs && shown < total; ++i) {
        if (!matches(g_procs[i].name, needle)) continue;
        if (lstrlenW(out) > 460 && shown < total) break;   /* ~500 char cap */
        ++shown;
        if (shown > 1) lstrcatW(out, L", ");
        wsprintfW(one, L"%lu ", g_procs[i].pid);
        lstrcatW(out, one);
        lstrcatW(out, g_procs[i].name);
    }
    if (shown < total) {
        wsprintfW(cnt, L" (+%d more)", total - shown);
        lstrcatW(out, cnt);
    }
    {
        wchar_t tail[48];
        wsprintfW(tail, L" (%d)", total);
        lstrcatW(out, tail);
    }
    wideToUtf8(out, g_result, sizeof(g_result));
}

/* ---------------------------------- kill -------------------------------------- */

static void doKill(const wchar_t* arg) {
    DWORD pids[MAX_HITS];
    wchar_t disp[MAX_PATH], q[MAX_PATH], msg[600], line[256];
    int i, n = 0, ok = 0, denied = 0, asPid = 1;
    DWORD pid = 0;

    lstrcpynW(q, arg, MAX_PATH);
    for (i = 0; q[i]; ++i) if (!iswdigit(q[i])) { asPid = 0; break; }
    if (asPid && q[0]) pid = (DWORD)wcstoul(q, NULL, 10);

    disp[0] = 0;
    for (i = 0; i < g_nprocs && n < MAX_HITS; ++i) {
        int hit = asPid ? (g_procs[i].pid == pid) : nameMatch(g_procs[i].name, q);
        if (hit) {
            if (n == 0) lstrcpynW(disp, g_procs[i].name, MAX_PATH);
            pids[n++] = g_procs[i].pid;
        }
    }

    if (n == 0) {
        wsprintfW(line, L"kill: no process named %s", q);
        wideToUtf8(line, g_result, sizeof(g_result));
        return;
    }

    if (g_confirm) {
        wsprintfW(msg, L"Kill %d process(es): %s \x2014 are you sure?", n, disp);
        if (MessageBoxW(NULL, msg, L"TypeBox", MB_YESNO | MB_ICONWARNING) != IDYES) {
            wideToUtf8(L"kill: cancelled", g_result, sizeof(g_result));
            return;
        }
    }
    for (i = 0; i < n; ++i) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pids[i]);
        if (!h) { ++denied; continue; }
        if (TerminateProcess(h, 0)) ++ok;
        else ++denied;
        CloseHandle(h);
    }
    if (ok > 0) {
        wsprintfW(line, L"kill: %s \x2014 terminated %d process(es)", disp, ok);
        if (denied > 0) {
            wchar_t extra[80];
            wsprintfW(extra, L", access denied for %d process(es)", denied);
            lstrcatW(line, extra);
        }
    } else if (denied > 0) {
        wsprintfW(line, L"kill: access denied for %d process(es)", denied);
    } else {
        wsprintfW(line, L"kill: no process named %s", q);
    }
    wideToUtf8(line, g_result, sizeof(g_result));
}

/* ----------------------------------- ABI ------------------------------------ */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
    refreshCache();                       /* prime the ET cache once */
}

PLUGIN_API void __cdecl Unload(void) {}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Task Manager Lite";
    i.version = "1.0";
    i.description = "kill <proc> / task <filter> - list processes and kill by name";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 2; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[2] = { "kill", "task" };
    return (n >= 0 && n < 2) ? k[n] : NULL;
}

/* ET rows: cache-only (refresh if >2000 ms old), unique image names, newest
 * first (reverse snapshot order), rows always start with the command word. */
static char g_etRows[9][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 9) lstrcpynA(g_etRows[g_etCount++], r, 256); }

static void etFill(const wchar_t* verb, const wchar_t* argFilterLower) {
    wchar_t seen[MAX_HITS][MAX_PATH];
    int seenN = 0, i, j, dup, added = 0;
    wchar_t row[256];
    char arow[256];
    for (i = g_nprocs - 1; i >= 0 && added < 8; --i) {
        dup = 0;
        for (j = 0; j < seenN; ++j) if (_wcsicmp(seen[j], g_procs[i].name) == 0) { dup = 1; break; }
        if (dup) continue;
        if (argFilterLower && *argFilterLower && !matches(g_procs[i].name, argFilterLower)) continue;
        if (seenN < MAX_HITS) { lstrcpynW(seen[seenN], g_procs[i].name, MAX_PATH); ++seenN; }
        wsprintfW(row, L"%s %s", verb, g_procs[i].name);
        wideToUtf8(row, arow, sizeof(arow));
        etAdd(arow);
        ++added;
    }
}

PLUGIN_API int __cdecl GetCountET(const char* input) {
    wchar_t arg[MAX_PATH];
    g_etCount = 0;
    if (!input) return 0;
    cacheIfStale();                                   /* never walk toolhelp per keystroke */
    if (!input[0]) {                                  /* gallery examples */
        etAdd("kill notepad.exe");
        etAdd("task chrome.exe");
        return g_etCount;
    }
    if (!_strnicmp(input, "kill ", 5) || !_strnicmp(input, "task ", 5)) {
        const wchar_t* verb = (input[0] == 'k' || input[0] == 'K') ? L"kill" : L"task";
        MultiByteToWideChar(CP_UTF8, 0, input + 5, -1, arg, MAX_PATH);
        lowerW(arg);
        etFill(verb, arg);
        return g_etCount;
    }
    if (_strnicmp(input, "kill", (int)lstrlenA(input)) == 0) { etFill(L"kill", NULL); return g_etCount; }
    if (_strnicmp(input, "task", (int)lstrlenA(input)) == 0) { etFill(L"task", NULL); return g_etCount; }
    return 0;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    const char* p;
    wchar_t argW[MAX_PATH];
    char a[512];
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    if (_strnicmp(str, "kill", 4) != 0 && _strnicmp(str, "task", 4) != 0) return ex;
    p = str + 4;
    while (*p == ' ') ++p;
    if (!*p) return ex;                                /* bare "kill"/"task": not ours */
    lstrcpynA(a, p, sizeof(a));
    {   /* trim trailing spaces */
        size_t n = strlen(a);
        while (n && isspace((unsigned char)a[n - 1])) a[--n] = 0;
    }
    if (!a[0]) return ex;
    MultiByteToWideChar(CP_UTF8, 0, a, -1, argW, MAX_PATH);
    refreshCache();                                    /* fresh view at run time */
    g_result[0] = 0;
    if (_strnicmp(str, "kill", 4) == 0) doKill(argW);
    else                                doTask(argW);
    if (!g_result[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
