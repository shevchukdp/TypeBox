/*
 * TypeBox plugin: developer one-liners - "ts" epochs/ISO, "slug" translit,
 * "sha256" checksum verify.
 * Spec: docs/IDEAS-RESEARCH-3.md #250 (timestamp converter), #251 (slugify
 * + translit) and #246 (sha256 verify); shipped as the eleventh bundled
 * plugin in v0.23.0, sha256 joins in v0.24.0. Console-first: every answer
 * lands in the console (con_text); the slug result and a bare sha256 digest
 * are additionally copied to the clipboard (5x retry, never a hard dependency).
 *
 * Build (MinGW):  gcc -shared -O2 -DUTIL_BUILDING_DLL util.c -o util.dll -luser32 -lbcrypt
 * Build (MSVC):   cl /LD /DUTIL_BUILDING_DLL /Fe:util.dll util.c user32.lib bcrypt.lib
 * Licensed under the GPL-3.0.
 *
 * Usage:
 *   ts                            now: epoch | epoch ms | ISO local | ISO UTC
 *   ts 1763385000                 epoch seconds -> ISO (UTC + local)
 *   ts 1763385000123              epoch millis  -> ISO (UTC + local)
 *   ts 2026-11-16T08:00+03:00     ISO (Z / +-hh:mm / bare = local) -> epoch
 *   ts clip                       convert whatever is on the clipboard
 *   slug Привет мир               -> "slug: privet-mir" (+ clipboard)
 *   slug hello world snake        -> hello_world_123 style joins
 *   slug hello world pascal       -> HelloWorld
 *   modes: (default kebab) snake, pascal, camel, upper
 *   sha256 <file>                 digest of one file (hex also on clipboard)
 *   sha256 /verify SHA256SUMS [base]
 *                                 GNU "hash  name" + BSD "SHA256 (name) = hash"
 *                                 sets, relative names resolve against [base]
 *                                 or the sums file folder (sha256sum -c rules)
 *
 * RU/UA/BG translit + diacritic stripping are compiled tables; unknown
 * symbols collapse to a word boundary instead of emitting mojibake.
 */
#include <windows.h>
#include <wctype.h>
#include <bcrypt.h>

#ifdef UTIL_BUILDING_DLL
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

#define EPOCH_DIFF 116444736000000000ULL      /* 1601->1970 in 100 ns units  */
#define OFF_LOCAL  0x40000000                  /* parseIso: no offset given    */

static HWND g_mainWnd = NULL;
static char g_result[4096];

/* --------------------------------- helpers ---------------------------------- */

static char* ullToA(ULONGLONG v, char* out) {
    char tmp[24]; int n = 0;
    if (!v) { *out++ = '0'; return out; }
    while (v) { tmp[n++] = (char)('0' + (int)(v % 10)); v /= 10; }
    while (n) *out++ = tmp[--n];
    *out = 0;
    return out;
}

static int isAllDigits(const char* s, int* len) {
    int n = 0;
    while (s[n] && s[n] >= '0' && s[n] <= '9') ++n;
    if (len) *len = n;
    return s[n] == 0 && n > 0;
}

/* v0.24 hardening: a thread-owned message-only window for OpenClipboard.
 * OpenClipboard(NULL) can BLOCK the host thread on a wedged clipboard chain;
 * with a real window it fails fast and the 5x retry loop does its job. */
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
        wc.lpszClassName = L"TypeBoxUtilClip";
        wc.hInstance     = self;
        if (RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
            h = CreateWindowExW(0, wc.lpszClassName, L"TypeBoxUtilClip", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, self, NULL);
    }
    return h;
}

static void copyToClipboard(const char* ascii) {
    int i;
    for (i = 0; i < 5; ++i) {
        HGLOBAL h; wchar_t* p; int wl;
        if (!OpenClipboard(ClipWnd())) { Sleep(120); continue; }
        EmptyClipboard();
        wl = (int)lstrlenA(ascii) + 1;
        h = GlobalAlloc(GMEM_MOVEABLE, (size_t)wl * sizeof(wchar_t));
        if (h) {
            p = (wchar_t*)GlobalLock(h);
            if (p) {
                int k;
                for (k = 0; ascii[k]; ++k) p[k] = (wchar_t)(unsigned char)ascii[k];
                p[k] = 0;
                GlobalUnlock(h);
                if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
            } else GlobalFree(h);
        }
        CloseClipboard();
        return;                                        /* best effort, never fatal */
    }
}

/* ----------------------------------- ts -------------------------------------- */

