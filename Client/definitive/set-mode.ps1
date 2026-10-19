#!/usr/bin/env pwsh
#requires -Version 7.0
# Change only owned configuration, keeping the installer manifest valid for removal.
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'None')]
param(
    [Parameter(Mandatory)][ValidateSet('Diagnostic', 'Smoke', 'Online')][string]$Mode,
    [switch]$NativeGaitReference,
    [switch]$MatchedGaitRate,
    [switch]$LockWalkRoot,
    [switch]$NativeInputReference,
    [switch]$MtaNativePadReference,
    [switch]$MtaNativeMovementTrial,
    [switch]$MtaCoreStartupReference,
    [switch]$MtaCoreInputReference,
    [switch]$MtaCoreMovementReference,
    [switch]$MtaCoreRenderReference,
    [switch]$MtaCoreRenderGpuReference,
    [switch]$MtaCoreRenderCustomPresentReference, [switch]$MtaCoreRenderFrameBoundaryReference, [switch]$MtaCoreRenderNativeDispatchReference,
    [switch]$MtaCoreGuiStartupReference, [switch]$MtaCoreGuiSurfaceReference, [switch]$MtaCoreGuiFrontendReference, [switch]$MtaCoreGuiFrontendInputReference, [switch]$MtaCoreGuiFrontendStartupReference, [switch]$MtaCorePersistentFrontend, [switch]$MtaCoreFrontendRuntime,
    [ValidateSet('D3D11', 'D3D12')][string]$MtaCoreRenderRoute = 'D3D11',
    [switch]$MtaCoreClockReference, [switch]$MtaCoreClockSetReference, [switch]$MtaCoreApplicationFrameReference, [switch]$MtaCoreStartupObservationReference,
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Runtime owns one completed Core across views. Its selection must never inherit
# another native lifetime merely because those packages share frontend assets.
$runtimeConflicts = @($PSBoundParameters.Keys | Where-Object {
    $_ -match '^(Native|Matched|Lock|Mta)' -and $_ -ne 'MtaCoreFrontendRuntime' -and
    $PSBoundParameters[$_] -is [Management.Automation.SwitchParameter] -and $PSBoundParameters[$_].IsPresent
})
if ($MtaCoreFrontendRuntime -and ($Mode -ne 'Smoke' -or $runtimeConflicts.Count -or $PSBoundParameters.ContainsKey('MtaCoreRenderRoute'))) {
    throw 'MTA frontend runtime requires separate opt-in Smoke mode without other trials.'
}
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
if ($MtaNativeMovementTrial -and ($Mode -ne 'Smoke' -or $MtaNativePadReference -or $NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA movement writes require separate opt-in Smoke mode.'
}
if ($MtaNativePadReference -and $Mode -ne 'Smoke') { throw 'MTA native pad reference requires Smoke mode.' }
if ($MtaNativePadReference -and ($NativeInputReference -or $NativeGaitReference -or $MatchedGaitRate -or $LockWalkRoot)) {
    throw 'MTA native pad reference conflicts with other input or gait trials.'
}
if ($NativeGaitReference -and $Mode -ne 'Smoke') { throw 'Native gait reference requires Smoke mode.' }
if ($MatchedGaitRate -and $Mode -ne 'Online') { throw 'Matched gait rate requires Online mode.' }
if ($LockWalkRoot -and $Mode -ne 'Online') { throw 'Walk-root locking requires Online mode.' }
if ($NativeInputReference -and $Mode -ne 'Smoke') { throw 'Native input reference requires Smoke mode.' }
if ($NativeInputReference -and $NativeGaitReference) { throw 'Native input reference conflicts with native gait reference.' }
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path.TrimEnd('\', '/')
# Reuse the removal preflight's complete path/link/hash validation without
# deleting anything. A changed user file must block this operation too.
& (Join-Path $PSScriptRoot 'uninstall.ps1') -GameDirectory $gameRoot -ValidateOnly 6>$null
$manifestPath = Join-Path $gameRoot 'de-online-install-manifest.json'
$configPath = Join-Path $gameRoot 'CLEO/de-online/config.mjs'
$originalManifest = [IO.File]::ReadAllBytes($manifestPath)
$originalConfig = [IO.File]::ReadAllBytes($configPath)
$installed = [Text.Encoding]::UTF8.GetString($originalManifest) | ConvertFrom-Json
if ((Get-FileHash -LiteralPath (Join-Path $gameRoot 'SanAndreas.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $installed.gameSha256) {
    throw 'The installed game changed; investigate its new build before enabling the adapter.'
}
$utf8 = [Text.UTF8Encoding]::new($false)
$configText = $utf8.GetString($originalConfig)
$modePattern = "export const MODE = '(diagnostic|smoke|online)';"
if ([regex]::Matches($configText, $modePattern).Count -ne 1) { throw 'Unexpected mode configuration.' }
$newConfig = $utf8.GetBytes([regex]::Replace($configText, $modePattern, "export const MODE = '$($Mode.ToLowerInvariant())';"))
$referencePattern = 'export const GAIT_REFERENCE = (true|false);'
if ([regex]::Matches($configText, $referencePattern).Count -ne 1) { throw 'Update the adapter before selecting native gait reference.' }
# An ordinary mode switch clears the experiment instead of leaving a hidden
# opt-in active when a later Smoke session was intended to be read-only.
$newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $referencePattern,
    "export const GAIT_REFERENCE = $($NativeGaitReference.IsPresent.ToString().ToLowerInvariant());"))
$ratePattern = 'export const GAIT_RATE = (true|false);'
if ([regex]::Matches($configText, $ratePattern).Count -ne 1) { throw 'Update the adapter before selecting matched gait rate.' }
$newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $ratePattern,
    "export const GAIT_RATE = $($MatchedGaitRate.IsPresent.ToString().ToLowerInvariant());"))
$rootPattern = 'export const GAIT_ROOT_LOCK = (true|false);'
if ([regex]::Matches($configText, $rootPattern).Count -ne 1) { throw 'Update the adapter before selecting walk-root locking.' }
$newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $rootPattern,
    "export const GAIT_ROOT_LOCK = $($LockWalkRoot.IsPresent.ToString().ToLowerInvariant());"))
