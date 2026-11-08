#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise only the prepared-service CPU envelope and unchanged partial-graph construction; no lifecycle or camera replay.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-prepared-game-borrow-sdk/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/prepared-game-borrow-sdk-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_prepared_game_borrow_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;results=@();failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Internal CPU prepared-graph envelope only: genuine test-issued token, exact identities, fresh publication, failure retention and explicit producer drain. Existing production partial graph remains abstract; no engine or full-factory authority.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;completeCGameReady=$false;
    preparedGameBorrowSdkTested=$false;preparedGameBorrowIdentityTested=$false;preparedGameBorrowPublicationTested=$false;preparedGameBorrowDrainTested=$false;preparedGameBorrowPureSurfaceTested=$false;preparedStartupServiceBindingSdkTested=$false
}

function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function AddFailure([string]$Message) {
    $record.success = $false
    if ($null -eq $record.failure) { $record.failure = $Message }
    else { $record.failure += [Environment]::NewLine + $Message }
}
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $Directory ($Name + '.log')
    $stderr = Join-Path $Directory ($Name + '-errors.log')
    $command = Join-Path $Directory ($Name + '-command.json')
    [ordered]@{executable=$Executable;sha256=(Sha $Executable);arguments=$Arguments;workingDirectory=$Directory;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot} | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['TEMP'] = $scratchRoot
    $info.Environment['TMP'] = $scratchRoot
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only the fixture process tree created by this invocation is owned.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;
            timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}
