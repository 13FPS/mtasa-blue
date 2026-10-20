#!/usr/bin/env pwsh
#requires -Version 7.0
# This file is part of Multi Theft Auto. See LICENSE in the repository root.
[CmdletBinding()]
param([ValidateSet('Diagnostic', 'Smoke', 'Online')][string]$Mode = 'Smoke',
    [switch]$NativeGaitReference, [switch]$MatchedGaitRate, [switch]$LockWalkRoot, [switch]$NativeInputReference,
    [switch]$MtaNativePadReference, [switch]$MtaNativeMovementTrial, [switch]$MtaCoreStartupReference, [switch]$MtaCoreInputReference,
    [switch]$MtaCoreMovementReference, [switch]$MtaCoreRenderReference, [switch]$MtaCoreRenderGpuReference, [switch]$MtaCoreRenderCustomPresentReference, [switch]$MtaCoreRenderFrameBoundaryReference, [switch]$MtaCoreRenderNativeDispatchReference,
    [ValidateSet('D3D11', 'D3D12')][string]$MtaCoreRenderRoute = 'D3D11', [switch]$MtaCoreGuiStartupReference, [switch]$MtaCoreGuiSurfaceReference, [switch]$MtaCoreGuiFrontendReference, [switch]$MtaCoreGuiFrontendInputReference, [switch]$MtaCoreGuiFrontendStartupReference, [switch]$MtaCorePersistentFrontend, [switch]$MtaCoreFrontendRuntime,
    [switch]$MtaCoreFrontendBindingPersistence, [switch]$MtaCoreFrontendNativeHost,
    [switch]$MtaCoreClockReference, [switch]$MtaCoreClockSetReference, [switch]$MtaCoreApplicationFrameReference, [switch]$MtaCoreStartupObservationReference,
    [string]$OutputDirectory)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'NativeFrontendPackage.ps1')