typedef struct { int y, mo, d, h, mi, sec; } Stamp;

static void fmtIso(const Stamp* st, int offMin, char* out) {
    if (offMin == 0x7FFFFFFF) {
        wsprintfA(out, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  st->y, st->mo, st->d, st->h, st->mi, st->sec);
    } else {
        const char* sign = offMin < 0 ? "-" : "+";   /* %s: wsprintf's %c is a WORD pop */
        int a = offMin < 0 ? -offMin : offMin;
        wsprintfA(out, "%04d-%02d-%02dT%02d:%02d:%02d%s%02d:%02d",
                  st->y, st->mo, st->d, st->h, st->mi, st->sec,
                  sign, a / 60, a % 60);
    }
}

static void stampFromSystemTime(const SYSTEMTIME* st, Stamp* out) {
    out->y = st->wYear; out->mo = st->wMonth; out->d = st->wDay;
    out->h = st->wHour; out->mi = st->wMinute; out->sec = st->wSecond;
}

/* minutes the local wall clock sits ahead of UTC for this UTC instant */
static int localOffsetMinFor(const SYSTEMTIME* utcST) {
    SYSTEMTIME loc;
    FILETIME fu, fl;
    if (!SystemTimeToTzSpecificLocalTime(NULL, utcST, &loc)) return 0;
    SystemTimeToFileTime(utcST, &fu);
    SystemTimeToFileTime(&loc, &fl);
    {
        ULONGLONG u  = ((ULONGLONG)fu.dwHighDateTime << 32) | fu.dwLowDateTime;
        ULONGLONG l  = ((ULONGLONG)fl.dwHighDateTime << 32) | fl.dwLowDateTime;
        if (l >= u) return (int)((l - u) / 600000000ULL);   /* 1 min = 6e8 * 100ns */
        return -(int)((u - l) / 600000000ULL);
    }
}

