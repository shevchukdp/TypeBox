/* weather.dll - TypeBox weather plugin (v0.28.0, user request).
 *
 *   weather              last place (cached); else [Weather] City; else IP-geo
 *   weather <city>       one line: now + today/tomorrow (Open-Meteo, keyless)
 *   weather /clear       forget the cached place and reading
 *
 * Network policy (the project's offline-first rule): NOTHING here touches
 * the network at Load() time, on a timer, or in the background - a request
 * goes out only when you type a `weather` line and press Enter, exactly
 * like yt.dll and the host's /update. Sources (all keyless):
 *   1. geocoding-api.open-meteo.com  city name  -> latitude/longitude
 *   2. api.open-meteo.com            forecast: current + 2 days
 *   3. ipwho.is                      IP geolocation, only when no city known
 * No network / unreachable -> an honest one-line answer, never a hang
 * (WinHttpSetTimeouts caps every call).
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WEATHER_BUILDING_DLL
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

static HWND  g_mainWnd = NULL;
static wchar_t g_defCity[128] = L"";     /* [Weather] City                    */
static DWORD g_timeoutMs = 6000;          /* every direction, every call       */
static int   g_imperial = 0;              /* [Weather] Units = im              */
static int   g_russian = 0;               /* [UI] Language ru / auto+RU locale */

/* cached reading so a bare `weather` is instant and needs no network */
static wchar_t g_place[192] = L"";        /* "Kyiv, Ukraine"                   */
static wchar_t g_line[1024] = L"";        /* last rendered answer              */
static double  g_lat = 0, g_lon = 0;
static int     g_haveGeo = 0;

/* --------------------------------- config ---------------------------------- */

static void loadConfig(void) {
    wchar_t ini[MAX_PATH] = {}, iniPath[MAX_PATH], lang[16];
    GetModuleFileNameW(NULL, ini, MAX_PATH);
    { wchar_t* bs = wcsrchr(ini, L'\\'); if (bs) bs[1] = 0; }
    wsprintfW(iniPath, L"%sTypeBox.ini", ini);
    GetPrivateProfileStringW(L"Weather", L"City", L"", g_defCity, 128, iniPath);
    {
        wchar_t u[8];
        GetPrivateProfileStringW(L"Weather", L"Units", L"m", u, 8, iniPath);
        g_imperial = (_wcsicmp(u, L"im") == 0);
    }
    g_timeoutMs = (DWORD)GetPrivateProfileIntW(L"Weather", L"TimeoutMs", 6000, iniPath);
    if (g_timeoutMs < 1500) g_timeoutMs = 1500;
    if (g_timeoutMs > 15000) g_timeoutMs = 15000;

    GetPrivateProfileStringW(L"UI", L"Language", L"auto", lang, 16, iniPath);
    if (_wcsicmp(lang, L"ru") == 0) g_russian = 1;
    else if (_wcsicmp(lang, L"auto") == 0 &&
             PRIMARYLANGID(LANGIDFROMLCID(GetUserDefaultUILanguage())) == LANG_RUSSIAN)
        g_russian = 1;
}

/* -------------------------------- tiny JSON -------------------------------- */

/* Find "key":"value" at/after *from; decode \uXXXX (surrogate pairs) to UTF-8. */
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
                case 'n': cp = '\n'; ++p; break;
                case 'r': cp = '\r'; ++p; break;
                case 't': cp = '\t'; ++p; break;
                case '/': cp = '/';  ++p; break;
                case '"': cp = '"';  ++p; break;
                case '\\': cp = '\\'; ++p; break;
                case 'u': {
                    int i; char hx[5] = {0,0,0,0,0};
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
                default: cp = (unsigned char)*p; ++p; break;
                }
            } else {
                /* raw JSON byte: the body is already UTF-8 - copy verbatim,
                 * re-encoding a continuation byte here would double-encode it */
                if (n + 1 < cap) out[n++] = *p++;
                continue;
            }
            if (cp < 0x80) { if (n + 1 < cap) out[n++] = (char)cp; }
            else if (cp < 0x800) { if (n + 2 < cap) { out[n++] = (char)(0xC0 | (cp >> 6)); out[n++] = (char)(0x80 | (cp & 0x3F)); } }
            else if (cp < 0x10000) { if (n + 3 < cap) { out[n++] = (char)(0xE0 | (cp >> 12)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); } }
            else { if (n + 4 < cap) { out[n++] = (char)(0xF0 | (cp >> 18)); out[n++] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); } }
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

