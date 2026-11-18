/* nettools.dll - TypeBox network tools plugin (v0.30.0 wave, gallery #15).
 *
 *   ip                 public IP (checkip.amazonaws.com, cached [CacheSec])
 *   weather <city>     one short line from wttr.in format=3 (or [NetTools]
 *                      WeatherURL); last city remembered in [NetTools] LastCity
 *   w <city>           alias of weather
 *   dns <host>         resolve to addresses (GetAddrInfoW - works offline for
 *                      localhost / hosts-file names)
 *   netinfo            adapters: name, IPv4, mask, gateway (GetAdaptersInfo)
 *   ping <host> [n]    ICMP echo replies + round-trip stats
 *
 * Offline-first rule: NOTHING here touches the network at Load() or on a
 * timer - a request goes out only when the user types a line, exactly like
 * yt.dll / weather.dll. dns/netinfo answer without any network at all.
 * Every HTTP call is capped by WinHttpSetTimeouts ([NetTools] TimeoutMs).
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
/* MinGW order rule: winsock2 BEFORE windows.h; the ICMP helper API lives in
 * <icmpapi.h>/<ipexport.h> (types there, not re-declared here). */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <ipexport.h>
#include <icmpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Field order PROVEN on this machine (icmpprobe.c, 8.8.8.8 reply): the API
 * fills the OLD iphlpapi layout - Address, Status, RoundTripTime - so
 * mingw-w64's ipexport.h icmp_echo_reply is the truthful one (the
 * "Status-first" doc layout makes every reply look like a failure: offset 0
 * carries the replying address). Use MinGW's PICMP_ECHO_REPLY below. */
#ifndef IP_STATUS_SUCCESS
#define IP_STATUS_SUCCESS 0
#endif

#ifdef NETTOOLS_BUILDING_DLL
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
static DWORD g_timeoutMs = 3000;      /* every winhttp direction             */
static DWORD g_cacheSec = 60;         /* ip + weather answer cache           */
static int   g_pingCount = 4;
static wchar_t g_weatherUrl[256] = L"https://wttr.in/%s?format=3";
static wchar_t g_ipUrl[256] = L"https://checkip.amazonaws.com/";
static wchar_t g_lastCity[128] = L""; /* [NetTools] LastCity                 */

/* caches so a repeated `ip` / same-city `weather` is instant and offline */
static ULONGLONG g_ipAt = 0;          static wchar_t g_ipLine[128] = L"";
static ULONGLONG g_wAt = 0;           static wchar_t g_wCity[128] = L"", g_wLine[512] = L"";

static WSADATA g_wsa;                 /* WSAStartup at Load (icmp + getaddrinfo) */

static char g_out[4096];              /* con_text bytes (CP_ACP)             */

/* --------------------------------- config ---------------------------------- */

static void iniPath(wchar_t* out) {
    wchar_t dir[MAX_PATH] = {};
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    { wchar_t* bs = wcsrchr(dir, L'\\'); if (bs) bs[1] = 0; }
    wsprintfW(out, L"%sTypeBox.ini", dir);
}

static void loadConfig(void) {
    wchar_t ini[MAX_PATH] = {};
    iniPath(ini);
    g_timeoutMs = (DWORD)GetPrivateProfileIntW(L"NetTools", L"TimeoutMs", 3000, ini);
    if (g_timeoutMs < 800) g_timeoutMs = 800;
    if (g_timeoutMs > 15000) g_timeoutMs = 15000;
    g_cacheSec = (DWORD)GetPrivateProfileIntW(L"NetTools", L"CacheSec", 60, ini);
    g_pingCount = GetPrivateProfileIntW(L"NetTools", L"PingCount", 4, ini);
    if (g_pingCount < 1) g_pingCount = 1;
    if (g_pingCount > 8) g_pingCount = 8;
    GetPrivateProfileStringW(L"NetTools", L"WeatherURL",
        L"https://wttr.in/%s?format=3", g_weatherUrl, 256, ini);
    GetPrivateProfileStringW(L"NetTools", L"IpUrl",
        L"https://checkip.amazonaws.com/", g_ipUrl, 256, ini);
    GetPrivateProfileStringW(L"NetTools", L"LastCity", L"", g_lastCity, 128, ini);
}

