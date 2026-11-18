/* todo.dll - TypeBox todo plugin (v0.30.0 wave, gallery #17).
 *
 *   todo <text> [when]     add; [when] from trailing tokens: today, tomorrow,
 *                          mon..sun (+full names), +Nd, +Nh, HH:MM,
 *                          YYYY-MM-DD (combinable: "tomorrow 18:00")
 *   todo add <text> [when] explicit add
 *   todo                   list, numbered, overdue marked "!!"
 *   todo today             what is due today (or overdue)
 *   todo done <n>          move item n to the done archive
 *   todo del <n>           delete item n
 *
 * Persistence: [Todo] File=todo.txt next to TypeBox.exe, one UTF-8 entry per
 * line "DUE\tCREATED\tTEXT" (tabs/newlines escaped, cliphist-style). Loaded
 * once at Load(), saved after every change - zero network, zero timers.
 * Russian "zavtra/sevodnya" match the typed CP_ACP bytes via \x escapes
 * (source stays pure ASCII - PS/codepage rules of the project).
 *
 * GPL-3.0 - (c) D. Shevchuk. Part of the TypeBox examples, continues
 * TypeAndRun by Evgeniy Galantsev (https://github.com/galancev/typeandrun).
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TODO_BUILDING_DLL
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

#define TODO_MAX 500

typedef struct {
    wchar_t text[512];
    SYSTEMTIME due;
    int hasDue;
    SYSTEMTIME created;
} TItem;

static HWND    g_mainWnd = NULL;
static wchar_t g_file[260] = L"todo.txt";     /* [Todo] File            */
static wchar_t g_doneFile[260] = L"todo-done.txt";
static int     g_maxRows = 20;                /* [Todo] MaxRows         */
static int     g_dueWarn = 1;                 /* [Todo] DueWarn         */
static TItem   g_items[TODO_MAX];
static int     g_count = 0;

static char g_out[16384];                     /* con_text bytes (CP_ACP) */

static void putLine(const wchar_t* w) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, g_out, sizeof(g_out), NULL, NULL);
}

/* --------------------------------- config ---------------------------------- */

static void moduleDir(wchar_t* dir) {
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    { wchar_t* bs = wcsrchr(dir, L'\\'); if (bs) bs[1] = 0; }
}

static void loadConfig(void) {
    wchar_t ini[MAX_PATH] = {}, dir[MAX_PATH] = {};
    moduleDir(dir);
    wsprintfW(ini, L"%sTypeBox.ini", dir);
    GetPrivateProfileStringW(L"Todo", L"File", L"todo.txt", g_file, 260, ini);
    GetPrivateProfileStringW(L"Todo", L"DoneFile", L"todo-done.txt", g_doneFile, 260, ini);
    g_maxRows = GetPrivateProfileIntW(L"Todo", L"MaxRows", 20, ini);
    if (g_maxRows < 5) g_maxRows = 5;
    if (g_maxRows > 100) g_maxRows = 100;
    g_dueWarn = GetPrivateProfileIntW(L"Todo", L"DueWarn", 1, ini);
}

static void filePath(wchar_t* out, size_t cap, const wchar_t* name) {
    wchar_t dir[MAX_PATH] = {};
    out[0] = 0;
    if (name[1] == L':' || name[0] == L'\\') { lstrcpynW(out, name, (int)cap); return; }
    moduleDir(dir);
    if (lstrlenW(dir) + lstrlenW(name) + 1 < (int)cap) wsprintfW(out, L"%s%s", dir, name);
}

/* --------------------------- ISO time helpers ------------------------------ */

static void fmtIso(const SYSTEMTIME* st, wchar_t* out) { /* "YYYY-MM-DD HH:MM" */
    wsprintfW(out, L"%04d-%02d-%02d %02d:%02d", (int)st->wYear, (int)st->wMonth,
              (int)st->wDay, (int)st->wHour, (int)st->wMinute);
}

