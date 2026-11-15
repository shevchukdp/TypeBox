/*
 * TypeBox plugin: base64 / URL / hex codec.
 * Spec: docs/PLUGIN_GALLERY.md #2. Zero dependencies beyond user32/kernel32.
 *
 * Build (MinGW):  gcc -shared -O2 -o codec.dll codec.c
 * Build (MSVC):   cl /LD /DCODEC_BUILDING_DLL /Fe:codec.dll codec.c user32.lib
 * Licensed under GPL-3.0.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef CODEC_BUILDING_DLL
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

#define MAXIN 4096

static HWND g_mainWnd = NULL;
static char g_result[MAXIN * 2];
static int  g_copyResult = 1, g_popup = 1;

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
    g_copyResult = (int)GetPrivateProfileIntW(L"Codec", L"CopyResult", 1, ini);
    g_popup      = (int)GetPrivateProfileIntW(L"Codec", L"Popup", 1, ini);
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

static void report(const char* result, const char* clipValue) {
    lstrcpynA(g_result, result, sizeof(g_result));
    if (clipValue && clipValue[0]) copyClip(clipValue);
    if (g_popup && g_result[0]) {
        wchar_t w[MAXIN * 2], t[16];
        MultiByteToWideChar(CP_UTF8, 0, g_result, -1, w, MAXIN * 2);
        MultiByteToWideChar(CP_UTF8, 0, "codec", -1, t, 16);
        MessageBoxW(g_mainWnd, w, t, MB_OK);
    }
}

static void trim(char* s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    char* p = s; while (*p && isspace((unsigned char)*p)) ++p;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

/* --------------------------------- base64 ---------------------------------- */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static void b64enc(const char* in) {
    char out[MAXIN * 2];
    size_t n = strlen(in), o = 0, i = 0;
    while (i + 2 < n) {
        unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i+1] << 8) | (unsigned char)in[i+2];
        out[o++] = B64[(v >> 18) & 63]; out[o++] = B64[(v >> 12) & 63];
        out[o++] = B64[(v >> 6) & 63];  out[o++] = B64[v & 63];
        i += 3;
    }
    if (i < n) {
        unsigned v = (unsigned char)in[i] << 16;
        if (i + 1 < n) v |= (unsigned char)in[i+1] << 8;
        out[o++] = B64[(v >> 18) & 63];
        out[o++] = B64[(v >> 12) & 63];
        out[o++] = (i + 1 < n) ? B64[(v >> 6) & 63] : '=';
        out[o++] = '=';
    }
    out[o] = 0;
    report(out, out);
}

static void b64dec(const char* in) {
    char out[MAXIN * 2];
    size_t o = 0, i = 0, n = strlen(in);
    int acc = 0, bits = 0;
    for (; i < n; ++i) {
        char c = in[i];
        if (c == '=' || c == '\n' || c == '\r') break;
        int v = b64val(c);
        if (v < 0) { report("b64dec: invalid character", ""); return; }
        acc = (acc << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (char)((acc >> bits) & 0xFF); }
    }
    out[o] = 0;
    report(out, out);
}

/* ----------------------------------- URL ----------------------------------- */

static void urlenc(const char* in) {
    static const char H[] = "0123456789ABCDEF";
    char out[MAXIN * 3];
    size_t o = 0;
    for (const unsigned char* p = (const unsigned char*)in; *p; ++p) {
        int c = *p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[o++] = (char)c;
        else { out[o++] = '%'; out[o++] = H[c >> 4]; out[o++] = H[c & 15]; }
    }
    out[o] = 0;
    report(out, out);
}

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void urldec(const char* in) {
    char out[MAXIN * 2];
    size_t o = 0;
    for (const char* p = in; *p; ++p) {
        if (*p == '+') out[o++] = ' ';
        else if (*p == '%' && p[1] && p[2]) {
            int hi = hexv(p[1]), lo = hexv(p[2]);
            if (hi < 0 || lo < 0) { report("urldec: bad escape", ""); return; }
            out[o++] = (char)((hi << 4) | lo); p += 2;
        } else out[o++] = *p;
    }
    out[o] = 0;
    report(out, out);
}

