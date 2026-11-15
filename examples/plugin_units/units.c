/*
 * TypeBox plugin: units & base converter — "conv 10 km mi", "hex 255".
 * Spec: docs/PLUGIN_GALLERY.md #1. Zero dependencies beyond user32/kernel32.
 *
 * Build (MinGW):  gcc -shared -O2 -o units.dll units.c
 * Build (MSVC):   cl /LD /DUNITS_BUILDING_DLL /Fe:units.dll units.c user32.lib
 * Licensed under GPL-3.0.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef UNITS_BUILDING_DLL
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
static char g_result[512];
static int  g_copyResult = 1, g_popup = 1;

/* ------------------------------- config + clipboard ------------------------ */

static void loadConfig(void) {
    wchar_t mod[MAX_PATH] = {}, ini[MAX_PATH] = {};
    HMODULE self = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&loadConfig, &self) &&
        GetModuleFileNameW(self, mod, MAX_PATH)) {
        lstrcpyW(ini, mod);
        wchar_t* slash = wcsrchr(ini, L'\\');
        if (slash) { *slash = 0;                      /* drop units.dll  */
            slash = wcsrchr(ini, L'\\');
            if (slash) *slash = 0;                    /* drop Plugins\   */
            lstrcatW(ini, L"\\TypeBox.ini");
        }
    } else {
        lstrcpyW(ini, L"TypeBox.ini");
    }
    g_copyResult = (int)GetPrivateProfileIntW(L"Units", L"CopyResult", 1, ini);
    g_popup      = (int)GetPrivateProfileIntW(L"Units", L"Popup", 1, ini);
}

static void copyClip(const char* txt) {
    if (!g_copyResult || !OpenClipboard(NULL)) return;
    EmptyClipboard();
    size_t n = lstrlenA(txt) + 1;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
    if (h) {
        char* p = (char*)GlobalLock(h);
        if (p) { lstrcpyA(p, txt); GlobalUnlock(h); SetClipboardData(CF_TEXT, h); }
        else GlobalFree(h);
    }
    CloseClipboard();
}

/* report a result: console text + optional clipboard + optional MessageBox */
static void report(const char* result, const char* clipValue) {
    lstrcpynA(g_result, result, sizeof(g_result));
    if (clipValue && clipValue[0]) copyClip(clipValue);
    if (g_popup && g_result[0]) {
        wchar_t w[512], t[16];
        MultiByteToWideChar(CP_UTF8, 0, g_result, -1, w, 512);
        MultiByteToWideChar(CP_UTF8, 0, "units", -1, t, 16);
        MessageBoxW(g_mainWnd, w, t, MB_OK);
    }
}

/* --------------------------------- unit tables ----------------------------- */

struct U { const char* u; double to_base; };

static const struct U length[] = {
    {"m",1},{"km",1000},{"cm",0.01},{"mm",0.001},{"um",1e-6},
    {"mi",1609.344},{"yd",0.9144},{"ft",0.3048},{"in",0.0254},{"nmi",1852.0},{NULL,0} };
static const struct U mass[] = {
    {"kg",1},{"g",0.001},{"mg",1e-6},{"t",1000},
    {"lb",0.45359237},{"oz",0.028349523125},{"st",6.35029318},{NULL,0} };
static const struct U speed[] = {
    {"mps",1},{"kph",1.0/3.6},{"mph",0.44704},{"knt",0.514444},{"fts",0.3048},{NULL,0} };
static const struct U data[] = {
    {"b",1},{"kb",1024.0},{"mb",1048576.0},{"gb",1073741824.0},
    {"tb",1099511627776.0},{"pb",1125899906842624.0},{NULL,0} };
static const struct U timeu[] = {
    {"s",1},{"ms",0.001},{"min",60.0},{"h",3600.0},{"d",86400.0},{"wk",604800.0},{NULL,0} };

static double lookup(const struct U* t, const char* u, int* found) {
    for (; t->u; ++t) if (_stricmp(t->u, u) == 0) { *found = 1; return t->to_base; }
    *found = 0; return 0;
}

/* temperature is affine, not a plain factor */
static int tempToC(const char* u, double v, double* c) {
    if (_stricmp(u, "c") == 0) { *c = v; return 1; }
    if (_stricmp(u, "f") == 0) { *c = (v - 32.0) * 5.0 / 9.0; return 1; }
    if (_stricmp(u, "k") == 0) { *c = v - 273.15; return 1; }
    return 0;
}
static int tempFromC(const char* u, double c, double* v) {
    if (_stricmp(u, "c") == 0) { *v = c; return 1; }
    if (_stricmp(u, "f") == 0) { *v = c * 9.0 / 5.0 + 32.0; return 1; }
    if (_stricmp(u, "k") == 0) { *v = c + 273.15; return 1; }
    return 0;
}