static int parseIso(const wchar_t* s, SYSTEMTIME* st) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    int got = 0;
    /* manual scan: no sscanf float/locale surprises */
    {
        const wchar_t* p = s;
        int i;
        int* dest[5] = { &y, &mo, &d, &h, &mi };
        for (i = 0; i < 5; ++i) {
            int v = 0, any = 0;
            while (*p >= L'0' && *p <= L'9') { v = v * 10 + (*p - L'0'); ++p; any = 1; }
            if (!any) return 0;
            *dest[i] = v;
            if (i < 4) {
                wchar_t sep = (i == 2) ? L' ' : L'-';
                if (*p != sep) return 0;
                ++p;
            }
        }
        if (*p != 0) return 0;
        ++got;
    }
    if (y < 2000 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
        h > 23 || mi > 59 || !got) return 0;
    memset(st, 0, sizeof(*st));
    st->wYear = (WORD)y; st->wMonth = (WORD)mo; st->wDay = (WORD)d;
    st->wHour = (WORD)h; st->wMinute = (WORD)mi;
    return 1;
}

static void addMinutes(SYSTEMTIME* st, long long minutes) {
    FILETIME ft;
    ULARGE_INTEGER u;
    if (!SystemTimeToFileTime(st, &ft)) return;
    u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    u.QuadPart += (ULONGLONG)minutes * 60000000ULL;
    ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
    FileTimeToSystemTime(&ft, st);
}

static const wchar_t* dayName(const SYSTEMTIME* st) {
    static const wchar_t* k[7] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    if (st->wDayOfWeek > 6) return L"?";
    return k[st->wDayOfWeek];
}

static int cmpNow(const SYSTEMTIME* due) { /* -1 past, 0 future */
    SYSTEMTIME now;
    FILETIME a, b;
    ULONGLONG ta, tb;
    GetLocalTime(&now);
    SystemTimeToFileTime(&now, &a);
    SystemTimeToFileTime(due, &b);
    ta = ((ULONGLONG)a.dwHighDateTime << 32) | a.dwLowDateTime;
    tb = ((ULONGLONG)b.dwHighDateTime << 32) | b.dwLowDateTime;
    return tb < ta ? -1 : 0;
}

static int dueWithin24(const SYSTEMTIME* due) {
    SYSTEMTIME now;
    FILETIME a, b;
    ULONGLONG ta, tb;
    GetLocalTime(&now);
    SystemTimeToFileTime(&now, &a);
    SystemTimeToFileTime(due, &b);
    ta = ((ULONGLONG)a.dwHighDateTime << 32) | a.dwLowDateTime;
    tb = ((ULONGLONG)b.dwHighDateTime << 32) | b.dwLowDateTime;
    return tb < ta + 24ULL * 3600 * 10000000ULL;
}

/* --------------------------------- storage --------------------------------- */

static void escapeText(FILE* f, const wchar_t* t) {
    for (; *t; ++t) {
        switch (*t) {
        case L'\n': fputs("\\n", f); break;
        case L'\r': fputs("\\r", f); break;
        case L'\t': fputs("\\t", f); break;
        case L'\\': fputs("\\\\", f); break;
        default:
            if (*t >= 32) {
                char u8[4];
                int n = WideCharToMultiByte(CP_UTF8, 0, t, 1, u8, sizeof(u8), NULL, NULL);
                if (n > 0) fwrite(u8, 1, (size_t)n, f);
            }
        }
    }
}

static void unescapeText(const char* s, wchar_t* out, size_t cap) {
    size_t n = 0;
    while (*s && n + 1 < cap) {
        if (*s == '\\') {
            ++s;
            if (!*s) break;
            switch (*s) {
            case 'n': out[n++] = L'\n'; break;
            case 'r': out[n++] = L'\r'; break;
            case 't': out[n++] = L'\t'; break;
            case '\\': out[n++] = L'\\'; break;
            default: out[n++] = (wchar_t)(unsigned char)*s;
            }
            ++s;
        } else {
            wchar_t w[2] = { 0, 0 };
            int lead = (unsigned char)s[0];
            int adv = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
            char seq[5];
            int k;
            for (k = 0; k < adv && s[k]; ++k) seq[k] = s[k];
            seq[k] = 0;
            MultiByteToWideChar(CP_UTF8, 0, seq, -1, w, 2);
            if (w[0]) out[n++] = w[0];
            s += k;
        }
    }
    out[n] = 0;
}

static void writeRecord(FILE* f, const TItem* it) {
    wchar_t iso[32];
    char u8[80];
    if (it->hasDue) {
        fmtIso(&it->due, iso);
        WideCharToMultiByte(CP_UTF8, 0, iso, -1, u8, sizeof(u8), NULL, NULL);
        fputs(u8, f);
    } else fputc('-', f);
    fputc('\t', f);
    fmtIso(&it->created, iso);
    WideCharToMultiByte(CP_UTF8, 0, iso, -1, u8, sizeof(u8), NULL, NULL);
    fputs(u8, f);
    fputc('\t', f);
    escapeText(f, it->text);
    fputc('\n', f);
}