static int fileTimeFromEpoch(const Stamp* st, FILETIME* ft, int offMin) {
    SYSTEMTIME stw;
    if (st->mo < 1 || st->mo > 12 || st->d < 1 || st->d > 31 ||
        st->h > 23 || st->mi > 59 || st->sec > 60) return 0;
    ZeroMemory(&stw, sizeof(stw));
    stw.wYear = (WORD)st->y; stw.wMonth = (WORD)st->mo; stw.wDay = (WORD)st->d;
    stw.wHour = (WORD)st->h; stw.wMinute = (WORD)st->mi; stw.wSecond = (WORD)st->sec;
    if (offMin == OFF_LOCAL) {                         /* bare = local wall time */
        SYSTEMTIME utc;
        if (!TzSpecificLocalTimeToSystemTime(NULL, &stw, &utc)) return 0;
        return SystemTimeToFileTime(&utc, ft);
    }
    if (!SystemTimeToFileTime(&stw, ft)) return 0;
    {
        ULONGLONG v = ((ULONGLONG)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
        ULONGLONG delta = (ULONGLONG)(offMin < 0 ? -offMin : offMin) * 600000000ULL;
        if (offMin >= 0) v -= delta; else v += delta;  /* 08:00+03 -> 05:00Z      */
        ft->dwLowDateTime = (DWORD)v;
        ft->dwHighDateTime = (DWORD)(v >> 32);
    }
    return 1;
}

static int parseIso(const char* s, Stamp* st, int* offMin) {
    int i, sign = 1, oh = 0, om = 0;
    int hasOff = 0;
    if (!(s[0] && s[1] && s[2] && s[3] && s[4] == '-')) return 0;
    for (i = 0; i < 4; ++i) if (s[i] < '0' || s[i] > '9') return 0;
    st->y = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    s += 5;
    if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9') return 0;
    st->mo = (s[0]-'0')*10 + (s[1]-'0'); s += 2;
    if (*s != '-') return 0;
    ++s;
    if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9') return 0;
    st->d = (s[0]-'0')*10 + (s[1]-'0'); s += 2;
    st->h = 0; st->mi = 0; st->sec = 0;
    if (*s == L'T' || *s == L't' || *s == L' ') {
        ++s;
        if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9') return 0;
        st->h = (s[0]-'0')*10 + (s[1]-'0'); s += 2;
        if (*s == ':') { ++s; }
        else st->h = 0, st->mi = 0, st->sec = 0, --s;   /* date-only form       */
        if (s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9') {
            st->mi = (s[0]-'0')*10 + (s[1]-'0'); s += 2;
            if (*s == ':') {
                ++s;
                if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9') return 0;
                st->sec = (s[0]-'0')*10 + (s[1]-'0'); s += 2;
            }
            if (*s == '.') { ++s; while (*s >= '0' && *s <= '9') ++s; }  /* drop frac */
        } else { st->mi = 0; st->sec = 0; }
    }
    if (*s == L'Z' || *s == L'z') { *offMin = 0; return 1; }
    if (*s == '+' || *s == '-') {
        sign = (*s == '-') ? -1 : 1; ++s;
        if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9') return 0;
        oh = (s[0]-'0')*10 + (s[1]-'0'); s += 2;
        if (*s == ':') ++s;
        if (s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9') {
            om = (s[0]-'0')*10 + (s[1]-'0'); s += 2;
        }
        if (oh > 23 || om > 59) return 0;
        *offMin = sign * (oh * 60 + om); hasOff = 1;
    }
    while (*s == ' ') ++s;
    if (*s) return 0;
    (void)hasOff;
    if (!hasOff) *offMin = OFF_LOCAL;
    return 1;
}

/* answer for a concrete UTC FILETIME */
static int emitEpochLine(ULONGLONG epoch100, const char* label, char* out) {
    FILETIME ft; Stamp ust, lst; int off; SYSTEMTIME ustW; char isoU[40], isoL[40];
    ULONGLONG v = epoch100 + EPOCH_DIFF;
    ft.dwLowDateTime = (DWORD)v; ft.dwHighDateTime = (DWORD)(v >> 32);
    if (!FileTimeToSystemTime(&ft, &ustW)) return wsprintfA(out, "ts: value out of range");
    stampFromSystemTime(&ustW, &ust);
    off = localOffsetMinFor(&ustW);
    { SYSTEMTIME loc;
      SystemTimeToTzSpecificLocalTime(NULL, &ustW, &loc);
      stampFromSystemTime(&loc, &lst); }
    fmtIso(&ust, 0x7FFFFFFF, isoU);
    fmtIso(&lst, off, isoL);
    return wsprintfA(out, "ts: %s -> %s | %s", label, isoU, isoL);
}

static int readClipboardText(char* ascii, int cap) {
    int i;
    for (i = 0; i < 5; ++i) {
        int ok = 0;
        HANDLE h = NULL;
        if (!OpenClipboard(ClipWnd())) { Sleep(120); continue; }
        h = GetClipboardData(CF_UNICODETEXT);
        if (h) {
            wchar_t* src = (wchar_t*)GlobalLock(h);
            if (src) {
                int k;
                for (k = 0; src[k] && k < cap - 1; ++k) {
                    if (src[k] == L'\r' || src[k] == L'\n') break;
                    ascii[k] = (char)(unsigned char)src[k];   /* epochs/ISO are ASCII */
                }
                ascii[k] = 0;
                GlobalUnlock(h);
                ok = 1;
            }
        }
        CloseClipboard();
        if (ok) return 1;
        Sleep(120);
    }
    return 0;
}

static const char* runTs(const char* rest) {
    char* out = g_result;
    int fromClip = 0;
    while (*rest == ' ') ++rest;

    if (!*rest) {                                      /* now: the four columns  */
        FILETIME ft; SYSTEMTIME utc, loc; Stamp ust, lst; int off, d10, d13;
        ULONGLONG v; char e10[24], e13[24], isoU[40], isoL[40];
        typedef void (WINAPI *PreciseFn)(LPFILETIME);
        static PreciseFn pPrecise = (PreciseFn)(size_t)-1;
        if (pPrecise == (PreciseFn)(size_t)-1)
            pPrecise = (PreciseFn)GetProcAddress(GetModuleHandleA("kernel32"),
                                                 "GetSystemTimePreciseAsFileTime");
        if (pPrecise) pPrecise(&ft); else GetSystemTimeAsFileTime(&ft);
        v = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        v -= EPOCH_DIFF;
        ullToA(v / 10000000ULL, e10);
        ullToA(v / 10000ULL, e13);
        d10 = (int)lstrlenA(e10); d13 = (int)lstrlenA(e13);
        FileTimeToSystemTime(&ft, &utc);
        stampFromSystemTime(&utc, &ust);
        off = localOffsetMinFor(&utc);
        SystemTimeToTzSpecificLocalTime(NULL, &utc, &loc);
        stampFromSystemTime(&loc, &lst);
        fmtIso(&ust, 0x7FFFFFFF, isoU);
        fmtIso(&lst, off, isoL);
        (void)d10; (void)d13;
        wsprintfA(out, "ts: %s | %s | %s | %s", e10, e13, isoL, isoU);
        return out;
    }

    if (_stricmp(rest, "clip") == 0) {
        char clip[256];
        if (!readClipboardText(clip, (int)sizeof(clip)) || !clip[0])
            { wsprintfA(out, "ts: clipboard is not a timestamp"); return out; }
        rest = clip;
        fromClip = 1;
        while (*rest == ' ') ++rest;
        if (!*rest) { wsprintfA(out, "ts: clipboard is not a timestamp"); return out; }
    }

    {   /* digits? */
        int dl = 0;
        if (isAllDigits(rest, &dl) && (dl == 10 || dl == 13)) {
            ULONGLONG v = 0; int i;
            for (i = 0; i < dl; ++i) v = v * 10 + (ULONGLONG)(rest[i] - '0');
            if (dl == 10) emitEpochLine(v * 10000000ULL, rest, out);
            else          emitEpochLine(v * 10000ULL,     rest, out);
            return out;
        }
    }
    {   /* ISO? */
        Stamp st; int off;
        if (parseIso(rest, &st, &off)) {
            FILETIME ft; ULONGLONG v;
            if (!fileTimeFromEpoch(&st, &ft, off))
                { wsprintfA(out, "ts: date out of range"); return out; }
            v = (((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) - EPOCH_DIFF;
            emitEpochLine(v, rest, out);              /* same shape as epochs    */
            return out;
        }
    }
    wsprintfA(out, fromClip ? "ts: clipboard is not a timestamp"
                            : "ts: use ts, ts 1763385000, ts 2026-11-16T08:00+03:00 or ts clip");
    return out;
}

/* ---------------------------------- slug ------------------------------------- */

struct TrPair { wchar_t src; const char* dst; };
static const struct TrPair kTr[] = {
    /* Russian */
    { 0x0430, "a" }, { 0x0431, "b" }, { 0x0432, "v" }, { 0x0433, "g" },
    { 0x0434, "d" }, { 0x0435, "e" }, { 0x0451, "e" },  { 0x0436, "zh" },
    { 0x0437, "z" }, { 0x0438, "i" }, { 0x0439, "y" },  { 0x043a, "k" },
    { 0x043b, "l" }, { 0x043c, "m" }, { 0x043d, "n" },  { 0x043e, "o" },
    { 0x043f, "p" }, { 0x0440, "r" }, { 0x0441, "s" },  { 0x0442, "t" },
    { 0x0443, "u" }, { 0x0444, "f" }, { 0x0445, "h" },  { 0x0446, "ts" },
    { 0x0447, "ch" }, { 0x0448, "sh" }, { 0x0449, "sch" }, { 0x044a, "" },
    { 0x044b, "y" }, { 0x044c, "" },  { 0x044d, "e" },  { 0x044e, "yu" },
    { 0x044f, "ya" },
    /* Ukrainian + Belarusian */
    { 0x0456, "i" }, { 0x0457, "yi" }, { 0x0454, "ye" }, { 0x0491, "g" },
    { 0x045E, "u" },
    /* common Latin diacritics */
    { 0x00E0, "a" }, { 0x00E1, "a" }, { 0x00E2, "a" }, { 0x00E3, "a" },
    { 0x00E4, "a" }, { 0x00E5, "a" }, { 0x0101, "a" }, { 0x0105, "a" },
    { 0x00E8, "e" }, { 0x00E9, "e" }, { 0x00EA, "e" }, { 0x00EB, "e" },
    { 0x0113, "e" }, { 0x0117, "e" }, { 0x0119, "e" },
    { 0x00EC, "i" }, { 0x00ED, "i" }, { 0x00EE, "i" }, { 0x00EF, "i" },
    { 0x012B, "i" },
    { 0x00F2, "o" }, { 0x00F3, "o" }, { 0x00F4, "o" }, { 0x00F5, "o" },
    { 0x00F6, "o" }, { 0x00F8, "o" }, { 0x014D, "o" },
    { 0x00F9, "u" }, { 0x00FA, "u" }, { 0x00FB, "u" }, { 0x00FC, "u" },
    { 0x016B, "u" }, { 0x01D8, "u" },
    { 0x00E7, "c" }, { 0x0107, "c" }, { 0x010D, "c" },
    { 0x00F1, "n" }, { 0x0144, "n" }, { 0x0146, "n" },
    { 0x0161, "s" }, { 0x015B, "s" }, { 0x015F, "s" },
    { 0x017E, "z" }, { 0x017A, "z" }, { 0x017C, "z" },
    { 0x010F, "d" }, { 0x0142, "l" }, { 0x0161, "s" },
    { 0x00DF, "ss" }, { 0x00E6, "ae" }, { 0x0153, "oe" },
    { 0x00F0, "d" }, { 0x00FE, "th" },
};

/* towlower() is locale-dependent (C locale = ASCII only, and the plugin never
 * calls setlocale), so fold cases ourselves for the ranges the table covers. */
static wchar_t WLower(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return (wchar_t)(c + 32);
    if (c >= 0x0410 && c <= 0x042F) return (wchar_t)(c + 0x20);   /* А-Я  */
    if (c >= 0x0400 && c <= 0x040F) return (wchar_t)(c + 0x50);   /* Ё-Ў  */
    if (c >= 0x00C0 && c <= 0x00D6) return (wchar_t)(c + 0x20);   /* À-Ö  */
    if (c >= 0x00D8 && c <= 0x00DE) return (wchar_t)(c + 0x20);   /* Ø-Þ  */
    if (c == 0x0490) return 0x0491;                               /* Ґ    */
    return c;
}

static int translit(wchar_t c, char* dst /* >= 5 */) {
    size_t i;
    c = WLower(c);
    for (i = 0; i < sizeof(kTr)/sizeof(kTr[0]); ++i)
        if (kTr[i].src == c) { lstrcpyA(dst, kTr[i].dst); return 1; }
    return 0;
}

static int isSep(wchar_t c) {
    return c == L' ' || c == L'-' || c == L'_' || c == L'.' || c == L'/' ||
           c == L'\\' || c == L':' || c == L';' || c == L',' || c == L'|' ||
           c == L'!' || c == L'?' || c == L'@' || c == L'#' || c == L'$' ||
           c == L'%' || c == L'^' || c == L'&' || c == L'*' || c == L'(' ||
           c == L')' || c == L'=' || c == L'+' || c == L'~' || c == L'`' ||
           c == L'\'' || c == L'"' || c == L'<' || c == L'>' || c == L'[' ||
           c == L']' || c == L'{' || c == L'}' || c == L'\t' ||
           c == L'\r' || c == L'\n';
}

enum { MODE_KEBAB = 0, MODE_SNAKE, MODE_UPPER, MODE_PASCAL, MODE_CAMEL };

static int modeOf(const char* w) {
    if (!_stricmp(w, "snake"))  return MODE_SNAKE;
    if (!_stricmp(w, "upper"))  return MODE_UPPER;
    if (!_stricmp(w, "pascal")) return MODE_PASCAL;
    if (!_stricmp(w, "camel"))  return MODE_CAMEL;
    if (!_stricmp(w, "kebab"))  return MODE_KEBAB;
    return -1;
}

static const char* runSlug(const char* rest) {
    char* out = g_result;
    wchar_t w[1024];
    int  mode = MODE_KEBAB, haveMode = 0;
    int textEnd, i, nwords = 0;
    char words[16][128];
    int  cur[1];

    while (*rest == ' ') ++rest;
    if (!*rest) { lstrcpyA(out, "slug: give me some text"); return out; }

    MultiByteToWideChar(CP_ACP, 0, rest, -1, w, 1024);
    textEnd = (int)lstrlenW(w);

    /* trailing mode token? ("slug hello world snake") */
    {   int e = textEnd, s;
        while (e > 0 && w[e-1] == L' ') --e;
        s = e;
        while (s > 0 && w[s-1] != L' ') --s;
        if (s < e) {
            char tok[16]; int k;
            for (k = 0; s + k < e && k < 15; ++k) tok[k] = (char)w[s+k];
            tok[k] = 0;
            if (modeOf(tok) >= 0 && s > 0) { mode = modeOf(tok); haveMode = 1; textEnd = s; }
        }
    }
    (void)haveMode;

    nwords = 0; cur[0] = 0;
    words[0][0] = 0;
    for (i = 0; i < textEnd; ++i) {
        wchar_t c = w[i];
        if (isSep(c)) {
            if (cur[0]) { if (nwords < 15) { ++nwords; words[nwords][0] = 0; } cur[0] = 0; }
            continue;
        }
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
            (c >= L'0' && c <= L'9')) {
            if (nwords > 15 || cur[0] >= 126) continue;
            words[nwords][cur[0]++] = (char)towlower(c);
            words[nwords][cur[0]] = 0;
            continue;
        }
        {   char piece[8];
            if (translit(c, piece)) {
                int k;
                if (piece[0]) {
                    for (k = 0; piece[k] && cur[0] < 126; ++k) {
                        if (nwords > 15) break;
                        words[nwords][cur[0]++] = piece[k];
                        words[nwords][cur[0]] = 0;
                    }
                } else if (cur[0]) {                 /* ъ/ь split the word       */
                    if (nwords < 15) { ++nwords; words[nwords][0] = 0; } cur[0] = 0;
                }
            } else {                                 /* unknown -> boundary      */
                if (cur[0]) { if (nwords < 15) { ++nwords; words[nwords][0] = 0; } cur[0] = 0; }
            }
        }
    }

    /* join */
    {   char res[2048]; int r = 0, k;
        int count = nwords + (cur[0] ? 1 : 0);        /* words[0..nwords] + open */
        char join = mode == MODE_SNAKE ? '_' : '-';
        if (count == 0) { lstrcpyA(out, "slug: nothing left to slug"); return out; }
        for (k = 0; k < count && r < 2000; ++k) {
            const char* word = words[k];
            if (mode == MODE_PASCAL || mode == MODE_CAMEL) {
                if (k || mode == MODE_PASCAL) {
                    if (word[0] >= 'a' && word[0] <= 'z') res[r++] = (char)(word[0] - 32);
                    else if (word[0]) res[r++] = word[0];
                    ++word;
                }
                while (*word && r < 2000) res[r++] = *word++;
            } else {
                if (k) res[r++] = join;
                while (*word && r < 2000) res[r++] = *word++;
            }
        }
        res[r] = 0;
        if (mode == MODE_UPPER) for (k = 0; res[k]; ++k)
            if (res[k] >= 'a' && res[k] <= 'z') res[k] = (char)(res[k] - 32);
        wsprintfA(out, "slug: %s", res);
        copyToClipboard(res);
    }
    return out;
}

/* ---------------------------------- sha256 ------------------------------------ */
/* RI3 #246: the release-download ritual, one command. BCrypt is the host's own
 * crypto provider (same one /update verifies with). Single-line console answer
 * for a set: "sha256: N checked, N ok" / "... N FAILED: names". */

static int isHex64(const char* s) {
    int i;
    for (i = 0; i < 64; ++i) {
        char c = s[i];
        int hexd = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hexd) return 0;
    }
    return s[64] == 0 || s[64] == ' ' || s[64] == '\t';
}

static int sha256FileW(const wchar_t* path, char* hex /* 65 */) {
    static const wchar_t* algId = L"SHA256";
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    HANDLE f = INVALID_HANDLE_VALUE;
    PUCHAR obj = NULL, chunk = NULL;
    ULONG objLen = 0, hashLen = 0, rd = 0;
    int ok = 0;
    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) goto done;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, algId, NULL, 0))) goto done;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen,
                                          sizeof(objLen), &rd, 0))) goto done;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen,
                                          sizeof(hashLen), &rd, 0))) goto done;
    if (hashLen != 32) goto done;
    obj = (PUCHAR)LocalAlloc(LPTR, objLen);
    chunk = (PUCHAR)LocalAlloc(LPTR, 65536);
    if (!obj || !chunk) goto done;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, obj, objLen, NULL, 0, 0))) goto done;
    for (;;) {
        if (!ReadFile(f, chunk, 65536, &rd, NULL) || rd == 0) break;
        if (!BCRYPT_SUCCESS(BCryptHashData(hHash, chunk, rd, 0))) goto done;
    }
    {
        UCHAR digest[32];
        int i;
        if (!BCRYPT_SUCCESS(BCryptFinishHash(hHash, digest, 32, 0))) goto done;
        for (i = 0; i < 32; ++i) {
            hex[i * 2]     = "0123456789abcdef"[digest[i] >> 4];
            hex[i * 2 + 1] = "0123456789abcdef"[digest[i] & 15];
        }
        hex[64] = 0;
        ok = 1;
    }
