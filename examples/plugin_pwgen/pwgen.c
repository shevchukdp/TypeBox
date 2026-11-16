/*
 * TypeBox plugin: password generator — "pw [len] [count] [flags]".
 * Spec: docs/PLUGIN_GALLERY.md #9. Console-first twist on the spec: the
 * passwords answer INSIDE the console (con_text), the clipboard gets the
 * first one, and the old-school popup is opt-in ([Pwgen] Popup=1).
 *
 * Build (MinGW):  gcc -shared -O2 -DPWGEN_BUILDING_DLL pwgen.c -o pwgen.dll -ladvapi32 -luser32
 * Build (MSVC):   cl /LD /DPWGEN_BUILDING_DLL /Fe:pwgen.dll pwgen.c advapi32.lib user32.lib
 * Licensed under GPL-3.0.
 *
 * Usage:
 *   pw              -> [Pwgen] Len long password, letters+digits
 *   pw 16           -> one 16-char password
 *   pw 16 4         -> four 16-char passwords (first one lands on the clipboard)
 *   pw 20 s         -> + symbols      ([Pwgen] Symbols alphabet)
 *   pw 12 n         -> digits only
 *   pw 10 p         -> pronounceable-ish CV syllables
 *
 * RNG: CryptAcquireContext(CRYPT_VERIFYCONTEXT) + CryptGenRandom, rejection
 * sampling (no modulo bias). If the RNG fails the clipboard stays untouched —
 * a weak fallback password is NEVER emitted silently.
 *
 * Config (TypeBox.ini next to the host exe, DLL lives in Plugins\):
 *   [Pwgen]
 *     Len      default length, clamped 4..128          (default 16)
 *     Symbols  symbol alphabet for the s flag          (default !@#$%^&*()-_=+[]{}?)
 *     Popup    1 = also show a MessageBox with the result (default 0; console-first)
 */
#include <windows.h>
#include <wincrypt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>

#ifdef PWGEN_BUILDING_DLL
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

#define PW_MIN    4
#define PW_MAX    128
#define PW_MAXCNT 10

static HWND       g_mainWnd = NULL;
static HCRYPTPROV g_prov    = 0;             /* CRYPT_VERIFYCONTEXT, kept open */

static int        g_defLen = 16;
static int        g_popup  = 0;
static wchar_t    g_symbols[64] = L"!@#$%^&*()-_=+[]{}?";

static char       g_result[4096];

static const wchar_t kLower[]  = L"abcdefghijkmnopqrstuvwxyz";      /* no l   */
static const wchar_t kUpper[]  = L"ABCDEFGHJKLMNPQRSTUVWXYZ";      /* no I,O */
static const wchar_t kDigits[] = L"23456789";                       /* no 0,1 */
static const wchar_t kCons[]   = L"bdfglmnrstvkz";
static const wchar_t kVow[]    = L"aeiou";

/* --------------------------------- config ---------------------------------- */
/* units.c style: locate our own DLL, step out of Plugins\, read the host ini. */
static void loadConfig(void) {
    wchar_t mod[MAX_PATH] = {}, ini[MAX_PATH] = {};
    HMODULE self = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&loadConfig, &self) &&
        GetModuleFileNameW(self, mod, MAX_PATH)) {
        lstrcpyW(ini, mod);
        wchar_t* slash = wcsrchr(ini, L'\\');
        if (slash) { *slash = 0;                      /* drop pwgen.dll       */
            lstrcatW(ini, L"\\TypeBox.ini");
            if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
                *wcsrchr(ini, L'\\') = 0;             /* drop TypeBox.ini     */
                *wcsrchr(ini, L'\\') = 0;             /* drop Plugins\        */
                lstrcatW(ini, L"\\TypeBox.ini");
            }
        } else lstrcpyW(ini, L"TypeBox.ini");
    } else lstrcpyW(ini, L"TypeBox.ini");
    g_defLen = (int)GetPrivateProfileIntW(L"Pwgen", L"Len", 16, ini);
    g_popup  = (int)GetPrivateProfileIntW(L"Pwgen", L"Popup", 0, ini);
    if (g_defLen < PW_MIN) g_defLen = PW_MIN;
    if (g_defLen > PW_MAX) g_defLen = PW_MAX;
    {   /* optional custom symbol alphabet; keep the built-in one when empty */
        wchar_t buf[64] = {};
        GetPrivateProfileStringW(L"Pwgen", L"Symbols", L"", buf, 64, ini);
        if (buf[0]) lstrcpynW(g_symbols, buf, 64);
    }
}

/* ---------------------------------- RNG -------------------------------------- */

