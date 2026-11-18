/* wol.dll - TypeBox Wake-on-LAN plugin (v0.31.0 wave, gallery #19, RI3 #238).
 *
 *   wol <MAC> [host [port]]   magic packet to host:port (default [Wol] Host/Port)
 *   wol <name> [host [port]]  named preset from [Wol] in TypeBox.ini
 *   wol list                  show the configured names (pure INI, no network)
 *
 * MAC accepts AA:BB:CC:DD:EE:FF, AA-BB-..., AABB.CCDD.EEFF or AABBCCDDEEFF.
 * [Wol] values are "<MAC> [host [port]]" (per-name target overrides).
 *
 * The magic packet is 6x FF followed by the target MAC 16 times, sent as UDP
 * broadcast - the only wake method that needs no vendor SDK (ws2_32 only).
 *
 * SAFETY CONTRACTS:
 *  - offline-first: Load() touches NO network, only the INI; a socket is
 *    created solely inside a typed `wol` line (host's plugin rules).
 *  - power family: with TYPEBOX_HEADLESS=1 or [UI] Headless=1 the packet is
 *    NEVER put on the wire - the answer is
 *    "wol: [headless] would send magic packet to AA:BB:CC:DD:EE:FF via h:p".
 *    Test suites therefore cannot wake anything.
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
/* MinGW order rule (playbook): winsock2 BEFORE windows.h. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WOL_BUILDING_DLL
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

static HWND   g_mainWnd = NULL;
static int    g_headless = 0;                 /* THE SAFETY SWITCH, see header */
static wchar_t g_host[128];                   /* [Wol] Host default            */
static int    g_port = 9;                     /* [Wol] Port default            */
static int    g_repeat = 3;                   /* [Wol] Repeat packets 1..5     */

static wchar_t g_wout[640];
static char    g_out[2048];
/* ini path; INI is reread per typed line (editing a [Wol] name then using it
 * is a normal flow; file IO only, zero network). Load() fills it first. */
static wchar_t g_ini[MAX_PATH];

static void putLine(const wchar_t* w) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, g_out, sizeof(g_out), NULL, NULL);
}

static void iniPath(wchar_t* buf, DWORD cap) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    lstrcpynW(buf, exe, cap);
    { wchar_t* sl = wcsrchr(buf, L'\\'); if (sl) sl[1] = 0; }
    lstrcatW(buf, L"TypeBox.ini");
}

/* ---- raw [Wol] reader ------------------------------------------------------
 * kernel32's profile APIs keep a per-process profile cache: on an externally
 * rewritten INI it can enumerate ZERO keys for [Section] while direct key
 * reads still serve values - an anomaly reproduced while building this plugin
 * (proftest.c). list / presets / ET therefore parse the file itself:
 * deterministic, BOM-aware, no cache games. Load() still uses the plain
 * GetPrivateProfile*W named reads for Host/Port/Repeat, like sibling plugins.
 * Rows are cached against the ini file's mtime (ET calls this per keystroke -
 * one 7 KB read only after a real edit; §4b perf track). */
typedef struct { wchar_t name[64]; wchar_t val[192]; } WRow;

static int loadIniW(wchar_t* buf, DWORD capW) {
    static unsigned char raw[131072];
    HANDLE h = CreateFileW(g_ini, GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    unsigned char* p;
    DWORD got = 0, n;
    UINT cp = CP_ACP;
    int w;
    if (h == INVALID_HANDLE_VALUE) return 0;
    for (;;) {
        DWORD r = 0;
        if (got >= sizeof(raw)) break;
        if (!ReadFile(h, raw + got, sizeof(raw) - got, &r, NULL) || !r) break;
        got += r;
    }
    CloseHandle(h);
    p = raw; n = got;
    if (n >= 2 && p[0] == 0xFF && p[1] == 0xFE) {        /* UTF-16LE BOM */
        p += 2; n -= 2; if (n % 2) --n;
        w = MultiByteToWideChar(1200 /*CP_UTF16LE*/, 0, (char*)p, (int)n, buf, (int)capW - 1);
        buf[w] = 0; return w > 0;
    }
    if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) { p += 3; n -= 3; cp = CP_UTF8; }
    w = MultiByteToWideChar(cp, 0, (char*)p, (int)n, buf, (int)capW - 1);
    buf[w] = 0; return w > 0;
}