# Runtime owns one completed Core across views. Its selection must never inherit
# another native lifetime merely because those packages share frontend assets.
$runtimeConflicts = @($PSBoundParameters.Keys | Where-Object {
    $_ -match '^(Native|Matched|Lock|Mta)' -and $_ -notin @('MtaCoreFrontendRuntime','MtaCoreFrontendBindingPersistence','MtaCoreFrontendNativeHost') -and
    $PSBoundParameters[$_] -is [Management.Automation.SwitchParameter] -and $PSBoundParameters[$_].IsPresent
})
if ($MtaCoreFrontendRuntime -and ($Mode -ne 'Smoke' -or $runtimeConflicts.Count -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'MTA frontend runtime requires separate opt-in Smoke mode without other trials.'
}
if ($MtaCoreFrontendBindingPersistence -and -not $MtaCoreFrontendRuntime) {
    throw 'Saved MTA movement bindings require MtaCoreFrontendRuntime.'
}
if ($MtaCoreFrontendNativeHost -and -not $MtaCoreFrontendRuntime) { throw 'Native MTA startup requires MtaCoreFrontendRuntime.' }
# Native clock ownership excludes other native service lifetimes. Reject a
# mixed package before touching build outputs or downloading dependencies.
$clockConflicts = @($PSBoundParameters.Keys | Where-Object {
    $_ -match '^(Native|Matched|Lock|Mta)' -and $_ -notin @('MtaCoreClockReference', 'MtaCoreClockSetReference', 'MtaCoreApplicationFrameReference', 'MtaCoreStartupObservationReference') -and
        $PSBoundParameters[$_] -is [Management.Automation.SwitchParameter] -and $PSBoundParameters[$_].IsPresent
})
$clockSelections = @(@($MtaCoreClockReference, $MtaCoreClockSetReference, $MtaCoreApplicationFrameReference, $MtaCoreStartupObservationReference) | Where-Object IsPresent)
if ($clockSelections.Count -gt 1 -or
    (($MtaCoreClockReference -or $MtaCoreClockSetReference -or $MtaCoreApplicationFrameReference -or $MtaCoreStartupObservationReference) -and ($Mode -ne 'Smoke' -or $clockConflicts.Count -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute')))) {
    throw 'MTA Core clock/application observation requires separate opt-in Smoke mode without other trials.'
}
# Persistent input has independent host/native leases and must never inherit
# one of the bounded diagnostic modes or a script player-control lease.
$persistentConflicts = @(@('NativeGaitReference','MatchedGaitRate','LockWalkRoot','NativeInputReference','MtaNativePadReference','MtaNativeMovementTrial','MtaCoreStartupReference','MtaCoreInputReference','MtaCoreMovementReference','MtaCoreRenderReference','MtaCoreRenderGpuReference','MtaCoreRenderCustomPresentReference','MtaCoreRenderFrameBoundaryReference','MtaCoreRenderNativeDispatchReference','MtaCoreGuiStartupReference','MtaCoreGuiSurfaceReference','MtaCoreGuiFrontendReference','MtaCoreGuiFrontendInputReference','MtaCoreGuiFrontendStartupReference') | Where-Object { $PSBoundParameters.ContainsKey($_) -and $PSBoundParameters[$_] }
)
if ($MtaCorePersistentFrontend -and ($Mode -ne 'Smoke' -or $persistentConflicts.Count -gt 0 -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'Persistent MTA frontend requires separate opt-in Smoke mode without other trials.'
}
# Completed frontend ownership is independent of the rejecting gameplay constructor.
if ($MtaCoreGuiFrontendStartupReference -and ($Mode -ne 'Smoke' -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendReference -or $MtaCoreGuiStartupReference -or $MtaCoreGuiSurfaceReference -or $MtaCoreRenderReference -or $MtaCoreRenderGpuReference -or $MtaCoreRenderCustomPresentReference -or $MtaCoreRenderFrameBoundaryReference -or $MtaCoreRenderNativeDispatchReference -or $MtaCoreMovementReference -or $MtaCoreInputReference -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'MTA Core GUI frontend startup requires separate opt-in Smoke mode without other trials or an explicit renderer route.'
}
# Actual MTA frontend input is a separate, explicit control-owning trial.
if ($MtaCoreGuiFrontendInputReference -and ($Mode -ne 'Smoke' -or $MtaCoreGuiFrontendStartupReference -or $MtaCoreGuiFrontendReference -or $MtaCoreGuiStartupReference -or $MtaCoreGuiSurfaceReference -or $MtaCoreRenderReference -or $MtaCoreRenderGpuReference -or $MtaCoreRenderCustomPresentReference -or $MtaCoreRenderFrameBoundaryReference -or $MtaCoreRenderNativeDispatchReference -or $MtaCoreMovementReference -or $MtaCoreInputReference -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'MTA Core GUI frontend input requires separate opt-in Smoke mode without other trials or an explicit renderer route.'
}
if ($MtaCoreGuiFrontendReference -and ($Mode -ne 'Smoke' -or $MtaCoreGuiFrontendStartupReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiStartupReference -or $MtaCoreGuiSurfaceReference -or $MtaCoreRenderReference -or $MtaCoreRenderGpuReference -or $MtaCoreRenderCustomPresentReference -or $MtaCoreRenderFrameBoundaryReference -or $MtaCoreRenderNativeDispatchReference -or $MtaCoreMovementReference -or $MtaCoreInputReference -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'MTA Core GUI frontend requires separate opt-in Smoke mode without other trials or an explicit renderer route.'
}
# GUI startup owns its device and queue after observation ends. It is a separate
# trial, so selecting it cannot inherit an input, movement or read-only trial.
if ($MtaCoreGuiStartupReference -and ($Mode -ne 'Smoke' -or $MtaCoreGuiFrontendStartupReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendReference -or $MtaCoreGuiSurfaceReference -or $MtaCoreRenderReference -or $MtaCoreRenderGpuReference -or $MtaCoreRenderCustomPresentReference -or $MtaCoreRenderFrameBoundaryReference -or $MtaCoreRenderNativeDispatchReference -or $MtaCoreMovementReference -or $MtaCoreInputReference -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'MTA Core GUI startup requires separate opt-in Smoke mode without other trials or an explicit renderer route.'
}
# Surface display is an independent opt-in: sharing GUI assets cannot arm it.
if ($MtaCoreGuiSurfaceReference -and ($Mode -ne 'Smoke' -or $MtaCoreGuiFrontendStartupReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendReference -or $MtaCoreGuiStartupReference -or $MtaCoreRenderReference -or $MtaCoreRenderGpuReference -or $MtaCoreRenderCustomPresentReference -or $MtaCoreRenderFrameBoundaryReference -or $MtaCoreRenderNativeDispatchReference -or $MtaCoreMovementReference -or $MtaCoreInputReference -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'MTA Core GUI surface requires separate opt-in Smoke mode without other trials or an explicit renderer route.'
}
# Frame association is a separate read-only hook lease, never implicit in recognition.
# Caller dispatch is a separate explicit hook lease, even when the older probe refuses.
if ($MtaCoreRenderNativeDispatchReference -and (-not $MtaCoreRenderFrameBoundaryReference -or -not $MtaCoreRenderCustomPresentReference -or -not $MtaCoreRenderGpuReference -or -not $MtaCoreRenderReference -or $MtaCoreRenderRoute -ne 'D3D12')) {
    throw 'Native-dispatch observation requires explicit D3D12 Core GPU, custom-present and frame-boundary observation.'
}
if ($MtaCoreRenderFrameBoundaryReference -and (-not $MtaCoreRenderCustomPresentReference -or -not $MtaCoreRenderGpuReference -or -not $MtaCoreRenderReference -or $MtaCoreRenderRoute -ne 'D3D12')) {
    throw 'Frame-boundary observation requires explicit D3D12 Core GPU and custom-present observation.'
}
if ($MtaCoreRenderCustomPresentReference -and (-not $MtaCoreRenderGpuReference -or -not $MtaCoreRenderReference -or $MtaCoreRenderRoute -ne 'D3D12')) {
    throw 'Custom-present recognition requires explicit D3D12 Core GPU observation.'
}
if ($MtaCoreRenderGpuReference -and (-not $MtaCoreRenderReference -or $MtaCoreRenderRoute -ne 'D3D12')) {
    throw 'GPU ownership queries require explicit D3D12 Core renderer observation.'
}
if ($PSBoundParameters.ContainsKey('MtaCoreRenderRoute') -and -not $MtaCoreRenderReference) {
    throw 'An explicit renderer route requires MtaCoreRenderReference.'
}
if ($MtaCoreRenderReference -and ($Mode -ne 'Smoke' -or $MtaCoreMovementReference -or $MtaCoreInputReference -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA Core renderer observation requires separate opt-in Smoke mode.'
}
if ($MtaCoreMovementReference -and ($Mode -ne 'Smoke' -or $MtaCoreInputReference -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA Core configured movement reference requires separate opt-in Smoke mode.'
}
if ($MtaCoreInputReference -and ($Mode -ne 'Smoke' -or $MtaCoreStartupReference -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA Core configured input reference requires separate opt-in Smoke mode.'
}
if ($MtaCoreStartupReference -and ($Mode -ne 'Smoke' -or $MtaNativeMovementTrial -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA Core startup reference requires separate opt-in Smoke mode.'
}
if ($MtaNativePadReference -and $Mode -ne 'Smoke') { throw 'MTA native pad reference requires Smoke mode.' }
if ($MtaNativeMovementTrial -and ($Mode -ne 'Smoke' -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA movement writes require separate opt-in Smoke mode.'
}
if ($MtaNativePadReference -and ($NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA native pad reference conflicts with other input or gait trials.'
}
if ($NativeGaitReference -and $Mode -ne 'Smoke') { throw 'Native gait reference requires Smoke mode.' }
if ($MatchedGaitRate -and $Mode -ne 'Online') { throw 'Matched gait rate requires Online mode.' }
if ($LockWalkRoot -and $Mode -ne 'Online') { throw 'Walk-root locking requires Online mode.' }
if ($NativeInputReference -and $Mode -ne 'Smoke') { throw 'Native input reference requires Smoke mode.' }
if ($NativeInputReference -and $NativeGaitReference) { throw 'Native input reference conflicts with native gait reference.' }
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$workspaceBuild = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'Build')).TrimEnd('\', '/')
$buildRoot = Join-Path $repositoryRoot 'Build/definitive-package'
$cacheRoot = Join-Path $buildRoot 'downloads'
$packageRoot = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/') } else { Join-Path $buildRoot $Mode }
# New candidates need separate artifacts so a later package cannot overwrite
# the files bound by a previous ready receipt or successful in-game evidence.
if (-not $packageRoot.StartsWith($workspaceBuild + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Package output must stay within the resolved workspace Build subtree.'
}
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$apiCommit = '34bedee0be2f57047ed8f0705c81f137ede3d3ce'
$utf8 = [Text.UTF8Encoding]::new($false)

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

function Get-Amd64Exports([string]$Path) {
    # A new opt-in must reject stale DLLs before staging or downloading. Read
    # their PE export tables without executing Core, bridge or their DllMain.
    Assert-NoLinks $Path
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5a4d) { throw "Invalid AMD64 PE: $Path" }
    $pe = [uint64][BitConverter]::ToUInt32($bytes, 0x3c)
    if ($pe + 24 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes, [int]$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes, [int]$pe + 4) -ne 0x8664) { throw "Invalid AMD64 PE: $Path" }
    $sectionCount = [BitConverter]::ToUInt16($bytes, [int]$pe + 6)
    $optionalSize = [BitConverter]::ToUInt16($bytes, [int]$pe + 20)
    $optional = [int]$pe + 24
    if ($optionalSize -lt 120 -or $optional + $optionalSize -gt $bytes.Length -or [BitConverter]::ToUInt16($bytes, $optional) -ne 0x20b -or
        [BitConverter]::ToUInt32($bytes, $optional + 108) -lt 1 -or -not $sectionCount -or $sectionCount -gt 96) { throw "Invalid PE optional header: $Path" }
    $exportRva = [BitConverter]::ToUInt32($bytes, $optional + 112)
    $exportSize = [BitConverter]::ToUInt32($bytes, $optional + 116)
    $table = $optional + $optionalSize
    if ($table + 40 * $sectionCount -gt $bytes.Length -or -not $exportRva -or $exportSize -lt 40) { throw "Missing PE exports: $Path" }
    $sections = @()
    for ($index = 0; $index -lt $sectionCount; ++$index) {
        $at = $table + 40 * $index
        $sections += @{rva=[uint64][BitConverter]::ToUInt32($bytes, $at + 12);raw=[uint64][BitConverter]::ToUInt32($bytes, $at + 20);size=[uint64][BitConverter]::ToUInt32($bytes, $at + 16)}
    }
    $offset = {
        param([uint64]$Rva, [uint64]$Size)
        foreach ($section in $sections) {
            if ($Rva -ge $section.rva -and $Rva - $section.rva + $Size -le $section.size -and $section.raw + $Rva - $section.rva + $Size -le $bytes.Length) {
                return [int]($section.raw + $Rva - $section.rva)
            }
        }
        throw "Invalid PE export RVA: $Path"
    }
    $directory = & $offset $exportRva 40
    $functionCount = [BitConverter]::ToUInt32($bytes, $directory + 20)
    $nameCount = [BitConverter]::ToUInt32($bytes, $directory + 24)
    if (-not $functionCount -or -not $nameCount -or $functionCount -gt 65536 -or $nameCount -gt 65536) { throw "Invalid PE export counts: $Path" }
    $functions = & $offset ([BitConverter]::ToUInt32($bytes, $directory + 28)) (4 * $functionCount)
    $names = & $offset ([BitConverter]::ToUInt32($bytes, $directory + 32)) (4 * $nameCount)
    $ordinals = & $offset ([BitConverter]::ToUInt32($bytes, $directory + 36)) (2 * $nameCount)
    $exports = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    for ($index = 0; $index -lt $nameCount; ++$index) {
        $ordinal = [BitConverter]::ToUInt16($bytes, $ordinals + 2 * $index)
        if ($ordinal -ge $functionCount) { throw "Invalid PE export ordinal: $Path" }
        $functionRva = [BitConverter]::ToUInt32($bytes, $functions + 4 * $ordinal)
        $nameRva = [BitConverter]::ToUInt32($bytes, $names + 4 * $index)
        $nameOffset = & $offset $nameRva 1
        $length = 0
        while ($length -lt 512 -and $nameOffset + $length -lt $bytes.Length -and $bytes[$nameOffset + $length]) { ++$length }
        if (-not $length -or $length -eq 512 -or $nameOffset + $length -ge $bytes.Length) { throw "Invalid PE export name: $Path" }
        & $offset $nameRva ($length + 1) | Out-Null
        # A forwarded name cannot establish that this reviewed module contains
        # the completed lifecycle implementation required by the new mode.
        if ($functionRva -and ($functionRva -lt $exportRva -or [uint64]$functionRva -ge [uint64]$exportRva + $exportSize)) {
            [void]$exports.Add([Text.Encoding]::ASCII.GetString($bytes, $nameOffset, $length))
        }
    }
    Write-Output -NoEnumerate $exports
}

function Get-VerifiedDownload([string]$Name, [string]$Url, [string]$Sha256) {
    $destination = Join-Path $cacheRoot $Name
    Assert-NoLinks $destination
    if (-not (Test-Path -LiteralPath $destination)) {
        $partial = "$destination.$([Guid]::NewGuid().ToString('N')).download"
        try {
            Invoke-WebRequest -Uri $Url -OutFile $partial
            if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Sha256) {
                throw "Downloaded dependency hash mismatch: $Name"
            }
            [IO.File]::Move($partial, $destination)
        } finally {
            if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial }
        }
    }
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Sha256) {
        throw "Cached dependency hash mismatch: $destination"
    }
    return $destination
}

function Copy-ZipEntry([string]$ArchivePath, [string]$EntryName, [string]$Destination) {
    # Select individual entries instead of expanding arbitrary ZIP paths into
    # the package. This also excludes unrelated plugins from the game runtime.
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $entry = $archive.GetEntry($EntryName)
        if ($null -eq $entry) { throw "Missing ZIP entry: $EntryName" }
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Destination)) | Out-Null
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $Destination, $false)
    } finally {
        $archive.Dispose()
    }
}

Assert-NoLinks $buildRoot
Assert-NoLinks $packageRoot
$sourceNames = @('index.js', 'controller.mjs', 'native-pad-reference.mjs', 'native-movement-trial.mjs', 'config.mjs', 'mod.json')
foreach ($name in $sourceNames) {
    $source = Join-Path $PSScriptRoot "game-adapter/$name"
    Assert-NoLinks $source
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Adapter source is missing: $source" }
}
$bridgePath = Join-Path $repositoryRoot 'Build/definitive-online/de_client_bridge.dll'
$gameDePath = Join-Path $repositoryRoot 'Bin/mta/game_de.dll'
$corePath = Join-Path $repositoryRoot 'Bin/mta/x64/core.dll'
$pthreadPath = Join-Path $repositoryRoot 'Bin/mta/x64/pthread.dll'
$xmlPath = Join-Path $repositoryRoot 'Bin/server/x64/xmll.dll'
$bindConfigPath = Join-Path $PSScriptRoot 'coreconfig.xml'
if ($MtaCoreGuiFrontendStartupReference) {
    $bridgeExports = Get-Amd64Exports $bridgePath
    foreach ($name in @('DE_CoreGuiFrontendStartupApiVersion', 'DE_CoreGuiFrontendStartupBegin', 'DE_CoreGuiFrontendStartupCapture', 'DE_CoreGuiFrontendStartupRead',
        'DE_CoreGuiFrontendInputCapture', 'DE_CoreGuiFrontendInputRead', 'DE_CoreStartupPoll', 'DE_CoreStartupCancel', 'DE_CoreStartupRead',
        'DE_CoreGuiFrontendStartupApiVersionV2','DE_CoreGuiFrontendStartupBeginV2','DE_CoreGuiFrontendStartupCaptureClockV2',
        'DE_CoreGuiFrontendStartupReadClockV2','DE_CoreGuiFrontendStartupCaptureClockOwnerV2','DE_CoreGuiFrontendStartupReadClockOwnerV2')) {
        if (-not $bridgeExports.Contains($name)) { throw "Required completed frontend bridge export missing: $name" }
    }
    $coreExports = Get-Amd64Exports $corePath
    foreach ($name in @('MTA_DEFrontendStartupApiVersion', 'MTA_BeginDEFrontendStartup', 'MTA_PulseDEFrontendStartup', 'MTA_BindDEFrontendStartupInput',
        'MTA_ReadDEFrontendStartup', 'MTA_ShutdownDEFrontendStartup', 'MTA_ReleaseDEFrontendStartup', 'MTA_RunDEFrontendStartupView',
        'MTA_DEFrontendClockReferenceApiVersion','MTA_RunDEFrontendClockReference')) {
        if (-not $coreExports.Contains($name)) { throw "Required completed frontend Core export missing: $name" }
    }
}
if ($MtaCoreGuiFrontendStartupReference) {
    $clockExports = Get-Amd64Exports $gameDePath
    foreach ($name in @('BeginGameClockServiceV1','SubmitGameClockCommandV1','RenewGameClockServiceV1','ReadGameClockServiceV1','EndGameClockServiceV1')) {
        if (-not $clockExports.Contains($name)) { throw "Completed Core clock provider lacks export: $name" }
    }
}
if ($MtaCoreClockReference -or $MtaCoreClockSetReference -or $MtaCoreApplicationFrameReference -or $MtaCoreStartupObservationReference) {
    foreach ($provider in @(
        @{path=$bridgePath; names=@('DE_CoreClockApiVersion','DE_CoreClockBegin','DE_CoreClockCapture','DE_CoreClockRead','DE_CoreStartupPoll','DE_CoreStartupCancel','DE_CoreStartupRead')},
        @{path=$corePath; names=@('MTA_DEClockReferenceApiVersion','MTA_ConfigureDEClockReference')},
        @{path=$gameDePath; names=@('BeginGameClockServiceV1','SubmitGameClockCommandV1','RenewGameClockServiceV1','ReadGameClockServiceV1','EndGameClockServiceV1')}
    )) {
        $exports = Get-Amd64Exports $provider.path
        foreach ($name in $provider.names) {
            if (-not $exports.Contains($name)) { throw "Clock reference provider lacks export: $name" }
        }
    }
}
if ($MtaCoreStartupObservationReference) {
    foreach ($provider in @(
        @{path=$bridgePath; names=@('DE_CoreStartupObservationApiVersion','DE_CoreStartupObservationBegin','DE_CoreStartupObservationCapture','DE_CoreStartupObservationRead')},
        @{path=$corePath; names=@('MTA_DEStartupObservationReferenceApiVersion','MTA_ConfigureDEStartupObservationReference')},
        @{path=$gameDePath; names=@('BeginGameStartupObservationV1','ReadGameStartupObservationV1')}
    )) {
        $exports = Get-Amd64Exports $provider.path
        foreach ($name in $provider.names) { if (-not $exports.Contains($name)) { throw "Startup observation provider lacks export: $name" } }
    }
}
if ($MtaCoreApplicationFrameReference) {
    foreach ($provider in @(
        @{path=$bridgePath; names=@('DE_CoreApplicationFrameApiVersion','DE_CoreApplicationFrameBegin','DE_CoreApplicationFrameCapture','DE_CoreApplicationFrameRead')},
        @{path=$corePath; names=@('MTA_DEApplicationFrameReferenceApiVersion','MTA_ConfigureDEApplicationFrameReference')},
        @{path=$gameDePath; names=@('BeginGameApplicationFrameObservationV1','ReadGameApplicationFrameObservationV1')}
    )) {
        $exports = Get-Amd64Exports $provider.path
        foreach ($name in $provider.names) { if (-not $exports.Contains($name)) { throw "Application-frame provider lacks export: $name" } }
    }
}
if ($MtaCoreClockSetReference) {
    foreach ($provider in @(
        @{path=$bridgePath; names=@('DE_CoreClockSetApiVersion','DE_CoreClockSetBegin','DE_CoreClockSetCapture','DE_CoreClockSetRead')},
        @{path=$corePath; names=@('MTA_DEClockSetReferenceApiVersion','MTA_ConfigureDEClockSetReference')}
    )) {
        $exports = Get-Amd64Exports $provider.path
        foreach ($name in $provider.names) {
            if (-not $exports.Contains($name)) { throw "Clock Set reference provider lacks export: $name" }
        }
    }
}
if ($MtaCorePersistentFrontend) {
    foreach ($pair in @(
        @{path=$bridgePath; names=@('DE_CorePersistentFrontendApiVersion','DE_CorePersistentFrontendBegin','DE_CorePersistentFrontendHeartbeat','DE_CorePersistentFrontendCapture','DE_CorePersistentFrontendRead','DE_CorePersistentFrontendCaptureLifecycle','DE_CorePersistentFrontendReadLifecycle','DE_CoreStartupPoll','DE_CoreStartupCancel','DE_CoreStartupRead')},
        @{path=$corePath; names=@('MTA_DEPersistentFrontendApiVersion','MTA_RunDEPersistentFrontendV1','MTA_DEFrontendStartupApiVersion','MTA_BeginDEFrontendStartup','MTA_ReadDEFrontendStartup','MTA_ShutdownDEFrontendStartup','MTA_ReleaseDEFrontendStartup','SetMTADirectory','SetGTADirectory')},
        @{path=$gameDePath; names=@('BeginGameFrontendInputLeaseV1','RenewGameFrontendInputLeaseV1','ReadGameFrontendInputLeaseV1','EndGameFrontendInputLeaseV1')}
    )) {
        $exports = Get-Amd64Exports $pair.path
        foreach ($name in $pair.names) { if (-not $exports.Contains($name)) { throw "Required persistent frontend export missing: $name" } }
    }
}
if ($MtaCoreFrontendRuntime) {
    # The current runtime script hands native input between the menu and the
    # same retained Core bindings. Require that complete optional group before
    # staging, while historical menu-only exports remain compatibility APIs.
    foreach ($pair in @(
        @{path=$bridgePath; names=@('DE_FrontendRuntimeApiVersionV1','DE_FrontendRuntimeStartV1','DE_FrontendRuntimeCommandV1','DE_FrontendRuntimeHeartbeatV1','DE_FrontendRuntimePollV1','DE_FrontendRuntimeCaptureV1','DE_FrontendRuntimeReadV1',
            'DE_LocalInputRuntimeApiVersionV1','DE_LocalInputRuntimeStartV1','DE_LocalInputRuntimeCaptureV1','DE_LocalInputRuntimeReadV1')},
        @{path=$corePath; names=@('MTA_DEFrontendRuntimeApiVersion','MTA_BeginDEFrontendRuntime','MTA_ReadDEFrontendRuntime','MTA_PulseDEFrontendRuntime','MTA_RunDEFrontendRuntimeView','MTA_ShutdownDEFrontendRuntime','MTA_ReleaseDEFrontendRuntime','SetMTADirectory','SetGTADirectory',
            'MTA_DELocalInputRuntimeApiVersion','MTA_ConfigureDELocalInputRuntime','MTA_PulseDELocalInputRuntime','MTA_ReadDELocalInputRuntime','MTA_SuspendDELocalInputRuntime','MTA_DisableDELocalInputRuntime')},
        @{path=$gameDePath; names=@('BeginGameFrontendInputLeaseV1','RenewGameFrontendInputLeaseV1','ReadGameFrontendInputLeaseV1','EndGameFrontendInputLeaseV1',
            'BeginGameLocalInputLeaseV1','RenewGameLocalInputLeaseV1','ReadGameLocalInputLeaseV1','EndGameLocalInputLeaseV1')}
    )) {
        $exports = Get-Amd64Exports $pair.path
        foreach ($name in $pair.names) { if (-not $exports.Contains($name)) { throw "Required frontend runtime export missing: $name" } }
    }
}
$guiSources = @()
if ($MtaCoreFrontendNativeHost) {
    $nativeAsiPath = Join-Path $repositoryRoot 'Build/definitive-online/MtaDefinitive.asi'
    $exports = Get-Amd64Exports $nativeAsiPath
    if (-not $exports.Contains('InitializeASI')) { throw 'Native MTA entry lacks InitializeASI.' }
    $exports = Get-Amd64Exports $bridgePath
    foreach ($name in @('DE_NativeFrontendStartV1','DE_NativeFrontendStopV1','DE_NativeFrontendReadV1')) {
        if (-not $exports.Contains($name)) { throw "Native MTA startup export missing: $name" }
    }
}
if ($MtaCoreFrontendBindingPersistence) {
    # Refuse an older Core/Bridge before staging a config that promises durable
    # Apply. An unavailable save service must never silently become session-only.
    foreach ($pair in @(
        @{path=$bridgePath; names=@('DE_LocalInputRuntimeStartPersistentV1')},
        @{path=$corePath; names=@('MTA_EnableDEFrontendRuntimeBindingPersistenceV1')}
    )) {
        $exports = Get-Amd64Exports $pair.path
        foreach ($name in $pair.names) { if (-not $exports.Contains($name)) { throw "Required saved-binding export missing: $name" } }
    }
}
if ($MtaCoreGuiStartupReference -or $MtaCoreGuiSurfaceReference -or $MtaCoreGuiFrontendReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendStartupReference -or $MtaCorePersistentFrontend -or $MtaCoreFrontendRuntime) {
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Bin/mta/x64/cgui.dll'); path='cgui.dll' }
    foreach ($name in @('sans.ttf', 'saheader.ttf', 'sagothic.ttf', 'Imageset.xsd', 'GUIScheme.xsd', 'Font.xsd', 'Falagard.xsd')) {
        $guiSources += @{ source=(Join-Path $repositoryRoot ('Shared/data/MTA San Andreas/MTA/cgui/' + $name)); path=('mta/cgui/' + $name) }
    }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Build/gui-asset-cache/unifont-16.0.04.ttf'); path='mta/cgui/unifont.ttf' }
    foreach ($name in @('CGUI.xml', 'CGUI.is.xml', 'CGUI.lnf.xml', 'CGUI.png')) {
        $guiSources += @{ source=(Join-Path $repositoryRoot ('Shared/data/MTA San Andreas/skins/Default/' + $name)); path=('skins/Default/' + $name) }
    }
    foreach ($entry in $guiSources) {
        Assert-NoLinks $entry.source
        if (-not (Test-Path -LiteralPath $entry.source -PathType Leaf)) { throw "GUI startup asset is missing: $($entry.source)" }
    }
    $unifont = @($guiSources | Where-Object path -eq 'mta/cgui/unifont.ttf')[0]
    if ((Get-FileHash -LiteralPath $unifont.source -Algorithm SHA256).Hash.ToLowerInvariant() -ne '6b18fdcf0f61f3df2e61db9737af6de9f37048ec93083621d41504ebf8cb18f4') { throw 'Pinned GUI Unifont hash mismatch.' }
}
if ($MtaCoreGuiFrontendReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendStartupReference -or $MtaCorePersistentFrontend -or $MtaCoreFrontendRuntime) {
    # Use the original message-box assets with MTA's existing dialog code; a
    # complete separate group preserves the historical 22-file frontend group.
    foreach ($relative in (Get-MtaFrontendMessageBoxPaths)) {
        $name = [IO.Path]::GetFileName($relative)
        $guiSources += @{ source=(Join-Path $repositoryRoot ('Shared/data/MTA San Andreas/MTA/cgui/images/' + $name)); path=$relative.Substring('CLEO/de-online/'.Length) }
    }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/background.png'); path='mta/cgui/images/background.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/background_logo.png'); path='mta/cgui/images/background_logo.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/mta_filler.png'); path='mta/cgui/images/mta_filler.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/version.png'); path='mta/cgui/images/version.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/the_language_icon.png'); path='mta/cgui/images/the_language_icon.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/locked.png'); path='mta/cgui/images/serverbrowser/locked.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/search-servers.png'); path='mta/cgui/images/serverbrowser/search-servers.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/search-players.png'); path='mta/cgui/images/serverbrowser/search-players.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/refresh.png'); path='mta/cgui/images/serverbrowser/refresh.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/favorite.png'); path='mta/cgui/images/serverbrowser/favorite.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/connect.png'); path='mta/cgui/images/serverbrowser/connect.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/info.png'); path='mta/cgui/images/serverbrowser/info.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/search.png'); path='mta/cgui/images/serverbrowser/search.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/cgui/images/serverbrowser/trashcan.png'); path='mta/cgui/images/serverbrowser/trashcan.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/menu_disconnect.png'); path='mta/locale/en_US/menu_disconnect.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/menu_quick_connect.png'); path='mta/locale/en_US/menu_quick_connect.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/menu_browse_servers.png'); path='mta/locale/en_US/menu_browse_servers.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/menu_settings.png'); path='mta/locale/en_US/menu_settings.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/menu_about.png'); path='mta/locale/en_US/menu_about.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/menu_quit.png'); path='mta/locale/en_US/menu_quit.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/latest_news.png'); path='mta/locale/en_US/latest_news.png' }
    $guiSources += @{ source=(Join-Path $repositoryRoot 'Shared/data/MTA San Andreas/MTA/locale/en_US/client.pot'); path='mta/locale/en_US/client.pot' }
    foreach ($entry in $guiSources) {
        Assert-NoLinks $entry.source
        if (-not (Test-Path -LiteralPath $entry.source -PathType Leaf)) { throw "MTA frontend asset is missing: $($entry.source)" }
    }
}
Assert-NoLinks $bridgePath
Assert-NoLinks $gameDePath
Assert-NoLinks $corePath
Assert-NoLinks $pthreadPath
Assert-NoLinks $xmlPath
Assert-NoLinks $bindConfigPath
if (-not (Test-Path -LiteralPath $xmlPath -PathType Leaf) -or -not (Test-Path -LiteralPath $bindConfigPath -PathType Leaf)) {
    throw 'Build the normal XML Release/x64 module and retain the explicit configured-input XML before packaging.'
}
if (-not (Test-Path -LiteralPath $bridgePath -PathType Leaf)) {
    throw 'Build the current native bridge with Client/definitive/build.ps1 before packaging.'
}
if (-not (Test-Path -LiteralPath $gameDePath -PathType Leaf)) { throw 'Build the normal Game DE Release/x64 module before packaging.' }
if (-not (Test-Path -LiteralPath $corePath -PathType Leaf) -or -not (Test-Path -LiteralPath $pthreadPath -PathType Leaf)) {
    throw 'Build the normal Core Release/x64 module and its pthread dependency before packaging.'
}
[IO.Directory]::CreateDirectory($cacheRoot) | Out-Null

# Immutable release URLs and independent hashes make a reviewed package
# reproducible without executing either third-party installer or downloaded DLL.
$dependencies = @(
    @{ name = 'cleo_redux_1.5.1.x64.zip'; url = 'https://github.com/cleolibrary/CLEO-Redux/releases/download/1.5.1/cleo_redux_1.5.1.x64.zip'; sha256 = '4205147c6b62a0393e4ed77f0e59cbbf52715601c5a1ec09c755b0a1320185dd' },
    @{ name = 'Ultimate-ASI-Loader-NoPDB_x64.zip'; url = 'https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader-NoPDB_x64.zip'; sha256 = 'e5860e7d9a1805267535b65749575b5e406cc6ea3325c7392189c578815045d1' },
    @{ name = 'CLEO_REDUX_LICENSE.txt'; url = 'https://raw.githubusercontent.com/cleolibrary/CLEO-Redux/1.5.1/LICENSE.txt'; sha256 = 'baec1109f27ba9165e84f9fcd8000029a54fd108895c53cc4fff4ef76799c7c1' },
    @{ name = 'ULTIMATE_ASI_LOADER_LICENSE.txt'; url = 'https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/v9.7.4/license'; sha256 = 'ceca73c504e39084b0b80d2a437aea9988313d1915418d3c629c3e4a8313f5a1' },
    @{ name = 'sa_unreal.json'; url = "https://raw.githubusercontent.com/sannybuilder/library/$apiCommit/sa_unreal/sa_unreal.json"; sha256 = 'cab1b9d6bd25a4fff41fef63aef0fd771a923c0d39cb018f030f44eceb7713f7' },
    @{ name = 'unknown_x64.json'; url = "https://raw.githubusercontent.com/sannybuilder/library/$apiCommit/unknown_x64/unknown_x64.json"; sha256 = '5206809f78515bd0dca716f945f867056f4d5b0da2f9d079caaadf55c0855676' },
    @{ name = 'SANNY_BUILDER_LIBRARY_README.md'; url = "https://raw.githubusercontent.com/sannybuilder/library/$apiCommit/README.md"; sha256 = '359d7c71d42f14bc4277616d6058a6715d13ef3bbdd7567216448d45f4fd721d' }
)
$downloads = @{}
foreach ($dependency in $dependencies) {
    $downloads[$dependency.name] = Get-VerifiedDownload $dependency.name $dependency.url $dependency.sha256
}
$saDefinitions = Get-Content -LiteralPath $downloads['sa_unreal.json'] -Raw | ConvertFrom-Json
$sdkDefinitions = Get-Content -LiteralPath $downloads['unknown_x64.json'] -Raw | ConvertFrom-Json
if ($saDefinitions.meta.version -ne '0.500' -or $sdkDefinitions.meta.version -ne '0.239') {
    throw 'Pinned API definitions have unexpected version metadata.'
}

$stagingRoot = Join-Path $buildRoot ('.staging-' + [Guid]::NewGuid().ToString('N'))
$stagingWin64 = Join-Path $stagingRoot 'Win64'
[IO.Directory]::CreateDirectory($stagingWin64) | Out-Null
Copy-ZipEntry $downloads['Ultimate-ASI-Loader-NoPDB_x64.zip'] 'dinput8.dll' (Join-Path $stagingWin64 'version.dll')
Copy-ZipEntry $downloads['cleo_redux_1.5.1.x64.zip'] 'cleo_redux64.asi' (Join-Path $stagingWin64 'cleo_redux64.asi')
Copy-ZipEntry $downloads['cleo_redux_1.5.1.x64.zip'] 'CLEO/CLEO_PLUGINS/dylib64.cleo' (Join-Path $stagingWin64 'CLEO/CLEO_PLUGINS/dylib64.cleo')
Copy-ZipEntry $downloads['cleo_redux_1.5.1.x64.zip'] 'CLEO/CLEO_PLUGINS/Input64.cleo' (Join-Path $stagingWin64 'CLEO/CLEO_PLUGINS/Input64.cleo')
$adapterDirectory = Join-Path $stagingWin64 'CLEO/de-online'
$configDirectory = Join-Path $stagingWin64 'CLEO/.config'
$licenseDirectory = Join-Path $stagingWin64 'DE_ONLINE_LICENSES'
foreach ($directory in @($adapterDirectory, $configDirectory, $licenseDirectory)) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
}
foreach ($name in $sourceNames) {
    [IO.File]::Copy((Join-Path $PSScriptRoot "game-adapter/$name"), (Join-Path $adapterDirectory $name), $false)
}
[IO.File]::Copy($bridgePath, (Join-Path $adapterDirectory 'de_client_bridge.dll'), $false)
# The actual Core consumer loads its real backend adjacent to the bridge only
# after explicit trial activation; ordinary modes do not initialize this module.
[IO.File]::Copy($gameDePath, (Join-Path $adapterDirectory 'game_de.dll'), $false)
# Keep the normal Core and its compiled dependency adjacent to Game DE. Merely
# owning these files must not activate them; the separate startup probe gates
# loading in the adapter and ordinary package modes leave that flag disabled.
[IO.File]::Copy($corePath, (Join-Path $adapterDirectory 'core.dll'), $false)
[IO.File]::Copy($pthreadPath, (Join-Path $adapterDirectory 'pthread.dll'), $false)
# Core parses these explicit records through the existing MTA XML/keybind code.
# Keep both files owned so a foreign configuration is never replaced on upgrade.
[IO.File]::Copy($xmlPath, (Join-Path $adapterDirectory 'xmll.dll'), $false)
$bindConfigDirectory = Join-Path $adapterDirectory 'mta/config'
[IO.Directory]::CreateDirectory($bindConfigDirectory) | Out-Null
[IO.File]::Copy($bindConfigPath, (Join-Path $bindConfigDirectory 'coreconfig.xml'), $false)
# Admit the complete asset group only in the explicit GUI package. Ordinary
# packages remain the same 23 owned paths and never acquire partial GUI assets.
foreach ($entry in $guiSources) {
    $destination = Join-Path $adapterDirectory $entry.path
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
    [IO.File]::Copy($entry.source, $destination, $false)
}
$configPath = Join-Path $adapterDirectory 'config.mjs'
$configText = [IO.File]::ReadAllText($configPath)
$modeMarker = "export const MODE = 'smoke';"
if ([regex]::Matches($configText, [regex]::Escape($modeMarker)).Count -ne 1) {
    throw 'config.mjs must contain exactly one default smoke-mode assignment.'
}
# Mode changes only the staged copy. Diagnostic retains the runtime files but
# exits before bridge/game access; the default Smoke mode validates player reads.
$configText = $configText.Replace($modeMarker, "export const MODE = '$($Mode.ToLowerInvariant())';")
$referenceMarker = 'export const GAIT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($referenceMarker)).Count -ne 1) { throw 'Expected disabled reference in source configuration.' }
$configText = $configText.Replace($referenceMarker,
    "export const GAIT_REFERENCE = $($NativeGaitReference.IsPresent.ToString().ToLowerInvariant());")
$rateMarker = 'export const GAIT_RATE = false;'
if ([regex]::Matches($configText, [regex]::Escape($rateMarker)).Count -ne 1) { throw 'Expected disabled rate trial in source configuration.' }
$configText = $configText.Replace($rateMarker,
    "export const GAIT_RATE = $($MatchedGaitRate.IsPresent.ToString().ToLowerInvariant());")
$rootMarker = 'export const GAIT_ROOT_LOCK = false;'
if ([regex]::Matches($configText, [regex]::Escape($rootMarker)).Count -ne 1) { throw 'Expected disabled root experiment in source configuration.' }
$configText = $configText.Replace($rootMarker,
    "export const GAIT_ROOT_LOCK = $($LockWalkRoot.IsPresent.ToString().ToLowerInvariant());")
$inputMarker = 'export const INPUT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($inputMarker)).Count -ne 1) { throw 'Expected disabled input reference in source configuration.' }
$configText = $configText.Replace($inputMarker,
    "export const INPUT_REFERENCE = $($NativeInputReference.IsPresent.ToString().ToLowerInvariant());")
$padMarker = 'export const MTA_NATIVE_PAD_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($padMarker)).Count -ne 1) { throw 'Expected disabled MTA native pad reference in source configuration.' }
# Keep the native reader opt-in independent from the previous CLEO pad probe;
# an ordinary package must never inherit either experiment from a prior trial.
$configText = $configText.Replace($padMarker,
    "export const MTA_NATIVE_PAD_REFERENCE = $($MtaNativePadReference.IsPresent.ToString().ToLowerInvariant());")
$movementMarker = 'export const MTA_NATIVE_MOVEMENT_TRIAL = false;'
if ([regex]::Matches($configText, [regex]::Escape($movementMarker)).Count -ne 1) { throw 'Expected disabled MTA native movement trial in source configuration.' }
$configText = $configText.Replace($movementMarker,
    "export const MTA_NATIVE_MOVEMENT_TRIAL = $($MtaNativeMovementTrial.IsPresent.ToString().ToLowerInvariant());")
$coreStartupMarker = 'export const MTA_CORE_STARTUP_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreStartupMarker)).Count -ne 1) { throw 'Expected disabled MTA Core startup reference in source configuration.' }
$configText = $configText.Replace($coreStartupMarker,
    "export const MTA_CORE_STARTUP_REFERENCE = $($MtaCoreStartupReference.IsPresent.ToString().ToLowerInvariant());")
$coreInputMarker = 'export const MTA_CORE_INPUT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreInputMarker)).Count -ne 1) { throw 'Expected disabled MTA Core configured input reference in source configuration.' }
$configText = $configText.Replace($coreInputMarker,
    "export const MTA_CORE_INPUT_REFERENCE = $($MtaCoreInputReference.IsPresent.ToString().ToLowerInvariant());")
$coreMovementMarker = 'export const MTA_CORE_MOVEMENT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreMovementMarker)).Count -ne 1) { throw 'Expected disabled MTA Core configured movement reference in source configuration.' }
$configText = $configText.Replace($coreMovementMarker,
    "export const MTA_CORE_MOVEMENT_REFERENCE = $($MtaCoreMovementReference.IsPresent.ToString().ToLowerInvariant());")
$coreRenderMarker = 'export const MTA_CORE_RENDER_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreRenderMarker)).Count -ne 1) { throw 'Expected disabled MTA Core renderer observation in source configuration.' }
$configText = $configText.Replace($coreRenderMarker,
    "export const MTA_CORE_RENDER_REFERENCE = $($MtaCoreRenderReference.IsPresent.ToString().ToLowerInvariant());")
$coreRenderRouteMarker = "export const MTA_CORE_RENDER_ROUTE = 'd3d11';"
if ([regex]::Matches($configText, [regex]::Escape($coreRenderRouteMarker)).Count -ne 1) { throw 'Expected explicit renderer route in source configuration.' }
$configText = $configText.Replace($coreRenderRouteMarker,
    "export const MTA_CORE_RENDER_ROUTE = '$($MtaCoreRenderRoute.ToLowerInvariant())';")
$coreRenderGpuMarker = 'export const MTA_CORE_RENDER_GPU_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreRenderGpuMarker)).Count -ne 1) { throw 'Expected disabled GPU ownership queries in source configuration.' }
$configText = $configText.Replace($coreRenderGpuMarker,
    "export const MTA_CORE_RENDER_GPU_REFERENCE = $($MtaCoreRenderGpuReference.IsPresent.ToString().ToLowerInvariant());")
$coreRenderCustomPresentMarker = 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreRenderCustomPresentMarker)).Count -ne 1) { throw 'Expected disabled custom-present recognition in source configuration.' }
$configText = $configText.Replace($coreRenderCustomPresentMarker,
    "export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = $($MtaCoreRenderCustomPresentReference.IsPresent.ToString().ToLowerInvariant());")
$coreRenderFrameBoundaryMarker = 'export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreRenderFrameBoundaryMarker)).Count -ne 1) { throw 'Expected disabled frame-boundary observation in source configuration.' }
$configText = $configText.Replace($coreRenderFrameBoundaryMarker,
    "export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = $($MtaCoreRenderFrameBoundaryReference.IsPresent.ToString().ToLowerInvariant());")
$coreRenderNativeDispatchMarker = 'export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($coreRenderNativeDispatchMarker)).Count -ne 1) { throw 'Expected disabled native-dispatch observation in source configuration.' }
$configText = $configText.Replace($coreRenderNativeDispatchMarker,
    "export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = $($MtaCoreRenderNativeDispatchReference.IsPresent.ToString().ToLowerInvariant());")
$guiMarker = 'export const MTA_CORE_GUI_STARTUP_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($guiMarker)).Count -ne 1) { throw 'Expected disabled MTA Core GUI startup in source configuration.' }
$configText = $configText.Replace($guiMarker,
    "export const MTA_CORE_GUI_STARTUP_REFERENCE = $($MtaCoreGuiStartupReference.IsPresent.ToString().ToLowerInvariant());")
$surfaceMarker = 'export const MTA_CORE_GUI_SURFACE_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($surfaceMarker)).Count -ne 1) { throw 'Expected disabled MTA Core GUI surface in source configuration.' }
$configText = $configText.Replace($surfaceMarker,
    "export const MTA_CORE_GUI_SURFACE_REFERENCE = $($MtaCoreGuiSurfaceReference.IsPresent.ToString().ToLowerInvariant());")
$frontendMarker = 'export const MTA_CORE_GUI_FRONTEND_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($frontendMarker)).Count -ne 1) { throw 'Expected disabled MTA Core GUI frontend in source configuration.' }
$configText = $configText.Replace($frontendMarker,
    "export const MTA_CORE_GUI_FRONTEND_REFERENCE = $($MtaCoreGuiFrontendReference.IsPresent.ToString().ToLowerInvariant());")
# Native neutral scopes have their own opt-in; sharing frontend assets never
# inherits the older script-owned SET_PLAYER_CONTROL experiment.
$frontendInputMarker = 'export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($frontendInputMarker)).Count -ne 1) { throw 'Expected disabled MTA Core GUI frontend input in source configuration.' }
$configText = $configText.Replace($frontendInputMarker,
    "export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE = $($MtaCoreGuiFrontendInputReference.IsPresent.ToString().ToLowerInvariant());")
# A completed Core owner has a distinct opt-in and cannot inherit a bounded
# InitializeCore experiment merely because both modes ship the same GUI assets.
$clockMarker = 'export const MTA_CORE_CLOCK_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($clockMarker)).Count -ne 1) { throw 'Expected disabled MTA Core clock reference in source configuration.' }
$configText = $configText.Replace($clockMarker,
    "export const MTA_CORE_CLOCK_REFERENCE = $($MtaCoreClockReference.IsPresent.ToString().ToLowerInvariant());")
$clockSetMarker = 'export const MTA_CORE_CLOCK_SET_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($clockSetMarker)).Count -ne 1) { throw 'Expected disabled MTA Core clock Set reference in source configuration.' }
$configText = $configText.Replace($clockSetMarker,
    "export const MTA_CORE_CLOCK_SET_REFERENCE = $($MtaCoreClockSetReference.IsPresent.ToString().ToLowerInvariant());")
$applicationMarker = 'export const MTA_CORE_APPLICATION_FRAME_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($applicationMarker)).Count -ne 1) { throw 'Expected disabled application-frame observation in source configuration.' }
$configText = $configText.Replace($applicationMarker,
    "export const MTA_CORE_APPLICATION_FRAME_REFERENCE = $($MtaCoreApplicationFrameReference.IsPresent.ToString().ToLowerInvariant());")
$startupObservationMarker = 'export const MTA_CORE_STARTUP_OBSERVATION_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($startupObservationMarker)).Count -ne 1) { throw 'Expected disabled startup observation in source configuration.' }
$configText = $configText.Replace($startupObservationMarker,
    "export const MTA_CORE_STARTUP_OBSERVATION_REFERENCE = $($MtaCoreStartupObservationReference.IsPresent.ToString().ToLowerInvariant());")
$runtimeMarker = 'export const MTA_CORE_FRONTEND_RUNTIME = false;'
if ([regex]::Matches($configText, [regex]::Escape($runtimeMarker)).Count -ne 1) { throw 'Expected disabled MTA frontend runtime in source configuration.' }
$configText = $configText.Replace($runtimeMarker,
    "export const MTA_CORE_FRONTEND_RUNTIME = $($MtaCoreFrontendRuntime.IsPresent.ToString().ToLowerInvariant());")
$nativeHostMarker = 'export const MTA_CORE_FRONTEND_NATIVE_HOST = false;'
if ([regex]::Matches($configText, [regex]::Escape($nativeHostMarker)).Count -ne 1) { throw 'Expected disabled native frontend host in source configuration.' }
$configText = $configText.Replace($nativeHostMarker,
    "export const MTA_CORE_FRONTEND_NATIVE_HOST = $($MtaCoreFrontendNativeHost.IsPresent.ToString().ToLowerInvariant());")
$bindingPersistenceMarker = 'export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($bindingPersistenceMarker)).Count -ne 1) { throw 'Expected disabled saved movement bindings in source configuration.' }
$configText = $configText.Replace($bindingPersistenceMarker,
    "export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = $($MtaCoreFrontendBindingPersistence.IsPresent.ToString().ToLowerInvariant());")
$persistentMarker = 'export const MTA_CORE_PERSISTENT_FRONTEND = false;'
if ([regex]::Matches($configText, [regex]::Escape($persistentMarker)).Count -ne 1) { throw 'Expected disabled persistent frontend in source configuration.' }
$configText = $configText.Replace($persistentMarker,
    "export const MTA_CORE_PERSISTENT_FRONTEND = $($MtaCorePersistentFrontend.IsPresent.ToString().ToLowerInvariant());")
$frontendStartupMarker = 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = false;'
if ([regex]::Matches($configText, [regex]::Escape($frontendStartupMarker)).Count -ne 1) { throw 'Expected disabled MTA Core GUI frontend startup in source configuration.' }
$configText = $configText.Replace($frontendStartupMarker,
    "export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = $($MtaCoreGuiFrontendStartupReference.IsPresent.ToString().ToLowerInvariant());")
[IO.File]::WriteAllText($configPath, $configText, $utf8)
if ($MtaCoreFrontendNativeHost) {
    [IO.File]::Copy($nativeAsiPath, (Join-Path $stagingRoot 'Win64/MtaDefinitive.asi'), $false)
    [IO.File]::WriteAllText((Join-Path $stagingRoot 'Win64/MtaDefinitive.ini'), (New-MtaNativeFrontendIni $true $MtaCoreFrontendBindingPersistence.IsPresent), $utf8)
}
foreach ($name in @('sa_unreal.json', 'unknown_x64.json')) {
    [IO.File]::Copy($downloads[$name], (Join-Path $configDirectory $name), $false)
}
$ini = @'
[General]
AllowCs=0
AllowJs=1
AllowFxt=0
LogOpcodes=0
CheckUpdates=0
PermissionLevel=Strict
[Host]
EnableSelfHost=0
[Permissions]
mem=1
dll=1
fs=0
net=0
'@
[IO.File]::WriteAllText((Join-Path $configDirectory 'cleo.ini'), $ini.Replace("`r`n", "`n") + "`n", $utf8)
foreach ($name in @('CLEO_REDUX_LICENSE.txt', 'ULTIMATE_ASI_LOADER_LICENSE.txt', 'SANNY_BUILDER_LIBRARY_README.md')) {
    [IO.File]::Copy($downloads[$name], (Join-Path $licenseDirectory $name), $false)
}
[IO.File]::Copy((Join-Path $repositoryRoot 'LICENSE'), (Join-Path $licenseDirectory 'MTA_GPLv3.txt'), $false)

$files = @(Get-ChildItem -LiteralPath $stagingWin64 -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = [IO.Path]::GetRelativePath($stagingWin64, $_.FullName).Replace('\', '/')
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
})
if ($files.Count -ne $(if ($MtaCoreFrontendNativeHost) { 64 } elseif ($MtaCoreGuiFrontendReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendStartupReference -or $MtaCorePersistentFrontend -or $MtaCoreFrontendRuntime) { 62 } elseif ($MtaCoreGuiStartupReference -or $MtaCoreGuiSurfaceReference) { 36 } else { 23 })) { throw 'Unexpected package ownership count.' }
$manifest = [ordered]@{
    packageId = 'de-online-prototype'
    formatVersion = 1
    mode = $Mode
    gameSha256 = $knownGameHash
    gameFixedVersion = '1.0.113.21181'
    apiCommit = $apiCommit
    dependencies = $dependencies
    files = $files
}
[IO.File]::WriteAllText((Join-Path $stagingRoot 'manifest.json'), ($manifest | ConvertTo-Json -Depth 6) + "`n", $utf8)

if (Test-Path -LiteralPath $packageRoot) {
    # Replace only a previous, intact package we generated. Extra or edited
    # files require review instead of recursive deletion of somebody's work.
    Assert-NoLinks $packageRoot
    $oldManifestPath = Join-Path $packageRoot 'manifest.json'
    Assert-NoLinks $oldManifestPath
    $old = Get-Content -LiteralPath $oldManifestPath -Raw | ConvertFrom-Json
    if ($old.packageId -ne 'de-online-prototype' -or $old.formatVersion -ne 1) { throw 'Existing output is not our package.' }
    $oldPaths = @($old.files | ForEach-Object { 'Win64/' + $_.path }) + @('manifest.json')
    $actualPaths = @(Get-ChildItem -LiteralPath $packageRoot -File -Recurse | ForEach-Object {
        [IO.Path]::GetRelativePath($packageRoot, $_.FullName).Replace('\', '/')
    })
    if (@(Compare-Object $oldPaths $actualPaths).Count -ne 0) { throw 'Existing package contains unrecorded or missing files.' }
    foreach ($directory in @(Get-ChildItem -LiteralPath $packageRoot -Directory -Recurse -Force)) {
        Assert-NoLinks $directory.FullName
    }
    foreach ($entry in $old.files) {
        $existing = [IO.Path]::GetFullPath((Join-Path $packageRoot ('Win64/' + $entry.path)))
        if (-not $existing.StartsWith($packageRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Existing package manifest escapes its output directory.'
        }
        Assert-NoLinks $existing
        if ((Get-FileHash -LiteralPath $existing -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
            throw "Existing package was modified: $existing"
        }
    }
    foreach ($relative in $oldPaths) { Remove-Item -LiteralPath (Join-Path $packageRoot $relative) }
    foreach ($directory in @(Get-ChildItem -LiteralPath $packageRoot -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending)) {
        Remove-Item -LiteralPath $directory.FullName
    }
    Remove-Item -LiteralPath $packageRoot
}
# Both resolved paths are descendants of the fixed ignored build directory;
# move the completed artifact only after all dependency/content checks pass.
Assert-NoLinks $stagingRoot
if (-not $stagingRoot.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    -not $packageRoot.StartsWith($workspaceBuild + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Package move would escape the intended build directory.'
}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($packageRoot)) | Out-Null
Move-Item -LiteralPath $stagingRoot -Destination $packageRoot
Write-Host "Prepared $Mode package: $packageRoot"
Write-Host "Verified $($files.Count) files. No game files were changed and no downloaded binary was run."
