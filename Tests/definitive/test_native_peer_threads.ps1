#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the actual shared owner and park only this fixture's own threads.
[CmdletBinding()]
param(
    [string]$DetoursLibrary,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedDetoursSha256='101171deea2d5b3e42ea74d31d2fbb3577a45097951d19ad700ab42a8e777afe'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
if(-not $DetoursLibrary){$DetoursLibrary=Join-Path $root 'Build/bin/x64/Release/detours.lib'}
function Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
$library=(Resolve-Path -LiteralPath $DetoursLibrary).Path
if((Sha $library) -cne $ExpectedDetoursSha256.ToLowerInvariant()){throw 'Explicit normal x64 VM-node Detours library pin differs.'}
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/definitive-recovery/mta-pad-owner-batch-20261020/native-peer-threads-'+$id)
$scratch=Join-Path $root ('Build/tmp/pad-owner-batch-20261020/native-peer-threads-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
$environmentBefore=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){$environmentBefore[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_peer_threads_tests_v1';success=$false;directory=$output;recordedAt=[DateTimeOffset]::Now.ToString('o');
    sources=@();sourcesUnchanged=$true;library=@{path=$library;sha256=(Sha $library)};commands=@();checks=0;failure=$null;scratch=$scratch;scratchRemoved=$false;
    ownProcessThreadsOnly=$true;gameLaunched=$false;gameAttached=$false;foreignProcessAccess=$false;gameMemoryWritten=$false;nativeHooksInstalled=$false;
    scope='Actual clock-derived NativePeerThreads prepare/park/context policy/Detours external registration/release/retained failure/retry. Ordinary owned workers plus narrowly injected OS failures; no native game addresses or engine admission are qualified.'}
function RunOwned([string]$Name,[string]$Executable,[string[]]$Arguments,[int]$Timeout){
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=$Executable;$start.WorkingDirectory=$output;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $start.Environment['TEMP']=$scratch;$start.Environment['TMP']=$scratch
    foreach($argument in $Arguments){$start.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$start
    try{
        if(-not $process.Start()){throw "Owned $Name process did not start"}
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($Timeout)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw "Owned $Name process did not terminate"}}
        $outPath=Join-Path $output ($Name+'.log');$errPath=Join-Path $output ($Name+'-errors.log')
        [IO.File]::WriteAllText($outPath,$stdout.GetAwaiter().GetResult(),$utf8)
        [IO.File]::WriteAllText($errPath,$stderr.GetAwaiter().GetResult(),$utf8)
        $command=[ordered]@{name=$Name;executable=$Executable;executableSha256=(Sha $Executable);arguments=$Arguments;exitCode=$process.ExitCode;timedOut=$timedOut;
            stdout=$outPath;stdoutSha256=(Sha $outPath);stderr=$errPath;stderrSha256=(Sha $errPath);temporaryDirectory=$scratch}
        $record.commands+=@($command)
        if($process.ExitCode -ne 0 -or $timedOut){throw "Owned $Name failed; see $outPath"}
        return $command
    }finally{$process.Dispose()}
}
try{
    $names=@('Client/game_de/NativePeerThreads.h','Client/game_de/NativePeerThreads.cpp','Client/game_de/NativeClockService.cpp',
        'Tests/definitive/NativePeerThreadsTests.cpp','Tests/definitive/test_native_peer_threads.ps1',
        'vendor/detours/4.0.1/src/detours.h','vendor/detours/4.0.1/src/detours.cpp','vendor/detours/4.0.1/src/modules.cpp',
        'vendor/detours/4.0.1/src/disasm.cpp','vendor/detours/4.0.1/src/image.cpp','vendor/detours/4.0.1/src/creatwth.cpp')
    $record.sources=@($names|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    $env:TEMP=$scratch;$env:TMP=$scratch
    $vswhere=Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vsResult=RunOwned 'vswhere' $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') 10000
    $vs=[IO.File]::ReadAllText($vsResult.stdout).Trim()
    if(-not $vs){throw 'MSVC x64 tools missing'}
    $capture=Join-Path $output 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $capture -Encoding ascii
    $environment=RunOwned 'msvc-environment' $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') 30000
    foreach($line in [IO.File]::ReadAllLines($environment.stdout)){
        if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
    }
    $env:TEMP=$scratch;$env:TMP=$scratch
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $executable=Join-Path $output 'native_peer_threads_tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MT','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
        ('/I'+(Join-Path $root 'vendor/detours/4.0.1/src')),(Join-Path $PSScriptRoot 'NativePeerThreadsTests.cpp'),(Join-Path $root 'Client/game_de/NativePeerThreads.cpp'),
        ('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$executable),'/link',$library,'user32.lib')
    RunOwned 'compile' $compiler $arguments 60000|Out-Null
    $result=RunOwned 'test' $executable @() 40000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.stdout).TrimEnd(),'(?:^|\r?\n)Passed (\d+) production peer-thread ownership checks$')
    if(-not $summary.Success){throw 'Actual peer-thread test count missing'}
    $record.checks=[int]$summary.Groups[1].Value
    $record.executable=@{path=$executable;sha256=(Sha $executable)}
    $record.success=$record.checks -gt 0
}catch{$record.failure=$_.Exception.ToString();$record.success=$false}
finally{
    foreach($name in $environmentBefore.Keys){[Environment]::SetEnvironmentVariable($name,$environmentBefore[$name],'Process')}
    foreach($source in $record.sources){
        if((Sha (Join-Path $root $source.path)) -cne $source.sha256){$record.sourcesUnchanged=$false}
    }
    if((Sha $library) -cne $record.library.sha256){$record.sourcesUnchanged=$false}
    try{
        $resolved=[IO.Path]::GetFullPath($scratch)
        $allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp/pad-owner-batch-20261020')).TrimEnd('\')+'\'
        if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch left owned task directory'}
        for($ancestor=Get-Item -LiteralPath $resolved;$null -ne $ancestor;$ancestor=$ancestor.Parent){
            if($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Scratch ancestry is linked'}
        }
        $files=@(Get-ChildItem -LiteralPath $resolved -Force)
        foreach($file in $files){
            if($file -is [IO.DirectoryInfo] -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Unexpected scratch directory/link retained'}
            if(-not [IO.Path]::GetFullPath($file.FullName).StartsWith($resolved+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch file left owned directory'}
        }
        foreach($file in $files){Remove-Item -LiteralPath $file.FullName -Force}
        [IO.Directory]::Delete($resolved,$false)
        $record.scratchRemoved=$true
    }catch{
        $record.success=$false
        $record.failure=($record.failure+[Environment]::NewLine+$_.Exception.ToString()).Trim()
    }
    $record.success=$record.success -and $record.sourcesUnchanged -and $record.scratchRemoved
    $record.finishedAt=[DateTimeOffset]::Now.ToString('o')
    $record|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $output 'test-result.json') -Encoding utf8NoBOM
}
@{success=$record.success;path=(Join-Path $output 'test-result.json');checks=$record.checks;failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw "Native peer-thread checks failed; see $output"}
