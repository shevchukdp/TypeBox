/* json.dll - TypeBox JSON/JWT plugin (v0.30.0 wave, gallery #18).
 *
 *   json fmt <text>      pretty-print (2-space), validates the JSON too
 *   json min <text>      minify (no spaces outside strings)
 *   json get <path> <t>  value at a.b[2].c - string raw, else compact JSON
 *   json keys <text>     top-level keys (object, or first element of array)
 *   jwt <token>          decode header + payload (base64url), show exp/iat
 *   jwt decode <token>   same
 *
 * Everything is pure computation on the typed line - zero network, zero
 * timers, the whole offline-first rule by construction. Typed bytes are
 * CP_ACP and travel through untouched; only the base64-decoded JWT body is
 * re-encoded UTF-8 -> CP_ACP.
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef JSON_BUILDING_DLL
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

#define J_CAP 65536

static HWND g_mainWnd = NULL;
static char g_out[2 * J_CAP];          /* "json: " prefix + body            */
static char g_body[J_CAP];             /* emit target before prefixing      */

typedef struct { char* buf; size_t cap; size_t len; int overflow; } Out;

static void oInit(Out* o, char* buf, size_t cap) {
    o->buf = buf; o->cap = cap; o->len = 0; o->overflow = 0;
}
static void oPut(Out* o, const char* s, size_t n) {
    if (o->len + n >= o->cap) { o->overflow = 1; return; }
    memcpy(o->buf + o->len, s, n);
    o->len += n;
}
static void oStr(Out* o, const char* s) { oPut(o, s, lstrlenA(s)); }
static void oCh(Out* o, char c) { oPut(o, &c, 1); }
static void oFinish(Out* o) { o->buf[o->len] = 0; }

/* prefix the body and hand out the answer */
static const char* ansBody(const char* prefix, const char* body) {
    size_t pl = lstrlenA(prefix);
    size_t bl = lstrlenA(body);
    if (pl + bl + 1 > sizeof(g_out)) {
        bl = sizeof(g_out) - pl - 5;
        memcpy(g_out, prefix, pl);
        memcpy(g_out + pl, body, bl);
        memcpy(g_out + pl + bl, "...\0", 4);
        return g_out;
    }
    memcpy(g_out, prefix, pl);
    memcpy(g_out + pl, body, bl + 1);
    return g_out;
}

static const char* ansMsg(const char* msg) {   /* body = fixed ASCII msg */
    return ansBody("json: ", msg);
}

static const char* ansWide(const wchar_t* w) { /* wide -> CP_ACP answer   */
    WideCharToMultiByte(CP_ACP, 0, w, -1, g_out, sizeof(g_out), NULL, NULL);
    return g_out;
}

/* ------------------------------- JSON parser -------------------------------- */

#define JSON_MAX_DEPTH 32

static void skipWs(const char** p) {
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') ++*p;
}

static int atEnd(const char** p) { skipWs(p); return **p == 0; }

static int isValueStart(char c) {
    return c == '{' || c == '[' || c == '"' || c == 't' || c == 'f' || c == 'n' ||
           c == '-' || (c >= '0' && c <= '9');
}

static int emitVal(const char** p, Out* o, int indent, int pretty, int depth);

static int emitStr(const char** p, Out* o, int raw) {
    /* *p at '"'; emits inner bytes (unescaped when raw), consumes quotes */
    ++*p;
    while (**p && **p != '"') {
        if (**p == '\\') {
            ++*p;
            if (!**p) return 0;
            if (raw) {
                switch (**p) {
                case 'n': oCh(o, '\n'); break;
                case 'r': oCh(o, '\r'); break;
                case 't': oCh(o, '\t'); break;
                case 'b': oCh(o, '\b'); break;
                case 'f': oCh(o, '\f'); break;
                case 'u': oStr(o, "\\u"); break;  /* keep \u literal */
                default: oCh(o, **p); break;
                }
            } else { oCh(o, '\\'); oCh(o, **p); }
            ++*p;
        } else {
            oCh(o, **p);
            ++*p;
        }
    }
    if (**p != '"') return 0;
    ++*p;
    return 1;
}

static int emitTok(const char** p, Out* o, const char* lit) {
    int n = lstrlenA(lit);
    if (_strnicmp(*p, lit, (size_t)n) != 0) return 0;
    oPut(o, lit, (size_t)n);
    *p += n;
    return 1;
}

