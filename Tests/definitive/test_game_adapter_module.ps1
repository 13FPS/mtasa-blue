#!/usr/bin/env pwsh
#requires -Version 7.0
# Build the actual x64 game-module DLL and exercise its exported factory. These
# checks validate MTA ownership/storage without loading a game or rebuilding CLEO.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/mta-game-de-module/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/sdk/game/CGameAdapter.h', 'Client/game_de/GameAdapter.cpp', 'Client/game_de/ControllerSnapshot.h',
    'Client/game_de/NativePadReader.h', 'Client/game_de/NativePadReader.cpp',
    'Client/definitive/MtaControllerSnapshot.h', 'Shared/sdk/CControllerState.h',
    'Shared/sdk/CControllerActions.h', 'Shared/sdk/CControlStateSynthesis.h',
    'Tests/definitive/GameAdapterModuleTests.cpp', 'Tests/definitive/test_game_adapter_module.ps1')
$sourceHashes = @($sourceNames | ForEach-Object {
    $taskPath = Join-Path $taskRoot $_
    [ordered]@{ path=$_; exists=(Test-Path -LiteralPath $taskPath); sha256=$(if (Test-Path -LiteralPath $taskPath) {
        (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant()
    } else { $null }) }
})
$environmentCommand = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand
$compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
foreach ($entry in $compilerEnvironment) {
    if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
$compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$module = Join-Path $outputRoot 'game_de.dll'
$library = Join-Path $outputRoot 'game_de.lib'
$common = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
    ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')))
$moduleExit = $null
$moduleSource = Join-Path $taskRoot 'Client/game_de/GameAdapter.cpp'
if (Test-Path -LiteralPath $moduleSource) {
    $moduleArguments = $common + @('/LD','/DMTA_GAME_DE_EXPORTS', $moduleSource, (Join-Path $taskRoot 'Client/game_de/NativePadReader.cpp'),
        ('/Fo' + $outputRoot + [IO.Path]::DirectorySeparatorChar), ('/Fe' + $module), '/link', ('/IMPLIB:' + $library), 'bcrypt.lib')
    $moduleOutput = @(& $compiler @moduleArguments 2>&1)
    $moduleExit = $LASTEXITCODE
    $moduleOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'module-compile.log') -Encoding utf8
}
# Before implementation exists this same test links against the declared SDK
# factory alone, capturing the causal missing-symbol red instead of a fake DLL.
$executable = Join-Path $outputRoot 'game_adapter_module_tests.exe'
$arguments = $common + @((Join-Path $PSScriptRoot 'GameAdapterModuleTests.cpp'),
    ('/Fo' + (Join-Path $outputRoot 'test.obj')), ('/Fe' + $executable))
if ($moduleExit -eq 0) { $arguments += $library }
$compilerOutput = @(& $compiler @arguments 2>&1)
$compilerExit = $LASTEXITCODE
$compilerOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$testExit = $null
$testCount = $null
if ($compilerExit -eq 0 -and $moduleExit -eq 0) {
    $testOutput = @(& $executable 2>&1)
    $testExit = $LASTEXITCODE
    $testOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'test.log') -Encoding utf8
    if (($testOutput -join "`n") -match 'Passed (\d+) MTA Game DE adapter checks') { $testCount = [int]$Matches[1] }
}
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) {
    $taskPath = Join-Path $taskRoot $entry.path
    if ((Test-Path -LiteralPath $taskPath) -ne $entry.exists -or
        ($entry.exists -and (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256)) {
        $sourceUnchanged = $false
    }
}
$record = [ordered]@{ recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;
    success=($moduleExit -eq 0 -and $compilerExit -eq 0 -and $testExit -eq 0 -and $null -ne $testCount -and $sourceUnchanged);
    sourceHashes=$sourceHashes; sourceUnchanged=$sourceUnchanged; architecture='x64'; moduleCompilerExit=$moduleExit;
    compilerExit=$compilerExit; testExit=$testExit; checks=$testCount; module=$module;
    moduleSha256=$(if (Test-Path -LiteralPath $module) { (Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null });
    executable=$executable; gameLaunched=$false; installationChanged=$false; cleoLoaded=$false;
    standaloneTransportBuilt=$false; fullMtaClientBuilt=$false; engineInputBindingVerified=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "MTA Game DE adapter checks failed; see $outputRoot" }
$record | ConvertTo-Json -Depth 8 -Compress
