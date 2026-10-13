#!/usr/bin/env pwsh
#requires -Version 7.0
# This file is part of Multi Theft Auto. See LICENSE in the repository root.
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'None')]
param(
    [ValidateSet('Diagnostic', 'Smoke', 'Online')][string]$Mode = 'Smoke',
    [string]$PackageDirectory,
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$allowedPaths = @(
    'version.dll', 'cleo_redux64.asi', 'CLEO/CLEO_PLUGINS/dylib64.cleo', 'CLEO/CLEO_PLUGINS/Input64.cleo',
    'CLEO/.config/cleo.ini', 'CLEO/.config/sa_unreal.json', 'CLEO/.config/unknown_x64.json',
    'CLEO/de-online/index.js', 'CLEO/de-online/controller.mjs', 'CLEO/de-online/config.mjs',
    'CLEO/de-online/mod.json', 'CLEO/de-online/de_client_bridge.dll',
    'CLEO/de-online/native-pad-reference.mjs',
    'CLEO/de-online/native-movement-trial.mjs', 'CLEO/de-online/game_de.dll',
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

if (-not $PackageDirectory) {
    $repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
    $PackageDirectory = Join-Path $repositoryRoot "Build/definitive-package/$Mode"
}
$packageRoot = (Resolve-Path -LiteralPath $PackageDirectory).Path
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path.TrimEnd('\', '/')
Assert-NoLinks $packageRoot
Assert-NoLinks $gameRoot
if (Get-Process -Name SanAndreas, PlayGTASanAndreas -ErrorAction SilentlyContinue) {
    throw 'Close San Andreas Definitive Edition and its game bootstrap before installing.'
}
$gamePath = Join-Path $gameRoot 'SanAndreas.exe'
Assert-NoLinks $gamePath
if ((Get-FileHash -LiteralPath $gamePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $knownGameHash) {
    throw 'Unsupported SanAndreas.exe fingerprint; installation requires the verified Steam executable.'
}
$manifestPath = Join-Path $packageRoot 'manifest.json'
Assert-NoLinks $manifestPath
$packageManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($packageManifest.packageId -ne 'de-online-prototype' -or $packageManifest.formatVersion -ne 1 -or
    $packageManifest.gameSha256 -ne $knownGameHash -or $packageManifest.mode -ne $Mode) {
    throw 'Package identity, mode, format or game fingerprint does not match this installer.'
}
$relativePaths = @($packageManifest.files | ForEach-Object { $_.path })
if ($relativePaths.Count -ne $allowedPaths.Count -or @($relativePaths | Select-Object -Unique).Count -ne $allowedPaths.Count -or
    @(Compare-Object $allowedPaths $relativePaths).Count -ne 0) {
    throw 'Package manifest must contain exactly the known owned file paths.'
}
$sourceRoot = Join-Path $packageRoot 'Win64'
$actualPaths = @(Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | ForEach-Object {
    [IO.Path]::GetRelativePath($sourceRoot, $_.FullName).Replace('\', '/')
})
if (@(Compare-Object $allowedPaths $actualPaths).Count -ne 0) { throw 'Package has unrecorded or missing files.' }
$installedManifestPath = Join-Path $gameRoot 'de-online-install-manifest.json'
# Owning a fresh CLEO directory makes rollback/uninstall provable and avoids
# replacing a user's existing scripts, runtime, loader or permission settings.
foreach ($relative in @('CLEO', 'DE_ONLINE_LICENSES', 'version.dll', 'cleo_redux64.asi', 'de-online-install-manifest.json')) {
    $target = Join-Path $gameRoot $relative
    Assert-NoLinks $target
    if (Test-Path -LiteralPath $target) { throw "Installation would replace an existing path: $target" }
}
$planned = @()
foreach ($entry in $packageManifest.files) {
    if ($entry.sha256 -notmatch '^[0-9a-f]{64}$') { throw 'Invalid package hash.' }
    $source = Join-Path $sourceRoot $entry.path
    $destination = [IO.Path]::GetFullPath((Join-Path $gameRoot $entry.path))
    if (-not $destination.StartsWith($gameRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Package destination escapes the game directory.'
    }
    Assert-NoLinks $source
    Assert-NoLinks $destination
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        throw "Package content hash mismatch: $source"
    }
    if (Test-Path -LiteralPath $destination) { throw "Target already exists: $destination" }
    $planned += [ordered]@{ path = $entry.path; destination = $destination; sha256 = $entry.sha256; source = $source }
}
Write-Host "Verified $Mode package and the exact game fingerprint. Planned owned files: $($planned.Count)."
if (-not $PSCmdlet.ShouldProcess($gameRoot, "Install the verified $Mode Definitive Edition prototype")) { return }

$createdFiles = [Collections.Generic.List[string]]::new()
$createdDirectories = [Collections.Generic.List[string]]::new()
try {
    foreach ($entry in $planned) {
        $parent = [IO.Path]::GetDirectoryName($entry.destination)
        $missing = [Collections.Generic.List[string]]::new()
        while (-not (Test-Path -LiteralPath $parent)) {
            $missing.Add($parent)
            $parent = [IO.Path]::GetDirectoryName($parent)
        }
        for ($index = $missing.Count - 1; $index -ge 0; $index--) {
            Assert-NoLinks $missing[$index]
            [IO.Directory]::CreateDirectory($missing[$index]) | Out-Null
            $createdDirectories.Add($missing[$index])
        }
        Assert-NoLinks $entry.destination
        # CreateNew refuses a target created after preflight. Register ownership
        # before copying bytes so a partial-write failure is rolled back too.
        $destinationStream = [IO.File]::Open($entry.destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $createdFiles.Add($entry.destination)
        try {
            $sourceStream = [IO.File]::OpenRead($entry.source)
            try { $sourceStream.CopyTo($destinationStream) } finally { $sourceStream.Dispose() }
        } finally {
            $destinationStream.Dispose()
        }
        if ((Get-FileHash -LiteralPath $entry.destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
            throw "Installed content hash mismatch: $($entry.destination)"
        }
    }
    $installed = [ordered]@{
        packageId = 'de-online-prototype'; formatVersion = 1; mode = $Mode
        gameDirectory = $gameRoot; gameSha256 = $knownGameHash
        files = @($planned | ForEach-Object { [ordered]@{ path = $_.path; destination = $_.destination; sha256 = $_.sha256 } })
    }
    $stream = [IO.File]::Open($installedManifestPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    $createdFiles.Add($installedManifestPath)
    try {
        $content = [Text.UTF8Encoding]::new($false).GetBytes(($installed | ConvertTo-Json -Depth 6) + "`n")
        $stream.Write($content, 0, $content.Length)
    } finally {
        $stream.Dispose()
    }
} catch {
    $originalError = $_
    foreach ($file in @($createdFiles | Sort-Object { $_.Length } -Descending)) {
        if (Test-Path -LiteralPath $file) {
            try { Assert-NoLinks $file; Remove-Item -LiteralPath $file } catch { Write-Warning "Rollback could not remove: $file" }
        }
    }
    foreach ($directory in @($createdDirectories | Sort-Object { $_.Length } -Descending)) {
        try {
            Assert-NoLinks $directory
            if (Test-Path -LiteralPath $directory) {
                if (@(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) { Remove-Item -LiteralPath $directory }
            }
        } catch { Write-Warning "Rollback retained directory: $directory" }
    }
    throw $originalError
}
Write-Host "Installed owned files only. Manifest: $installedManifestPath"
Write-Host 'Launch the game through Steam or Rockstar Games Launcher when ready.'
