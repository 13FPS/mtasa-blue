#!/usr/bin/env pwsh
#requires -Version 7.0
# One owned production host compile: focused intent/route cases, then the actual rebuilt Core export.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreDll,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedCoreSha256,
    [Parameter(Mandatory)][string]$GameDeDll,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedGameDeSha256,
    [Parameter(Mandatory)][string]$PthreadDll,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedPthreadSha256,
    [Parameter(Mandatory)][string]$XmllDll,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedXmllSha256
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if([IntPtr]::Size -ne 8){throw 'Use native x64 PowerShell.'}
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $root ('Build/mta-loaded-world-frontend-startup/'+$id)
$scratchRoot=Join-Path $root ('Build/tmp/loaded-world-frontend-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$record=[ordered]@{schema='mta_loaded_world_frontend_startup_tests_v1';success=$false;directory=$outputRoot;scratch=$scratchRoot;scratchRemoved=$false;
    sourceHashes=@();sourcesUnchanged=$true;compiles=@();runs=@();artifacts=@();failure=$null;
    loadedWorldStartupIntentTested=$false;loadedWorldAutoHostRouteTested=$false;loadedWorldManualCompatibilityTested=$false;
    loadedWorldActualCoreAdmissionTested=$false;loadedWorldLegacyReportsPreserved=$false;gameLoaded=$false;gameAttached=$false;installedPayloadChanged=$false}