static void saveAll(void) {
    wchar_t path[MAX_PATH * 2];
    char apath[MAX_PATH * 4];
    FILE* f;
    int i;
    filePath(path, MAX_PATH * 2, g_file);
    if (!path[0]) return;
    WideCharToMultiByte(CP_ACP, 0, path, -1, apath, sizeof(apath), NULL, NULL);
    f = fopen(apath, "wb");
    if (!f) return;
    for (i = 0; i < g_count; ++i) writeRecord(f, &g_items[i]);
    fclose(f);
}

static void appendDone(const TItem* it) {
    wchar_t path[MAX_PATH * 2];
    char apath[MAX_PATH * 4];
    FILE* f;
    filePath(path, MAX_PATH * 2, g_doneFile);
    if (!path[0]) return;
    WideCharToMultiByte(CP_ACP, 0, path, -1, apath, sizeof(apath), NULL, NULL);
    f = fopen(apath, "ab");
    if (!f) return;
    writeRecord(f, it);
    fclose(f);
}

static void loadAll(void) {
    wchar_t path[MAX_PATH * 2];
    char apath[MAX_PATH * 4];
    FILE* f;
    char line[2048];
    g_count = 0;
    filePath(path, MAX_PATH * 2, g_file);
    if (!path[0]) return;
    WideCharToMultiByte(CP_ACP, 0, path, -1, apath, sizeof(apath), NULL, NULL);
    f = fopen(apath, "rb");
    if (!f) return;
    while (fgets(line, sizeof(line), f) && g_count < TODO_MAX) {
        char* t1 = strchr(line, '\t');
        char* t2;
        TItem* it;
        size_t n;
        if (!t1) continue;
        *t1++ = 0;
        t2 = strchr(t1, '\t');
        if (!t2) continue;
        *t2++ = 0;
        n = lstrlenA(t2);
        while (n && (t2[n - 1] == '\n' || t2[n - 1] == '\r')) t2[--n] = 0;
        it = &g_items[g_count];
        memset(it, 0, sizeof(*it));
        if (strcmp(line, "-") != 0) {
            wchar_t wiso[32];
            MultiByteToWideChar(CP_UTF8, 0, line, -1, wiso, 32);
            if (parseIso(wiso, &it->due)) it->hasDue = 1;
        }
        { wchar_t wiso[32]; MultiByteToWideChar(CP_UTF8, 0, t1, -1, wiso, 32); parseIso(wiso, &it->created); }
        unescapeText(t2, it->text, 512);
        if (it->text[0]) ++g_count;
    }
    fclose(f);
}

/* ------------------------------ when-parser -------------------------------- */

/* CP1251 bytes for "zavtra" and "sevodnya" (source stays ASCII) */
#define ZAVTRA    "\xE7\xE0\xE2\xF2\xF0\xE0"
#define SEVODNYA  "\xF1\xE5\xE3\xEE\xE4\xED\xFF"

static int dayToken(const char* tok, int* offset, int* targetDow, char* fullDate) {
    static const char* names[7][2] = {
        { "sun", "sunday" }, { "mon", "monday" }, { "tue", "tuesday" },
        { "wed", "wednesday" }, { "thu", "thursday" }, { "fri", "friday" },
        { "sat", "saturday" }
    };
    int d, i, j;
    if (_stricmp(tok, "today") == 0 || _stricmp(tok, SEVODNYA) == 0) { *offset = 0; return 1; }
    if (_stricmp(tok, "tomorrow") == 0 || _stricmp(tok, ZAVTRA) == 0) { *offset = 1; return 1; }
    for (d = 0; d < 7; ++d)
        for (i = 0; i < 2; ++i)
            if (_stricmp(tok, names[d][i]) == 0) { *targetDow = d; return 2; }
    if (tok[0] == '+' && tok[1] && (tok[2] == 'd' || tok[2] == 'D') && !tok[3]) {
        for (j = 1; tok[j + 1]; ++j) if (tok[j] < '0' || tok[j] > '9') return 0;
        *offset = atoi(tok + 1);
        if (*offset >= 0 && *offset <= 365) return 1;
        return 0;
    }
    /* YYYY-MM-DD (10 chars, digits + two dashes) */
    if (lstrlenA(tok) == 10) {
        int dashes = 0;
        for (j = 0; j < 10; ++j) {
            if (tok[j] == '-') ++dashes;
            else if (tok[j] < '0' || tok[j] > '9') { dashes = -9; break; }
        }
        if (dashes == 2) { lstrcpynA(fullDate, tok, 16); return 3; }
    }
    return 0;
}