static int emitNum(const char** p, Out* o) {
    const char* start = *p;
    if (**p == '-') ++*p;
    while (**p >= '0' && **p <= '9') ++*p;
    if (**p == '.') { ++*p; while (**p >= '0' && **p <= '9') ++*p; }
    if (**p == 'e' || **p == 'E') {
        ++*p;
        if (**p == '+' || **p == '-') ++*p;
        while (**p >= '0' && **p <= '9') ++*p;
    }
    if (*p == start) return 0;
    oPut(o, start, (size_t)(*p - start));
    return 1;
}

static void putIndent(Out* o, int indent) {
    int i;
    for (i = 0; i < indent; ++i) oStr(o, "  ");
}

/* skip one value without emitting (throwaway writer is fine, overflow ignored) */
static int skipVal(const char** p, int depth) {
    Out sink;
    char s[8];
    oInit(&sink, s, sizeof(s));
    return emitVal(p, &sink, 0, 0, depth);
}

static int emitVal(const char** p, Out* o, int indent, int pretty, int depth) {
    if (depth > JSON_MAX_DEPTH) return 0;
    skipWs(p);
    if (!isValueStart(**p)) return 0;
    switch (**p) {
    case '{': {
        ++*p;
        oCh(o, '{');
        skipWs(p);
        if (**p == '}') { ++*p; oCh(o, '}'); return 1; }
        for (;;) {
            skipWs(p);
            if (**p != '"') return 0;
            if (pretty) { oCh(o, '\n'); putIndent(o, indent + 1); }
            oCh(o, '"');                          /* member name keeps its quotes */
            if (!emitStr(p, o, 0)) return 0;
            oCh(o, '"');
            oCh(o, ':');
            if (pretty) oCh(o, ' ');
            skipWs(p);
            if (**p != ':') return 0;
            ++*p;
            if (!emitVal(p, o, indent + 1, pretty, depth + 1)) return 0;
            skipWs(p);
            if (**p == ',') { ++*p; oCh(o, ','); continue; }
            if (**p == '}') {
                ++*p;
                if (pretty) { oCh(o, '\n'); putIndent(o, indent); }
                oCh(o, '}');
                return 1;
            }
            return 0;
        }
    }
    case '[': {
        ++*p;
        oCh(o, '[');
        skipWs(p);
        if (**p == ']') { ++*p; oCh(o, ']'); return 1; }
        for (;;) {
            if (pretty) { oCh(o, '\n'); putIndent(o, indent + 1); }
            if (!emitVal(p, o, indent + 1, pretty, depth + 1)) return 0;
            skipWs(p);
            if (**p == ',') { ++*p; oCh(o, ','); continue; }
            if (**p == ']') {
                ++*p;
                if (pretty) { oCh(o, '\n'); putIndent(o, indent); }
                oCh(o, ']');
                return 1;
            }
            return 0;
        }
    }
    case '"':
        oCh(o, '"');
        if (!emitStr(p, o, 0)) return 0;
        oCh(o, '"');
        return 1;
    case 't': return emitTok(p, o, "true");
    case 'f': return emitTok(p, o, "false");
    case 'n': return emitTok(p, o, "null");
    default:  return emitNum(p, o);
    }
}

/* ----------------------------- json commands -------------------------------- */

static const char* runFmt(const char* text, int pretty) {
    Out o;
    const char* p = text;
    if (!*p) return ansMsg(pretty ? "usage: json fmt <text>" : "usage: json min <text>");
    oInit(&o, g_body, sizeof(g_body));
    if (!emitVal(&p, &o, 0, pretty, 0) || !atEnd(&p))
        return ansMsg("invalid JSON - check braces/quotes");
    if (o.overflow) return ansMsg("answer too long (64 KB cap)");
    oFinish(&o);
    return ansBody("json: ", g_body);
}

