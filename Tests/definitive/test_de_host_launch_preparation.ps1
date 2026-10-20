#!/usr/bin/env pwsh
#requires -Version 7.0
# Validate the real launcher preparation boundary. This suite never dispatches
# Steam: the positive case calls only the normal validator's preparation export.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameExe,
    [Parameter(Mandatory)][string]$PackageDirectory,
    [string]$LauncherExe,
    [string]$PreflightDll
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if(-not $LauncherExe){$LauncherExe=Join-Path $root 'Bin/mta-launcher/x64/Multi Theft Auto.exe'}
if(-not $PreflightDll){$PreflightDll=Join-Path $root 'Bin/mta-launcher/x64/mta/de_preflight.dll'}
function Sha([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$game=(Resolve-Path -LiteralPath $GameExe).Path
$installed=Split-Path $game
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$packageManifest=Join-Path $package 'manifest.json'
$packageText=[IO.File]::ReadAllText($packageManifest)
$packageRecord=$packageText|ConvertFrom-Json
if($packageRecord.files.Count -notin @(60,64)){throw 'Host preparation requires a complete native 60/64-file package.'}
if((Sha $game) -ne '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'){throw 'Unsupported game input.'}
if(Get-Process -Name SanAndreas -ErrorAction SilentlyContinue){throw 'GTA must be closed; this suite never starts or stops it.'}
$runId=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-de-host-launch/'+$runId)
$scratch=Join-Path $root ('Build/tmp/de-host-launch-'+$runId)
$hostRoot=Join-Path $output 'owned preparation host'
$fixture=Join-Path $scratch 'Steam library/steamapps/common/Owned GTA DE/Gameface/Binaries/Win64'
$fixtureManifest=Join-Path $scratch 'package.json'
$fixtureReceipt=Join-Path $fixture 'de-online-install-manifest.json'
$utf8=[Text.UTF8Encoding]::new($false)
[IO.Directory]::CreateDirectory((Join-Path $hostRoot 'mta'))|Out-Null
[IO.Directory]::CreateDirectory($fixture)|Out-Null
[IO.Directory]::CreateDirectory((Join-Path $scratch 'compiler'))|Out-Null
# A coherent copied appmanifest is still insufficient to authorize Steam
# dispatch. Supply this owned metadata so the negative case reaches the real
# registered-library check instead of failing earlier on a missing file.
$fixtureAppManifest=Join-Path $scratch 'Steam library/steamapps/appmanifest_1547000.acf'
[IO.File]::WriteAllText($fixtureAppManifest,'"AppState" { "appid" "1547000" "StateFlags" "4" "installdir" "Owned GTA DE" }'+"`n",$utf8)
$inputHashes=[ordered]@{}
foreach($path in @($game,(Join-Path $installed 'de-online-install-manifest.json'),$packageManifest,$LauncherExe,$PreflightDll)){$inputHashes[$path]=Sha $path}
foreach($entry in $packageRecord.files){
    $source=Join-Path $package ('Win64/'+$entry.path)
    if((Sha $source) -ne $entry.sha256 -or (Sha (Join-Path $installed $entry.path)) -ne $entry.sha256){throw 'Positive read-only preparation requires the package to match the actual installed native owner.'}
    $inputHashes[(Join-Path $installed $entry.path)]=$entry.sha256
    $destination=Join-Path $fixture $entry.path
    [IO.Directory]::CreateDirectory((Split-Path $destination))|Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
}
Copy-Item -LiteralPath $game -Destination (Join-Path $fixture 'SanAndreas.exe')
Copy-Item -LiteralPath $LauncherExe -Destination (Join-Path $hostRoot 'Multi Theft Auto.exe')
Copy-Item -LiteralPath $PreflightDll -Destination (Join-Path $hostRoot 'mta/de_preflight.dll')
[IO.File]::WriteAllText($fixtureManifest,$packageText,$utf8)
$receipt=[ordered]@{packageId='de-online-prototype';formatVersion=1;mode=$packageRecord.mode;gameDirectory=$fixture;gameSha256=$packageRecord.gameSha256;files=@(
    foreach($entry in $packageRecord.files){[ordered]@{path=$entry.path;destination=(Join-Path $fixture $entry.path);sha256=$entry.sha256}}
)}
$receiptText=($receipt|ConvertTo-Json -Depth 12)+"`n"
[IO.File]::WriteAllText($fixtureReceipt,$receiptText,$utf8)
$checks=0
$cases=[Collections.Generic.List[object]]::new()
function Check([bool]$condition,[string]$label){$script:checks++;if(-not $condition){throw $label}}
function Invoke-Case([string]$name,[string]$executable,[string[]]$arguments,[int]$exit,[string]$status,[bool]$ready=$false){
    $start=[Diagnostics.ProcessStartInfo]::new($executable)
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.WorkingDirectory=$scratch
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$start
    Check ($process.Start()) "$name did not start the owned host."
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    if(-not $process.WaitForExit(15000)){$process.Kill();$process.WaitForExit();throw "$name preparation blocked."}
    $text=$stdout.GetAwaiter().GetResult();$errors=$stderr.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $output ($name+'.stdout.json')),$text,$utf8)
    [IO.File]::WriteAllText((Join-Path $output ($name+'.stderr.log')),$errors,$utf8)
    Check ($process.ExitCode -eq $exit) "$name unexpected exit: $text $errors"
    Check ([string]::IsNullOrWhiteSpace($errors)) "$name emitted stderr."
    $record=$text|ConvertFrom-Json
    Check ($record.status -ceq $status) "$name unexpected status: $text"
    Check ($record.preflightValid -eq $ready -and $record.hostLaunchReady -eq $ready) "$name wrong preparation readiness."
    Check (-not $record.hostLaunchRequested -and -not $record.launchReady -and -not $record.fullMtaReady -and -not $record.gameLaunched -and -not $record.coreLoaded -and -not $record.installationWrites) "$name crossed the preparation-only boundary."
    Check ($record.verifiedFiles -eq $(if($ready){$packageRecord.files.Count}else{0})) "$name wrong verified count."
    $script:cases.Add([ordered]@{name=$name;exit_code=$process.ExitCode;record=$record})
}
$sourceNames=@('Client/launch/Main.cpp','Client/launch/StdInc.cpp','Client/launch/StdInc.h','Client/launch/premake5.lua',
    'Client/launch/DefinitivePreflight/Loader.cpp','Client/launch/DefinitivePreflight/Preflight.h','Client/launch/DefinitivePreflight/SteamInstall.h',
    'Client/launch/DefinitivePreflight/StrictJson.c','Client/launch/DefinitivePreflight/premake5.lua',
    'Tests/definitive/DeHostLaunchPreparationTests.cpp','Tests/definitive/test_de_host_launch_preparation.ps1')