static int timeToken(const char* tok, int* h, int* m) { /* "HH:MM" */
    if (lstrlenA(tok) != 5 || tok[2] != ':') return 0;
    if (!isdigit((unsigned char)tok[0]) || !isdigit((unsigned char)tok[1]) ||
        !isdigit((unsigned char)tok[3]) || !isdigit((unsigned char)tok[4])) return 0;
    *h = (tok[0] - '0') * 10 + (tok[1] - '0');
    *m = (tok[3] - '0') * 10 + (tok[4] - '0');
    return *h <= 23 && *m <= 59;
}

static int hoursToken(const char* tok) { /* "+5h" -> 5 else -1 */
    int j;
    if (tok[0] != '+' || !tok[1] || !tok[2]) return -1;
    if (tok[2] != 'h' && tok[2] != 'H') return -1;
    if (tok[3]) return -1;
    if (tok[1] < '0' || tok[1] > '9') return -1;
    for (j = 1; j < 2; ++j) if (tok[j] < '0' || tok[j] > '9') return -1;
    return atoi(tok + 1);
}

/* Consume trailing when-tokens; returns malloc-free pointer to the joined
 * item text (static), or NULL when the whole line was when-parts. */
static const char* parseTrailing(SYSTEMTIME* due, int* hasDue, char* line) {
    char* tokens[8];
    int tk = 0;
    int dayOff = -1, dow = -1, haveDate = 0, hour = -1, minute = 0, hours = -1;
    char fullDate[16] = "";
    int i;
    char* p;
    static char joined[1024];
    SYSTEMTIME d;

    p = line;
    while (*p && tk < 8) {
        while (*p == ' ') ++p;
        if (!*p) break;
        tokens[tk++] = p;
        while (*p && *p != ' ') ++p;
        if (*p) { *p = 0; ++p; }
    }
    if (!tk) return NULL;

    while (tk > 1) {
        char* t = tokens[tk - 1];
        int off = 0, td = 0;
        int kind = dayToken(t, &off, &td, fullDate);
        if (kind == 1 && dayOff < 0 && !haveDate && dow < 0) { dayOff = off; --tk; continue; }
        if (kind == 2 && dow < 0 && !haveDate && dayOff < 0) { dow = td; --tk; continue; }
        if (kind == 3 && !haveDate && dayOff < 0 && dow < 0) { haveDate = 1; --tk; continue; }
        if (kind == 0) {
            int h, m;
            if (timeToken(t, &h, &m) && hour < 0) { hour = h; minute = m; --tk; continue; }
            {   int hh = hoursToken(t);
                if (hh >= 0 && hours < 0 && dayOff < 0 && dow < 0 && !haveDate) { hours = hh; --tk; continue; } }
        }
        break;
    }
    if (dayOff < 0 && dow < 0 && !haveDate && hour < 0 && hours < 0) {
        /* nothing consumed: full line is the text */
        goto join;
    }
    memset(&d, 0, sizeof(d));
    if (haveDate) {
        wchar_t wiso[32];
        MultiByteToWideChar(CP_ACP, 0, fullDate, -1, wiso, 32);
        if (!parseIso(wiso, &d)) { /* bad date: treat token as text */
            dayOff = dow = -1; haveDate = 0; hour = -1; hours = -1;
            goto join;
        }
        d.wHour = (WORD)(hour >= 0 ? hour : 9);
        d.wMinute = (WORD)(hour >= 0 ? minute : 0);
    } else {
        GetLocalTime(&d);
        d.wSecond = 0; d.wMilliseconds = 0;
        if (dow >= 0) dayOff = (dow - (int)d.wDayOfWeek + 7) % 7;
        if (dayOff >= 0) addMinutes(&d, (long long)dayOff * 24 * 60);
        d.wHour = (WORD)(hour >= 0 ? hour : 9);
        d.wMinute = (WORD)(hour >= 0 ? minute : 0);
    }
    if (hours >= 0) { GetLocalTime(&d); addMinutes(&d, (long long)hours * 60); }
    *due = d;
    *hasDue = 1;
join:
    joined[0] = 0;
    for (i = 0; i < tk; ++i) {
        if (i) {
            if (lstrlenA(joined) + 2 > (int)sizeof(joined)) break;
            lstrcatA(joined, " ");
        }
        if (lstrlenA(joined) + lstrlenA(tokens[i]) + 1 >= (int)sizeof(joined)) break;
        lstrcatA(joined, tokens[i]);
    }
    if (!joined[0]) return NULL;
    return joined;
}

