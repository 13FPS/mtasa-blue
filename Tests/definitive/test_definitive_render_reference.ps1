#!/usr/bin/env pwsh
#requires -Version 7.0
# Run Core's production value-only lifecycle with a fake monotonic clock/observer.
# No DLL, game image, renderer, input device or process attachment is involved.
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$output=Join-Path $root ('Build/mta-core-render-lifecycle/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
function Sha([string]$path){return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$sources=@('Client/core/DefinitiveRenderReference.h','Client/core/DefinitiveRenderEndpointLog.h','Client/sdk/game/CGameRenderEndpointDiagnostic.h','Client/sdk/game/CGameRenderObservation.h','Client/sdk/game/CGameAdapter.h',
    'Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h',
    'Tests/definitive/DefinitiveRenderReferenceTests.cpp','Tests/definitive/test_definitive_render_reference.ps1')
$hashes=[ordered]@{}
foreach($name in $sources){$hashes[$name]=Sha (Join-Path $root $name)}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
$runs=@()
foreach($architecture in @('x86','x64')){
    $environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $dev,$architecture)
    if($LASTEXITCODE -ne 0){throw 'Compiler environment failed.'}
    foreach($entry in $environment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $compiler=Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$architecture+'/cl.exe')
    $binary=Join-Path $output ('render-lifecycle-'+$architecture+'.exe')
    $compileLog=Join-Path $output ('compile-'+$architecture+'.log')
    & $compiler /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX ("/I"+(Join-Path $root 'Shared/sdk')) (Join-Path $PSScriptRoot 'DefinitiveRenderReferenceTests.cpp') ("/Fo"+(Join-Path $output ($architecture+'.obj'))) ("/Fe"+$binary) /link /NOLOGO 2>&1 | Set-Content -LiteralPath $compileLog -Encoding utf8
    if($LASTEXITCODE -ne 0){throw "Render lifecycle compilation failed: $compileLog"}
    $runLog=Join-Path $output ('run-'+$architecture+'.json')
    & $binary 2>&1 | Set-Content -LiteralPath $runLog -Encoding utf8
    if($LASTEXITCODE -ne 0){throw "Render lifecycle test failed: $runLog"}
    $result=Get-Content -LiteralPath $runLog -Raw | ConvertFrom-Json
    if(-not $result.success){throw 'Render lifecycle fixture reported failure.'}
    $runs+=@{architecture=$architecture;checks=$result.checks;binary=$binary;binary_sha256=(Sha $binary);compile_log=$compileLog;compile_log_sha256=(Sha $compileLog);run_log=$runLog;run_log_sha256=(Sha $runLog)}
}
foreach($name in $sources){if((Sha (Join-Path $root $name)) -cne $hashes[$name]){throw 'Lifecycle source changed during testing.'}}
$record=[ordered]@{success=$true;created_at=[DateTimeOffset]::Now.ToString('o');directory=$output;source_hashes=$hashes;sources_unchanged=$true;runs=$runs;
    scope='Exact production Core render lifecycle, injected value-only observer/clock, cancellation/error/cleanup/deadline preservation';game_operations=$false;native_callback_verified=$false;full_mta_ready=$false}
$target=Join-Path $output 'test-result.json'
$record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $target -Encoding utf8
@{success=$true;path=$target;sha256=(Sha $target);checks_each_arch=$runs[0].checks}|ConvertTo-Json -Compress
