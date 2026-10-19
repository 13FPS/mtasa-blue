#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the app-frame opt-in using one ordinary, never-executed game copy.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ApplicationFramePackageDirectory,
    [Parameter(Mandatory)][string]$PlainPackageDirectory,
    [string]$PersistentPackageDirectory,
    [Parameter(Mandatory)][string]$GameExecutable
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $repositoryRoot ('Build/mta-application-frame-package/' + $id)
$scratchParent = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'Build/tmp'))
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $scratchParent ('application-frame-package-' + $id)))
$fakeGame = Join-Path $scratchRoot 'owned-fake-game'
$fixtureRoot = Join-Path $scratchRoot 'package-fixtures'
$privateSource = Join-Path $scratchRoot 'provider-source'
$utf8 = [Text.UTF8Encoding]::new($false)
$knownGameHash = '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'
$marker = 'MTA_CORE_APPLICATION_FRAME_REFERENCE'
$markerPattern = 'export const MTA_CORE_APPLICATION_FRAME_REFERENCE = (true|false);'
$checks = [Collections.Generic.List[object]]::new()
$runs = [Collections.Generic.List[object]]::new()
$backups = [Collections.Generic.List[object]]::new()
$inputPins = [Collections.Generic.List[object]]::new()
$sourcePins = [ordered]@{}
$failure = $null
$sourcesUnchanged = $false
$scratchRemoved = $false
$gameCopiesCreated = 0
New-Item -ItemType Directory -Path $outputRoot,$fixtureRoot -Force | Out-Null
$savedTemp = [Environment]::GetEnvironmentVariable('TEMP','Process')
$savedTmp = [Environment]::GetEnvironmentVariable('TMP','Process')
$env:TEMP = $scratchRoot
$env:TMP = $scratchRoot
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Save([string]$Path,$Value) {
    [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 20) + [Environment]::NewLine,$utf8)
}
function Check([string]$Name,[bool]$Passed,$Detail = $null) {
    $checks.Add([ordered]@{name=$Name;passed=$Passed;detail=$Detail})
    if (-not $Passed) { throw ('Check failed: ' + $Name) }
}
function No-Links([string]$Path) {
    $candidate = [IO.Path]::GetFullPath($Path)
    while ($candidate) {
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Linked fixture or input path refused.'
        }
        $candidate = [IO.Path]::GetDirectoryName($candidate)
    }
}
function Owned([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($scratchRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Fixture operation escaped this task scratch directory.'
    }
    No-Links $resolved
    $resolved
}
function Tree([string]$Directory) {
    $rows = @(Get-ChildItem -LiteralPath $Directory -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($Directory,$_.FullName).Replace('\','/')
        if ($_.PSIsContainer) { [ordered]@{path=$relative;kind='directory'} }
        else { [ordered]@{path=$relative;kind='file';bytes=$_.Length;sha256=(Sha $_.FullName)} }
    })
    # Empty parent directories are observable writes, too.
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
        if (-not $process.Start()) { throw 'Could not start package fixture.' }
        $outTask = $process.StandardOutput.ReadToEndAsync()
        $errorTask = $process.StandardError.ReadToEndAsync()
        $timeout = -not $process.WaitForExit(60000)
        if ($timeout) { $process.Kill($true); $process.WaitForExit() }
        $outPath = Join-Path $outputRoot ($Name + '.log')
        $errorPath = Join-Path $outputRoot ($Name + '-errors.log')
        [IO.File]::WriteAllText($outPath,$outTask.GetAwaiter().GetResult(),$utf8)
        [IO.File]::WriteAllText($errorPath,$errorTask.GetAwaiter().GetResult(),$utf8)
        $row = [ordered]@{name=$Name;script=$Script;scriptSha256=(Sha $Script);arguments=$Arguments;exitCode=$process.ExitCode;
            timedOut=$timeout;expectedSuccess=$ExpectSuccess;stdout=$outPath;stdoutSha256=(Sha $outPath);stderr=$errorPath;stderrSha256=(Sha $errorPath)}
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
function Package-Info([string]$Directory,[int]$Count,[string]$Name) {
    $resolved = (Resolve-Path -LiteralPath $Directory).Path
    No-Links $resolved
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
        $path = [IO.Path]::GetFullPath((Join-Path $sourceRoot $entry.path))
        Check ($Name + ':bounded-path:' + $entry.path) ($path.StartsWith($sourceRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))
        No-Links $path
        $hash = Sha $path
        Check ($Name + ':hash:' + $entry.path) ($hash -ceq $entry.sha256)
        $rows += [ordered]@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;sha256=$hash}
    }
    $inputPins.Add([ordered]@{name=$Name;directory=$resolved;manifest=$manifestPath;manifestSha256=(Sha $manifestPath);files=$rows})
    [ordered]@{root=$resolved;manifest=$manifest}
}
function Installed($Package,[string]$Name,[string]$Mode='Smoke') {
    $manifest = Get-Content -LiteralPath (Join-Path $fakeGame 'de-online-install-manifest.json') -Raw | ConvertFrom-Json
    Check ($Name + ':identity') ($manifest.packageId -ceq 'de-online-prototype' -and $manifest.formatVersion -eq 1 -and
        $manifest.gameDirectory -ceq $fakeGame -and $manifest.gameSha256 -ceq $knownGameHash -and $manifest.mode -ceq $Mode)
    Check ($Name + ':count') (@($manifest.files).Count -eq @($Package.files).Count -and @($manifest.files.path | Select-Object -Unique).Count -eq @($Package.files).Count)
    Check ($Name + ':paths') (@(Compare-Object @($manifest.files.path) @($Package.files.path)).Count -eq 0)
    foreach ($entry in $manifest.files) {
        $path = Owned (Join-Path $fakeGame $entry.path)
        Check ($Name + ':destination:' + $entry.path) ($entry.destination -ceq $path)
        Check ($Name + ':hash:' + $entry.path) ((Sha $path) -ceq $entry.sha256)
    }
    Save (Join-Path $outputRoot ($Name + '-installed-manifest.json')) $manifest
    $manifest
}
function Selection([string]$Directory) {
    [ordered]@{root=$Directory;manifest=(Get-Content -LiteralPath (Join-Path $Directory 'manifest.json') -Raw | ConvertFrom-Json)}
}
function Variant([string]$Name,$Source,[scriptblock]$Change) {
    # Reuse one ordinary package tree per owned-file count. Preserve each
    # changed config/manifest in the receipt directory before the next case.
    $target = Owned (Join-Path $fixtureRoot ('shared-' + @($Source.manifest.files).Count))
    if (-not (Test-Path -LiteralPath $target)) {
        New-Item -ItemType Directory -Path $target | Out-Null
        Copy-Item -LiteralPath (Join-Path $Source.root 'Win64') -Destination $target -Recurse
    }
    foreach ($entry in $Source.manifest.files) {
        $destination = Owned (Join-Path (Join-Path $target 'Win64') $entry.path)
        if (-not (Test-Path -LiteralPath $destination) -or (Sha $destination) -cne $entry.sha256) {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
            [IO.File]::Copy((Join-Path (Join-Path $Source.root 'Win64') $entry.path),$destination,$true)
        }
    }
    $config = Join-Path $target 'Win64/CLEO/de-online/config.mjs'
    $text = & $Change ([IO.File]::ReadAllText((Join-Path $Source.root 'Win64/CLEO/de-online/config.mjs')))
    [IO.File]::WriteAllText($config,$text,$utf8)
    $manifest = Get-Content -LiteralPath (Join-Path $Source.root 'manifest.json') -Raw | ConvertFrom-Json
    @($manifest.files | Where-Object path -CEQ 'CLEO/de-online/config.mjs')[0].sha256 = Sha $config
    Save (Join-Path $target 'manifest.json') $manifest
    [IO.File]::Copy($config,(Join-Path $outputRoot ($Name + '-config.mjs')),$true)
    Save (Join-Path $outputRoot ($Name + '-manifest.json')) $manifest
    Selection $target
}
function Mutate-InstalledConfig([string]$Text) {
    [IO.File]::WriteAllText($installedConfig,$Text,$utf8)
    $manifest = Get-Content -LiteralPath $installedManifest -Raw | ConvertFrom-Json
    @($manifest.files | Where-Object path -CEQ 'CLEO/de-online/config.mjs')[0].sha256 = Sha $installedConfig
    Save $installedManifest $manifest
}
function Exclusive([string]$Name,[string]$Expected='',[bool]$AllowAbsent=$false) {
    $text = [IO.File]::ReadAllText($installedConfig)
    $enabled = @([regex]::Matches($text,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;') | ForEach-Object {$_.Groups[1].Value})
    Check ($Name + ':exclusive-enabled-marker') (($Expected -ceq '' -and $enabled.Count -eq 0) -or
        ($Expected -cne '' -and $enabled.Count -eq 1 -and $enabled[0] -ceq $Expected)) $enabled
    $matches = [regex]::Matches($text,$markerPattern)
    $expectedValue = ($Expected -ceq $marker).ToString().ToLowerInvariant()
    Check ($Name + ':app-marker') (($matches.Count -eq 1 -and $matches[0].Groups[1].Value -ceq $expectedValue) -or ($AllowAbsent -and $matches.Count -eq 0))
}
function Export-Records([string]$Path) {
    # Inspect file bytes only. The fixture never loads providers or DllMain.
    No-Links $Path
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d) { throw 'Invalid provider DOS header.' }
    $pe = [uint64][BitConverter]::ToUInt32($bytes,0x3c)
    if ($pe + 24 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes,[int]$pe) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes,[int]$pe+4) -ne 0x8664) { throw 'Invalid AMD64 provider.' }
    $count = [BitConverter]::ToUInt16($bytes,[int]$pe+6)
    $optionalSize = [BitConverter]::ToUInt16($bytes,[int]$pe+20)
    $optional = [int]$pe + 24
    if ($optionalSize -lt 120 -or $optional + $optionalSize -gt $bytes.Length -or
        [BitConverter]::ToUInt16($bytes,$optional) -ne 0x20b -or [BitConverter]::ToUInt32($bytes,$optional+108) -lt 1 -or
        -not $count -or $count -gt 96) { throw 'Invalid provider optional header.' }
    $exportRva = [uint64][BitConverter]::ToUInt32($bytes,$optional+112)
    $exportSize = [uint64][BitConverter]::ToUInt32($bytes,$optional+116)
    $table = $optional + $optionalSize
    if ($table + 40*$count -gt $bytes.Length -or -not $exportRva -or $exportSize -lt 40) { throw 'Missing provider exports.' }
    $sections = @()
    for ($i=0; $i -lt $count; ++$i) {
        $at = $table + 40*$i
        $sections += @{rva=[uint64][BitConverter]::ToUInt32($bytes,$at+12);raw=[uint64][BitConverter]::ToUInt32($bytes,$at+20);size=[uint64][BitConverter]::ToUInt32($bytes,$at+16)}
    }
    $offset = {
        param([uint64]$Rva,[uint64]$Size)
        foreach ($section in $sections) {
            if ($Rva -ge $section.rva -and $Rva-$section.rva+$Size -le $section.size -and
                $section.raw+$Rva-$section.rva+$Size -le $bytes.Length) { return [int]($section.raw+$Rva-$section.rva) }
        }
        throw 'Provider export RVA is outside file sections.'
    }
    $directory = & $offset $exportRva 40
    $functionCount = [BitConverter]::ToUInt32($bytes,$directory+20)
    $nameCount = [BitConverter]::ToUInt32($bytes,$directory+24)
    if (-not $functionCount -or -not $nameCount -or $functionCount -gt 65536 -or $nameCount -gt 65536) { throw 'Invalid provider export counts.' }
    $functions = & $offset ([BitConverter]::ToUInt32($bytes,$directory+28)) (4*$functionCount)
    $names = & $offset ([BitConverter]::ToUInt32($bytes,$directory+32)) (4*$nameCount)
    $ordinals = & $offset ([BitConverter]::ToUInt32($bytes,$directory+36)) (2*$nameCount)
    $records = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    for ($i=0; $i -lt $nameCount; ++$i) {
        $ordinal = [BitConverter]::ToUInt16($bytes,$ordinals+2*$i)
        if ($ordinal -ge $functionCount) { throw 'Invalid provider export ordinal.' }
        $slot = $functions + 4*$ordinal
        $functionRva = [uint64][BitConverter]::ToUInt32($bytes,$slot)
        $nameRva = [uint64][BitConverter]::ToUInt32($bytes,$names+4*$i)
        $nameOffset = & $offset $nameRva 1
        $length = 0
        while ($length -lt 512 -and $nameOffset+$length -lt $bytes.Length -and $bytes[$nameOffset+$length]) { ++$length }
        if (-not $length -or $length -eq 512 -or $nameOffset+$length -ge $bytes.Length) { throw 'Invalid provider export name.' }
        & $offset $nameRva ($length+1) | Out-Null
        $name = [Text.Encoding]::ASCII.GetString($bytes,$nameOffset,$length)
        if ($records.ContainsKey($name)) { throw 'Duplicate provider export name.' }
        $records.Add($name,[ordered]@{nameOffset=$nameOffset;nameRva=$nameRva;slot=$slot;functionRva=$functionRva;
            exportRva=$exportRva;exportSize=$exportSize;concrete=($functionRva -ne 0 -and ($functionRva -lt $exportRva -or $functionRva -ge $exportRva+$exportSize))})
    }
    # Preserve the dictionary itself: Write-Output -NoEnumerate wraps generic
    # collections on PowerShell 7.5, which changes string indexing into a list.
    return ,$records
}
function Backup-Directories {
    $root = Join-Path $repositoryRoot 'Build/definitive-update-backups'
    if (Test-Path -LiteralPath $root) { @(Get-ChildItem -LiteralPath $root -Directory | ForEach-Object FullName) } else { @() }
}
function Transition([string]$Name,$Selection) {
    $prior = Get-Content -LiteralPath $installedManifest -Raw | ConvertFrom-Json
    $priorManifestHash = Sha $installedManifest
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
    $after = Installed $Selection.manifest ($Name+'-after')
    foreach ($row in $after.files) {
        $expected = @($Selection.manifest.files | Where-Object {$_.path -ceq $row.path})
        Check ($Name+':package-hash:'+$row.path) ($expected.Count -eq 1 -and $row.sha256 -ceq $expected[0].sha256)
    }
    $newBackups = @(Backup-Directories | Where-Object {$_ -notin $beforeBackups})
    Check ($Name+':one-new-backup') ($newBackups.Count -eq 1)
    $backup = $newBackups[0]
    Check ($Name+':original-manifest-backup') ((Sha (Join-Path $backup 'manifest.json')) -ceq $priorManifestHash)
    $savedOriginals = @()
    for ($i=0; $i -lt $changes.Count; ++$i) {
        $original = Join-Path $backup ($i.ToString()+'.original')
        if ($changes[$i].original) {
            Check ($Name+':ordered-original:'+$changes[$i].path) ((Test-Path -LiteralPath $original) -and (Sha $original) -ceq $changes[$i].sha256)
            $savedOriginals += [ordered]@{ownedPath=$changes[$i].path;path=$original;sha256=(Sha $original)}
        } else { Check ($Name+':addition-no-original:'+$changes[$i].path) (-not (Test-Path -LiteralPath $original)) }
    }
    Check ($Name+':exact-backup-count') (@(Get-ChildItem -LiteralPath $backup -File -Recurse).Count -eq $savedOriginals.Count+1)
    $removed = @($prior.files | Where-Object {$_.path -notin $after.files.path})
    $added = @($after.files | Where-Object {$_.path -notin $prior.files.path})
    Check ($Name+':complete-group-transition') (($removed.Count -eq 35 -and $added.Count -eq 0) -or
        ($removed.Count -eq 0 -and $added.Count -eq 35) -or ($removed.Count -eq 0 -and $added.Count -eq 0))
    foreach ($row in @($removed)+@($added)) {
        Check ($Name+':GUI-path:'+$row.path) ($row.path -match '^CLEO/de-online/(cgui\.dll|mta/cgui/|skins/Default/|mta/locale/en_US/)')
    }
    foreach ($row in $removed) { Check ($Name+':removed:'+$row.path) (-not (Test-Path -LiteralPath $row.destination)) }
    foreach ($old in $prior.files) {
        if ($old.path -notin $changes.path) { Check ($Name+':unchanged-owned:'+$old.path) ((Sha $old.destination) -ceq $old.sha256) }
    }
    Check ($Name+':unowned-preserved') ((Sha $unowned) -ceq $unownedHash)
    Check ($Name+':game-preserved') ((Sha (Join-Path $fakeGame 'SanAndreas.exe')) -ceq $knownGameHash)
    $backups.Add([ordered]@{name=$Name;directory=$backup;originalManifestSha256=$priorManifestHash;orderedChanges=$changes;savedOriginals=$savedOriginals})
}
function App-On([string]$Name) {
    Run $Name $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference') $true | Out-Null
    Installed $application.manifest $Name | Out-Null
    Exclusive $Name $marker
}
function Change-Flag([string]$Text,[string]$Name) {
    $pattern = 'export const ' + [regex]::Escape($Name) + ' = false;'
    if ([regex]::Matches($Text,$pattern).Count -ne 1) { throw ('Missing disabled fixture marker: ' + $Name) }
    [regex]::Replace($Text,$pattern,('export const '+$Name+' = true;'))
}
try {
    # The production installer deliberately rejects any running GTA process,
    # even when its destination is this isolated fixture. Check before copying
    # the game so that this refusal cannot mask malformed-package assertions.
    Check 'GTA-closed-for-production-installer-guard' (@(Get-Process -Name SanAndreas,PlayGTASanAndreas -ErrorAction SilentlyContinue).Count -eq 0)
    foreach ($name in @('Client/definitive/install.ps1','Client/definitive/update.ps1','Client/definitive/uninstall.ps1',
        'Client/definitive/set-mode.ps1','Client/definitive/package.ps1','Tests/definitive/test_application_frame_package.ps1',
        'Client/definitive/game-adapter/index.js','Client/definitive/game-adapter/controller.mjs',
        'Client/definitive/game-adapter/native-pad-reference.mjs','Client/definitive/game-adapter/native-movement-trial.mjs',
        'Client/definitive/game-adapter/config.mjs','Client/definitive/game-adapter/mod.json')) {
        $sourcePins[$name] = Sha (Join-Path $repositoryRoot $name)
    }
    $GameExecutable = (Resolve-Path -LiteralPath $GameExecutable).Path
    No-Links $GameExecutable
    Check 'source-game-exact-fingerprint' ((Sha $GameExecutable) -ceq $knownGameHash)
    $application = Package-Info $ApplicationFramePackageDirectory 23 'application-package'
    $plain = Package-Info $PlainPackageDirectory 23 'plain-package'
    $persistent = if ($PersistentPackageDirectory) { Package-Info $PersistentPackageDirectory 58 'persistent-package' } else { $null }
    $appText = [IO.File]::ReadAllText((Join-Path $application.root 'Win64/CLEO/de-online/config.mjs'))
    $plainText = [IO.File]::ReadAllText((Join-Path $plain.root 'Win64/CLEO/de-online/config.mjs'))
    $trueMarkers = @([regex]::Matches($appText,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;') | ForEach-Object {$_.Groups[1].Value})
    Check 'application-package-exclusive' ($trueMarkers.Count -eq 1 -and $trueMarkers[0] -ceq $marker -and [regex]::Matches($appText,$markerPattern).Count -eq 1)
    Check 'plain-package-no-enabled-trials' ([regex]::Matches($plainText,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;').Count -eq 0)
    Check 'plain-marker-disabled-or-historical' ([regex]::Matches($plainText,('export const '+$marker+' =')).Count -eq 0 -or
        ([regex]::Matches($plainText,$markerPattern).Count -eq 1 -and $plainText -match ('export const '+$marker+' = false;')))
    $install = Join-Path $repositoryRoot 'Client/definitive/install.ps1'
    $update = Join-Path $repositoryRoot 'Client/definitive/update.ps1'
    $setMode = Join-Path $repositoryRoot 'Client/definitive/set-mode.ps1'
    $uninstall = Join-Path $repositoryRoot 'Client/definitive/uninstall.ps1'
    $packageScript = Join-Path $repositoryRoot 'Client/definitive/package.ps1'
    $installedConfig = Join-Path $fakeGame 'CLEO/de-online/config.mjs'
    $installedManifest = Join-Path $fakeGame 'de-online-install-manifest.json'
    $ownedGame = Owned $fakeGame
    Check 'fake-game-starts-absent' (-not (Test-Path -LiteralPath $ownedGame))
    New-Item -ItemType Directory -Path $ownedGame | Out-Null
    # All cases share this one ordinary file copy. Never execute it, link it,
    # or delete/recreate the game fixture between malformed-package cases.
    [IO.File]::Copy($GameExecutable,(Join-Path $ownedGame 'SanAndreas.exe'),$false)
    ++$gameCopiesCreated
    Check 'fake-game-exact-fingerprint' ((Sha (Join-Path $ownedGame 'SanAndreas.exe')) -ceq $knownGameHash)

    $privatePackage = Owned (Join-Path $privateSource 'Client/definitive/package.ps1')
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($privatePackage)) | Out-Null
    [IO.File]::Copy($packageScript,$privatePackage,$false)
    foreach ($name in @('index.js','controller.mjs','native-pad-reference.mjs','native-movement-trial.mjs','config.mjs','mod.json')) {
        $destination = Owned (Join-Path $privateSource ('Client/definitive/game-adapter/'+$name))
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        [IO.File]::Copy((Join-Path $repositoryRoot ('Client/definitive/game-adapter/'+$name)),$destination,$false)
    }
    $providers = @(
        @{name='bridge';package='CLEO/de-online/de_client_bridge.dll';source='Build/definitive-online/de_client_bridge.dll';exports=@('DE_CoreApplicationFrameApiVersion','DE_CoreApplicationFrameBegin','DE_CoreApplicationFrameCapture','DE_CoreApplicationFrameRead')},
        @{name='core';package='CLEO/de-online/core.dll';source='Bin/mta/x64/core.dll';exports=@('MTA_DEApplicationFrameReferenceApiVersion','MTA_ConfigureDEApplicationFrameReference')},
        @{name='backend';package='CLEO/de-online/game_de.dll';source='Bin/mta/game_de.dll';exports=@('BeginGameApplicationFrameObservationV1','ReadGameApplicationFrameObservationV1')}
    )
    foreach ($provider in $providers) {
        $source = Join-Path (Join-Path $application.root 'Win64') $provider.package
        $provider['path'] = Owned (Join-Path $privateSource $provider.source)
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($provider.path)) | Out-Null
        [IO.File]::Copy($source,$provider.path,$false)
        $provider['sha256'] = Sha $provider.path
        $provider['records'] = Export-Records $provider.path
        foreach ($name in $provider.exports) {
            Check ('concrete-provider:'+ $provider.name+':'+$name) ($provider.records.ContainsKey($name) -and $provider.records[$name].concrete)
        }
    }
    $privateOutput = Owned (Join-Path $privateSource 'Build/must-not-stage')
    foreach ($provider in $providers) {
        $original = [IO.File]::ReadAllBytes($provider.path)
        foreach ($name in $provider.exports) {
            $row = $provider.records[$name]
            $bytes = [byte[]]$original.Clone()
            $bytes[$row.nameOffset] = [byte][char]'X'
            $caseName = 'missing-export-'+$provider.name+'-'+$name
            try {
                [IO.File]::WriteAllBytes($provider.path,$bytes)
                $before = Tree $fakeGame
                $run = Run $caseName $privatePackage @('-Mode','Smoke','-MtaCoreApplicationFrameReference','-OutputDirectory',$privateOutput) $false
                Check ($caseName+':exact-export-refusal') ([IO.File]::ReadAllText($run.stderr).Contains('Application-frame provider lacks export: '+$name))
                Check ($caseName+':no-package-staged') (-not (Test-Path -LiteralPath $privateOutput))
                Check ($caseName+':game-tree-unchanged') ($before.canonical -ceq (Tree $fakeGame).canonical)
            } finally { [IO.File]::WriteAllBytes($provider.path,$original) }
            Check ($caseName+':provider-restored') ((Sha $provider.path) -ceq $provider.sha256)
        }
        # A function RVA inside the export-directory range must not count as
        # a concrete implementation. This is a file fixture, not a DLL load.
        $name = $provider.exports[0]
        $row = $provider.records[$name]
        $bytes = [byte[]]$original.Clone()
        [BitConverter]::GetBytes([uint32]$row.exportRva).CopyTo($bytes,[int]$row.slot)
        $caseName = 'forwarded-range-'+$provider.name
        try {
            [IO.File]::WriteAllBytes($provider.path,$bytes)
            $run = Run $caseName $privatePackage @('-Mode','Smoke','-MtaCoreApplicationFrameReference','-OutputDirectory',$privateOutput) $false
            Check ($caseName+':exact-export-refusal') ([IO.File]::ReadAllText($run.stderr).Contains('Application-frame provider lacks export: '+$name))
            Check ($caseName+':no-package-staged') (-not (Test-Path -LiteralPath $privateOutput))
        } finally { [IO.File]::WriteAllBytes($provider.path,$original) }
        Check ($caseName+':provider-restored') ((Sha $provider.path) -ceq $provider.sha256)
    }

    $badConfigs = [ordered]@{
        'duplicate-app' = {param($text) $text + [Environment]::NewLine + 'export const MTA_CORE_APPLICATION_FRAME_REFERENCE = true;' + [Environment]::NewLine}
        'nonboolean-app' = {param($text) $text.Replace('export const MTA_CORE_APPLICATION_FRAME_REFERENCE = true;','export const MTA_CORE_APPLICATION_FRAME_REFERENCE = 1;')}
        'string-app' = {param($text) $text.Replace('export const MTA_CORE_APPLICATION_FRAME_REFERENCE = true;','export const MTA_CORE_APPLICATION_FRAME_REFERENCE = "true";')}
        'mixed-clock-read' = {param($text) Change-Flag $text 'MTA_CORE_CLOCK_REFERENCE'}
        'mixed-clock-set' = {param($text) Change-Flag $text 'MTA_CORE_CLOCK_SET_REFERENCE'}
        'mixed-input' = {param($text) Change-Flag $text 'MTA_CORE_INPUT_REFERENCE'}
        'mixed-gui' = {param($text) Change-Flag $text 'MTA_CORE_GUI_STARTUP_REFERENCE'}
        'mixed-frontend' = {param($text) Change-Flag $text 'MTA_CORE_FRONTEND_RUNTIME'}
        'mixed-gait' = {param($text) Change-Flag $text 'GAIT_REFERENCE'}
        'mixed-future' = {param($text) $text + [Environment]::NewLine + 'export const MTA_FUTURE_UNREVIEWED_REFERENCE = true;' + [Environment]::NewLine}
    }
    foreach ($name in $badConfigs.Keys) {
        $variant = Variant ('install-'+$name) $application $badConfigs[$name]
        Refuse ('install-'+$name) $install @('-PackageDirectory',$variant.root,'-GameDirectory',$fakeGame)
    }
    Run 'install-application' $install @('-PackageDirectory',$application.root,'-GameDirectory',$fakeGame) $true | Out-Null
    Installed $application.manifest 'install-application' | Out-Null
    Exclusive 'install-application' $marker
    $unowned = Owned (Join-Path $fakeGame 'player-owned-sentinel.txt')
    [IO.File]::WriteAllText($unowned,'Fixture-owned unowned sentinel.'+[Environment]::NewLine,$utf8)
    $unownedHash = Sha $unowned
    foreach ($name in $badConfigs.Keys) {
        $variant = Variant ('update-'+$name) $application $badConfigs[$name]
        Refuse ('update-'+$name) $update @('-PackageDirectory',$variant.root,'-GameDirectory',$fakeGame)
    }

    foreach ($mode in @('Smoke','Diagnostic','Online')) {
        Run ('ordinary-'+$mode) $setMode @('-GameDirectory',$fakeGame,'-Mode',$mode) $true | Out-Null
        Installed $application.manifest ('ordinary-'+$mode) $mode | Out-Null
        Exclusive ('ordinary-'+$mode)
        App-On ('reenable-after-'+$mode)
    }
    foreach ($selection in @(@{switch='MtaCoreClockReference';marker='MTA_CORE_CLOCK_REFERENCE'},@{switch='MtaCoreClockSetReference';marker='MTA_CORE_CLOCK_SET_REFERENCE'})) {
        Run ('switch-'+$selection.switch) $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke',('-'+$selection.switch)) $true | Out-Null
        Exclusive ('switch-'+$selection.switch) $selection.marker
        App-On ('app-after-'+$selection.switch)
    }
    foreach ($conflict in @('MtaCoreClockReference','MtaCoreClockSetReference','MtaCoreInputReference','MtaCoreFrontendRuntime','NativeGaitReference')) {
        Refuse ('mode-conflict-'+$conflict) $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference',('-'+$conflict))
        $out = Owned (Join-Path $scratchRoot ('Build-conflict-'+$conflict))
        Run ('package-conflict-'+$conflict) $packageScript @('-Mode','Smoke','-MtaCoreApplicationFrameReference',('-'+$conflict),'-OutputDirectory',$out) $false | Out-Null
        Check ('package-conflict-no-output-'+$conflict) (-not (Test-Path -LiteralPath $out))
    }
    foreach ($mode in @('Diagnostic','Online')) {
        Refuse ('app-refuses-'+$mode) $setMode @('-GameDirectory',$fakeGame,'-Mode',$mode,'-MtaCoreApplicationFrameReference')
        Run ('package-refuses-'+$mode) $packageScript @('-Mode',$mode,'-MtaCoreApplicationFrameReference','-OutputDirectory',$privateOutput) $false | Out-Null
        Check ('package-refuses-'+$mode+':no-output') (-not (Test-Path -LiteralPath $privateOutput))
    }
    Refuse 'mode-explicit-render-route' $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference','-MtaCoreRenderRoute','D3D12')
    Run 'package-explicit-render-route' $packageScript @('-Mode','Smoke','-MtaCoreApplicationFrameReference','-MtaCoreRenderRoute','D3D12','-OutputDirectory',$privateOutput) $false | Out-Null
    Check 'package-explicit-render-route:no-output' (-not (Test-Path -LiteralPath $privateOutput))

    $configBytes = [IO.File]::ReadAllBytes($installedConfig)
    $manifestBytes = [IO.File]::ReadAllBytes($installedManifest)
    foreach ($name in @('duplicate-app','nonboolean-app','string-app','mixed-clock-read','mixed-clock-set')) {
        try {
            Mutate-InstalledConfig (& $badConfigs[$name] ([Text.Encoding]::UTF8.GetString($configBytes)))
            Refuse ('installed-explicit-'+$name) $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference')
            Refuse ('installed-ordinary-'+$name) $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke')
        } finally {
            [IO.File]::WriteAllBytes($installedConfig,$configBytes)
            [IO.File]::WriteAllBytes($installedManifest,$manifestBytes)
        }
    }
    try {
        $manifest = Get-Content -LiteralPath $installedManifest -Raw | ConvertFrom-Json
        $manifest.files[0].destination = Join-Path $scratchRoot 'outside-game.txt'
        Save $installedManifest $manifest
        Refuse 'unsafe-installed-destination-mode' $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference')
        Refuse 'unsafe-installed-destination-update' $update @('-PackageDirectory',$application.root,'-GameDirectory',$fakeGame)
    } finally { [IO.File]::WriteAllBytes($installedManifest,$manifestBytes) }
    $ownedConfig = Join-Path $fakeGame 'CLEO/de-online/mta/config/coreconfig.xml'
    $ownedConfigBytes = [IO.File]::ReadAllBytes($ownedConfig)
    try {
        [IO.File]::AppendAllText($ownedConfig,[Environment]::NewLine+'<!-- fixture edited owned bytes -->',$utf8)
        Refuse 'edited-owned-config-mode' $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference')
        Refuse 'edited-owned-config-uninstall' $uninstall @('-GameDirectory',$fakeGame)
    } finally { [IO.File]::WriteAllBytes($ownedConfig,$ownedConfigBytes) }

    $normalizedPlain = Variant 'plain-current' $plain {param($text)
        if ([regex]::Matches($text,'export const MTA_CORE_APPLICATION_FRAME_REFERENCE =').Count -eq 0) {
            $text + [Environment]::NewLine + 'export const MTA_CORE_APPLICATION_FRAME_REFERENCE = false;' + [Environment]::NewLine
        } else { $text }
    }
    Transition 'application-to-plain' $normalizedPlain
    Exclusive 'application-to-plain'
    $historical = Variant 'plain-historical' $plain {param($text)
        [regex]::Replace($text,'(?m)^export const MTA_CORE_APPLICATION_FRAME_REFERENCE = (true|false);\r?\n?','')
    }
    Transition 'plain-to-historical' $historical
    Run 'historical-ordinary-mode' $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke') $true | Out-Null
    Exclusive 'historical-ordinary-mode' '' $true
    Check 'historical-marker-stays-absent' ([regex]::Matches([IO.File]::ReadAllText($installedConfig),'export const MTA_CORE_APPLICATION_FRAME_REFERENCE =').Count -eq 0)
    Refuse 'historical-explicit-app-refused' $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference')
    Transition 'historical-to-application' $application
    Exclusive 'historical-to-application' $marker
    if ($null -ne $persistent) {
        Transition 'application-to-persistent' $persistent
        Refuse 'persistent58-cannot-arm-app' $setMode @('-GameDirectory',$fakeGame,'-Mode','Smoke','-MtaCoreApplicationFrameReference')
        $wrongCount = Variant 'app-marker-on58' $persistent {param($text)
            $text = [regex]::Replace($text,'export const ((?:GAIT_|INPUT_|MTA_)[A-Z0-9_]+) = true;','export const $1 = false;')
            if ([regex]::Matches($text,'export const MTA_CORE_APPLICATION_FRAME_REFERENCE =').Count -eq 0) {
                $text + [Environment]::NewLine + 'export const MTA_CORE_APPLICATION_FRAME_REFERENCE = true;' + [Environment]::NewLine
            } else { $text.Replace('export const MTA_CORE_APPLICATION_FRAME_REFERENCE = false;','export const MTA_CORE_APPLICATION_FRAME_REFERENCE = true;') }
        }
        Refuse 'update-app-requires23' $update @('-PackageDirectory',$wrongCount.root,'-GameDirectory',$fakeGame)
        Transition 'persistent-to-application' $application
        Exclusive 'persistent-to-application' $marker
    }
    Run 'uninstall-validate-only' $uninstall @('-GameDirectory',$fakeGame,'-ValidateOnly') $true | Out-Null
    Save (Join-Path $outputRoot 'before-uninstall-tree.json') (Tree $fakeGame).rows
    Run 'uninstall-application' $uninstall @('-GameDirectory',$fakeGame) $true | Out-Null
    Check 'uninstall-owned-manifest-removed' (-not (Test-Path -LiteralPath $installedManifest))
    Check 'uninstall-owned-loader-removed' (-not (Test-Path -LiteralPath (Join-Path $fakeGame 'version.dll')))
    Check 'uninstall-game-preserved' ((Sha (Join-Path $fakeGame 'SanAndreas.exe')) -ceq $knownGameHash)
    Check 'uninstall-unowned-preserved' ((Sha $unowned) -ceq $unownedHash)
    Check 'single-ordinary-game-copy' ($gameCopiesCreated -eq 1)
} catch { $failure = $_.Exception.ToString() + [Environment]::NewLine + $_.ScriptStackTrace }
finally {
    try {
        $sourcesUnchanged = $true
        foreach ($name in $sourcePins.Keys) {
            if ((Sha (Join-Path $repositoryRoot $name)) -cne $sourcePins[$name]) { $sourcesUnchanged = $false }
        }
        foreach ($inputPackage in $inputPins) {
            if ((Sha $inputPackage.manifest) -cne $inputPackage.manifestSha256) { $sourcesUnchanged = $false }
            foreach ($entry in $inputPackage.files) {
                if ((Sha $entry.path) -cne $entry.sha256) { $sourcesUnchanged = $false }
            }
        }
        if ($GameExecutable -and (Test-Path -LiteralPath $GameExecutable) -and (Sha $GameExecutable) -cne $knownGameHash) { $sourcesUnchanged = $false }
        if (Test-Path -LiteralPath $scratchRoot) {
            $cleanupPath = [IO.Path]::GetFullPath($scratchRoot)
            if (-not $cleanupPath.StartsWith($scratchParent + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch cleanup escaped Build/tmp.' }
            No-Links $cleanupPath
            foreach ($entry in Get-ChildItem -LiteralPath $cleanupPath -Recurse -Force) {
                if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Scratch cleanup contains a link.' }
            }
            Remove-Item -LiteralPath $cleanupPath -Recurse -Force
        }
        $scratchRemoved = -not (Test-Path -LiteralPath $scratchRoot)
    } catch { if ($null -eq $failure) { $failure = $_.Exception.ToString() } }
    [Environment]::SetEnvironmentVariable('TEMP',$savedTemp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$savedTmp,'Process')
    $success = $null -eq $failure -and $sourcesUnchanged -and $scratchRemoved
    $receipt = [ordered]@{schema='mta_application_frame_package_ownership_tests_v1';success=$success;failure=$failure;
        repositoryRoot=$repositoryRoot;outputDirectory=$outputRoot;scratchDirectory=$scratchRoot;scratchRemoved=$scratchRemoved;
        sourceGame=$GameExecutable;sourceGameSha256=$knownGameHash;gameCopiesCreated=$gameCopiesCreated;copyMethod='ordinary-file-copy-no-hardlinks';
        sourcesUnchanged=$sourcesUnchanged;sourcePins=$sourcePins;inputPackages=$inputPins;checks=$checks;runs=$runs;backups=$backups;
        gameExecuted=$false;providersLoaded=$false;liveGameModified=$false;fullMtaReady=$false}
    Save (Join-Path $outputRoot 'result.json') $receipt
    Write-Host ('Application-frame package regression result: ' + (Join-Path $outputRoot 'result.json'))
}
if (-not $success) { throw ('Application-frame package regression failed: ' + $failure) }
Write-Host ('Passed ' + $checks.Count + ' application-frame package checks')