$inputPattern = 'export const INPUT_REFERENCE = (true|false);'
$inputMatches = [regex]::Matches($configText, $inputPattern).Count
if ($inputMatches -gt 1 -or ($NativeInputReference -and $inputMatches -ne 1)) {
    throw 'Update the adapter before selecting native input reference.'
}
# Preserve ordinary mode switches on older intact installations; they never
# had this opt-in. New installations clear it unless explicitly selected.
if ($inputMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $inputPattern,
        "export const INPUT_REFERENCE = $($NativeInputReference.IsPresent.ToString().ToLowerInvariant());"))
}
$padPattern = 'export const MTA_NATIVE_PAD_REFERENCE = (true|false);'
$padMatches = [regex]::Matches($configText, $padPattern).Count
if ($padMatches -gt 1 -or ($MtaNativePadReference -and $padMatches -ne 1)) {
    throw 'Update the adapter before selecting MTA native pad reference.'
}
if ($MtaNativePadReference -and @($installed.files | Where-Object { $_.path -eq 'CLEO/de-online/native-pad-reference.mjs' }).Count -ne 1) {
    throw 'Update the adapter before selecting MTA native pad reference: its owned module is missing.'
}
# Older intact installations lack this marker and module. Ordinary mode changes
# remain available, but enabling the probe requires both reviewed package pieces.
if ($padMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $padPattern,
        "export const MTA_NATIVE_PAD_REFERENCE = $($MtaNativePadReference.IsPresent.ToString().ToLowerInvariant());"))
}
$movementPattern = 'export const MTA_NATIVE_MOVEMENT_TRIAL = (true|false);'
$movementMatches = [regex]::Matches($configText, $movementPattern).Count
if ($movementMatches -gt 1 -or ($MtaNativeMovementTrial -and $movementMatches -ne 1)) {
    throw 'Update the adapter before selecting MTA movement writes.'
}
if ($MtaNativeMovementTrial -and @($installed.files | Where-Object {
    $_.path -in @('CLEO/de-online/native-movement-trial.mjs', 'CLEO/de-online/game_de.dll')
}).Count -ne 2) { throw 'Update the adapter before selecting MTA movement writes: its owned backend files are missing.' }
# An ordinary mode switch must clear the explicit write trial, including when
# returning to Smoke later. Older intact installations have no such marker.
if ($movementMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $movementPattern,
        "export const MTA_NATIVE_MOVEMENT_TRIAL = $($MtaNativeMovementTrial.IsPresent.ToString().ToLowerInvariant());"))
}
$coreStartupPattern = 'export const MTA_CORE_STARTUP_REFERENCE = (true|false);'
$coreStartupMatches = [regex]::Matches($configText, $coreStartupPattern).Count
if ($coreStartupMatches -gt 1 -or ($MtaCoreStartupReference -and $coreStartupMatches -ne 1)) {
    throw 'Update the adapter before selecting MTA Core startup reference.'
}
if ($MtaCoreStartupReference -and @($installed.files | Where-Object {
    $_.path -in @('CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll', 'CLEO/de-online/game_de.dll',
        'CLEO/de-online/native-movement-trial.mjs', 'CLEO/de-online/native-pad-reference.mjs')
}).Count -ne 5) { throw 'Update the adapter before selecting MTA Core startup reference: its owned Core backend files are missing.' }
# Ordinary mode changes remain valid on old intact manifests without this
# marker. New configurations clear the Core probe unless expressly selected.
if ($coreStartupMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreStartupPattern,
        "export const MTA_CORE_STARTUP_REFERENCE = $($MtaCoreStartupReference.IsPresent.ToString().ToLowerInvariant());"))
}
$clockPattern = 'export const MTA_CORE_CLOCK_REFERENCE = (true|false);'
$clockMatches = [regex]::Matches($configText, $clockPattern).Count
if ($clockMatches -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_CLOCK_REFERENCE\b').Count -ne $clockMatches -or
    ($MtaCoreClockReference -and ($clockMatches -ne 1 -or $installed.files.Count -ne 23))) {
    throw 'Update the adapter before selecting the separate MTA Core clock reference.'
}
if ($clockMatches -eq 1) {
    # Ordinary mode changes revoke this opt-in, while intact older installs
    # without the marker remain usable for their historical modes.
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $clockPattern,
        "export const MTA_CORE_CLOCK_REFERENCE = $($MtaCoreClockReference.IsPresent.ToString().ToLowerInvariant());"))
}
# Set is separately admitted and revoked on ordinary mode changes. Historical
# intact packages without this newer marker retain their original mode support.
$clockSetPattern = 'export const MTA_CORE_CLOCK_SET_REFERENCE = (true|false);'
$clockSetMarkers = [regex]::Matches($configText, $clockSetPattern)
if ($clockSetMarkers.Count -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_CLOCK_SET_REFERENCE\b').Count -ne $clockSetMarkers.Count -or
    ($MtaCoreClockSetReference -and ($clockSetMarkers.Count -ne 1 -or $installed.files.Count -ne 23))) {
    throw 'Update the adapter before selecting the separate MTA Core clock Set reference.'
}
if ($configText -match 'export const MTA_CORE_CLOCK_REFERENCE = true;' -and $configText -match 'export const MTA_CORE_CLOCK_SET_REFERENCE = true;') {
    throw 'Installed configuration combines the independent clock Read and Set trials.'
}
if ($clockSetMarkers.Count -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $clockSetPattern,
        "export const MTA_CORE_CLOCK_SET_REFERENCE = $($MtaCoreClockSetReference.IsPresent.ToString().ToLowerInvariant());"))
}
$applicationPattern = 'export const MTA_CORE_APPLICATION_FRAME_REFERENCE = (true|false);'
$applicationMarkers = [regex]::Matches($configText, $applicationPattern)
if ($applicationMarkers.Count -gt 1 -or [regex]::Matches($configText, '\bMTA_CORE_APPLICATION_FRAME_REFERENCE\b').Count -ne $applicationMarkers.Count -or
    ($MtaCoreApplicationFrameReference -and ($applicationMarkers.Count -ne 1 -or $installed.files.Count -ne 23))) {
    throw 'Update the adapter before selecting the separate application-frame observation.'
}
if ($configText -match 'export const MTA_CORE_APPLICATION_FRAME_REFERENCE = true;') {
    $applicationConflicts = @([regex]::Matches($configText, 'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;') |
        Where-Object {$_.Groups[1].Value -ne 'MTA_CORE_APPLICATION_FRAME_REFERENCE'})
    if ($installed.files.Count -ne 23 -or $installed.mode -ne 'Smoke' -or $applicationConflicts.Count) {
        throw 'Installed configuration combines independent native observation modes or ownership.'
    }
}
if ($applicationMarkers.Count -eq 1) {
    # Ordinary changes revoke the optional observation, including on intact
    # historical installations that do not yet carry this marker.
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $applicationPattern,
        "export const MTA_CORE_APPLICATION_FRAME_REFERENCE = $($MtaCoreApplicationFrameReference.IsPresent.ToString().ToLowerInvariant());"))
}
$startupObservationPattern = 'export const MTA_CORE_STARTUP_OBSERVATION_REFERENCE = (true|false);'
$startupObservationMarkers = [regex]::Matches($configText, $startupObservationPattern)
if ($startupObservationMarkers.Count -gt 1 -or
    [regex]::Matches($configText, '\bMTA_CORE_STARTUP_OBSERVATION_REFERENCE\b').Count -ne $startupObservationMarkers.Count -or
    ($MtaCoreStartupObservationReference -and ($startupObservationMarkers.Count -ne 1 -or $installed.files.Count -ne 23))) {
    throw 'Update the adapter before selecting the separate startup observation.'
}
if ($configText -match 'export const MTA_CORE_STARTUP_OBSERVATION_REFERENCE = true;') {
    # Validate the installed mode before computing any replacement. A request
    # to leave a malformed mixed configuration must not silently legitimize it.
    $startupObservationConflicts = @([regex]::Matches($configText, 'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;') |
        Where-Object {$_.Groups[1].Value -ne 'MTA_CORE_STARTUP_OBSERVATION_REFERENCE'})
    if ($installed.files.Count -ne 23 -or $installed.mode -ne 'Smoke' -or $startupObservationConflicts.Count) {
        throw 'Installed configuration combines independent startup observation modes or ownership.'
    }
}
if ($startupObservationMarkers.Count -eq 1) {
    # Historical intact configurations without this marker keep their ordinary
    # modes; every current ordinary mode explicitly revokes this newer opt-in.
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $startupObservationPattern,
        "export const MTA_CORE_STARTUP_OBSERVATION_REFERENCE = $($MtaCoreStartupObservationReference.IsPresent.ToString().ToLowerInvariant());"))
}
$coreInputPattern = 'export const MTA_CORE_INPUT_REFERENCE = (true|false);'
$coreInputMatches = [regex]::Matches($configText, $coreInputPattern).Count
if ($coreInputMatches -gt 1 -or ($MtaCoreInputReference -and $coreInputMatches -ne 1)) {
    throw 'Update the adapter before selecting MTA Core configured input reference.'
}
if ($MtaCoreInputReference -and @($installed.files | Where-Object {
    $_.path -in @('CLEO/de-online/xmll.dll', 'CLEO/de-online/mta/config/coreconfig.xml', 'CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll')
}).Count -ne 4) { throw 'Update the adapter before selecting MTA Core configured input reference: its owned XML/config files are missing.' }
# Ordinary modes must clear this input reference while remaining compatible
# with historical intact configurations that predate the marker entirely.
if ($coreInputMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreInputPattern,
        "export const MTA_CORE_INPUT_REFERENCE = $($MtaCoreInputReference.IsPresent.ToString().ToLowerInvariant());"))
}
$coreMovementPattern = 'export const MTA_CORE_MOVEMENT_REFERENCE = (true|false);'
$coreMovementMatches = [regex]::Matches($configText, $coreMovementPattern).Count
if ($coreMovementMatches -gt 1 -or ($MtaCoreMovementReference -and $coreMovementMatches -ne 1)) {
    throw 'Update the adapter before selecting MTA Core configured movement reference.'
}
if ($MtaCoreMovementReference -and @($installed.files | Where-Object {
    $_.path -in @('CLEO/de-online/xmll.dll', 'CLEO/de-online/mta/config/coreconfig.xml', 'CLEO/de-online/core.dll',
        'CLEO/de-online/pthread.dll', 'CLEO/de-online/game_de.dll', 'CLEO/de-online/native-movement-trial.mjs',
        'CLEO/de-online/native-pad-reference.mjs')
}).Count -ne 7) { throw 'Update the adapter before selecting MTA Core configured movement reference: its owned Core/backend/XML files are missing.' }
# A prior read-only selection must never leave the separate engine-write trial
# active. Historical intact configurations remain usable without this marker.
if ($coreMovementMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreMovementPattern,
        "export const MTA_CORE_MOVEMENT_REFERENCE = $($MtaCoreMovementReference.IsPresent.ToString().ToLowerInvariant());"))
}
$coreRenderPattern = 'export const MTA_CORE_RENDER_REFERENCE = (true|false);'
$coreRenderMatches = [regex]::Matches($configText, $coreRenderPattern).Count
if ($coreRenderMatches -gt 1 -or ($MtaCoreRenderReference -and $coreRenderMatches -ne 1)) {
    throw 'Update the adapter before selecting MTA Core renderer observation.'
}
if ($MtaCoreRenderReference -and @($installed.files | Where-Object {
    $_.path -in @('CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll', 'CLEO/de-online/game_de.dll')
}).Count -ne 3) { throw 'Update the adapter before selecting MTA Core renderer observation: its owned Core/backend files are missing.' }
# Clear this separate observer on ordinary mode switches; historical intact
# configurations remain usable without an observer marker.
if ($coreRenderMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreRenderPattern,
        "export const MTA_CORE_RENDER_REFERENCE = $($MtaCoreRenderReference.IsPresent.ToString().ToLowerInvariant());"))
}
# A historical observer has no route contract. Keep ordinary switches usable,
# but require the reviewed route marker before activating the newer observer.
$coreRenderRoutePattern = "export const MTA_CORE_RENDER_ROUTE = '(d3d11|d3d12)';"
$coreRenderRouteMatches = [regex]::Matches($configText, $coreRenderRoutePattern).Count
if ($coreRenderRouteMatches -gt 1 -or ($MtaCoreRenderReference -and $coreRenderRouteMatches -ne 1)) {
    throw 'Update the adapter before selecting an explicit MTA Core renderer route.'
}
if ($coreRenderRouteMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreRenderRoutePattern,
        "export const MTA_CORE_RENDER_ROUTE = '$($MtaCoreRenderRoute.ToLowerInvariant())';"))
}
$coreRenderGpuPattern = 'export const MTA_CORE_RENDER_GPU_REFERENCE = (true|false);'
$coreRenderGpuMatches = [regex]::Matches($configText, $coreRenderGpuPattern).Count
if ($coreRenderGpuMatches -gt 1 -or ($MtaCoreRenderGpuReference -and $coreRenderGpuMatches -ne 1)) {
    throw 'Update the adapter before selecting GPU ownership queries.'
}
# Historical read-only observers keep working; ordinary mode switches clear
# the newer COM-query opt-in instead of silently inheriting it from a trial.
if ($coreRenderGpuMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreRenderGpuPattern,
        "export const MTA_CORE_RENDER_GPU_REFERENCE = $($MtaCoreRenderGpuReference.IsPresent.ToString().ToLowerInvariant());"))
}
$coreRenderCustomPresentPattern = 'export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = (true|false);'
$coreRenderCustomPresentMatches = [regex]::Matches($configText, $coreRenderCustomPresentPattern).Count
if ($coreRenderCustomPresentMatches -gt 1 -or ($MtaCoreRenderCustomPresentReference -and $coreRenderCustomPresentMatches -ne 1)) {
    throw 'Update the adapter before selecting custom-present recognition.'
}
# Classification is independent opt-in even on an existing GPU diagnostic.
# Leaving API4 must clear it, while historical intact configurations stay usable.
if ($coreRenderCustomPresentMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreRenderCustomPresentPattern,
        "export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = $($MtaCoreRenderCustomPresentReference.IsPresent.ToString().ToLowerInvariant());"))
}
$coreRenderFrameBoundaryPattern = 'export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = (true|false);'
$coreRenderFrameBoundaryMatches = [regex]::Matches($configText, $coreRenderFrameBoundaryPattern).Count
if ($coreRenderFrameBoundaryMatches -gt 1 -or ($MtaCoreRenderFrameBoundaryReference -and $coreRenderFrameBoundaryMatches -ne 1)) {
    throw 'Update the adapter before selecting frame-boundary observation.'
}
# API5 must be cleared on every ordinary mode switch; older intact markers
# cannot grant this new hook lease merely by requesting API4 recognition.
if ($coreRenderFrameBoundaryMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreRenderFrameBoundaryPattern,
        "export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = $($MtaCoreRenderFrameBoundaryReference.IsPresent.ToString().ToLowerInvariant());"))
}
$coreRenderNativeDispatchPattern = 'export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = (true|false);'
$coreRenderNativeDispatchMatches = [regex]::Matches($configText, $coreRenderNativeDispatchPattern).Count
if ($coreRenderNativeDispatchMatches -gt 1 -or ($MtaCoreRenderNativeDispatchReference -and $coreRenderNativeDispatchMatches -ne 1)) {
    throw 'Update the adapter before selecting native-dispatch observation.'
}
# Clear the newer lease on ordinary switches while preserving intact old configurations.
if ($coreRenderNativeDispatchMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $coreRenderNativeDispatchPattern,
        "export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = $($MtaCoreRenderNativeDispatchReference.IsPresent.ToString().ToLowerInvariant());"))
}
$guiPattern = 'export const MTA_CORE_GUI_STARTUP_REFERENCE = (true|false);'
$guiMatches = [regex]::Matches($configText, $guiPattern).Count
if ($guiMatches -gt 1 -or ($MtaCoreGuiStartupReference -and $guiMatches -ne 1)) { throw 'Update the adapter before selecting MTA Core GUI startup.' }
if ($MtaCoreGuiStartupReference -or $MtaCoreGuiSurfaceReference -or $MtaCoreGuiFrontendReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendStartupReference -or $MtaCorePersistentFrontend -or $MtaCoreFrontendRuntime) {
    $guiPaths = @('CLEO/de-online/cgui.dll',
        'CLEO/de-online/mta/cgui/sans.ttf', 'CLEO/de-online/mta/cgui/saheader.ttf', 'CLEO/de-online/mta/cgui/sagothic.ttf',
        'CLEO/de-online/mta/cgui/Imageset.xsd', 'CLEO/de-online/mta/cgui/GUIScheme.xsd', 'CLEO/de-online/mta/cgui/Font.xsd',
        'CLEO/de-online/mta/cgui/Falagard.xsd', 'CLEO/de-online/mta/cgui/unifont.ttf',
        'CLEO/de-online/skins/Default/CGUI.xml', 'CLEO/de-online/skins/Default/CGUI.is.xml',
        'CLEO/de-online/skins/Default/CGUI.lnf.xml', 'CLEO/de-online/skins/Default/CGUI.png')
    if (@($installed.files | Where-Object { $_.path -in $guiPaths }).Count -ne 13) { throw 'Update the adapter before selecting MTA Core GUI startup: complete owned GUI assets are missing.' }
}
# A later ordinary mode selection clears this independent GPU startup opt-in,
# including on complete GUI installs whose assets remain owned for uninstall.
if ($guiMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $guiPattern,
        "export const MTA_CORE_GUI_STARTUP_REFERENCE = $($MtaCoreGuiStartupReference.IsPresent.ToString().ToLowerInvariant());"))
}
# Old configurations remain usable, but cannot infer this newer surface opt-in.
$surfacePattern = 'export const MTA_CORE_GUI_SURFACE_REFERENCE = (true|false);'
$surfaceMatches = [regex]::Matches($configText, $surfacePattern).Count
if ($surfaceMatches -gt 1 -or ($MtaCoreGuiSurfaceReference -and $surfaceMatches -ne 1)) { throw 'Update the adapter before selecting MTA Core GUI surface.' }
if ($surfaceMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $surfacePattern,
        "export const MTA_CORE_GUI_SURFACE_REFERENCE = $($MtaCoreGuiSurfaceReference.IsPresent.ToString().ToLowerInvariant());"))
}
$frontendPattern = 'export const MTA_CORE_GUI_FRONTEND_REFERENCE = (true|false);'
$frontendMatches = [regex]::Matches($configText, $frontendPattern).Count
if ($frontendMatches -gt 1 -or ($MtaCoreGuiFrontendReference -and $frontendMatches -ne 1)) { throw 'Update the adapter before selecting MTA Core GUI frontend.' }
if ($MtaCoreGuiFrontendReference -or $MtaCoreGuiFrontendInputReference -or $MtaCoreGuiFrontendStartupReference -or $MtaCorePersistentFrontend -or $MtaCoreFrontendRuntime) {
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
    if (@($installed.files | Where-Object { $_.path -in $frontendPaths }).Count -ne 22) { throw 'Update the adapter before selecting MTA Core GUI frontend: complete owned frontend assets are missing.' }
}
# Ordinary switches revoke the opt-in while retaining owned assets for cleanup.
if ($frontendMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $frontendPattern,
        "export const MTA_CORE_GUI_FRONTEND_REFERENCE = $($MtaCoreGuiFrontendReference.IsPresent.ToString().ToLowerInvariant());"))
}
# Clear the newer mode on every ordinary selection while accepting intact
# historical packages that predate its optional marker.
$frontendInputPattern = 'export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE = (true|false);'
$frontendInputMatches = [regex]::Matches($configText, $frontendInputPattern).Count
if ($frontendInputMatches -gt 1 -or ([regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE\b').Count -ne $frontendInputMatches) -or ($MtaCoreGuiFrontendInputReference -and $frontendInputMatches -ne 1)) { throw 'Update the adapter before selecting MTA Core GUI frontend input.' }
if ($frontendInputMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $frontendInputPattern,
        "export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE = $($MtaCoreGuiFrontendInputReference.IsPresent.ToString().ToLowerInvariant());"))
}
# Older owned installations may predate this optional marker. Ordinary mode
# changes clear it when present; selecting the new mode requires current bytes.
$frontendStartupPattern = 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = (true|false);'
$frontendStartupMatches = [regex]::Matches($configText, $frontendStartupPattern).Count
if ($frontendStartupMatches -gt 1 -or ([regex]::Matches($configText, 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE\b').Count -ne $frontendStartupMatches) -or ($MtaCoreGuiFrontendStartupReference -and $frontendStartupMatches -ne 1)) { throw 'Update the adapter before selecting completed MTA Core GUI frontend startup.' }
if ($frontendStartupMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $frontendStartupPattern,
        "export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = $($MtaCoreGuiFrontendStartupReference.IsPresent.ToString().ToLowerInvariant());"))
}
$runtimePattern = 'export const MTA_CORE_FRONTEND_RUNTIME = (true|false);'
$runtimeMatches = [regex]::Matches($configText, $runtimePattern).Count
if ($runtimeMatches -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_FRONTEND_RUNTIME\b').Count -ne $runtimeMatches -or ($MtaCoreFrontendRuntime -and $runtimeMatches -ne 1)) { throw 'Update the adapter before selecting MTA frontend runtime.' }
if ($runtimeMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $runtimePattern,
        "export const MTA_CORE_FRONTEND_RUNTIME = $($MtaCoreFrontendRuntime.IsPresent.ToString().ToLowerInvariant());"))
}
$persistentPattern = 'export const MTA_CORE_PERSISTENT_FRONTEND = (true|false);'
$persistentMatches = [regex]::Matches($configText, $persistentPattern).Count
if ($persistentMatches -gt 1 -or [regex]::Matches($configText, 'export const MTA_CORE_PERSISTENT_FRONTEND\b').Count -ne $persistentMatches -or ($MtaCorePersistentFrontend -and $persistentMatches -ne 1)) { throw 'Update the adapter before selecting persistent MTA frontend.' }
if ($persistentMatches -eq 1) {
    $newConfig = $utf8.GetBytes([regex]::Replace($utf8.GetString($newConfig), $persistentPattern,
        "export const MTA_CORE_PERSISTENT_FRONTEND = $($MtaCorePersistentFrontend.IsPresent.ToString().ToLowerInvariant());"))
}
$configEntry = @($installed.files | Where-Object { $_.path -eq 'CLEO/de-online/config.mjs' })
if ($configEntry.Count -ne 1) { throw 'Missing owned configuration record.' }
$configEntry[0].sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($newConfig)).ToLowerInvariant()
$installed.mode = $Mode
$newManifest = $utf8.GetBytes(($installed | ConvertTo-Json -Depth 6) + "`n")
if (-not $PSCmdlet.ShouldProcess($configPath, "Set the installed adapter mode to $Mode")) { return }

