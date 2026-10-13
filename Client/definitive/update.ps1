#!/usr/bin/env pwsh
#requires -Version 7.0
# Update verified owned files without removing the runtime or generated user data.
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'None')]
param(
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path.TrimEnd('\', '/')
& (Join-Path $PSScriptRoot 'uninstall.ps1') -GameDirectory $gameRoot -ValidateOnly 6>$null
$manifestPath = Join-Path $gameRoot 'de-online-install-manifest.json'
$originalManifest = [IO.File]::ReadAllBytes($manifestPath)
$installed = [Text.Encoding]::UTF8.GetString($originalManifest) | ConvertFrom-Json
if ((Get-FileHash -LiteralPath (Join-Path $gameRoot 'SanAndreas.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $installed.gameSha256) {
    throw 'Game build changed; investigate before updating the adapter.'
}
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$packageRoot = Join-Path $repositoryRoot ('Build/definitive-package/' + $installed.mode)
$package = Get-Content -LiteralPath (Join-Path $packageRoot 'manifest.json') -Raw | ConvertFrom-Json
if ($package.packageId -ne $installed.packageId -or $package.formatVersion -ne 1 -or
    $package.mode -ne $installed.mode -or $package.gameSha256 -ne $installed.gameSha256) { throw 'Package identity or mode differs.' }
$installedPaths = @($installed.files | ForEach-Object { $_.path })
$packagePaths = @($package.files | ForEach-Object { $_.path })
if (@($packagePaths | Select-Object -Unique).Count -ne $packagePaths.Count -or
    @($installedPaths | Where-Object { $_ -notin $packagePaths }).Count -ne 0) { throw 'Package owned paths differ.' }
$addedPaths = @($packagePaths | Where-Object { $_ -notin $installedPaths })
# Admit only the named MTA probe/backend files from the newer package. All
# additions are checked for existing user bytes before any update is applied.
$allowedAdditions = @('CLEO/de-online/native-pad-reference.mjs', 'CLEO/de-online/native-movement-trial.mjs', 'CLEO/de-online/game_de.dll',
    'CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll')
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
$changes = @()
foreach ($entry in $package.files) {
    $sourcePath = Join-Path $packageRoot ('Win64/' + $entry.path)
    $sourceBytes = [IO.File]::ReadAllBytes($sourcePath)
    $actualHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($sourceBytes)).ToLowerInvariant()
    if ($actualHash -ne $entry.sha256) { throw 'Package hash mismatch.' }
    $records = @($installed.files | Where-Object { $_.path -eq $entry.path })
    if ($records.Count -gt 1) { throw 'Duplicate installed file record.' }
    if ($records.Count -eq 0) {
        $destination = [IO.Path]::GetFullPath((Join-Path $gameRoot $entry.path))
        if (-not $destination.StartsWith($gameRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            (Test-Path -LiteralPath $destination)) { throw 'New owned reference path already exists or escapes game directory.' }
        $record = [pscustomobject]@{ path=$entry.path; destination=$destination; sha256=$entry.sha256 }
        $installed.files = @($installed.files) + @($record)
        $changes += [pscustomobject]@{ path=$destination; bytes=$sourceBytes; original=$null; additive=$true; applied=$false }
        continue
    }
    $record = $records[0]
    if ($record.sha256 -ne $entry.sha256) {
        $changes += [pscustomobject]@{ path=$record.destination; bytes=$sourceBytes; original=[IO.File]::ReadAllBytes($record.destination); additive=$false; applied=$false }
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
try {
    foreach ($change in $changes) {
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
    throw $failure
}
Write-Host "Updated $($changes.Count) owned files; prior bytes saved under $backupRoot"