$sourceHashes=[ordered]@{};foreach($name in $sourceNames){$sourceHashes[$name]=Sha (Join-Path $root $name)}
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
$hold=$null
try{
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
    if($LASTEXITCODE -ne 0){throw 'Host fixture compiler environment failed.'}
    foreach($entry in $environment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $env:TEMP=Join-Path $scratch 'compiler';$env:TMP=$env:TEMP
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $hostExecutable=Join-Path $hostRoot 'PrepareHost.exe'
    & $compiler /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX (Join-Path $PSScriptRoot 'DeHostLaunchPreparationTests.cpp') ("/Fo"+(Join-Path $scratch 'compiler/host.obj')) ("/Fe"+$hostExecutable) /link /NOLOGO shell32.lib 2>&1 | Set-Content -LiteralPath (Join-Path $output 'host-build.log') -Encoding utf8
    if($LASTEXITCODE -ne 0){throw 'Host preparation fixture compilation failed.'}
    $parser=& $hostExecutable --steam-parser-tests
    [IO.File]::WriteAllText((Join-Path $output 'steam-parser-tests.json'),($parser -join "`n")+"`n",$utf8)
    if($LASTEXITCODE -ne 0){throw 'Pure Steam locator checks failed.'}
    $parserRecord=$parser|ConvertFrom-Json
    Check ($parserRecord.success -and $parserRecord.parserChecks -ge 15 -and -not $parserRecord.hostLaunchRequested -and -not $parserRecord.gameLaunched) 'Pure Steam parser check record is incomplete.'
    $realArguments=@('--de-launch','--game-exe',$game,'--bundle',$installed,'--manifest',$packageManifest)
    Invoke-Case 'actual-closed-installation-prepared-without-dispatch' $hostExecutable $realArguments 0 'host_launch_prepared' $true
    $launcher=Join-Path $hostRoot 'Multi Theft Auto.exe'
    Invoke-Case 'actual-launcher-missing-arguments' $launcher @('--de-launch') 4 'preflight_arguments_required'
    Invoke-Case 'actual-launcher-mixed-selection-refused' $launcher (@('--de-preflight')+$realArguments) 4 'preflight_arguments_required'
    $fixtureArguments=@('--de-launch','--game-exe',(Join-Path $fixture 'SanAndreas.exe'),'--bundle',$fixture,'--manifest',$fixtureManifest)
    Invoke-Case 'copied-install-not-registered-with-steam' $launcher $fixtureArguments 4 'steam_library_not_registered'
    function Receipt-Case([string]$name,[scriptblock]$mutate,[string]$status){
        $record=$receiptText|ConvertFrom-Json
        & $mutate $record
        [IO.File]::WriteAllText($fixtureReceipt,($record|ConvertTo-Json -Depth 12)+"`n",$utf8)
        Invoke-Case $name $launcher $fixtureArguments 4 $status
        [IO.File]::WriteAllText($fixtureReceipt,$receiptText,$utf8)
    }
    Receipt-Case 'wrong-installed-directory' {param($r)$r.gameDirectory=Split-Path $fixture} 'installed_manifest_identity_invalid'
    Receipt-Case 'wrong-installed-mode' {param($r)$r.mode='Online'} 'installed_manifest_identity_invalid'
    Receipt-Case 'missing-installed-path' {param($r)$r.files=@($r.files|Select-Object -Skip 1)} 'installed_owned_group_mismatch'
    Receipt-Case 'wrong-installed-destination' {param($r)$r.files[0].destination=Join-Path $scratch 'unowned.dll'} 'installed_owned_path_mismatch'
    Receipt-Case 'wrong-installed-hash' {param($r)$r.files[0].sha256='0'*64} 'installed_owned_path_mismatch'
    Receipt-Case 'duplicate-installed-path' {param($r)$r.files[1]=$r.files[0]} 'installed_owned_path_mismatch'
    Receipt-Case 'unknown-installed-field' {param($r)$r|Add-Member -NotePropertyName unowned -NotePropertyValue $true} 'manifest_unknown_field'
    $config=Join-Path $fixture 'CLEO/de-online/config.mjs';$ini=Join-Path $fixture 'MtaDefinitive.ini'
    $configText=[IO.File]::ReadAllText($config);$iniText=[IO.File]::ReadAllText($ini)
    [IO.File]::WriteAllText($config,$configText.Replace('export const MTA_CORE_FRONTEND_NATIVE_HOST = true;','export const MTA_CORE_FRONTEND_NATIVE_HOST = false;'),$utf8)
    [IO.File]::WriteAllText($ini,$iniText.Replace('Enabled=1','Enabled=0'),$utf8)
    $disarmed=$packageText|ConvertFrom-Json
    foreach($entry in $disarmed.files){if($entry.path -in @('CLEO/de-online/config.mjs','MtaDefinitive.ini')){$entry.sha256=Sha (Join-Path $fixture $entry.path)}}
    [IO.File]::WriteAllText($fixtureManifest,($disarmed|ConvertTo-Json -Depth 12)+"`n",$utf8)
    Invoke-Case 'owned-but-disarmed-native-host' $launcher $fixtureArguments 4 'enabled_native_frontend_required'
    [IO.File]::WriteAllText($config,$configText,$utf8);[IO.File]::WriteAllText($ini,$iniText,$utf8);[IO.File]::WriteAllText($fixtureManifest,$packageText,$utf8)
    $holdPath=Join-Path $hostRoot 'SanAndreas.exe'
    Copy-Item -LiteralPath $hostExecutable -Destination $holdPath
    $hold=Start-Process -FilePath $holdPath -ArgumentList '--hold' -PassThru -WindowStyle Hidden
    Invoke-Case 'already-running-name-refused-without-touching-game' $launcher $realArguments 4 'game_already_running'
    $hold.Kill();$hold.WaitForExit();$hold.Dispose();$hold=$null
    Invoke-Case 'actual-install-still-prepares-without-dispatch' $hostExecutable $realArguments 0 'host_launch_prepared' $true
    foreach($name in $inputHashes.Keys){Check ((Sha $name) -eq $inputHashes[$name]) 'Actual game/package/launcher input changed during preparation-only checks.'}
    foreach($name in $sourceHashes.Keys){Check ((Sha (Join-Path $root $name)) -eq $sourceHashes[$name]) 'Preparation source changed during focused validation.'}
    $receiptPath=Join-Path $output 'test-result.json'
    $finalRecord=[ordered]@{success=$true;checks=$checks;cases=$cases;parserChecks=$parserRecord.parserChecks;hostLaunchRequested=$false;gameLaunched=$false;
        sources=$sourceHashes;inputs=$inputHashes;host=[ordered]@{path=$hostExecutable;sha256=(Sha $hostExecutable)};scratch=$scratch;scratch_cleaned=$true}
}finally{
    if($hold -and -not $hold.HasExited){$hold.Kill();$hold.WaitForExit();$hold.Dispose()}
    $env:TEMP=$oldTemp;$env:TMP=$oldTmp
    $resolved=[IO.Path]::GetFullPath($scratch)
    $allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp')).TrimEnd('\')+'\'
    if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch cleanup left workspace.'}
    if(Test-Path -LiteralPath $resolved){
        for($current=Get-Item -LiteralPath $resolved;$null -ne $current;$current=$current.Parent){
            if($current.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Scratch cleanup refuses linked paths.'}
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
[IO.File]::WriteAllText($receiptPath,($finalRecord|ConvertTo-Json -Depth 20)+"`n",$utf8)
@{success=$true;path=$receiptPath;sha256=(Sha $receiptPath);checks=$checks;cases=$cases.Count;parserChecks=$parserRecord.parserChecks}|ConvertTo-Json -Compress
