# Versioning policy

Version format: `MAJOR.MINOR.PATCH`. Bump size is decided by the **contract
surface** the change touches - never by lines of code or effort spent.
A 5000-line internal refactor is a patch; 20 lines that break `TypeBox.ini`
are a major.

## Bump triggers

| Change | Example | Bump |
|---|---|---|
| Bug fixes only. No new commands, no new INI keys, plugins load exactly as before | fix the `yt` instance fallback on HTTP 429 | `0.27.1` (patch) |
| Any new user-visible feature with full backwards compatibility: old `TypeBox.ini` still works, existing commands keep their semantics, new INI keys are optional | `/tabs`, `Ctrl+R`, `/day`, a new plugin, `[UI] Headless=1` | `0.28.0` (minor) |
| A contract break the user must act on: INI needs manual migration (the host does not auto-migrate), danger-gate or hotkey semantics change, a command is removed, or the plugin ABI changes in a way third-party plugins cannot survive | renaming `[UI]` keys with no fallback | `1.0.0` (major) |

## Rules

1. **Backwards compatibility is the test, size is not.** If a user can drop
   the new exe next to their old `TypeBox.ini` and everything behaves, it is
   at most a minor.
2. **Patch is for hotfixes.** A release wave that turns out to have a bug
   ships as `x.y.z+1`; do not swallow fixes into the next feature wave.
3. **`0.x` is not "beta".** For an application it simply means the major has
   not been declared yet. `1.0.0` is a conscious point, reached when:
   (a) the plugin SDK is versioned and its ABI is committed to, and
   (b) no breaking INI migrations are planned.
4. **Host + plugins upgrade atomically.** `/update` mirrors the whole staged
   tree (host *and* `Plugins\*.dll`), so internal plugin-ABI changes are not
   breaking for our users - they are minor. They become major only when
   third-party plugins exist and would break.
5. **Never skip or reuse versions.** `/update` and CI depend on tag ordering;
   once a tag is published it is burned.
6. **Docs-only commits do not bump.** Documentation, screenshots and tests
   ride along with the next real bump.

## Naming

Release tags are `v{MAJOR}.{MINOR}.{PATCH}` and are created via the GitHub
API (`POST /repos/.../git/refs`), one commit per release wave. The release
title follows `v{version}: {wave name}`.
