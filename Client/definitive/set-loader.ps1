#!/usr/bin/env pwsh
#requires -Version 7.0
# Bypass startup loading without uninstalling the runtime or losing adapter work.
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'None')]
param(
    [Parameter(Mandatory)][ValidateSet('Disable', 'Enable', 'Status')][string]$Action,
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$allowedPaths = @(
    'version.dll', 'cleo_redux64.asi', 'CLEO/CLEO_PLUGINS/dylib64.cleo', 'CLEO/CLEO_PLUGINS/Input64.cleo',
    'CLEO/.config/cleo.ini', 'CLEO/.config/sa_unreal.json', 'CLEO/.config/unknown_x64.json',
    'CLEO/de-online/index.js', 'CLEO/de-online/controller.mjs', 'CLEO/de-online/config.mjs',
    'CLEO/de-online/mod.json', 'CLEO/de-online/de_client_bridge.dll',
    'DE_ONLINE_LICENSES/CLEO_REDUX_LICENSE.txt', 'DE_ONLINE_LICENSES/ULTIMATE_ASI_LOADER_LICENSE.txt',
    'DE_ONLINE_LICENSES/MTA_GPLv3.txt', 'DE_ONLINE_LICENSES/SANNY_BUILDER_LIBRARY_README.md'
)

function Assert-NoLinks([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if (Test-Path -LiteralPath $candidate) {
            if ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing a linked path: $candidate"
            }
        }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}

function Assert-GameClosed {
    if (Get-Process -Name SanAndreas, PlayGTASanAndreas -ErrorAction SilentlyContinue) {
        throw 'Close San Andreas Definitive Edition and its game bootstrap before changing the loader.'
    }
}

function Write-DurableRecord([string]$Path, [object]$Record) {
    Assert-NoLinks $Path
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($Record | ConvertTo-Json -Depth 6) + "`n")
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
}

$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path.TrimEnd('\', '/')
Assert-NoLinks $gameRoot
if ($Action -ne 'Status') { Assert-GameClosed }
$manifestPath = Join-Path $gameRoot 'de-online-install-manifest.json'
Assert-NoLinks $manifestPath
$installed = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($installed.packageId -cne 'de-online-prototype' -or $installed.formatVersion -ne 1 -or
    $installed.gameDirectory -ne $gameRoot) {
    throw 'Installation manifest identity or target directory does not match.'
}
$relativePaths = @($installed.files | ForEach-Object { $_.path })
# The additive read-only probe script is optional for the original intact
# sixteen-file install. Once recorded it receives the same hash/path checks.
if ($relativePaths -ccontains 'CLEO/de-online/native-pad-reference.mjs') {
    $allowedPaths += 'CLEO/de-online/native-pad-reference.mjs'
}
$uniquePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($relative in $relativePaths) {
    if ($relative -cnotin $allowedPaths -or -not $uniquePaths.Add($relative)) {
        throw 'Installation manifest does not contain exactly the known owned files.'
    }
}
if ($relativePaths.Count -ne $allowedPaths.Count) { throw 'Installation manifest has missing owned files.' }

$enabledPath = Join-Path $gameRoot 'version.dll'
$disabledPath = Join-Path $gameRoot 'version.dll.de-online-disabled'
Assert-NoLinks $enabledPath
Assert-NoLinks $disabledPath
$hasEnabled = Test-Path -LiteralPath $enabledPath
$hasDisabled = Test-Path -LiteralPath $disabledPath
# Two copies are ambiguous, and neither copy cannot be recovered by guessing.
if ($hasEnabled -eq $hasDisabled) { throw 'Loader collision or missing loader: exactly one enabled or disabled copy is required.' }
$state = if ($hasEnabled) { 'Enabled' } else { 'Disabled' }
foreach ($entry in $installed.files) {
    $destination = [IO.Path]::GetFullPath((Join-Path $gameRoot $entry.path))
    if (-not $destination.StartsWith($gameRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        $entry.destination -ne $destination -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$') {
        throw 'Installation manifest contains an invalid destination or hash.'
    }
    $actualPath = if ($entry.path -ceq 'version.dll' -and $state -eq 'Disabled') { $disabledPath } else { $destination }
    Assert-NoLinks $actualPath
    if (-not (Test-Path -LiteralPath $actualPath -PathType Leaf)) { throw "Installed file is missing: $actualPath" }
    if ((Get-FileHash -LiteralPath $actualPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        throw "Installed file was modified; loader was not changed: $actualPath"
    }
}

$result = [ordered]@{
    action = $Action; state = $state; verifiedFiles = $allowedPaths.Count; gameDirectory = $gameRoot
    loaderPath = $(if ($hasEnabled) { $enabledPath } else { $disabledPath })
    manifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    recoveryDirectory = $null
    recovery = 'File paths and hashes determine state even if a restart interrupted journal completion.'
}
$desiredState = if ($Action -eq 'Disable') { 'Disabled' } else { 'Enabled' }
if ($Action -eq 'Status' -or $state -eq $desiredState) { [pscustomobject]$result; return }
$source = $result.loaderPath
$destination = if ($Action -eq 'Disable') { $disabledPath } else { $enabledPath }
if (-not $PSCmdlet.ShouldProcess($source, "$Action startup loading by renaming to $destination")) {
    [pscustomobject]$result
    return
}

$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$recoveryDirectory = Join-Path $repositoryRoot ('Build/definitive-loader-test/' + [Guid]::NewGuid().ToString('N'))
Assert-NoLinks $recoveryDirectory
[IO.Directory]::CreateDirectory($recoveryDirectory) | Out-Null
$record = [ordered]@{
    action = $Action; gameDirectory = $gameRoot; manifestSha256 = $result.manifestSha256
    source = $source; destination = $destination; desiredState = $desiredState
}
# Flush intent before the rename. Recovery never depends on this record being
# completed: the unchanged manifest validates the renamed DLL after a reboot.
Write-DurableRecord (Join-Path $recoveryDirectory 'prepared.json') $record
Assert-GameClosed
Assert-NoLinks $source
Assert-NoLinks $destination
if (Test-Path -LiteralPath $destination) { throw 'Loader destination appeared after validation; nothing was overwritten.' }
# The two-argument move refuses overwriting. Keeping the manifest untouched also
# blocks ordinary updates/removal while the loader is bypassed.
[IO.File]::Move($source, $destination)
$result.state = $desiredState
$result.loaderPath = $destination
$result.recoveryDirectory = $recoveryDirectory
Write-DurableRecord (Join-Path $recoveryDirectory 'completed.json') $result
[pscustomobject]$result