/* ---------------------------------- utils ---------------------------------- */

static void trimW(wchar_t* s) {
    size_t n;
    while (*s == L' ' || *s == L'\t' || *s == L'\r' || *s == L'\n') {
        if (*s == 0) break;
        MoveMemory(s, s + 1, (lstrlenW(s)) * sizeof(wchar_t));
    }
    n = lstrlenW(s);
    while (n && (s[n - 1] == L' ' || s[n - 1] == L'\t' || s[n - 1] == L'\r' || s[n - 1] == L'\n'))
        s[--n] = 0;
}

static void putLine(const wchar_t* w) { /* wide answer -> CP_ACP con_text */
    WideCharToMultiByte(CP_ACP, 0, w, -1, g_out, sizeof(g_out), NULL, NULL);
}

static void putU8(const char* utf8) { /* UTF-8 body -> CP_ACP (best effort) */
    wchar_t w[2048];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 2048);
    putLine(w);
}

/* wttr.in lines carry weather pictographs (U+1F300.. etc.) that print as
 * "?" in a CP_ACP console - drop surrogates and symbol blocks, keep words/degrees */
static void stripEmoji(wchar_t* s) {
    wchar_t* w = s;
    while (*s) {
        wchar_t c = *s;
        int drop = (c >= 0xD800 && c <= 0xDFFF) ||                   /* any surrogate pair half   */
                   (c >= 0x2600 && c <= 0x27BF) ||                   /* misc symbols / dingbats   */
                   (c >= 0xFE00 && c <= 0xFE0F) ||                   /* variation selectors       */
                   c == 0x2B50 || c == 0x2B55;                       /* star / circle             */
        if (!drop) *w++ = c;
        ++s;
    }
    *w = 0;
}

/* ------------------------------ HTTP (winhttp) ------------------------------ */

/* parse scheme://host[:port]/path; https -> secure. Returns 0 on garbage. */
static int splitUrl(const wchar_t* url, wchar_t* host, size_t hcap,
                    wchar_t* path, size_t pcap, INTERNET_PORT* port, int* secure) {
    const wchar_t* p = url;
    const wchar_t* h;
    const wchar_t* sl;
    size_t n;
    *secure = 1; *port = INTERNET_DEFAULT_HTTPS_PORT;
    if (_wcsnicmp(p, L"https://", 8) == 0) p += 8;
    else if (_wcsnicmp(p, L"http://", 7) == 0) { p += 7; *secure = 0; *port = INTERNET_DEFAULT_HTTP_PORT; }
    else return 0;
    h = p;
    sl = wcschr(p, L'/');
    if (!sl) sl = p + lstrlenW(p);
    n = (size_t)(sl - h);
    { const wchar_t* col = wcschr(h, L':'); if (col && col < sl) {
        n = (size_t)(col - h);
        *port = (INTERNET_PORT)_wtoi(col + 1);
    } }
    if (!n || n + 1 > hcap) return 0;
    CopyMemory(host, h, n * sizeof(wchar_t)); host[n] = 0;
    if (!*sl) lstrcpyW(path, L"/");
    else {
        n = lstrlenW(sl);
        if (n + 1 > pcap) return 0;
        lstrcpyW(path, sl);
    }
    return 1;
}

