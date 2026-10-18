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
    'Client/sdk/game/CGameClockService.h', 'Client/sdk/game/CGameLocalInputLease.h', 'Client/sdk/game/CGameFrontendInputLease.h',
    'Client/sdk/game/CGameAdapter.h', 'Client/sdk/game/CGameRenderObservation.h', 'Client/sdk/game/CGameRenderEndpointDiagnostic.h', 'Client/sdk/game/CGameRenderSetupDiagnostic.h', 'Client/sdk/game/CGameRenderRootsDiagnostic.h', 'Client/sdk/game/CGameRenderDeviceLease.h', 'Client/sdk/game/CGameRenderDeviceWindowLease.h', 'Shared/sdk/CControllerState.h',
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
    $clockFixtures = @()
    $localInputFixtures = @()
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
        foreach ($mode in 1..4) {
            $clockFixture = Join-Path $outputDirectory ("clock-provider-$mode.dll")
            $clockArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                "/DMTA_TEST_CLOCK_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "clock-$mode.obj")), ('/Fe' + $clockFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation', '/EXPORT:BeginGameClockServiceV1',
                '/EXPORT:SubmitGameClockCommandV1', '/EXPORT:RenewGameClockServiceV1', '/EXPORT:ReadGameClockServiceV1')
            if ($mode -ne 4) { $clockArguments += '/EXPORT:EndGameClockServiceV1' }
            $clockOutput = @(& $compiler @clockArguments 2>&1)
            $clockExit = $LASTEXITCODE
            $clockLog = Join-Path $outputDirectory "clock-$mode-compile.log"
            $clockOutput | Set-Content -LiteralPath $clockLog -Encoding utf8
            $clockFixtures += [ordered]@{ mode=$mode; path=$clockFixture; compilerExit=$clockExit; log=$clockLog;
                sha256=$(if (Test-Path -LiteralPath $clockFixture) { (Get-FileHash -LiteralPath $clockFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($clockExit -ne 0) { $failure = "Clock provider fixture $mode did not compile"; break }
        }
        if ($failure) { break }
        foreach ($mode in 1..4) {
            $localFixture = Join-Path $outputDirectory ("local-input-provider-$mode.dll")
            $localArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                "/DMTA_TEST_LOCAL_INPUT_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "local-input-$mode.obj")), ('/Fe' + $localFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation', '/EXPORT:BeginGameLocalInputLeaseV1',
                '/EXPORT:RenewGameLocalInputLeaseV1', '/EXPORT:ReadGameLocalInputLeaseV1')
            if ($mode -ne 2) { $localArguments += '/EXPORT:EndGameLocalInputLeaseV1' }
            $localOutput = @(& $compiler @localArguments 2>&1)
            $localExit = $LASTEXITCODE
            $localLog = Join-Path $outputDirectory "local-input-$mode-compile.log"
            $localOutput | Set-Content -LiteralPath $localLog -Encoding utf8
            $localInputFixtures += [ordered]@{ mode=$mode; path=$localFixture; compilerExit=$localExit; log=$localLog;
                sha256=$(if (Test-Path -LiteralPath $localFixture) { (Get-FileHash -LiteralPath $localFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($localExit -ne 0) { $failure = "Local input provider fixture $mode did not compile"; break }
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
    if ($architecture -eq 'x64') {
        $renderV3Fixtures = @()
        foreach ($mode in 1..8) {
            $renderFixture = Join-Path $outputDirectory ("render-v3-provider-$mode.dll")
            $renderArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                '/DMTA_TEST_RENDER_V2=1','/DMTA_TEST_RENDER_V3=1',"/DMTA_TEST_RENDER_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "render-v3-$mode.obj")), ('/Fe' + $renderFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation', '/EXPORT:CreateGameRenderObservationV2', '/EXPORT:CreateGameRenderObservationV3')
            $renderOutput = @(& $compiler @renderArguments 2>&1)
            $renderExit = $LASTEXITCODE
            $renderLog = Join-Path $outputDirectory "render-v3-$mode-compile.log"
            $renderOutput | Set-Content -LiteralPath $renderLog -Encoding utf8
            $renderV3Fixtures += [ordered]@{ mode=$mode; path=$renderFixture; compilerExit=$renderExit; log=$renderLog;
                sha256=$(if (Test-Path -LiteralPath $renderFixture) { (Get-FileHash -LiteralPath $renderFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($renderExit -ne 0) { $failure = "Render version3 fixture $mode did not compile"; break }
        }
        if ($failure) { break }
    }
    if ($architecture -eq 'x64') {
        $renderV4Fixtures = @()
        foreach ($mode in 1..8) {
            $renderFixture = Join-Path $outputDirectory ("render-v4-provider-$mode.dll")
            $renderArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                '/DMTA_TEST_RENDER_V2=1','/DMTA_TEST_RENDER_V3=1','/DMTA_TEST_RENDER_V4=1',"/DMTA_TEST_RENDER_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "render-v4-$mode.obj")), ('/Fe' + $renderFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation', '/EXPORT:CreateGameRenderObservationV2', '/EXPORT:CreateGameRenderObservationV3', '/EXPORT:CreateGameRenderObservationV4')
            $renderOutput = @(& $compiler @renderArguments 2>&1)
            $renderExit = $LASTEXITCODE
            $renderLog = Join-Path $outputDirectory "render-v4-$mode-compile.log"
            $renderOutput | Set-Content -LiteralPath $renderLog -Encoding utf8
            $renderV4Fixtures += [ordered]@{ mode=$mode; path=$renderFixture; compilerExit=$renderExit; log=$renderLog;
                sha256=$(if (Test-Path -LiteralPath $renderFixture) { (Get-FileHash -LiteralPath $renderFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($renderExit -ne 0) { $failure = "Render version4 fixture $mode did not compile"; break }
        }
        if ($failure) { break }
        $renderV5Fixtures = @()
        foreach ($mode in 1..8) {
            $renderFixture = Join-Path $outputDirectory ("render-v5-provider-$mode.dll")
            $renderArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                '/DMTA_TEST_RENDER_V2=1','/DMTA_TEST_RENDER_V3=1','/DMTA_TEST_RENDER_V4=1','/DMTA_TEST_RENDER_V5=1',"/DMTA_TEST_RENDER_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "render-v5-$mode.obj")), ('/Fe' + $renderFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation', '/EXPORT:CreateGameRenderObservationV2', '/EXPORT:CreateGameRenderObservationV3', '/EXPORT:CreateGameRenderObservationV4', '/EXPORT:CreateGameRenderObservationV5')
            if ($mode -ne 6) { $renderArguments += '/EXPORT:ReadGameRenderEndpointDiagnosticV1' }
            $renderOutput = @(& $compiler @renderArguments 2>&1)
            $renderExit = $LASTEXITCODE
            $renderLog = Join-Path $outputDirectory "render-v5-$mode-compile.log"
            $renderOutput | Set-Content -LiteralPath $renderLog -Encoding utf8
            $renderV5Fixtures += [ordered]@{ mode=$mode; path=$renderFixture; compilerExit=$renderExit; log=$renderLog;
                sha256=$(if (Test-Path -LiteralPath $renderFixture) { (Get-FileHash -LiteralPath $renderFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($renderExit -ne 0) { $failure = "Render version5 fixture $mode did not compile"; break }
        }
        if ($failure) { break }
        $renderV6Fixtures = @()
        foreach ($mode in 1..8) {
            $renderFixture = Join-Path $outputDirectory ("render-v6-provider-$mode.dll")
            $renderArguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/LD','/DMTA_TEST_SHUTDOWN_FAILURE_MODULE',
                '/DMTA_TEST_RENDER_V2=1','/DMTA_TEST_RENDER_V3=1','/DMTA_TEST_RENDER_V4=1','/DMTA_TEST_RENDER_V5=1','/DMTA_TEST_RENDER_V6=1',"/DMTA_TEST_RENDER_MODE=$mode", '/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')),
                (Join-Path $PSScriptRoot 'MtaGameAdapterHostTests.cpp'), ('/Fo' + (Join-Path $outputDirectory "render-v6-$mode.obj")), ('/Fe' + $renderFixture),
                '/link', '/EXPORT:CreateGameAdapter', '/EXPORT:CreateGameRenderObservation', '/EXPORT:CreateGameRenderObservationV2', '/EXPORT:CreateGameRenderObservationV3', '/EXPORT:CreateGameRenderObservationV4', '/EXPORT:CreateGameRenderObservationV5', '/EXPORT:CreateGameRenderObservationV6')
            if ($mode -ne 6) {
                $renderArguments += @('/EXPORT:ReadGameRenderEndpointDiagnosticV2', '/EXPORT:ReadGameRenderSetupDiagnosticV1', '/EXPORT:ReadGameRenderRootsDiagnosticV1', '/EXPORT:RequestGameRenderDeviceLeaseV1', '/EXPORT:TakeGameRenderDeviceLeaseV1', '/EXPORT:RequestGameRenderDeviceWindowLeaseV2')
                if ($mode -eq 7) { $renderArguments = @('/DMTA_TEST_DEVICE_WINDOW_MISSING_TAKE=1') + $renderArguments } else { $renderArguments += '/EXPORT:TakeGameRenderDeviceWindowLeaseV2' }
            }
            $renderOutput = @(& $compiler @renderArguments 2>&1)
            $renderExit = $LASTEXITCODE
            $renderLog = Join-Path $outputDirectory "render-v6-$mode-compile.log"
            $renderOutput | Set-Content -LiteralPath $renderLog -Encoding utf8
            $renderV6Fixtures += [ordered]@{ mode=$mode; path=$renderFixture; compilerExit=$renderExit; log=$renderLog;
                sha256=$(if (Test-Path -LiteralPath $renderFixture) { (Get-FileHash -LiteralPath $renderFixture -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
            if ($renderExit -ne 0) { $failure = "Render version6 fixture $mode did not compile"; break }
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
        if ($architecture -eq 'x64') { $testArguments += $fixture; $testArguments += @($facadeFixtures | ForEach-Object path); $testArguments += @($renderFixtures | ForEach-Object path); $testArguments += @($renderV2Fixtures | ForEach-Object path); $testArguments += @($renderV3Fixtures | ForEach-Object path); $testArguments += @($renderV4Fixtures | ForEach-Object path); $testArguments += @($renderV5Fixtures | ForEach-Object path); $testArguments += @($renderV6Fixtures | ForEach-Object path); $testArguments += @($clockFixtures | ForEach-Object path); $testArguments += @($localInputFixtures | ForEach-Object path) }
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
        clockProviderFixtures=$clockFixtures;
        localInputProviderFixtures=$localInputFixtures;
        renderV2ProviderFixtures=$(if ($architecture -eq 'x64') { $renderV2Fixtures } else { @() });
        renderV3ProviderFixtures=$(if ($architecture -eq 'x64') { $renderV3Fixtures } else { @() });
        renderV4ProviderFixtures=$(if ($architecture -eq 'x64') { $renderV4Fixtures } else { @() });
        renderV5ProviderFixtures=$(if ($architecture -eq 'x64') { $renderV5Fixtures } else { @() });
        renderV6ProviderFixtures=$(if ($architecture -eq 'x64') { $renderV6Fixtures } else { @() });
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
