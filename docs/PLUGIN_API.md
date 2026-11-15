# TypeBox plugin ABI

Carried over from **TypeAndRun** (see `Plugins.pas` in galancev/typeandrun) so that
existing plugins keep working after recompilation. Plugins are native DLLs exported
with the C calling convention (`cdecl`).

| Export | Signature | Purpose |
|---|---|---|
| `Load` | `void Load(HWND mainWnd)` | init, receive host window handle |
| `Unload` | `void Unload()` | deinit |
| `GetInfo` | `TInfo GetInfo()` | name/version/description/author/copyright/homepage |
| `GetCount` | `int GetCount()` | number of plugin command strings |
| `GetString` | `const char* GetString(int n)` | n-th command string |
| `GetCountET` | `int GetCountET(const char* input)` | completion count for input |
| `GetStringET` | `const char* GetStringET(int n)` | n-th completion |
| `RunString` | `TExec RunString(const char* str)` | execute command |

```cpp
struct TInfo {
    int size;               // sizeof(TInfo)
    const char* name;
    const char* version;
    const char* description;
    const char* author;
    const char* copyright;
    const char* homepage;
};

struct TExec {
    int size;               // sizeof(TExec)
    int run;                // non-zero: consume input and set console text
    const char* con_text;   // text to place into the console
    int con_sel_start;      // caret selection start
    int con_sel_length;     // selection length
};
```

Notes
- Strings are ANSI (CP_ACP) for TypeAndRun compatibility; a Unicode variant
  (`GetInfoW` / `RunStringW`, UTF-16) will be introduced with a host version flag.
- Plugins are discovered from the portable `Plugins\` folder and `[Plugins]` entries
  in `TypeBox.ini`.
