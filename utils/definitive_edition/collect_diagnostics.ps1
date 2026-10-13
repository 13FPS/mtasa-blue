#!/usr/bin/env pwsh
#requires -Version 7.0
# Preserve startup evidence without launching GTA, capturing windows, or changing devices.
[CmdletBinding()]
param(
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition\Gameface\Binaries\Win64',
    [ValidateRange(1, 168)][int]$Hours = 24,
    [switch]$RequireReady
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$evidenceRoot = Join-Path $repositoryRoot ('Build/definitive-diagnostics/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$issues = [Collections.Generic.List[string]]::new()
$blockers = [Collections.Generic.List[string]]::new()
$since = (Get-Date).AddHours(-$Hours)
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'

function Write-Evidence([string]$Name, [byte[]]$Bytes) {
    $path = Join-Path $evidenceRoot $Name
    $stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($Bytes, 0, $Bytes.Length); $stream.Flush($true) } finally { $stream.Dispose() }
}

function Read-Events([string]$Log, [string]$Provider = '') {
    $filter = @{ LogName=$Log; StartTime=$since }
    if ($Provider) { $filter.ProviderName = $Provider }
    try { @(Get-WinEvent -FilterHashtable $filter -MaxEvents 2000 -ErrorAction Stop) }
    catch {
        # An empty log is normal; inaccessible logs must be visible in the report.
        if ($_.FullyQualifiedErrorId -notlike 'NoMatchingEventsFound*') { $issues.Add("Cannot read ${Log}: $($_.Exception.Message)") }
        @()
    }
}

$os = $null
$osRelease = $null
$graphics = @()
try { $os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber, LastBootUpTime }
catch { $issues.Add($_.Exception.Message) }
try { $osRelease = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' | Select-Object DisplayVersion, CurrentBuildNumber, UBR }
catch { $issues.Add("OS release query: $($_.Exception.Message)") }
try {
    $graphics = @(Get-CimInstance Win32_VideoController | Select-Object Name, Status, ConfigManagerErrorCode, PNPDeviceID, DriverVersion, DriverDate)
    if (-not $graphics.Count) { $blockers.Add('No display device was reported.') }
    foreach ($device in $graphics) {
        if ($null -eq $device.ConfigManagerErrorCode -or $device.ConfigManagerErrorCode -ne 0 -or $device.Status -ne 'OK') {
            $blockers.Add("Display device $($device.Name) is not healthy: status $($device.Status), problem code $($device.ConfigManagerErrorCode).")
        }
    }
} catch { $issues.Add($_.Exception.Message); $blockers.Add('Display device health could not be read.') }
$pnpGraphics = @()
try { $pnpGraphics = @(Get-PnpDevice -Class Display -ErrorAction Stop | Select-Object Status, FriendlyName, InstanceId, Problem) }
catch { $issues.Add("PnP display query unavailable: $($_.Exception.Message)") }
$runningGames = @(Get-Process -Name SanAndreas, PlayGTASanAndreas -ErrorAction SilentlyContinue | Select-Object Id, ProcessName)
if ($runningGames.Count) { $blockers.Add('GTA or its bootstrap is running; close it before changing the diagnostic profile.') }

$systemEvents = @(Read-Events 'System' | Where-Object {
    ($_.ProviderName -eq 'Microsoft-Windows-Kernel-Power' -and $_.Id -eq 41) -or $_.Id -eq 6008 -or
    $_.ProviderName -in @('Display', 'amdwddmg', 'Microsoft-Windows-WHEA-Logger', 'Microsoft-Windows-WER-SystemErrorReporting')
} | Select-Object TimeCreated, Id, ProviderName, RecordId, Message)
$werEvents = @(Read-Events 'Application' 'Windows Error Reporting' | Where-Object { $_.Message -match 'LiveKernelEvent' })
# WER replays old queued reports after reboot. Group by dump identity instead of
# interpreting every reporting timestamp as a new crash during this experiment.
$dumpRecords = @(foreach ($event in $werEvents) {
    $code = [regex]::Match($event.Message, '(?m)^P1: (\S+)').Groups[1].Value
    foreach ($match in [regex]::Matches($event.Message, 'WATCHDOG-(\d{8})-(\d{4})\.dmp')) {
        [pscustomobject]@{ code=$code; dump=$match.Value; reported=$event.TimeCreated; recordId=$event.RecordId }
    }
})
$dumpGroups = @($dumpRecords | Group-Object code,dump | ForEach-Object {
    $ordered = @($_.Group | Sort-Object reported)
    [pscustomobject]@{ code=$ordered[0].code; dump=$ordered[0].dump; reportCount=$ordered.Count; firstReported=$ordered[0].reported; lastReported=$ordered[-1].reported }
} | Sort-Object dump -Descending)
# Keep one full report per dump for parameters and access paths; duplicate replay
# messages only add size, while their counts remain in the JSON summary.
$uniqueWer = @($dumpRecords | Group-Object code,dump | ForEach-Object {
    $recordId = $_.Group[0].recordId
    $werEvents | Where-Object RecordId -eq $recordId | Select-Object -First 1 TimeCreated, RecordId, Message
})
$utf8 = [Text.UTF8Encoding]::new($false)
Write-Evidence 'windows-events.json' ($utf8.GetBytes((@{ system=$systemEvents; liveKernelReports=$uniqueWer } | ConvertTo-Json -Depth 8)))

$game = [ordered]@{ directory=$GameDirectory; sha256=$null; knownBuild=$false; installedMode=$null; ownedFilesVerified=$false; logBytes=$null }
try {
    $gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path.TrimEnd('\', '/')
    $game.sha256 = (Get-FileHash -LiteralPath (Join-Path $gameRoot 'SanAndreas.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    $game.knownBuild = $game.sha256 -eq $knownGameHash
    if (-not $game.knownBuild) { $blockers.Add('Game executable differs from the verified build.') }
    $manifestPath = Join-Path $gameRoot 'de-online-install-manifest.json'
    $manifestBytes = [IO.File]::ReadAllBytes($manifestPath)
    Write-Evidence 'installed-manifest.json' $manifestBytes
    $manifest = $utf8.GetString($manifestBytes) | ConvertFrom-Json
    $game.installedMode = $manifest.mode
    # This uses the existing complete owned-file/link/hash preflight. ValidateOnly
    # returns before removal is proposed or executed; no executable is loaded.
    & (Join-Path $repositoryRoot 'Client/definitive/uninstall.ps1') -GameDirectory $gameRoot -ValidateOnly 6>$null
    $game.ownedFilesVerified = $true
    $logPath = Join-Path $gameRoot 'cleo_redux.log'
    if (Test-Path -LiteralPath $logPath -PathType Leaf) {
        $logFile = Get-Item -LiteralPath $logPath
        $game.logBytes = $logFile.Length
        if ($logFile.Length -le 16MB) { Write-Evidence 'cleo_redux.log' ([IO.File]::ReadAllBytes($logPath)) }
        else { $issues.Add('CLEO log exceeds 16 MiB; retained in the game directory, not copied.') }
    }
} catch { $issues.Add("Game inspection: $($_.Exception.Message)"); $blockers.Add('Game/install inspection did not complete.') }

$report = [ordered]@{
    collectedAt=(Get-Date -Format o); lookbackHours=$Hours; readOnlySystemInspection=$true
    gameLaunched=$false; deviceChanges=$false; os=$os; osRelease=$osRelease; graphics=$graphics; pnpGraphics=$pnpGraphics
    runningGames=$runningGames; game=$game; liveDumpReports=$dumpGroups
    manualTestPrerequisitesMet=($blockers.Count -eq 0); blockers=@($blockers.ToArray()); collectionIssues=@($issues.ToArray())
    stabilityEstablished=$false; note='A healthy device query does not prove runtime stability. Dumps have not been analyzed. Event queries are limited to 2000 most recent entries per log/provider. No game launch or window capture is performed.'
}
Write-Evidence 'report.json' ($utf8.GetBytes(($report | ConvertTo-Json -Depth 8)))
$lines = [Collections.Generic.List[string]]::new()
$lines.Add('# Definitive Edition startup diagnostics')
$lines.Add('')
$lines.Add("Collected: $($report.collectedAt). No game launch, window capture or device changes.")
$lines.Add('')
foreach ($device in $graphics) { $lines.Add("- GPU: $($device.Name); status $($device.Status); code $($device.ConfigManagerErrorCode); driver $($device.DriverVersion).") }
$lines.Add("- Installed mode: $($game.installedMode); owned files verified: $($game.ownedFilesVerified); known game hash: $($game.knownBuild).")
$lines.Add('- Runtime stability remains unverified. Event reporting times can refer to older queued dumps.')
$lines.Add('')
foreach ($blocker in $blockers) { $lines.Add("BLOCKED: $blocker") }
foreach ($issue in $issues) { $lines.Add("Collection issue: $issue") }
$lines.Add('')
$lines.Add('See report.json for grouped dump identities and windows-events.json for the corresponding event parameters.')
$lines.Add('')
$lines.Add('Code 22: https://learn.microsoft.com/en-us/windows-hardware/drivers/install/cm-prob-disabled')
$lines.Add('Code 141: https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x141---video-engine-timeout-detected')
$lines.Add('Code 193: https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x193--video-dxgkrnl-livedump')
Write-Evidence 'report.md' ($utf8.GetBytes(($lines -join "`n") + "`n"))
Write-Host "Diagnostics saved: $evidenceRoot"
foreach ($blocker in $blockers) { Write-Host "BLOCKED: $blocker" }
if ($RequireReady -and $blockers.Count) { exit 2 }
