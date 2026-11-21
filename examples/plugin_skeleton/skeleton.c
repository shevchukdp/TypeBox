/*
 * skeleton.c - the reference TypeBox plugin (v0.36.0). Copy this folder,
 * rename, change the word, and you have a new plugin. Every part an agent
 * (or a human) needs is here and commented:
 *
 *   command word ..... k[] + GetCount/GetString          -> dd completion
 *   drop-down rows ... GetCountET/GetStringET             -> templates
 *   line handling .... RunString returns TExec            -> the answer
 *   identity ......... GetInfo (shown on Settings > Plugins)
 *   state ............ Data\<name>\ (host pre-creates it; TbPluginDirAt)
 *   declaration ...... <name>.tbplugin.json sidecar (see hello.tbplugin.json
 *                      next to this file - declare your words there!)
 *
 * Build (MinGW):  gcc -shared -O2 -I ..\..\include -o hello.dll skeleton.c
 * Build (MSVC):   cl /LD /I ..\..\include /Fe:hello.dll skeleton.c
 * or add it to CMakeLists like the other examples. Drop the dll into
 * the portable Plugins\ folder and press the plugin page RESCAN button.
 *
 * Rules that keep everyone happy (full text: docs/PLUGIN_API.md):
 *   - __cdecl exports, ANSI (CP_ACP) strings, size = sizeof(struct).
 *   - RunString sees EVERY typed line - return TbNoRun() unless it is yours.
 *   - Never touch the network / clipboard / SendInput unless the user typed
 *     the command. TypeBox is offline-first.
 *   - Never crash: the host skips a faulting plugin for the whole session.
 *
 * GPL-3.0, same as TypeBox.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TB_BUILDING_DLL             /* turns TB_EXPORT into __declspec(dllexport) */
#include "typebox_plugin.h"

#define TB_PLUGIN_NAME    "hello"
#define TB_PLUGIN_VERSION "1.0"

static HMODULE g_self = 0;          /* our DLL handle, captured in DllMain */
static LONG    g_answers = 0;       /* demo counter, persisted below */

/* The command words this plugin owns. The manifest (hello.tbplugin.json)
 * MUST declare the same list - the host refuses the plugin if a declared
 * word collides with the host or with another plugin. */
static const char* k[1] = { "hello" };

TB_EXPORT int __cdecl GetCount(void) { return 1; }

TB_EXPORT const char* __cdecl GetString(int n) {
    return (n >= 0 && n < GetCount()) ? k[n] : 0;
}

TB_EXPORT TInfo __cdecl GetInfo(void) {
    return TbInfo(TB_PLUGIN_NAME, TB_PLUGIN_VERSION,
                  "Say hello (reference plugin skeleton)",
                  "TypeBox examples", "GPL-3.0",
                  "https://github.com/shevchukdp/TypeBox");
}

/* ---------------- drop-down templates ----------------
 * The host passes the WHOLE typed line so you can key on "hello " subwords.
 * Return how many rows you offer, then the rows by index. Keep the strings
 * alive until the next call (static buffers are fine). */
static const char* g_etRows[3] = { "hello world", "hello typebox", "hello <name>" };

TB_EXPORT int __cdecl GetCountET(const char* line) {
    if (!line) return 0;
    if (_strnicmp(line, "hello", 5) != 0) return 0;
    if (line[5] && line[5] != ' ' && line[5] != '\t') return 0;
    return 3;
}

TB_EXPORT const char* __cdecl GetStringET(int i) {
    return (i >= 0 && i < 3) ? g_etRows[i] : 0;
}

/* ---------------- the line handler ---------------- */

static char g_answer[512];

TB_EXPORT TExec __cdecl RunString(const char* line) {
    const char* rest;
    if (!line) return TbNoRun();
    while (*line == ' ' || *line == '\t') ++line;
    if (_strnicmp(line, "hello", 5) != 0) return TbNoRun();
    rest = line + 5;
    while (*rest == ' ' || *rest == '\t') ++rest;

    /* Demo state: Data\hello\count.dat next to TypeBox.exe (host-created). */
    {
        wchar_t dir[MAX_PATH], file[MAX_PATH];
        g_answers++;
        if (TbPluginDirAt(g_self, dir, MAX_PATH)) {
            wsprintfW(file, L"%s\\count.dat", dir);
            FILE* f = _wfopen(file, L"a+");
            if (f) { char n[32] = ""; long v = 0;
                     if (fscanf(f, "%31s", n) == 1) v = atol(n);
                     if (v + 1 > g_answers) g_answers = (LONG)(v + 1);
                     rewind(f); fprintf(f, "%ld", (long)g_answers); fclose(f); }
        }
    }

    if (!*rest)
        wsprintfA(g_answer, "hello! try \"hello world\" or \"hello <name>\" (answered %d time%s)",
                  (int)g_answers, g_answers == 1 ? "" : "s");
    else
        wsprintfA(g_answer, "hello, %s! (answered %d times)", rest, (int)g_answers);

    /* Select just the greeting so the user can type over the name, bash-style. */
    return TbAnswer(g_answer, 0, (int)strlen(g_answer));
}

/* ---------------- lifetime ---------------- */

TB_EXPORT void __cdecl Load(HWND mainWnd)  { (void)mainWnd; }
TB_EXPORT void __cdecl Unload(void)        { }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, void* unused) {
    (void)unused;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}

/* Optional exports plugins usually skip - delete these five and nothing
 * breaks. GetPluginMenu/About/Options/FastConfig/HookKey are probed by the
 * host with GetProcAddress and simply stay unbound when absent. */