static const char* runKeys(const char* text) {
    Out o;
    const char* p = text;
    if (!*p) return ansMsg("usage: json keys <text>");
    skipWs(&p);
    if (*p == '[') { ++p; skipWs(&p); }
    if (*p != '{') return ansMsg("keys: expected an object");
    ++p;
    oInit(&o, g_body, sizeof(g_body));
    skipWs(&p);
    if (*p == '}') return ansMsg("keys: (empty object)");
    for (;;) {
        const char* ks;
        int klen;
        skipWs(&p);
        if (*p != '"') break;
        ks = ++p;
        while (*p && *p != '"') { if (*p == '\\') ++p; if (*p) ++p; }
        klen = (int)(p - ks);
        if (*p != '"') return ansMsg("keys: invalid JSON - unterminated string");
        ++p;
        skipWs(&p);
        if (*p != ':') return ansMsg("keys: invalid JSON - expected ':'");
        ++p;
        if (!skipVal(&p, 0)) return ansMsg("keys: invalid JSON value");
        if (o.len) oStr(&o, ", ");
        oPut(&o, ks, (size_t)klen);
        skipWs(&p);
        if (*p == ',') { ++p; continue; }
        if (*p == '}') { ++p; break; }
        return ansMsg("keys: invalid JSON - expected ',' or '}'");
    }
    if (o.len == 0) return ansMsg("keys: (no string keys)");
    oFinish(&o);
    return ansBody("json: keys: ", g_body);
}

/* find member `name` (already unescaped) inside the object at *val;
 * returns pointer to its value or NULL */
static const char* objMember(const char* val, const char* name) {
    int want = lstrlenA(name);
    skipWs(&val);
    if (*val != '{') return NULL;
    ++val;
    skipWs(&val);
    while (*val == '"') {
        char unesc[256];
        int un = 0;
        const char* ks;
        ++val;
        ks = val;
        while (*val && *val != '"') {
            if (*val == '\\' && val[1]) {
                if (un < 255) unesc[un++] = val[1];
                val += 2;
            } else {
                if (un < 255) unesc[un++] = *val;
                ++val;
            }
        }
        unesc[un] = 0;
        if (*val != '"') return NULL;
        ++val;
        skipWs(&val);
        if (*val != ':') return NULL;
        ++val;
        skipWs(&val);
        if (un == want && memcmp(unesc, name, (size_t)want) == 0)
            return val;
        if (!skipVal(&val, 0)) return NULL;
        skipWs(&val);
        if (*val == ',') { ++val; skipWs(&val); continue; }
        if (*val == '}') return NULL;
        return NULL;
    }
    return NULL;
}

/* find element idx inside the array at *val; returns its value or NULL */
static const char* arrElement(const char* val, int idx) {
    int k;
    skipWs(&val);
    if (*val != '[') return NULL;
    ++val;
    skipWs(&val);
    if (*val == ']') return NULL;
    for (k = 0;; ++k) {
        const char* here;
        skipWs(&val);
        if (k == idx) return val;
        here = val;
        if (!skipVal(&val, 0) || val == here) return NULL;
        skipWs(&val);
        if (*val == ',') { ++val; continue; }
        return NULL;
    }
}

static const char* runGet(const char* args) {
    Out o;
    char path[256];
    const char* sp;
    const char* val;
    char* pe;
    if (!*args) return ansMsg("usage: json get <path> <text>");
    sp = strchr(args, ' ');
    if (!sp) return ansMsg("usage: json get <path> <text>");
    {   int n = (int)(sp - args);
        if (n <= 0 || n >= (int)sizeof(path)) return ansMsg("get: bad path");
        memcpy(path, args, (size_t)n);
        path[n] = 0;
    }
    val = sp + 1;
    while (*val == ' ') ++val;
    if (!*val) return ansMsg("usage: json get <path> <text>");

    pe = path;
    for (;;) {
        char* dot = strchr(pe, '.');
        char* br = strchr(pe, '[');
        char* afterDot = NULL;
        if (dot) { *dot = 0; afterDot = dot + 1; }
        if (br) {
            int idx;
            *br = 0;
            idx = atoi(br + 1);
            if (*pe) {   /* name[n] */
                val = objMember(val, pe);
                if (!val) return ansMsg("get: path not found");
            }
            val = arrElement(val, idx);
            if (!val) return ansMsg("get: path not found");
            if (!afterDot) break;
            pe = afterDot;
            continue;
        }
        if (!*pe) return ansMsg("get: bad path");
        val = objMember(val, pe);
        if (!val) return ansMsg("get: path not found");
        if (!afterDot) break;
        pe = afterDot;
    }
    skipWs(&val);
    if (*val == '"') {   /* string: hand back unescaped, raw bytes */
        oInit(&o, g_body, sizeof(g_body));
        if (!emitStr(&val, &o, 1)) return ansMsg("get: bad string");
        oFinish(&o);
        return ansBody("json: ", g_body);
    }
    oInit(&o, g_body, sizeof(g_body));
    if (!emitVal(&val, &o, 0, 0, 0)) return ansMsg("get: bad value");
    if (o.overflow) return ansMsg("get: value too long (64 KB cap)");
    oFinish(&o);
    return ansBody("json: ", g_body);
}

