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
    'CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll',
    'CLEO/de-online/xmll.dll', 'CLEO/de-online/mta/config/coreconfig.xml',
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
$guiPaths = @('CLEO/de-online/cgui.dll',
    'CLEO/de-online/mta/cgui/sans.ttf', 'CLEO/de-online/mta/cgui/saheader.ttf', 'CLEO/de-online/mta/cgui/sagothic.ttf',
    'CLEO/de-online/mta/cgui/Imageset.xsd', 'CLEO/de-online/mta/cgui/GUIScheme.xsd', 'CLEO/de-online/mta/cgui/Font.xsd',
    'CLEO/de-online/mta/cgui/Falagard.xsd', 'CLEO/de-online/mta/cgui/unifont.ttf',
    'CLEO/de-online/skins/Default/CGUI.xml', 'CLEO/de-online/skins/Default/CGUI.is.xml',
    'CLEO/de-online/skins/Default/CGUI.lnf.xml', 'CLEO/de-online/skins/Default/CGUI.png')
$frontendPaths = @(
    'CLEO/de-online/mta/cgui/images/background.png',
    'CLEO/de-online/mta/cgui/images/background_logo.png',
    'CLEO/de-online/mta/cgui/images/mta_filler.png',
    'CLEO/de-online/mta/cgui/images/version.png',
    'CLEO/de-online/mta/cgui/images/the_language_icon.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/locked.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search-servers.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search-players.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/refresh.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/favorite.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/connect.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/info.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/trashcan.png',
    'CLEO/de-online/mta/locale/en_US/menu_disconnect.png',
    'CLEO/de-online/mta/locale/en_US/menu_quick_connect.png',
    'CLEO/de-online/mta/locale/en_US/menu_browse_servers.png',
    'CLEO/de-online/mta/locale/en_US/menu_settings.png',
    'CLEO/de-online/mta/locale/en_US/menu_about.png',
    'CLEO/de-online/mta/locale/en_US/menu_quit.png',
    'CLEO/de-online/mta/locale/en_US/latest_news.png',
    'CLEO/de-online/mta/locale/en_US/client.pot')
