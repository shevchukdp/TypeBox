# AGENTS.md - generating a TypeBox plugin from a spec

You are an agent asked to write a TypeBox plugin. TypeBox is a Windows
hotkey console/launcher; a plugin is a plain native DLL with eight optional
`cdecl` exports. Everything an agent needs is in this file - do not
reverse-engineer the host, do not invent exports beyond the list below.

## Ground truth to read first

1. [`include/typebox_plugin.h`](include/typebox_plugin.h) - the SDK header:
   types (`TInfo`, `TExec`), `TB_EXPORT`, helpers `TbNoRun()` /
   `TbAnswer(str)` / `TbInfo(...)` / `TbPluginDirAt(self)`.
2. [`examples/plugin_skeleton/`](examples/plugin_skeleton/skeleton.c) -
   a complete, compiling reference plugin (`hello`) with a sidecar
   `hello.tbplugin.json`. Copy the folder, rename, change the word and the
   answer. This is the fastest correct path.
3. [`docs/PLUGIN_API.md`](docs/PLUGIN_API.md) - ABI table, manifest spec,
   hot-reload semantics, the hard rules below.

## The contract in one paragraph

The host loads every `Plugins\*.dll`, binds the exports it finds
(`Load`, `Unload`, `GetInfo`, `GetCount`, `GetString`, `GetCountET`,
`GetStringET`, `RunString`, all `__cdecl`, all optional) and then:
`GetCount`/`GetString` feed drop-down candidates, `GetCountET(line)` +
`GetStringET(i)` feed drop-down template rows for the typed line, and
`RunString(line)` is asked about EVERY typed line - return `TbNoRun()`
immediately for lines that are not yours, `TbAnswer(text)` to consume a
line and put `text` in the console. Strings are ANSI (CP_ACP). Keep
`GetString` returns alive for the process lifetime (static storage).

## Build

```
gcc -shared -O2 -I include -D<DLLNAME>_BUILDING_DLL -o Plugins/<name>.dll <name>.c -luser32
```

(MSVC: `cl /LD /I include /Fe:Plugins\<name>.dll <name>.c user32.lib`.)
`TB_BUILDING_DLL` must be defined before including the SDK header, or the
exports are not decorated for the host's `GetProcAddress` probes.

## Word law

Pick a command word no one owns: check the 19 shipped plugins
(`tools/audit-words.ps1` prints the roster; run it - CI runs it too) and
the host words `/help /stats /plugdebug /reload /reread /settings /exit
/timers`. Declare your words in the sidecar `<name>.tbplugin.json`
(`{"words":["hello"],"author":"...","hostmin":"0.36.0"}`); the host
refuses to load a dll whose declared word collides with a host command or
another plugin. A missing manifest still loads (v1 compat) but you lose
the Settings metadata block.

## Hard rules (violations get your dll retired or worse)

- Offline-first: touch network/registry-writes/power-commands/SendInput
  ONLY from a line the user typed, and NEVER under `TYPEBOX_HEADLESS=1`
  (answer `would run ...` / `would send ...` instead).
- Window classes: capture your own `HMODULE` (DllMain or
  `GetModuleHandleExW(FROM_ADDRESS)`) and pass it as `hInstance` to BOTH
  `RegisterClass(Ex)W` and `CreateWindowExW`; `UnregisterClassW` in
  `Unload`. Using `GetModuleHandleW(NULL)` (the exe) leaves a class with a
  dangling `WndProc` after `/reload` -> 0xc0000005.
- Never crash across the ABI: the host guards every call and permanently
  retires a faulting plugin for the session (`<key> FAULT` in `/plugdebug`).
- State lives in `Data\<dllname>\` (the host pre-creates it; get the path
  from `TbPluginDirAt` / `GetPluginDir`), never beside the exe.
- `Load` should stay under ~20 ms (the host logs `Load took Nms` beyond
  that) and must be safe to call again after `Unload` - `/reload` does
  exactly that.

## Prove it before handing over

```
set TYPEBOX_HEADLESS=1
set TB_QUIET=1
TypeBox.exe            # boots, loads Plugins\, writes TypeBox.log
```

The log must show `plugin manifest: <name> ok`, no `FAULTED`, and
`/plugdebug` (typed line) must list `<name> ok <hits>/<avg>ms ...`. Then
run `tools/audit-words.ps1` - it must end with `audit-words: OK`.
