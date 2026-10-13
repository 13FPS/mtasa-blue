#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param([string]$SavedNativeMovementDirectory)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/mta-native-movement-input/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$testSource = Join-Path $PSScriptRoot 'NativeMovementInputTests.cpp'
$savedSources = @{}
$stagedSourceHashes = @()
if ($SavedNativeMovementDirectory) {
    $savedRoot = (Resolve-Path -LiteralPath $SavedNativeMovementDirectory).Path
    $savedManifest = Get-Content -LiteralPath (Join-Path $savedRoot 'manifest.json') -Raw | ConvertFrom-Json
    $fixtureRoot = Join-Path $outputRoot 'saved-production-fixture'
    # Substitute only the immutable saved production pair. The same current
    # race assertion must fail behaviorally; no shipping Begin guard is changed.
    foreach ($name in @('NativeMovementInput.cpp','NativeMovementInput.h')) {
        $key = 'Client/game_de/' + $name
        $entries = @($savedManifest.files | Where-Object { ($_.source -replace '\\','/') -eq $key })
        $savedPath = Join-Path $savedRoot $name
        if ($entries.Count -ne 1 -or $entries[0].sha256 -notmatch '^[0-9a-f]{64}$' -or
            (Get-FileHash -LiteralPath $savedPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entries[0].sha256) {
            throw "Saved production source does not match its immutable manifest: $name"
        }
        $savedSources[$key] = $savedPath
    }
    foreach ($key in @('Client/game_de/NativeMovementInput.cpp','Client/game_de/NativeMovementInput.h',
        'Client/game_de/NativeInputRouting.h','Client/game_de/NativePadReader.h','Client/sdk/game/CGameAdapter.h',
        'Tests/definitive/NativeMovementInputTests.cpp')) {
        $source = if ($savedSources.ContainsKey($key)) { $savedSources[$key] } else { Join-Path $taskRoot $key }
        $destination = Join-Path $fixtureRoot $key
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
        if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHash) {
            throw "Staged causal source differs from its selected input: $key"
        }
        $stagedSourceHashes += [ordered]@{path=$key;sourcePath=$source;stagedPath=$destination;sha256=$sourceHash}
    }
    $testSource = Join-Path $fixtureRoot 'Tests/definitive/NativeMovementInputTests.cpp'
}
$sourceNames = @('Client/game_de/NativeMovementInput.h','Client/game_de/NativeMovementInput.cpp',
    'Client/game_de/NativeInputRouting.h','Client/game_de/NativePadReader.h','Client/game_de/NativePadReader.cpp',
    'Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h','Shared/sdk/enums/PedState.h','Tests/definitive/NativeMovementInputTests.cpp',
    'Tests/definitive/test_native_movement_input.ps1',
    'vendor/detours/4.0.1/src/detours.h','vendor/detours/4.0.1/src/detours.cpp','vendor/detours/4.0.1/src/modules.cpp',
    'vendor/detours/4.0.1/src/disasm.cpp','vendor/detours/4.0.1/src/image.cpp','vendor/detours/4.0.1/src/creatwth.cpp')
$sourceHashes = @($sourceNames | ForEach-Object {
    $sourcePath = if ($savedSources.ContainsKey($_)) { $savedSources[$_] } else { Join-Path $taskRoot $_ }
    [ordered]@{ path=$_; sourcePath=$sourcePath; sha256=(Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant() }
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
$cacheArguments = if ($SavedNativeMovementDirectory) { @('/DMTA_NATIVE_ELIGIBILITY_SAVED_OBSERVER') } else { @('/DMTA_NATIVE_ELIGIBILITY_PUBLISHED_CACHE') }
$arguments = $common + $cacheArguments + @('/W4','/WX',$testSource,
    (Join-Path $taskRoot 'Client/game_de/NativePadReader.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),
    ('/Fe'+$executable),'/link','bcrypt.lib') + $objects
$compilerOutput = @(& $compiler @arguments 2>&1)
$compilerExit = $LASTEXITCODE
$compilerOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'compile.log') -Encoding utf8
$testExit = $null
$testCount = $null
$testTimedOut = $false
$failedChecks = $null
$failedAssertion = $null
if ($detoursExit -eq 0 -and $compilerExit -eq 0) {
    # A deadlock in the actual suspension/teardown fixture must fail boundedly
    # without leaving a test process alive or requiring an operator to stop it.
    $testLog = Join-Path $outputRoot 'test.log'
    $testErrors = Join-Path $outputRoot 'test-errors.log'
    $testProcess = Start-Process -FilePath $executable -WindowStyle Hidden -PassThru -RedirectStandardOutput $testLog -RedirectStandardError $testErrors
    if (-not $testProcess.WaitForExit(30000)) {
        $testTimedOut = $true
        $testProcess.Kill()
        $testProcess.WaitForExit()
    }
    $testExit = $testProcess.ExitCode
    $testOutput = @(Get-Content -LiteralPath $testLog) + @(Get-Content -LiteralPath $testErrors)
    $testProcess.Dispose()
    if (($testOutput -join "`n") -match 'Passed (\d+) production native movement callback checks') { $testCount = [int]$Matches[1] }
    if (($testOutput -join "`n") -match 'Failed after (\d+) checks: ([^\r\n]+)') {
        $failedChecks = [int]$Matches[1]
        $failedAssertion = $Matches[2]
    }
}
$unchanged = $true
foreach ($entry in $sourceHashes) {
    if ((Get-FileHash -LiteralPath $entry.sourcePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $unchanged = $false }
}
$stagedUnchanged = $true
foreach ($entry in $stagedSourceHashes) {
    if ((Get-FileHash -LiteralPath $entry.stagedPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256 -or
        (Get-FileHash -LiteralPath $entry.sourcePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { $stagedUnchanged = $false }
}
$record = [ordered]@{ recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;architecture='x64';
    success=($detoursExit -eq 0 -and $compilerExit -eq 0 -and $testExit -eq 0 -and $null -ne $testCount -and -not $testTimedOut -and $unchanged -and $stagedUnchanged);
    detoursCompilerExit=$detoursExit;compilerExit=$compilerExit;testExit=$testExit;testTimedOut=$testTimedOut;checks=$testCount;
    savedNativeMovementDirectory=$SavedNativeMovementDirectory;failedChecks=$failedChecks;failedAssertion=$failedAssertion;
    fixtureBuildDefines=$cacheArguments;
    sourceHashes=$sourceHashes;sourceUnchanged=$unchanged;executable=$executable;
    stagedSourceHashes=$stagedSourceHashes;stagedSourceUnchanged=$stagedUnchanged;
    gameLaunched=$false;gameProcessAttached=$false;gameInputWritten=$false;gameHooksInstalled=$false;wrongExecutableBeginInstalledHooks=$false;
    syntheticOwnProcessPadWritten=$true;productionCallbackTested=$true;syntheticOwnProcessDetoursInstalled=$true;
    executableSha256=$(if (Test-Path -LiteralPath $executable) { (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null });
    nativeRuntimeVerified=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "Native movement callback checks failed; see $outputRoot" }
$record | ConvertTo-Json -Depth 8 -Compress
