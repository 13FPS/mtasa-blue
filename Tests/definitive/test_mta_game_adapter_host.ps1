#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the actual Core-owned adapter consumer and load an explicitly built Game DE DLL
# or installing the experimental transport DLL. Game ABI work remains separate.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ModulePath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/definitive-game-adapter-host/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/core/CGameAdapterModule.h', 'Client/core/CGameAdapterModule.cpp',
    'Client/sdk/game/CGameAdapter.h', 'Client/sdk/game/CGameRenderObservation.h', 'Shared/sdk/CControllerState.h',
    'Shared/sdk/CControllerActions.h', 'Shared/sdk/CControlStateSynthesis.h',
    'Tests/definitive/MtaGameAdapterHostTests.cpp', 'Tests/definitive/test_mta_game_adapter_host.ps1')
$sourceHashes = @($sourceNames | ForEach-Object {
    $taskPath = Join-Path $taskRoot $_
    [ordered]@{ path=$_; exists=(Test-Path -LiteralPath $taskPath); sha256=$(if (Test-Path -LiteralPath $taskPath) {
        (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant()
    } else { $null }) }
})
$results = @()
$ModulePath = (Resolve-Path -LiteralPath $ModulePath).Path
$moduleSha256 = (Get-FileHash -LiteralPath $ModulePath -Algorithm SHA256).Hash.ToLowerInvariant()
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
    $executable = Join-Path $outputDirectory 'mta_game_adapter_host_tests.exe'
    $fixture = Join-Path $outputDirectory 'shutdown-failure-fixture.dll'
    $fixtureExit = $null
    $facadeFixtures = @()
    $renderFixtures = @()
    if ($architecture -eq 'x64') {
        # This explicitly named failure provider tests the consumer's unload
        # policy; actual movement and capability checks still use ModulePath.
        $fixtureArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
            '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN', ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
            (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory 'fixture.obj')), ('/Fe' + $fixture),
            '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation')
        $fixtureOutput = @(& $compiler @fixtureArguments 2>&1)
        $fixtureExit = $LASTEXITCODE
        $fixtureOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'fixture-compile.log') -Encoding utf8
        if ($fixtureExit -ne 0) { $failure = 'Shutdown failure fixture did not compile'; break }
        foreach ($mode in 1..3) {
            $facadeFixture = Join-Path $outputDirectory ("facade-provider-$mode.dll")
            $facadeArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                "/DMTA_TEST_FACADE_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "facade-$mode.obj")), ('/Fe' + $facadeFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation')
            $facadeOutput = @(& $compiler @facadeArguments 2>&1)
            $facadeExit = $LASTEXITCODE
            $facadeLog = Join-Path $outputDirectory "facade-$mode-compile.log"
            $facadeOutput | Set-Content -LiteralPath $facadeLog -Encoding utf8
            $facadeFixtures += [ordered]@{ mode=$mode; path=$facadeFixture; compilerExit=$facadeExit; log=$facadeLog;
                sha256=$(if (Test-Path -LiteralPath $facadeFixture) { (Get-FileHash -LiteralPath $facadeFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($facadeExit -ne 0) { $failure = "Facade fixture $mode did not compile"; break }
        }
        if ($failure) { break }
        foreach ($mode in 1..5) {
            $renderFixture = Join-Path $outputDirectory ("render-provider-$mode.dll")
            $renderArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                "/DMTA_TEST_RENDER_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "render-$mode.obj")), ('/Fe' + $renderFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation')
            $renderOutput = @(& $compiler @renderArguments 2>&1)
            $renderExit = $LASTEXITCODE
            $renderLog = Join-Path $outputDirectory "render-$mode-compile.log"
            $renderOutput | Set-Content -LiteralPath $renderLog -Encoding utf8
            $renderFixtures += [ordered]@{ mode=$mode; path=$renderFixture; compilerExit=$renderExit; log=$renderLog;
                sha256=$(if (Test-Path -LiteralPath $renderFixture) { (Get-FileHash -LiteralPath $renderFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($renderExit -ne 0) { $failure = "Render fixture $mode did not compile"; break }
        }
        if ($failure) { break }
        $renderV2Fixtures = @()
        foreach ($mode in 1..7) {
            $renderFixture = Join-Path $outputDirectory ("render-v2-provider-$mode.dll")
            $renderArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE','/DMTA_TEST_RENDER_V2=1',
                "/DMTA_TEST_RENDER_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "render-v2-$mode.obj")), ('/Fe' + $renderFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation', '/EXPORT:CreateGameRenderObservationV2')
            $renderOutput = @(& $compiler @renderArguments 2>&1)
            $renderExit = $LASTEXITCODE
            $renderLog = Join-Path $outputDirectory "render-v2-$mode-compile.log"
            $renderOutput | Set-Content -LiteralPath $renderLog -Encoding utf8
            $renderV2Fixtures += [ordered]@{ mode=$mode; path=$renderFixture; compilerExit=$renderExit; log=$renderLog;
                sha256=$(if (Test-Path -LiteralPath $renderFixture) { (Get-FileHash -LiteralPath $renderFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($renderExit -ne 0) { $failure = "Render version2 fixture $mode did not compile"; break }
        }
        if ($failure) { break }
    }
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
        (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), (Join-Path $taskRoot 'Client/core/CGameAdapterModule.cpp'),
        ('/Fo' + $outputDirectory + [IO.Path]::DirectorySeparatorChar), ('/Fe' + $executable))
    $compilerOutput = @(& $compiler @arguments 2>&1)
    $compilerExit = $LASTEXITCODE
    $compilerOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'compile.log') -Encoding utf8
    $testExit = $null
    $testCount = $null
    if ($compilerExit -eq 0) {
        $testArguments = @($ModulePath)
        if ($architecture -eq 'x64') { $testArguments += $fixture; $testArguments += @($facadeFixtures | ForEach-Object path); $testArguments += @($renderFixtures | ForEach-Object path); $testArguments += @($renderV2Fixtures | ForEach-Object path) }
        $testOutput = @(& $executable @testArguments 2>&1)
        $testExit = $LASTEXITCODE
        $testOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'test.log') -Encoding utf8
        $testText = $testOutput -join "`n"
        if ($testText -match 'Passed (\d+) MTA game adapter host checks') { $testCount = [int]$Matches[1] }
    }
    $results += [ordered]@{ architecture=$architecture; compilerExit=$compilerExit; testExit=$testExit; checks=$testCount;
        shutdownFailureFixtureCompilerExit=$fixtureExit; shutdownFailureFixture=$fixture;
        shutdownFailureFixtureSha256=$(if (Test-Path -LiteralPath $fixture) { (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null });
        facadeProviderFixtures=$facadeFixtures;
        renderProviderFixtures=$renderFixtures;
        renderV2ProviderFixtures=$(if ($architecture -eq 'x64') { $renderV2Fixtures } else { @() });
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
    success=($null -eq $failure -and $sourceUnchanged); sourceHashes=$sourceHashes; sourceUnchanged=$sourceUnchanged;
    results=$results; failure=$failure; modulePath=$ModulePath; moduleSha256=$moduleSha256; gameLaunched=$false; installationChanged=$false;
    existingBridgeRebuilt=$false; fullMtaClientBuilt=$false; facadeFixtureHasWorkingCGame=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "MTA Core adapter host checks failed: $failure; source unchanged=$sourceUnchanged" }
$record | ConvertTo-Json -Depth 8 -Compress
