#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise real ownership transitions against an owned, never-executed game copy.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ClockPackageDirectory,
    [Parameter(Mandatory)][string]$PlainPackageDirectory,
    [Parameter(Mandatory)][string]$PersistentPackageDirectory,
    [Parameter(Mandatory)][string]$GameExecutable
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $repositoryRoot ('Build/mta-clock-package/' + $id)
$fakeGame = Join-Path $outputRoot 'owned-fake-game'
$fixtureRoot = Join-Path $outputRoot 'package-fixtures'
$utf8 = [Text.UTF8Encoding]::new($false)
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$clockPattern = 'export const MTA_CORE_CLOCK_REFERENCE = (true|false);'
$checks = [Collections.Generic.List[object]]::new()
$runs = [Collections.Generic.List[object]]::new()
$backups = [Collections.Generic.List[object]]::new()
$sourcePins = [ordered]@{}
$inputPins = [Collections.Generic.List[object]]::new()
$failure = $null
$sourcesUnchanged = $false
$fakeRemoved = $false
New-Item -ItemType Directory -Path $outputRoot,$fixtureRoot -Force | Out-Null
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Save([string]$Path,$Value) {
    [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 18) + [Environment]::NewLine,$utf8)
}
function Check([string]$Name,[bool]$Passed,$Detail = $null) {
    $checks.Add([ordered]@{name=$Name;passed=$Passed;detail=$Detail})
    if (-not $Passed) { throw ('Check failed: ' + $Name) }
}
function Assert-Owned([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($outputRoot) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture operation escaped this test directory.' }
    $candidate = $resolved
    while ($candidate) {
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Fixture path contains a link.'
        }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
    $resolved
}
function Tree([string]$Directory) {
    $rows = @(Get-ChildItem -LiteralPath $Directory -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($Directory,$_.FullName).Replace('\','/')
        if ($_.PSIsContainer) { [ordered]@{path=$relative;kind='directory'} }
        else { [ordered]@{path=$relative;kind='file';bytes=$_.Length;sha256=(Sha $_.FullName)} }
    })
    # Directory records catch refusals that accidentally create empty parents.
    [ordered]@{rows=$rows;canonical=($rows | ConvertTo-Json -Depth 5 -Compress)}
}
function Run([string]$Name,[string]$Script,[string[]]$Arguments,[bool]$ExpectSuccess) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = (Get-Process -Id $PID).Path
    $start.WorkingDirectory = $repositoryRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in (@('-NoLogo','-NoProfile','-NonInteractive','-File',$Script) + $Arguments)) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw 'Could not start installer fixture.' }
        $outTask = $process.StandardOutput.ReadToEndAsync()
        $errorTask = $process.StandardError.ReadToEndAsync()
        $timeout = -not $process.WaitForExit(60000)
        if ($timeout) { $process.Kill($true); $process.WaitForExit() }
        $outPath = Join-Path $outputRoot ($Name + '.log')
        $errorPath = Join-Path $outputRoot ($Name + '-errors.log')
        [IO.File]::WriteAllText($outPath,$outTask.GetAwaiter().GetResult(),$utf8)
        [IO.File]::WriteAllText($errorPath,$errorTask.GetAwaiter().GetResult(),$utf8)
        $row = [ordered]@{name=$Name;script=$Script;scriptSha256=(Sha $Script);arguments=$Arguments;exitCode=$process.ExitCode;
            timedOut=$timeout;expectedSuccess=$ExpectSuccess;stdout=$outPath;stdoutSha256=(Sha $outPath);
            stderr=$errorPath;stderrSha256=(Sha $errorPath)}
        $runs.Add($row)
        Check ($Name + ':expected-result') (-not $timeout -and (($process.ExitCode -eq 0) -eq $ExpectSuccess)) $row
        $row
    } finally { $process.Dispose() }
}
function Refuse([string]$Name,[string]$Script,[string[]]$Arguments) {
    $before = Tree $fakeGame
    Run $Name $Script $Arguments $false | Out-Null
    $after = Tree $fakeGame
    Check ($Name + ':tree-unchanged') ($before.canonical -ceq $after.canonical)
    Save (Join-Path $outputRoot ($Name + '-tree-proof.json')) ([ordered]@{before=$before.rows;after=$after.rows;unchanged=$true})
}
function Fresh-Game {
    $owned = Assert-Owned $fakeGame
    if (Test-Path -LiteralPath $owned) { Remove-Item -LiteralPath $owned -Recurse -Force }
    New-Item -ItemType Directory -Path $owned | Out-Null
    [IO.File]::Copy($GameExecutable,(Join-Path $owned 'SanAndreas.exe'),$false)
    Check 'fake-executable-exact-fingerprint' ((Sha (Join-Path $owned 'SanAndreas.exe')) -ceq $knownGameHash)
}
function Package-Info([string]$Directory,[int]$Count,[string]$Name) {
    $resolved = (Resolve-Path -LiteralPath $Directory).Path
    $manifestPath = Join-Path $resolved 'manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    Check ($Name + ':identity') ($manifest.packageId -ceq 'de-online-prototype' -and $manifest.formatVersion -eq 1 -and
        $manifest.mode -ceq 'Smoke' -and $manifest.gameSha256 -ceq $knownGameHash)
    Check ($Name + ':owned-count') (@($manifest.files).Count -eq $Count -and @($manifest.files.path | Select-Object -Unique).Count -eq $Count)
    $sourceRoot = Join-Path $resolved 'Win64'
    $actual = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | ForEach-Object {[IO.Path]::GetRelativePath($sourceRoot,$_.FullName).Replace('\','/')})
    Check ($Name + ':exact-tree') (@(Compare-Object @($manifest.files.path) $actual).Count -eq 0)
    $rows = @()
    foreach ($entry in $manifest.files) {
        $path = Join-Path $sourceRoot $entry.path
        $hash = Sha $path
        Check ($Name + ':hash:' + $entry.path) ($hash -ceq $entry.sha256)
        $rows += [ordered]@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;sha256=$hash}
    }
    $inputPins.Add([ordered]@{name=$Name;directory=$resolved;manifest=$manifestPath;manifestSha256=(Sha $manifestPath);files=$rows})
    [ordered]@{root=$resolved;manifest=$manifest}
}
function Installed([int]$Count,$Package,[string]$Name) {
    $manifestPath = Join-Path $fakeGame 'de-online-install-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    Check ($Name + ':identity') ($manifest.packageId -ceq 'de-online-prototype' -and $manifest.formatVersion -eq 1 -and
        $manifest.gameDirectory -ceq $fakeGame -and $manifest.gameSha256 -ceq $knownGameHash -and $manifest.mode -ceq 'Smoke')
    Check ($Name + ':count') (@($manifest.files).Count -eq $Count -and @($manifest.files.path | Select-Object -Unique).Count -eq $Count)
    if ($null -ne $Package) { Check ($Name + ':exact-owned-paths') (@(Compare-Object @($manifest.files.path) @($Package.files.path)).Count -eq 0) }
    foreach ($entry in $manifest.files) {
        $path = [IO.Path]::GetFullPath((Join-Path $fakeGame $entry.path))
        Check ($Name + ':destination:' + $entry.path) ($entry.destination -ceq $path)
        Check ($Name + ':hash:' + $entry.path) ((Sha $path) -ceq $entry.sha256)
        if ($null -ne $Package) {
            $expected = @($Package.files | Where-Object path -CEQ $entry.path)
            Check ($Name + ':package-hash:' + $entry.path) ($expected.Count -eq 1 -and $entry.sha256 -ceq $expected[0].sha256)
        }
    }
    Save (Join-Path $outputRoot ($Name + '-installed-manifest.json')) $manifest
    $manifest
}
function Variant([string]$Name,[string]$Source,[scriptblock]$Change) {
    $target = Assert-Owned (Join-Path $fixtureRoot $Name)
    New-Item -ItemType Directory -Path $target | Out-Null
    Copy-Item -LiteralPath (Join-Path $Source 'manifest.json') -Destination $target
    Copy-Item -LiteralPath (Join-Path $Source 'Win64') -Destination $target -Recurse
    $config = Join-Path $target 'Win64/CLEO/de-online/config.mjs'
    $text = & $Change ([IO.File]::ReadAllText($config))
    [IO.File]::WriteAllText($config,$text,$utf8)
    $manifestPath = Join-Path $target 'manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    @($manifest.files | Where-Object path -CEQ 'CLEO/de-online/config.mjs')[0].sha256 = Sha $config
    Save $manifestPath $manifest
    $target
}
function Mutate-InstalledConfig([string]$Text) {
    $configPath = Join-Path $fakeGame 'CLEO/de-online/config.mjs'
    [IO.File]::WriteAllText($configPath,$Text,$utf8)
    $manifestPath = Join-Path $fakeGame 'de-online-install-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    @($manifest.files | Where-Object path -CEQ 'CLEO/de-online/config.mjs')[0].sha256 = Sha $configPath
    Save $manifestPath $manifest
}
function Marker([bool]$Enabled,[string]$Name) {
    $text = [IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs'))
    $matches = [regex]::Matches($text,$clockPattern)
    Check ($Name + ':clock-marker') ($matches.Count -eq 1 -and $matches[0].Groups[1].Value -ceq $Enabled.ToString().ToLowerInvariant())
}
function Backup-Directories {
    $root = Join-Path $repositoryRoot 'Build/definitive-update-backups'
    if (Test-Path -LiteralPath $root) { @(Get-ChildItem -LiteralPath $root -Directory | ForEach-Object FullName) } else { @() }
}
try {
    foreach ($name in @('Client/definitive/install.ps1','Client/definitive/update.ps1','Client/definitive/uninstall.ps1',
        'Client/definitive/set-mode.ps1','Client/definitive/package.ps1','Client/definitive/game-adapter/index.js',
        'Client/definitive/game-adapter/config.mjs','Tests/definitive/test_clock_package.ps1')) {
        $sourcePins[$name] = Sha (Join-Path $repositoryRoot $name)
    }
    $GameExecutable = (Resolve-Path -LiteralPath $GameExecutable).Path
    Check 'source-executable-exact-fingerprint' ((Sha $GameExecutable) -ceq $knownGameHash)
    $clock = Package-Info $ClockPackageDirectory 23 'clock-package'
    $plain = Package-Info $PlainPackageDirectory 23 'plain-package'
    $persistent = Package-Info $PersistentPackageDirectory 58 'persistent-package'
    foreach ($path in @('version.dll','CLEO/de-online/core.dll','CLEO/de-online/game_de.dll','CLEO/de-online/de_client_bridge.dll','CLEO/de-online/pthread.dll')) {
        $hashes = @(@($clock,$plain,$persistent) | ForEach-Object { @($_.manifest.files | Where-Object path -CEQ $path)[0].sha256 })
        Check ('shared-binary:' + $path) (@($hashes | Select-Object -Unique).Count -eq 1)
    }
    $clockText = [IO.File]::ReadAllText((Join-Path $clock.root 'Win64/CLEO/de-online/config.mjs'))
    $plainText = [IO.File]::ReadAllText((Join-Path $plain.root 'Win64/CLEO/de-online/config.mjs'))
    Check 'clock-input-armed' ($clockText -match 'export const MTA_CORE_CLOCK_REFERENCE = true;')
    Check 'plain-input-disarmed' ($plainText -match 'export const MTA_CORE_CLOCK_REFERENCE = false;')
    $install = Join-Path $repositoryRoot 'Client/definitive/install.ps1'
    $update = Join-Path $repositoryRoot 'Client/definitive/update.ps1'
    $setMode = Join-Path $repositoryRoot 'Client/definitive/set-mode.ps1'
    $uninstall = Join-Path $repositoryRoot 'Client/definitive/uninstall.ps1'
    $variants = [ordered]@{
        'duplicate-clock' = (Variant 'duplicate-clock' $clock.root {param($text) $text + [Environment]::NewLine + 'export const MTA_CORE_CLOCK_REFERENCE = true;' + [Environment]::NewLine})
        'nonboolean-clock' = (Variant 'nonboolean-clock' $clock.root {param($text) $text.Replace('export const MTA_CORE_CLOCK_REFERENCE = true;','export const MTA_CORE_CLOCK_REFERENCE = 1;')})
        'malformed-clock' = (Variant 'malformed-clock' $clock.root {param($text) $text.Replace('export const MTA_CORE_CLOCK_REFERENCE = true;','export const MTA_CORE_CLOCK_REFERENCE = "true";')})
        'mixed-base-input' = (Variant 'mixed-base-input' $clock.root {param($text) $text.Replace('export const MTA_CORE_INPUT_REFERENCE = false;','export const MTA_CORE_INPUT_REFERENCE = true;')})
        'mixed-base-gui' = (Variant 'mixed-base-gui' $clock.root {param($text) $text.Replace('export const MTA_CORE_GUI_STARTUP_REFERENCE = false;','export const MTA_CORE_GUI_STARTUP_REFERENCE = true;')})
        'gui-owned-clock' = (Variant 'gui-owned-clock' $persistent.root {param($text)
            $text.Replace('export const MTA_CORE_CLOCK_REFERENCE = false;','export const MTA_CORE_CLOCK_REFERENCE = true;').
                Replace('export const MTA_CORE_PERSISTENT_FRONTEND = true;','export const MTA_CORE_PERSISTENT_FRONTEND = false;')})
    }
    $unsafe = Variant 'unsafe-package-path' $clock.root {param($text) $text}
    $unsafeManifestPath = Join-Path $unsafe 'manifest.json'
    $unsafeManifest = Get-Content -LiteralPath $unsafeManifestPath -Raw | ConvertFrom-Json
    $unsafeManifest.files[0].path = '../clock-package-escape.txt'
    Save $unsafeManifestPath $unsafeManifest
    $variants['unsafe-package-path'] = $unsafe
    $older = Variant 'historical-no-clock-marker' $plain.root {param($text) [regex]::Replace($text,'(?m)^export const MTA_CORE_CLOCK_REFERENCE = false;\r?\n?','')}

    Fresh-Game
    foreach ($name in $variants.Keys) {
        Refuse ('install-reject-' + $name) $install @('-Mode','Smoke','-PackageDirectory',$variants[$name],'-GameDirectory',$fakeGame)
    }
    Run 'install-persistent-58' $install @('-Mode','Smoke','-PackageDirectory',$persistent.root,'-GameDirectory',$fakeGame) $true | Out-Null
    $prior = Installed 58 $persistent.manifest 'persistent-installed'
    $priorManifestPath = Join-Path $fakeGame 'de-online-install-manifest.json'
    $priorManifestSha = Sha $priorManifestPath
    $priorVersionHash = Sha (Join-Path $fakeGame 'version.dll')
    $unowned = Join-Path $fakeGame 'CLEO/user-kept.txt'
    [IO.File]::WriteAllText($unowned,'unowned test sentinel',$utf8)
    $unownedHash = Sha $unowned
    Refuse 'set-clock-refuses-58-ownership' $setMode @('-Mode','Smoke','-MtaCoreClockReference','-GameDirectory',$fakeGame)
    foreach ($name in $variants.Keys) {
        Refuse ('update-reject-' + $name) $update @('-PackageDirectory',$variants[$name],'-GameDirectory',$fakeGame)
    }
    $beforeBackups = @(Backup-Directories)
    Run 'update-persistent-to-clock' $update @('-PackageDirectory',$clock.root,'-GameDirectory',$fakeGame) $true | Out-Null
    $newBackups = @(Backup-Directories | Where-Object {$_ -notin $beforeBackups})
    Check 'transition-one-owned-backup' ($newBackups.Count -eq 1) $newBackups
    $backup = $newBackups[0]
    Check 'transition-backup-original-manifest' ((Sha (Join-Path $backup 'manifest.json')) -ceq $priorManifestSha)
    $now = Installed 23 $clock.manifest 'clock-installed'
    $removed = @($prior.files | Where-Object {$_.path -notin $now.files.path})
    Check 'transition-removes-exactly-35-gui-assets' ($removed.Count -eq 35 -and
        @($removed | Where-Object {$_.path -notmatch '^CLEO/de-online/(cgui\.dll|mta/cgui/|skins/Default/|mta/locale/en_US/)'}).Count -eq 0)
    foreach ($entry in $removed) { Check ('removed:' + $entry.path) (-not (Test-Path -LiteralPath $entry.destination)) }
    $changedPrior = @($prior.files | Where-Object {
        $previous = $_
        $replacement = @($now.files | Where-Object path -CEQ $previous.path)
        $replacement.Count -eq 0 -or $replacement[0].sha256 -cne $previous.sha256
    })
    $backupRows = @(Get-ChildItem -LiteralPath $backup -Filter '*.original' -File | ForEach-Object {
        [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Sha $_.FullName)}
    })
    Check 'transition-backs-every-changed-or-removed-row' ($backupRows.Count -eq $changedPrior.Count)
    Check 'transition-all-prior-mutated-bytes-recoverable' (@(Compare-Object @($changedPrior.sha256 | Sort-Object) @($backupRows.sha256 | Sort-Object)).Count -eq 0)
    $backups.Add([ordered]@{directory=$backup;originalManifestSha256=$priorManifestSha;changedPriorRows=$changedPrior;
        savedOriginalRows=$backupRows;unchangedPriorRows=@($prior.files | Where-Object {$_.path -notin $changedPrior.path});
        scope='Original manifest plus every replaced/removed row; other prior bytes remain unchanged in fake game.'})
    Check 'transition-preserves-version-loader' ((Sha (Join-Path $fakeGame 'version.dll')) -ceq $priorVersionHash)
    Check 'transition-preserves-unowned-file' ((Sha $unowned) -ceq $unownedHash)
    Check 'transition-preserves-game-copy' ((Sha (Join-Path $fakeGame 'SanAndreas.exe')) -ceq $knownGameHash)
    Marker $true 'updated-clock'
    Run 'ordinary-smoke-clears-clock' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
    Marker $false 'ordinary-smoke'
    Installed 23 $null 'ordinary-smoke-installed' | Out-Null
    Run 'explicit-clock-reenables' $setMode @('-Mode','Smoke','-MtaCoreClockReference','-GameDirectory',$fakeGame) $true | Out-Null
    Marker $true 'explicit-clock'
    Refuse 'set-clock-mixed-input' $setMode @('-Mode','Smoke','-MtaCoreClockReference','-MtaCoreInputReference','-GameDirectory',$fakeGame)
    Refuse 'set-clock-mixed-render-route' $setMode @('-Mode','Smoke','-MtaCoreClockReference','-MtaCoreRenderRoute','D3D12','-GameDirectory',$fakeGame)
    Refuse 'set-clock-online-refused' $setMode @('-Mode','Online','-MtaCoreClockReference','-GameDirectory',$fakeGame)
    $validConfig = [IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs'))
    $validManifest = [IO.File]::ReadAllBytes((Join-Path $fakeGame 'de-online-install-manifest.json'))
    foreach ($name in @('duplicate-clock','nonboolean-clock','malformed-clock')) {
        Mutate-InstalledConfig ([IO.File]::ReadAllText((Join-Path $variants[$name] 'Win64/CLEO/de-online/config.mjs')))
        Refuse ('set-reject-installed-' + $name) $setMode @('-Mode','Smoke','-MtaCoreClockReference','-GameDirectory',$fakeGame)
        Refuse ('ordinary-reject-installed-' + $name) $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame)
        [IO.File]::WriteAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs'),$validConfig,$utf8)
        [IO.File]::WriteAllBytes((Join-Path $fakeGame 'de-online-install-manifest.json'),$validManifest)
    }
    $unsafeInstalledPath = Join-Path $fakeGame 'de-online-install-manifest.json'
    $unsafeInstalled = Get-Content -LiteralPath $unsafeInstalledPath -Raw | ConvertFrom-Json
    $unsafeInstalled.files[0].destination = Join-Path $outputRoot 'outside-owned-game.txt'
    Save $unsafeInstalledPath $unsafeInstalled
    Refuse 'set-clock-unsafe-installed-ownership' $setMode @('-Mode','Smoke','-MtaCoreClockReference','-GameDirectory',$fakeGame)
    Refuse 'update-clock-unsafe-installed-ownership' $update @('-PackageDirectory',$clock.root,'-GameDirectory',$fakeGame)
    [IO.File]::WriteAllBytes($unsafeInstalledPath,$validManifest)
    Check 'all-mode-changes-preserve-version-loader' ((Sha (Join-Path $fakeGame 'version.dll')) -ceq $priorVersionHash)

    # An absent historical declaration permits old ordinary modes, never a new
    # native service. Only an explicit reviewed package update grants the marker.
    Fresh-Game
    Run 'install-historical-no-marker' $install @('-Mode','Smoke','-PackageDirectory',$older,'-GameDirectory',$fakeGame) $true | Out-Null
    Run 'historical-ordinary-smoke-compatible' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
    Check 'historical-marker-remains-absent' ([regex]::Matches([IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs')),'export const MTA_CORE_CLOCK_REFERENCE\b').Count -eq 0)
    Refuse 'historical-clock-refused-before-update' $setMode @('-Mode','Smoke','-MtaCoreClockReference','-GameDirectory',$fakeGame)
    $historicalManifestHash = Sha (Join-Path $fakeGame 'de-online-install-manifest.json')
    $historicalConfigHash = Sha (Join-Path $fakeGame 'CLEO/de-online/config.mjs')
    $historicalVersionHash = Sha (Join-Path $fakeGame 'version.dll')
    $beforeHistoricalBackups = @(Backup-Directories)
    Run 'historical-update-clock' $update @('-PackageDirectory',$clock.root,'-GameDirectory',$fakeGame) $true | Out-Null
    $historicalBackups = @(Backup-Directories | Where-Object {$_ -notin $beforeHistoricalBackups})
    Check 'historical-update-one-owned-backup' ($historicalBackups.Count -eq 1)
    $historicalBackup = $historicalBackups[0]
    Check 'historical-update-original-manifest-saved' ((Sha (Join-Path $historicalBackup 'manifest.json')) -ceq $historicalManifestHash)
    $historicalOriginalRows = @(Get-ChildItem -LiteralPath $historicalBackup -Filter '*.original' -File | ForEach-Object {
        [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Sha $_.FullName)}
    })
    Check 'historical-update-original-config-saved' ($historicalOriginalRows.Count -eq 1 -and $historicalOriginalRows[0].sha256 -ceq $historicalConfigHash)
    Check 'historical-update-preserves-version-loader' ((Sha (Join-Path $fakeGame 'version.dll')) -ceq $historicalVersionHash)
    $backups.Add([ordered]@{directory=$historicalBackup;originalManifestSha256=$historicalManifestHash;savedOriginalRows=$historicalOriginalRows;
        scope='Historical declaration-free config replacement; every other owned package byte remains unchanged.'})
    Marker $true 'historical-after-update'
    Installed 23 $clock.manifest 'historical-updated-installed' | Out-Null
    Run 'historical-updated-enable-clock' $setMode @('-Mode','Smoke','-MtaCoreClockReference','-GameDirectory',$fakeGame) $true | Out-Null
    Run 'final-owned-validation' $uninstall @('-ValidateOnly','-GameDirectory',$fakeGame) $true | Out-Null
    Save (Join-Path $outputRoot 'final-fake-game-tree.json') (Tree $fakeGame).rows
    $sourcesUnchanged = $true
    foreach ($name in $sourcePins.Keys) {
        if ((Sha (Join-Path $repositoryRoot $name)) -cne $sourcePins[$name]) { $sourcesUnchanged = $false }
    }
    Check 'production-and-test-sources-unchanged' $sourcesUnchanged
    foreach ($pin in $inputPins) {
        Check ($pin.name + ':manifest-unchanged') ((Sha $pin.manifest) -ceq $pin.manifestSha256)
        foreach ($file in $pin.files) { Check ($pin.name + ':input-unchanged:' + $file.path) ((Sha $file.path) -ceq $file.sha256) }
    }
} catch {
    $failure = $_.Exception.Message
} finally {
    # Remove only our uniquely created fake game. Keep logs, mutated package
    # fixtures and updater recovery copies as durable, independently readable evidence.
    if (Test-Path -LiteralPath $fakeGame) {
        $owned = Assert-Owned $fakeGame
        Remove-Item -LiteralPath $owned -Recurse -Force
    }
    $fakeRemoved = -not (Test-Path -LiteralPath $fakeGame)
}
$record = [ordered]@{
    schema='mta_clock_package_transition_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot
    success=($null -eq $failure -and $sourcesUnchanged -and $fakeRemoved);failure=$failure
    checks=$checks.Count;cases=$checks.ToArray();runs=$runs.ToArray();sourcePins=$sourcePins;sourcesUnchanged=$sourcesUnchanged
    inputPackages=$inputPins.ToArray();ownedUpdateBackups=$backups.ToArray();fakeGameDirectory=$fakeGame;fakeGameRemoved=$fakeRemoved
    copiedGameExecutable=$GameExecutable;sourceGameSha256=$knownGameHash;fakeExecutableExecuted=$false
    liveInstallationChanged=$false;liveGameLaunched=$false;nativeClockWritten=$false;fullMtaReady=$false
    scope='Real installer/updater/set-mode scripts against one owned fake game tree; no game executable or package DLL is executed.'
}
$resultPath = Join-Path $outputRoot 'test-result.json'
Save $resultPath $record
[ordered]@{success=$record.success;checks=$record.checks;path=$resultPath;sha256=(Sha $resultPath);failure=$failure} | ConvertTo-Json -Depth 4
if (-not $record.success) { exit 1 }
