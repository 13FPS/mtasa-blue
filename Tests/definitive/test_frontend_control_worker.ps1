#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the production value-only mailbox and startup worker in disposable
# hosts. Controlled DLLs do not contain MTA, native engine commands or graphics.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$output=Join-Path $root ('Build/mta-frontend-control-worker/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($output)|Out-Null
function Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
$names=@('Client/definitive/FrontendControlMailbox.h','Client/definitive/CoreStartupReference.h','Client/definitive/CoreStartupReference.cpp',
    'Client/core/DefinitiveFrontend.h','Client/core/DefinitiveGuiFrontendReference.h','Client/core/DefinitiveGuiFrontendInputReference.h','Client/core/DefinitiveGuiSurfaceReference.h',
    'Client/core/DefinitiveGuiStartupReference.h','Client/core/DefinitiveInputReference.h','Client/core/DefinitiveMovementReference.h','Client/core/DefinitiveRenderReference.h',
    'Tests/definitive/FrontendControlMailboxTests.cpp','Tests/definitive/CoreGuiFrontendFixture.cpp','Tests/definitive/CoreGuiFrontendWorkerTests.cpp','Tests/definitive/test_frontend_control_worker.ps1')
$hashes=[ordered]@{}
foreach($name in $names){
    $source=Join-Path $root $name;$hashes[$name]=Sha $source
    $saved=Join-Path $output ('sources/'+$name)
    [IO.Directory]::CreateDirectory((Split-Path $saved))|Out-Null
    Copy-Item -LiteralPath $source -Destination $saved
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'Visual Studio x64 native tools are required.'}
$dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
if($LASTEXITCODE -ne 0){throw 'Compiler environment initialization failed.'}
foreach($entry in $environment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$common=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/MT','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DUNICODE','/D_UNICODE')
$compiles=[Collections.Generic.List[object]]::new()
$runs=[Collections.Generic.List[object]]::new()
function Compile([string]$Name,[string[]]$Sources,[string]$Target,[string[]]$Extra=@()){
    $objects=Join-Path $output ('objects/'+$Name)
    [IO.Directory]::CreateDirectory($objects)|Out-Null
    [IO.Directory]::CreateDirectory((Split-Path $Target))|Out-Null
    $messages=@(& $compiler @common @Extra @Sources ('/Fo'+$objects+[IO.Path]::DirectorySeparatorChar) ('/Fe'+$Target) /link user32.lib 2>&1)
    $code=$LASTEXITCODE;$log=Join-Path $output ($Name+'-compile.log')
    $messages|Set-Content -LiteralPath $log -Encoding utf8
    $compiles.Add(@{name=$Name;exit_code=$code;log=$log;log_sha256=(Sha $log);artifact=$Target})
    if($code -ne 0){throw "Compilation failed: $Name; see $log"}
}
function Run([string]$Name,[string]$Executable,[string]$Argument=''){
    $stdout=Join-Path $output ($Name+'-stdout.txt');$stderr=Join-Path $output ($Name+'-stderr.txt')
    $arguments=@{FilePath=$Executable;WorkingDirectory=(Split-Path $Executable);WindowStyle='Hidden';PassThru=$true;RedirectStandardOutput=$stdout;RedirectStandardError=$stderr}
    if($Argument){$arguments.ArgumentList=$Argument}
    $process=Start-Process @arguments
    if(-not $process.WaitForExit(20000)){$process.Kill();$process.WaitForExit();throw "Owned fixture timeout: $Name"}
    if($process.ExitCode -ne 0){throw "Owned fixture failed: $Name; see $stdout and $stderr"}
    $runs.Add(@{name=$Name;exit_code=$process.ExitCode;stdout=$stdout;stdout_sha256=(Sha $stdout);stderr=$stderr;host_sha256=(Sha $Executable)})
}
$failure=$null
try{
    $mailbox=Join-Path $output 'mailbox/FrontendControlMailboxTests.exe'
    Compile 'mailbox' @((Join-Path $PSScriptRoot 'FrontendControlMailboxTests.cpp')) $mailbox
    Run 'deterministic-mailbox' $mailbox
    $fixture=Join-Path $output 'fixture/CoreGuiFrontendFixture.dll'
    Compile 'controlled-core' @((Join-Path $PSScriptRoot 'CoreGuiFrontendFixture.cpp')) $fixture @('/LD')
    $workerHostPath=Join-Path $output 'host/FrontendWorkerHost.exe'
    Compile 'production-worker' @((Join-Path $PSScriptRoot 'CoreGuiFrontendWorkerTests.cpp'),(Join-Path $root 'Client/definitive/CoreStartupReference.cpp')) $workerHostPath @('/DMTA_TEST_CORE_STARTUP_ACCESS=1','/DMTA_TEST_FRONTEND_CONTROL_ACCESS=1')
    foreach($mode in @((0..12)+(100..111))){
        $directory=Join-Path $output ('case-'+$mode);[IO.Directory]::CreateDirectory($directory)|Out-Null
        $owned=Join-Path $directory 'FrontendWorkerHost.exe'
        Copy-Item -LiteralPath $workerHostPath -Destination $owned
        Copy-Item -LiteralPath $fixture -Destination (Join-Path $directory 'core.dll')
        Run ('worker-'+$mode) $owned ([string]$mode)
    }
}catch{$failure=$_.Exception.Message}
$unchanged=$true
foreach($name in $names){if((Sha (Join-Path $root $name)) -ne $hashes[$name]){$unchanged=$false}}
$record=[ordered]@{success=($null -eq $failure -and $unchanged -and $runs.Count -eq 26);failure=$failure;directory=$output;
    source_hashes=$hashes;sources_unchanged=$unchanged;compiles=$compiles;runs=$runs;game_launched=$false;game_memory_read=$false;
    synthetic_core_is_mta=$false;engine_control_write_verified=$false;full_mta_startup_verified=$false}
$result=Join-Path $output 'test-result.json';$record|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $result -Encoding utf8
@{success=$record.success;failure=$failure;path=$result;runs=$runs.Count}|ConvertTo-Json -Compress
if(-not $record.success){throw "Frontend control worker tests failed; see $result"}