function InterfaceMethods([string]$RelativePath) {
    $text = [IO.File]::ReadAllText((Join-Path $taskRoot $RelativePath))
    $text = [regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    foreach ($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature = [regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name = [regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if (-not $name) { throw "Cannot derive rejecting fixture method from $signature" }
        # Defaults are caller syntax, not part of an overriding signature.
        $signature = [regex]::Replace($signature,'\s*=\s*[^,)]+','')
        [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
    }
}

try {
    $productionSources = @('Client/game_de/CGamePreparedBorrowDE.cpp','Client/game_de/CGameBuildingPoolDefaultDE.cpp','Tests/definitive/TextureRegistrySStringSupport.cpp','Client/game_de/CGameWorldDE.cpp','Client/game_de/CGameResetDE.cpp','Client/game_de/CGameInitializationDE.cpp','Client/game_de/CGaragesDE.cpp','Client/game_de/CGameDE.cpp','Client/game_de/CGameTimingDE.cpp','Client/game_de/CGameWeaponInfoDE.cpp','Client/game_de/CWeaponStatFactoryDE.cpp','Client/game_de/CClockDE.cpp',
        'Client/game_de/CKeyGenDE.cpp','Client/game_de/CAsyncLoadingDE.cpp','Client/game_de/NativeGravityReadAccess.cpp','Client/game_de/NativeGameTimingReader.cpp',
        'Client/game_de/NativeGameTimeReader.cpp','Client/game_de/NativeStreamingFileIdsReader.cpp',
        'Client/game_de/NativeGameStateReader.cpp','Client/game_de/NativeGameStateMemory.cpp','Client/game_de/NativeDataFileNamesReader.cpp','Client/game_de/NativeImageAccess.cpp',
        'Client/game_de/NativeCheatProfile.cpp',
        'Client/game_de/CWeaponInfoReadDE.cpp','Client/game_de/CWeaponInfoWriteDE.cpp','Client/game_de/CWeaponInfoDE.cpp','Client/game_de/CWeaponInfoBankDE.cpp','Client/game_de/CWeaponStatNativeDE.cpp','Client/game_de/CWeaponStatManagerPolicyDE.cpp','Client/game_de/CWeaponStatManagerDE.cpp','Client/game_de/NativeWeaponInfoReader.cpp')
    # Link the actual receivers so stale prepared service identities are tested
    # through production getters rather than a second fixture implementation.
    $productionSources += @('Client/game_de/CGaragesStartupDE.cpp','Client/game_de/CGarageNamedDE.cpp',
        'Client/game_de/CStreamingDE.cpp','Client/game_de/CStreamingStartupDE.cpp')
    $sourceNames = $productionSources + @('Client/game_de/CGameModelPreparationDE.h','Client/sdk/game/CModelPedCreationPolicy.h','Client/game_de/NativeModelClassificationAccess.h','Client/sdk/game/CStreaming.h','Client/game_de/CGameInitializationDE.h','Client/game_de/CGaragesDE.h','Client/sdk/game/CGameInitializationPolicy.h','Client/sdk/game/CGameModelPreparationPolicy.h','Client/sdk/game/CModelInfo.h','Client/sdk/game/CGarageInitializationPolicy.h','Client/sdk/game/CGarages.h','Client/sdk/game/CGarage.h','Client/game_de/CGameDE.h','Client/game_de/CGameTimingDE.h','Client/game_de/CGameWeaponInfoDE.h','Client/sdk/game/CWeaponInfoIndexPolicy.h','Client/game_de/CClockDE.h',
        'Client/game_de/CWeaponStatFactoryDE.h','Client/sdk/game/CWeaponStatData.h','Client/sdk/game/CWeaponStatDefaults.h',
        'Client/sdk/game/CWeaponStatFlagPolicy.h','Client/sdk/game/CWeaponStatOwned.h','Client/sdk/game/CWeaponStat.h',
        'Client/game_de/CKeyGenDE.h','Client/game_de/CAsyncLoadingDE.h','Client/game_de/NativeGravityReadAccess.h','Client/game_de/NativeGravityReadProfile.h','Client/game_de/NativeGravityReadAccess.cpp','Client/game_de/NativeGameTimingReader.h',
        'Client/game_de/NativeGameTimeReader.h','Client/game_de/NativeStreamingFileIdsReader.h','Client/game_de/NativeGameStateReader.h',
        'Client/game_de/NativeGameStateMemory.h','Client/game_de/NativeDataFileNamesReader.h','Client/game_de/NativeDataFileNamesProfile.h','Shared/sdk/SharedUtil.Defines.h',
        'Client/game_de/NativeGameTimingAccess.h','Client/game_de/NativeWeaponConstructorProgress.h','Client/game_de/NativeRopesAccess.h','Client/sdk/game/CRopes.h','Client/game_de/NativePlayerInfoAccess.h','Client/game_de/NativeWantedAccess.h','Client/game_de/NativeWeaponInfoVectorAccess.h','Client/game_de/NativeWeaponInfoWriteAccess.h',
        'Client/game_de/NativePedTransformReader.h','Client/game_de/NativePedPoolReader.h',
        'Client/game_de/NativePedTeleportAccess.h',
        'Client/game_de/NativePedVelocityAccess.h',
        'Client/game_de/NativeStartupCompletionAccess.h','Client/game_de/NativeStartupCompletionProfile.h',
        'Client/game_de/NativeStatsWriteAccess.h','Client/sdk/game/CStats.h','Client/game_de/NativeWeatherOverrideAccess.h',
        'Client/game_de/NativeWeaponInfoWriteProfile.h','Client/game_de/CWeatherScalarsDE.h','Client/game_de/NativeWeatherScalarProfile.h',
        'Client/game_de/NativeImageAccess.h','Client/game_de/NativeClockMemory.h',
        'Client/game_de/CWeatherTransitionDE.h','Client/sdk/game/CWeather.h',
        'Client/game_de/NativeCheatProfile.h','Client/game_de/NativeCheatFlagAccess.h','Client/sdk/game/CWorldCheatPolicy.h',
        'Client/game_de/NativeClockProfile.h','Client/game_de/NativeGameTimingProfile.h','Client/game_de/NativeGameFPSProfile.h',
        'Client/game_de/NativeStreamingFileIdsProfile.h','Client/game_de/NativeGameStateProfile.h','Client/game_de/NativeFrontendMenuProfile.h',
        'Shared/sdk/CAsyncLoadingPolicy.h','Shared/sdk/CJetpackWeaponPolicy.h','Client/sdk/game/CGame.h','Client/sdk/game/CPools.h','Client/sdk/game/Common.h',
        'Client/sdk/game/CGameAdapter.h','Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameTimeService.h',
        'Client/game_sa/CGameSA.h','Client/game_sa/CGameSA.cpp','Tests/definitive/PreparedGameBorrowSdkTests.cpp','Tests/definitive/test_prepared_game_borrow_sdk.ps1')
    $sourceNames += @(
        'Client/sdk/game/CWeaponStatManager.h',
        'Client/game_de/CWeaponInfoReadDE.h','Client/game_de/CWeaponInfoWriteDE.h','Client/game_de/CWeaponInfoDE.h','Client/game_de/CWeaponInfoBankDE.h','Client/game_de/CWeaponStatNativeDE.h','Client/game_de/CWeaponStatManagerPolicyDE.h','Client/game_de/CWeaponStatManagerDE.h','Client/game_de/NativeWeaponInfoReader.h','Client/game_de/NativeWeaponInfoProfile.h','Client/game_de/NativeWeaponInfoVectorProfile.h',
        'Client/sdk/game/CWeaponStatListPolicy.h','Client/sdk/game/CWeaponStartupPolicy.h'
    )
    $sourceNames += @('Client/game_de/CGameWorldDE.h','Client/sdk/game/CGameWorldPolicy.h','Client/sdk/game/CDummyPool.h','Client/game_de/CGameResetDE.h','Client/sdk/game/CGameResetPolicy.h','Client/game_de/NativeGameConstructorServices.h',
        'Client/sdk/game/CFireManager.h','Client/sdk/game/CCamera.h','Client/sdk/game/CHud.h','Client/sdk/game/CBuildingsPool.h',
        'Shared/sdk/SharedUtil.Misc.h','Tests/definitive/Write-GameWorldFixture.ps1')
    $sourceNames += @('Build/definitive-recovery/mta-startup-model-collision-20261028/startup-model-collision-api-source-ready.json','Build/definitive-recovery/mta-startup-model-collision-20261028/startup-model-collision-sdk-admission-ready.json','Build/definitive-recovery/mta-startup-model-collision-tests-20261028/startup-model-collision-sdk-tests-source-ready.json')
    $sourceNames += @('Client/game_de/CGameResetMixinDE.inl','Client/game_de/CGameWorldMixinDE.inl','Client/game_de/CGameStartupLifecycleDE.h','Client/game_de/CGameStartupInitializationDE.h','Client/game_de/CGameStartupStreamingDE.h','Client/game_de/CGameNumericGaragesDE.h','Client/game_de/CGameNativeMenuDE.h','Client/game_de/CGameRenderPreparationDE.h','Client/game_de/CGamePresentationPolicyDE.h','Client/game_de/NativePresentationPolicyAccess.h','Client/game_de/WeaponFireHandlerAccess.h','Client/sdk/game/CGamePresentationPolicy.h','Client/game_de/CRenderWarePreparationDE.h','Client/sdk/game/CShaderTextureWatchTypes.h','Client/sdk/game/CShaderTextureWatch.h','Client/sdk/game/CShaderTextureWatch.inl','Client/sdk/game/CTextureWatchInitializationPolicy.h','Shared/sdk/SString.hpp','Shared/sdk/SharedUtil.FastHashMap.h','Shared/sdk/SharedUtil.FastHashSet.h','Tests/definitive/Write-StartupLifecycleFixture.ps1')
    $sourceNames += @('vendor/sparsehash/src/google/dense_hash_map','vendor/sparsehash/src/google/dense_hash_set','vendor/sparsehash/src/sparsehash/dense_hash_map','vendor/sparsehash/src/sparsehash/dense_hash_set','vendor/sparsehash/src/sparsehash/internal/densehashtable.h','vendor/sparsehash/src/sparsehash/internal/hashtable-common.h','vendor/sparsehash/src/sparsehash/internal/libc_allocator_with_realloc.h','vendor/sparsehash/src/sparsehash/type_traits.h','vendor/sparsehash/src/sparsehash/template_util.h','vendor/sparsehash/src/windows/sparsehash/internal/sparseconfig.h')
    $sourceNames += @('Client/game_de/CGameBuildingPoolDefaultDE.h','Tests/definitive/Write-PreparedGameBorrowFixture.ps1','Client/game_de/CGamePreparedBorrowDE.h','Client/game_de/NativePreparedGameBorrow.h')
    $sourceNames += @('Client/game_de/CGaragesStartupDE.h','Client/game_de/CGarageNamedDE.h',
        'Client/game_de/NativeNumericGarageAccess.h','Client/game_de/NativeNumericGarageProfile.h','Client/game_de/NativeNamedGarageAccess.h',
        'Client/game_de/CStreamingDE.h','Client/game_de/CStreamingStartupDE.h','Client/game_de/NativeStreamingModelAccess.h',
        'Client/game_de/NativeStreamingModelGroupsReader.h','Client/game_de/NativeStreamingModelGroupsProfile.h',
        'Client/game_de/NativeModelLoadedReader.h','Client/game_de/NativeModelBankReader.h','Client/game_de/NativePedPoolProfile.h',
        'Client/sdk/game/CModelRequestPolicy.h','Client/sdk/game/CAnimBlock.h','Shared/sdk/CVector.h')
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    . (Join-Path $PSScriptRoot 'Write-PreparedGameBorrowFixture.ps1')
    $generated=Join-Path $scratchRoot 'PreparedGameBorrowFixture.generated.h'
    $record.generatedFixture=Write-PreparedGameBorrowFixture $taskRoot $generated
    $durableGenerated=Join-Path $outputRoot 'PreparedGameBorrowFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture['durablePath']=$durableGenerated
    $record.generatedFixture['durableSha256']=Sha $durableGenerated

    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    # This fixture now includes the actual DE model/UObject owner headers,
    # whose native pointer layout explicitly requires x64. Classic x86 uses
    # its separate backend; it cannot instantiate this DE ownership graph.
    foreach($architecture in @('x64')) {
        $directory=Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory)|Out-Null
        $environmentScript=Join-Path $directory 'capture-msvc.cmd'
        @('@echo off',('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
            'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|
            Set-Content -LiteralPath $environmentScript -Encoding ascii
        $environmentResult=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'msvc-environment' 30000
        if($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut){throw "$architecture MSVC environment initialization failed"}
        foreach($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
            if($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
            }
        }
        $compiler=Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $executable=Join-Path $directory 'prepared_game_borrow_sdk_tests.exe'
        $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/w14471','/O2','/MD','/DWIN32','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            # Match the existing Windows project dependency order: select
            # the real sparsehash Windows config before its common headers.
            ('/external:I'+(Join-Path $taskRoot 'vendor/sparsehash/src/windows')),('/external:I'+(Join-Path $taskRoot 'vendor/sparsehash/src')),'/external:W0',
            (Join-Path $PSScriptRoot 'PreparedGameBorrowSdkTests.cpp'))+@($productionSources|ForEach-Object{Join-Path $taskRoot $_})+
            @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
        $result=[ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;checks=0;pointerBits=$null;executable=$executable;executableSha256=$null}
        $record.results+=@($result)
        $result.compile=RunOwned $compiler $arguments $directory 'compile' 60000
        if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw "$architecture prepared game borrow SDK compilation failed"}
        $result.executableSha256=Sha $executable
        $result.test=RunOwned $executable @() $directory 'test' 10000
        $stdout=[IO.File]::ReadAllText($result.test.stdout)
        $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) prepared game borrow SDK checks \((\d+)-bit; CPU provider doubles only\)$')
        if($summary.Success){$result.checks=[int]$summary.Groups[1].Value;$result.pointerBits=[int]$summary.Groups[2].Value}
        $result['preparedGameBorrowSdkTested']=$stdout.Contains('Prepared game borrow SDK lifetime fixture completed')
        $result['preparedGameBorrowIdentityTested']=$stdout.Contains('Prepared game borrow current graph identity fixture completed')
        $result['preparedGameBorrowPublicationTested']=$stdout.Contains('Prepared game borrow publication and output preservation fixture completed')
        $result['preparedGameBorrowDrainTested']=$stdout.Contains('Prepared game borrow producer drain fixture completed')
        $result['preparedGameBorrowPureSurfaceTested']=$stdout.Contains('All prepared graph SDK obligations and thirteen lifecycle hooks remain abstract')
        $result['preparedStartupServiceBindingSdkTested']=[regex]::Matches($stdout,'(?m)^Production prepared startup service binding SDK fixture completed\r?$').Count -eq 1
        $expectedBits=if($architecture -eq 'x86'){32}else{64}
        if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -ne 91 -or $result.pointerBits -ne $expectedBits -or
            -not($result.preparedGameBorrowSdkTested -and $result.preparedGameBorrowIdentityTested -and $result.preparedGameBorrowPublicationTested -and $result.preparedGameBorrowDrainTested -and $result.preparedGameBorrowPureSurfaceTested -and $result.preparedStartupServiceBindingSdkTested)) {
            throw "$architecture prepared borrow SDK checks or required lifetime evidence failed"
        }
        # A separate owned process verifies the deliberately fatal live-token
        # destructor without terminating the normal check process or releasing
        # a token from an unsupported destructor/foreign thread.
        $result['destructorTest']=RunOwned $executable @('--live-token-destructor') $directory 'live-token-destructor' 10000
        $destructorOutput=[IO.File]::ReadAllText($result.destructorTest.stdout)
        $result['liveDestructorRetained']=$result.destructorTest.exitCode -eq 86 -and -not $result.destructorTest.timedOut -and
            $destructorOutput.Contains('Live prepared token destructor refused without implicit release')
        if(-not $result.liveDestructorRetained){throw "$architecture live-token destructor did not retain its genuine CPU hold"}
        $result['normalChecks']=$result.checks
        $result.checks++

    }
    foreach($marker in @('preparedGameBorrowSdkTested','preparedGameBorrowIdentityTested','preparedGameBorrowPublicationTested','preparedGameBorrowDrainTested','preparedGameBorrowPureSurfaceTested','preparedStartupServiceBindingSdkTested')) {
        $record[$marker]=@($record.results|Where-Object{-not $_[$marker]}).Count -eq 0
    }
    # Index the dictionaries explicitly: a single-architecture result must
    # aggregate the same way as a multi-result PowerShell pipeline.
    $record['checks']=0
    foreach($completed in $record.results){$record['checks'] += [int]$completed['checks']}
    $record.success=$record.results.Count -eq 1 -and $record.checks -eq 92 -and $record.preparedGameBorrowSdkTested -and $record.preparedGameBorrowIdentityTested -and $record.preparedGameBorrowPublicationTested -and $record.preparedGameBorrowDrainTested -and $record.preparedGameBorrowPureSurfaceTested -and $record.preparedStartupServiceBindingSdkTested


} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
    }
    foreach ($entry in $record.sourceHashes) {
        try { if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false } }
        catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Validate this invocation's flat scratch directory before deleting its
        # generated fixture and compiler scratch. Durable evidence stays above.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Task scratch escaped Build/tmp or became a reparse point.' }
        $entries = @(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
                -not (Resolve-Path -LiteralPath $entry.FullName).Path.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected scratch directory/link/path preserved.' }
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved = $true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Prepared game borrow SDK checks failed; see $outputRoot" }