/* ----------------------------------- hex ----------------------------------- */

static void hexenc(const char* in) {
    static const char H[] = "0123456789abcdef";
    char out[MAXIN * 2];
    size_t o = 0;
    for (const unsigned char* p = (const unsigned char*)in; *p; ++p) {
        out[o++] = H[*p >> 4]; out[o++] = H[*p & 15];
    }
    out[o] = 0;
    report(out, out);
}

static void hexdec(const char* in) {
    char buf[MAXIN * 2];
    lstrcpynA(buf, in, sizeof(buf));
    trim(buf);
    size_t n = strlen(buf);
    if (n % 2) { report("hexdec: odd number of digits", ""); return; }
    char out[MAXIN];
    for (size_t i = 0; i < n; i += 2) {
        int hi = hexv(buf[i]), lo = hexv(buf[i+1]);
        if (hi < 0 || lo < 0) { report("hexdec: not a hex digit", ""); return; }
        out[i / 2] = (char)((hi << 4) | lo);
    }
    out[n / 2] = 0;
    report(out, out);
}

/* ----------------------------------- ABI ----------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) { g_mainWnd = mainWnd; loadConfig(); }
PLUGIN_API void __cdecl Unload(void) {}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Codec";
    i.version = "1.0";
    i.description = "b64enc/b64dec urlenc/urldec hexenc/hexdec - paste, encode, decode";
    i.author = "TypeBox project";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 6; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[6] = { "b64enc", "b64dec", "urlenc", "urldec", "hexenc", "hexdec" };
    return (n >= 0 && n < 6) ? k[n] : NULL;
}

static const char* g_etRows[8];
static int g_etCount;
static void etAdd(const char* r) { if (g_etCount < 8) g_etRows[g_etCount++] = r; }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    g_etCount = 0;
    if (!input) return 0;
    if (!_strnicmp(input, "b64enc", 6)) etAdd("b64enc hello");
    else if (!_strnicmp(input, "b64dec", 6)) etAdd("b64dec aGVsbG8=");
    else if (!_strnicmp(input, "b64", 3)) { etAdd("b64enc hello"); etAdd("b64dec aGVsbG8="); }
    else if (!_strnicmp(input, "urlenc", 6)) etAdd("urlenc hello world?");
    else if (!_strnicmp(input, "urldec", 6)) etAdd("urldec hello%20world%3F");
    else if (!_strnicmp(input, "url", 3)) { etAdd("urlenc hello world?"); etAdd("urldec hello%20world%3F"); }
    else if (!_strnicmp(input, "hexenc", 6)) etAdd("hexenc hi");
    else if (!_strnicmp(input, "hexdec", 6)) etAdd("hexdec 6869");
    else if (!_strnicmp(input, "hex", 3)) { etAdd("hexenc hi"); etAdd("hexdec 6869"); }
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

static int argAfter(const char* str, size_t kw, const char** arg) {
    const char* p = str + kw;
    while (*p == ' ') ++p;
    *arg = p;
    return kw;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    const char* arg;
    g_result[0] = 0;
    if (!_strnicmp(str, "b64enc", 6)) { argAfter(str, 6, &arg); if (*arg) b64enc(arg); }
    else if (!_strnicmp(str, "b64dec", 6)) { argAfter(str, 6, &arg); if (*arg) b64dec(arg); }
    else if (!_strnicmp(str, "urlenc", 6)) { argAfter(str, 6, &arg); if (*arg) urlenc(arg); }
    else if (!_strnicmp(str, "urldec", 6)) { argAfter(str, 6, &arg); if (*arg) urldec(arg); }
    else if (!_strnicmp(str, "hexenc", 6)) { argAfter(str, 6, &arg); if (*arg) hexenc(arg); }
    else if (!_strnicmp(str, "hexdec", 6)) { argAfter(str, 6, &arg); if (*arg) hexdec(arg); }
    else return ex;
    if (!g_result[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
