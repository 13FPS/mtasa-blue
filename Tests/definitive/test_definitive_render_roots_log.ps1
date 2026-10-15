#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the production optional scalar formatter in disposable x86/x64 hosts.
# No game, renderer, process attachment, DLL loading or hook installation occurs.
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$output=Join-Path $root ('Build/mta-core-render-roots-log/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
function Sha([string]$path){return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$sources=@('Client/core/DefinitiveRenderRootsLog.h','Client/sdk/game/CGameRenderRootsDiagnostic.h','Client/sdk/game/CGameRenderObservation.h','Client/sdk/game/CGameAdapter.h',
    'Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h',
    'Tests/definitive/DefinitiveRenderRootsLogTests.cpp','Tests/definitive/test_definitive_render_roots_log.ps1',
    'utils/definitive_edition/render_roots_log.py','Tests/definitive/test_render_roots_log.py','utils/definitive_edition/session_log.py')
$hashes=[ordered]@{}
foreach($name in $sources){$hashes[$name]=Sha (Join-Path $root $name)}
$runs=@()
$pythonExit=$null
$pythonChecks=$null
$pythonLog=Join-Path $output 'python-tests.log'
$failure=$null
try {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if($LASTEXITCODE -ne 0 -or -not $vs){throw 'Visual C++ toolchain not found.'}
    $dev=Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    foreach($architecture in @('x86','x64')){
        $environment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $dev,$architecture)
        if($LASTEXITCODE -ne 0){throw 'Compiler environment failed.'}
        foreach($entry in $environment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
        $compiler=Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$architecture+'/cl.exe')
        # Avoid Windows' legacy installer-name heuristic for this ordinary,
        # unprivileged scalar fixture; its filename must not request elevation.
        $binary=Join-Path $output ('render-scalar-log-'+$architecture+'.exe')
        $compileLog=Join-Path $output ('compile-'+$architecture+'.log')
        & $compiler /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX ("/I"+(Join-Path $root 'Shared/sdk')) (Join-Path $PSScriptRoot 'DefinitiveRenderRootsLogTests.cpp') ("/Fo"+(Join-Path $output ($architecture+'.obj'))) ("/Fe"+$binary) /link /NOLOGO 2>&1 | Set-Content -LiteralPath $compileLog -Encoding utf8
        $compilerExit=$LASTEXITCODE
        if($compilerExit -ne 0){throw "Roots log compilation failed: $compileLog"}
        $runLog=Join-Path $output ('run-'+$architecture+'.json')
        & $binary 2>&1 | Set-Content -LiteralPath $runLog -Encoding utf8
        $testExit=$LASTEXITCODE
        if($testExit -ne 0){throw "Roots log test failed: $runLog"}
        $result=Get-Content -LiteralPath $runLog -Raw | ConvertFrom-Json
        if(-not $result.success){throw 'Roots log fixture reported failure.'}
        $runs+=@{architecture=$architecture;checks=$result.checks;compiler_exit=$compilerExit;test_exit=$testExit;binary=$binary;binary_sha256=(Sha $binary);compile_log=$compileLog;compile_log_sha256=(Sha $compileLog);run_log=$runLog;run_log_sha256=(Sha $runLog)}
    }
    # Validate the strict saved-file reader without any game or native host.
    Push-Location $root
    try {
        & python -B -m unittest discover -s Tests/definitive -p test_render_roots_log.py 2>&1 | Set-Content -LiteralPath $pythonLog -Encoding utf8
        $pythonExit=$LASTEXITCODE
    } finally { Pop-Location }
    $pythonText=Get-Content -LiteralPath $pythonLog -Raw
    if($pythonText -match 'Ran (\d+) tests'){$pythonChecks=[int]$Matches[1]}
    if($pythonExit -ne 0 -or $null -eq $pythonChecks){throw 'Roots saved-file reader tests failed.'}
} catch {$failure=$_.Exception.Message}
$unchanged=$true
foreach($name in $sources){if((Sha (Join-Path $root $name)) -cne $hashes[$name]){$unchanged=$false}}
$success=($null -eq $failure -and $unchanged -and $runs.Count -eq 2 -and $pythonExit -eq 0)
$record=[ordered]@{success=$success;created_at=[DateTimeOffset]::Now.ToString('o');directory=$output;source_hashes=$hashes;sources_unchanged=$unchanged;runs=$runs;failure=$failure;
    python_tests=$pythonChecks;python_exit=$pythonExit;python_log=$pythonLog;python_log_sha256=$(if(Test-Path -LiteralPath $pythonLog){Sha $pythonLog}else{$null});
    scope='Exact production14-scalar roots formatter/framing/deduplication/sink retry/terminal reservation and strict saved-log reader; separate from actual native roots or GPU acceptance';game_operations=$false;hooks_installed=$false;gpu_commands_issued=$false;full_mta_ready=$false}
$target=Join-Path $output 'test-result.json'
$record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $target -Encoding utf8
@{success=$success;path=$target;sha256=(Sha $target);architectures=$runs.Count}|ConvertTo-Json -Compress
if(-not $success){exit 1}