static void doConv(const char* rest) {
    char val[64] = {}, from[32] = {}, to[32] = {};
    int n = sscanf(rest, "%63s %31s %31s", val, from, to);
    if (n < 3) { report("conv: usage conv <value> <from> <to>  e.g. conv 10 km mi", ""); return; }
    char* end = NULL;
    double v = strtod(val, &end);
    if (end == val || *end) { report("conv: not a number", ""); return; }

    double out;
    char line[256];
    double c;
    if (tempToC(from, v, &c) && tempFromC(to, c, &out)) {
        if (_stricmp(to, "k") == 0 && out < 0) { report("conv: below absolute zero", ""); return; }
    } else {
        const struct U* groups[] = { length, mass, speed, data, timeu };
        int i, ok = 0;
        double fb = 0, tb = 0, f1, t1;
        for (i = 0; i < 5 && !ok; ++i) {
            f1 = lookup(groups[i], from, &ok);
            if (ok) { t1 = lookup(groups[i], to, &ok); if (ok) { fb = f1; tb = t1; } }
        }
        if (!ok) { report("conv: unknown unit (try m,km,mi,ft,in,kg,lb,oz,mps,kph,mph,gb,tb,s,min,h)", ""); return; }
        out = v * fb / tb;
    }
    wsprintfA(line, "%s %s = ", val, from);
    snprintf(line + lstrlenA(line), sizeof(line) - lstrlenA(line), "%g %s", out, to);
    char num[64];
    snprintf(num, sizeof(num), "%g", out);
    report(line, num);
}

/* ---------------------------------- bases ---------------------------------- */

static void doBase(const char* kind, const char* rest) {
    while (*rest == ' ') ++rest;
    if (!*rest) { report("usage: hex 255 | bin 0xFF | dec 0xFF | oct 777", ""); return; }
    /* The keyword names the OUTPUT base; the input is decimal unless prefixed
     * (0x/0b) - "hex 255" must print FF, not re-parse 255 as hex. */
    int base_in = 10;
    const char* s = rest;
    if (s[0] == '0' && tolower(s[1]) == 'x') { base_in = 16; s += 2; }
    else if (s[0] == '0' && tolower(s[1]) == 'b') { base_in = 2; s += 2; }
    if (!*s) { report("not a number", ""); return; }
    char* end = NULL;
    unsigned long long v = strtoull(s, &end, base_in);
    if (end == s) { report("not a number", ""); return; }
    while (*end == ' ') ++end;
    if (*end) { report("not a number", ""); return; }
    char line[160], num[80];
    if (kind[0] == 'h') snprintf(num, sizeof(num), "%llX", v);
    else if (kind[0] == 'o') snprintf(num, sizeof(num), "%llo", v);
    else if (kind[0] == 'd') snprintf(num, sizeof(num), "%llu", v);
    else { /* binary */
        int i = 0; char b[65]; unsigned long long t = v;
        if (t == 0) b[i++] = '0';
        else { char r[65]; int j = 0; while (t) { r[j++] = (char)('0' + (t & 1)); t >>= 1; }
               while (j) b[i++] = r[--j]; }
        b[i] = 0; lstrcpyA(num, b);
    }
    snprintf(line, sizeof(line), "%s", num);
    report(line, num);
}

/* ----------------------------------- ABI ----------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) { g_mainWnd = mainWnd; loadConfig(); }
PLUGIN_API void __cdecl Unload(void) {}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Units";
    i.version = "1.0";
    i.description = "conv 10 km mi - units and hex/bin/dec/oct base converter";
    i.author = "TypeBox project";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 5; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[5] = { "conv", "hex", "bin", "dec", "oct" };
    return (n >= 0 && n < 5) ? k[n] : NULL;
}

static const char* g_etRows[8];
static int g_etCount;

static void etAdd(const char* r) { if (g_etCount < 8) g_etRows[g_etCount++] = r; }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    g_etCount = 0;
    if (!input) return 0;
    if (!_strnicmp(input, "conv", 4)) {
        etAdd("conv 10 km mi"); etAdd("conv 98.6 f c"); etAdd("conv 70 kg lb");
        etAdd("conv 100 kph mph"); etAdd("conv 100 gb mb");
    } else if (!_strnicmp(input, "hex", 3)) { etAdd("hex 255"); etAdd("hex 0xDEAD"); }
    else if (!_strnicmp(input, "bin", 3)) { etAdd("bin 255"); etAdd("bin 0xFF"); }
    else if (!_strnicmp(input, "dec", 3)) { etAdd("dec 0xFF"); }
    else if (!_strnicmp(input, "oct", 3)) { etAdd("oct 255"); }
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
    g_result[0] = 0;
    if (!_strnicmp(str, "conv", 4))      doConv(str + 4);
    else if (!_strnicmp(str, "hex", 3))  doBase("hex", str + 3);
    else if (!_strnicmp(str, "bin", 3))  doBase("bin", str + 3);
    else if (!_strnicmp(str, "dec", 3))  doBase("dec", str + 3);
    else if (!_strnicmp(str, "oct", 3))  doBase("oct", str + 3);
    else return ex;
    if (!g_result[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
