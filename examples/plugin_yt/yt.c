/* yt.dll - TypeBox YouTube search plugin (v0.26.0, user request).
 *
 *   yt <query>      top videos right in the console:  1) title (12:34) channel | ...
 *   yt <n>          open the n-th result of the last search in the browser
 *   yt /clear       forget the cached results
 *
 * Network policy (the project's offline-first rule): NOTHING here touches
 * the network at Load() time, on a timer, or in the background - a request
 * goes out only when you type `yt <query>` and press Enter, exactly like
 * the host's /update. Three tiers, tried in order:
 *   1. [Youtube] ApiKey=...  -> YouTube Data API v3 (googleapis.com): the
 *      official road, titles/channels guaranteed (durations need a second
 *      call, so the keyless tiers below actually show more).
 *   2. keyless public instances: Piped (/search?q=..&filter=videos) and
 *      Invidious (/api/v1/search?q=..&type=video) JSON - short timeout
 *      each, first one to answer wins. [Youtube] Instances= overrides the
 *      baked-in list (comma-separated base URLs, both flavours accepted).
 *   3. last resort, always works: open the YouTube results page in the
 *      default browser and say so in the answer line.
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef YT_BUILDING_DLL
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

#define YT_MAX 12

static HWND    g_mainWnd = NULL;
static char    g_apiKey[256] = "";
static wchar_t g_instances[8][256];      /* base URLs, keyless tier        */
static int     g_instCount = 0;
static int     g_top = 7;                /* rows to show                   */
static DWORD   g_timeoutMs = 6000;       /* per backend, both directions   */

/* last search, so `yt 3` can open it */
static struct { wchar_t id[64]; wchar_t title[256]; } g_last[YT_MAX];
static int g_lastCount = 0;

/* ------------------------------- config ------------------------------------ */

static void trimA(char* s) {
    size_t n;
    while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
}

static int g_headless = 0;  /* v0.27: env TYPEBOX_HEADLESS=1 or [UI] Headless=1:
                             * browser opens go minimized+inactive, no focus steal */

static void loadConfig(void) {
    wchar_t ini[MAX_PATH] = {}, iniPath[MAX_PATH], buf[2048], *ctx, *tok;
    int i;
    /* The HOST exe anchors the portable INI (the note.dll lesson). */
    GetModuleFileNameW(NULL, ini, MAX_PATH);
    { wchar_t* bs = wcsrchr(ini, L'\\'); if (bs) bs[1] = 0; }
    wsprintfW(iniPath, L"%sTypeBox.ini", ini);
    {
        wchar_t ev[8]; ev[0] = 0;
        GetEnvironmentVariableW(L"TYPEBOX_HEADLESS", ev, 7);
        g_headless = (ev[0] == L'1') ||
                     (int)GetPrivateProfileIntW(L"UI", L"Headless", 0, iniPath);
    }

    {
        wchar_t w[256];
        GetPrivateProfileStringW(L"Youtube", L"ApiKey", L"", w, 256, iniPath);
        WideCharToMultiByte(CP_ACP, 0, w, -1, g_apiKey, sizeof(g_apiKey), NULL, NULL);
        trimA(g_apiKey);
    }
    g_top = (int)GetPrivateProfileIntW(L"Youtube", L"Top", 7, iniPath);
    if (g_top < 3) g_top = 3;
    if (g_top > YT_MAX) g_top = YT_MAX;
    g_timeoutMs = (DWORD)GetPrivateProfileIntW(L"Youtube", L"TimeoutMs", 6000, iniPath);
    if (g_timeoutMs < 1500) g_timeoutMs = 1500;
    if (g_timeoutMs > 15000) g_timeoutMs = 15000;

    g_instCount = 0;
    GetPrivateProfileStringW(L"Youtube", L"Instances", L"", buf, 2048, iniPath);
    for (ctx = buf;;) {
        while (*ctx == L' ' || *ctx == L',' || *ctx == L';') ++ctx;
        if (!*ctx) break;
        tok = ctx;
        while (*ctx && *ctx != L',' && *ctx != L';') ++ctx;
        {
            wchar_t c = *ctx;
            *ctx = 0;
            {
                size_t n = lstrlenW(tok);
                while (n && tok[n - 1] == L' ') tok[--n] = 0;
                if (n && g_instCount < 8) lstrcpynW(g_instances[g_instCount++], tok, 256);
            }
            if (!c) break;
            ++ctx;
        }
    }
    if (!g_instCount) {   /* verified-working first (2026-11), classics last */
        static const wchar_t* dflt[6] = {
            L"https://api.piped.private.coffee", L"https://pipedapi.ducks.party",
            L"https://yt.chocolatemoo53.com",    L"https://invidious.materialio.us",
            L"https://pipedapi.kavin.rocks",     L"https://pipedapi.adminforge.de"
        };
        for (i = 0; i < 6; ++i) lstrcpynW(g_instances[g_instCount++], dflt[i], 256);
    }
}

