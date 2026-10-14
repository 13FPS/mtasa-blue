#!/usr/bin/env pwsh
#requires -Version 7.0
# Query only a disposable own-process WARP swapchain; never start or attach GTA.
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$output=Join-Path $root ('Build/mta-native-gpu-ownership/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output|Out-Null
function Sha([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$names=@('Client/game_de/NativeD3D12OwnershipProbe.h','Client/game_de/NativeD3D12OwnershipProbe.cpp','Client/sdk/game/CGameRenderObservation.h',
    'Tests/definitive/NativeGpuOwnershipProbeTests.cpp','Tests/definitive/test_native_gpu_ownership_probe.ps1')
$hashes=[ordered]@{};foreach($name in $names){$hashes[$name]=Sha (Join-Path $root $name)}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
if($LASTEXITCODE -ne 0){throw 'Compiler environment failed'}
foreach($entry in $environment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$binary=Join-Path $output 'gpu-query-tests.exe';$compile=Join-Path $output 'compile.log'
& $compiler /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX /DNOMINMAX /DWIN32_LEAN_AND_MEAN ("/I"+(Join-Path $root 'Shared/sdk')) (Join-Path $PSScriptRoot 'NativeGpuOwnershipProbeTests.cpp') (Join-Path $root 'Client/game_de/NativeD3D12OwnershipProbe.cpp') ("/Fo"+$output+[IO.Path]::DirectorySeparatorChar) ("/Fe"+$binary) /link /NOLOGO d3d12.lib dxgi.lib user32.lib 2>&1|Set-Content -LiteralPath $compile -Encoding utf8
if($LASTEXITCODE -ne 0){throw "Query compilation failed: $compile"}
$log=Join-Path $output 'test.json';$errors=Join-Path $output 'test-errors.log'
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError $errors
if(-not $process.WaitForExit(30000)){$process.Kill();$process.WaitForExit();throw 'Owned query fixture timed out'}
if($process.ExitCode -ne 0){throw "Query fixture failed: $errors"}
$result=Get-Content -LiteralPath $log -Raw|ConvertFrom-Json
foreach($name in $names){if((Sha (Join-Path $root $name)) -cne $hashes[$name]){throw 'Query source changed during testing'}}
$record=[ordered]@{success=$result.success;directory=$output;checks=$result.checks;source_hashes=$hashes;sources_unchanged=$true;binary=$binary;binary_sha256=(Sha $binary);compile_log=$compile;compile_log_sha256=(Sha $compile);test_log=$log;test_log_sha256=(Sha $log);actual_warp_com_query=$true;gpu_commands_issued=$false;game_operations=$false;native_game_runtime_verified=$false}
$target=Join-Path $output 'test-result.json';$record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $target -Encoding utf8
@{success=$record.success;checks=$record.checks;path=$target;sha256=(Sha $target)}|ConvertTo-Json -Compress
