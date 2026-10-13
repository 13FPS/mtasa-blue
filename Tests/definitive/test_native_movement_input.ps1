#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/mta-native-movement-input/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/game_de/NativeMovementInput.h','Client/game_de/NativeMovementInput.cpp',
    'Client/game_de/NativeInputRouting.h','Client/game_de/NativePadReader.h','Client/game_de/NativePadReader.cpp',
    'Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h','Tests/definitive/NativeMovementInputTests.cpp',
    'Tests/definitive/test_native_movement_input.ps1')
$sourceHashes = @($sourceNames | ForEach-Object {
    [ordered]@{ path=$_; sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot $_) -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$environmentCommand = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand
$compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
foreach ($entry in $compilerEnvironment) {
    if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
}
$compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
$common = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
    ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'vendor/detours/4.0.1/src')))
$detoursSources = @('detours.cpp','modules.cpp','disasm.cpp','image.cpp','creatwth.cpp') | ForEach-Object {
    Join-Path $taskRoot ('vendor/detours/4.0.1/src/'+$_)
}
$detoursArguments = $common + @('/w','/c') + $detoursSources + @('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar)
$detoursOutput = @(& $compiler @detoursArguments 2>&1)
$detoursExit = $LASTEXITCODE
$detoursOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'detours-compile.log') -Encoding utf8
$executable = Join-Path $outputRoot 'native_movement_input_tests.exe'
$objects = @('detours.obj','modules.obj','disasm.obj','image.obj','creatwth.obj') | ForEach-Object { Join-Path $outputRoot $_ }
$arguments = $common + @('/W4','/WX',(Join-Path $PSScriptRoot 'NativeMovementInputTests.cpp'),
    (Join-Path $taskRoot 'Client/game_de/NativePadReader.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),
    ('/Fe'+$executable),'/link','bcrypt.lib') + $objects
$compilerOutput = @(& $compiler @arguments 2>&1)
$compilerExit = $LASTEXITCODE
$compilerOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$testExit = $null
$testCount = $null
if ($detoursExit -eq 0 -and $compilerExit -eq 0) {
    $testOutput = @(& $executable 2>&1)
    $testExit = $LASTEXITCODE
    $testOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'test.log') -Encoding utf8
    if (($testOutput -join "`n") -match 'Passed (\d+) production native movement callback checks') { $testCount = [int]$Matches[1] }
}
$unchanged = $true
foreach ($entry in $sourceHashes) {
    if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $unchanged = $false }
}
$record = [ordered]@{ recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;architecture='x64';
    success=($detoursExit -eq 0 -and $compilerExit -eq 0 -and $testExit -eq 0 -and $null -ne $testCount -and $unchanged);
    detoursCompilerExit=$detoursExit;compilerExit=$compilerExit;testExit=$testExit;checks=$testCount;
    sourceHashes=$sourceHashes;sourceUnchanged=$unchanged;executable=$executable;
    gameLaunched=$false;gameProcessAttached=$false;gameInputWritten=$false;nativeHooksInstalled=$false;
    syntheticOwnProcessPadWritten=$true;productionCallbackTested=$true;nativeRuntimeVerified=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Native movement callback checks failed; see $outputRoot" }
$record | ConvertTo-Json -Depth 8 -Compress
