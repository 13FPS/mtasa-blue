#!/usr/bin/env pwsh
#requires -Version 7.0
# One disposable game fingerprint exercises the new mode without nine copied
# executables. No script launches GTA; all production writes stay in Build.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageDirectory,
    [Parameter(Mandatory)][string]$GameExecutable,
    [string]$PreflightDll,
    [string]$LauncherExe
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$sourcePackage = (Resolve-Path -LiteralPath $PackageDirectory).Path
$sourceExecutable = (Resolve-Path -LiteralPath $GameExecutable).Path
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$utf8 = [Text.UTF8Encoding]::new($false)
$basePaths = @(
    'version.dll', 'cleo_redux64.asi', 'CLEO/CLEO_PLUGINS/dylib64.cleo', 'CLEO/CLEO_PLUGINS/Input64.cleo',
    'CLEO/.config/cleo.ini', 'CLEO/.config/sa_unreal.json', 'CLEO/.config/unknown_x64.json',
    'CLEO/de-online/index.js', 'CLEO/de-online/controller.mjs', 'CLEO/de-online/config.mjs',
    'CLEO/de-online/mod.json', 'CLEO/de-online/de_client_bridge.dll',
    'CLEO/de-online/native-pad-reference.mjs', 'CLEO/de-online/native-movement-trial.mjs',
    'CLEO/de-online/game_de.dll', 'CLEO/de-online/core.dll', 'CLEO/de-online/pthread.dll',
    'CLEO/de-online/xmll.dll', 'CLEO/de-online/mta/config/coreconfig.xml',
    'DE_ONLINE_LICENSES/CLEO_REDUX_LICENSE.txt', 'DE_ONLINE_LICENSES/ULTIMATE_ASI_LOADER_LICENSE.txt',
    'DE_ONLINE_LICENSES/MTA_GPLv3.txt', 'DE_ONLINE_LICENSES/SANNY_BUILDER_LIBRARY_README.md')
$guiPaths = @('CLEO/de-online/cgui.dll',
    'CLEO/de-online/mta/cgui/sans.ttf', 'CLEO/de-online/mta/cgui/saheader.ttf', 'CLEO/de-online/mta/cgui/sagothic.ttf',
    'CLEO/de-online/mta/cgui/Imageset.xsd', 'CLEO/de-online/mta/cgui/GUIScheme.xsd', 'CLEO/de-online/mta/cgui/Font.xsd',
    'CLEO/de-online/mta/cgui/Falagard.xsd', 'CLEO/de-online/mta/cgui/unifont.ttf',
    'CLEO/de-online/skins/Default/CGUI.xml', 'CLEO/de-online/skins/Default/CGUI.is.xml',
    'CLEO/de-online/skins/Default/CGUI.lnf.xml', 'CLEO/de-online/skins/Default/CGUI.png')
$frontendPaths = @(
    'CLEO/de-online/mta/cgui/images/background.png', 'CLEO/de-online/mta/cgui/images/background_logo.png',
    'CLEO/de-online/mta/cgui/images/mta_filler.png', 'CLEO/de-online/mta/cgui/images/version.png',
    'CLEO/de-online/mta/cgui/images/the_language_icon.png', 'CLEO/de-online/mta/cgui/images/serverbrowser/locked.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search-servers.png', 'CLEO/de-online/mta/cgui/images/serverbrowser/search-players.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/refresh.png', 'CLEO/de-online/mta/cgui/images/serverbrowser/favorite.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/connect.png', 'CLEO/de-online/mta/cgui/images/serverbrowser/info.png',
    'CLEO/de-online/mta/cgui/images/serverbrowser/search.png', 'CLEO/de-online/mta/cgui/images/serverbrowser/trashcan.png',
    'CLEO/de-online/mta/locale/en_US/menu_disconnect.png', 'CLEO/de-online/mta/locale/en_US/menu_quick_connect.png',
    'CLEO/de-online/mta/locale/en_US/menu_browse_servers.png', 'CLEO/de-online/mta/locale/en_US/menu_settings.png',
    'CLEO/de-online/mta/locale/en_US/menu_about.png', 'CLEO/de-online/mta/locale/en_US/menu_quit.png',
    'CLEO/de-online/mta/locale/en_US/latest_news.png', 'CLEO/de-online/mta/locale/en_US/client.pot')
