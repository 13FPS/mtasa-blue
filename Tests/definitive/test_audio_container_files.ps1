#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile/execute only the complete file service and owned synthetic SDK cases.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-audio-container-files/'+$id)
$scratch=Join-Path $root ('Build/tmp/audio-container-files-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$previous=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_audio_container_files_tests_v1';success=$false;recordedAt=[DateTimeOffset]::Now.ToString('o');
    directory=$output;sourceHashes=@();sourcesUnchanged=$false;scratchDirectory=$scratch;scratchRemoved=$false;
    architecture='x64';configuration='Release';compile=$null;test=$null;executable=$null;executableSha256=$null;checks=0;pointerBits=0;
    syntheticArchiveRoot=$null;syntheticArchiveFiles=@();failure=$null;
    actualAudioContainerFilesSdkTested=$false;actualAudioLookupRiffSdkTested=$false;actualAudioRadioSdkTested=$false;actualAudioValidationSdkTested=$false;
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeNativeAudioReady=$false;definitiveArchiveCompatibilityProven=$false;gameAudioServicePublished=$false;
    scope='Actual complete three-method CAudioContainer SDK on small independent synthetic lookup/SFX/radio files. RIFF, filtered bank order, XOR/track selection, malformed/missing input, output ownership and unchanged validation heuristic. No native audio, game copies or historical suites.'}