/* --------------------------------- commands --------------------------------- */

static const char* renderList(int todayOnly) {
    static wchar_t out[9216];
    int i, shown = 0, total = 0;
    out[0] = 0;
    for (i = 0; i < g_count; ++i) {
        TItem* it = &g_items[i];
        wchar_t line[720];
        const wchar_t* mark = L"";
        if (todayOnly) {
            SYSTEMTIME now;
            int past = cmpNow(&it->due) < 0;
            if (!it->hasDue) continue;
            if (!past) {
                GetLocalTime(&now);
                if (!(it->due.wYear == now.wYear && it->due.wMonth == now.wMonth &&
                      it->due.wDay == now.wDay)) continue;
            }
        }
        ++total;
        if (shown >= g_maxRows) continue;
        if (it->hasDue && g_dueWarn) {
            if (cmpNow(&it->due) < 0) mark = L"!! ";
            else if (dueWithin24(&it->due)) mark = L"! ";
        }
        if (it->hasDue)
            wsprintfW(line, L"%d. %s%s (%s %02d:%02d)", shown + 1, mark, it->text,
                      dayName(&it->due), (int)it->due.wHour, (int)it->due.wMinute);
        else
            wsprintfW(line, L"%d. %s%s", shown + 1, mark, it->text);
        if (lstrlenW(out) + lstrlenW(line) + 2 >= (int)(sizeof(out) / sizeof(wchar_t))) {
            ++shown;
            break; /* header room reserved below */
        }
        if (out[0]) lstrcatW(out, L"\n");
        lstrcatW(out, line);
        ++shown;
    }
    if (!total) { putLine(todayOnly ? L"todo: nothing due today" : L"todo: empty"); return g_out; }
    {
        static wchar_t full[9600];
        if (todayOnly) wsprintfW(full, L"todo: %d item(s) - today\n", total);
        else wsprintfW(full, L"todo: %d item(s)\n", total);
        lstrcatW(full, out);
        if (total > shown) {
            wchar_t more[64];
            wsprintfW(more, L"\n... (%d more, [Todo] MaxRows)", total - shown);
            lstrcatW(full, more);
        }
        putLine(full);
    }
    return g_out;
}

static const char* addItem(const char* textWithWhen) {
    wchar_t out[700];
    SYSTEMTIME due, created;
    int hasDue = 0;
    char buf[1024];
    const char* text;
    static wchar_t wtext[512];
    lstrcpynA(buf, textWithWhen, sizeof(buf));
    {   /* collapse runs of spaces, drop the trailing ones */
        char* r = buf, *w = buf;
        int seen = 1;                       /* 1: squeeze leading too */
        while (*r) {
            if (*r == ' ') { if (!seen && w != buf) *w++ = ' '; seen = 1; }
            else { *w++ = *r; seen = 0; }
            ++r;
        }
        while (w != buf && w[-1] == ' ') --w;
        *w = 0;
    }
    if (!buf[0]) { putLine(L"usage: todo <text> [today|tomorrow|fri|+2d|+5h|18:00]"); return g_out; }
    text = parseTrailing(&due, &hasDue, buf);
    if (!text || !text[0]) { putLine(L"todo: text required (when-parts alone are not enough)"); return g_out; }
    if (g_count >= TODO_MAX) { putLine(L"todo: too many items"); return g_out; }
    {
        TItem* it = &g_items[g_count];
        memset(it, 0, sizeof(*it));
        GetLocalTime(&created);
        it->created = created;
        MultiByteToWideChar(CP_ACP, 0, text, -1, it->text, 512);
        if (hasDue) { it->due = due; it->hasDue = 1; }
        ++g_count;
    }
    saveAll();
    MultiByteToWideChar(CP_ACP, 0, text, -1, wtext, 512);
    if (hasDue)
        wsprintfW(out, L"todo: added #%d: %s (due %s %02d:%02d)", g_count, wtext,
                  dayName(&due), (int)due.wHour, (int)due.wMinute);
    else
        wsprintfW(out, L"todo: added #%d: %s", g_count, wtext);
    putLine(out);
    return g_out;
}