done:
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    if (obj) LocalFree(obj);
    if (chunk) LocalFree(chunk);
    return ok;
}

/* resolve a (possibly relative) name from a sums file against base/dir */
static void resolveName(wchar_t* out, int cap, const wchar_t* dir, const char* nameA) {
    wchar_t nm[512];
    int i;
    MultiByteToWideChar(CP_ACP, 0, nameA, -1, nm, 512);
    for (i = 0; nm[i]; ++i) if (nm[i] == L'/') nm[i] = L'\\';
    if (nm[1] == L':' || nm[0] == L'\\') { lstrcpynW(out, nm, cap); return; }   /* absolute */
    out[0] = 0;
    if (dir && *dir) { lstrcatW(out, dir); if (out[lstrlenW(out) - 1] != L'\\') lstrcatW(out, L"\\"); }
    lstrcatW(out, nm);
}

static const char* runSha(const char* rest) {
    char* out = g_result;
    while (*rest == ' ') ++rest;

    if (!_strnicmp(rest, "/verify", 7) && (rest[7] == 0 || rest[7] == ' ')) {
        char sums[512] = {}, base[512] = {};
        char buf[65536];
        DWORD rd = 0;
        HANDLE f;
        wchar_t wsums[560], wdir[560] = L"";
        const char* p = rest + 7;
        int i = 0, checked = 0, failed = 0, more = 0;
        char fails[3][160];
        char* line;
        fails[0][0] = fails[1][0] = fails[2][0] = 0;
        while (*p == ' ') ++p;
        if (!*p) { wsprintfA(out, "sha256: use sha256 /verify SHA256SUMS [base]"); return out; }
        {   int quoted = (*p == '"');
            if (quoted) ++p;
            while (*p && i < 510 && (quoted ? *p != '"' : *p != ' ')) sums[i++] = *p++;
            sums[i] = 0;
            if (quoted) while (*p == '"') ++p;
        }
        while (*p == ' ') ++p;
        if (*p) {                       /* optional base folder for relative names */
            int q = (*p == '"');
            int j = 0;
            if (q) ++p;
            while (*p && j < 510 && (q ? *p != '"' : *p != 0)) base[j++] = *p++;
            base[j] = 0;
        }
        MultiByteToWideChar(CP_ACP, 0, sums, -1, wsums, 560);
        f = CreateFileW(wsums, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) { wsprintfA(out, "sha256: cannot open %s", sums); return out; }
        if (!ReadFile(f, buf, sizeof(buf) - 1, &rd, NULL)) { CloseHandle(f); wsprintfA(out, "sha256: cannot read %s", sums); return out; }
        CloseHandle(f);
        buf[rd] = 0;
        /* default base = the sums file's own folder (sha256sum -c rules) */
        if (!base[0]) {
            int k;
            lstrcpynW(wdir, wsums, 560);
            for (k = (int)lstrlenW(wdir) - 1; k >= 0 && wdir[k] != L'\\'; --k) {}
            if (k >= 0) wdir[k] = 0; else wdir[0] = 0;
        } else {
            wchar_t wb[560];
            MultiByteToWideChar(CP_ACP, 0, base, -1, wb, 560);
            for (i = 0; wb[i]; ++i) if (wb[i] == L'/') wb[i] = L'\\';
            lstrcpynW(wdir, wb, 560);
        }
        for (line = buf; line && *line;) {
            char* nl = strchr(line, '\n');
            char* eq;
            char name[480];
            char want[80];
            char got[65];
            wchar_t wpath[1200];
            char* sp;
            int lineBad = 0;
            if (nl) *nl = 0;
            /* trim CR/spaces both ends */
            {   int e = (int)lstrlenA(line);
                while (e > 0 && (line[e-1] == '\r' || line[e-1] == ' ' || line[e-1] == '\t')) line[--e] = 0; }
            while (*line == ' ' || *line == '\t') ++line;
            if (!*line || *line == '#') { if (nl) line = nl + 1; else break; continue; }
            want[0] = name[0] = 0;
            eq = strstr(line, "= ");
            if (_strnicmp(line, "SHA256 (", 8) == 0 && eq) {       /* BSD flavour */
                char* lb = line + 7, *rb = strchr(lb, ')');
                int n = 0;
                if (!rb) lineBad = 1;
                else {
                    ++lb;
                    while (*lb && n < 470 && lb < rb) name[n++] = *lb++;
                    name[n] = 0;
                    lstrcpynA(want, eq + 2, 72);
                }
            } else if (isHex64(line)) {                            /* GNU flavour */
                lstrcpynA(want, line, 65);                         /* 64 hex + NUL (66 kept a space!) */
                sp = line + 64;
                while (*sp == ' ' || *sp == '\t') ++sp;
                if (*sp == '*' || *sp == '?') ++sp;                /* binary/text flag */
                lstrcpynA(name, sp, 480);
            } else lineBad = 1;
            if (lineBad || !want[0] || !name[0]) {
                ++checked; ++failed;
                if (failed <= 3) lstrcpynA(fails[failed - 1], "<malformed>", 160);
                else ++more;
                if (nl) line = nl + 1; else break;
                continue;
            }
            {   int k; for (k = 0; want[k]; ++k)
                    if (want[k] >= 'A' && want[k] <= 'F') want[k] = (char)(want[k] + 32); }
            resolveName(wpath, 1200, wdir, name);
            ++checked;
            if (sha256FileW(wpath, got) && lstrcmpA(got, want) == 0) {
                if (nl) line = nl + 1; else break;   /* MUST advance - a bare
                                                      * continue here hung the
                                                      * host thread on the first OK line */
                continue;
            }
            ++failed;
            if (failed <= 3) lstrcpynA(fails[failed - 1], name, 160);
            else ++more;
            if (nl) line = nl + 1; else break;
        }
        if (failed == 0) wsprintfA(out, "sha256: %d checked, all ok", checked);
        else {
            int n = wsprintfA(out, "sha256: %d checked, %d FAILED: %s", checked, failed, fails[0]);
            if (fails[1][0]) n += wsprintfA(out + n, ", %s", fails[1]);
            if (fails[2][0]) n += wsprintfA(out + n, ", %s", fails[2]);
            if (more > 0)    wsprintfA(out + n, " (+%d more)", more);
        }
        return out;
    }

    if (*rest) {                                       /* single-file digest   */
        char file[512];
        wchar_t wfile[560];
        char hex[65];
        const char* p = rest;
        int i = 0, q = (*p == '"');
        if (q) ++p;
        while (*p && i < 510 && (q ? *p != '"' : *p != 0)) file[i++] = *p++;
        file[i] = 0;
        if (!file[0]) { wsprintfA(out, "sha256: use sha256 <file> or sha256 /verify SHA256SUMS [base]"); return out; }
        MultiByteToWideChar(CP_ACP, 0, file, -1, wfile, 560);
        if (!sha256FileW(wfile, hex)) { wsprintfA(out, "sha256: cannot open %s", file); return out; }
        wsprintfA(out, "sha256: %s  %s", hex, file);
        copyToClipboard(hex);
        return out;
    }
    wsprintfA(out, "sha256: use sha256 <file> or sha256 /verify SHA256SUMS [base]");
    return out;
}