function Sha([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function RunOwned([string]$name,[string]$exe,[string[]]$arguments,[int]$deadline){
    $stdout=Join-Path $output ($name+'.log');$stderr=Join-Path $output ($name+'-errors.log');$command=Join-Path $output ($name+'-command.json')
    [ordered]@{executable=$exe;sha256=(Sha $exe);arguments=$arguments;workingDirectory=$output;temporaryDirectory=$scratch;deadlineMs=$deadline}|
        ConvertTo-Json -Depth 5|Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=$exe;$info.WorkingDirectory=$output;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true;$info.Environment['TEMP']=$scratch;$info.Environment['TMP']=$scratch
    foreach($argument in $arguments){$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    try{
        if(-not $process.Start()){throw ('Cannot start owned '+$name)}
        $readOut=$process.StandardOutput.ReadToEndAsync();$readErr=$process.StandardError.ReadToEndAsync();$timedOut=-not $process.WaitForExit($deadline)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned fixture did not terminate'}}
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false));[IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr);command=$command;commandSha256=(Sha $command)}
    }finally{$process.Dispose()}
}
try{
    $names=@('Client/sdk/game/CAudioContainerFiles.h','Client/sdk/game/CAudioContainerFiles.cpp','Client/sdk/game/CAudioContainer.h','Client/sdk/game/Common.h',
        'Client/game_sa/CAudioContainerSA.h','Client/game_sa/CAudioContainerSA.cpp','Client/game_sa/premake5.lua','Client/game_de/premake5.lua',
        'Tests/definitive/AudioContainerFilesTests.cpp','Tests/definitive/AudioContainerSyntheticArchives.h','Tests/definitive/test_audio_container_files.ps1')
    $record.sourceHashes=@($names|Sort-Object -Unique|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned 'vswhere' $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') 10000
    $vs=[IO.File]::ReadAllText($discovery.stdout).Trim();if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $vs){throw 'MSVC x64 tools required'}
    $developer=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $capture=Join-Path $output 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f $developer),'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|
        Set-Content -LiteralPath $capture -Encoding ascii
    $environment=RunOwned 'msvc-environment' $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') 30000
    if($environment.exitCode -ne 0 -or $environment.timedOut){throw 'MSVC environment failed'}
    foreach($line in [IO.File]::ReadAllLines($environment.stdout)){if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $env:TEMP=$scratch;$env:TMP=$scratch
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe';$exe=Join-Path $output 'audio_container_files_tests.exe'
    $record.executable=$exe
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/w14471','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DMTA_RELEASE',
        ('/I'+(Join-Path $root 'Client/sdk')),(Join-Path $root 'Tests/definitive/AudioContainerFilesTests.cpp'),(Join-Path $root 'Client/sdk/game/CAudioContainerFiles.cpp'),
        ('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratch 'fixture.pdb')),('/Fe'+$exe))
    $record.compile=RunOwned 'compile' $compiler $arguments 30000
    if($record.compile.exitCode -ne 0 -or $record.compile.timedOut){throw 'Complete audio file SDK fixture compilation failed'}
    $record.executableSha256=Sha $exe;$record.syntheticArchiveRoot=Join-Path $output 'synthetic-archives'
    $record.test=RunOwned 'test' $exe @($record.syntheticArchiveRoot) 30000
    $stdout=[IO.File]::ReadAllText($record.test.stdout)
    $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) complete file audio SDK checks \((\d+)-bit; no native engine code executed\)$')
    if($summary.Success){$record.checks=[int]$summary.Groups[1].Value;$record.pointerBits=[int]$summary.Groups[2].Value}
    $record.actualAudioLookupRiffSdkTested=$stdout.Contains('Actual complete MTA file-audio lookup/PCM/RIFF SDK fixture completed')
    $record.actualAudioRadioSdkTested=$stdout.Contains('Actual complete MTA file-audio radio SDK fixture completed')
    $record.actualAudioValidationSdkTested=$stdout.Contains('Actual complete MTA file-audio validation SDK fixture completed')
    $record.actualAudioContainerFilesSdkTested=$record.actualAudioLookupRiffSdkTested -and $record.actualAudioRadioSdkTested -and $record.actualAudioValidationSdkTested
    $record.success=$record.test.exitCode -eq 0 -and -not $record.test.timedOut -and $record.checks -gt 0 -and $record.pointerBits -eq 64 -and $record.actualAudioContainerFilesSdkTested
    if(-not $record.success){throw 'Complete audio SDK cases or exact execution markers failed'}
}catch{$record.success=$false;$record.failure=$_.Exception.ToString()}
finally{
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    $record.sourcesUnchanged=$true
    foreach($item in $record.sourceHashes){try{if((Sha (Join-Path $root $item.path)) -cne $item.sha256){$record.sourcesUnchanged=$false}}catch{$record.sourcesUnchanged=$false}}
    if($record.syntheticArchiveRoot -and (Test-Path -LiteralPath $record.syntheticArchiveRoot)){
        $record.syntheticArchiveFiles=@(Get-ChildItem -LiteralPath $record.syntheticArchiveRoot -Recurse -File|ForEach-Object{[ordered]@{path=[IO.Path]::GetRelativePath($output,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Sha $_.FullName)}})
    }
    try{
        $resolved=(Resolve-Path -LiteralPath $scratch).Path.TrimEnd('\')+'\';$allowed=(Resolve-Path -LiteralPath (Join-Path $root 'Build/tmp')).Path.TrimEnd('\')+'\'
        if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or ((Get-Item -LiteralPath $scratch -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Task compiler scratch escaped or became linked'}
        $items=@(Get-ChildItem -LiteralPath $scratch -Force)
        foreach($item in $items){if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or -not $item.FullName.StartsWith($resolved,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected scratch child preserved'}}
        foreach($item in $items){Remove-Item -LiteralPath $item.FullName -Force};[IO.Directory]::Delete($scratch,$false);$record.scratchRemoved=$true
    }catch{$record.success=$false;$record.failure=($record.failure+[Environment]::NewLine+$_.Exception.ToString()).Trim()}
    $record.success=$record.success -and $record.sourcesUnchanged -and $record.scratchRemoved;$record.finishedAt=[DateTimeOffset]::Now.ToString('o')
    $record|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $output 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $output 'test-result.json');checks=$record.checks;failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw ('Complete audio file SDK fixture failed; see '+$output)}
