#!/usr/bin/env pwsh
#requires -Version 7.0
# Focused actual Probe final edge and shared lifetime. No game image is read.
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$run=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-native-present-policy-edge/'+$run)
$scratch=Join-Path $taskRoot ('Build/tmp/native-present-policy-edge-'+$run)
[void][IO.Directory]::CreateDirectory($outputRoot)
[void][IO.Directory]::CreateDirectory($scratch)
$sourceNames=@('Client/game_de/NativeFrameBoundaryProbe.h','Client/game_de/NativeFrameBoundaryProbe.cpp',
    'Client/game_de/NativePresentTransitionGate.h','Client/game_de/NativePresentTransitionGate.cpp',
    'Client/game_de/NativeCustomPresentProbe.h','Client/game_de/NativeCustomPresentProbe.cpp',
    'Client/sdk/game/CGameRenderObservation.h','Client/sdk/game/CGameRenderEndpointDiagnostic.h',
    'Client/sdk/game/CGameRenderSetupDiagnostic.h','Client/sdk/game/CGameRenderRootsDiagnostic.h',
    'Client/sdk/game/CGameRenderDeviceLease.h','Client/sdk/game/CGameRenderDeviceWindowLease.h',
    'Tests/definitive/NativePresentPolicyOwnedImageFixture.inl',
    'Tests/definitive/NativePresentPolicyEdgeTests.cpp','Tests/definitive/test_native_present_policy_edge.ps1',
    'vendor/detours/4.0.1/src/detours.h','vendor/detours/4.0.1/src/detours.cpp',
    'vendor/detours/4.0.1/src/modules.cpp','vendor/detours/4.0.1/src/disasm.cpp',
    'vendor/detours/4.0.1/src/image.cpp','vendor/detours/4.0.1/src/creatwth.cpp')
$markers=@('actualNativePolicyFreshRootsTested','actualNativePolicyExactForwardTested',
    'actualNativePolicyClockReadReleaseTested','actualNativePolicyPoisonTested',
    'actualNativePolicySharedDiagnosticLifetimeTested','actualNativePolicyPreconfigurationLifecycleTested')