static int wolRows(WRow* rows, int max) {
    static wchar_t buf[65536];
    wchar_t* p;
    int count = 0, inSec = 0;
    if (!loadIniW(buf, 65535)) return 0;
    for (p = buf; *p; ) {
        wchar_t* e = p;
        wchar_t* s;
        wchar_t* t;
        while (*e && *e != L'\n') ++e;
        s = p; t = e;
        while (s < t && (*s == L' ' || *s == L'\t' || *s == L'\r')) ++s;
        while (t > s && (t[-1] == L' ' || t[-1] == L'\t' || t[-1] == L'\r')) --t;
        if (t > s && *s == L'[') {                        /* section header */
            wchar_t* rb = s + 1;
            while (rb < t && *rb != L']') ++rb;
            inSec = 0;
            if (rb < t) {
                DWORD len = (DWORD)(rb - (s + 1));
                while (len && (s[1 + len - 1] == L' ' || s[1 + len - 1] == L'\t')) --len;
                inSec = (len == 3 && _wcsnicmp(s + 1, L"Wol", 3) == 0);
            }
        } else if (inSec && *s != L';' && *s != L'*') {   /* key line? */
            wchar_t* eq = s;
            while (eq < t && *eq != L'=') ++eq;
            if (eq > s && eq < t && count < max) {
                wchar_t* ns = s;
                wchar_t* ne = eq;
                wchar_t* vs = eq + 1;
                wchar_t* ve = t;
                DWORD i;
                while (ne > ns && (ne[-1] == L' ' || ne[-1] == L'\t')) --ne;
                while (vs < ve && (*vs == L' ' || *vs == L'\t')) ++vs;
                while (ve > vs && (ve[-1] == L' ' || ve[-1] == L'\t')) --ve;
                if (ne > ns && ve > vs) {
                    WRow* r = &rows[count];
                    for (i = 0; i < 63 && ns + i < ne; ++i) r->name[i] = ns[i];
                    r->name[i] = 0;
                    for (i = 0; i < 191 && vs + i < ve; ++i) r->val[i] = vs[i];
                    r->val[i] = 0;
                    /* a stray '=' inside the name would slip through: reject */
                    if (wcschr(r->name, L'=') == 0) ++count;
                }
            }
        }
        p = (*e == L'\n') ? e + 1 : e;
    }
    return count;
}

static WRow     g_rows[64];
static int      g_rowsN = -1;
static FILETIME g_rowsMtime;

