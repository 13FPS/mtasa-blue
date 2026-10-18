#!/usr/bin/env pwsh
#requires -Version 7.0
# Validate completed-owner Clock then frontend ownership against one never-executed game copy.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ClockFrontendPackageDirectory,
    [Parameter(Mandatory)][string]$PlainPackageDirectory,
    [Parameter(Mandatory)][string]$SetPackageDirectory,
    [Parameter(Mandatory)][string]$GameExecutable
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $repositoryRoot ('Build/mta-frontend-clock-package/' + $id)
$scratchParent = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'Build/tmp'))
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $scratchParent ('frontend-clock-package-' + $id)))
$fakeGame = Join-Path $scratchRoot 'owned-fake-game'
$fixtureRoot = Join-Path $scratchRoot 'package-fixtures'
$utf8 = [Text.UTF8Encoding]::new($false)
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$clockPattern = 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = (true|false);'
$checks = [Collections.Generic.List[object]]::new()
$runs = [Collections.Generic.List[object]]::new()
$backups = [Collections.Generic.List[object]]::new()
$sourcePins = [ordered]@{}
$inputPins = [Collections.Generic.List[object]]::new()
$failure = $null
$sourcesUnchanged = $false
$fakeRemoved = $false
New-Item -ItemType Directory -Path $outputRoot,$fixtureRoot -Force | Out-Null
$savedTemp = [Environment]::GetEnvironmentVariable('TEMP','Process')
$savedTmp = [Environment]::GetEnvironmentVariable('TMP','Process')
$env:TEMP = $scratchRoot
$env:TMP = $scratchRoot
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
    $prefix = [IO.Path]::GetFullPath($scratchRoot) + [IO.Path]::DirectorySeparatorChar
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
function Input-NoLinks([string]$Path) {
    $candidate=[IO.Path]::GetFullPath($Path)
    while($candidate) {
        if((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {throw 'Linked package input refused.'}
        $candidate=[IO.Path]::GetDirectoryName($candidate)
    }
}
function Get-Amd64Exports([string]$Path) {
    # A new opt-in must reject stale DLLs before staging or downloading. Read
    # their PE export tables without executing Core, bridge or their DllMain.
    Input-NoLinks $Path
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

function Transition([string]$Name,$Selection) {
    $prior = Installed $(if ((Get-Content -Raw -LiteralPath (Join-Path $fakeGame 'de-online-install-manifest.json') | ConvertFrom-Json).files.Count -eq 58) {58} else {23}) $null ($Name+'-before')
    $priorManifest = Sha (Join-Path $fakeGame 'de-online-install-manifest.json')
    $changes = @()
    foreach ($old in $prior.files) {
        if ($old.path -notin $Selection.manifest.files.path) { $changes += [ordered]@{path=$old.path;original=$true;sha256=$old.sha256} }
    }
    foreach ($new in $Selection.manifest.files) {
        $old = @($prior.files | Where-Object {$_.path -ceq $new.path})
        if (-not $old.Count) { $changes += [ordered]@{path=$new.path;original=$false;sha256=$null} }
        elseif ($old[0].sha256 -cne $new.sha256) { $changes += [ordered]@{path=$new.path;original=$true;sha256=$old[0].sha256} }
    }
    $beforeBackups = @(Backup-Directories)
    Run $Name $update @('-PackageDirectory',$Selection.root,'-GameDirectory',$fakeGame) $true | Out-Null
    $installed = Installed $Selection.manifest.files.Count $Selection.manifest ($Name+'-after')
    $newBackups = @(Backup-Directories | Where-Object {$_ -notin $beforeBackups})
    Check ($Name+':one-new-backup') ($newBackups.Count -eq 1)
    $backup = $newBackups[0]
    Check ($Name+':original-manifest-backup') ((Sha (Join-Path $backup 'manifest.json')) -ceq $priorManifest)
    $savedOriginals = @()
    for ($index=0; $index -lt $changes.Count; ++$index) {
        $original = Join-Path $backup ($index.ToString()+'.original')
        if ($changes[$index].original) {
            Check ($Name+':exact-ordered-original:'+ $changes[$index].path) ((Test-Path -LiteralPath $original) -and (Sha $original) -ceq $changes[$index].sha256)
            $savedOriginals += [ordered]@{ownedPath=$changes[$index].path;path=$original;sha256=(Sha $original);bytes=(Get-Item -LiteralPath $original).Length}
        } else { Check ($Name+':addition-has-no-original:'+ $changes[$index].path) (-not (Test-Path -LiteralPath $original)) }
    }
    Check ($Name+':exact-backup-file-count') (@(Get-ChildItem -LiteralPath $backup -File -Recurse).Count -eq $savedOriginals.Count+1)
    $removed = @($prior.files | Where-Object {$_.path -notin $installed.files.path})
    $added = @($installed.files | Where-Object {$_.path -notin $prior.files.path})
    Check ($Name+':complete-GUI-group-transition') (($removed.Count -eq 0 -and $added.Count -eq 35) -or ($removed.Count -eq 35 -and $added.Count -eq 0))
    foreach ($row in @($removed)+@($added)) {
        Check ($Name+':GUI-owned-path:'+ $row.path) ($row.path -match '^CLEO/de-online/(cgui\.dll|mta/cgui/|skins/Default/|mta/locale/en_US/)')
    }
    foreach ($row in $removed) { Check ($Name+':removed:'+ $row.path) (-not (Test-Path -LiteralPath $row.destination)) }
    foreach ($old in $prior.files) {
        if ($old.path -notin $changes.path) { Check ($Name+':unchanged-owned:'+ $old.path) ((Sha $old.destination) -ceq $old.sha256) }
    }
    Check ($Name+':loader-preserved') ((Sha (Join-Path $fakeGame 'version.dll')) -ceq $loaderHash)
    Check ($Name+':unowned-preserved') ((Sha $unowned) -ceq $unownedHash)
    Check ($Name+':game-preserved') ((Sha (Join-Path $fakeGame 'SanAndreas.exe')) -ceq $knownGameHash)
    $backups.Add([ordered]@{name=$Name;directory=$backup;originalManifestSha256=$priorManifest;orderedChanges=$changes;
        savedOriginals=$savedOriginals;removedPaths=@($removed | ForEach-Object path);addedPaths=@($added | ForEach-Object path)})
}
try {
    foreach ($name in @('Client/definitive/install.ps1','Client/definitive/update.ps1','Client/definitive/uninstall.ps1',
        'Client/definitive/set-mode.ps1','Client/definitive/package.ps1','Client/definitive/game-adapter/index.js',
        'Client/definitive/game-adapter/config.mjs','Tests/definitive/test_frontend_clock_package.ps1')) {
        $sourcePins[$name] = Sha (Join-Path $repositoryRoot $name)
    }
    $GameExecutable = (Resolve-Path -LiteralPath $GameExecutable).Path
    Check 'known-source-executable' ((Sha $GameExecutable) -ceq $knownGameHash)
    $frontend = Package-Info $ClockFrontendPackageDirectory 58 'completed-clock-frontend-package'
    $plain = Package-Info $PlainPackageDirectory 23 'plain-package'
    $set = Package-Info $SetPackageDirectory 23 'Set-package'
    foreach ($path in @('version.dll','CLEO/de-online/core.dll','CLEO/de-online/game_de.dll','CLEO/de-online/de_client_bridge.dll','CLEO/de-online/pthread.dll','CLEO/de-online/index.js')) {
        $hashes = @(@($frontend,$plain,$set) | ForEach-Object {@($_.manifest.files | Where-Object path -CEQ $path)[0].sha256})
        Check ('same-reviewed-binary:'+ $path) (@($hashes | Select-Object -Unique).Count -eq 1)
    }
    foreach ($selection in @($frontend,$plain,$set)) {
        $config=[IO.File]::ReadAllText((Join-Path $selection.root 'Win64/CLEO/de-online/config.mjs'))
        foreach ($marker in @('MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE','MTA_CORE_CLOCK_REFERENCE','MTA_CORE_CLOCK_SET_REFERENCE')) {
            Check ('one-boolean-marker:'+ $selection.root+':'+$marker) ([regex]::Matches($config,('export const '+$marker+' = (true|false);')).Count -eq 1)
        }
    }
    $frontendText=[IO.File]::ReadAllText((Join-Path $frontend.root 'Win64/CLEO/de-online/config.mjs'))
    $trueMarkers=[regex]::Matches($frontendText,'export const ([A-Z0-9_]+) = true;')
    Check 'frontend-startup-is-only-enabled-trial' ($trueMarkers.Count -eq 1 -and $trueMarkers[0].Groups[1].Value -ceq 'MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE')
    $plainText=[IO.File]::ReadAllText((Join-Path $plain.root 'Win64/CLEO/de-online/config.mjs'))
    $setText=[IO.File]::ReadAllText((Join-Path $set.root 'Win64/CLEO/de-online/config.mjs'))
    Check 'plain-package-has-no-enabled-trial' ([regex]::Matches($plainText,'export const ([A-Z0-9_]+) = true;').Count -eq 0)
    $setTrueMarkers=[regex]::Matches($setText,'export const ([A-Z0-9_]+) = true;')
    Check 'Set-package-has-only-Set-enabled' ($setTrueMarkers.Count -eq 1 -and $setTrueMarkers[0].Groups[1].Value -ceq 'MTA_CORE_CLOCK_SET_REFERENCE')
    Check 'Set-package-explicitly-disables-Read' ([regex]::Matches($setText,'export const MTA_CORE_CLOCK_REFERENCE = false;').Count -eq 1)
    Check 'plain-startup-disabled' ([IO.File]::ReadAllText((Join-Path $plain.root 'Win64/CLEO/de-online/config.mjs')) -match 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = false;')
    Check 'Set-startup-disabled' ([IO.File]::ReadAllText((Join-Path $set.root 'Win64/CLEO/de-online/config.mjs')) -match 'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = false;')
    $stagedScript=Join-Path $frontend.root 'Win64/CLEO/de-online/index.js'
    Check 'staged-script-matches-current-reviewed-source' ((Sha $stagedScript) -ceq (Sha (Join-Path $repositoryRoot 'Client/definitive/game-adapter/index.js')))
    $stagedText=[IO.File]::ReadAllText($stagedScript)
    foreach ($name in @('DE_CoreGuiFrontendStartupApiVersionV2','DE_CoreGuiFrontendStartupBeginV2','DE_CoreGuiFrontendStartupCaptureClockV2',
        'DE_CoreGuiFrontendStartupReadClockV2','DE_CoreGuiFrontendStartupCaptureClockOwnerV2','DE_CoreGuiFrontendStartupReadClockOwnerV2')) {
        Check ('staged-JS-V2-route:'+ $name) ($stagedText.Contains("'"+$name+"'"))
    }
    foreach ($provider in @(
        @{relative='CLEO/de-online/core.dll';names=@('MTA_DEFrontendClockReferenceApiVersion','MTA_RunDEFrontendClockReference')},
        @{relative='CLEO/de-online/de_client_bridge.dll';names=@('DE_CoreGuiFrontendStartupApiVersionV2','DE_CoreGuiFrontendStartupBeginV2',
            'DE_CoreGuiFrontendStartupCaptureClockV2','DE_CoreGuiFrontendStartupReadClockV2','DE_CoreGuiFrontendStartupCaptureClockOwnerV2',
            'DE_CoreGuiFrontendStartupReadClockOwnerV2')},
        @{relative='CLEO/de-online/game_de.dll';names=@('BeginGameClockServiceV1','SubmitGameClockCommandV1','RenewGameClockServiceV1',
            'ReadGameClockServiceV1','EndGameClockServiceV1')})) {
        $exports=Get-Amd64Exports (Join-Path $frontend.root ('Win64/'+$provider.relative))
        foreach($name in $provider.names) { Check ('static-AMD64-clock-provider-export:'+ $name) $exports.Contains($name) }
    }
    $install = Join-Path $repositoryRoot 'Client/definitive/install.ps1'
    $update = Join-Path $repositoryRoot 'Client/definitive/update.ps1'
    $setMode = Join-Path $repositoryRoot 'Client/definitive/set-mode.ps1'
    $uninstall = Join-Path $repositoryRoot 'Client/definitive/uninstall.ps1'
    $variants = [ordered]@{
        'duplicate-startup'=(Variant 'duplicate-startup' $frontend.root {param($text) $text+[Environment]::NewLine+'export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = true;'+[Environment]::NewLine})
        'nonboolean-startup'=(Variant 'nonboolean-startup' $frontend.root {param($text) $text.Replace('export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = true;','export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = 1;')})
        'malformed-startup'=(Variant 'malformed-startup' $frontend.root {param($text) $text.Replace('export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = true;','export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = "true";')})
        'mixed-Read'=(Variant 'mixed-Read' $frontend.root {param($text) $text.Replace('export const MTA_CORE_CLOCK_REFERENCE = false;','export const MTA_CORE_CLOCK_REFERENCE = true;')})
        'mixed-Set'=(Variant 'mixed-Set' $frontend.root {param($text) $text.Replace('export const MTA_CORE_CLOCK_SET_REFERENCE = false;','export const MTA_CORE_CLOCK_SET_REFERENCE = true;')})
        'mixed-input'=(Variant 'mixed-input' $frontend.root {param($text) $text.Replace('export const MTA_CORE_INPUT_REFERENCE = false;','export const MTA_CORE_INPUT_REFERENCE = true;')})
        'base23-startup'=(Variant 'base23-startup' $plain.root {param($text) $text.Replace('export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = false;','export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = true;')})
    }
    foreach ($name in $variants.Keys) {
        [IO.File]::Copy((Join-Path $variants[$name] 'Win64/CLEO/de-online/config.mjs'),(Join-Path $outputRoot ($name+'-variant-config.mjs')),$false)
        [IO.File]::Copy((Join-Path $variants[$name] 'manifest.json'),(Join-Path $outputRoot ($name+'-variant-manifest.json')),$false)
    }
    Fresh-Game
    foreach ($name in $variants.Keys) { Refuse ('install-refuses-'+$name) $install @('-Mode','Smoke','-PackageDirectory',$variants[$name],'-GameDirectory',$fakeGame) }
    Run 'install-plain23' $install @('-Mode','Smoke','-PackageDirectory',$plain.root,'-GameDirectory',$fakeGame) $true | Out-Null
    Installed 23 $plain.manifest 'installed-plain23' | Out-Null
    $loaderHash=Sha (Join-Path $fakeGame 'version.dll')
    $unowned=Join-Path $fakeGame 'CLEO/user-kept.txt'
    [IO.File]::WriteAllText($unowned,'Task-owned unowned sentinel must survive every transition.',$utf8)
    $unownedHash=Sha $unowned
    Refuse 'startup-refuses-base23-ownership' $setMode @('-Mode','Smoke','-MtaCoreGuiFrontendStartupReference','-GameDirectory',$fakeGame)
    foreach ($name in $variants.Keys) { Refuse ('update-refuses-'+$name) $update @('-PackageDirectory',$variants[$name],'-GameDirectory',$fakeGame) }
    Transition 'plain23-to-clock-frontend58' $frontend
    Marker $true 'upgraded-startup'
    Run 'ordinary-Smoke-revokes-startup' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
    Marker $false 'ordinary-Smoke'
    Installed 58 $null 'ordinary-Smoke58-ownership' | Out-Null
    Run 'explicit-startup-reenables58' $setMode @('-Mode','Smoke','-MtaCoreGuiFrontendStartupReference','-GameDirectory',$fakeGame) $true | Out-Null
    Marker $true 'explicit-startup'
    foreach ($switch in @('MtaCoreClockReference','MtaCoreClockSetReference','MtaCoreInputReference')) {
        Refuse ('startup-mixed-switch-refused-'+$switch) $setMode @('-Mode','Smoke','-MtaCoreGuiFrontendStartupReference',('-'+$switch),'-GameDirectory',$fakeGame)
    }
    Refuse 'startup-Online-refused' $setMode @('-Mode','Online','-MtaCoreGuiFrontendStartupReference','-GameDirectory',$fakeGame)
    $validConfig=[IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs'))
    $validManifest=[IO.File]::ReadAllBytes((Join-Path $fakeGame 'de-online-install-manifest.json'))
    foreach ($name in @('duplicate-startup','nonboolean-startup','malformed-startup')) {
        Mutate-InstalledConfig ([IO.File]::ReadAllText((Join-Path $variants[$name] 'Win64/CLEO/de-online/config.mjs')))
        Refuse ('explicit-refuses-installed-'+$name) $setMode @('-Mode','Smoke','-MtaCoreGuiFrontendStartupReference','-GameDirectory',$fakeGame)
        Refuse ('ordinary-refuses-installed-'+$name) $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame)
        [IO.File]::WriteAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs'),$validConfig,$utf8)
        [IO.File]::WriteAllBytes((Join-Path $fakeGame 'de-online-install-manifest.json'),$validManifest)
    }
    $editedPath=Join-Path $fakeGame 'CLEO/de-online/mta/config/coreconfig.xml'
    $original=[IO.File]::ReadAllBytes($editedPath)
    [IO.File]::WriteAllText($editedPath,'Task-owned edit must block mode changes and update.',$utf8)
    Refuse 'startup-refuses-edited-owned-file' $setMode @('-Mode','Smoke','-MtaCoreGuiFrontendStartupReference','-GameDirectory',$fakeGame)
    Refuse 'update-refuses-edited-owned-file' $update @('-PackageDirectory',$set.root,'-GameDirectory',$fakeGame)
    [IO.File]::WriteAllBytes($editedPath,$original)
    Transition 'clock-frontend58-to-Set23' $set
    Marker $false 'Set-transition-revokes-startup'
    Check 'Set-transition-only-Set-enabled' ([IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs')) -match 'export const MTA_CORE_CLOCK_SET_REFERENCE = true;')
    Refuse 'startup-still-refuses-Set23' $setMode @('-Mode','Smoke','-MtaCoreGuiFrontendStartupReference','-GameDirectory',$fakeGame)
    Run 'ordinary-Smoke-clears-Set23' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
    Check 'ordinary-Smoke-Set-marker-disabled' ([IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs')) -match 'export const MTA_CORE_CLOCK_SET_REFERENCE = false;')
    Check 'all-mode-changes-loader-unchanged' ((Sha (Join-Path $fakeGame 'version.dll')) -ceq $loaderHash)
    Check 'all-mode-changes-unowned-unchanged' ((Sha $unowned) -ceq $unownedHash)
    Run 'final-owned-validation' $uninstall @('-ValidateOnly','-GameDirectory',$fakeGame) $true | Out-Null
    Save (Join-Path $outputRoot 'final-fake-game-tree.json') (Tree $fakeGame).rows
    $sourcesUnchanged=$true
    foreach ($name in $sourcePins.Keys) { if ((Sha (Join-Path $repositoryRoot $name)) -cne $sourcePins[$name]) {$sourcesUnchanged=$false} }
    Check 'production-and-test-sources-unchanged' $sourcesUnchanged
    foreach ($pin in $inputPins) {
        Check ($pin.name+':manifest-unchanged') ((Sha $pin.manifest) -ceq $pin.manifestSha256)
        foreach ($file in $pin.files) { Check ($pin.name+':input-unchanged:'+ $file.path) ((Sha $file.path) -ceq $file.sha256) }
    }
} catch {
    $failure=$_.Exception.Message
} finally {
    [Environment]::SetEnvironmentVariable('TEMP',$savedTemp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$savedTmp,'Process')
    # The one game copy and variant DLL copies are disposable. Raw command
    # output, manifests/configs, tree hashes and updater backups remain durable.
    $resolvedScratch=[IO.Path]::GetFullPath($scratchRoot)
    if (-not $resolvedScratch.StartsWith($scratchParent+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Scratch escaped Build/tmp.'}
    if (Test-Path -LiteralPath $resolvedScratch) {Remove-Item -LiteralPath $resolvedScratch -Recurse -Force}
    $fakeRemoved=-not (Test-Path -LiteralPath $fakeGame)
}
$record=[ordered]@{
    schema='mta_frontend_clock_package_transition_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot
    success=($null -eq $failure -and $sourcesUnchanged -and $fakeRemoved);failure=$failure;checks=$checks.Count;cases=$checks.ToArray();runs=$runs.ToArray()
    sourcePins=$sourcePins;sourcesUnchanged=$sourcesUnchanged;inputPackages=$inputPins.ToArray();ownedUpdateBackups=$backups.ToArray()
    fakeGameDirectory=$fakeGame;fakeGameRemoved=$fakeRemoved;scratchDirectory=$scratchRoot;scratchFilesRemaining=$(if(Test-Path -LiteralPath $scratchRoot){@(Get-ChildItem -LiteralPath $scratchRoot -File -Recurse).Count}else{0})
    copiedGameExecutable=$GameExecutable;sourceGameSha256=$knownGameHash;fakeExecutableExecuted=$false
    liveInstallationChanged=$false;liveGameLaunched=$false;nativeClockWritten=$false;fullMtaReady=$false
    scope='Actual install/update/set-mode/uninstall validation against one owned Build/tmp game copy. No game, DLL, GUI, native hook or network is executed.'
}
$resultPath=Join-Path $outputRoot 'test-result.json'
Save $resultPath $record
[ordered]@{success=$record.success;checks=$record.checks;path=$resultPath;sha256=(Sha $resultPath);failure=$failure} | ConvertTo-Json -Depth 4
if (-not $record.success) {exit 1}