$allPaths = $basePaths + $guiPaths + $frontendPaths
$configRelative = 'CLEO/de-online/config.mjs'
$newMarker = 'MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE'
$scripts = @('install.ps1', 'update.ps1', 'uninstall.ps1', 'set-mode.ps1', 'package.ps1')
$outputRoot = Join-Path $repositoryRoot ('Build/mta-frontend-input-package-tests/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
$fixtureRoot = Join-Path $outputRoot 'repository'
$fixtureScripts = Join-Path $fixtureRoot 'Client/definitive'
$fixturePackage = Join-Path $fixtureRoot 'Build/definitive-package/Smoke'
$fixtureGame = Join-Path $outputRoot 'game'
$logPath = Join-Path $outputRoot 'production-scripts.log'
$checks = [Collections.Generic.List[string]]::new()
$sourcePins = @()
$packagePins = @()
$preflightArtifacts = @()
$preflightCases = [Collections.Generic.List[object]]::new()
$preflightEnabled = $PreflightDll -or $LauncherExe
$fixtureLauncher = Join-Path $fixtureRoot 'launcher/Multi Theft Auto.exe'

function Get-Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert-NoLinks([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Linked path: $candidate" }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}
function Assert-Check([bool]$Condition, [string]$Label) {
    if (-not $Condition) { throw "Check failed: $Label" }
    $checks.Add($Label)
}
function Write-Fixture([string]$Path, [byte[]]$Bytes) {
    $target = [IO.Path]::GetFullPath($Path)
    if (-not $target.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Fixture write escaped Build: $target" }
    Assert-NoLinks $target
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    [IO.File]::WriteAllBytes($target, $Bytes)
}
function Write-FixtureJson([string]$Path, [object]$Value) { Write-Fixture $Path ($utf8.GetBytes(($Value | ConvertTo-Json -Depth 12) + "`n")) }
function Copy-Fixture([string]$Source, [string]$Destination) {
    Assert-NoLinks $Source
    Write-Fixture $Destination ([IO.File]::ReadAllBytes($Source))
}
function Get-TreeHash([string]$Root) {
    $lines = @(Get-ChildItem -LiteralPath $Root -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($Root, $_.FullName).Replace('\', '/')
        if ($_.PSIsContainer) { 'D ' + $relative } else { 'F ' + $relative + ' ' + (Get-Sha $_.FullName) }
    })
    [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($utf8.GetBytes(($lines -join "`n")))).ToLowerInvariant()
}
function Invoke-Production([string]$Name, [hashtable]$Arguments) {
    Add-Content -LiteralPath $logPath -Value ($Name + ' ' + ($Arguments.Keys -join ',')) -Encoding utf8
    try { & (Join-Path $fixtureScripts $Name) @Arguments 6>&1 | ForEach-Object { Add-Content -LiteralPath $logPath -Value $_.ToString() -Encoding utf8 } }
    catch { Add-Content -LiteralPath $logPath -Value ('REFUSED: ' + $_.Exception.Message) -Encoding utf8; throw }
}
function Assert-Refused([scriptblock]$Action, [string]$Label, [string]$Pattern = '.') {
    $before = Get-TreeHash $fixtureRoot
    $beforeGame = Get-TreeHash $fixtureGame
    $message = $null
    try { & $Action } catch { $message = $_.Exception.Message }
    Assert-Check ($null -ne $message -and $message -match $Pattern) ($Label + ' refused')
    Assert-Check ($before -eq (Get-TreeHash $fixtureRoot) -and $beforeGame -eq (Get-TreeHash $fixtureGame)) ($Label + ' no writes')
}
function Read-Installed { Get-Content -LiteralPath (Join-Path $fixtureGame 'de-online-install-manifest.json') -Raw | ConvertFrom-Json }
function Assert-Installed([int]$Count) {
    $installed = Read-Installed
    Assert-Check (@($installed.files).Count -eq $Count) "Installed exact $Count records"
    foreach ($entry in $installed.files) {
        Assert-Check ($entry.destination -eq (Join-Path $fixtureGame $entry.path) -and (Get-Sha $entry.destination) -eq $entry.sha256) ('Installed verified ' + $entry.path)
    }
    Invoke-Production 'uninstall.ps1' @{GameDirectory=$fixtureGame;ValidateOnly=$true}
}
function Set-PackageConfig([string]$Text) {
    Write-Fixture (Join-Path $fixturePackage ('Win64/' + $configRelative)) ($utf8.GetBytes($Text))
    $manifest = $originalPackageManifest | ConvertFrom-Json
    @($manifest.files | Where-Object path -eq $configRelative)[0].sha256 = Get-Sha (Join-Path $fixturePackage ('Win64/' + $configRelative))
    Write-FixtureJson (Join-Path $fixturePackage 'manifest.json') $manifest
}
function Invoke-Preflight([string]$Name, [int]$ExpectedExit, [string]$ExpectedStatus) {
    if (-not $preflightEnabled) { return }
    $before = Get-TreeHash $fixtureRoot
    $beforeGame = Get-TreeHash $fixtureGame
    # Reuse the normal launcher's explicit read-only dispatch. The preflight
    # loader is its adjacent DLL, never an alternate GTA bootstrap executable.
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $fixtureLauncher
    $start.WorkingDirectory = $outputRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('--de-preflight', '--game-exe', (Join-Path $fixtureGame 'SanAndreas.exe'),
        '--bundle', (Join-Path $fixturePackage 'Win64'), '--manifest', (Join-Path $fixturePackage 'manifest.json'))) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        Assert-Check ($process.Start()) ("Preflight $Name started owned launcher")
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(10000)) {
            # Only this newly started owned launcher handle may be terminated.
            # SanAndreas.exe is supplied as a hash input and never executed.
            $process.Kill()
            $process.WaitForExit()
            throw "Preflight $Name blocked instead of returning headless status."
        }
        $watch.Stop()
        $text = $stdout.GetAwaiter().GetResult()
        $errors = $stderr.GetAwaiter().GetResult()
        $stdoutPath = Join-Path $outputRoot ("preflight-$Name.stdout.json")
        $stderrPath = Join-Path $outputRoot ("preflight-$Name.stderr.log")
        Write-Fixture $stdoutPath ($utf8.GetBytes($text))
        Write-Fixture $stderrPath ($utf8.GetBytes($errors))
        Assert-Check ($process.ExitCode -eq $ExpectedExit) ("Preflight $Name exit $ExpectedExit; actual=$($process.ExitCode): $text $errors")
        $record = $text | ConvertFrom-Json
        Assert-Check ($record.status -ceq $ExpectedStatus) ("Preflight $Name status $ExpectedStatus")
        Assert-Check ($record.preflightValid -eq ($ExpectedExit -eq 0)) ("Preflight $Name success flag")
        Assert-Check (-not $record.launchReady -and -not $record.fullMtaReady -and -not $record.gameLaunched -and
            -not $record.coreLoaded -and -not $record.installationWrites) ("Preflight $Name remains read-only and startup gated")
        Assert-Check ($record.verifiedFiles -eq $(if ($ExpectedExit -eq 0) { 58 } else { 0 })) ("Preflight $Name owned count")
        Assert-Check ([string]::IsNullOrWhiteSpace($errors)) ("Preflight $Name no stderr")
        Assert-Check ($before -eq (Get-TreeHash $fixtureRoot) -and $beforeGame -eq (Get-TreeHash $fixtureGame)) ("Preflight $Name actual fixture unchanged")
        $preflightCases.Add([ordered]@{name=$Name;exit_code=$process.ExitCode;status=$record.status;elapsed_ms=$watch.ElapsedMilliseconds;
            stdout=$stdoutPath;stdout_sha256=(Get-Sha $stdoutPath);stderr=$stderrPath;stderr_sha256=(Get-Sha $stderrPath);record=$record})
    } finally { $process.Dispose() }
}

Assert-NoLinks $sourcePackage
Assert-NoLinks $sourceExecutable
if ((Get-Sha $sourceExecutable) -ne $knownGameHash) { throw 'Supply the verified real Steam executable; never a hardlinked or synthetic game host.' }
$manifestPath = Join-Path $sourcePackage 'manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.packageId -ne 'de-online-prototype' -or $manifest.formatVersion -ne 1 -or $manifest.mode -ne 'Smoke' -or $manifest.gameSha256 -ne $knownGameHash -or
    @($manifest.files).Count -ne 58 -or @($manifest.files.path | Select-Object -Unique).Count -ne 58 -or @(Compare-Object $allPaths @($manifest.files.path)).Count) { throw 'Supply the complete real frontend58 Smoke candidate.' }
