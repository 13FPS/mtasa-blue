#!/usr/bin/env pwsh
#requires -Version 7.0
# Load the actual already-built client in a fresh isolated process; do not
# call its factory or create Core, a game interface, or a network interface.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$workspace=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$identifier=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $workspace ('Build/mta-client-module-load/'+$identifier)
$scratchRoot=Join-Path $workspace 'Build/tmp'
$scratch=Join-Path $scratchRoot ('client-module-load-'+$identifier)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP
$env:TEMP=$scratch;$env:TMP=$scratch
$record=[ordered]@{schema='mta_real_client_module_load_v1';success=$false;runs=@();failure=$null;checks=$null;
    sourcePins=@();artifactPins=@();pinsUnchanged=$false;clientFactoryCalled=$false;gameLaunched=$false;installationChanged=$false;scratchRemoved=$false}
function Pin([string]$Path){[ordered]@{path=$Path;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}}
function Run([string]$Name,[string]$Executable,[string[]]$Arguments,[int]$DeadlineSeconds){
    $info=[Diagnostics.ProcessStartInfo]::new($Executable)
    $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true;$info.WorkingDirectory=$workspace
    foreach($argument in $Arguments){$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::Start($info)
    try{
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit($DeadlineSeconds*1000)){$process.Kill($true);$process.WaitForExit();throw ($Name+' exceeded its isolated process deadline.')}
        $content=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
        $log=Join-Path $output ($Name+'.log');$content|Set-Content -LiteralPath $log -Encoding utf8NoBOM
        $record.runs+=@([ordered]@{name=$Name;executable=$Executable;arguments=$Arguments;exitCode=$process.ExitCode;log=$log})
        if($process.ExitCode -ne 0){throw ($Name+' failed; '+$log)}
        return $content
    }finally{$process.Dispose()}
}
try{
    $record.sourcePins=@('Client/core/StartupModule.h','Client/mods/deathmatch/premake5.lua',
        'Tests/definitive/ClientModuleLoadHost.cpp','Tests/definitive/test_client_module_load.ps1'|ForEach-Object{Pin (Join-Path $workspace $_)})
    $clientRoot=Join-Path $workspace 'Bin/mods/deathmatch/x64'
    $record.artifactPins=@('client.dll','bass.dll','bass_fx.dll','bassmix.dll','tags.dll','lua5.1c.dll','pcre2.dll'|ForEach-Object{Pin (Join-Path $clientRoot $_)})
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(-not $vs){throw 'Visual Studio x64 compiler tools are required.'}
    $developer=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developer)
    if($LASTEXITCODE -ne 0){throw 'MSVC x64 environment initialization failed.'}
    foreach($entry in $environment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    # The isolated host must use the recorded options, not inherited compiler
    # switches that could silently add another source or a loader-test seam.
    [Environment]::SetEnvironmentVariable('CL',$null,'Process')
    [Environment]::SetEnvironmentVariable('_CL_',$null,'Process')
    $env:TEMP=$scratch;$env:TMP=$scratch
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $hostExe=Join-Path $scratch 'client-module-load.exe'
    $compilerOutput=Run 'host-compile' $compiler @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MT',
        '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',(Join-Path $PSScriptRoot 'ClientModuleLoadHost.cpp'),
        ('/Fo'+(Join-Path $scratch 'host.obj')),('/Fe'+$hostExe)) 90
    $testOutput=Run 'actual-client-load' $hostExe @((Join-Path $clientRoot 'client.dll')) 20
    if($testOutput -notmatch 'Passed (\d+) real client module loader checks'){throw 'Actual module loader checks did not report completion.'}
    $record.checks=[int]$Matches[1]
    $record.pinsUnchanged=$true
    foreach($pin in @($record.sourcePins)+@($record.artifactPins)){
        if((Get-FileHash -LiteralPath $pin.path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.sha256){$record.pinsUnchanged=$false}
    }
    if(-not $record.pinsUnchanged){throw 'A client input changed during the isolated load check.'}
    $record.success=$true
}catch{$record.failure=$_.Exception.Message}
finally{
    $env:TEMP=$oldTemp;$env:TMP=$oldTmp
    # Only this fresh workspace scratch tree is disposable. Keep all logs,
    # input hashes and the outcome durable even when compilation/loading fails.
    try{
        $resolvedRoot=[IO.Path]::GetFullPath($scratchRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
        $resolvedScratch=(Resolve-Path -LiteralPath $scratch).Path
        if(-not $resolvedScratch.StartsWith($resolvedRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch path escaped Build/tmp.'}
        $items=@(Get-Item -LiteralPath $resolvedScratch)+@(Get-ChildItem -LiteralPath $resolvedScratch -Force -Recurse)
        if(@($items|Where-Object{($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0}).Count){throw 'Scratch cleanup refused a reparse point.'}
        Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
        $record.scratchRemoved=$true
    }catch{$record.success=$false;$record.failure=($record.failure+'; '+$_.Exception.Message).Trim(';',' ')}
    $record|ConvertTo-Json -Depth 9|Set-Content -LiteralPath (Join-Path $output 'test-result.json') -Encoding utf8NoBOM
}
if(-not $record.success){throw ('Real client module loader validation failed: '+$record.failure+'; '+$output)}
$record|ConvertTo-Json -Depth 9 -Compress