static const char* byIndex(const char* arg, int done) { /* 1=done 0=del */
    wchar_t out[700];
    int n;
    const char* p = arg;
    TItem* it;
    static wchar_t wtext[512];
    while (*p == ' ') ++p;
    if (!*p) { putLine(done ? L"usage: todo done <n>" : L"usage: todo del <n>"); return g_out; }
    for (; *p; ++p) if (*p < '0' || *p > '9') {
        putLine(done ? L"usage: todo done <n>" : L"usage: todo del <n>");
        return g_out;
    }
    n = atoi(arg);
    if (n < 1 || n > g_count) { wsprintfW(out, L"todo: no item #%d (1..%d)", n, g_count); putLine(out); return g_out; }
    it = &g_items[n - 1];
    if (done) appendDone(it);
    lstrcpyW(wtext, it->text);
    MoveMemory(it, it + 1, (size_t)(g_count - n) * sizeof(TItem));
    --g_count;
    saveAll();
    wsprintfW(out, L"todo: %s #%d: %s", done ? L"done" : L"deleted", n, wtext);
    putLine(out);
    return g_out;
}

/* -------------------------------- dispatch --------------------------------- */

static const char* dispatch(const char* line) {
    const char* arg;
    int len = lstrlenA(line);
    if (len == 4 && _strnicmp(line, "todo", 4) == 0)
        return renderList(0);
    if (len < 5 || _strnicmp(line, "todo ", 5) != 0) return NULL;
    arg = line + 5;
    while (*arg == ' ') ++arg;
    if (!arg[0]) return renderList(0);
    if (_stricmp(arg, "today") == 0) return renderList(1);
    if (_strnicmp(arg, "done", 4) == 0 && (arg[4] == 0 || arg[4] == ' '))
        return byIndex(arg + 4, 1);
    if (_strnicmp(arg, "del", 3) == 0 && (arg[3] == 0 || arg[3] == ' '))
        return byIndex(arg + 3, 0);
    if (_strnicmp(arg, "add", 3) == 0 && arg[3] == ' ')
        return addItem(arg + 4);
    return addItem(arg);                      /* "todo <text>..." */
}

/* ----------------------------------- ABI ------------------------------------ */

PLUGIN_API void __cdecl Load(HWND mainWnd) {
    g_mainWnd = mainWnd;
    loadConfig();
    loadAll();
}

PLUGIN_API void __cdecl Unload(void) { }

PLUGIN_API TInfo __cdecl GetInfo(void) {
    TInfo i;
    i.size = sizeof(TInfo);
    i.name = "Todo";
    i.version = "1.0";
    i.description = "todo <text> [tomorrow 18:00] / todo / todo today / todo done <n> / todo del <n> - todo.txt next to exe";
    i.author = "D. Shevchuk";
    i.copyright = "GPL-3.0";
    i.homepage = "https://github.com/shevchukdp/TypeBox";
    return i;
}

PLUGIN_API int __cdecl GetCount(void) { return 1; }

PLUGIN_API const char* __cdecl GetString(int n) {
    static const char* k[1] = { "todo" };
    return (n >= 0 && n < 1) ? k[n] : NULL;
}

static char g_etRows[5][128];
static int  g_etCount;
static void etAdd(const char* r) { if (g_etCount < 5) lstrcpynA(g_etRows[g_etCount++], r, 128); }

PLUGIN_API int __cdecl GetCountET(const char* input) {
    int len;
    g_etCount = 0;
    if (!input || !input[0]) return 0;
    len = lstrlenA(input);
    if (len <= 4 && _strnicmp(input, "todo", (size_t)len) == 0) {
        etAdd("todo");
        etAdd("todo today");
        etAdd("todo done 1");
        etAdd("todo del 1");
    } else if (len > 4 && _strnicmp(input, "todo ", 5) == 0) {
        etAdd("todo today");
        etAdd("todo done 1");
        etAdd("todo del 1");
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
