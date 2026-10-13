#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the production reader's bounded copy contract in ordinary test
# processes. Binding rejection here never proves a native game input read.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/mta-native-pad-reader/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/game_de/NativePadReader.h', 'Client/game_de/NativePadReader.cpp',
    'Client/sdk/game/CGameAdapter.h', 'Shared/sdk/CControllerActions.h', 'Shared/sdk/CControlStateSynthesis.h',
    'Shared/sdk/CControllerState.h', 'Tests/definitive/NativePadReaderTests.cpp', 'Tests/definitive/test_native_pad_reader.ps1')
$sourceHashes = @($sourceNames | ForEach-Object {
    $taskPath = Join-Path $taskRoot $_
    [ordered]@{ path=$_; exists=(Test-Path -LiteralPath $taskPath); sha256=$(if (Test-Path -LiteralPath $taskPath) {
        (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant()
    } else { $null }) }
})
$results = @()
$failure = $null
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
    $executable = Join-Path $outputDirectory 'native_pad_reader_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I' + (Join-Path $taskRoot 'Shared/sdk')), (Join-Path $PSScriptRoot 'NativePadReaderTests.cpp'))
    $readerSource = Join-Path $taskRoot 'Client/game_de/NativePadReader.cpp'
    if (Test-Path -LiteralPath $readerSource) { $arguments += $readerSource }
    $arguments += @(('/Fo' + $outputDirectory + [IO.Path]::DirectorySeparatorChar), ('/Fe' + $executable), '/link', 'bcrypt.lib')
    $compilerOutput = @(& $compiler @arguments 2>&1)
    $compilerExit = $LASTEXITCODE
    $compilerOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'compile.log') -Encoding utf8
    $testExit = $null
    $testCount = $null
    if ($compilerExit -eq 0) {
        $testOutput = @(& $executable 2>&1)
        $testExit = $LASTEXITCODE
        $testOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'test.log') -Encoding utf8
        if (($testOutput -join "`n") -match 'Passed (\d+) native pad reader checks') { $testCount = [int]$Matches[1] }
    }
    $results += [ordered]@{ architecture=$architecture; compilerExit=$compilerExit; testExit=$testExit; checks=$testCount;
        executable=$executable; executableSha256=$(if (Test-Path -LiteralPath $executable) {
            (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
        } else { $null }) }
    if ($compilerExit -ne 0 -or $testExit -ne 0 -or $null -eq $testCount) {
        $failure = "$architecture compilation/tests failed; see $outputDirectory"
        break
    }
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
    success=($null -eq $failure -and $sourceUnchanged -and $results.Count -eq 2); sourceHashes=$sourceHashes; sourceUnchanged=$sourceUnchanged;
    results=$results; failure=$failure; gameLaunched=$false; installationChanged=$false; gameProcessAttached=$false;
    nativeInputReadVerified=$false; fullMtaClientBuilt=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Native pad reader checks failed: $failure; source unchanged=$sourceUnchanged" }
$record | ConvertTo-Json -Depth 8 -Compress
