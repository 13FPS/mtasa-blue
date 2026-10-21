#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-clock-service/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/clock-service-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$record = [ordered]@{schema='mta_clock_service_tests_v1';directory=$outputRoot;architecture='x64';success=$false;
    compileRuns=@();testRun=$null;checks=0;failure=$null;sourceHashes=@();sourcesUnchanged=$true;
    existingSdkClockImplementationTested=$false;productionNativeCallbackTested=$false;wrongExecutableRefusalVerified=$false;
    productionFreezePatchTested=$false;readOnlyGameTimeReaderTested=$false;nativeCGameTimingPrerequisiteTested=$false;nativeCGameTimingWriterTested=$false;nativeStoredFPSReaderTested=$false;nativePauseSafeOwnerTested=$false;nativeApplicationFrameObserverTested=$false;nativeSystemStateOwnerTested=$false;nativeCheatFlagOwnerTested=$false;nativeWorldCheatRegistryTested=$false;nativeWeatherOwnerTested=$false;nativeWeatherScalarOwnerTested=$false;nativeTypedWeaponWriteOwnerTested=$false;nativeWeaponVectorAndBankOwnerTested=$false;nativeWeatherOverrideOwnerTested=$false;nativeWeatherPlacementOwnerTested=$false;nativeStatsSdkOwnerTested=$false;nativeWeaponStatSdkOwnerTested=$false;nativeStartGameOwnerTested=$false;nativeStartupCompletionOwnerTested=$false;nativeRainOwnerTested=$false;nativePedPoolOwnerTested=$false;nativePlayerInfoWantedOwnerTested=$false;executionArtifacts=@();
    gameLaunched=$false;gameAttached=$false;gameClockWritten=$false;gameHooksInstalled=$false;fullMtaReady=$false}
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function RunBounded([string]$Executable,[string[]]$Arguments,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $outputRoot ($Name + '.log')
    $stderr = Join-Path $outputRoot ($Name + '-errors.log')
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $outputRoot
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $timedOut = $false
    try {
        if (-not $process.Start()) { throw "Failed to launch owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only this newly launched compiler/test tree belongs to the run.
            $process.Kill($true)
            $process.WaitForExit()
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;
            stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}
try {
    if ([IntPtr]::Size -ne 8) { throw 'Clock service verification requires native x64 PowerShell.' }
    $nativeFiles = @(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/game_de') -File |
        Where-Object { $_.Name -like 'NativeClock*' -and $_.Extension -in @('.h','.cpp') })
    if (-not ($nativeFiles.Name -contains 'NativeClockService.cpp')) { throw 'Production native clock runtime is not ready.' }
    $sourceNames = @('Client/game_de/CClockDE.h','Client/game_de/CClockDE.cpp','Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp',
        'Client/game_de/NativeGameTimeReader.h','Client/game_de/NativeGameTimeReader.cpp','Client/sdk/game/CGameTimeService.h',
        'Client/game_de/NativeGravityReadAccess.h','Client/game_de/NativeGravityReadProfile.h','Client/game_de/NativeGravityReadAccess.cpp','Client/game_de/NativeGameTimingReader.h','Client/game_de/NativeGameTimingReader.cpp','Client/game_de/NativeGameTimingProfile.h','Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolReader.cpp','Client/game_de/NativePedPoolProfile.h',
        'Client/game_de/NativeGameFPSProfile.h',
        'Client/game_de/NativeGameStateMemory.h','Client/game_de/NativeGameStateMemory.cpp','Client/game_de/NativeGameStateProfile.h',
        'Client/game_de/NativeCheatProfile.h','Client/game_de/NativeCheatProfile.cpp','Client/game_de/NativeCheatFlagAccess.h','Client/sdk/game/CWorldCheatPolicy.h',
        'Tests/definitive/NativeWorldCheatOwnerTests.inl',
        'Client/game_de/CWeatherTransitionDE.h','Client/game_de/CWeatherTransitionDE.cpp','Client/sdk/game/CWeather.h',
        'Client/game_de/NativeWeatherTransitionProfile.h','Client/game_de/NativeWeatherTransitionProfile.cpp',
        'Client/game_de/NativeWeatherTransitionMemory.h','Client/game_de/NativeWeatherTransitionJournal.h',
        'Client/game_de/CWeatherScalarsDE.h','Client/game_de/CWeatherScalarsDE.cpp',
        'Client/game_de/NativeWeatherScalarProfile.h','Client/game_de/NativeWeatherScalarProfile.cpp','Client/game_de/NativeWeatherScalarMemory.h',
        'Client/game_de/NativeWeatherOverridePlan.h','Client/game_de/NativeWeatherOverridePlan.cpp','Client/game_de/NativeWeatherOverrideProfile.cpp',
        'Client/game_de/NativeWeatherOverrideAccess.h','Client/game_de/CWeatherOverridesDE.h','Client/game_de/CWeatherOverridesDE.cpp',
        'Client/game_de/NativeWeatherOverrideAtomic.h','Client/game_de/NativeWeatherOverrideAtomic.cpp',
        'Client/game_de/NativeWeatherOverrideOwner.h','Client/game_de/NativeWeatherOverrideOwner.cpp',
        'Client/game_de/NativeWeatherOverrideWin32.h','Client/game_de/NativeWeatherOverrideWin32.cpp',
        'Client/game_de/NativeWeatherOverridePlacement.h','Client/game_de/NativeWeatherOverridePlacement.cpp',
        'Client/game_de/NativeWeatherOverrideSlot.h','Client/game_de/NativeWeatherOverrideSlot.cpp','Client/game_de/NativeWeatherRainProfile.h','Client/game_de/NativeWeatherRainBridge.h','Client/game_de/NativeWeatherRainBridge.cpp','Client/game_de/NativeWeatherRainBorrowGuard.h','Client/game_de/NativeWeatherRainBorrowGuard.cpp','Client/game_de/NativeWeatherRainLeaf.h','Client/game_de/NativeWeatherRainLeaf.cpp',
        'Client/game_de/NativeWeaponInfoReader.h','Client/game_de/NativeWeaponInfoReader.cpp','Client/game_de/NativeWeaponInfoProfile.h',
        'Client/game_de/NativeWeaponInfoWriteAccess.h','Client/game_de/NativeWeaponInfoWriteMemory.h','Client/game_de/NativeWeaponInfoWriteMemory.cpp',
        'Client/game_de/CWeaponStatNativeDE.h','Client/game_de/CWeaponStatNativeDE.cpp','Client/game_de/NativeWeaponInfoExtraProfile.h',
        'Client/sdk/game/CWeaponStat.h','Client/sdk/game/CWeaponStatFlagPolicy.h','Tests/definitive/NativeWeaponStatSdkTests.inl',
        'Client/game_de/NativeWeaponInfoWriteProfile.h','Client/game_de/CWeaponInfoWriteDE.h','Client/game_de/CWeaponInfoWriteDE.cpp',
        'Client/game_de/CWeaponInfoReadDE.h','Client/game_de/CWeaponInfoReadDE.cpp',
        'Client/game_de/NativeWeaponInfoVectorAccess.h','Client/game_de/NativeWeaponInfoVectorMemory.h','Client/game_de/NativeWeaponInfoVectorMemory.cpp',
        'Client/game_de/NativeWeaponInfoVectorProfile.h','Client/game_de/CWeaponInfoDE.h','Client/game_de/CWeaponInfoDE.cpp',
        'Client/game_de/CWeaponInfoBankDE.h','Client/game_de/CWeaponInfoBankDE.cpp','Shared/sdk/CVector.h','Shared/sdk/CVector4D.h',
        'Client/game_de/NativePlayerInfoAccess.h','Client/game_de/NativePlayerInfoMemory.h','Client/game_de/NativePlayerInfoMemory.cpp',
        'Client/game_de/NativePlayerInfoProfile.h','Client/game_de/NativePlayerInfoExtraProfile.h','Client/game_de/CPlayerInfoDE.h','Client/game_de/CPlayerInfoDE.cpp','Client/sdk/game/CPlayerInfo.h',
        'Client/game_de/NativeWantedAccess.h','Client/game_de/NativeWantedMemory.h','Client/game_de/NativeWantedMemory.cpp',
        'Client/game_de/NativeWantedProfile.h','Client/game_de/CWantedDE.h','Client/game_de/CWantedDE.cpp','Client/sdk/game/CWanted.h',
        'Tests/definitive/NativePlayerInfoWantedOwnerTests.inl',
        'Client/game_de/NativeStatsReader.h','Client/game_de/NativeStatsReader.cpp','Client/game_de/NativeStatsProfile.h',
        'Client/game_de/NativeStatsWriteAccess.h','Client/game_de/NativeStatsWriteCalls.h','Client/game_de/NativeStatsWriteCalls.cpp',
        'Client/game_de/NativeStatsWriteProfile.h','Client/game_de/CStatsReadDE.h','Client/game_de/CStatsReadDE.cpp',
        'Client/game_de/CStatsDE.h','Client/game_de/CStatsDE.cpp','Client/sdk/game/CStats.h',
        'Client/game_de/NativeStartGameCalls.h','Client/game_de/NativeStartGameCalls.cpp','Client/game_de/NativeStartGameProfile.h',
        'Client/game_de/NativeBorrowedGcProfile.h','Client/game_de/NativeUObjectMembershipAccess.h','Client/game_de/NativeUObjectMembershipProfile.h',
        'Client/game_de/NativeStartupCompletionAccess.h','Client/game_de/NativeStartupCompletionProfile.h',
        'Tests/definitive/NativeStartGameOwnerTests.inl',
        'Client/game_de/NativeGameTimingAccess.h',
        'Client/game_de/NativeGamePauseProfile.h',
        'Client/game_de/NativeApplicationFrame.h','Client/game_de/NativeApplicationFrameProfile.h',
        'Client/game_de/NativeUiClassMapAccess.h','Client/game_de/NativeUiClassMapProfile.h','Client/game_de/NativeClassChainAccess.h',
        'Client/game_de/NativeStartupOwnerAccess.h','Client/game_de/NativeStartupOwnerProfile.h',
        'Client/game_de/NativeStartupWorldAccess.h','Client/game_de/NativeStartupWorldProfile.h','Client/sdk/game/CGameStartupObservation.h',
        'Client/game_de/NativeStartupProvidersAccess.h','Client/game_de/NativeStartupProvidersProfile.h',
        'Client/game_de/CGameTimingDE.h','Client/game_de/CGameTimingDE.cpp','Client/sdk/game/CGame.h','Client/sdk/game/CWeaponInfo.h',
        'Client/sdk/game/Common.h','Shared/sdk/SString.h','Shared/sdk/SharedUtil.IntTypes.h','Shared/sdk/enums/SystemState.h',
        'Shared/sdk/enums/EntityType.h','Shared/sdk/enums/PedPieceTypes.h',
        'Client/game_de/NativeServiceAdmission.h',
        'Client/sdk/game/CClock.h','Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h',
        'Shared/sdk/CControlStateSynthesis.h','Tests/definitive/ClockServiceTests.cpp','Tests/definitive/NativeWeatherRainOwnerTests.inl','Tests/definitive/NativePedPoolOwnerTests.inl','Tests/definitive/test_clock_service.ps1')
    $sourceNames += @($nativeFiles | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName).Replace('\','/') })
    $detoursNames = @('detours.cpp','modules.cpp','disasm.cpp','image.cpp','creatwth.cpp')
    $sourceNames += @($detoursNames + @('detours.h') | ForEach-Object { 'vendor/detours/4.0.1/src/' + $_ })
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $record.sourceSnapshotScope = 'Declared clock runtime, SDK and Detours inputs; not an independently recovered compiler-read trace.'
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio x64 C++ tools are required.' }
    $dev = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed.' }
    foreach ($entry in $environment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $record.compiler = [ordered]@{path=$compiler;sha256=(Sha $compiler)}
    $common = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
        ('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'vendor/detours/4.0.1/src')))
    $detoursSources = @($detoursNames | ForEach-Object { Join-Path $taskRoot ('vendor/detours/4.0.1/src/' + $_) })
    $detoursArguments = $common + @('/w','/c') + $detoursSources + @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')))
    $record.compileRuns += RunBounded $compiler $detoursArguments 'detours-compile' 60000
    if ($record.compileRuns[-1].exitCode -ne 0 -or $record.compileRuns[-1].timedOut) { throw 'Owned Detours compilation failed.' }
    $objects = @($detoursNames | ForEach-Object { Join-Path $scratchRoot ([IO.Path]::ChangeExtension($_,'.obj')) })
    $executable = Join-Path $outputRoot 'clock_service_tests.exe'
    $runtimeDependencies = @($nativeFiles | Where-Object { $_.Extension -eq '.cpp' -and $_.Name -ne 'NativeClockService.cpp' } | ForEach-Object FullName)
    # The fixture includes the exact production runtime translation unit, like
    # the native movement tests. It also includes the actual shared state-memory
    # CPP once so its guarded OS-read boundary can schedule a deterministic
    # owned-image transition; it includes the actual cheat profile once with
    # only its executable-identity selection redirected to the same owned image.
    # Both complete production weather profiles use that same identity selection;
    # scalar field reads still traverse the real memory and callback owner guards.
    # The genuine weapon reader and typed writer are likewise included once;
    # only owned-image identity selection and actual OS/intrinsic fault timing
    # differ. Their per-CAS scope callback is the unchanged real timing owner.
    # No live-profile or native ownership check is substituted.
    # The actual stats reader and Apply CPP are included once in the fixture;
    # only the final typed native invocation is observed instead of executing
    # GTA code. All player, code, state and real callback bookends still run.
    # The StartGame primitive likewise compiles once in the actual fixture;
    # its final call alone is observed, while the real frame/GT/GC scope and
    # complete synchronous UObject/type/provider membership guards execute.
    # Its shipping executable gate stays unchanged.
    $arguments = $common + @('/W4','/WX',(Join-Path $PSScriptRoot 'ClockServiceTests.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CClockDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CGameTimingDE.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeGravityReadAccess.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeatherTransitionDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CWeatherScalarsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeatherOverridesDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponInfoReadDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CWeaponInfoWriteDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponInfoDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CWeaponInfoBankDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponStatNativeDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWantedDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CPlayerInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CStatsReadDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CStatsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlan.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideAtomic.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideOwner.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideWin32.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlacement.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideSlot.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBridge.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBorrowGuard.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainLeaf.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeImageAccess.cpp')) +
        $runtimeDependencies + @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib','user32.lib') + $objects
    [ordered]@{compiler=$record.compiler;detoursArguments=$detoursArguments;arguments=$arguments} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8NoBOM
    $record.compileRuns += RunBounded $compiler $arguments 'compile' 60000
    if ($record.compileRuns[-1].exitCode -ne 0 -or $record.compileRuns[-1].timedOut) { throw 'Clock SDK/runtime fixture compilation failed.' }
    $record.testRun = RunBounded $executable @($outputRoot) 'test' 30000
    $text = [IO.File]::ReadAllText($record.testRun.stdout) + [IO.File]::ReadAllText($record.testRun.stderr)
    if ($text -match 'Passed (\d+) production clock service checks') { $record.checks = [int]$Matches[1] }
    if ($record.testRun.exitCode -ne 0 -or $record.testRun.timedOut -or $record.checks -eq 0) { throw 'Clock service fixture failed.' }
    if (-not $text.Contains('Production native clock runtime fixture completed')) { throw 'Actual production native runtime fixture did not complete.' }
    $record.existingSdkClockImplementationTested = $true
    $record.productionNativeCallbackTested = $true
    $record.wrongExecutableRefusalVerified = $true
    $record.productionFreezePatchTested = $text.Contains('Native freeze patch capability: verified with production review gate')
    $record.readOnlyGameTimeReaderTested = $text.Contains('Production read-only game-time reader fixture completed')
    if (-not $record.readOnlyGameTimeReaderTested) { throw 'Production read-only native time reader fixture did not complete.' }
    $record.nativeCGameTimingPrerequisiteTested = $text.Contains('Production CGame native timing prerequisite fixture completed')
    if (-not $record.nativeCGameTimingPrerequisiteTested) { throw 'Actual native CGame timing prerequisite fixture did not complete.' }
    $record.nativeStoredFPSReaderTested = $text.Contains('Production native stored FPS reader fixture completed')
    if (-not $record.nativeStoredFPSReaderTested) { throw 'Actual native stored FPS reader fixture did not complete.' }
    $record.nativeCGameTimingWriterTested = $text.Contains('Production native CGame timing writer fixture completed')
    if (-not $record.nativeCGameTimingWriterTested) { throw 'Actual native CGame timing writer fixture did not complete.' }
    $record.nativePauseSafeOwnerTested = $text.Contains('Production pause-safe native timing owner fixture completed')
    if (-not $record.nativePauseSafeOwnerTested) { throw 'Actual pause-safe native timing owner fixture did not complete.' }
    $record.nativeSystemStateOwnerTested = $text.Contains('Production native CGame system-state owner fixture completed')
    if (-not $record.nativeSystemStateOwnerTested) { throw 'Actual native CGame system-state owner fixture did not complete.' }
    $record.nativeCheatFlagOwnerTested = $text.Contains('Production native four-cheat-flag owner fixture completed')
    if (-not $record.nativeCheatFlagOwnerTested) { throw 'Actual native four-cheat-flag owner fixture did not complete.' }
    $record.nativeWorldCheatRegistryTested = $text.Contains('Production native fourteen-row world cheat registry read owner fixture completed')
    if (-not $record.nativeWorldCheatRegistryTested) { throw 'Actual native14-row world cheat registry read owner fixture did not complete.' }
    $record.nativeWeatherOwnerTested = $text.Contains('Production native weather transaction owner fixture completed')
    if (-not $record.nativeWeatherOwnerTested) { throw 'Actual native weather transaction owner fixture did not complete.' }
    $record.nativeWeatherScalarOwnerTested = $text.Contains('Production native weather scalar owner fixture completed')
    if (-not $record.nativeWeatherScalarOwnerTested) { throw 'Actual native weather scalar owner fixture did not complete.' }
    $record.nativeTypedWeaponWriteOwnerTested = $text.Contains('Production native typed weapon write owner fixture completed')
    if (-not $record.nativeTypedWeaponWriteOwnerTested) { throw 'Actual native typed weapon write owner fixture did not complete.' }
    $record.nativeWeaponStatSdkOwnerTested = $text.Contains('Production native weapon-stat SDK slice fixture completed')
    if (-not $record.nativeWeaponStatSdkOwnerTested) { throw 'Actual native weapon-stat SDK slice fixture did not complete.' }
    $record.nativeWeaponVectorAndBankOwnerTested = $text.Contains('Production native weapon vector and actual80-wrapper bank owner fixture completed')
    if (-not $record.nativeWeaponVectorAndBankOwnerTested) { throw 'Actual native weapon vector and genuine80-wrapper bank owner fixture did not complete.' }
    $record.nativeWeatherOverrideOwnerTested = $text.Contains('Production native weather atomic override SDK owner fixture completed')
    if (-not $record.nativeWeatherOverrideOwnerTested) { throw 'Actual native weather atomic override SDK owner fixture did not complete.' }
    $record.nativeWeatherPlacementOwnerTested = $text.Contains('Production native seven-group weather placement owner fixture completed')
    if (-not $record.nativeWeatherPlacementOwnerTested) { throw 'Actual native seven-group weather placement owner fixture did not complete.' }
    $record.nativeStatsSdkOwnerTested = $text.Contains('Production native complete CStats SDK owner fixture completed')
    if (-not $record.nativeStatsSdkOwnerTested) { throw 'Actual native complete CStats SDK owner fixture did not complete.' }
    $record.nativeStartGameOwnerTested = $text.Contains('Production native synchronous StartGame frame owner fixture completed')
    if (-not $record.nativeStartGameOwnerTested) { throw 'Actual native synchronous StartGame frame owner fixture did not complete.' }
    $record.nativePlayerInfoWantedOwnerTested = $text.Contains('Production native player-info and complete Wanted SDK owner fixture completed')
    if (-not $record.nativePlayerInfoWantedOwnerTested) { throw 'Actual native player-info and complete Wanted SDK owner fixture did not complete.' }
    $record.nativePedPoolOwnerTested = $text.Contains('Production native ped-pool handle callback owner fixture completed')
    if (-not $record.nativePedPoolOwnerTested) { throw 'Actual native ped-pool handle callback owner fixture did not complete.' }
    $record.nativeRainOwnerTested = $text.Contains('Production native complete Rain weather SDK owner fixture completed')
    if (-not $record.nativeRainOwnerTested) { throw 'Actual complete Rain weather SDK owner fixture did not complete.' }
    $record.nativeStartupCompletionOwnerTested = $text.Contains('Production native asynchronous startup completion owner fixture completed')
    if (-not $record.nativeStartupCompletionOwnerTested) { throw 'Actual native asynchronous startup completion owner fixture did not complete.' }
    $record.nativeApplicationFrameObserverTested = $text.Contains('Production natural application-frame observer fixture completed')
    if (-not $record.nativeApplicationFrameObserverTested) { throw 'Actual application-frame observer fixture did not complete.' }
    $record.nativeFixtureScope = 'Owned SEC_IMAGE with exact native pins and actual Detours, callback, native memory, protection and peer operations. This does not admit the test executable as GTA or execute the game clock update.'
    $record.executable = [ordered]@{path=$executable;bytes=(Get-Item -LiteralPath $executable).Length;sha256=(Sha $executable)}
    foreach ($entry in $record.sourceHashes) { if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false } }
    $record.success = $record.sourcesUnchanged
} catch { $record.failure = $_.Exception.ToString() }
finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    # Remove only disposable files in this task's resolved scratch directory.
    # Binaries, compiler commands, source pins and logs remain durable evidence.
    $resolvedScratch = [IO.Path]::GetFullPath($scratchRoot).TrimEnd('\') + '\'
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp')).TrimEnd('\') + '\'
    if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase)) { throw 'Task scratch escaped Build/tmp.' }
    foreach ($file in @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File)) {
        if (-not [IO.Path]::GetFullPath($file.FullName).StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch file escaped the task directory.' }
        Remove-Item -LiteralPath $file.FullName -Force
    }
    $record.scratchDirectory = $scratchRoot
    $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File).Count
    $record.executionArtifacts = @(Get-ChildItem -LiteralPath $outputRoot -File | Where-Object { $_.Extension -in @('.exe','.bin','.obj') } | ForEach-Object {
        [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Sha $_.FullName)}
    })
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');checks=$record.checks;failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Clock service checks failed; see $outputRoot" }