function RunOwned([string]$exe,[string[]]$arguments,[string]$directory,[string]$name,[int]$deadline=30000){
    $stdout=Join-Path $outputRoot ($name+'-stdout.log');$stderr=Join-Path $outputRoot ($name+'-stderr.log')
    $command=Join-Path $outputRoot ($name+'-command.json')
    [ordered]@{executable=$exe;sha256=(Sha $exe);arguments=$arguments;workingDirectory=$directory;deadlineMs=$deadline;temporaryDirectory=$scratchRoot}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=$exe;$info.WorkingDirectory=$directory;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true;$info.Environment['TEMP']=$scratchRoot;$info.Environment['TMP']=$scratchRoot
    foreach($arg in $arguments){$info.ArgumentList.Add($arg)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    try{
        if(-not $process.Start()){throw "Could not start owned $name"}
        $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync();$timedOut=-not $process.WaitForExit($deadline)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned process did not terminate'}}
        [IO.File]::WriteAllText($stdout,$out.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$err.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        return [ordered]@{name=$name;exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr);command=$command;commandSha256=(Sha $command)}
    }finally{$process.Dispose()}
}
function Compile([string]$name,[string[]]$sources,[string]$destination,[string[]]$extra){
    [IO.Directory]::CreateDirectory((Split-Path $destination))|Out-Null
    $arguments=$script:common+$extra+$sources+@(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$destination))
    $result=RunOwned $script:compiler $arguments $outputRoot $name 60000
    $record.compiles+=@($result)
    if($result.exitCode -ne 0 -or $result.timedOut){throw "Owned compilation failed: $name"}
}
function RunHost([string]$name,[string]$directory,[string[]]$arguments){
    $result=RunOwned (Join-Path $directory 'SanAndreas.exe') $arguments $directory $name 30000
    if($result.exitCode -ne 0 -or $result.timedOut){throw "Owned route host failed: $name"}
    $summary=Get-Content -LiteralPath $result.stdout -Raw|ConvertFrom-Json
    if(-not $summary.success -or $summary.game_loaded){throw "Invalid route receipt: $name"}
    $result['summary']=$summary;$record.runs+=@($result)
}
try{
    $names=@('Tests/definitive/test_loaded_world_frontend_startup.ps1','Tests/definitive/LoadedWorldFrontendStartupTests.inl',
        'Tests/definitive/CoreStartupReferenceTests.cpp','Tests/definitive/CoreStartupFixture.cpp',
        'Client/definitive/CoreStartupReference.h','Client/definitive/CoreStartupReference.cpp','Client/definitive/NativeFrontendHost.h',
        'Client/definitive/NativeFrontendHost.cpp','Client/definitive/NativeFrontendActivation.h','Client/definitive/NativeFrontendStartupIntent.h',
        'Client/definitive/FrontendAdmissionSample.h','Client/definitive/FrontendRuntimeCache.h','Client/definitive/FrontendRuntimeMailbox.h',
        'Client/definitive/FrontendRuntimeReport.h','Client/definitive/FrontendControlMailbox.h','Client/definitive/PersistentFrontendHostLease.h',
        'Client/core/DefinitiveFrontendRuntime.h','Client/core/DefinitiveFrontendStartup.h','Client/core/DefinitiveFrontendStartup.cpp','Client/core/Core.cpp',
        'Client/core/DefinitiveFrontend.h','Client/core/DefinitivePersistentFrontend.h','Client/core/DefinitiveLocalInputRuntime.h',
        'Client/sdk/game/CGameFrontendAdmission.h','Client/sdk/game/CGameAdapter.h','Client/definitive/coreconfig.xml',
        'Shared/data/MTA San Andreas/MTA/locale/en_US/client.pot')
    $record.sourceHashes=@($names|Sort-Object -Unique|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    $env:TEMP=$scratchRoot;$env:TMP=$scratchRoot
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $vs=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or -not $vs){throw 'MSVC x64 tools unavailable'}
    $batch=Join-Path $outputRoot 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
    $setup=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'msvc-environment' 30000
    if($setup.exitCode -ne 0 -or $setup.timedOut){throw 'MSVC environment failed'}
    foreach($line in [IO.File]::ReadAllLines($setup.stdout)){if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $script:compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $script:common=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DUNICODE','/D_UNICODE',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')))
    $normalFixture=Join-Path $outputRoot 'fixture/CoreFixture.dll';$missingFixture=Join-Path $outputRoot 'fixture/CoreMissingAdmission.dll'
    Compile 'fixture-normal' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $normalFixture @('/LD','/EHc-')
    Compile 'fixture-missing-admission' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $missingFixture @('/LD','/EHc-','/DCORE_FIXTURE_MISSING_AUTO_ADMISSION_EXPORT=1')
    $hostExecutable=Join-Path $outputRoot 'host/SanAndreas.exe'
    Compile 'actual-worker-host' @((Join-Path $PSScriptRoot 'CoreStartupReferenceTests.cpp'),(Join-Path $root 'Client/definitive/CoreStartupReference.cpp'),
        (Join-Path $root 'Client/definitive/NativeFrontendHost.cpp')) $hostExecutable @('/DMTA_TEST_CORE_STARTUP_ACCESS=1','/DMTA_TEST_NATIVE_FRONTEND_HOST_CONTEXT=1','user32.lib')
    $pinned=@(@{source=$CoreDll;name='core.dll';sha=$ExpectedCoreSha256},@{source=$GameDeDll;name='game_de.dll';sha=$ExpectedGameDeSha256},
        @{source=$PthreadDll;name='pthread.dll';sha=$ExpectedPthreadSha256},@{source=$XmllDll;name='xmll.dll';sha=$ExpectedXmllSha256})
    foreach($item in $pinned){$item.source=(Resolve-Path -LiteralPath $item.source).Path;if((Sha $item.source) -ne $item.sha.ToLowerInvariant()){throw "Normal artifact pin differs: $($item.name)"};$record.artifacts+=@([ordered]@{source=$item.source;name=$item.name;sha256=(Sha $item.source)})}
    $modes=@('loaded-world-intent','loaded-world-host-legacy','loaded-world-host-gates','loaded-world-host-manual','loaded-world-host-stop','loaded-world-host-missing')
    foreach($mode in $modes){
        $directory=Join-Path $outputRoot $mode;[IO.Directory]::CreateDirectory($directory)|Out-Null
        Copy-Item -LiteralPath $hostExecutable -Destination (Join-Path $directory 'SanAndreas.exe')
        Copy-Item -LiteralPath $GameDeDll -Destination (Join-Path $directory 'game_de.dll')
        $fixture=if($mode -eq 'loaded-world-host-missing'){$missingFixture}else{$normalFixture}
        $arguments=if($mode -eq 'loaded-world-intent'){@($directory,$mode)}else{@($directory,$mode,$fixture)}
        RunHost $mode $directory $arguments
    }
    $actual=Join-Path $outputRoot 'actual-host/CLEO/de-online'
    [IO.Directory]::CreateDirectory((Join-Path $actual 'mta/config'))|Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $actual 'mta/logs'))|Out-Null
    Copy-Item -LiteralPath $hostExecutable -Destination (Join-Path $actual 'SanAndreas.exe')
    foreach($item in $pinned){Copy-Item -LiteralPath $item.source -Destination (Join-Path $actual $item.name)}
    Copy-Item -LiteralPath (Join-Path $root 'Client/definitive/coreconfig.xml') -Destination (Join-Path $actual 'mta/config/coreconfig.xml')
    # The real retained Core requires the same file-backed client catalog as
    # the shipping frontend package, even while no GUI view is attached.
    $catalogSource=Join-Path $root 'Shared/data/MTA San Andreas/MTA/locale/en_US/client.pot'
    $catalogDestination=Join-Path $actual 'mta/locale/en_US/client.pot'
    [IO.Directory]::CreateDirectory((Split-Path $catalogDestination -Parent))|Out-Null
    Copy-Item -LiteralPath $catalogSource -Destination $catalogDestination
    if((Sha $catalogDestination) -cne (Sha $catalogSource)){throw 'Actual Core locale catalog copy differs from the shipped asset'}
    $record.localeCatalog=@{source=$catalogSource;path=$catalogDestination;sha256=(Sha $catalogSource)}
    RunHost 'actual-loaded-world-admission' $actual @($actual,'actual-loaded-world-admission')
    $record.loadedWorldStartupIntentTested=@($record.runs|Where-Object{$_.name -eq 'loaded-world-intent'}).Count -eq 1
    $record.loadedWorldAutoHostRouteTested=@($record.runs|Where-Object{$_.name -in @('loaded-world-host-gates','loaded-world-host-manual','loaded-world-host-stop','loaded-world-host-missing')}).Count -eq 4
    $record.loadedWorldManualCompatibilityTested=@($record.runs|Where-Object{$_.name -eq 'loaded-world-host-legacy'}).Count -eq 1
    $record.loadedWorldActualCoreAdmissionTested=@($record.runs|Where-Object{$_.name -eq 'actual-loaded-world-admission' -and $_.summary.actual_mode}).Count -eq 1
    $record.loadedWorldLegacyReportsPreserved=$record.loadedWorldActualCoreAdmissionTested -and $record.loadedWorldAutoHostRouteTested
    $record.success=$record.runs.Count -eq 7 -and $record.loadedWorldStartupIntentTested -and $record.loadedWorldAutoHostRouteTested -and $record.loadedWorldManualCompatibilityTested -and $record.loadedWorldActualCoreAdmissionTested -and $record.loadedWorldLegacyReportsPreserved
}catch{$record.failure=$_.Exception.ToString();$record.success=$false}
finally{
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    foreach($item in $record.sourceHashes){if((Sha (Join-Path $root $item.path)) -ne $item.sha256){$record.sourcesUnchanged=$false}}
    foreach($item in $record.artifacts){if((Sha $item.source) -ne $item.sha256){$record.sourcesUnchanged=$false}}
    $record.success=$record.success -and $record.sourcesUnchanged
    try{
        $resolved=(Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\')+'\';$parent=(Resolve-Path -LiteralPath (Join-Path $root 'Build/tmp')).Path.TrimEnd('\')+'\'
        if(-not $resolved.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -or ((Get-Item -LiteralPath $scratchRoot).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Scratch path escaped its owned directory'}
        $entries=@(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach($entry in $entries){if($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Unexpected scratch directory/link preserved'}}
        foreach($entry in $entries){Remove-Item -LiteralPath $entry.FullName -Force}
        [IO.Directory]::Delete($scratchRoot,$false);$record.scratchRemoved=$true
    }catch{$record.success=$false;$record.failure=([string]$record.failure)+[Environment]::NewLine+$_.Exception.ToString()}
    $record|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw "Loaded-world frontend checks failed: $outputRoot"}
