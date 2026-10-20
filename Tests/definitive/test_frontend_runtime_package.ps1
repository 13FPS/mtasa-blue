#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise runtime package admission and reversible ownership with one never-executed game copy.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RuntimePackageDirectory,
    [Parameter(Mandatory)][string]$PlainPackageDirectory,
    [string]$LegacyFrontendPackageDirectory,
    [string]$BindingPersistencePackageDirectory,
    [string]$NativeRuntimePackageDirectory,
    [Parameter(Mandatory)][string]$GameExecutable,
    [switch]$BindingPersistenceOnly,
    [switch]$NativeHostOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $repositoryRoot ('Build/mta-frontend-runtime-package/' + $id)
$scratchParent = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'Build/tmp'))
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $scratchParent ('frontend-runtime-package-' + $id)))
$fakeGame = Join-Path $scratchRoot 'owned-fake-game'
$fixtureRoot = Join-Path $scratchRoot 'package-fixtures'
$utf8 = [Text.UTF8Encoding]::new($false)
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$runtimePattern = 'export const MTA_CORE_FRONTEND_RUNTIME = (true|false);'
$checks = [Collections.Generic.List[object]]::new()
$runs = [Collections.Generic.List[object]]::new()
$backups = [Collections.Generic.List[object]]::new()
$sourcePins = [ordered]@{}
$inputPins = [Collections.Generic.List[object]]::new()
$failure = $null
$sourcesUnchanged = $false
$fakeRemoved = $false
$gameCopiesCreated = 0
$nativeSavedData = $null
$nativeSavedDataHash = $null
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
function Fresh-Game {
    $owned = Assert-Owned $fakeGame
    if (Test-Path -LiteralPath $owned) { Remove-Item -LiteralPath $owned -Recurse -Force }
    New-Item -ItemType Directory -Path $owned | Out-Null
    [IO.File]::Copy($GameExecutable,(Join-Path $owned 'SanAndreas.exe'),$false)
    $script:gameCopiesCreated++
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
    # Reuse at most one fixture per 23/58/60-file group. Only configs
    # and manifests vary; a matrix case must never duplicate the 89 MB game.
    $count=@((Get-Content -LiteralPath (Join-Path $Source 'manifest.json') -Raw | ConvertFrom-Json).files).Count
    $target=Assert-Owned (Join-Path $fixtureRoot ('shared-'+$count))
    if(-not (Test-Path -LiteralPath $target)) {
        New-Item -ItemType Directory -Path $target | Out-Null
        Copy-Item -LiteralPath (Join-Path $Source 'Win64') -Destination $target -Recurse
    }
    $config=Join-Path $target 'Win64/CLEO/de-online/config.mjs'
    $text=& $Change ([IO.File]::ReadAllText((Join-Path $Source 'Win64/CLEO/de-online/config.mjs')))
    [IO.File]::WriteAllText($config,$text,$utf8)
    $manifest=Get-Content -LiteralPath (Join-Path $Source 'manifest.json') -Raw | ConvertFrom-Json
    @($manifest.files | Where-Object path -CEQ 'CLEO/de-online/config.mjs')[0].sha256=Sha $config
    Save (Join-Path $target 'manifest.json') $manifest
    [IO.File]::Copy($config,(Join-Path $outputRoot ($Name+'-config.mjs')),$true)
    Save (Join-Path $outputRoot ($Name+'-manifest.json')) $manifest
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
    $matches = [regex]::Matches($text,$runtimePattern)
    Check ($Name + ':runtime-marker') ($matches.Count -eq 1 -and $matches[0].Groups[1].Value -ceq $Enabled.ToString().ToLowerInvariant())
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
    $priorCount = @((Get-Content -Raw -LiteralPath (Join-Path $fakeGame 'de-online-install-manifest.json') | ConvertFrom-Json).files).Count
    Check ($Name+':known-owned-group-size') ($priorCount -in @(23,58,60))
    $prior = Installed $priorCount $null ($Name+'-before')
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
    $nativePaths = @('MtaDefinitive.asi','MtaDefinitive.ini')
    $removedGui = @($removed | Where-Object {$_.path -notin $nativePaths})
    $addedGui = @($added | Where-Object {$_.path -notin $nativePaths})
    Check ($Name+':complete-GUI-group-transition') (($removedGui.Count -eq 0 -and $addedGui.Count -eq 35) -or
        ($removedGui.Count -eq 35 -and $addedGui.Count -eq 0) -or ($removedGui.Count -eq 0 -and $addedGui.Count -eq 0))
    Check ($Name+':complete-native-entry-group-transition') (@($removed | Where-Object {$_.path -in $nativePaths}).Count -in @(0,2) -and
        @($added | Where-Object {$_.path -in $nativePaths}).Count -in @(0,2))
    foreach ($row in @($removedGui)+@($addedGui)) {
        Check ($Name+':GUI-owned-path:'+ $row.path) ($row.path -match '^CLEO/de-online/(cgui\.dll|mta/cgui/|skins/Default/|mta/locale/en_US/)')
    }
    foreach ($row in $removed) { Check ($Name+':removed:'+ $row.path) (-not (Test-Path -LiteralPath $row.destination)) }
    foreach ($old in $prior.files) {
        if ($old.path -notin $changes.path) { Check ($Name+':unchanged-owned:'+ $old.path) ((Sha $old.destination) -ceq $old.sha256) }
    }
    Check ($Name+':loader-preserved') ((Sha (Join-Path $fakeGame 'version.dll')) -ceq $loaderHash)
    Check ($Name+':unowned-preserved') ((Sha $unowned) -ceq $unownedHash)
    if ($NativeHostOnly) { Check ($Name+':saved-data-preserved') ((Sha $nativeSavedData) -ceq $nativeSavedDataHash) }
    Check ($Name+':game-preserved') ((Sha (Join-Path $fakeGame 'SanAndreas.exe')) -ceq $knownGameHash)
    $backups.Add([ordered]@{name=$Name;directory=$backup;originalManifestSha256=$priorManifest;orderedChanges=$changes;
        savedOriginals=$savedOriginals;removedPaths=@($removed | ForEach-Object path);addedPaths=@($added | ForEach-Object path)})
}
function All-Off([string]$Name,[bool]$AllowMissingRuntime=$false) {
    $text=[IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs'))
    Check ($Name+':no-enabled-trials') ([regex]::Matches($text,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;').Count -eq 0)
    foreach($marker in $oldMarkers) {
        Check ($Name+':explicitly-disabled:'+ $marker) ([regex]::Matches($text,('export const '+$marker+' = false;')).Count -eq 1)
    }
    $runtimeMarkers=[regex]::Matches($text,$runtimePattern)
    Check ($Name+':runtime-disabled-or-historical-absent') (($runtimeMarkers.Count -eq 1 -and $runtimeMarkers[0].Groups[1].Value -ceq 'false') -or
        ($AllowMissingRuntime -and $runtimeMarkers.Count -eq 0))
}
function Selection([string]$Directory) {
    [ordered]@{root=$Directory;manifest=(Get-Content -LiteralPath (Join-Path $Directory 'manifest.json') -Raw | ConvertFrom-Json)}
}
function Flags-Switches([string]$Script) {
    @((Get-Command -Name $Script).Parameters.Values | Where-Object {
        $_.Name -match '^(Native|Matched|Lock|Mta)' -and $_.Name -notin @('MtaCoreFrontendRuntime','MtaCoreFrontendBindingPersistence','MtaCoreFrontendNativeHost') -and
        $_.ParameterType -eq [Management.Automation.SwitchParameter]
    } | ForEach-Object Name | Sort-Object)
}
function Unchanged-Run([string]$Name,[string]$Script,[string[]]$Arguments,[bool]$ExpectedSuccess,[int]$ExpectedBackups=0) {
    $before=Tree $fakeGame
    $previousBackups=@(Backup-Directories)
    Run $Name $Script $Arguments $ExpectedSuccess | Out-Null
    $after=Tree $fakeGame
    Check ($Name+':exact-tree-unchanged') ($before.canonical -ceq $after.canonical)
    $newBackups=@(Backup-Directories | Where-Object {$_ -notin $previousBackups})
    Check ($Name+':backup-count') ($newBackups.Count -eq $ExpectedBackups)
    $backupRows=@()
    foreach($backup in $newBackups){
        $backupRows+=,[ordered]@{directory=$backup;files=(Tree $backup).rows}
    }
    Save (Join-Path $outputRoot ($Name+'-unchanged-proof.json')) ([ordered]@{before=$before.rows;after=$after.rows;unchanged=$true;backups=$backupRows})
}
function Rollback-Update([string]$Name,$Selection) {
    $manifestPath=Join-Path $fakeGame 'de-online-install-manifest.json'
    $prior=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $manifestHash=Sha $manifestPath
    $changes=@()
    foreach($old in $prior.files) {
        if($old.path -notin $Selection.manifest.files.path){$changes+=,[ordered]@{path=$old.path;original=$true;sha256=$old.sha256}}
    }
    foreach($new in $Selection.manifest.files) {
        $old=@($prior.files|Where-Object path -CEQ $new.path)
        if(-not $old.Count){$changes+=,[ordered]@{path=$new.path;original=$false;sha256=$null}}
        elseif($old[0].sha256 -cne $new.sha256){$changes+=,[ordered]@{path=$new.path;original=$true;sha256=$old[0].sha256}}
    }
    $previousBackups=@(Backup-Directories)
    # Use the same real replacement refusal as the existing ownership matrix.
    # A downgrade removes all 35 GUI assets before replacing this config, so
    # exact-tree recovery proves the removed group was restored without a seam.
    $held=[IO.File]::Open((Join-Path $fakeGame 'CLEO/de-online/config.mjs'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try { Unchanged-Run $Name $update @('-PackageDirectory',$Selection.root,'-GameDirectory',$fakeGame) $false 1 }
    finally {$held.Dispose()}
    $backup=@(Backup-Directories | Where-Object {$_ -notin $previousBackups})[0]
    Check ($Name+':original-manifest-retained') ((Sha (Join-Path $backup 'manifest.json')) -ceq $manifestHash)
    $originalCount=0
    for($index=0;$index -lt $changes.Count;++$index) {
        $original=Join-Path $backup ($index.ToString()+'.original')
        if($changes[$index].original) {
            ++$originalCount
            Check ($Name+':exact-ordered-backup:'+ $changes[$index].path) ((Test-Path -LiteralPath $original) -and (Sha $original) -ceq $changes[$index].sha256)
        } else {Check ($Name+':addition-has-no-original:'+ $changes[$index].path) (-not (Test-Path -LiteralPath $original))}
    }
    Check ($Name+':exact-backup-files') (@(Get-ChildItem -LiteralPath $backup -File -Recurse).Count -eq $originalCount+1)
    $backups.Add([ordered]@{name=$Name;rollbackObserved=$true;directory=$backup;originalManifestSha256=$manifestHash;files=(Tree $backup).rows})
}
function Native-Variant([string]$Name,[string]$Source,[scriptblock]$Change) {
    $target = Variant $Name $Source {param($text)$text}
    # One shared60 fixture is reset from its immutable input for every case.
    # Restoring the two small native files also repairs earlier partial groups.
    foreach ($relative in @('MtaDefinitive.asi','MtaDefinitive.ini')) {
        $destination = Assert-Owned (Join-Path $target ('Win64/'+$relative))
        [IO.File]::Copy((Join-Path $Source ('Win64/'+$relative)),$destination,$true)
    }
    $manifest = Get-Content -LiteralPath (Join-Path $Source 'manifest.json') -Raw | ConvertFrom-Json
    & $Change $target $manifest
    foreach ($entry in $manifest.files) { $entry.sha256 = Sha (Join-Path $target ('Win64/'+$entry.path)) }
    Save (Join-Path $target 'manifest.json') $manifest
    Save (Join-Path $outputRoot ($Name+'-manifest.json')) $manifest
    $ini = Join-Path $target 'Win64/MtaDefinitive.ini'
    if (Test-Path -LiteralPath $ini) { [IO.File]::Copy($ini,(Join-Path $outputRoot ($Name+'-MtaDefinitive.ini')),$true) }
    [IO.File]::Copy((Join-Path $target 'Win64/CLEO/de-online/config.mjs'),(Join-Path $outputRoot ($Name+'-config.mjs')),$true)
    $target
}
function Native-State([string]$Name,[bool]$Runtime,[bool]$Enabled,[bool]$Persistent) {
    $text = [IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs'))
    foreach ($pair in @(@('MTA_CORE_FRONTEND_RUNTIME',$Runtime),@('MTA_CORE_FRONTEND_NATIVE_HOST',$Enabled),@('MTA_CORE_FRONTEND_BINDING_PERSISTENCE',$Persistent))) {
        $marker = [regex]::Matches($text,('export const '+$pair[0]+' = (true|false);'))
        Check ($Name+':marker:'+ $pair[0]) ($marker.Count -eq 1 -and $marker[0].Groups[1].Value -ceq $pair[1].ToString().ToLowerInvariant())
    }
    $expected = "[MTA]`nEnabled=$([int]$Enabled)`nLocalInput=1`nBindingPersistence=$([int]$Persistent)`n"
    Check ($Name+':INI-and-marker-agree') ([IO.File]::ReadAllText((Join-Path $fakeGame 'MtaDefinitive.ini')).Replace("`r`n","`n") -ceq $expected)
    Installed 60 $null ($Name+'-owned60') | Out-Null
}
try {
    foreach($name in @('Client/definitive/install.ps1','Client/definitive/update.ps1','Client/definitive/uninstall.ps1',
        'Client/definitive/set-mode.ps1','Client/definitive/package.ps1','Client/definitive/NativeFrontendPackage.ps1','Client/definitive/game-adapter/index.js',
        'Client/definitive/game-adapter/config.mjs','Tests/definitive/test_frontend_runtime_package.ps1')) {
        $sourcePins[$name]=Sha (Join-Path $repositoryRoot $name)
    }
    $GameExecutable=(Resolve-Path -LiteralPath $GameExecutable).Path
    if ($NativeHostOnly -and $BindingPersistenceOnly) { throw 'Choose either focused native-host or saved-binding ownership coverage.' }
    Check 'known-source-executable' ((Sha $GameExecutable) -ceq $knownGameHash)
    $runtime=Package-Info $RuntimePackageDirectory 58 'runtime-package'
    $plain=Package-Info $PlainPackageDirectory 23 'plain-package'
    $legacy=$null
    if($LegacyFrontendPackageDirectory){$legacy=Package-Info $LegacyFrontendPackageDirectory 58 'legacy-frontend-package'}
    foreach($path in @('version.dll','CLEO/de-online/core.dll','CLEO/de-online/game_de.dll','CLEO/de-online/de_client_bridge.dll','CLEO/de-online/pthread.dll','CLEO/de-online/index.js')) {
        $hashes=@(@($runtime,$plain) | ForEach-Object {@($_.manifest.files | Where-Object path -CEQ $path)[0].sha256})
        Check ('same-reviewed-payload:'+ $path) (@($hashes | Select-Object -Unique).Count -eq 1)
    }
    $runtimeText=[IO.File]::ReadAllText((Join-Path $runtime.root 'Win64/CLEO/de-online/config.mjs'))
    $plainText=[IO.File]::ReadAllText((Join-Path $plain.root 'Win64/CLEO/de-online/config.mjs'))
    $trueFlags=[regex]::Matches($runtimeText,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;')
    Check 'runtime-is-only-enabled-trial' ($trueFlags.Count -eq 1 -and $trueFlags[0].Groups[1].Value -ceq 'MTA_CORE_FRONTEND_RUNTIME')
    Check 'plain-has-no-enabled-trial' ([regex]::Matches($plainText,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;').Count -eq 0)
    Check 'plain-runtime-marker-explicit-false' ([regex]::Matches($plainText,'export const MTA_CORE_FRONTEND_RUNTIME = false;').Count -eq 1)
    $oldMarkers=@([regex]::Matches($runtimeText,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = false;') |
        ForEach-Object {$_.Groups[1].Value} | Where-Object {$_ -notin @('MTA_CORE_FRONTEND_BINDING_PERSISTENCE','MTA_CORE_FRONTEND_NATIVE_HOST')})
    Check 'coverage-includes-persistent-clock-Read-clock-Set-and-old-input' ('MTA_CORE_PERSISTENT_FRONTEND' -cin $oldMarkers -and 'MTA_CORE_CLOCK_REFERENCE' -cin $oldMarkers -and 'MTA_CORE_CLOCK_SET_REFERENCE' -cin $oldMarkers -and 'MTA_CORE_INPUT_REFERENCE' -cin $oldMarkers)
    Check 'old-trial-marker-list-is-unique' ($oldMarkers.Count -eq @($oldMarkers|Select-Object -Unique).Count)
    $stagedScript=Join-Path $runtime.root 'Win64/CLEO/de-online/index.js'
    Check 'staged-script-is-current-reviewed-source' ((Sha $stagedScript) -ceq (Sha (Join-Path $repositoryRoot 'Client/definitive/game-adapter/index.js')))
    foreach($provider in @(
        @{relative='CLEO/de-online/core.dll';names=@('MTA_DEFrontendRuntimeApiVersion','MTA_BeginDEFrontendRuntime','MTA_ReadDEFrontendRuntime','MTA_PulseDEFrontendRuntime','MTA_RunDEFrontendRuntimeView','MTA_ShutdownDEFrontendRuntime','MTA_ReleaseDEFrontendRuntime',
            'MTA_DELocalInputRuntimeApiVersion','MTA_ConfigureDELocalInputRuntime','MTA_PulseDELocalInputRuntime','MTA_ReadDELocalInputRuntime','MTA_SuspendDELocalInputRuntime','MTA_DisableDELocalInputRuntime')},
        @{relative='CLEO/de-online/de_client_bridge.dll';names=@('DE_FrontendRuntimeApiVersionV1','DE_FrontendRuntimeStartV1','DE_FrontendRuntimeCommandV1','DE_FrontendRuntimeHeartbeatV1','DE_FrontendRuntimePollV1','DE_FrontendRuntimeCaptureV1','DE_FrontendRuntimeReadV1',
            'DE_LocalInputRuntimeApiVersionV1','DE_LocalInputRuntimeStartV1','DE_LocalInputRuntimeCaptureV1','DE_LocalInputRuntimeReadV1')},
        @{relative='CLEO/de-online/game_de.dll';names=@('BeginGameFrontendInputLeaseV1','RenewGameFrontendInputLeaseV1','ReadGameFrontendInputLeaseV1','EndGameFrontendInputLeaseV1',
            'BeginGameLocalInputLeaseV1','RenewGameLocalInputLeaseV1','ReadGameLocalInputLeaseV1','EndGameLocalInputLeaseV1')})) {
        $exports=Get-Amd64Exports (Join-Path $runtime.root ('Win64/'+$provider.relative))
        foreach($name in $provider.names){Check ('static-AMD64-runtime-export:'+ $name) $exports.Contains($name)}
    }
    $install=Join-Path $repositoryRoot 'Client/definitive/install.ps1'
    $update=Join-Path $repositoryRoot 'Client/definitive/update.ps1'
    $setMode=Join-Path $repositoryRoot 'Client/definitive/set-mode.ps1'
    $package=Join-Path $repositoryRoot 'Client/definitive/package.ps1'
    $uninstall=Join-Path $repositoryRoot 'Client/definitive/uninstall.ps1'
    $variantChanges=[ordered]@{
        duplicate={param($text)$text+[Environment]::NewLine+'export const MTA_CORE_FRONTEND_RUNTIME = true;'+[Environment]::NewLine}
        nonboolean={param($text)$text.Replace('export const MTA_CORE_FRONTEND_RUNTIME = true;','export const MTA_CORE_FRONTEND_RUNTIME = 1;')}
        malformed={param($text)$text.Replace('export const MTA_CORE_FRONTEND_RUNTIME = true;','export const MTA_CORE_FRONTEND_RUNTIME = "true";')}
    }
    if ($NativeHostOnly) {
        if (-not $NativeRuntimePackageDirectory) { throw 'Focused native-host checks require their actual60-file package.' }
        $native = Package-Info $NativeRuntimePackageDirectory 60 'native-runtime-package'
        $nativeText = [IO.File]::ReadAllText((Join-Path $native.root 'Win64/CLEO/de-online/config.mjs'))
        $nativePersistent = [regex]::Matches($nativeText,'export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = true;').Count -eq 1
        $nativeTrueFlags = @([regex]::Matches($nativeText,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;') | ForEach-Object {$_.Groups[1].Value})
        $expectedFlags = @('MTA_CORE_FRONTEND_RUNTIME','MTA_CORE_FRONTEND_NATIVE_HOST')
        if ($nativePersistent) { $expectedFlags += 'MTA_CORE_FRONTEND_BINDING_PERSISTENCE' }
        Check 'native-package-only-explicit-runtime-options' (@(Compare-Object $nativeTrueFlags $expectedFlags).Count -eq 0)
        Check 'native-package-complete-entry-group' (@($native.manifest.files | Where-Object {$_.path -in @('MtaDefinitive.asi','MtaDefinitive.ini')}).Count -eq 2)
        Check 'session-runtime-default-native-host-disabled' ([regex]::Matches($runtimeText,'export const MTA_CORE_FRONTEND_NATIVE_HOST = false;').Count -eq 1)
        foreach ($path in @('version.dll','CLEO/de-online/core.dll','CLEO/de-online/game_de.dll','CLEO/de-online/de_client_bridge.dll','CLEO/de-online/pthread.dll','CLEO/de-online/index.js')) {
            Check ('native-package-same-reviewed-payload:'+ $path) (@($native.manifest.files | Where-Object path -CEQ $path)[0].sha256 -ceq
                @($runtime.manifest.files | Where-Object path -CEQ $path)[0].sha256)
        }
        $nativeIni = "[MTA]`nEnabled=1`nLocalInput=1`nBindingPersistence=$([int]$nativePersistent)`n"
        Check 'native-package-exact-owned-INI' ([IO.File]::ReadAllText((Join-Path $native.root 'Win64/MtaDefinitive.ini')).Replace("`r`n","`n") -ceq $nativeIni)
        $nativeChanges = [ordered]@{
            duplicate_marker = {param($root,$manifest)
                $path=Join-Path $root 'Win64/CLEO/de-online/config.mjs'
                [IO.File]::AppendAllText($path,"`nexport const MTA_CORE_FRONTEND_NATIVE_HOST = true;`n",$utf8)
            }
            nonboolean_marker = {param($root,$manifest)
                $path=Join-Path $root 'Win64/CLEO/de-online/config.mjs'
                [IO.File]::WriteAllText($path,[IO.File]::ReadAllText($path).Replace('export const MTA_CORE_FRONTEND_NATIVE_HOST = true;','export const MTA_CORE_FRONTEND_NATIVE_HOST = 1;'),$utf8)
            }
            missing_marker = {param($root,$manifest)
                $path=Join-Path $root 'Win64/CLEO/de-online/config.mjs'
                [IO.File]::WriteAllText($path,[regex]::Replace([IO.File]::ReadAllText($path),'(?m)^export const MTA_CORE_FRONTEND_NATIVE_HOST = true;\r?\n?',''),$utf8)
            }
            without_runtime = {param($root,$manifest)
                $path=Join-Path $root 'Win64/CLEO/de-online/config.mjs'
                [IO.File]::WriteAllText($path,[IO.File]::ReadAllText($path).Replace('export const MTA_CORE_FRONTEND_RUNTIME = true;','export const MTA_CORE_FRONTEND_RUNTIME = false;'),$utf8)
            }
            disabled_ini = {param($root,$manifest)
                $path=Join-Path $root 'Win64/MtaDefinitive.ini'
                [IO.File]::WriteAllText($path,[IO.File]::ReadAllText($path).Replace('Enabled=1','Enabled=0'),$utf8)
            }
            mismatched_persistence = {param($root,$manifest)
                $path=Join-Path $root 'Win64/MtaDefinitive.ini'
                $text=[IO.File]::ReadAllText($path)
                $replacement=if($text.Contains('BindingPersistence=1')){'BindingPersistence=0'}else{'BindingPersistence=1'}
                [IO.File]::WriteAllText($path,[regex]::Replace($text,'BindingPersistence=[01]',$replacement),$utf8)
            }
            disabled_local_input = {param($root,$manifest)
                $path=Join-Path $root 'Win64/MtaDefinitive.ini'
                [IO.File]::WriteAllText($path,[IO.File]::ReadAllText($path).Replace('LocalInput=1','LocalInput=0'),$utf8)
            }
            missing_asi = {param($root,$manifest)
                Remove-Item -LiteralPath (Assert-Owned (Join-Path $root 'Win64/MtaDefinitive.asi'))
                $manifest.files=@($manifest.files | Where-Object path -CNE 'MtaDefinitive.asi')
            }
            missing_ini = {param($root,$manifest)
                Remove-Item -LiteralPath (Assert-Owned (Join-Path $root 'Win64/MtaDefinitive.ini'))
                $manifest.files=@($manifest.files | Where-Object path -CNE 'MtaDefinitive.ini')
            }
        }
        Fresh-Game
        foreach ($name in $nativeChanges.Keys) {
            $variant=Native-Variant ('invalid-native-'+$name) $native.root $nativeChanges[$name]
            Unchanged-Run ('install-refuses-native-'+$name) $install @('-Mode','Smoke','-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
        }
        $standalone=Variant 'session58-native-marker-without-entry' $runtime.root {param($text)$text.Replace('export const MTA_CORE_FRONTEND_NATIVE_HOST = false;','export const MTA_CORE_FRONTEND_NATIVE_HOST = true;')}
        Unchanged-Run 'install-refuses-native-marker-without-entry' $install @('-Mode','Smoke','-PackageDirectory',$standalone,'-GameDirectory',$fakeGame) $false
        Unchanged-Run 'native-install-WhatIf-no-writes' $install @('-Mode','Smoke','-PackageDirectory',$native.root,'-GameDirectory',$fakeGame,'-WhatIf') $true
        Run 'install-native-runtime60' $install @('-Mode','Smoke','-PackageDirectory',$native.root,'-GameDirectory',$fakeGame) $true | Out-Null
        Installed 60 $native.manifest 'native-initial60' | Out-Null
        Native-State 'native-initial-selection' $true $true $nativePersistent
        $loaderHash=Sha (Join-Path $fakeGame 'version.dll')
        $manifestPath=Join-Path $fakeGame 'de-online-install-manifest.json'
        $unowned=Join-Path $fakeGame 'CLEO/user-kept.txt'
        [IO.File]::WriteAllText($unowned,'Unowned player content survives native entry changes.',$utf8)
        $unownedHash=Sha $unowned
        $nativeSavedData=Join-Path $fakeGame 'CLEO/de-online/mta/config/user-saved-movement-bindings.xml'
        [IO.File]::WriteAllText($nativeSavedData,'<definitive_keybinds version="1"><keybinds /></definitive_keybinds>',$utf8)
        $nativeSavedDataHash=Sha $nativeSavedData
        foreach ($name in $nativeChanges.Keys) {
            $variant=Native-Variant ('invalid-native-'+$name) $native.root $nativeChanges[$name]
            Unchanged-Run ('update-refuses-native-'+$name) $update @('-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
        }
        Unchanged-Run 'update-refuses-native-marker-without-entry' $update @('-PackageDirectory',$standalone,'-GameDirectory',$fakeGame) $false
        Unchanged-Run 'native-mode-standalone-flag-refused' $setMode @('-Mode','Smoke','-MtaCoreFrontendNativeHost','-GameDirectory',$fakeGame) $false
        $rejectedOutput=Join-Path $scratchRoot 'must-not-stage-native-package'
        Unchanged-Run 'native-package-standalone-flag-refused-before-staging' $package @('-Mode','Smoke','-MtaCoreFrontendNativeHost','-OutputDirectory',$rejectedOutput) $false
        Check 'native-standalone-package-does-not-stage' (-not(Test-Path -LiteralPath $rejectedOutput))
        Unchanged-Run 'native-mode-WhatIf-no-writes' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame,'-WhatIf') $true
        foreach ($relative in @('de-online-install-manifest.json','MtaDefinitive.ini')) {
            $label=if($relative -ceq 'MtaDefinitive.ini'){'INI'}else{'manifest'}
            $held=[IO.File]::Open((Join-Path $fakeGame $relative),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
            try { Unchanged-Run ('native-mode-locked-'+$label+'-restores-all-bytes') $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $false }
            finally { $held.Dispose() }
        }
        foreach ($mode in @('Smoke','Diagnostic','Online')) {
            Run ('native-ordinary-'+$mode+'-disarms-entry') $setMode @('-Mode',$mode,'-GameDirectory',$fakeGame) $true | Out-Null
            All-Off ('native-ordinary-'+$mode)
            $ini=[IO.File]::ReadAllText((Join-Path $fakeGame 'MtaDefinitive.ini')).Replace("`r`n","`n")
            Check ('native-ordinary-'+$mode+':entry-disarmed') ($ini -ceq "[MTA]`nEnabled=0`nLocalInput=1`nBindingPersistence=0`n")
            Run ('native-explicit-session-after-'+$mode) $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreFrontendNativeHost','-GameDirectory',$fakeGame) $true | Out-Null
            Native-State ('native-session-after-'+$mode) $true $true $false
        }
        Run 'native-explicit-persistent-runtime' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreFrontendNativeHost','-MtaCoreFrontendBindingPersistence','-GameDirectory',$fakeGame) $true | Out-Null
        Native-State 'native-persistent-enabled' $true $true $true
        Rollback-Update 'native60-to-plain23-locked-config-restores-both-groups' $plain
        Transition 'native60-to-session58' $runtime
        Check 'native60-to58-removes-complete-entry-group' (-not(Test-Path -LiteralPath (Join-Path $fakeGame 'MtaDefinitive.asi')) -and
            -not(Test-Path -LiteralPath (Join-Path $fakeGame 'MtaDefinitive.ini')))
        Unchanged-Run 'session58-native-mode-requires-owned-entry-group' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreFrontendNativeHost','-GameDirectory',$fakeGame) $false
        foreach ($relative in @('MtaDefinitive.asi','MtaDefinitive.ini')) {
            $collision=Assert-Owned (Join-Path $fakeGame $relative)
            [IO.File]::WriteAllText($collision,'Unowned root entry must never be adopted or overwritten.',$utf8)
            $collisionHash=Sha $collision
            Unchanged-Run ('native-upgrade-refuses-unowned-'+$relative) $update @('-PackageDirectory',$native.root,'-GameDirectory',$fakeGame) $false
            Check ('native-upgrade-keeps-unowned:'+ $relative) ((Sha $collision) -ceq $collisionHash)
            Remove-Item -LiteralPath $collision
        }
        Unchanged-Run 'native-upgrade-WhatIf-no-writes' $update @('-PackageDirectory',$native.root,'-GameDirectory',$fakeGame,'-WhatIf') $true
        Transition 'session58-to-native60' $native
        Native-State 'native-readded-after58' $true $true $nativePersistent
        Transition 'native60-to-plain23' $plain
        Check 'native60-to23-removes-complete-entry-and-GUI-groups' (-not(Test-Path -LiteralPath (Join-Path $fakeGame 'MtaDefinitive.asi')) -and
            -not(Test-Path -LiteralPath (Join-Path $fakeGame 'MtaDefinitive.ini')) -and
            -not(Test-Path -LiteralPath (Join-Path $fakeGame 'CLEO/de-online/cgui.dll')))
        Transition 'plain23-to-native60' $native
        Native-State 'native-final60' $true $true $nativePersistent
        Unchanged-Run 'native-uninstall-WhatIf-no-writes' $uninstall @('-GameDirectory',$fakeGame,'-WhatIf') $true
    } elseif ($BindingPersistenceOnly) {
        # Reuse the existing owned package/transition helpers for the new
        # additive option instead of repeating every unchanged legacy trial.
        $disabled = 'export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = false;'
        $enabled = 'export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = true;'
        Check 'saved-binding-source-default-disabled' ($runtimeText.Contains($disabled))
        # Variant deliberately reuses one mutable fixture directory. Positive
        # saved transitions need their own immutable packaged input so later
        # malformed variants cannot replace the source they are meant to admit.
        if (-not $BindingPersistencePackageDirectory) { throw 'Focused saved-binding checks require their actual opt-in package.' }
        $savedInput = Package-Info $BindingPersistencePackageDirectory 58 'saved-binding-package'
        $savedPackage = $savedInput.root
        $savedSelection = Selection $savedPackage
        Check 'saved-package-optin-enabled' ([IO.File]::ReadAllText((Join-Path $savedPackage 'Win64/CLEO/de-online/config.mjs')).Contains($enabled))
        foreach ($provider in @(
            @{relative='CLEO/de-online/core.dll';name='MTA_EnableDEFrontendRuntimeBindingPersistenceV1'},
            @{relative='CLEO/de-online/de_client_bridge.dll';name='DE_LocalInputRuntimeStartPersistentV1'})) {
            $exports = Get-Amd64Exports (Join-Path $runtime.root ('Win64/'+$provider.relative))
            Check ('static-saved-binding-export:'+ $provider.name) $exports.Contains($provider.name)
        }
        $invalidSavedChanges = [ordered]@{
            duplicate={param($text)$text+[Environment]::NewLine+$enabled+[Environment]::NewLine}.GetNewClosure()
            nonboolean={param($text)$text.Replace($disabled,'export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = 1;')}.GetNewClosure()
            standalone={param($text)$text.Replace($disabled,$enabled).Replace('export const MTA_CORE_FRONTEND_RUNTIME = true;','export const MTA_CORE_FRONTEND_RUNTIME = false;')}.GetNewClosure()
        }
        Fresh-Game
        foreach ($name in $invalidSavedChanges.Keys) {
            $variant = Variant ('invalid-saved-'+$name) $runtime.root $invalidSavedChanges[$name]
            Unchanged-Run ('install-refuses-saved-'+$name) $install @('-Mode','Smoke','-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
        }
        Run 'install-saved-bindings58' $install @('-Mode','Smoke','-PackageDirectory',$savedPackage,'-GameDirectory',$fakeGame) $true | Out-Null
        Installed 58 $savedSelection.manifest 'saved-initial58' | Out-Null
        $loaderHash=Sha (Join-Path $fakeGame 'version.dll')
        $manifestPath=Join-Path $fakeGame 'de-online-install-manifest.json'
        $unowned=Join-Path $fakeGame 'CLEO/user-kept.txt'
        [IO.File]::WriteAllText($unowned,'User binding data remains independent of the owned installation.',$utf8)
        $unownedHash=Sha $unowned
        foreach ($name in $invalidSavedChanges.Keys) {
            $variant = Variant ('invalid-saved-'+$name) $runtime.root $invalidSavedChanges[$name]
            Unchanged-Run ('update-refuses-saved-'+$name) $update @('-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
        }
        Unchanged-Run 'saved-mode-WhatIf-no-writes' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreFrontendBindingPersistence','-GameDirectory',$fakeGame,'-WhatIf') $true
        Run 'ordinary-mode-clears-saved-binding-optin' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
        Check 'ordinary-mode-saved-marker-cleared' ([IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs')).Contains($disabled))
        Run 'explicit-session-runtime-remains-unsaved' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $true | Out-Null
        Check 'session-runtime-saved-marker-still-cleared' ([IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs')).Contains($disabled))
        Run 'explicit-saved-binding-runtime' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreFrontendBindingPersistence','-GameDirectory',$fakeGame) $true | Out-Null
        Check 'explicit-saved-marker-enabled' ([IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs')).Contains($enabled))
        Unchanged-Run 'saved-mode-standalone-refused' $setMode @('-Mode','Smoke','-MtaCoreFrontendBindingPersistence','-GameDirectory',$fakeGame) $false
        $rejectedOutput=Join-Path $scratchRoot 'must-not-stage-saved-package'
        Unchanged-Run 'saved-package-standalone-refused-before-staging' $package @('-Mode','Smoke','-MtaCoreFrontendBindingPersistence','-OutputDirectory',$rejectedOutput) $false
        Check 'standalone-saved-package-no-staging' (-not(Test-Path -LiteralPath $rejectedOutput))
        $historical=Variant 'historical58-no-saved-marker' $runtime.root {param($text)[regex]::Replace($text,'(?m)^export const MTA_CORE_FRONTEND_BINDING_PERSISTENCE = false;\r?\n?','')}
        Transition 'saved58-to-historical58-no-saved-marker' (Selection $historical)
        Run 'historical-session-runtime-compatible' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $true | Out-Null
        Unchanged-Run 'historical-saved-enable-requires-update' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreFrontendBindingPersistence','-GameDirectory',$fakeGame) $false
        Transition 'historical58-to-saved58' $savedSelection
        Rollback-Update 'saved58-downgrade-refusal-restores-owned-files' $plain
        Transition 'saved58-to-plain23' $plain
        Check 'downgrade-saved-marker-disabled' ([IO.File]::ReadAllText((Join-Path $fakeGame 'CLEO/de-online/config.mjs')).Contains($disabled))
        Transition 'plain23-to-saved58' $savedSelection
    } else {
    Fresh-Game
    foreach($name in $variantChanges.Keys) {
        $variant=Variant ('invalid-'+$name) $runtime.root $variantChanges[$name]
        Unchanged-Run ('install-refuses-'+$name) $install @('-Mode','Smoke','-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
    }
    foreach($marker in $oldMarkers) {
        $change={param($text)$text.Replace(('export const '+$marker+' = false;'),('export const '+$marker+' = true;'))}.GetNewClosure()
        $variant=Variant ('mixed-'+$marker) $runtime.root $change
        Unchanged-Run ('install-refuses-mixed-'+$marker) $install @('-Mode','Smoke','-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
    }
    $variant=Variant 'base23-runtime-true' $plain.root {param($text)$text.Replace('export const MTA_CORE_FRONTEND_RUNTIME = false;','export const MTA_CORE_FRONTEND_RUNTIME = true;')}
    Unchanged-Run 'install-refuses-base23-runtime' $install @('-Mode','Smoke','-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
    Run 'install-plain23' $install @('-Mode','Smoke','-PackageDirectory',$plain.root,'-GameDirectory',$fakeGame) $true | Out-Null
    Installed 23 $plain.manifest 'initial-plain23' | Out-Null
    $loaderHash=Sha (Join-Path $fakeGame 'version.dll')
    $unowned=Join-Path $fakeGame 'CLEO/user-kept.txt'
    [IO.File]::WriteAllText($unowned,'Task-owned unowned sentinel must survive all updates, refusals and rollback.',$utf8)
    $unownedHash=Sha $unowned
    foreach($name in $variantChanges.Keys) {
        $variant=Variant ('invalid-'+$name) $runtime.root $variantChanges[$name]
        Unchanged-Run ('update-refuses-'+$name) $update @('-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
    }
    foreach($marker in $oldMarkers) {
        $change={param($text)$text.Replace(('export const '+$marker+' = false;'),('export const '+$marker+' = true;'))}.GetNewClosure()
        $variant=Variant ('mixed-'+$marker) $runtime.root $change
        Unchanged-Run ('update-refuses-mixed-'+$marker) $update @('-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
    }
    $variant=Variant 'base23-runtime-true' $plain.root {param($text)$text.Replace('export const MTA_CORE_FRONTEND_RUNTIME = false;','export const MTA_CORE_FRONTEND_RUNTIME = true;')}
    Unchanged-Run 'update-refuses-base23-runtime' $update @('-PackageDirectory',$variant,'-GameDirectory',$fakeGame) $false
    Unchanged-Run 'set-mode-refuses-base23-runtime' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $false
    $collision=Join-Path $fakeGame 'CLEO/de-online/cgui.dll'
    [IO.File]::WriteAllText($collision,'Unowned GUI asset must not be replaced.',$utf8)
    Unchanged-Run 'update-refuses-unowned-GUI-collision' $update @('-PackageDirectory',$runtime.root,'-GameDirectory',$fakeGame) $false
    Remove-Item -LiteralPath (Assert-Owned $collision)
    $manifestPath=Join-Path $fakeGame 'de-online-install-manifest.json'
    $validManifest=[IO.File]::ReadAllBytes($manifestPath)
    $badManifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $badManifest.files=@($badManifest.files)+@($badManifest.files[0])
    Save $manifestPath $badManifest
    Unchanged-Run 'update-refuses-duplicate-owned-record' $update @('-PackageDirectory',$runtime.root,'-GameDirectory',$fakeGame) $false
    Unchanged-Run 'set-mode-refuses-duplicate-owned-record' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $false
    [IO.File]::WriteAllBytes($manifestPath,$validManifest)
    $badManifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $badManifest.files[0].destination=Join-Path $scratchRoot 'outside-game-never-created.bin'
    Save $manifestPath $badManifest
    Unchanged-Run 'update-refuses-unsafe-owned-destination' $update @('-PackageDirectory',$runtime.root,'-GameDirectory',$fakeGame) $false
    [IO.File]::WriteAllBytes($manifestPath,$validManifest)
    Check 'unsafe-destination-never-created' (-not (Test-Path -LiteralPath (Join-Path $scratchRoot 'outside-game-never-created.bin')))
    Unchanged-Run 'WhatIf-upgrade-no-writes' $update @('-PackageDirectory',$runtime.root,'-GameDirectory',$fakeGame,'-WhatIf') $true
    Rollback-Update 'upgrade-locked-config-keeps23-no-new-GUI' $runtime
    Transition 'plain23-to-runtime58' $runtime
    Marker $true 'runtime-upgraded'
    foreach($mode in @('Smoke','Diagnostic','Online')) {
        Run ('ordinary-'+$mode+'-clears-runtime') $setMode @('-Mode',$mode,'-GameDirectory',$fakeGame) $true | Out-Null
        All-Off ('ordinary-'+$mode)
        if($mode -ne 'Smoke'){Run ('restore-Smoke-after-'+$mode) $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null}
        Run ('explicit-runtime-after-'+$mode) $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $true | Out-Null
        Marker $true ('explicit-runtime-after-'+$mode)
    }
    Installed 58 $null 'runtime-after-mode-changes' | Out-Null
    $modeSwitches=Flags-Switches $setMode
    $packageSwitches=Flags-Switches $package
    Check 'set-mode-switch-coverage-includes-all-service-kinds' ('MtaCorePersistentFrontend' -cin $modeSwitches -and 'MtaCoreClockReference' -cin $modeSwitches -and 'MtaCoreClockSetReference' -cin $modeSwitches -and 'NativeGaitReference' -cin $modeSwitches)
    foreach($switch in $modeSwitches){Unchanged-Run ('mixed-switch-'+$switch) $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime',('-'+$switch),'-GameDirectory',$fakeGame) $false}
    $rejectedOutput=Join-Path $scratchRoot 'must-not-stage-package'
    foreach($switch in $packageSwitches){
        Unchanged-Run ('package-mixed-switch-'+$switch) $package @('-Mode','Smoke','-MtaCoreFrontendRuntime',('-'+$switch),'-OutputDirectory',$rejectedOutput) $false
        Check ('package-mixed-does-not-stage:'+ $switch) (-not (Test-Path -LiteralPath $rejectedOutput))
    }
    foreach($script in @($setMode,$package)) {
        $label=if($script -ceq $setMode){'set-mode'}else{'package'}
        foreach($mode in @('Diagnostic','Online')){
            $caseArguments=if($label -eq 'set-mode'){@('-Mode',$mode,'-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame)}else{@('-Mode',$mode,'-MtaCoreFrontendRuntime','-OutputDirectory',$rejectedOutput)}
            Unchanged-Run ($label+'-runtime-refuses-'+$mode) $script $caseArguments $false
        }
        $caseArguments=if($label -eq 'set-mode'){@('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreRenderRoute','D3D11','-GameDirectory',$fakeGame)}else{@('-Mode','Smoke','-MtaCoreFrontendRuntime','-MtaCoreRenderRoute','D3D11','-OutputDirectory',$rejectedOutput)}
        Unchanged-Run ($label+'-runtime-refuses-explicit-render-route') $script $caseArguments $false
    }
    Check 'all-package-conflicts-left-no-staging-directory' (-not (Test-Path -LiteralPath $rejectedOutput))
    $configPath=Join-Path $fakeGame 'CLEO/de-online/config.mjs'
    $validConfig=[IO.File]::ReadAllText($configPath)
    $validManifest=[IO.File]::ReadAllBytes($manifestPath)
    foreach($name in $variantChanges.Keys) {
        $variant=Variant ('installed-invalid-'+$name) $runtime.root $variantChanges[$name]
        Mutate-InstalledConfig ([IO.File]::ReadAllText((Join-Path $variant 'Win64/CLEO/de-online/config.mjs')))
        Unchanged-Run ('explicit-refuses-installed-'+$name) $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $false
        Unchanged-Run ('ordinary-refuses-installed-'+$name) $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $false
        [IO.File]::WriteAllText($configPath,$validConfig,$utf8)
        [IO.File]::WriteAllBytes($manifestPath,$validManifest)
    }
    $held=[IO.File]::Open($manifestPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try{Unchanged-Run 'mode-rollback-restores-config-and-manifest' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $false}
    finally{$held.Dispose()}
    $editedPath=Join-Path $fakeGame 'CLEO/de-online/mta/config/coreconfig.xml'
    $original=[IO.File]::ReadAllBytes($editedPath)
    [IO.File]::WriteAllText($editedPath,'Task-owned edit refuses mode switch and ownership removal.',$utf8)
    Unchanged-Run 'runtime-refuses-edited-owned-config' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $false
    Unchanged-Run 'downgrade-refuses-edited-owned-config' $update @('-PackageDirectory',$plain.root,'-GameDirectory',$fakeGame) $false
    [IO.File]::WriteAllBytes($editedPath,$original)
    if($legacy){
        Transition 'runtime58-to-legacy-frontend58' $legacy
        Run 'ordinary-Smoke-clears-legacy' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
        All-Off 'legacy-ordinary-mode' $true
        Transition 'legacy58-to-runtime58' $runtime
    }
    $historical=Variant 'historical58-missing-runtime-marker' $runtime.root {param($text)[regex]::Replace($text,'(?m)^export const MTA_CORE_FRONTEND_RUNTIME = true;\r?\n?','')}
    Transition 'runtime58-to-historical58-missing-marker' (Selection $historical)
    Run 'ordinary-mode-compatible-with-missing-runtime-marker' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
    All-Off 'historical58-ordinary-mode' $true
    Unchanged-Run 'runtime-enable-missing-marker-requires-update' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $false
    Transition 'historical58-to-updated-runtime58' $runtime
    Marker $true 'historical-updated-runtime'
    Rollback-Update 'downgrade-rollback-restores58-owned-assets' $plain
    Transition 'runtime58-to-plain23' $plain
    Marker $false 'downgrade-runtime-cleared'
    All-Off 'downgrade-all-trials-cleared'
    $historical=Variant 'historical23-missing-runtime-marker' $plain.root {param($text)[regex]::Replace($text,'(?m)^export const MTA_CORE_FRONTEND_RUNTIME = false;\r?\n?','')}
    Transition 'plain23-to-historical23-missing-marker' (Selection $historical)
    Run 'ordinary-mode-compatible-with-historical23' $setMode @('-Mode','Smoke','-GameDirectory',$fakeGame) $true | Out-Null
    All-Off 'historical23-ordinary-mode' $true
    Unchanged-Run 'runtime-refuses-historical23-until-update' $setMode @('-Mode','Smoke','-MtaCoreFrontendRuntime','-GameDirectory',$fakeGame) $false
    Transition 'historical23-to-runtime58' $runtime
    Marker $true 'final-runtime-enabled'
    }
    Check 'loader-preserved-through-all-updates' ((Sha (Join-Path $fakeGame 'version.dll')) -ceq $loaderHash)
    Check 'unowned-preserved-through-all-updates' ((Sha $unowned) -ceq $unownedHash)
    Check 'one-game-copy-still-exact' ((Sha (Join-Path $fakeGame 'SanAndreas.exe')) -ceq $knownGameHash)
    Check 'exactly-one-game-copy-was-created' ($gameCopiesCreated -eq 1)
    Run 'final-owned-validation' $uninstall @('-ValidateOnly','-GameDirectory',$fakeGame) $true | Out-Null
    Save (Join-Path $outputRoot 'final-installed-tree.json') (Tree $fakeGame).rows
    Run 'final-uninstall-removes-only-owned-files' $uninstall @('-GameDirectory',$fakeGame) $true | Out-Null
    Check 'uninstall-kept-game' ((Sha (Join-Path $fakeGame 'SanAndreas.exe')) -ceq $knownGameHash)
    Check 'uninstall-kept-unowned-sentinel' ((Sha $unowned) -ceq $unownedHash)
    if ($NativeHostOnly) {
        Check 'native-uninstall-kept-unowned-saved-binding-data' ((Sha $nativeSavedData) -ceq $nativeSavedDataHash)
        Check 'native-uninstall-retired-ASI-and-INI' (-not(Test-Path -LiteralPath (Join-Path $fakeGame 'MtaDefinitive.asi')) -and
            -not(Test-Path -LiteralPath (Join-Path $fakeGame 'MtaDefinitive.ini')))
    }
    Check 'uninstall-retired-owned-manifest-and-loader' (-not (Test-Path -LiteralPath $manifestPath) -and -not (Test-Path -LiteralPath (Join-Path $fakeGame 'version.dll')))
    Save (Join-Path $outputRoot 'final-uninstalled-tree.json') (Tree $fakeGame).rows
    $sourcesUnchanged=$true
    foreach($name in $sourcePins.Keys){if((Sha (Join-Path $repositoryRoot $name)) -cne $sourcePins[$name]){$sourcesUnchanged=$false}}
    Check 'production-and-test-sources-unchanged' $sourcesUnchanged
    foreach($pin in $inputPins){
        Check ($pin.name+':manifest-unchanged') ((Sha $pin.manifest) -ceq $pin.manifestSha256)
        foreach($file in $pin.files){Check ($pin.name+':input-unchanged:'+ $file.path) ((Sha $file.path) -ceq $file.sha256)}
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
    if (Test-Path -LiteralPath $resolvedScratch) {Input-NoLinks $resolvedScratch; Remove-Item -LiteralPath $resolvedScratch -Recurse -Force}
    $fakeRemoved=-not (Test-Path -LiteralPath $fakeGame)
}
$record=[ordered]@{
    schema='mta_frontend_runtime_package_transition_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot
    success=($null -eq $failure -and $sourcesUnchanged -and $fakeRemoved);failure=$failure;checks=$checks.Count;cases=$checks.ToArray();runs=$runs.ToArray()
    sourcePins=$sourcePins;sourcesUnchanged=$sourcesUnchanged;inputPackages=$inputPins.ToArray();ownedUpdateBackups=$backups.ToArray()
    fakeGameDirectory=$fakeGame;fakeGameRemoved=$fakeRemoved;scratchDirectory=$scratchRoot;scratchFilesRemaining=$(if(Test-Path -LiteralPath $scratchRoot){@(Get-ChildItem -LiteralPath $scratchRoot -File -Recurse).Count}else{0})
    copiedGameExecutable=$GameExecutable;sourceGameSha256=$knownGameHash;fakeExecutableExecuted=$false
    liveInstallationChanged=$false;liveGameLaunched=$false;nativeClockWritten=$false;fullMtaReady=$false;gameCopiesCreated=$gameCopiesCreated;configurationHistoricalMarkerFixturesOnly=$true
    scope='Actual install/update/set-mode/uninstall validation against one owned Build/tmp game copy. No game, DLL, GUI, native hook or network is executed.'
    bindingPersistenceOnly=$BindingPersistenceOnly.IsPresent
    nativeHostOnly=$NativeHostOnly.IsPresent
}
$resultPath=Join-Path $outputRoot 'test-result.json'
Save $resultPath $record
[ordered]@{success=$record.success;checks=$record.checks;path=$resultPath;sha256=(Sha $resultPath);failure=$failure} | ConvertTo-Json -Depth 4
if (-not $record.success) {exit 1}