/* ------------------------------- tiny JSON ---------------------------------- */

/* Find  "key":"value"  at/after *from; decode JSON escapes (incl. \uXXXX with
 * surrogate pairs) into UTF-8. Advances *from past the value; 1 = found. */
static int jsonStr(const char* body, const char** from, const char* key, char* out, size_t cap) {
    const char* p = *from;
    while ((p = strstr(p, key)) != NULL) {
        size_t n = 0;
        p += strlen(key);
        while (*p == ' ' || *p == '\t') ++p;
        if (*p != ':') continue;
        ++p;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p != '"') continue;
        ++p;
        while (*p && *p != '"') {
            unsigned int cp;
            if (*p == '\\') {
                ++p;
                switch (*p) {
                case 'n':  cp = '\n';  ++p; break;
                case 'r':  cp = '\r';  ++p; break;
                case 't':  cp = '\t';  ++p; break;
                case 'b':  cp = '\b';  ++p; break;
                case 'f':  cp = '\f';  ++p; break;
                case '/':  cp = '/';   ++p; break;
                case '"':  cp = '"';   ++p; break;
                case '\\': cp = '\\';  ++p; break;
                case 'u': {
                    int i;
                    char hx[5] = { 0, 0, 0, 0, 0 };
                    ++p;
                    for (i = 0; i < 4 && *p && isxdigit((unsigned char)*p); ++i) hx[i] = *p++;
                    cp = (unsigned int)strtoul(hx, NULL, 16);
                    if (cp >= 0xD800 && cp <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {
                        unsigned int lo = 0;
                        p += 2;
                        for (i = 0; i < 4 && *p && isxdigit((unsigned char)*p); ++i) {
                            char h2[2]; h2[0] = *p++; h2[1] = 0;
                            lo = lo * 16 + (unsigned int)strtoul(h2, NULL, 16);
                        }
                        if (lo >= 0xDC00 && lo <= 0xDFFF)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    break;
                }
                default:   cp = (unsigned char)*p; ++p; break;
                }
            } else {
                /* raw JSON byte: the body is already UTF-8 - copy verbatim,
                 * re-encoding a continuation byte here would double-encode it
                 * (this is why Cyrillic titles used to render as mojibake) */
                if (n + 1 < cap) out[n++] = *p++;
                continue;
            }
            if (cp < 0x80) {
                if (n + 1 < cap) out[n++] = (char)cp;
            } else if (cp < 0x800) {
                if (n + 2 < cap) { out[n++] = (char)(0xC0 | (cp >> 6)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
            } else if (cp < 0x10000) {
                if (n + 3 < cap) { out[n++] = (char)(0xE0 | (cp >> 12)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
            } else {
                if (n + 4 < cap) { out[n++] = (char)(0xF0 | (cp >> 18)); out[n++] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
            }
        }
        out[n] = 0;
        *from = p;
        return 1;
    }
    return 0;
}

static int jsonNum(const char* body, const char** from, const char* key, long* val) {
    const char* p = *from;
    while ((p = strstr(p, key)) != NULL) {
        char* end;
        p += strlen(key);
        while (*p == ' ' || *p == '\t') ++p;
        if (*p != ':') continue;
        ++p;
        while (*p == ' ' || *p == '\t') ++p;
        *val = strtol(p, &end, 10);
        if (end != p) { *from = end; return 1; }
    }
    return 0;
}

/* Split the JSON array of objects starting near `arr` into element strings
 * (string-aware brace counting), calling cb for each; cb returns 1 to stop. */
typedef int (*ObjCb)(const char* obj, void* ctx);
static void forEachObject(const char* arr, ObjCb cb, void* ctx) {
    const char* p = strchr(arr, '[');
    if (!p) return;
    ++p;
    for (;;) {
        const char* start;
        char* obj;
        int depth = 0, stop;
        size_t len;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') ++p;
        if (*p != '{') return;                       /* ']' or end: done      */
        start = p;
        while (*p) {
            if (*p == '"') {
                ++p;
                while (*p && *p != '"') { if (*p == '\\') ++p; if (*p) ++p; }
                if (*p == '"') ++p;
                continue;
            }
            if (*p == '{') ++depth;
            else if (*p == '}') { if (!--depth) { ++p; break; } }
            ++p;
        }
        len = (size_t)(p - start);
        obj = (char*)malloc(len + 1);
        if (!obj) return;
        memcpy(obj, start, len);
        obj[len] = 0;
        stop = cb(obj, ctx);
        free(obj);
        if (stop) return;
    }
}

/* ------------------------------- HTTP (winhttp) ----------------------------- */

static char* httpGet(const wchar_t* host, const wchar_t* path) {
    char* body = NULL;
    size_t cap = 0, len = 0;
    HINTERNET ses = WinHttpOpen(L"TypeBox-yt/1.0 (offline-first; user-typed search)",
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (ses) {
        HINTERNET con = NULL, req = NULL;
        WinHttpSetTimeouts(ses, (int)g_timeoutMs, (int)g_timeoutMs,
                           (int)g_timeoutMs, (int)g_timeoutMs);
        con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (con) req = WinHttpOpenRequest(con, L"GET", path, NULL, NULL, NULL,
                                          WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
        if (req) {
            static const wchar_t hdr[] = L"Accept: application/json\r\n";
            DWORD code = 0, sz = sizeof(code);
            if (WinHttpAddRequestHeaders(req, hdr, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD) &&
                WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) &&
                WinHttpReceiveResponse(req, NULL) &&
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    NULL, &code, &sz, NULL) && code == 200) {
                for (;;) {
                    DWORD avail = 0, got = 0;
                    if (!WinHttpQueryDataAvailable(req, &avail) || !avail) break;
                    if (len + avail + 1 > cap) {
                        char* nb;
                        if (len + avail + 1 > 768 * 1024) break;       /* answer cap  */
                        cap = (len + avail + 1) * 2;
                        nb = (char*)realloc(body, cap);
                        if (!nb) break;
                        body = nb;
                    }
                    if (!WinHttpReadData(req, body + len, avail, &got) || !got) break;
                    len += got;
                }
            }
        }
        if (req) WinHttpCloseHandle(req);
        if (con) WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
    }
    if (body) body[len] = 0;
    return body;
}

static void urlEncodeUtf8(const wchar_t* w, char* out, size_t cap) {
    char u8[2048];
    size_t n = 0;
    int i;
    out[0] = 0;
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, u8, sizeof(u8), NULL, NULL) <= 0) return;
    for (i = 0; u8[i] && n + 4 < cap; ++i) {
        unsigned char c = (unsigned char)u8[i];
        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') out[n++] = (char)c;
        else { wsprintfA(out + n, "%%%02X", c); n += 3; }
    }
    out[n] = 0;
}

/* ------------------------------- result model -------------------------------- */

typedef struct { char id[64]; char title[512]; char author[256]; long seconds; } Item;
typedef struct { Item items[YT_MAX]; int count; } Ctx;

static int keepItem(Ctx* c, const Item* it) {
    if (!it->id[0] || !it->title[0]) return 0;
    if (c->count < YT_MAX) { c->items[c->count++] = *it; return 1; }
    return 2;                                                /* stop */
}

/* Piped item: "url":"/watch?v=ID", "title", "duration", "uploaderName" */
static int cbPiped(const char* obj, void* vc) {
    Ctx* c = (Ctx*)vc;
    Item it;
    char url[128];
    const char* cur;
    memset(&it, 0, sizeof(it));
    cur = obj;
    if (!jsonStr(obj, &cur, "\"url\"", url, sizeof(url))) return 0;
    {
        char* v = strstr(url, "v=");
        if (!v) return 0;
        lstrcpynA(it.id, v + 2, sizeof(it.id));
    }
    cur = obj; jsonStr(obj, &cur, "\"title\"", it.title, sizeof(it.title));
    cur = obj; jsonStr(obj, &cur, "\"uploaderName\"", it.author, sizeof(it.author));
    cur = obj; if (!jsonNum(obj, &cur, "\"duration\"", &it.seconds)) it.seconds = 0;
    return keepItem(c, &it) == 2;
}

/* Invidious video: "videoId", "title", "author", "lengthSeconds" */
static int cbInvidious(const char* obj, void* vc) {
    Ctx* c = (Ctx*)vc;
    Item it;
    const char* cur;
    memset(&it, 0, sizeof(it));
    cur = obj;
    if (!jsonStr(obj, &cur, "\"videoId\"", it.id, sizeof(it.id))) return 0;
    cur = obj; jsonStr(obj, &cur, "\"title\"", it.title, sizeof(it.title));
    cur = obj;
    if (!jsonStr(obj, &cur, "\"author\"", it.author, sizeof(it.author))) {
        cur = obj; jsonStr(obj, &cur, "\"authorText\"", it.author, sizeof(it.author));
    }
    cur = obj; if (!jsonNum(obj, &cur, "\"lengthSeconds\"", &it.seconds)) it.seconds = 0;
    return keepItem(c, &it) == 2;
}

/* Data API v3 item: "id":{"videoId":".."}, "snippet":{"title","channelTitle"} */
static int cbDataApi(const char* obj, void* vc) {
    Ctx* c = (Ctx*)vc;
    Item it;
    const char* cur;
    memset(&it, 0, sizeof(it));
    cur = obj;
    if (!jsonStr(obj, &cur, "\"videoId\"", it.id, sizeof(it.id))) return 0;
    cur = obj;
    if (!jsonStr(obj, &cur, "\"title\"", it.title, sizeof(it.title))) return 0;
    cur = obj; jsonStr(obj, &cur, "\"channelTitle\"", it.author, sizeof(it.author));
    it.seconds = -1;                                  /* not fetched */
    return keepItem(c, &it) == 2;
}

/* ------------------------------- backends ------------------------------------ */

static int tryDataApi(const wchar_t* qw, Ctx* c) {
    wchar_t pathW[2600];
    char enc[2048], pathA[2600];
    char* body;
    const char* items;
    urlEncodeUtf8(qw, enc, sizeof(enc));
    wsprintfA(pathA, "/youtube/v3/search?part=snippet&type=video&maxResults=%d&q=%s&key=%s",
              g_top, enc, g_apiKey);
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, pathW, 2600);
    body = httpGet(L"www.googleapis.com", pathW);
    if (!body) return 0;
    items = strstr(body, "\"items\"");
    if (items) forEachObject(items, cbDataApi, c);
    free(body);
    return c->count > 0;
}

/* probe one instance: Piped flavour first, then Invidious, same host */
static int tryInstance(const wchar_t* base, const wchar_t* qw, Ctx* c) {
    wchar_t host[256], pathW[2200];
    const wchar_t* s = base;
    char enc[2048], pathA[2200];
    char* body;
    const char* p;
    int i = 0;
    if (!_wcsnicmp(s, L"https://", 8)) s += 8;
    else if (!_wcsnicmp(s, L"http://", 7)) s += 7;
    while (*s && *s != L'/' && i < 255) host[i++] = *s++;
    host[i] = 0;
    if (!host[0]) return 0;

    urlEncodeUtf8(qw, enc, sizeof(enc));
    wsprintfA(pathA, "/search?q=%s&filter=videos", enc);
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, pathW, 2200);
    body = httpGet(host, pathW);
    if (body) {
        p = strstr(body, "\"items\"");
        if (p) forEachObject(p, cbPiped, c);
        free(body);
        if (c->count) return 1;
    }
    urlEncodeUtf8(qw, enc, sizeof(enc));
    wsprintfA(pathA, "/api/v1/search?q=%s&type=video", enc);
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, pathW, 2200);
    body = httpGet(host, pathW);
    if (body) {
        if (body[0] == '[') forEachObject(body, cbInvidious, c);
        free(body);
    }
    return c->count > 0;
}

/* ------------------------------- answers ------------------------------------- */

static char g_answer[4096];

static void catA(char* dst, const char* src) {            /* bounded on dst */
    size_t cap = 4096, n = strlen(dst);
    if (n + 1 < cap) lstrcpynA(dst + n, src, (int)(cap - n));
}
static void answerWide(const wchar_t* w) {
    if (WideCharToMultiByte(CP_ACP, 0, w, -1, g_answer, sizeof(g_answer), NULL, NULL) <= 0)
        g_answer[0] = 0;
}

static void fmtDuration(long sec, wchar_t* out, size_t cap) {
    wchar_t tmp[32];
    if (sec < 0)          lstrcpynW(out, L"", 1);            /* no data     */
    else if (sec == 0)    lstrcpynW(out, L"live", cap);
    else if (sec >= 3600) { wsprintfW(tmp, L"%d:%02d:%02d", (int)(sec / 3600), (int)(sec / 60) % 60, (int)(sec % 60)); lstrcpynW(out, tmp, (int)cap); }
    else                  { wsprintfW(tmp, L"%d:%02d", (int)(sec / 60), (int)(sec % 60)); lstrcpynW(out, tmp, (int)cap); }
}

static void catW(wchar_t* dst, size_t cap, const wchar_t* src) {
    size_t n = lstrlenW(dst);
    if (n + 1 < cap) lstrcpynW(dst + n, src, (int)(cap - n));
}

/* Render the console line from Ctx and cache the rows for `yt <n>`. */
static void render(Ctx* c) {
    static wchar_t line[3072];
    int i, shown = c->count < g_top ? c->count : g_top;
    line[0] = 0;
    catW(line, 3072, L"yt: ");
    g_lastCount = shown;
    for (i = 0; i < shown; ++i) {
        Item* it = &c->items[i];
        wchar_t tbuf[512], abuf[256], dur[32], base[1200], tmp[1300], idw[80];
        if (i) catW(line, 3072, L"  |  ");
        if (MultiByteToWideChar(CP_UTF8, 0, it->title, -1, tbuf, 512) <= 0) tbuf[0] = 0;
        if (lstrlenW(tbuf) > 64) tbuf[64] = 0;
        if (MultiByteToWideChar(CP_UTF8, 0, it->author, -1, abuf, 256) <= 0) abuf[0] = 0;
        if (lstrlenW(abuf) > 28) abuf[28] = 0;
        fmtDuration(it->seconds, dur, 32);
        wsprintfW(base, L"%d) %s", i + 1, tbuf);
        if (dur[0]) { wsprintfW(tmp, L"%s (%ls)", base, dur); lstrcpynW(base, tmp, 1200); }
        if (abuf[0]) { wsprintfW(tmp, L"%s %s", base, abuf); lstrcpynW(base, tmp, 1200); }
        catW(line, 3072, base);
        MultiByteToWideChar(CP_ACP, 0, it->id, -1, idw, 80);
        lstrcpynW(g_last[i].id, idw, 64);
        if (MultiByteToWideChar(CP_UTF8, 0, it->title, -1, g_last[i].title, 256) <= 0)
            g_last[i].title[0] = 0;
        if (lstrlenW(g_last[i].title) > 120) g_last[i].title[120] = 0;
    }
    answerWide(line);
}

/* ------------------------------- commands ------------------------------------ */

static void openResultsInBrowser(const wchar_t* qw, const wchar_t* reason) {
    char enc[2048];
    wchar_t url[2300], msg[2400];
    urlEncodeUtf8(qw, enc, sizeof(enc));
    wsprintfW(url, L"https://www.youtube.com/results?search_query=%hs", enc);
    ShellExecuteW(NULL, L"open", url, NULL, NULL,
                  g_headless ? SW_SHOWMINNOACTIVE : SW_SHOWNORMAL);
    wsprintfW(msg, L"yt: %s - opened the results in your browser", reason);
    answerWide(msg);
}

static const char* runYt(const char* rest) {
    const char* p = rest;
    wchar_t qw[1024];
    while (*p == ' ') ++p;
    if (!g_instCount) loadConfig();
    g_answer[0] = 0;

    if (!*p) {
        lstrcpynA(g_answer, "yt: type `yt <query>` to search, `yt <n>` opens result n, `yt /clear` forgets them", sizeof(g_answer));
        return g_answer;
    }
    if (_stricmp(p, "/clear") == 0) {
        g_lastCount = 0;
        lstrcpynA(g_answer, "yt: cached results cleared", sizeof(g_answer));
        return g_answer;
    }

    /* pure number -> open from the cached results */
    {
        const char* q;
        int allDigits = 1, n;
        for (q = p; *q; ++q) if (*q < '0' || *q > '9') { allDigits = 0; break; }
        if (!allDigits) goto search;
        n = atoi(p);
        if (n < 1 || n > g_lastCount) {
            wchar_t m[160];
            wsprintfW(m, L"yt: nothing cached for #%d - run `yt <query>` first (n = 1..%d)", n, g_lastCount);
            answerWide(m);
            return g_answer;
        }
        {
            wchar_t url[160], msg[420];
            const wchar_t* vid = g_last[n - 1].id;
            wsprintfW(url, L"https://www.youtube.com/watch?v=%s", vid);
            ShellExecuteW(NULL, L"open", url, NULL, NULL,
                          g_headless ? SW_SHOWMINNOACTIVE : SW_SHOWNORMAL);
            wsprintfW(msg, L"yt: opened #%d %s", n, g_last[n - 1].title);
            answerWide(msg);
        }
        return g_answer;
    }

search:
    if (MultiByteToWideChar(CP_ACP, 0, p, -1, qw, 1024) <= 0) {
        lstrcpynA(g_answer, "yt: bad query", sizeof(g_answer));
        return g_answer;
    }
    {
        Ctx c;
        int i, apiFailed = 0;
        memset(&c, 0, sizeof(c));
        if (g_apiKey[0]) {
            if (tryDataApi(qw, &c)) { render(&c); return g_answer; }
            apiFailed = 1;
            memset(&c, 0, sizeof(c));
        }
        for (i = 0; i < g_instCount; ++i) {
            memset(&c, 0, sizeof(c));
            if (tryInstance(g_instances[i], qw, &c)) {
                render(&c);
                if (apiFailed) catA(g_answer, " (Data API failed, instances answered)");
                return g_answer;
            }
        }
        openResultsInBrowser(qw, apiFailed ? L"Data API and every public instance failed"
                                           : L"every public instance is silent");
        return g_answer;
    }
}

/* ----------------------------------- ABI ------------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
}
PLUGIN_API void __cdecl Unload(void) { }

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Youtube";
    i.version = "1.0";
    i.description = "youtube video search in the console \u00b7 yt <query> / yt <n> / yt /clear \u00b7 optional [Youtube] ApiKey, keyless instances, browser fallback";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "yt" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

static char g_etRows[3][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 3) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int len;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    len = lstrlenA(input);
    if (len <= 2 && _strnicmp(input, "yt", len) == 0) {
        etAdd("yt /clear");
    } else if (len >= 3 && _strnicmp(input, "yt ", 3) == 0 &&
               _strnicmp(input + 3, "/clear", (len - 3 < 6) ? (size_t)(len - 3) : 6) == 0) {
        etAdd("yt /clear");
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
    if (_strnicmp(str, "yt", 2) == 0 && (str[2] == 0 || str[2] == ' '))
        text = runYt(str + 2);
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = text;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(text);
    return ex;
}