/* "key": 7.4 (or 7) -> double */
static int jsonFloat(const char* body, const char** from, const char* key, double* val) {
    const char* p = *from;
    while ((p = strstr(p, key)) != NULL) {
        char* end;
        p += strlen(key);
        while (*p == ' ' || *p == '\t') ++p;
        if (*p != ':') continue;
        ++p;
        while (*p == ' ' || *p == '\t') ++p;
        *val = strtod(p, &end);
        if (end != p) { *from = end; return 1; }
    }
    return 0;
}

/* "key":[a,b,...] -> first two numbers (max/min of the daily arrays) */
static int jsonPair(const char* body, const char* key, double* a, double* b) {
    const char* p = strstr(body, key);
    if (!p) return 0;
    p += strlen(key);
    while (*p && *p != '[') ++p;
    if (*p != '[') return 0;
    ++p;
    {
        char* end;
        *a = strtod(p, &end);
        if (end == p) return 0;
        p = end;
        while (*p && *p != ',') ++p;
        if (*p != ',') { *b = *a; return 1; }
        ++p;
        *b = strtod(p, &end);
        if (end == p) { *b = *a; return 1; }
        return 1;
    }
}

/* ------------------------------ HTTP (winhttp) ----------------------------- */

static char* httpGet(const wchar_t* host, const wchar_t* path) {
    char* body = NULL;
    size_t cap = 0, len = 0;
    HINTERNET ses = WinHttpOpen(L"TypeBox-weather/1.0 (offline-first; user-typed query)",
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
                        if (len + avail + 1 > 128 * 1024) break;      /* forecast json is small */
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

/* --------------------------- WMO codes -> words ---------------------------- */

typedef struct { int lo, hi; const wchar_t* en; const wchar_t* ru; } WmoM;

static const wchar_t* wmoLabel(int code) {
    static const WmoM m[] = {
        { 0,   0,  L"clear",            L"ясно" },
        { 1,   1,  L"mostly clear",     L"в основном ясно" },
        { 2,   2,  L"partly cloudy",    L"переменная облачность" },
        { 3,   3,  L"overcast",         L"пасмурно" },
        { 45,  48, L"fog",              L"туман" },
        { 51,  57, L"drizzle",          L"морось" },
        { 61,  65, L"rain",             L"дождь" },
        { 66,  67, L"freezing rain",    L"ледяной дождь" },
        { 71,  77, L"snow",             L"снег" },
        { 80,  82, L"showers",          L"дождь" },
        { 85,  86, L"snow showers",     L"снег" },
        { 95,  95, L"thunderstorm",     L"гроза" },
        { 96,  99, L"thunderstorm hail",L"гроза с градом" },
    };
    size_t i;
    for (i = 0; i < sizeof(m) / sizeof(m[0]); ++i)
        if (code >= m[i].lo && code <= m[i].hi)
            return g_russian ? m[i].ru : m[i].en;
    return g_russian ? L"погода" : L"weather";
}

/* --------------------------------- answers --------------------------------- */

static char g_answer[4096];

static void answerWide(const wchar_t* w) {
    if (WideCharToMultiByte(CP_ACP, 0, w, -1, g_answer, sizeof(g_answer), NULL, NULL) <= 0)
        g_answer[0] = 0;
}
static void catW(wchar_t* dst, size_t cap, const wchar_t* src) {
    size_t n = lstrlenW(dst);
    if (n + 1 < cap) lstrcpynW(dst + n, src, (int)(cap - n));
}

/* --------------------------------- sources --------------------------------- */

/* geocoding-api.open-meteo.com: name -> lat/lon + display place */
static int geoCity(const wchar_t* city) {
    wchar_t pathW[2300];
    char enc[1024], pathA[1600], *body;
    const char* cur;
    char name[192] = "", country[96] = "";
    double la = 0, lo = 0;
    urlEncodeUtf8(city, enc, sizeof(enc));
    wsprintfA(pathA, "/v1/search?name=%s&count=1&language=%s&format=json",
              enc, g_russian ? "ru" : "en");
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, pathW, 2300);
    body = httpGet(L"geocoding-api.open-meteo.com", pathW);
    if (!body) return -1;                                 /* network problem  */
    cur = body;
    if (!jsonStr(body, &cur, "\"name\"", name, sizeof(name)) ||
        !jsonFloat(body, &cur, "\"latitude\"", &la)  ||
        !(cur = body) || !jsonFloat(body, &cur, "\"longitude\"", &lo)) {
        free(body);
        return 0;                                         /* no such place    */
    }
    cur = body; jsonStr(body, &cur, "\"country\"", country, sizeof(country));
    free(body);
    {
        wchar_t wname[224], wctry[128];
        if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 224) <= 0) return 0;
        if (MultiByteToWideChar(CP_UTF8, 0, country, -1, wctry, 128) <= 0) wctry[0] = 0;
        lstrcpynW(g_place, wname, 192);
        if (wctry[0]) {
            wchar_t tmp[256];
            wsprintfW(tmp, L", %s", wctry);
            catW(g_place, 192, tmp);
        }
    }
    g_lat = la; g_lon = lo; g_haveGeo = 1;
    return 1;
}