$actual = @(Get-ChildItem -LiteralPath (Join-Path $sourcePackage 'Win64') -File -Recurse | ForEach-Object { [IO.Path]::GetRelativePath((Join-Path $sourcePackage 'Win64'), $_.FullName).Replace('\', '/') })
if (@(Compare-Object $allPaths $actual).Count) { throw 'Candidate has missing or unrecorded payload files.' }
$packagePins = @([ordered]@{path='manifest.json';sha256=(Get-Sha $manifestPath)})
foreach ($entry in $manifest.files) {
    $path = Join-Path $sourcePackage ('Win64/' + $entry.path)
    Assert-NoLinks $path
    if ((Get-Sha $path) -ne $entry.sha256) { throw "Candidate hash differs: $($entry.path)" }
    $packagePins += [ordered]@{path=('Win64/' + $entry.path);sha256=$entry.sha256}
}
$candidateConfig = [IO.File]::ReadAllText((Join-Path $sourcePackage ('Win64/' + $configRelative)))
if ([regex]::Matches($candidateConfig, "export const $newMarker = true;").Count -ne 1 -or
    $candidateConfig -match 'export const (GAIT_REFERENCE|GAIT_RATE|GAIT_ROOT_LOCK|INPUT_REFERENCE|MTA_NATIVE_PAD_REFERENCE|MTA_NATIVE_MOVEMENT_TRIAL|MTA_CORE_STARTUP_REFERENCE|MTA_CORE_INPUT_REFERENCE|MTA_CORE_MOVEMENT_REFERENCE|MTA_CORE_RENDER_REFERENCE|MTA_CORE_RENDER_GPU_REFERENCE|MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE|MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE|MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE|MTA_CORE_GUI_STARTUP_REFERENCE|MTA_CORE_GUI_SURFACE_REFERENCE|MTA_CORE_GUI_FRONTEND_REFERENCE) = true;') { throw 'Candidate must arm only the new frontend input mode.' }
$sourcePaths = @($scripts | ForEach-Object { 'Client/definitive/' + $_ }) + @('Client/launch/DefinitivePreflight/Loader.cpp', 'Tests/definitive/test_frontend_input_package.ps1')
if ($preflightEnabled) {
    if (-not $PreflightDll) { $PreflightDll = Join-Path $repositoryRoot 'Bin/mta-launcher/x64/mta/de_preflight.dll' }
    if (-not $LauncherExe) { $LauncherExe = Join-Path $repositoryRoot 'Bin/mta-launcher/x64/Multi Theft Auto.exe' }
    $PreflightDll = (Resolve-Path -LiteralPath $PreflightDll).Path
    $LauncherExe = (Resolve-Path -LiteralPath $LauncherExe).Path
    foreach ($spec in @(@($LauncherExe, 'launcher/Multi Theft Auto.exe'), @($PreflightDll, 'launcher/mta/de_preflight.dll'))) {
        Assert-NoLinks $spec[0]
        $preflightArtifacts += [ordered]@{source=$spec[0];fixture=(Join-Path $fixtureRoot $spec[1]);sha256=(Get-Sha $spec[0])}
    }
    $sourcePaths += @('Client/launch/Main.cpp', 'Client/launch/DefinitivePreflight/Preflight.h', 'Client/launch/DefinitivePreflight/StrictJson.c')
}
$sourcePins = @($sourcePaths | ForEach-Object { [ordered]@{path=$_;sha256=(Get-Sha (Join-Path $repositoryRoot $_))} })
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
foreach ($name in $scripts) { Copy-Fixture (Join-Path $repositoryRoot ('Client/definitive/' + $name)) (Join-Path $fixtureScripts $name) }
foreach ($entry in $packagePins) { Copy-Fixture (Join-Path $sourcePackage $entry.path) (Join-Path $fixturePackage $entry.path) }
foreach ($entry in $preflightArtifacts) { Copy-Fixture $entry.source $entry.fixture }
# A real copy has a different file identity; fixture mutation can never modify
# the installed executable through a hard link. It is never executed here.
Copy-Fixture $sourceExecutable (Join-Path $fixtureGame 'SanAndreas.exe')
$originalPackageManifest = [IO.File]::ReadAllText((Join-Path $fixturePackage 'manifest.json'))
$success = $false
$failure = $null
try {
    Invoke-Preflight 'valid-frontend-input58' 0 'preflight_valid'
    # Legacy ownership accepts packages predating the optional marker. These
    # records contain verified actual DLL bytes, not fabricated PE fixtures.
    $legacyConfig = [regex]::Replace($candidateConfig, "(?m)^export const $newMarker = true;\r?\n", '')
    $installedFiles = @()
    foreach ($relative in $basePaths) {
        $destination = Join-Path $fixtureGame $relative
        Copy-Fixture (Join-Path $fixturePackage ('Win64/' + $relative)) $destination
        if ($relative -eq $configRelative) { Write-Fixture $destination ($utf8.GetBytes($legacyConfig)) }
        $installedFiles += [ordered]@{path=$relative;destination=$destination;sha256=(Get-Sha $destination)}
    }
    $installed = [ordered]@{packageId='de-online-prototype';formatVersion=1;mode='Smoke';gameDirectory=$fixtureGame;gameSha256=$knownGameHash;files=$installedFiles}
    Write-FixtureJson (Join-Path $fixtureGame 'de-online-install-manifest.json') $installed
    Assert-Installed 23
    foreach ($relative in $guiPaths) {
        $destination = Join-Path $fixtureGame $relative
        Copy-Fixture (Join-Path $fixturePackage ('Win64/' + $relative)) $destination
        $installed.files += [ordered]@{path=$relative;destination=$destination;sha256=(Get-Sha $destination)}
    }
    Write-FixtureJson (Join-Path $fixtureGame 'de-online-install-manifest.json') $installed
    Assert-Installed 36
    Invoke-Production 'update.ps1' @{GameDirectory=$fixtureGame;PackageDirectory=$fixturePackage}
    Assert-Installed 58
    Assert-Check ([IO.File]::ReadAllText((Join-Path $fixtureGame $configRelative)) -eq $candidateConfig) 'Upgrade selects exact candidate mode'
    foreach ($selection in @('plain', 'frontend', 'input', 'startup', 'surface', 'input')) {
        $arguments = @{Mode='Smoke';GameDirectory=$fixtureGame}
        switch ($selection) {
            'frontend' { $arguments.MtaCoreGuiFrontendReference=$true }
            'input' { $arguments.MtaCoreGuiFrontendInputReference=$true }
            'startup' { $arguments.MtaCoreGuiStartupReference=$true }
            'surface' { $arguments.MtaCoreGuiSurfaceReference=$true }
        }
        Invoke-Production 'set-mode.ps1' $arguments
        $text = [IO.File]::ReadAllText((Join-Path $fixtureGame $configRelative))
        $armed = @([regex]::Matches($text, 'export const MTA_CORE_GUI_(STARTUP|SURFACE|FRONTEND|FRONTEND_INPUT)_REFERENCE = true;'))
        Assert-Check ($armed.Count -eq $(if ($selection -eq 'plain') { 0 } else { 1 })) ('Exclusive GUI selection ' + $selection)
        Assert-Check ($text.Contains("export const $newMarker = $($($selection -eq 'input').ToString().ToLowerInvariant());")) ('Input marker selection ' + $selection)
    }
    Assert-Installed 58
    $conflictSwitches = @('NativeGaitReference', 'MatchedGaitRate', 'LockWalkRoot', 'NativeInputReference', 'MtaNativePadReference',
        'MtaNativeMovementTrial', 'MtaCoreStartupReference', 'MtaCoreInputReference', 'MtaCoreMovementReference', 'MtaCoreRenderReference',
        'MtaCoreRenderGpuReference', 'MtaCoreRenderCustomPresentReference', 'MtaCoreRenderFrameBoundaryReference',
        'MtaCoreRenderNativeDispatchReference', 'MtaCoreGuiStartupReference', 'MtaCoreGuiSurfaceReference', 'MtaCoreGuiFrontendReference')
    foreach ($name in @('package.ps1', 'set-mode.ps1')) {
        foreach ($conflict in $conflictSwitches + @('route', 'Online', 'Diagnostic')) {
            $arguments = @{Mode='Smoke';MtaCoreGuiFrontendInputReference=$true}
            if ($name -eq 'set-mode.ps1') { $arguments.GameDirectory=$fixtureGame }
            if ($conflict -eq 'route') { $arguments.MtaCoreRenderRoute='D3D12' }
            elseif ($conflict -in @('Online', 'Diagnostic')) { $arguments.Mode=$conflict }
            else { $arguments[$conflict]=$true }
            Assert-Refused { Invoke-Production $name $arguments } "$name $conflict" 'frontend input requires separate'
        }
    }
    $configConflicts = @('MTA_CORE_GUI_FRONTEND_REFERENCE', 'MTA_CORE_GUI_STARTUP_REFERENCE', 'MTA_CORE_GUI_SURFACE_REFERENCE',
        'MTA_CORE_MOVEMENT_REFERENCE', 'MTA_CORE_INPUT_REFERENCE', 'MTA_CORE_STARTUP_REFERENCE', 'MTA_CORE_RENDER_REFERENCE',
        'MTA_CORE_RENDER_GPU_REFERENCE', 'MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE', 'MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE',
        'MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE', 'MTA_NATIVE_MOVEMENT_TRIAL', 'MTA_NATIVE_PAD_REFERENCE', 'INPUT_REFERENCE',
        'GAIT_REFERENCE', 'GAIT_RATE', 'GAIT_ROOT_LOCK')
    foreach ($conflict in $configConflicts + @('duplicate', 'malformed')) {
        $text = if ($conflict -eq 'duplicate') { $candidateConfig + "`nexport const $newMarker = true;`n" }
                elseif ($conflict -eq 'malformed') { $candidateConfig.Replace("export const $newMarker = true;", "export const $newMarker = 1;") }
                else { $candidateConfig.Replace("export const $conflict = false;", "export const $conflict = true;") }
        Assert-Check ($text -ne $candidateConfig) ('Conflict marker exists ' + $conflict)
        Set-PackageConfig $text
        foreach ($name in @('install.ps1', 'update.ps1')) {
            $arguments = @{GameDirectory=$fixtureGame;PackageDirectory=$fixturePackage;WhatIf=$true}
            if ($name -eq 'install.ps1') { $arguments.Mode='Smoke' }
            Assert-Refused { Invoke-Production $name $arguments } "$name config $conflict" 'GUI|frontend'
        }
        Invoke-Preflight ("config-$conflict") 4 $(if ($conflict -in @('duplicate', 'malformed')) { 'gui_configuration_marker_invalid' } else { 'gui_mode_conflict' })
    }
    Set-PackageConfig $candidateConfig
    Invoke-Preflight 'restored-frontend-input58' 0 'preflight_valid'
    foreach ($missing in @($guiPaths[0], $frontendPaths[-1])) {
        $modified = $originalPackageManifest | ConvertFrom-Json
        $modified.files = @($modified.files | Where-Object path -ne $missing)
        Write-FixtureJson (Join-Path $fixturePackage 'manifest.json') $modified
        Assert-Refused { Invoke-Production 'update.ps1' @{GameDirectory=$fixtureGame;PackageDirectory=$fixturePackage;WhatIf=$true} } ('Partial group ' + $missing) 'ownership|owned'
    }
    $modified = $originalPackageManifest | ConvertFrom-Json
    $modified.files[0].path = '../escape.dll'
    Write-FixtureJson (Join-Path $fixturePackage 'manifest.json') $modified
    Assert-Refused { Invoke-Production 'update.ps1' @{GameDirectory=$fixtureGame;PackageDirectory=$fixturePackage;WhatIf=$true} } 'Escaping candidate path' 'ownership|owned'
    Write-Fixture (Join-Path $fixturePackage 'manifest.json') ($utf8.GetBytes($originalPackageManifest))
    $corruptPath = Join-Path $fixturePackage 'Win64/CLEO/de-online/cgui.dll'
    $original = [IO.File]::ReadAllBytes($corruptPath)
    $changed = [byte[]]$original.Clone(); $changed[0] = $changed[0] -bxor 1
    Write-Fixture $corruptPath $changed
    Assert-Refused { Invoke-Production 'update.ps1' @{GameDirectory=$fixtureGame;PackageDirectory=$fixturePackage;WhatIf=$true} } 'Candidate hash mismatch' 'hash|differs'
    Write-Fixture $corruptPath $original
    $installedPath = Join-Path $fixtureGame 'de-online-install-manifest.json'
    $installedBytes = [IO.File]::ReadAllBytes($installedPath)
    $modified = Read-Installed
    $modified.files[0].destination = Join-Path $outputRoot 'foreign.dll'
    Write-FixtureJson $installedPath $modified
    Assert-Refused { Invoke-Production 'update.ps1' @{GameDirectory=$fixtureGame;PackageDirectory=$fixturePackage;WhatIf=$true} } 'Foreign installed destination' 'destination|hash'
    Write-Fixture $installedPath $installedBytes
    Invoke-Production 'update.ps1' @{GameDirectory=$fixtureGame;PackageDirectory=$fixturePackage;WhatIf=$true}
    Assert-Installed 58
    Assert-Check ((Get-Sha (Join-Path $fixtureGame 'SanAndreas.exe')) -eq $knownGameHash) 'Copied executable unchanged and never launched'
    foreach ($pin in $sourcePins) { Assert-Check ((Get-Sha (Join-Path $repositoryRoot $pin.path)) -eq $pin.sha256) ('Source unchanged ' + $pin.path) }
    foreach ($pin in $packagePins) { Assert-Check ((Get-Sha (Join-Path $sourcePackage $pin.path)) -eq $pin.sha256) ('Candidate unchanged ' + $pin.path) }
    foreach ($pin in $preflightArtifacts) {
        Assert-Check ((Get-Sha $pin.source) -eq $pin.sha256 -and (Get-Sha $pin.fixture) -eq $pin.sha256) ('Preflight artifact unchanged ' + $pin.source)
    }
    $success = $true
} catch { $failure = $_.Exception.Message }
$result = [ordered]@{schema='mta_frontend_input_package_tests_v1';success=$success;scope='frontend-input-only-one-copied-executable';
    output_directory=$outputRoot;candidate=$sourcePackage;game_executable_sha256=$knownGameHash;checks=$checks.Count;check_labels=$checks;
    source_hashes=$sourcePins;package_hashes=$packagePins;legacy_ownership_counts=@(23,36);final_ownership_count=58;error=$failure;
    preflight_exercised=[bool]$preflightEnabled;preflight_artifacts=$preflightArtifacts;preflight_cases=$preflightCases;
    note='Bounded production ownership/mode validation; no GTA launch, installed game writes, broad historical gait rerun or normal runtime build.'}
Write-FixtureJson (Join-Path $outputRoot 'test-result.json') $result
Write-Host ("Frontend input package checks: $($checks.Count); success=$success; receipt=" + (Join-Path $outputRoot 'test-result.json'))
if (-not $success) { throw $failure }
# Expected negative native CLI cases must not leak their child exit status to
# callers after the complete ownership/preflight suite has succeeded.
$global:LASTEXITCODE = 0