$hashes=@()
$detoursExit=$null
$compilerExit=$null
$testExit=$null
$checks=$null
$timedOut=$false
$failure=$null
$scratchRemoved=$false
$sourceUnchanged=$false
$observed=@{}
foreach($marker in $markers){$observed[$marker]=$false}
$priorTemp=[Environment]::GetEnvironmentVariable('TEMP','Process')
$priorTmp=[Environment]::GetEnvironmentVariable('TMP','Process')
try{
    $hashes=@($sourceNames|ForEach-Object {@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot $_) -Algorithm SHA256).Hash.ToLowerInvariant()}})
    [Environment]::SetEnvironmentVariable('TEMP',$scratch,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$scratch,'Process')
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(-not $visualStudio){throw 'Existing Visual Studio x64 tools required'}
    $developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $compilerEnvironment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand)
    if($LASTEXITCODE -ne 0){throw 'Compiler environment initialization failed'}
    foreach($entry in $compilerEnvironment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    # VsDevCmd may redefine TEMP/TMP. Keep compiler/linker scratch in this task.
    [Environment]::SetEnvironmentVariable('TEMP',$scratch,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$scratch,'Process')
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $common=@('/nologo','/std:c++17','/EHsc','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        '/DDETOURS_MTA_VM_TRANSACTION_NODES',('/I'+(Join-Path $taskRoot 'vendor/detours/4.0.1/src')),
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')))
    $detours=@('detours.cpp','modules.cpp','disasm.cpp','image.cpp','creatwth.cpp')|ForEach-Object {Join-Path $taskRoot ('vendor/detours/4.0.1/src/'+$_)}
    $vendorOutput=@(& $compiler @common /w /c @detours ('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar) 2>&1)
    $detoursExit=$LASTEXITCODE
    $vendorOutput|Set-Content -LiteralPath (Join-Path $outputRoot 'detours-compile.log') -Encoding utf8NoBOM
    $objects=@('detours.obj','modules.obj','disasm.obj','image.obj','creatwth.obj')|ForEach-Object {Join-Path $scratch $_}
    $executable=Join-Path $outputRoot 'native_present_policy_edge_tests.exe'
    # Private inclusion exposes the real guard/current-code helpers. Owned
    # executable-image profiles pass their ordinary MEM_IMAGE checks; no test
    # switch is added to production, and no stock image is mapped or extracted.
    $arguments=$common+@('/W4','/WX',(Join-Path $PSScriptRoot 'NativePresentPolicyEdgeTests.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePresentTransitionGate.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeCustomPresentProbe.cpp'),
        ('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),'/link','/INCREMENTAL:NO','bcrypt.lib','user32.lib')+$objects
    $compilerOutput=@(& $compiler @arguments 2>&1)
    $compilerExit=$LASTEXITCODE
    $compilerOutput|Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8NoBOM
    if($detoursExit -eq 0 -and $compilerExit -eq 0){
        $log=Join-Path $outputRoot 'test.log'
        $errors=Join-Path $outputRoot 'test-errors.log'
        $process=Start-Process -FilePath $executable -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError $errors
        if(-not $process.WaitForExit(30000)){$timedOut=$true;$process.Kill();$process.WaitForExit()}
        $testExit=$process.ExitCode
        $process.Dispose()
        $text=(@(Get-Content -LiteralPath $log)+@(Get-Content -LiteralPath $errors)) -join "`n"
        if($text -match 'Passed (\d+) native present policy edge checks'){$checks=[int]$Matches[1]}
        foreach($marker in $markers){$observed[$marker]=[regex]::IsMatch($text,'(?m)^'+[regex]::Escape($marker)+'=1\s*$')}
    }
    $sourceUnchanged=$true
    foreach($pin in $hashes){if((Get-FileHash -LiteralPath (Join-Path $taskRoot $pin.path) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pin.sha256){$sourceUnchanged=$false}}
}catch{$failure=$_.Exception.Message}
finally{
    [Environment]::SetEnvironmentVariable('TEMP',$priorTemp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$priorTmp,'Process')
    try{
        $expectedParent=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp'))
        if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($scratch)) -cne $expectedParent){throw 'Scratch path escaped task parent'}
        for($ancestor=$scratch;$ancestor;$ancestor=[IO.Path]::GetDirectoryName($ancestor)){
            if((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked scratch ancestor refused'}
        }
        $children=@(Get-ChildItem -LiteralPath $scratch -Force)
        foreach($item in $children){
            if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
               [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($item.FullName)) -cne [IO.Path]::GetFullPath($scratch)){throw 'Unexpected scratch entry retained'}
        }
        foreach($item in $children){Remove-Item -LiteralPath $item.FullName -Force}
        [IO.Directory]::Delete($scratch,$false)
        $scratchRemoved=$true
    }catch{$failure=($failure+';scratch:'+ $_.Exception.Message)}
}
$success=($detoursExit -eq 0 -and $compilerExit -eq 0 -and $testExit -eq 0 -and $null -ne $checks -and
    -not $timedOut -and $sourceUnchanged -and $scratchRemoved -and -not $failure -and @($markers|Where-Object {-not $observed[$_]}).Count -eq 0)
$result=[ordered]@{schema='mta_native_present_policy_edge_focused_tests_v1';success=$success;recordedAt=[DateTimeOffset]::Now.ToString('o');
    directory=$outputRoot;detoursCompilerExit=$detoursExit;compilerExit=$compilerExit;testExit=$testExit;checks=$checks;
    timedOut=$timedOut;failure=$failure;sourceHashes=$hashes;sourceUnchanged=$sourceUnchanged;scratchRemoved=$scratchRemoved;
    originalGameImageRead=$false;historicalMatrixReplayed=$false;actualProbeAndMgateImplementationsTested=$success;
    ownedMemImageProfilesOnly=$true;gameLaunched=$false;gameProcessAttached=$false;gameHooksInstalled=$false;gameNativeInvoked=$false;
    sdkDisableVSyncProviderTested=$false;nativeRuntimeVerified=$false}
foreach($marker in $markers){$result[$marker]=$observed[$marker]}
$result|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
if(-not $success){throw ('Focused policy-edge tests failed; see '+$outputRoot)}
$result|ConvertTo-Json -Depth 8 -Compress