/* ------------------------------ JWT decode ---------------------------------- */

static int b64Decode(const char* in, char* out, size_t cap) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int acc = 0, bits = 0, n = 0, i;
    for (i = 0; in[i]; ++i) {
        int v;
        char c = in[i];
        if (c == '-' || c == '_') v = c == '-' ? 62 : 63;
        else if (c == '=') break;
        else {
            const char* q = strchr(tbl, c);
            if (!q) return 0;
            v = (int)(q - tbl);
        }
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if ((size_t)n + 1 >= cap) return 0;
            out[n++] = (char)((acc >> bits) & 0xFF);
        }
    }
    out[n] = 0;
    return 1;
}

static long long jsonNum(const char* body, const char* key) {
    /* top-level "key": <number>; -1 when absent (good enough for JWT claims) */
    char pat[40];
    const char* p = body;
    wsprintfA(pat, "\"%s\"", key);
    while ((p = strstr(p, pat)) != NULL) {
        const char* q = p + lstrlenA(pat);
        long long v = 0;
        int neg = 0, any = 0;
        while (*q == ' ' || *q == '\t') ++q;
        if (*q != ':') { p = q; continue; }
        ++q;
        while (*q == ' ' || *q == '\t') ++q;
        if (*q == '-') { neg = 1; ++q; }
        while (*q >= '0' && *q <= '9') { v = v * 10 + (*q - '0'); ++q; any = 1; }
        if (any) return neg ? -v : v;
        p = q;
    }
    return -1;
}

static void addClockLine(wchar_t* wout, size_t wcap, const wchar_t* label, long long secs) {
    wchar_t line[128];
    FILETIME ft;
    SYSTEMTIME st;
    ULARGE_INTEGER u;
    const wchar_t* state = L"";
    u.QuadPart = 116444736000000000ULL + (ULONGLONG)secs * 10000000ULL;
    ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
    FileTimeToSystemTime(&ft, &st);
    {   /* exp gets an expired/valid verdict against the wall clock */
        SYSTEMTIME now;
        FILETIME n1;
        ULONGLONG tn;
        GetSystemTime(&now);
        SystemTimeToFileTime(&now, &n1);
        tn = ((ULONGLONG)n1.dwHighDateTime << 32) | n1.dwLowDateTime;
        if (lstrcmpW(label, L"exp") == 0)
            state = (u.QuadPart < tn) ? L" - expired" : L" - valid";
    }
    wsprintfW(line, L"%s: %04d-%02d-%02d %02d:%02d UTC%s\n", label,
              (int)st.wYear, (int)st.wMonth, (int)st.wDay,
              (int)st.wHour, (int)st.wMinute, state);
    if (lstrlenW(wout) + lstrlenW(line) + 1 < wcap) lstrcatW(wout, line);
}

