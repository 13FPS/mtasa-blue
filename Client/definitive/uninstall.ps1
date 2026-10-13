#!/usr/bin/env pwsh
#requires -Version 7.0
# This file is part of Multi Theft Auto. See LICENSE in the repository root.
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'None')]
param(
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64',
    [switch]$ValidateOnly
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

$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path.TrimEnd('\', '/')
Assert-NoLinks $gameRoot
if (Get-Process -Name SanAndreas, PlayGTASanAndreas -ErrorAction SilentlyContinue) {
    throw 'Close San Andreas Definitive Edition and its game bootstrap before uninstalling.'
}
$manifestPath = Join-Path $gameRoot 'de-online-install-manifest.json'
Assert-NoLinks $manifestPath
$installed = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($installed.packageId -ne 'de-online-prototype' -or $installed.formatVersion -ne 1 -or
    $installed.gameDirectory -ne $gameRoot) {
    throw 'Installation manifest identity or target directory does not match.'
}
$relativePaths = @($installed.files | ForEach-Object { $_.path })
# Version 1 installations existed before the native pad module. Preserve their
# exact 16-file removal contract while validating all 17 files in newer installs.
if ($relativePaths -contains 'CLEO/de-online/native-pad-reference.mjs') {
    $allowedPaths += 'CLEO/de-online/native-pad-reference.mjs'
}
# Older 16/17-file installs remain removable. The new trial script and the
# real MTA backend are admitted together, never as arbitrary extra DLL paths.
$hasMovementScript = $relativePaths -contains 'CLEO/de-online/native-movement-trial.mjs'
$hasGameDe = $relativePaths -contains 'CLEO/de-online/game_de.dll'
if ($hasMovementScript -ne $hasGameDe -or ($hasMovementScript -and $relativePaths -notcontains 'CLEO/de-online/native-pad-reference.mjs')) {
    throw 'Incomplete MTA movement package ownership.'
}
if ($hasMovementScript) { $allowedPaths += @('CLEO/de-online/native-movement-trial.mjs', 'CLEO/de-online/game_de.dll') }
$hasCore = $relativePaths -contains 'CLEO/de-online/core.dll'
$hasPthread = $relativePaths -contains 'CLEO/de-online/pthread.dll'
# Preserve the historical 16/17/19-file removal contracts. New installations
# own the reviewed Core/dependency pair only alongside the complete DE backend.
if ($hasCore -ne $hasPthread -or ($hasCore -and -not $hasMovementScript)) {
    throw 'Incomplete MTA Core startup package ownership.'
}
if ($hasCore) { $allowedPaths += @('CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll') }
if ($relativePaths.Count -ne $allowedPaths.Count -or @($relativePaths | Select-Object -Unique).Count -ne $allowedPaths.Count -or
    @(Compare-Object $allowedPaths $relativePaths).Count -ne 0) {
    throw 'Installation manifest does not contain exactly the known owned files.'
}
$targets = @()
foreach ($entry in $installed.files) {
    $target = [IO.Path]::GetFullPath((Join-Path $gameRoot $entry.path))
    if (-not $target.StartsWith($gameRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        $entry.destination -ne $target -or $entry.sha256 -notmatch '^[0-9a-f]{64}$') {
        throw 'Installation manifest contains an invalid destination or hash.'
    }
    Assert-NoLinks $target
    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "Installed file is missing: $target" }
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        throw "Installed file was modified; nothing has been removed: $target"
    }
    $targets += $target
}
# Validate every file before deleting any: an edited adapter/config requires
# review and must not be silently erased alongside the unmodified DLLs.
Write-Host "Verified all $($targets.Count) owned installed file hashes."
# Diagnostic collection and configuration updates need this validation without
# even proposing removal. Return before ShouldProcess and all deletion paths.
if ($ValidateOnly) { return }
if (-not $PSCmdlet.ShouldProcess($gameRoot, 'Uninstall only the unchanged files recorded by the prototype installer')) { return }
foreach ($target in $targets) {
    Assert-NoLinks $target
    Remove-Item -LiteralPath $target
}
Remove-Item -LiteralPath $manifestPath
$directories = @('CLEO/de-online', 'CLEO/CLEO_PLUGINS', 'CLEO/.config', 'DE_ONLINE_LICENSES', 'CLEO')
foreach ($relative in $directories) {
    $directory = Join-Path $gameRoot $relative
    Assert-NoLinks $directory
    # Generated typings, logs, saves and any later user files remain intact;
    # no recursive delete can accidentally absorb something we did not own.
    if ((Test-Path -LiteralPath $directory) -and @(Get-ChildItem -LiteralPath $directory -Force).Count -eq 0) {
        Remove-Item -LiteralPath $directory
    }
}
Write-Host 'Removed recorded prototype files. Generated or unrelated files were retained.'
