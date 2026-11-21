# TypeBox plugin ABI

Implemented in the host since **v0.4.0** (`src/main.cpp`, see also the working example
[`examples/plugin_calc/calc.c`](../examples/plugin_calc/calc.c)).

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

## v0.36: the SDK header

`include/typebox_plugin.h` is the one-file SDK. Include it, define
`TB_BUILDING_DLL` before including, and every export gets the right
decoration for both MinGW and MSVC:

```c
#define TB_BUILDING_DLL
#include "typebox_plugin.h"

TB_EXPORT int __cdecl GetCount(void) { return 1; }
```

The header also ships three authoring helpers so `RunString` stays a
two-liner: `TbNoRun()` (pass), `TbAnswer(str)` (consume the line and put
`str` in the console) and `TbInfo(...)` (fill a `TInfo`). `TbPluginDirAt`
returns the plugin's own directory (pass `g_self` captured in `DllMain`) -
the only sanctioned place for plugin state, e.g. `Data\hello\count.dat`.

Reference plugin: [`examples/plugin_skeleton/skeleton.c`](../examples/plugin_skeleton/skeleton.c)
(+ its `hello.tbplugin.json`). CI compiles it as proof the header matches
the host ABI; it is deliberately NOT in the release zip.

## v0.36: the sidecar manifest (`*.tbplugin.json`)

Drop `<dllname>.tbplugin.json` next to the dll. Flat JSON, all fields
optional:

| Key | Meaning |
|---|---|
| `words` | command words this plugin owns (the host enforces them) |
| `author` | author shown on Settings > Plugins |
| `hostmin` | minimum host version, e.g. `"0.36.0"` - refuses to load below it |
| `version` / `description` | optional metadata |

The word law runs at load time (same rule `tools/audit-words.ps1` checks in
CI): a declared word that collides with a host command (`/help`, `/stats`,
`/settings`, `/reload`, `/reread`, `/exit`, `/plugdebug`, `/timers`) or with
a word another plugin already owns blocks the load with
`plugin manifest: "x" collides with <owner> - <dll> not loaded`. A missing
manifest is fine (ABI-v1 plugins load unchanged); an unreadable one warns
`plugin manifest: <file> unreadable - ignored` and still loads. Settings >
Plugins shows the manifest block (`manifest v.. by .. needs>=.. / words: ..`).

## v0.36: hot reload

`/reload <plugin>` (console command, tab-completable) and the **Reload**
button on Settings > Plugins both do: guarded `Unload` -> `FreeLibrary` ->
`LoadLibrary` -> rebind -> re-read manifest -> guarded `Load` -> counters
zeroed. ET caches are invalidated. Log: `plugin: reloaded <key> (Nms)`;
failures log `plugin: reload FAILED: <key> (GetLastError=..)` and the plugin
stays out until the next attempt.

## v0.36: performance & observability (§4b)

- `/plugdebug` shows per plugin: `<key> ok <hits>/<avg>ms et <calls>/<avg>ms c<hits>`
  (console curates the first 6; the log line carries ALL plugins).
- `Load()` longer than 20 ms logs `plugin: <key> Load took Nms` - the budget
  is measured, not claimed.
- ET results are cached per plugin (LRU, 8 rows-sets, 8 s TTL, keyed by the
  exact ANSI line) so re-filtering a drop-down burst does not re-enter the
  dll. The cache is dropped by `/reread`, `/reload` and plugin rescan.

## v0.36: hard rules (learned the hard way)

- **Window classes must be registered under the plugin's own module**, never
  `GetModuleHandleW(NULL)` (that is the EXE): capture `self` via
  `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, &local, &self)`,
  pass it as `hInstance` to both `RegisterClass` and `CreateWindowEx`, and
  call `UnregisterClassW` in `Unload`. Classes registered under the exe
  survive `FreeLibrary` and the next `/reload` binds windows to a `WndProc`
  in freed memory (0xc0000005). `tbplugin_skeleton` and the shipped
  cliphist/sysmon/timer follow this pattern.
- Every plugin export can be called at any time - keep `Load`/`Unload`
  idempotent, never fault across the ABI (the host guards calls and retires
  a faulting plugin for the session, but a retired plugin answers nothing).
- Offline-first: touch network/clipboard/SendInput only from a line the user
  actually typed, and never in headless mode (`TYPEBOX_HEADLESS=1`).