/* ipwho.is: last-resort geolocation so a bare `weather` can answer */
static int geoIp(void) {
    char* body = httpGet(L"ipwho.is", L"/");
    const char* cur;
    double la = 0, lo = 0;
    char city[192] = "", country[96] = "";
    if (!body) return -1;
    cur = body;
    if (!jsonFloat(body, &cur, "\"latitude\"", &la) ||
        !(cur = body) || !jsonFloat(body, &cur, "\"longitude\"", &lo)) {
        free(body);
        return 0;
    }
    cur = body; jsonStr(body, &cur, "\"city\"", city, sizeof(city));
    cur = body; jsonStr(body, &cur, "\"country\"", country, sizeof(country));
    free(body);
    {
        wchar_t wc1[224], wc2[128];
        if (MultiByteToWideChar(CP_UTF8, 0, city, -1, wc1, 224) <= 0) wc1[0] = 0;
        if (MultiByteToWideChar(CP_UTF8, 0, country, -1, wc2, 128) <= 0) wc2[0] = 0;
        if (!wc1[0]) lstrcpynW(g_place, g_russian ? L"IP-geo" : L"your location", 192);
        else {
            lstrcpynW(g_place, wc1, 192);
            if (wc2[0]) { wchar_t t[160]; wsprintfW(t, L", %s", wc2); catW(g_place, 192, t); }
        }
    }
    g_lat = la; g_lon = lo; g_haveGeo = 1;
    return 1;
}

/* render the one-line console answer for the cached coordinates */
static int forecast(void) {
    wchar_t pathW[1600];
    char pathA[1200], *body;
    const char* cur, *daily;
    long code = -1, dcode = -1;
    double tNow = 0, feels = 0, wind = 0, dmax = 0, dmin = 0, tmax2 = 0, tmin2 = 0;
    wchar_t line[1024], tmp[256];
    if (!g_haveGeo) return 0;
    /* wsprintf has no floating point - build the query with snprintf */
    snprintf(pathA, sizeof(pathA),
        "/v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        "&forecast_days=3&timezone=auto%s",
        g_lat, g_lon, g_imperial ? "&temperature_unit=fahrenheit&wind_speed_unit=mph" : "");
    MultiByteToWideChar(CP_ACP, 0, pathA, -1, pathW, 1600);
    body = httpGet(L"api.open-meteo.com", pathW);
    if (!body) return -1;

    cur = body;
    jsonFloat(body, &cur, "\"temperature_2m\"", &tNow);
    cur = body; jsonFloat(body, &cur, "\"apparent_temperature\"", &feels);
    cur = body; jsonNum(body,  &cur, "\"weather_code\"", &code);
    cur = body; jsonFloat(body, &cur, "\"wind_speed_10m\"", &wind);
    daily = strstr(body, "\"daily\"");
    if (daily) {
        cur = daily; jsonNum(daily, &cur, "\"weather_code\"", &dcode);
        if (!jsonPair(daily, "\"temperature_2m_max\"", &dmax, &tmax2)) dmin = dmax = 0;
        jsonPair(daily, "\"temperature_2m_min\"", &dmin, &tmin2);
    }
    free(body);

    line[0] = 0;
    wsprintfW(line, L"weather: %s ", g_place[0] ? g_place : L"?");
    wsprintfW(tmp, L"%d%s %s", (int)(tNow + (tNow >= 0 ? 0.5 : -0.5)),
              g_imperial ? L"\u00b0F" : L"\u00b0C", wmoLabel((int)code));
    catW(line, 1024, tmp);
    wsprintfW(tmp, L", feels %d%s", (int)(feels + (feels >= 0 ? 0.5 : -0.5)),
              g_imperial ? L"\u00b0F" : L"\u00b0C");
    catW(line, 1024, tmp);
    wsprintfW(tmp, L", wind %d %s", (int)(wind + 0.5),
              g_imperial ? L"mph" : L"km/h");
    catW(line, 1024, tmp);
    if (dcode >= 0) {
        int hi = (int)(dmax + (dmax >= 0 ? 0.5 : -0.5));
        int lo = (int)(dmin + (dmin >= 0 ? 0.5 : -0.5));
        wsprintfW(tmp, L" | %s %s%d/%s%d %s",
                  g_russian ? L"\u0441\u0435\u0433\u043e\u0434\u043d\u044f" : L"today",
                  hi >= 0 ? L"+" : L"", hi, lo >= 0 ? L"+" : L"", lo,
                  wmoLabel((int)dcode));
        catW(line, 1024, tmp);
        if (tmax2 != 0 || tmin2 != 0) {
            int hi2 = (int)(tmax2 + (tmax2 >= 0 ? 0.5 : -0.5));
            int lo2 = (int)(tmin2 + (tmin2 >= 0 ? 0.5 : -0.5));
            wsprintfW(tmp, L" | %s %s%d/%s%d",
                      g_russian ? L"\u0437\u0430\u0432\u0442\u0440\u0430" : L"tomorrow",
                      hi2 >= 0 ? L"+" : L"", hi2, lo2 >= 0 ? L"+" : L"", lo2);
            catW(line, 1024, tmp);
        }
    }
    lstrcpynW(g_line, line, 1024);
    answerWide(line);
    return 1;
}