$guiCount = @($relativePaths | Where-Object { $_ -in $guiPaths }).Count
$frontendCount = @($relativePaths | Where-Object { $_ -in $frontendPaths }).Count
if ($frontendCount -ne 0 -and ($frontendCount -ne 22 -or $guiCount -ne 13)) { throw 'Incomplete MTA Core GUI frontend package ownership.' }
if ($guiCount -ne 0 -and $guiCount -ne $guiPaths.Count) { throw 'Incomplete MTA Core GUI startup package ownership.' }
if ($guiCount) { $allowedPaths += $guiPaths }
if ($frontendCount) { $allowedPaths += $frontendPaths }
if ($relativePaths.Count -ne $allowedPaths.Count -or @($relativePaths | Select-Object -Unique).Count -ne $allowedPaths.Count -or
    @(Compare-Object $allowedPaths $relativePaths).Count -ne 0) {
    throw 'Package manifest must contain exactly the known owned file paths.'
}
$sourceRoot = Join-Path $packageRoot 'Win64'
$actualPaths = @(Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | ForEach-Object {
    [IO.Path]::GetRelativePath($sourceRoot, $_.FullName).Replace('\', '/')
})
if (@(Compare-Object $allowedPaths $actualPaths).Count -ne 0) { throw 'Package has unrecorded or missing files.' }
$configText = [IO.File]::ReadAllText((Join-Path $sourceRoot 'CLEO/de-online/config.mjs'))

# The clock package has no GUI or input lease. Its marker is optional only for
# intact historical packages, and cannot hide a second active trial.
$clockMarkers = [regex]::Matches($configText, 'export const MTA_CORE_CLOCK_REFERENCE = (true|false);')
if ($clockMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_CLOCK_REFERENCE\b').Count -ne $clockMarkers.Count) {
    throw 'Invalid MTA Core clock reference configuration marker.'
}
$clockSetMarkers = [regex]::Matches($configText, 'export const MTA_CORE_CLOCK_SET_REFERENCE = (true|false);')
if ($clockSetMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_CLOCK_SET_REFERENCE\b').Count -ne $clockSetMarkers.Count) {
    throw 'Invalid MTA Core clock Set reference configuration marker.'
}
$clockEnabled = $clockMarkers.Count -eq 1 -and $clockMarkers[0].Groups[1].Value -eq 'true'
$clockSetEnabled = $clockSetMarkers.Count -eq 1 -and $clockSetMarkers[0].Groups[1].Value -eq 'true'
if ($clockEnabled -and $clockSetEnabled) { throw 'Clock Read and Set trials require independent opt-in packages.' }
if ($clockEnabled -or $clockSetEnabled) {
    if ($relativePaths.Count -ne 23 -or $guiCount -or $frontendCount -or $Mode -ne 'Smoke' -or
        $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core clock reference requires base ownership and separate opt-in Smoke mode.'
    }
}

$guiPattern = 'export const MTA_CORE_GUI_STARTUP_REFERENCE = (true|false);'
$guiMarkers = [regex]::Matches($configText, $guiPattern)
if ($guiMarkers.Count -gt 1 -or ($guiCount -and $guiMarkers.Count -ne 1)) { throw 'Invalid GUI startup configuration marker.' }
if ($guiMarkers.Count -eq 1 -and $guiMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $guiCount -or $Mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI startup requires the complete GUI group and separate opt-in Smoke mode.'
    }
}
# A surface package shares the complete asset group with offscreen startup,
# but its activation is a separate marker and may never inherit another trial.
$surfaceMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_SURFACE_REFERENCE = (true|false);')
if ($surfaceMarkers.Count -gt 1) { throw 'Invalid GUI surface configuration marker.' }
if ($surfaceMarkers.Count -eq 1 -and $surfaceMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $guiCount -or $Mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI surface requires the complete GUI group and separate opt-in Smoke mode.'
    }
}
$frontendMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_REFERENCE = (true|false);')
if ($frontendMarkers.Count -gt 1 -or ($frontendCount -and $frontendMarkers.Count -ne 1)) { throw 'Invalid GUI frontend configuration marker.' }
if ($frontendMarkers.Count -eq 1 -and $frontendMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $frontendCount -or $Mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI frontend requires the complete frontend group and separate opt-in Smoke mode.'
    }
}
$frontendInputMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE = (true|false);')
if ($frontendInputMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE\b').Count -ne $frontendInputMarkers.Count) { throw 'Invalid GUI frontend input configuration marker.' }
if ($frontendInputMarkers.Count -eq 1 -and $frontendInputMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $frontendCount -or $Mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI frontend input requires the complete frontend group and separate opt-in Smoke mode.'
    }
}
$frontendStartupMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = (true|false);')
if ($frontendStartupMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE\b').Count -ne $frontendStartupMarkers.Count) { throw 'Invalid GUI frontend startup configuration marker.' }
if ($frontendStartupMarkers.Count -eq 1 -and $frontendStartupMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $frontendCount -or $Mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI frontend startup requires the complete frontend group and separate opt-in Smoke mode.'
    }
}
# Optional for intact historical packages; strict and exclusive when present.
$persistentMarkers = [regex]::Matches($configText, 'export const MTA_CORE_PERSISTENT_FRONTEND = (true|false);')
if ($persistentMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_PERSISTENT_FRONTEND\b').Count -ne $persistentMarkers.Count) { throw 'Invalid persistent frontend configuration marker.' }
if ($persistentMarkers.Count -eq 1 -and $persistentMarkers[0].Groups[1].Value -eq 'true') {
    if ($frontendCount -ne 22 -or $guiCount -ne 13 -or $relativePaths.Count -ne 58 -or $Mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE) = true;') {
        throw 'Persistent MTA frontend requires complete frontend ownership and separate opt-in Smoke mode.'
    }
}

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
