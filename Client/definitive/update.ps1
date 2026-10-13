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
if ($packagePaths.Count -ne $installedPaths.Count -or @($packagePaths | Select-Object -Unique).Count -ne $installedPaths.Count -or
    @(Compare-Object $installedPaths $packagePaths).Count -ne 0) { throw 'Package owned paths differ.' }
$changes = @()
foreach ($entry in $package.files) {
    $sourcePath = Join-Path $packageRoot ('Win64/' + $entry.path)
    $sourceBytes = [IO.File]::ReadAllBytes($sourcePath)
    $actualHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($sourceBytes)).ToLowerInvariant()
    if ($actualHash -ne $entry.sha256) { throw 'Package hash mismatch.' }
    $record = $installed.files | Where-Object { $_.path -eq $entry.path }
    if ($record.sha256 -ne $entry.sha256) {
        $changes += [pscustomobject]@{ path=$record.destination; bytes=$sourceBytes; original=[IO.File]::ReadAllBytes($record.destination) }
        $record.sha256 = $entry.sha256
    }
}
if (-not $changes.Count) { Write-Host 'Installed package is current.'; return }
if (-not $PSCmdlet.ShouldProcess($gameRoot, "Update $($changes.Count) verified owned files")) { return }

function Write-AtomicFile([string]$Path, [byte[]]$Bytes) {
    $temporary = $Path + '.de-online-' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
        try { $stream.Write($Bytes, 0, $Bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
        [IO.File]::Move($temporary, $Path, $true)
    } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
}
# Keep an on-disk recovery copy before changing files, independent of chat memory.
$backupRoot = Join-Path $repositoryRoot ('Build/definitive-update-backups/' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
[IO.File]::WriteAllBytes((Join-Path $backupRoot 'manifest.json'), $originalManifest)
for ($index = 0; $index -lt $changes.Count; $index++) {
    [IO.File]::WriteAllBytes((Join-Path $backupRoot "$index.original"), $changes[$index].original)
}
try {
    foreach ($change in $changes) { Write-AtomicFile $change.path $change.bytes }
    $newManifest = [Text.UTF8Encoding]::new($false).GetBytes(($installed | ConvertTo-Json -Depth 6) + "`n")
    Write-AtomicFile $manifestPath $newManifest
} catch {
    $failure = $_
    foreach ($change in $changes) { Write-AtomicFile $change.path $change.original }
    Write-AtomicFile $manifestPath $originalManifest
    throw $failure
}
Write-Host "Updated $($changes.Count) owned files; prior bytes saved under $backupRoot"