/* GET full URL -> malloc'd NUL body, or NULL. Mirrors yt.c (winhttp, capped). */
static char* httpGet(const wchar_t* url, const wchar_t* accept) {
    wchar_t host[128], path[256];
    INTERNET_PORT port; int secure;
    char* body = NULL;
    size_t cap = 0, len = 0;
    if (!splitUrl(url, host, 128, path, 256, &port, &secure)) return NULL;
    {
        HINTERNET ses = WinHttpOpen(L"TypeBox-nettools/1.0 (offline-first; user-typed only)",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (ses) {
            HINTERNET con = NULL, req = NULL;
            WinHttpSetTimeouts(ses, (int)g_timeoutMs, (int)g_timeoutMs,
                               (int)g_timeoutMs, (int)g_timeoutMs);
            con = WinHttpConnect(ses, host, port, 0);
            if (con) req = WinHttpOpenRequest(con, L"GET", path, NULL, NULL, NULL,
                                              secure ? WINHTTP_FLAG_SECURE : 0);
            if (req) {
                wchar_t hdr[128];
                DWORD code = 0, sz = sizeof(code);
                wsprintfW(hdr, L"Accept: %s\r\n", accept);
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
                            if (len + avail + 1 > 256 * 1024) break;
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
    }
    if (body) body[len] = 0;
    return body;
}

static void urlEncodeUtf8(const wchar_t* w, char* out, size_t cap) {
    char u8[1024];
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

/* replace first %s in template with encoded city */
static void buildUrl(wchar_t* out, size_t cap, const wchar_t* tpl, const wchar_t* city) {
    char enc[512];
    wchar_t wenc[512];
    const wchar_t* pct = wcsstr(tpl, L"%s");
    urlEncodeUtf8(city, enc, sizeof(enc));
    MultiByteToWideChar(CP_ACP, 0, enc, -1, wenc, 512); /* ascii, lossless */
    if (!pct) { lstrcpynW(out, tpl, (int)cap); return; }
    {
        size_t pre = (size_t)(pct - tpl);
        wchar_t tail[256];
        lstrcpynW(out, tpl, (int)pre + 1);
        lstrcpynW(tail, pct + 2, 256);
        lstrcatW(out, wenc);
        if (lstrlenW(out) + lstrlenW(tail) + 1 < cap) lstrcatW(out, tail);
    }
}

/* --------------------------------- commands --------------------------------- */

static const char* runIp(void) {
    ULONGLONG now = GetTickCount64();
    if (g_ipLine[0] && g_ipAt && now - g_ipAt < g_cacheSec * 1000ULL) {
        wchar_t out[256];
        wsprintfW(out, L"ip: %s (cached)", g_ipLine);
        putLine(out);
        return g_out;
    }
    {
        char* body = httpGet(g_ipUrl, L"text/plain");
        if (body) {
            wchar_t w[160];
            MultiByteToWideChar(CP_UTF8, 0, body, -1, w, 160);
            free(body);
            trimW(w);
            if (w[0]) {
                wchar_t out[256];
                lstrcpyW(g_ipLine, w); g_ipAt = now;
                wsprintfW(out, L"ip: %s", w);
                putLine(out);
                return g_out;
            }
        }
    }
    putLine(L"ip: unreachable - check network or [NetTools] IpUrl");
    return g_out;
}

static const char* runWeather(const wchar_t* city) {
    ULONGLONG now = GetTickCount64();
    wchar_t out[1024];
    if (!city[0]) {
        if (g_lastCity[0]) city = g_lastCity;
        else { putLine(L"usage: weather <city>  (or w <city>)"); return g_out; }
    }
    if (g_wLine[0] && g_wAt && _wcsicmp(city, g_wCity) == 0 &&
        now - g_wAt < g_cacheSec * 1000ULL) {
        wsprintfW(out, L"weather: %s (cached)", g_wLine);
        putLine(out);
        return g_out;
    }
    {
        wchar_t url[512];
        char* body;
        buildUrl(url, 512, g_weatherUrl, city);
        body = httpGet(url, L"text/plain");
        if (body) {
            wchar_t w[640];
            int i, has = 0;
            MultiByteToWideChar(CP_UTF8, 0, body, -1, w, 640);
            free(body);
            trimW(w);
            stripEmoji(w);
            for (i = 0; w[i]; ++i) if (w[i] != L' ') has = 1;
            if (has) {
                lstrcpyW(g_wCity, city); lstrcpyW(g_wLine, w); g_wAt = now;
                wsprintfW(out, L"weather: %s", w);
                putLine(out);
                {   /* remember for bare `weather` next time */
                    wchar_t ini[MAX_PATH] = {};
                    iniPath(ini);
                    WritePrivateProfileStringW(L"NetTools", L"LastCity", city, ini);
                    lstrcpyW(g_lastCity, city);
                }
                return g_out;
            }
        }
    }
    putLine(L"weather: unreachable - check network or [NetTools] WeatherURL");
    return g_out;
}

static const char* runDns(const wchar_t* host) {
    wchar_t out[1024];
    /* MinGW quirk: GetAddrInfoW takes struct addrinfoW (not POSIX addrinfo) */
    struct addrinfoW hints, * res = NULL, * a;
    int shown = 0, rc;
    if (!host[0]) { putLine(L"usage: dns <host>"); return g_out; }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    rc = GetAddrInfoW(host, NULL, &hints, &res);
    if (rc != 0 || !res) {
        wsprintfW(out, L"dns: %s - not resolved (%d)", host, rc);
        putLine(out);
        return g_out;
    }
    wsprintfW(out, L"dns: %s ->", host);
    for (a = res; a && shown < 6; a = a->ai_next) {
        wchar_t ip[64] = L"";
        DWORD ipCap = 64;
        if (a->ai_family == AF_INET)
            InetNtopW(AF_INET, &((struct sockaddr_in*)a->ai_addr)->sin_addr, ip, ipCap);
        else if (a->ai_family == AF_INET6)
            InetNtopW(AF_INET6, &((struct sockaddr_in6*)a->ai_addr)->sin6_addr, ip, ipCap);
        if (!ip[0]) continue;
        ++shown;
        lstrcatW(out, L" ");
        lstrcatW(out, ip);
    }
    FreeAddrInfoW(res);
    if (!shown) lstrcatW(out, L" (no addresses)");
    putLine(out);
    return g_out;
}

static const char* runNetinfo(void) {
    wchar_t out[3500];
    ULONG bufLen = 0;
    PIP_ADAPTER_INFO ai;
    DWORD err;
    int n = 0;
    out[0] = 0;
    err = GetAdaptersInfo(NULL, &bufLen);
    if (err != ERROR_BUFFER_OVERFLOW && err != ERROR_SUCCESS) {
        wsprintfW(out, L"netinfo: GetAdaptersInfo failed (%u)", err);
        putLine(out); return g_out;
    }
    ai = (PIP_ADAPTER_INFO)malloc(bufLen);
    if (!ai) { putLine(L"netinfo: out of memory"); return g_out; }
    err = GetAdaptersInfo(ai, &bufLen);
    if (err != ERROR_SUCCESS) {
        wsprintfW(out, L"netinfo: GetAdaptersInfo failed (%u)", err);
        free(ai); putLine(out); return g_out;
    }
    for (; ai && n < 6; ai = ai->Next) {
        wchar_t line[560];
        wchar_t ip[64] = L"-", mask[64] = L"-", gw[64] = L"-";
        if (!ai->IpAddressList.IpAddress.String[0] ||
            strcmp(ai->IpAddressList.IpAddress.String, "0.0.0.0") == 0)
            continue; /* skip down/no-IP adapters */
        /* GetAdaptersInfo hands these over as dotted-decimal ANSI strings */
        MultiByteToWideChar(CP_ACP, 0, ai->IpAddressList.IpAddress.String, -1, ip, 64);
        MultiByteToWideChar(CP_ACP, 0, ai->IpAddressList.IpMask.String, -1, mask, 64);
        if (ai->GatewayList.IpAddress.String[0] &&
            strcmp(ai->GatewayList.IpAddress.String, "0.0.0.0") != 0)
            MultiByteToWideChar(CP_ACP, 0, ai->GatewayList.IpAddress.String, -1, gw, 64);
        {   /* description is ASCII-ish ANSI from the driver */
            wchar_t desc[192] = L"?";
            MultiByteToWideChar(CP_ACP, 0, ai->Description, -1, desc, 192);
            trimW(desc);
            wsprintfW(line, L"netinfo: %s | ip %s mask %s gw %s", desc, ip, mask, gw);
        }
        if (lstrlenW(out) + lstrlenW(line) + 2 < 3500) {
            if (out[0]) lstrcatW(out, L"\n");
            lstrcatW(out, line);
        }
        ++n;
    }
    free(ai);
    if (!out[0]) { putLine(L"netinfo: no connected adapters with IPv4"); return g_out; }
    putLine(out);
    return g_out;
}

static const char* runPing(const wchar_t* host, int count) {
    wchar_t out[512];
    struct addrinfoW hints, * res = NULL;
    int rc, i, replies = 0, minMs = 0, maxMs = 0;
    long long sumMs = 0;
    HANDLE hIcmp;
    if (!host[0]) { putLine(L"usage: ping <host> [count]"); return g_out; }
    if (count <= 0) count = g_pingCount;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;             /* ICMP echo over IPv4 */
    hints.ai_socktype = SOCK_STREAM;
    rc = GetAddrInfoW(host, NULL, &hints, &res);
    if (rc != 0 || !res) {
        wsprintfW(out, L"ping: %s - not resolved", host);
        putLine(out);
        return g_out;
    }
    hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        FreeAddrInfoW(res);
        putLine(L"ping: IcmpCreateFile failed - raw ICMP unavailable");
        return g_out;
    }
    {
        SOCKADDR_IN* sa = (SOCKADDR_IN*)res->ai_addr;
        char data[32];
        char replyBuf[1024];
        PICMP_ECHO_REPLY rep = (PICMP_ECHO_REPLY)replyBuf;
        for (i = 0; i < (int)sizeof(data); ++i) data[i] = (char)('A' + i % 26);
        for (i = 0; i < count; ++i) {
            DWORD ok = IcmpSendEcho(hIcmp, sa->sin_addr.s_addr, data, sizeof(data),
                                    NULL, replyBuf, sizeof(replyBuf), 1000);
            if (ok && rep->Status == IP_STATUS_SUCCESS) {
                long ms = (long)rep->RoundTripTime;
                ++replies;
                sumMs += ms;
                if (!minMs || ms < minMs) minMs = (int)ms;
                if ((int)ms > maxMs) maxMs = (int)ms;
            }
        }
        IcmpCloseHandle(hIcmp);
    }
    FreeAddrInfoW(res);
    if (!replies) {
        wsprintfW(out, L"ping: %s - no replies (%d sent)", host, count);
        putLine(out);
        return g_out;
    }
    {   /* avg via integer math (no float formatting in wsprintf - ABI rule) */
        int avg = (int)(sumMs / replies);
        wsprintfW(out, L"ping: %s - %d/%d replies, min %d ms, avg %d ms, max %d ms",
                  host, replies, count, minMs, avg, maxMs);
    }
    putLine(out);
    return g_out;
}

/* ----------------------------------- ABI ------------------------------------ */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
    WSAStartup(MAKEWORD(2, 2), &g_wsa);   /* local helpers only, no traffic */
}

PLUGIN_API void __cdecl Unload(void) {
    WSACleanup();
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "NetTools";
    i.version = "1.0";
    i.description = "ip / w <city> (one short line) / dns <host> / netinfo / ping <host> [n] - network only on typed lines";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 5; }

PLUGIN_API const char* __cdecl GetString(int n) {
    /* v0.30.1 command split: the full word `weather` belongs to weather.dll
     * (its rich answer); nettools owns the short `w <city>` line. Two plugins
     * exporting the same first word = the alphabetically-first one silently
     * shadows the other (Discovery is alphabetical, first run!=0 wins). */
    static const char* k[5] = { "ip", "dns", "netinfo", "ping", "w" };
    return (n >= 0 && n < 5) ? k[n] : NULL;
}

static char g_etRows[8][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 8) lstrcpynA(g_etRows[g_etCount++], r, 256); }

static int startsWith(const char* in, const char* pfx, int len) {
    return _strnicmp(in, pfx, (size_t)len) == 0;
}

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int len;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    len = lstrlenA(input);
    if (len <= 2 && startsWith(input, "ip", len)) etAdd("ip");
    else if (len <= 7 && startsWith(input, "netinfo", len)) etAdd("netinfo");
    else if (len <= 3 && startsWith(input, "dns", len)) etAdd("dns localhost");
    else if (startsWith(input, "dns ", 4) && len > 4) etAdd("dns localhost");
    else if (len <= 4 && startsWith(input, "ping", len)) etAdd("ping 8.8.8.8");
    else if (startsWith(input, "ping ", 5) && len > 5) etAdd("ping 8.8.8.8");
    else if (len <= 1 && startsWith(input, "w", len)) {
        if (g_lastCity[0]) {
            char city[192];
            WideCharToMultiByte(CP_ACP, 0, g_lastCity, -1, city, sizeof(city), NULL, NULL);
            { char row[256]; wsprintfA(row, "w %s", city); etAdd(row); }
        }
        etAdd("w Kyiv");
    }
    return g_etCount;
}

