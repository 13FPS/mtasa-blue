#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile actual SDK storage and the classic x86 specialization in ordinary
# processes. These checks do not establish native remote-context readiness.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/mta-remote-data-storage/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/sdk/multiplayer/CRemoteDataStorage.h', 'Client/sdk/multiplayer/CRemoteDataStorageImpl.h',
    'Client/game_de/RemoteDataStorage.h', 'Client/sdk/game/CGameAdapter.h', 'Client/sdk/game/CStats.h', 'Client/sdk/game/CWeaponInfo.h',
    'Client/multiplayer_sa/CRemoteDataSA.h', 'Client/game_sa/CPadSA.h', 'Client/sdk/game/CPad.h',
    'Client/sdk/game/Common.h', 'Shared/sdk/Common.h', 'Shared/sdk/SharedUtil.Defines.h', 'Shared/sdk/SharedUtil.IntTypes.h', 'Shared/sdk/CVector.h', 'Shared/sdk/CVector4D.h',
    'Shared/sdk/CControllerState.h', 'Shared/sdk/CControllerActions.h', 'Shared/sdk/CControlStateSynthesis.h',
    'Tests/definitive/RemoteDataStorageTests.cpp', 'Tests/definitive/test_remote_data_storage.ps1')
$sourceHashes = @($sourceNames | ForEach-Object {
    $taskPath = Join-Path $taskRoot $_
    [ordered]@{ path=$_; sha256=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$results = @()
$executionArtifacts = @()
$failure = $null
try {
    foreach ($architecture in @('x86', 'x64')) {
        $outputDirectory = Join-Path $outputRoot $architecture
        New-Item -ItemType Directory -Path $outputDirectory | Out-Null
        $environmentCommand = 'call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand, $architecture
        $compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
        if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
        foreach ($entry in $compilerEnvironment) {
            if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
        }
        $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $executable = Join-Path $outputDirectory 'remote_data_storage_tests.exe'
        $compileLog = Join-Path $outputDirectory 'compile.log'
        $testLog = Join-Path $outputDirectory 'test.log'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
            (Join-Path $PSScriptRoot 'RemoteDataStorageTests.cpp'),
            ('/Fo' + $outputDirectory + [IO.Path]::DirectorySeparatorChar), ('/Fe' + $executable))
        $compilerOutput = @(& $compiler @arguments 2>&1)
        $compilerExit = $LASTEXITCODE
        $compilerOutput | Set-Content -LiteralPath $compileLog -Encoding utf8
        $testExit = $null
        $testCount = $null
        if ($compilerExit -eq 0) {
            $testOutput = @(& $executable 2>&1)
            $testExit = $LASTEXITCODE
            $testOutput | Set-Content -LiteralPath $testLog -Encoding utf8
            if (($testOutput -join "`n") -match '^Passed (\d+) remote data storage checks') { $testCount = [int]$Matches[1] }
        }
        foreach ($artifactPath in @($executable, $compileLog, $testLog)) {
            if (Test-Path -LiteralPath $artifactPath) {
                $executionArtifacts += [ordered]@{ path=$artifactPath; bytes=(Get-Item -LiteralPath $artifactPath).Length;
                    sha256=(Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash.ToLowerInvariant() }
            }
        }
        $results += [ordered]@{ architecture=$architecture; compilerExit=$compilerExit; testExit=$testExit; checks=$testCount;
            executable=$executable; compiler=$compiler; compilerSha256=(Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash.ToLowerInvariant() }
        if ($compilerExit -ne 0 -or $testExit -ne 0 -or $null -eq $testCount) {
            throw "$architecture compilation/tests failed; see $outputDirectory"
        }
    }
} catch {
    $failure = $_.Exception.Message
}
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) {
    $taskPath = Join-Path $taskRoot $entry.path
    if (-not (Test-Path -LiteralPath $taskPath) -or
        (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
        $sourceUnchanged = $false
    }
}
$record = [ordered]@{ schema='mta_remote_data_storage_v1'; recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;
    success=($null -eq $failure -and $sourceUnchanged -and $results.Count -eq 2); sourceHashes=$sourceHashes; sourceUnchanged=$sourceUnchanged;
    results=$results; executionArtifacts=$executionArtifacts; failure=$failure; gameLaunched=$false; installationChanged=$false; gameProcessAttached=$false;
    classicFullPadLayoutVerified=($results.Count -gt 0 -and $results[0].architecture -eq 'x86' -and $results[0].testExit -eq 0);
    nativeInputReadVerified=$false; nativeInputWriteVerified=$false; remotePlayerContextVerified=$false; fullMtaClientBuilt=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Remote data storage checks failed: $failure; source unchanged=$sourceUnchanged" }
$record | ConvertTo-Json -Depth 8 -Compress