/* -------------------------------- commands --------------------------------- */

static const char* runWeather(const char* rest) {
    const char* p = rest;
    while (*p == ' ') ++p;
    g_answer[0] = 0;

    if (!_stricmp(p, "/clear")) {
        g_line[0] = 0; g_haveGeo = 0;
        lstrcpynA(g_answer, "weather: cache cleared", sizeof(g_answer));
        return g_answer;
    }
    if (!*p) {
        if (g_line[0]) { answerWide(g_line); return g_answer; }   /* cached     */
        if (g_defCity[0]) {                                        /* [Weather]  */
            int r = geoCity(g_defCity);
            if (r == 1) { if (forecast() >= 0) return g_answer; }
            else if (r == -1) { lstrcpynA(g_answer,
                "weather: no network or open-meteo unreachable", sizeof(g_answer)); return g_answer; }
        }
        {
            int r = geoIp();
            if (r == 1) { if (forecast() >= 0) return g_answer; }
            else if (r == -1) { lstrcpynA(g_answer,
                "weather: no network - type `weather <city>` when online", sizeof(g_answer)); return g_answer; }
        }
        lstrcpynA(g_answer,
            "weather: place unknown - type `weather <city>` or set [Weather] City= in TypeBox.ini",
            sizeof(g_answer));
        return g_answer;
    }
    {
        wchar_t city[256];
        int r;
        if (MultiByteToWideChar(CP_ACP, 0, p, -1, city, 256) <= 0) {
            lstrcpynA(g_answer, "weather: bad city", sizeof(g_answer));
            return g_answer;
        }
        r = geoCity(city);
        if (r == 1) { if (forecast() >= 0) return g_answer; }
        if (r == -1) { lstrcpynA(g_answer, "weather: no network or open-meteo unreachable", sizeof(g_answer)); return g_answer; }
        lstrcpynA(g_answer, "weather: no such place - check the city name", sizeof(g_answer));
        return g_answer;
    }
}

/* ----------------------------------- ABI ----------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
}
PLUGIN_API void __cdecl Unload(void) { }

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Weather";
    i.version = "1.0";
    i.description = "one-line weather + 2-day forecast \u00b7 weather <city> / weather /clear \u00b7 Open-Meteo keyless, network only on a typed line";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "weather" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

static char g_etRows[2][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 2) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int len;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    len = lstrlenA(input);
    if (len <= 7 && _strnicmp(input, "weather", len) == 0) {
        etAdd("weather /clear");
    } else if (len >= 8 && _strnicmp(input, "weather ", 8) == 0 &&
               _strnicmp(input + 8, "/clear", (len - 8 < 6) ? (size_t)(len - 8) : 6) == 0) {
        etAdd("weather /clear");
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
    if (_strnicmp(str, "weather", 7) == 0 && (str[7] == 0 || str[7] == ' '))
        text = runWeather(str + 7);
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = text;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(text);
    return ex;
}