static int randBytes(BYTE* out, DWORD n) {
    if (!g_prov) {
        if (!CryptAcquireContextW(&g_prov, NULL, NULL, PROV_RSA_FULL,
                                  CRYPT_VERIFYCONTEXT)) {
            g_prov = 0;
            return 0;
        }
    }
    return CryptGenRandom(g_prov, n, out) != 0;
}

/* uniform index in [0,range) via rejection sampling (gallery #9 requirement) */
static int randBelow(int range) {
    BYTE b;
    int accept;
    if (range <= 0) return 0;
    if (range > 256) range = 256;
    accept = (256 / range) * range;
    do {
        if (!randBytes(&b, 1)) return -1;
    } while ((int)b >= accept);
    return (int)(b % range);
}

static wchar_t pick(const wchar_t* alpha) {
    int n = (int)lstrlenW(alpha);
    int r = randBelow(n);
    return (r >= 0) ? alpha[r] : L'?';
}

/* one password into out[outc]. flags: n=digits only, s=symbols, p=pronounceable */
static void makePw(int len, const char* flags, wchar_t* out, int outc) {
    int digitsOnly = strchr(flags, 'n') != NULL;
    int symbols    = strchr(flags, 's') != NULL;
    int pro        = strchr(flags, 'p') != NULL;
    int i, o, pool;
    wchar_t* poolBuf;

    if (len < PW_MIN) len = PW_MIN;
    if (len > PW_MAX) len = PW_MAX;
    if (len > outc - 1) len = outc - 1;

    if (pro) {                                  /* CV(C) syllables           */
        o = 0;
        while (o < len) {
            if (digitsOnly) out[o++] = pick(kDigits);
            else {
                out[o++] = pick(kCons);
                if (o < len) out[o++] = pick(kVow);
                if (o < len && randBelow(2) == 1) out[o++] = pick(kCons);
            }
        }
        out[o] = 0;
        return;
    }

    poolBuf = (wchar_t*)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!poolBuf) { out[0] = 0; return; }
    if (digitsOnly) {
        for (i = 0; i < len; ++i) poolBuf[i] = pick(kDigits);
    } else {
        int nl = (int)lstrlenW(kLower), nu = (int)lstrlenW(kUpper);
        int nd = (int)lstrlenW(kDigits);
        pool = nl + nu + nd + (symbols ? (int)lstrlenW(g_symbols) : 0);
        poolBuf[0] = pick(kLower);                        /* guaranteed class coverage */
        poolBuf[1] = pick(kUpper);
        poolBuf[2] = pick(kDigits);
        if (symbols) poolBuf[3] = pick(g_symbols);
        for (i = symbols ? 4 : 3; i < len; ++i) {
            int r = randBelow(pool);
            if (r < 0) r = 0;
            if (r < nl)                     poolBuf[i] = pick(kLower);
            else if (r < nl + nu)            poolBuf[i] = pick(kUpper);
            else if (r < nl + nu + nd)       poolBuf[i] = pick(kDigits);
            else                             poolBuf[i] = pick(g_symbols);
        }
    }
    /* Fisher-Yates shuffle: the guaranteed chars are not always in front */
    for (i = len - 1; i > 0; --i) {
        int j = randBelow(i + 1);
        wchar_t t;
        if (j < 0) j = 0;
        t = poolBuf[i]; poolBuf[i] = poolBuf[j]; poolBuf[j] = t;
    }
    memcpy(out, poolBuf, (size_t)len * sizeof(wchar_t));
    out[len] = 0;
    free(poolBuf);
}

/* ------------------------------- clipboard ----------------------------------- */

static int putOnClipboard(const wchar_t* w) {
    int ok = 0;
    if (!OpenClipboard(NULL)) return 0;
    EmptyClipboard();
    {
        size_t bytes = (lstrlenW(w) + 1) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (h) {
            wchar_t* p = (wchar_t*)GlobalLock(h);
            if (p) { lstrcpyW(p, w); GlobalUnlock(h);
                     if (SetClipboardData(CF_UNICODETEXT, h)) ok = 1;  /* clipboard owns h */
                     else GlobalFree(h); }
            else GlobalFree(h);
        }
    }
    CloseClipboard();
    return ok;
}

static int wideToUtf8(const wchar_t* w, char* a, int ac) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, a, ac, NULL, NULL);
    return (n > 0) ? n - 1 : 0;
}

/* --------------------------------- commands ---------------------------------- */

/* arg = everything after "pw". Grammar: [len] [count] [flags] in any order —
 * a pure number is len first then count, letters are flags (ns / nsp / s...). */
