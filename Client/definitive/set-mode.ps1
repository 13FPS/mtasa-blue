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
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
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
if ($MtaCoreMovementReference) {
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
