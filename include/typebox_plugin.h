/*
 * typebox_plugin.h - TypeBox plugin SDK, ABI v1 (v0.36.0)
 *
 * The one header a TypeBox plugin needs. Drop it next to your .c file:
 *
 *     #include "typebox_plugin.h"
 *
 *     static const char* k[1] = { "hello" };
 *     __declspec(dllexport) int __cdecl GetCount(void) { return 1; }
 *     ...
 *
 * ABI law (frozen until TypeBox 2.0 - see docs/VERSIONING.md):
 *   - Every export is __cdecl and uses ANSI (CP_ACP) strings. This keeps the
 *     2001-2004 TypeAndRun plugin sources loading unchanged.
 *   - Mandatory: RunString(const char* line) -> TExec. Recommended:
 *     GetInfo, GetCount/GetString (command words), GetCountET/GetStringET
 *     (drop-down templates). Optional: Load/Unload, GetPluginMenu, About,
 *     Options, FastConfig, HookKey.
 *   - TInfo/TExec .size MUST be sizeof(the struct) - the host rejects a
 *     mismatched size and treats the plugin as foreign.
 *   - Returned const char* must stay valid until your next call for the same
 *     query (static buffers are fine; the host copies immediately).
 *   - A plugin that faults is skipped for the rest of the session - never
 *     crash through, guard your own parsing.
 *
 * Declare your command words in Plugins\<name>.tbplugin.json (see
 * docs/PLUGIN_API.md "Manifest"); the host refuses a plugin whose declared
 * words collide with the host or with another plugin.
 *
 * Licensed under GPL-3.0 (same as TypeBox). This header is deliberately
 * dependency-free beyond <windows.h> and compiles under MinGW-w64 and MSVC.
 */
#ifndef TYPEBOX_PLUGIN_H
#define TYPEBOX_PLUGIN_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- ABI structures (layout is the contract) ---------------- */

typedef struct TB_TInfo {
    int size;              /* MUST be sizeof(TInfo) */
    const char* name;
    const char* version;
    const char* description;
    const char* author;
    const char* copyright;
    const char* homepage;
} TInfo;

typedef struct TB_TExec {
    int size;              /* MUST be sizeof(TExec) */
    int run;               /* 0 = I did not handle this line; 1 = handled */
    const char* con_text;  /* text to place in the console (NULL = keep) */
    int con_sel_start;     /* selection start in con_text */
    int con_sel_length;    /* selection length (<0 = caret at end) */
} TExec;

/* ---------------- export signatures ---------------- */

typedef void        (__cdecl* PLoad)(HWND mainWnd);
typedef void        (__cdecl* PUnload)(void);
typedef TInfo       (__cdecl* PGetInfo)(void);
typedef int         (__cdecl* PGetCount)(void);
typedef const char* (__cdecl* PGetString)(int n);
typedef int         (__cdecl* PGetCountET)(const char* line);
typedef const char* (__cdecl* PGetStringET)(int i);
typedef TExec       (__cdecl* PRunString)(const char* line);
typedef HMENU       (__cdecl* PGetPluginMenu)(void);
typedef void        (__cdecl* PAbout)(void);
typedef void        (__cdecl* POptions)(void);
typedef void        (__cdecl* PFastConfig)(void);
typedef int         (__cdecl* PHookKey)(HWND mainWnd, WPARAM vk);

/* ---------------- authoring helpers ---------------- */

/* Mark a function as a plugin export (MSVC needs the attribute; MinGW
 * exports plain cdecl too, the macro is harmless there). Define your build
 * symbol first, e.g. `#define SKELETON_BUILDING_DLL` like the examples do. */
#ifndef TB_EXPORT
#  ifdef TB_BUILDING_DLL
#    define TB_EXPORT __declspec(dllexport)
#  else
#    define TB_EXPORT
#  endif
#endif

/* Build the "not my line" answer every non-matching RunString must return. */
static TExec TbNoRun(void) {
    TExec e; e.size = (int)sizeof(TExec); e.run = 0;
    e.con_text = 0; e.con_sel_start = 0; e.con_sel_length = 0;
    return e;
}

/* Answer a line: put `text` in the console, caret at `selStart` selecting
 * `selLen` bytes (-1 = just place the caret). */
static TExec TbAnswer(const char* text, int selStart, int selLen) {
    TExec e; e.size = (int)sizeof(TExec); e.run = 1;
    e.con_text = text; e.con_sel_start = selStart; e.con_sel_length = selLen;
    return e;
}

/* Fill a TInfo from literal strings. */
static TInfo TbInfo(const char* name, const char* version, const char* desc,
                    const char* author, const char* copyright,
                    const char* homepage) {
    TInfo i; i.size = (int)sizeof(TInfo);
    i.name = name; i.version = version; i.description = desc;
    i.author = author; i.copyright = copyright; i.homepage = homepage;
    return i;
}

/* Your own DLL directory (…\TypeBox\Plugins\). The host pre-creates
 * …\TypeBox\Data\<dllname>\ for your writable state - reach it as
 * TbPluginDir() -> "…\Data\<dllname>" (caller passes its own module, i.e.
 * the DllMain hinstDLL saved in a static, or (HMODULE)&__ImageBase with
 * MinGW). Returns 0 on failure. */
static int TbPluginDirAt(HMODULE self, wchar_t* out, int outChars) {
    wchar_t path[MAX_PATH];
    wchar_t base[64];
    wchar_t* slash;
    wchar_t* dot;
    if (!self || !out || outChars <= 0) return 0;
    if (!GetModuleFileNameW(self, path, MAX_PATH)) return 0;
    slash = wcsrchr(path, L'\\');
    if (!slash) return 0;
    *slash = 0;                       /* …\Plugins           */
    lstrcpynW(base, slash + 1, 64);   /* <name>.dll          */
    dot = wcsrchr(base, L'.');
    if (dot) *dot = 0;                /* <name>              */
    slash = wcsrchr(path, L'\\');     /* up from Plugins     */
    if (!slash) return 0;
    *slash = 0;                       /* …\TypeBox           */
    if (wsprintfW(out, L"%s\\Data\\%s", path, base) >= outChars) return 0;
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* TYPEBOX_PLUGIN_H */
