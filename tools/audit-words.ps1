# audit-words.ps1 - global command-word uniqueness for TypeBox (v0.31.0 law).
#
# Why: the v0.30.0 release shipped with nettools.dll shadowing weather.dll's
# `weather` (discovery is alphabetical, first plugin whose RunString returns
# run!=0 wins). Per-wave suites copy only their own plugin subset, so they
# could not see it. This script reads every example plugin's source, extracts
# its GetCount()/GetString() command words, folds in the host's own words, and
# FAILS (exit 1) on any collision - before a DLL can ever shadow another.
#
# Wiring: run by build-plugins-v0310.ps1 before compiling, by CI, and by the
# full-roster release gate. Word forms understood:
#   static const char* k[N] = { "a", "b" };
#   GetString(int n) { return n == 0 ? "calc" : NULL; }   (calc/vol style)
#
# Usage: powershell -File tools\audit-words.ps1 [-RepoRoot <path>]
param([string]$RepoRoot)
$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$exDir = Join-Path $RepoRoot 'examples'
if (-not (Test-Path $exDir)) { "FAIL: no examples dir under $RepoRoot"; exit 2 }

# Host-claimed words (main.cpp). kInternalCmds are all slash-form -> no clash
# possible with plugin words; these bare words ARE host words (v0.23 pos/stayawake,
# v0.33 search/snippet, v0.34 in/at, v0.35 favs/autostart):
$hostWords = @('pos', 'stayawake', 'search', 'snippet', 'in', 'at', 'favs', 'autostart')

$words = @{}        # word -> list of owners
$fails = @()

Get-ChildItem $exDir -Directory -Filter 'plugin_*' | Sort-Object Name | ForEach-Object {
    $plug = $_.Name -replace '^plugin_', ''
    $src  = Get-ChildItem $_.FullName -Filter '*.c' | Select-Object -First 1
    if (-not $src) { $fails += "$plug : no .c source"; return }
    $text = Get-Content $src.FullName -Raw
    $found = @()

    # form 1: k[] table
    $m = [regex]::Match($text, 'static\s+const\s+char\s*\*\s*k\s*\[\s*\d*\s*\]\s*=\s*\{([^}]*)\}')
    if ($m.Success) {
        $found = @([regex]::Matches($m.Groups[1].Value, '"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    } else {
        # form 2: GetString body with inline ternaries (calc/vol)
        $gm = [regex]::Match($text, 'GetString\s*\(\s*(?:const\s+char\*\s*)?int\s+n\s*\)\s*\{(.*?)\}', 'Singleline')
        if ($gm.Success) {
            $found = @([regex]::Matches($gm.Groups[1].Value, '"([^"]+)"') |
                       ForEach-Object { $_.Groups[1].Value } |
                       Where-Object { $_ -and $_ -notmatch '\s' })   # ET rows carry trailing spaces
        }
    }
    if ($found.Count -eq 0) { $fails += "$plug : NO command words parsed (audit blind spot!)"; return }

    # GetCount sanity (warn-level only when k[] form: host uses GetCount anyway)
    $cm = [regex]::Match($text, 'GetCount\s*\(\s*void\s*\)\s*\{\s*return\s+(\d+)\s*;\s*\}')
    if ($cm.Success -and [int]$cm.Groups[1].Value -ne $found.Count) {
        "WARN ${plug}: GetCount=$($cm.Groups[1].Value) but parsed $($found.Count) words: [$($found -join ', ')]"
    }

    foreach ($w in $found) {
        $lw = $w.ToLowerInvariant()
        if (-not $words.ContainsKey($lw)) { $words[$lw] = @() }
        $words[$lw] += $plug
    }
}

# host-word collisions
foreach ($hw in $hostWords) {
    if ($words.ContainsKey($hw)) { $fails += "word '$hw' : claimed by host AND by $($words[$hw] -join ', ')" }
}
# plugin-vs-plugin collisions
foreach ($kv in $words.GetEnumerator() | Sort-Object Name) {
    if ($kv.Value.Count -gt 1) { $fails += "word '$($kv.Name)' : claimed by $($kv.Value -join ' + ')" }
}

"audit-words: $($words.Count) unique words across $($words.Values | ForEach-Object { $_[0] } | Sort-Object -Unique) plugin owners"
foreach ($f in $fails) { "FAIL: $f" }
if ($fails.Count) { "audit-words: $($fails.Count) collision(s) - FIX BEFORE BUILD"; exit 1 }
"audit-words: OK (no shadowing possible across the roster + host words)"
exit 0