static const char* runJwt(const char* token) {
    wchar_t wout[8192];
    char t2[4200];
    static char part0[2048], part1[2048];
    char decoded[2048];
    int i, ndot = 0;
    char* cut;
    if (!*token) {
        return ansWide(L"jwt: usage: jwt <token> (or jwt decode <token>)");
    }
    lstrcpynA(t2, token, sizeof(t2));
    {   size_t n = lstrlenA(t2);
        while (n && (t2[n - 1] == ' ' || t2[n - 1] == '\t')) t2[--n] = 0;
    }
    for (i = 0; t2[i]; ++i) if (t2[i] == '.') ++ndot;
    if (ndot < 1)
        return ansWide(L"jwt: not a token - expected header.payload[.signature]");

    cut = strchr(t2, '.');
    *cut = 0;
    lstrcpynA(part0, t2, sizeof(part0));
    lstrcpynA(part1, cut + 1, sizeof(part1));
    {   char* c2 = strchr(part1, '.');
        if (c2) *c2 = 0;
    }

    wout[0] = 0;
    for (i = 0; i < 2; ++i) {
        const char* part = (i == 0) ? part0 : part1;
        const char* p = decoded;
        Out o;
        wchar_t wpart[4096];
        wchar_t head[24];
        if (!b64Decode(part, decoded, sizeof(decoded)))
            return ansWide(i == 0 ? L"jwt: header is not valid base64url"
                                  : L"jwt: payload is not valid base64url");
        oInit(&o, g_body, sizeof(g_body));
        if (!emitVal(&p, &o, 0, 1, 0) || !atEnd(&p))
            return ansWide(i == 0 ? L"jwt: header is not valid JSON"
                                  : L"jwt: payload is not valid JSON");
        if (o.overflow) return ansWide(L"jwt: decoded part too long");
        oFinish(&o);
        /* RFC 7519: JWT bodies are UTF-8 -> wide, then one CP_ACP answer */
        MultiByteToWideChar(CP_UTF8, 0, g_body, -1, wpart, 4096);
        wsprintfW(head, i == 0 ? L"header:\n" : L"payload:\n");
        if (lstrlenW(wout) + lstrlenW(head) + lstrlenW(wpart) + 2 < 8192) {
            if (wout[0]) lstrcatW(wout, L"\n");
            lstrcatW(wout, head);
            lstrcatW(wout, wpart);
        }
    }
    if (b64Decode(part1, decoded, sizeof(decoded))) {
        long long exp = jsonNum(decoded, "exp");
        long long iat = jsonNum(decoded, "iat");
        if (iat >= 0) addClockLine(wout, 8192, L"iat", iat);
        if (exp >= 0) addClockLine(wout, 8192, L"exp", exp);
    }
    {
        static wchar_t full[8320];
        lstrcpyW(full, L"jwt:\n");
        lstrcatW(full, wout);
        return ansWide(full);
    }
}

/* -------------------------------- dispatch ---------------------------------- */

static const char* dispatch(const char* line) {
    if (_strnicmp(line, "json", 4) == 0 && (line[4] == 0 || line[4] == ' ')) {
        const char* arg = line + 4;
        while (*arg == ' ') ++arg;
        if (!_stricmp(arg, "fmt")) return ansMsg("usage: json fmt <text>");
        if (_strnicmp(arg, "fmt ", 4) == 0) return runFmt(arg + 4, 1);
        if (!_stricmp(arg, "min") || !_stricmp(arg, "minify"))
            return ansMsg("usage: json min <text>");
        if (_strnicmp(arg, "min ", 4) == 0) return runFmt(arg + 4, 0);
        if (_strnicmp(arg, "minify ", 7) == 0) return runFmt(arg + 7, 0);
        if (!_stricmp(arg, "keys")) return ansMsg("usage: json keys <text>");
        if (_strnicmp(arg, "keys ", 5) == 0) return runKeys(arg + 5);
        if (!_stricmp(arg, "get")) return ansMsg("usage: json get <path> <text>");
        if (_strnicmp(arg, "get ", 4) == 0) return runGet(arg + 4);
        return ansWide(L"json: unknown subcommand (fmt|min|get|keys)");
    }
    if (_strnicmp(line, "jwt", 3) == 0 && (line[3] == 0 || line[3] == ' ')) {
        const char* arg = line + 3;
        while (*arg == ' ') ++arg;
        if (_strnicmp(arg, "decode ", 7) == 0) { arg += 7; while (*arg == ' ') ++arg; }
        return runJwt(arg);
    }
    return NULL;
}

/* ----------------------------------- ABI ------------------------------------ */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
}

PLUGIN_API void __cdecl Unload(void) { }

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Json";
    i.version = "1.0";
    i.description = "json fmt|min|get <path>|keys <text> + jwt <token> decode - pure offline computation";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 2; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[2] = { "json", "jwt" };
    return (n >= 0 && n < 2) ? k[n] : NULL;
}

static char g_etRows[6][128];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 6) lstrcpynA(g_etRows[g_etCount++], r, 128); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int len;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    len = lstrlenA(input);
    if (len <= 4 && _strnicmp(input, "json", (size_t)len) == 0) {
        etAdd("json fmt {}");
        etAdd("json min {}");
        etAdd("json get a.b {}");
        etAdd("json keys {}");
    } else if (len > 4 && _strnicmp(input, "json ", 5) == 0) {
        etAdd("json fmt {}");
        etAdd("json min {}");
        etAdd("json get a.b {}");
        etAdd("json keys {}");
    } else if (len <= 3 && _strnicmp(input, "jwt", (size_t)len) == 0) {
        etAdd("jwt decode <token>");
    } else if (len > 3 && _strnicmp(input, "jwt ", 4) == 0) {
        etAdd("jwt decode <token>");
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