function Write-AtomicFile([string]$Path, [byte[]]$Bytes) {
    $temporary = $Path + '.de-online-' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try {
            $stream.Write($Bytes, 0, $Bytes.Length)
            # Flush data before replacement; a system restart must not leave a
            # correctly sized configuration file whose contents were never saved.
            $stream.Flush($true)
        } finally { $stream.Dispose() }
        [IO.File]::Move($temporary, $Path, $true)
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}

try {
    Write-AtomicFile $configPath $newConfig
    Write-AtomicFile $manifestPath $newManifest
} catch {
    $failure = $_
    Write-AtomicFile $configPath $originalConfig
    Write-AtomicFile $manifestPath $originalManifest
    throw $failure
}
if ($MtaCoreFrontendRuntime) {
    Write-Host 'Installed adapter mode: Smoke, retained MTA frontend runtime. Ctrl+Shift+F9 toggles views after release; Quit/Escape hides, Ctrl+Shift+End stops. Applied bindings survive hide/show for this session.'
} elseif ($MtaCorePersistentFrontend) {
    Write-Host 'Installed adapter mode: Smoke, renewable MTA frontend. Ctrl+Shift+F9 starts after release; Quit/Escape/End closes it. Focus loss or heartbeat expiry requests cleanup.'
} elseif ($MtaCoreGuiFrontendStartupReference) {
    Write-Host 'Installed adapter mode: Smoke, completed MTA Core frontend startup armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup.'
} elseif ($MtaCoreGuiFrontendInputReference) {
    Write-Host 'Installed adapter mode: Smoke, MTA Core frontend with scoped neutral engine input armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup.'
} elseif ($MtaCoreGuiFrontendReference) {
    Write-Host 'Installed adapter mode: Smoke, existing MTA frontend with bounded script-owned player-control lease armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup.'
} elseif ($MtaCoreGuiSurfaceReference) {
    Write-Host 'Installed adapter mode: Smoke, MTA child composition GUI surface armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup.'
} elseif ($MtaCoreGuiStartupReference) {
    Write-Host 'Installed adapter mode: Smoke, MTA GUI startup on an owned offscreen queue armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup.'
} elseif ($MtaCoreRenderNativeDispatchReference) {
    Write-Host 'Installed adapter mode: Smoke, read-only scoped native D3D12 dispatch armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup. No drawing or input writes.'
} elseif ($MtaCoreRenderFrameBoundaryReference) {
    Write-Host 'Installed adapter mode: Smoke, read-only scoped D3D12 Present association armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup. No drawing or input writes.'
} elseif ($MtaCoreRenderCustomPresentReference) {
    Write-Host 'Installed adapter mode: Smoke, read-only D3D12 custom-present code recognition armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup. No drawing or input writes.'
} elseif ($MtaCoreRenderGpuReference) {
    Write-Host 'Installed adapter mode: Smoke, read-only D3D12 GPU ownership queries armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup. No drawing or input writes.'
} elseif ($MtaCoreRenderReference) {
    Write-Host "Installed adapter mode: Smoke, Core $MtaCoreRenderRoute observation armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End requests cleanup. No drawing or input writes."
} elseif ($MtaCoreMovementReference) {
    Write-Host 'Installed adapter mode: Smoke, bounded Core configured movement writes armed. Ctrl+Shift+F9 activates after release; Ctrl+Shift+End requests cleanup.'
} elseif ($MtaCoreInputReference) {
    Write-Host 'Installed adapter mode: Smoke, read-only Core configured input reference armed. Ctrl+Shift+F9 starts after release; Ctrl+Shift+End cancels.'
} elseif ($MtaCoreStartupReference) {
    Write-Host 'Installed adapter mode: Smoke, MTA Core startup reference armed. Follow the bounded startup plan; Ctrl+Shift+End stops the adapter.'
} elseif ($MtaNativePadReference) {
    Write-Host 'Installed adapter mode: Smoke, read-only MTA native pad reference armed. Follow the bounded probe plan; Ctrl+Shift+End stops the adapter.'
} elseif ($NativeInputReference) {
    Write-Host 'Installed adapter mode: Smoke, read-only local input reference armed. Ctrl+Shift+F9 starts a bounded 30-second observation; Ctrl+Shift+End cancels or stops it.'
} elseif ($LockWalkRoot) {
    Write-Host 'Installed adapter mode: Online, horizontal walk-root comparison armed. Ctrl+Shift+Home activates it; Ctrl+Shift+End stops it.'
} elseif ($MatchedGaitRate) {
    Write-Host 'Installed adapter mode: Online, matched gait trial armed. Ctrl+Shift+Home activates it; Ctrl+Shift+End stops it.'
} elseif ($NativeGaitReference) {
    Write-Host 'Installed adapter mode: Smoke, native gait reference armed. Outside on a flat street, Ctrl+Shift+Home starts one short NPC walk; Ctrl+Shift+End cancels or stops it.'
} elseif ($Mode -eq 'Diagnostic') {
    Write-Host 'Installed adapter mode: Diagnostic. The script records host metadata and exits without loading the bridge.'
} elseif ($Mode -eq 'Online' -and $utf8.GetString($newConfig) -match "export const ONLINE_ACTIVATION = 'manual';") {
    # Preserve the activation preference and explain intentional idle behavior.
    Write-Host 'Installed adapter mode: Online, awaiting Ctrl+Shift+Home during gameplay. Ctrl+Shift+End cancels or stops the adapter.'
} else {
    Write-Host "Installed adapter mode: $Mode. Ctrl+Shift+End stops the adapter during gameplay."
}