PLUGIN_API const char* __cdecl GetStringET(int n) {
    return (n >= 0 && n < g_etCount) ? g_etRows[n] : NULL;
}

/* strip leading spaces, then require cmd + (end|space), return arg start or NULL */
static const char* argAfter(const char* str, const char* cmd) {
    int clen = lstrlenA(cmd);
    if (_strnicmp(str, cmd, clen) != 0) return NULL;
    if (str[clen] == 0) return str + clen;
    if (str[clen] != ' ') return NULL;
    while (str[clen] == ' ') ++clen;
    return str + clen;
}

static void skipSpacesW(const wchar_t* s, wchar_t** out) { while (*s == L' ') ++s; *out = (wchar_t*)s; }

PLUGIN_API TExec __cdecl RunString(const char* str) {
    TExec ex;
    const char* arg;
    const char* text = NULL;
    wchar_t warg[512];
    ex.size = sizeof(TExec);
    ex.run = 0; ex.con_text = NULL; ex.con_sel_start = 0; ex.con_sel_length = 0;
    if (!str) return ex;
    while (*str == ' ') ++str;

    if ((arg = argAfter(str, "ip")) != NULL) {
        if (!arg[0]) text = runIp();
    } else if ((arg = argAfter(str, "netinfo")) != NULL) {
        if (!arg[0]) text = runNetinfo();
    } else if ((arg = argAfter(str, "dns")) != NULL) {
        wchar_t* wa;
        MultiByteToWideChar(CP_ACP, 0, arg, -1, warg, 512);
        trimW(warg);
        skipSpacesW(warg, &wa);
        text = runDns(wa);
    } else if ((arg = argAfter(str, "ping")) != NULL && arg[0]) {
        char a[512], *sp;
        int cnt = 0;
        lstrcpynA(a, arg, sizeof(a));
        sp = strchr(a, ' ');
        if (sp) { *sp = 0; cnt = atoi(sp + 1); }
        MultiByteToWideChar(CP_ACP, 0, a, -1, warg, 512);
        trimW(warg);
        text = runPing(warg, cnt);
    } else if ((arg = argAfter(str, "w")) != NULL && arg[0]) {
        /* v0.30.1: `weather <city>` is NOT routed here anymore - weather.dll
         * answers it in full (feels/km/h/today/tomorrow). nettools keeps the
         * short one-liner under `w` only. */
        wchar_t* wa;
        MultiByteToWideChar(CP_ACP, 0, arg, -1, warg, 512);
        trimW(warg);
        skipSpacesW(warg, &wa);
        text = runWeather(wa);
    }
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = text;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(text);
    return ex;
}
