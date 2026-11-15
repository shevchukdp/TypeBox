/*
 * TypeBox example plugin: calculator ("calc 2+3*4" -> shows the result).
 * Implements the TypeAndRun-compatible plugin ABI (see docs/PLUGIN_API.md),
 * so the same source also builds against the original TypeAndRun host.
 *
 * Build (MinGW):  gcc -shared -O2 -o calc.dll calc.c
 * Build (MSVC):   cl /LD /Fe:calc.dll calc.c
 *
 * Put calc.dll into the portable Plugins\ folder next to TypeBox.exe.
 * Licensed under GPL-3.0 (same as TypeBox / TypeAndRun).
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* MSVC needs explicit dllexport; MinGW also exports plain cdecl symbols, and
 * the host additionally tolerates "_Name" variants (32-bit cdecl). */
#ifdef CALC_BUILDING_DLL
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

/* ---------------- tiny recursive-descent expression parser ---------------- */

typedef struct { const char* s; int err; } Parser;

static double expr(Parser* p);

static void skip(Parser* p) { while (*p->s == ' ') ++p->s; }

static double number(Parser* p) {
    char* end = NULL;
    double v = strtod(p->s, &end);
    if (end == p->s) { p->err = 1; return 0; }
    p->s = end;
    return v;
}

static double factor(Parser* p) {
    skip(p);
    if (*p->s == '(') {
        ++p->s;
        double v = expr(p);
        skip(p);
        if (*p->s == ')') ++p->s; else p->err = 1;
        return v;
    }
    if (*p->s == '-') { ++p->s; return -factor(p); }
    if (*p->s == '+') { ++p->s; return  factor(p); }
    return number(p);
}

static double term(Parser* p) {
    double v = factor(p);
    for (;;) {
        skip(p);
        if (*p->s == '*') { ++p->s; v *= factor(p); }
        else if (*p->s == '/') { ++p->s; double d = factor(p); if (d == 0) { p->err = 1; return 0; } v /= d; }
        else if (*p->s == '%') { ++p->s; double d = factor(p); if (d == 0) { p->err = 1; return 0; } v = fmod(v, d); }
        else break;
    }
    return v;
}

static double expr(Parser* p) {
    double v = term(p);
    for (;;) {
        skip(p);
        if (*p->s == '+') { ++p->s; v += term(p); }
        else if (*p->s == '-') { ++p->s; v -= term(p); }
        else break;
    }
    return v;
}

/* ------------------------------- ABI exports ------------------------------ */

static char g_result[256];

PLUGIN_API void __cdecl Load(HWND mainWnd) { (void)mainWnd; }
PLUGIN_API void __cdecl Unload(void) {}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Calculator";
    i.version = "1.0";
    i.description = "calc <expression> - like the legacy tar_math plugin";
    i.author = "TypeBox project";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) { return n == 0 ? "calc" : NULL; }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    return (input && _strnicmp(input, "ca", 2) == 0) ? 1 : 0;
}

PLUGIN_API const char* __cdecl GetStringET(int n) { return n == 0 ? "calc " : NULL; }

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    ex.size = sizeof(TExec);
    ex.run = 0;
    ex.con_text = NULL;
    ex.con_sel_start = 0;
    ex.con_sel_length = 0;
    if (!str) return ex;
    if (_strnicmp(str, "calc ", 5) != 0) return ex;

    Parser p = { str + 5, 0 };
    double v = expr(&p);
    skip(&p);
    if (p.err || *p.s != '\0')
        lstrcpyA(g_result, "calc: syntax error");
    else
        wsprintfA(g_result, "%g", v);

    ex.run = 1;                  /* consume input, show result in the console */
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