static void runPw(const char* arg) {
    wchar_t first[PW_MAX + 2], all[PW_MAX * (PW_MAXCNT + 2)];
    wchar_t msg[512];
    char fl[16];
    const char* p = arg;
    int len = g_defLen, count = 1, iflag = 0;
    int i, rng, clipOk = 0;

    while (*p == ' ') ++p;
    fl[0] = 0;
    while (*p) {
        if (isdigit((unsigned char)*p)) {                    /* a number token */
            long v = strtol(p, (char**)&p, 10);
            if (iflag == 0) {
                len = (int)v;
                if (len < PW_MIN) {
                    wideToUtf8(L"pw: length 4..128", g_result, sizeof(g_result));
                    return;
                }
                if (len > PW_MAX) len = PW_MAX;              /* capped, warned below */
                iflag = 1;
            } else if (iflag == 1) {
                count = (int)v;
                if (count < 1) count = 1;
                if (count > PW_MAXCNT) count = PW_MAXCNT;
                iflag = 2;
            } else return;                                   /* third number: not ours */
        } else if (isalpha((unsigned char)*p)) {
            char c = (char)tolower((unsigned char)*p);
            size_t L = strlen(fl);
            if (L < 12 && !strchr(fl, c)) { fl[L] = c; fl[L + 1] = 0; }
            ++p;
        } else if (*p == ' ' || *p == '\t') ++p;
        else return;                                         /* junk: not ours */
    }
    {   /* flag letters: unknown ones are a usage error (gallery #9) */
        const char* f = fl;
        for (; *f; ++f)
            if (*f != 'n' && *f != 's' && *f != 'p') {
                wideToUtf8(L"pw: flags are n (digits) s (symbols) p (pronounceable)",
                           g_result, sizeof(g_result));
                return;
            }
    }

    /* one RNG probe up front: never emit weak output, never touch clipboard */
    {
        BYTE probe;
        rng = randBytes(&probe, 1);
    }
    if (!rng) {
        wideToUtf8(L"pw: RNG unavailable", g_result, sizeof(g_result));
        return;
    }

    all[0] = 0;
    first[0] = 0;
    for (i = 0; i < count; ++i) {
        wchar_t one[PW_MAX + 2];
        makePw(len, fl, one, PW_MAX + 2);
        if (!one[0]) { wideToUtf8(L"pw: RNG unavailable", g_result, sizeof(g_result)); return; }
        if (i == 0) lstrcpyW(first, one);
        if (all[0]) lstrcatW(all, L"  ");
        lstrcatW(all, one);
        if (lstrlenW(all) > 3500) break;                     /* console sanity cap */
    }
    clipOk = putOnClipboard(first);

    {   /* console line: the passwords + where the first one went */
        wchar_t tail[96];
        if (count == 1)
            lstrcpynW(tail, clipOk ? L"  [copied]" : L"  [clipboard busy]", 96);
        else
            wsprintfW(tail, L"   [%d generated, first %s]", count,
                      clipOk ? L"copied" : L"clipboard busy");
        lstrcatW(all, tail);
    }
    wideToUtf8(all, g_result, sizeof(g_result));

    if (g_popup) {
        MultiByteToWideChar(CP_UTF8, 0, g_result, -1, msg, (int)(sizeof(msg) / sizeof(msg[0])) - 2);
        MessageBoxW(g_mainWnd, msg, L"TypeBox \x2014 passwords", MB_OK | MB_ICONINFORMATION);
        g_result[0] = 0;                                     /* popup mode: console clean */
    }
}

/* ----------------------------------- ABI ------------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    g_prov = 0;
    loadConfig();
}

PLUGIN_API void __cdecl Unload(void) {
    if (g_prov) { CryptReleaseContext(g_prov, 0); g_prov = 0; }
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Password Generator";
    i.version = "1.0";
    i.description = "pw 16 - crypto-random password to clipboard (n=digits s=symbols p=pronounceable)";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "pw" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

/* ET rows: templates only, never a real password (gallery #9). */
static char g_etRows[5][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 5) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    g_etCount = 0;
    if (!input) return 0;
    if (lstrlenA(input) <= 2) {
        if (_strnicmp(input, "pw", lstrlenA(input)) != 0) return 0;
    } else {
        if (_strnicmp(input, "pw ", 3) != 0) return 0;
    }
    etAdd("pw 16");
    etAdd("pw 12");
    etAdd("pw 8");
    etAdd("pw 24");
    etAdd("pw 16 4 s");
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
    if (_strnicmp(str, "pw", 2) != 0) return ex;
    if (str[2] && str[2] != ' ') return ex;                  /* "pwd" etc. is not ours */
    g_result[0] = 0;
    runPw(str + 2);
    if (!g_result[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
