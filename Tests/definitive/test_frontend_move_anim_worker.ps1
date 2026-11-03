#!/usr/bin/env pwsh
#requires -Version 7.0
# Only the three new movement-aware worker modes execute. The controlled Core
# transports receipts, while the actual worker owns its thread and lifecycle.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameDeDll,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedGameDeSha256
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repository=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $repository ('Build/mta-frontend-moveanim-worker/'+$id)
$scratchRoot=Join-Path $repository ('Build/tmp/moveanim-worker-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previousEnvironment=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){$previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$markers=@('frontendMoveAnimWorkerDeferredTested','frontendMoveAnimWorkerStopTested','frontendMoveAnimWorkerLegacyTested','frontendMoveAnimWorkerCacheTested')
$record=[ordered]@{schema='mta_frontend_move_anim_worker_tests_v1';directory=$outputRoot;success=$false;recordedAt=[DateTimeOffset]::Now.ToString('o');
    sourceHashes=@();sourcesUnchanged=$true;compiles=@();results=@();checks=0;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    backend=$null;artifacts=@();disposedOwnedCopies=@();ownedCopiesRemoved=$false;
    scope='Three new actual CoreStartupReference worker modes: defer input/reopening while explicit movement work is queued/pending, continue Core pulses, Stop through Core-owned authoritative cleanup, generation/thread-bound copied report, and legacy absent-export behavior. Controlled Core exports publish receipts; no native workload or original game code executes.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;fullMtaReady=$false;historicalModesExecuted=$false}
foreach($marker in $markers){$record[$marker]=$false}
function Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function AddFailure([string]$Message){$record.success=$false;if($null -eq $record.failure){$record.failure=$Message}else{$record.failure += [Environment]::NewLine+$Message}}
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$DeadlineMs){
    $stdout=Join-Path $Directory ($Name+'.log');$stderr=Join-Path $Directory ($Name+'-errors.log');$command=Join-Path $Directory ($Name+'-command.json')
    [ordered]@{executable=$Executable;sha256=(Sha $Executable);arguments=$Arguments;workingDirectory=$Directory;deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot}|
        ConvertTo-Json -Depth 4|Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=$Executable;$info.WorkingDirectory=$Directory;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true;$info.Environment['TEMP']=$scratchRoot;$info.Environment['TMP']=$scratchRoot
    foreach($argument in $Arguments){$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    try{
        if(-not $process.Start()){throw "Could not start owned $Name process."}
        $readOut=$process.StandardOutput.ReadToEndAsync();$readErr=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($DeadlineMs)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw "Owned $Name process did not terminate."}}
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;timedOut=$timedOut;
            stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    }finally{$process.Dispose()}
}
try{
    $GameDeDll=(Resolve-Path -LiteralPath $GameDeDll).Path
    if((Sha $GameDeDll) -ne $ExpectedGameDeSha256.ToLowerInvariant()){throw 'Actual backend input hash differs.'}
    $bytes=[IO.File]::ReadAllBytes($GameDeDll)
    if($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d){throw 'Invalid backend image.'}
    $pe=[BitConverter]::ToInt32($bytes,0x3c)
    if($pe -lt 64 -or $pe -gt $bytes.Length-26 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($bytes,$pe+24) -ne 0x20b -or
        -not([BitConverter]::ToUInt16($bytes,$pe+22) -band 0x2000)){throw 'Actual backend must be an AMD64 PE32+ DLL.'}
    $record.backend=[ordered]@{path=$GameDeDll;sha256=(Sha $GameDeDll);bytes=$bytes.Length;machine='AMD64'}
    $sourceNames=@('Tests/definitive/CoreStartupFixture.cpp','Tests/definitive/CoreStartupReferenceTests.cpp',
        'Tests/definitive/FrontendMoveAnimWorkerTests.inl','Tests/definitive/test_frontend_move_anim_worker.ps1',
        'Client/definitive/CoreStartupReference.cpp','Client/definitive/NativeFrontendHost.cpp',
        'Client/definitive/FrontendMoveAnimWorkloadGate.h','Client/core/DefinitiveMoveAnimWorkload.h','Client/sdk/game/CGameMoveAnimWorkload.h')
    $pending=[Collections.Generic.Queue[string]]::new();foreach($name in $sourceNames){$pending.Enqueue($name)}
    $observed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count){
        $source=$pending.Dequeue();if(-not $observed.Add($source)){continue};$path=Join-Path $repository $source
        if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Worker input missing: $source"}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"]+)[>"]')){
            $name=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path -Parent $path) $name),(Join-Path $repository ('Shared/sdk/'+$name)),(Join-Path $repository ('Client/sdk/'+$name)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){
                    $relative=[IO.Path]::GetRelativePath($repository,(Resolve-Path -LiteralPath $candidate).Path).Replace('\','/')
                    if($relative.StartsWith('../')){throw 'Worker source escaped the workspace.'};$pending.Enqueue($relative);break
                }
            }
        }
    }
    $record.sourceHashes=@($observed|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $repository $_))}})
    foreach($inputFile in $record.sourceHashes){
        $saved=Join-Path $outputRoot ('sources/'+$inputFile.path);[IO.Directory]::CreateDirectory((Split-Path -Parent $saved))|Out-Null
        Copy-Item -LiteralPath (Join-Path $repository $inputFile.path) -Destination $saved
    }
    $env:TEMP=$scratchRoot;$env:TMP=$scratchRoot
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio){throw 'MSVC x64 tools are required.'}
    $batch=Join-Path $outputRoot 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
    $environment=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'msvc-environment' 30000
    if($environment.exitCode -ne 0 -or $environment.timedOut){throw 'MSVC environment initialization failed.'}
    foreach($entry in [IO.File]::ReadAllLines($environment.stdout)){
        if($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
    }
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $common=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DUNICODE','/D_UNICODE',
        ('/I'+(Join-Path $repository 'Client/sdk')),('/I'+(Join-Path $repository 'Shared/sdk')))
    function Compile([string]$Name,[string[]]$Sources,[string]$Destination,[string[]]$Extra){
        $objects=Join-Path $scratchRoot $Name;[IO.Directory]::CreateDirectory($objects)|Out-Null
        $arguments=$common+$Extra+$Sources+@(('/Fo'+$objects+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $objects 'compiler.pdb')),('/Fe'+$Destination),'/link','user32.lib')
        $result=RunOwned $compiler $arguments $outputRoot ($Name+'-compile') 60000;$record.compiles+=@($result)
        if($result.exitCode -ne 0 -or $result.timedOut){throw "Compilation failed: $Name"}
        $record.artifacts+=@([ordered]@{path=$Destination;sha256=(Sha $Destination);bytes=(Get-Item -LiteralPath $Destination).Length})
    }
    $fixture=Join-Path $outputRoot 'MoveAnimCoreFixture.dll';$legacyFixture=Join-Path $outputRoot 'LegacyCoreFixture.dll';$hostExe=Join-Path $outputRoot 'frontend_moveanim_worker_tests.exe'
    Compile 'fixture-moveanim' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $fixture @('/LD','/EHc-','/DCORE_FIXTURE_MOVEANIM_WORKER=1')
    Compile 'fixture-legacy' @((Join-Path $PSScriptRoot 'CoreStartupFixture.cpp')) $legacyFixture @('/LD','/EHc-')
    Compile 'production-worker-host' @((Join-Path $PSScriptRoot 'CoreStartupReferenceTests.cpp'),(Join-Path $repository 'Client/definitive/CoreStartupReference.cpp'),
        (Join-Path $repository 'Client/definitive/NativeFrontendHost.cpp')) $hostExe @('/DMTA_TEST_CORE_STARTUP_ACCESS=1')
    if((Sha $hostExe) -eq '63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f'){throw 'Owned host unexpectedly matches the real game.'}
    foreach($mode in @('moveanim-worker-deferred','moveanim-worker-stop','moveanim-worker-legacy')){
        $caseRoot=Join-Path $scratchRoot $mode;[IO.Directory]::CreateDirectory($caseRoot)|Out-Null
        $ownedHost=Join-Path $caseRoot 'SanAndreas.exe';$ownedBackend=Join-Path $caseRoot 'game_de.dll'
        Copy-Item -LiteralPath $hostExe -Destination $ownedHost;Copy-Item -LiteralPath $GameDeDll -Destination $ownedBackend
        if((Sha $ownedBackend) -ne $record.backend.sha256){throw 'Owned backend copy differs.'}
        $controlled=if($mode -eq 'moveanim-worker-legacy'){$legacyFixture}else{$fixture}
        $run=RunOwned $ownedHost @($caseRoot,$mode,$controlled) $outputRoot $mode 30000
        $text=[IO.File]::ReadAllText($run.stdout)
        $summary=@($text -split '\r?\n'|Where-Object{$_.TrimStart().StartsWith('{')})|Select-Object -Last 1
        $result=if($summary){$summary|ConvertFrom-Json}else{$null}
        $run['mode']=$mode;$run['result']=$result;$run['completed']=$text.Contains('MoveAnim worker case completed: '+$mode)
        $run['cacheTested']=$text.Contains('MoveAnim worker cached report case completed: '+$mode)
        $record.results+=@($run)
        if($run.exitCode -ne 0 -or $run.timedOut -or $null -eq $result -or -not $result.success -or $result.checks -lt 1 -or
            -not $run.completed -or -not $run.cacheTested -or $result.actual_mode -or $result.game_loaded){throw "Movement-aware worker case failed: $mode"}
        $record.checks += [int]$result.checks
        foreach($copy in @($ownedHost,$ownedBackend,(Join-Path $caseRoot 'core.dll'))){
            if(Test-Path -LiteralPath $copy -PathType Leaf){$record.disposedOwnedCopies+=@([ordered]@{path=$copy;sha256BeforeRemoval=(Sha $copy);removed=$false})}
        }
    }
    $record.frontendMoveAnimWorkerDeferredTested=$record.results[0].completed
    $record.frontendMoveAnimWorkerStopTested=$record.results[1].completed
    $record.frontendMoveAnimWorkerLegacyTested=$record.results[2].completed
    $record.frontendMoveAnimWorkerCacheTested=@($record.results|Where-Object{-not $_.cacheTested}).Count -eq 0
    $record.success=$record.results.Count -eq 3
}catch{AddFailure ($_.Exception.ToString())}
finally{
    foreach($name in $previousEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')}
    foreach($inputFile in $record.sourceHashes){
        try{if((Sha (Join-Path $repository $inputFile.path)) -ne $inputFile.sha256){$record.sourcesUnchanged=$false}}
        catch{$record.sourcesUnchanged=$false;AddFailure ($_.Exception.ToString())}
    }
    try{if($record.backend -and (Sha $record.backend.path) -ne $record.backend.sha256){throw 'Actual backend input changed.'}}
    catch{AddFailure ($_.Exception.ToString())}
    $record.success=$record.success -and $record.sourcesUnchanged
    try{
        # Every copied backend/host is disposable and owned by this invocation.
        # Check the complete native PowerShell deletion target before recursion.
        $resolvedScratch=(Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\')+'\'
        $allowed=(Resolve-Path -LiteralPath (Join-Path $repository 'Build/tmp')).Path.TrimEnd('\')+'\'
        if(-not $resolvedScratch.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Owned scratch escaped Build/tmp.'}
        foreach($entry in @(Get-ChildItem -LiteralPath $scratchRoot -Force -Recurse)){
            if(($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -or -not $entry.FullName.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)){
                throw 'Unexpected link or escaped scratch path; preserve for inspection.'
            }
        }
        Remove-Item -LiteralPath $scratchRoot -Force -Recurse
        $record.scratchRemoved=-not(Test-Path -LiteralPath $scratchRoot)
        foreach($copy in $record.disposedOwnedCopies){$copy.removed=-not(Test-Path -LiteralPath $copy.path)}
        $record.ownedCopiesRemoved=$record.scratchRemoved -and @($record.disposedOwnedCopies|Where-Object{-not $_.removed}).Count -eq 0
    }catch{AddFailure ($_.Exception.ToString())}
    $record.success=$record.success -and $record.scratchRemoved -and $record.ownedCopiesRemoved
    $record.finishedAt=[DateTimeOffset]::Now.ToString('o')
    $record|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw "Movement-aware worker checks failed; see $outputRoot"}