/* ----------------------------------- ABI ------------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) { g_mainWnd = mainWnd; }
PLUGIN_API void __cdecl Unload(void) { }

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Util";
    i.version = "1.1";
    i.description = "ts epoch/ISO converter \u00b7 slug translit \u00b7 sha256 file + /verify sums";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 3; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[3] = { "ts", "slug", "sha256" };
    return (n >= 0 && n < 3) ? k[n] : NULL;
}

static char g_etRows[4][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 4) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int len;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    len = lstrlenA(input);
    if (len <= 2 && _strnicmp(input, "ts", len) == 0) {
        etAdd("ts"); etAdd("ts clip"); etAdd("ts 1763385000"); etAdd("ts 2026-11-16T08:00Z");
    } else if (len > 2 && _strnicmp(input, "ts", 2) == 0) {
        etAdd("ts clip"); etAdd("ts 1763385000");
    } else if (len <= 4 && _strnicmp(input, "slug", len) == 0) {
        etAdd("slug hello world"); etAdd("slug hello world snake"); etAdd("slug hello world pascal");
    } else if (len > 4 && _strnicmp(input, "slug", 4) == 0) {
        etAdd("slug hello world snake"); etAdd("slug hello world pascal");
    } else if (len <= 6 && _strnicmp(input, "sha256", len) == 0) {
        etAdd("sha256 file.bin"); etAdd("sha256 /verify SHA256SUMS");
    } else if (len > 6 && _strnicmp(input, "sha256", 6) == 0) {
        etAdd("sha256 /verify SHA256SUMS");
    }
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    const char* text = NULL;
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    while (*str == ' ') ++str;
    if (_strnicmp(str, "ts", 2) == 0 && (str[2] == 0 || str[2] == ' '))
        text = runTs(str + 2);
    else if (_strnicmp(str, "slug", 4) == 0 && (str[4] == 0 || str[4] == ' '))
        text = runSlug(str + 4);
    else if (_strnicmp(str, "sha256", 6) == 0 && (str[6] == 0 || str[6] == ' '))
        text = runSha(str + 6);                      /* RI3 #246                  */
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = g_result;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(g_result);
    return ex;
}