static int wolRowsCached(void) {              /* returns row count, fills g_rows */
    HANDLE f = CreateFileW(g_ini, GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    FILETIME mt;
    if (f == INVALID_HANDLE_VALUE) { g_rowsN = 0; return 0; }
    if (!GetFileTime(f, NULL, NULL, &mt)) { CloseHandle(f); g_rowsN = 0; return 0; }
    CloseHandle(f);
    if (g_rowsN < 0 || mt.dwLowDateTime != g_rowsMtime.dwLowDateTime
                     || mt.dwHighDateTime != g_rowsMtime.dwHighDateTime) {
        g_rowsN = wolRows(g_rows, 64);
        g_rowsMtime = mt;
    }
    return g_rowsN;
}

static int isWolSetting(const wchar_t* name) {   /* config keys, not host names */
    return _wcsicmp(name, L"Host") == 0 || _wcsicmp(name, L"Port") == 0
        || _wcsicmp(name, L"Repeat") == 0;
}

static int findWolRowRaw(const wchar_t* name, wchar_t* valOut, DWORD cap) {
    int i, n = wolRowsCached();
    for (i = 0; i < n; ++i)
        if (_wcsicmp(g_rows[i].name, name) == 0) {
            lstrcpynW(valOut, g_rows[i].val, (int)cap);
            return 1;
        }
    return 0;
}

static int findWolRow(const wchar_t* name, wchar_t* valOut, DWORD cap) {
    int i, n = wolRowsCached();
    for (i = 0; i < n; ++i)
        if (!isWolSetting(g_rows[i].name) && _wcsicmp(g_rows[i].name, name) == 0) {
            lstrcpynW(valOut, g_rows[i].val, (int)cap);
            return 1;
        }
    return 0;
}

/* parse a MAC in any common spelling into 6 bytes; 1 on success */
static int parseMacW(const wchar_t* s, unsigned char mac[6]) {
    int nib = 0;
    while (*s && nib < 12) {
        wchar_t c = *s;
        int v;
        if (c == L':' || c == L'-' || c == L'.' || c == L' ') { ++s; continue; }
        if (c >= L'0' && c <= L'9')      v = c - L'0';
        else if (c >= L'a' && c <= L'f') v = c - L'a' + 10;
        else if (c >= L'A' && c <= L'F') v = c - L'A' + 10;
        else return 0;
        if (nib % 2 == 0) mac[nib / 2] = (unsigned char)(v << 4);
        else              mac[nib / 2] = (unsigned char)(mac[nib / 2] | v);
        ++nib; ++s;
    }
    while (*s == L' ' || *s == L':' || *s == L'-' || *s == L'.') ++s;
    return (nib == 12 && !*s);
}

static void fmtMacW(const unsigned char mac[6], wchar_t* out) {
    wsprintfW(out, L"%02X:%02X:%02X:%02X:%02X:%02X",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* one [Wol] row value: "<MAC> [host [port]]". host/port keep the caller's
 * defaults unless the value itself carries them. */
static int parseValueW(const wchar_t* val, unsigned char mac[6], wchar_t* host, int* port) {
    wchar_t tok[192];
    const wchar_t* p = val;
    int i = 0;
    while (*p == L' ') ++p;
    while (*p && *p != L' ' && i < 127) tok[i++] = *p++;
    tok[i] = 0;
    if (!parseMacW(tok, mac)) return 0;
    while (*p == L' ') ++p;
    if (*p) {
        i = 0;
        while (*p && *p != L' ' && i < 126) host[i++] = *p++;
        host[i] = 0;
        while (*p == L' ') ++p;
        if (*p && port) {
            int q = 0;
            while (*p >= L'0' && *p <= L'9') { q = q * 10 + (*p - L'0'); ++p; }
            if (q >= 1 && q <= 65535) *port = q;
        }
    }
    return 1;
}

static const char* doSend(const unsigned char mac[6], const wchar_t* host, int port, int npk) {
    wchar_t macs[32], line[400];
    fmtMacW(mac, macs);
    if (g_headless) {
        /* safety first: not a single byte goes on the wire (suite contract) */
        wsprintfW(line, L"wol: [headless] would send magic packet to %s via %s:%d (%d packets)",
                  macs, host, port, npk);
        putLine(line);
        return g_out;
    }
    {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        BOOL bc = TRUE;
        struct sockaddr_in to;
        unsigned char pkt[6 + 16 * 6];
        int i, j, sent = 0;
        if (s == INVALID_SOCKET) {
            wsprintfW(line, L"wol: socket failed (err %d)", WSAGetLastError());
            putLine(line);
            return g_out;
        }
        setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&bc, sizeof(bc));
        memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port = htons((u_short)port);
        if (!host[0] || _wcsicmp(host, L"broadcast") == 0) to.sin_addr.s_addr = INADDR_BROADCAST;
        else if (InetPtonW(AF_INET, host, &to.sin_addr) != 1) {
            /* allow host names on a typed line - resolve or fail honestly */
            struct addrinfoW hints, * res = NULL;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            if (GetAddrInfoW(host, NULL, &hints, &res) != 0 || !res) {
                wsprintfW(line, L"wol: host not resolved: %s", host);
                putLine(line);
                closesocket(s);
                return g_out;
            }
            to.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
            FreeAddrInfoW(res);
        }
        memset(pkt, 0xFF, 6);
        for (i = 0; i < 16; ++i)
            for (j = 0; j < 6; ++j) pkt[6 + i * 6 + j] = mac[j];
        for (i = 0; i < npk; ++i)
            if (sendto(s, (const char*)pkt, sizeof(pkt), 0,
                       (struct sockaddr*)&to, sizeof(to)) != SOCKET_ERROR) ++sent;
        closesocket(s);
        if (!sent) {
            wsprintfW(line, L"wol: send failed (err %d) - check VPN/firewall or set [Wol] Host to your subnet .255",
                       WSAGetLastError());
            putLine(line);
            return g_out;
        }
        wsprintfW(line, L"wol: sent magic packet to %s via %s:%d (%d/%d packets)",
                  macs, host, port, sent, npk);
        putLine(line);
        return g_out;
    }
}

static const char* doList(void) {
    wchar_t acc[1600];
    int i, n = wolRowsCached(), shown = 0;
    acc[0] = 0;
    for (i = 0; i < n && shown < 20; ++i) {
        wchar_t line[320];
        if (isWolSetting(g_rows[i].name)) continue;
        wsprintfW(line, L"%s = %s", g_rows[i].name, g_rows[i].val);
        if (shown) lstrcatW(acc, L"  |  ");
        lstrcatW(acc, line);
        ++shown;
    }
    if (!shown)
        lstrcpynW(acc, L"wol: no hosts - add  pc = AA:BB:CC:DD:EE:FF  under [Wol] in TypeBox.ini", 1600);
    else {
        wchar_t head[64];
        wsprintfW(head, L"wol: %d host(s)  |  ", shown);
        { wchar_t joined[1700];
          lstrcpynW(joined, head, 1700);
          lstrcatW(joined, acc);
          putLine(joined);
          return g_out; }
    }
    putLine(acc);
    return g_out;
}

static const char* dispatch(const char* line) {
    char a[256];
    const char* arg;
    wchar_t aw[256], tok1[192];
    unsigned char mac[6];
    wchar_t host[128];
    int port = g_port;

    if (_strnicmp(line, "wol", 3) != 0) return NULL;
    if (line[3] != 0 && line[3] != ' ') return NULL;
    arg = line + 3;
    while (*arg == ' ') ++arg;
    if (!*arg) {
        putLine(L"usage: wol <MAC|name> [host [port]]  (wol list shows names)");
        return g_out;
    }
    lstrcpynA(a, arg, sizeof(a));
    { size_t n = strlen(a); while (n && isspace((unsigned char)a[n - 1])) a[--n] = 0; }
    MultiByteToWideChar(CP_ACP, 0, a, -1, aw, 256);

    /* first token */
    { const wchar_t* p = aw; int i = 0;
      while (*p == L' ') ++p;
      while (*p && *p != L' ') { if (i < 190) tok1[i++] = *p; ++p; }
      tok1[i] = 0; }

    if (_wcsicmp(tok1, L"list") == 0) return doList();

    lstrcpynW(host, g_host, 128);
    if (parseMacW(tok1, mac)) {
        /* optional host / port overrides on the typed line */
        wchar_t* sp = wcschr(aw, L' ');
        if (sp) {
            int pv = 0;
            while (*sp == L' ') ++sp;
            lstrcpynW(host, sp, 128);
            { wchar_t* sp2 = wcschr(host, L' ');
              if (sp2) { *sp2 = 0; ++sp2;
                  while (*sp2 >= L'0' && *sp2 <= L'9') { pv = pv * 10 + (*sp2 - L'0'); ++sp2; }
                  if (pv >= 1 && pv <= 65535) port = pv; } }
        }
        return doSend(mac, host, port, g_repeat);
    }
    {   /* named preset: [Wol] <name> = "<MAC> [host [port]]" (raw parse) */
        wchar_t val[256];
        val[0] = 0;
        if (findWolRow(tok1, val, 256)) {
            int p2 = g_port;
            wchar_t h2[128];
            lstrcpynW(h2, g_host, 128);
            if (!parseValueW(val, mac, h2, &p2)) {
                wsprintfW(g_wout, L"wol: bad [Wol] value for %s: %s", tok1, val);
                putLine(g_wout);
                return g_out;
            }
            /* command-line host override wins over the preset target */
            { wchar_t* sp = wcschr(aw, L' ');
              if (sp) {
                  int pv = 0;
                  while (*sp == L' ') ++sp;
                  lstrcpynW(h2, sp, 128);
                  { wchar_t* sp2 = wcschr(h2, L' ');
                    if (sp2) { *sp2 = 0; ++sp2;
                        while (*sp2 >= L'0' && *sp2 <= L'9') { pv = pv * 10 + (*sp2 - L'0'); ++sp2; }
                        if (pv >= 1 && pv <= 65535) p2 = pv; } }
              } }
            (void)port;
            return doSend(mac, h2, p2, g_repeat);
        }
    }
    {   wchar_t* sp = wcschr(aw, L' '); if (sp) *sp = 0;   /* just the word */
        wsprintfW(g_wout, L"wol: not a MAC or configured name - %s  (wol list shows names)", aw); }
    putLine(g_wout);
    return g_out;
}

/* ------------------------------- exports ----------------------------------- */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    wchar_t hb[8], e[8], v[64];
    g_mainWnd = mainWnd;
    iniPath(g_ini, MAX_PATH);
    g_rowsN = -1;                       /* row cache belongs to this ini path */
    /* Host/Port/Repeat via the same raw reader as presets/list: one parser,
     * no per-process profile-cache surprises after an external INI edit. */
    g_port = 9; g_repeat = 3;
    lstrcpynW(g_host, L"255.255.255.255", 128);
    if (findWolRowRaw(L"Host", v, 64) && v[0]) lstrcpynW(g_host, v, 128);
    if (findWolRowRaw(L"Port", v, 64) && v[0]) {
        int q = (int)wcstol(v, NULL, 10);
        if (q >= 1 && q <= 65535) g_port = q;
    }
    if (findWolRowRaw(L"Repeat", v, 64) && v[0]) {
        int q = (int)wcstol(v, NULL, 10);
        if (q >= 1 && q <= 5) g_repeat = q;
    }
    /* [UI] Headless is the host's own well-known key (off.c reads it the same
     * way); env TYPEBOX_HEADLESS=1 is the suite's primary switch. */
    { wchar_t hb[8];
      GetPrivateProfileStringW(L"UI", L"Headless", L"0", hb, 8, g_ini);
      if (hb[0] == L'1') g_headless = 1; }
    if (GetEnvironmentVariableW(L"TYPEBOX_HEADLESS", e, 8) > 0 && e[0] == L'1')
        g_headless = 1;
    /* zero network here: WSAStartup only (playbook: Load must stay silent) */
    { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
}

PLUGIN_API void __cdecl Unload(void) {
    WSACleanup();
}

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "WakeOnLan";
    i.version = "1.0";
    i.description = "wol <MAC|name> [host] - Wake-on-LAN magic packet (UDP); wol list - only on typed lines";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "wol" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

static char g_etRows[9][256];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 9) lstrcpynA(g_etRows[g_etCount++], r, 256); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int i, n, added = 0;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    if (_strnicmp(input, "wol", (lstrlenA(input) < 3) ? lstrlenA(input) : 3) != 0) return 0;
    etAdd("wol list");
    n = wolRowsCached();
    for (i = 0; i < n && added < 6; ++i) {
        char row[256];
        wchar_t wrow[256];
        if (isWolSetting(g_rows[i].name)) continue;
        wsprintfW(wrow, L"wol %s", g_rows[i].name);
        WideCharToMultiByte(CP_ACP, 0, wrow, -1, row, sizeof(row), NULL, NULL);
        etAdd(row);
        ++added;
    }
    if (added == 0) etAdd("wol AA:BB:CC:DD:EE:FF");
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
    text = dispatch(str);
    if (!text || !text[0]) return ex;
    ex.run = 1;
    ex.con_text = text;
    ex.con_sel_start = 0;
    ex.con_sel_length = (int)lstrlenA(text);
    return ex;
}
