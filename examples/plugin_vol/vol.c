/*
 * TypeBox plugin: volume control via simulated media keys.
 * Spec: docs/PLUGIN_GALLERY.md #3. user32 only (SendInput) - no Core Audio.
 *
 * Build (MinGW):  gcc -shared -O2 -o vol.dll vol.c -luser32
 * Build (MSVC):   cl /LD /DVOL_BUILDING_DLL /Fe:vol.dll vol.c user32.lib
 * Licensed under GPL-3.0.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef VOL_BUILDING_DLL
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
static char g_result[128];
static int  g_step = 1, g_beeps = 0;

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
            slash = wcsrchr(ini, L'\\');
            if (slash) *slash = 0;
            lstrcatW(ini, L"\\TypeBox.ini");
        }
    } else lstrcpyW(ini, L"TypeBox.ini");
    g_step  = (int)GetPrivateProfileIntW(L"Vol", L"Step", 1, ini);
    g_beeps = (int)GetPrivateProfileIntW(L"Vol", L"Beeps", 0, ini);
    if (g_step < 1) g_step = 1;
    if (g_step > 5) g_step = 5;
}

/* One media-key tap: key-down + key-up through SendInput. */
static int press(BYTE vk, int times) {
    INPUT in[2];
    int i;
    for (i = 0; i < times; ++i) {
        memset(in, 0, sizeof(in));
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wVk = vk;
        in[1].type = INPUT_KEYBOARD;
        in[1].ki.wVk = vk;
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        if (SendInput(2, in, sizeof(INPUT)) != 2) return 0;
        Sleep(15);
    }
    return 1;
}

static void say(const char* s) { lstrcpynA(g_result, s, sizeof(g_result)); }

static void doVol(const char* arg) {
    while (*arg == ' ') ++arg;
    if (!*arg)            { say("vol: use + - mute or 0..10"); return; }
    if (*arg == '+')      { if (press(VK_VOLUME_UP, g_step)) { if (g_beeps) MessageBeep(MB_OK); say("vol +"); } else say("vol: input blocked"); return; }
    if (*arg == '-')      { if (press(VK_VOLUME_DOWN, g_step)) { if (g_beeps) MessageBeep(MB_OK); say("vol -"); } else say("vol: input blocked"); return; }
    if (_stricmp(arg, "mute") == 0 || _stricmp(arg, "m") == 0) {
        if (press(VK_VOLUME_MUTE, 1)) say("vol mute"); else say("vol: input blocked");
        return;
    }
    if (isdigit((unsigned char)*arg)) {
        int n = atoi(arg);
        if (n < 0 || n > 10) { say("vol: level is 0..10"); return; }
        int presses = n * 5;
        if (presses == 0) presses = 1;               /* "vol 0" = tap down once */
        /* step down from wherever we are; media keys are relative per design */
        if (press(VK_VOLUME_DOWN, presses)) { if (g_beeps) MessageBeep(MB_OK); say("vol set ~"); }
        else { say("vol: input blocked"); return; }
        wsprintfA(g_result, "vol %d (~%d%%)", n, n * 10);
        return;
    }
    say("vol: use + - mute or 0..10");
}

PLUGIN_API void __cdecl Load(HWND mainWnd) { g_mainWnd = mainWnd; loadConfig(); }
PLUGIN_API void __cdecl Unload(void) {}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Volume";
    i.version = "1.0";
    i.description = "vol +/-/mute or vol 1..10 - media-key volume stepping";
    i.author = "TypeBox project";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }
PLUGIN_API const char* __cdecl GetString(int n) { return n == 0 ? "vol" : NULL; }

static const char* g_etRows[13];
static int g_etCount;
static void etAdd(const char* r) { if (g_etCount < 13) g_etRows[g_etCount++] = r; }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    g_etCount = 0;
    if (!input || _strnicmp(input, "vo", 2)) return 0;
    etAdd("vol +"); etAdd("vol -"); etAdd("vol mute");
    etAdd("vol 0"); etAdd("vol 1"); etAdd("vol 2"); etAdd("vol 3");
    etAdd("vol 4"); etAdd("vol 5"); etAdd("vol 6"); etAdd("vol 7");
    etAdd("vol 8"); etAdd("vol 10");
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str || _strnicmp(str, "vol", 3)) return ex;
    g_result[0] = 0;
    doVol(str + 3);
    if (!g_result[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
