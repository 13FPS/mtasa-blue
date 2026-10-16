#!/usr/bin/env pwsh
#requires -Version 7.0
# Update verified owned files without removing the runtime or generated user data.
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'None')]
param(
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64',
    [string]$PackageDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-NoLinks([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if (Test-Path -LiteralPath $candidate) {
            $item = Get-Item -LiteralPath $candidate -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing a linked path: $candidate" }
        }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path.TrimEnd('\', '/')
& (Join-Path $PSScriptRoot 'uninstall.ps1') -GameDirectory $gameRoot -ValidateOnly 6>$null
$manifestPath = Join-Path $gameRoot 'de-online-install-manifest.json'
$originalManifest = [IO.File]::ReadAllBytes($manifestPath)
$installed = [Text.Encoding]::UTF8.GetString($originalManifest) | ConvertFrom-Json
if ((Get-FileHash -LiteralPath (Join-Path $gameRoot 'SanAndreas.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $installed.gameSha256) {
    throw 'Game build changed; investigate before updating the adapter.'
}
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$packageRoot = if ($PackageDirectory) { (Resolve-Path -LiteralPath $PackageDirectory).Path } else { Join-Path $repositoryRoot ('Build/definitive-package/' + $installed.mode) }
Assert-NoLinks $packageRoot
$package = Get-Content -LiteralPath (Join-Path $packageRoot 'manifest.json') -Raw | ConvertFrom-Json
if ($package.packageId -ne $installed.packageId -or $package.formatVersion -ne 1 -or
    $package.mode -ne $installed.mode -or $package.gameSha256 -ne $installed.gameSha256) { throw 'Package identity or mode differs.' }
$installedPaths = @($installed.files | ForEach-Object { $_.path })
$packagePaths = @($package.files | ForEach-Object { $_.path })
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
$guiCount = @($packagePaths | Where-Object { $_ -in $guiPaths }).Count
$frontendCount = @($packagePaths | Where-Object { $_ -in $frontendPaths }).Count
if ($frontendCount -ne 0 -and ($frontendCount -ne 22 -or $guiCount -ne 13)) { throw 'Incomplete MTA Core GUI frontend package ownership.' }
if ($guiCount -ne 0 -and $guiCount -ne 13) { throw 'Incomplete MTA Core GUI startup package ownership.' }
$removedPaths = @($installedPaths | Where-Object { $_ -notin $packagePaths })
if (@($packagePaths | Select-Object -Unique).Count -ne $packagePaths.Count -or
    @($removedPaths | Where-Object { $_ -notin ($guiPaths + $frontendPaths) }).Count -ne 0 -or
    @($removedPaths | Where-Object { $_ -in $guiPaths }).Count -notin @(0, 13) -or
    @($removedPaths | Where-Object { $_ -in $frontendPaths }).Count -notin @(0, 22)) { throw 'Package owned paths differ.' }
$addedPaths = @($packagePaths | Where-Object { $_ -notin $installedPaths })
# Admit only the named MTA probe/backend files from the newer package. All
# additions are checked for existing user bytes before any update is applied.
$allowedAdditions = @('CLEO/de-online/native-pad-reference.mjs', 'CLEO/de-online/native-movement-trial.mjs', 'CLEO/de-online/game_de.dll',
    'CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll',
    'CLEO/de-online/xmll.dll', 'CLEO/de-online/mta/config/coreconfig.xml') + $guiPaths + $frontendPaths
if (@($addedPaths | Where-Object { $_ -notin $allowedAdditions }).Count -ne 0) {
    throw 'Unsupported additive package path.'
}
# Validate the complete new ownership contract before touching any bytes; a
# truncated package must not leave an installation that cannot be uninstalled.
$hasMovementScript = $packagePaths -contains 'CLEO/de-online/native-movement-trial.mjs'
$hasGameDe = $packagePaths -contains 'CLEO/de-online/game_de.dll'
if ($hasMovementScript -ne $hasGameDe -or ($hasMovementScript -and $packagePaths -notcontains 'CLEO/de-online/native-pad-reference.mjs')) {
    throw 'Incomplete MTA movement package ownership.'
}
$hasCore = $packagePaths -contains 'CLEO/de-online/core.dll'
$hasPthread = $packagePaths -contains 'CLEO/de-online/pthread.dll'
# A partially added Core cannot be loaded safely or removed under the same
# ownership contract. Refuse the complete update before staging any file.
if ($hasCore -ne $hasPthread -or ($hasCore -and -not $hasMovementScript)) {
    throw 'Incomplete MTA Core startup package ownership.'
}
$hasXml = $packagePaths -contains 'CLEO/de-online/xmll.dll'
$hasBindConfig = $packagePaths -contains 'CLEO/de-online/mta/config/coreconfig.xml'
if ($hasXml -ne $hasBindConfig -or ($hasXml -and -not $hasCore)) {
    throw 'Incomplete MTA Core configured input package ownership.'
}
if ($guiCount -and ($packagePaths.Count -ne $(if ($frontendCount) { 58 } else { 36 }) -or -not $hasXml)) { throw 'Incomplete MTA Core GUI startup package ownership.' }
$sourceRoot = Join-Path $packageRoot 'Win64'
$actualPaths = @(Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | ForEach-Object { [IO.Path]::GetRelativePath($sourceRoot, $_.FullName).Replace('\', '/') })
if (@(Compare-Object $packagePaths $actualPaths).Count -ne 0) { throw 'Package has unrecorded or missing files.' }
$configText = [IO.File]::ReadAllText((Join-Path $sourceRoot 'CLEO/de-online/config.mjs'))
$guiMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_STARTUP_REFERENCE = (true|false);')
if ($guiMarkers.Count -gt 1 -or ($guiCount -and $guiMarkers.Count -ne 1)) { throw 'Invalid GUI startup configuration marker.' }
if ($guiMarkers.Count -eq 1 -and $guiMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $guiCount -or $package.mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI startup requires the complete GUI group and separate opt-in Smoke mode.'
    }
}
# A surface package shares the complete asset group with offscreen startup,
# but its activation is a separate marker and may never inherit another trial.
$surfaceMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_SURFACE_REFERENCE = (true|false);')
if ($surfaceMarkers.Count -gt 1) { throw 'Invalid GUI surface configuration marker.' }
if ($surfaceMarkers.Count -eq 1 -and $surfaceMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $guiCount -or $package.mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI surface requires the complete GUI group and separate opt-in Smoke mode.'
    }
}
$frontendMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_REFERENCE = (true|false);')
if ($frontendMarkers.Count -gt 1 -or ($frontendCount -and $frontendMarkers.Count -ne 1)) { throw 'Invalid GUI frontend configuration marker.' }
if ($frontendMarkers.Count -eq 1 -and $frontendMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $frontendCount -or $package.mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI frontend requires the complete frontend group and separate opt-in Smoke mode.'
    }
}
$frontendInputMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE = (true|false);')
if ($frontendInputMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE\b').Count -ne $frontendInputMarkers.Count) { throw 'Invalid GUI frontend input configuration marker.' }
if ($frontendInputMarkers.Count -eq 1 -and $frontendInputMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $frontendCount -or $package.mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI frontend input requires the complete frontend group and separate opt-in Smoke mode.'
    }
}
$frontendStartupMarkers = [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = (true|false);')
if ($frontendStartupMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE\b').Count -ne $frontendStartupMarkers.Count) { throw 'Invalid GUI frontend startup configuration marker.' }
if ($frontendStartupMarkers.Count -eq 1 -and $frontendStartupMarkers[0].Groups[1].Value -eq 'true') {
    if (-not $frontendCount -or $package.mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_PERSISTENT_FRONTEND) = true;') {
        throw 'MTA Core GUI frontend startup requires the complete frontend group and separate opt-in Smoke mode.'
    }
}
# Optional for intact historical packages; strict and exclusive when present.
$persistentMarkers = [regex]::Matches($configText, 'export const MTA_CORE_PERSISTENT_FRONTEND = (true|false);')
if ($persistentMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_PERSISTENT_FRONTEND\b').Count -ne $persistentMarkers.Count) { throw 'Invalid persistent frontend configuration marker.' }
if ($persistentMarkers.Count -eq 1 -and $persistentMarkers[0].Groups[1].Value -eq 'true') {
    if ($frontendCount -ne 22 -or $guiCount -ne 13 -or $packagePaths.Count -ne 58 -or $package.mode -ne 'Smoke' -or $configText -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE|MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE|MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE) = true;') {
        throw 'Persistent MTA frontend requires complete frontend ownership and separate opt-in Smoke mode.'
    }
}

$changes = @()
# Removal is limited to the complete, previously verified GUI group. Save each
# original just like replacements so a failed 36-to-23 update restores ownership.
foreach ($relative in $removedPaths) {
    $record = @($installed.files | Where-Object path -eq $relative)[0]
    $changes += [pscustomobject]@{ path=$record.destination; bytes=$null; original=[IO.File]::ReadAllBytes($record.destination); additive=$false; removal=$true; applied=$false }
}
$installed.files = @($installed.files | Where-Object { $_.path -notin $removedPaths })
foreach ($entry in $package.files) {
    if ($entry.sha256 -notmatch '^[0-9a-f]{64}$') { throw 'Invalid package hash.' }
    $sourcePath = Join-Path $packageRoot ('Win64/' + $entry.path)
    Assert-NoLinks $sourcePath
    $sourceBytes = [IO.File]::ReadAllBytes($sourcePath)
    $actualHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($sourceBytes)).ToLowerInvariant()
    if ($actualHash -ne $entry.sha256) { throw 'Package hash mismatch.' }
    $records = @($installed.files | Where-Object { $_.path -eq $entry.path })
    if ($records.Count -gt 1) { throw 'Duplicate installed file record.' }
    if ($records.Count -eq 0) {
        $destination = [IO.Path]::GetFullPath((Join-Path $gameRoot $entry.path))
        Assert-NoLinks $destination
        if (-not $destination.StartsWith($gameRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            (Test-Path -LiteralPath $destination)) { throw 'New owned reference path already exists or escapes game directory.' }
        # The nested standard MTA configuration may share existing user folders.
        # Refuse a file/linked ancestor before backups or changes, and never
        # claim those folders as owned when they already exist.
        $parent = [IO.Path]::GetDirectoryName($destination)
        while ($parent.StartsWith($gameRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            if ((Test-Path -LiteralPath $parent) -and -not (Test-Path -LiteralPath $parent -PathType Container)) {
                throw 'New owned reference parent is not a directory.'
            }
            $parent = [IO.Path]::GetDirectoryName($parent)
        }
        $record = [pscustomobject]@{ path=$entry.path; destination=$destination; sha256=$entry.sha256 }
        $installed.files = @($installed.files) + @($record)
        $changes += [pscustomobject]@{ path=$destination; bytes=$sourceBytes; original=$null; additive=$true; removal=$false; applied=$false }
        continue
    }
    $record = $records[0]
    if ($record.sha256 -ne $entry.sha256) {
        $changes += [pscustomobject]@{ path=$record.destination; bytes=$sourceBytes; original=[IO.File]::ReadAllBytes($record.destination); additive=$false; removal=$false; applied=$false }
        $record.sha256 = $entry.sha256
    }
}
if (-not $changes.Count) { Write-Host 'Installed package is current.'; return }
if (-not $PSCmdlet.ShouldProcess($gameRoot, "Update $($changes.Count) verified owned files")) { return }

function Write-AtomicFile([string]$Path, [byte[]]$Bytes, [bool]$Replace = $true) {
    $temporary = $Path + '.de-online-' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
        try { $stream.Write($Bytes, 0, $Bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
        [IO.File]::Move($temporary, $Path, $Replace)
    } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
}
# Keep an on-disk recovery copy before changing files, independent of chat memory.
$backupRoot = Join-Path $repositoryRoot ('Build/definitive-update-backups/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
[IO.File]::WriteAllBytes((Join-Path $backupRoot 'manifest.json'), $originalManifest)
for ($index = 0; $index -lt $changes.Count; $index++) {
    if (-not $changes[$index].additive) {
        [IO.File]::WriteAllBytes((Join-Path $backupRoot "$index.original"), $changes[$index].original)
    }
}
$createdDirectories = [Collections.Generic.List[string]]::new()
try {
    foreach ($change in $changes) {
        if ($change.removal) {
            Assert-NoLinks $change.path
            Remove-Item -LiteralPath $change.path
            $change.applied = $true
            continue
        }
        $parent = [IO.Path]::GetDirectoryName($change.path)
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
        Assert-NoLinks $change.path
        Write-AtomicFile $change.path $change.bytes (-not $change.additive)
        $change.applied = $true
    }
    $newManifest = [Text.UTF8Encoding]::new($false).GetBytes(($installed | ConvertTo-Json -Depth 6) + "`n")
    Write-AtomicFile $manifestPath $newManifest
} catch {
    $failure = $_
    foreach ($change in $changes) {
        if ($change.additive) {
            if ($change.applied -and (Test-Path -LiteralPath $change.path)) { Remove-Item -LiteralPath $change.path }
        } else {
            Write-AtomicFile $change.path $change.original
        }
    }
    Write-AtomicFile $manifestPath $originalManifest
    foreach ($directory in @($createdDirectories | Sort-Object Length -Descending)) {
        Assert-NoLinks $directory
        if ((Test-Path -LiteralPath $directory) -and @(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
            Remove-Item -LiteralPath $directory
        }
    }
    throw $failure
}
Write-Host "Updated $($changes.Count) owned files; prior bytes saved under $backupRoot"
