#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise only the same-ped intelligence/manager SDK borrow; no native task adoption or historical replay.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-ped-intelligence-sdk/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/ped-intelligence-sdk-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_ped_intelligence_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;results=@();failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='One abstract intelligence SDK getter over a genuine caller-supplied complete typed task manager, exact native-owner/full-token provenance and existing opaque identity bookends. No task/native factory, pointer lease or engine code.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;completeCGameReady=$false;
    pedIntelligenceSdkTested=$false;pedIntelligenceManagerProvenanceTested=$false;pedIntelligenceIdentityBookendsTested=$false;pedIntelligencePureSurfaceTested=$false
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
    $productionSources = @('Client/game_de/CPedIntelligenceDE.cpp','Client/game_de/CTaskManagerDE.cpp')
    $sourceNames = @(
        'Client/game_de/CPedIntelligenceDE.cpp',
        'Client/game_de/CPedIntelligenceDE.h',
        'Client/game_de/CTaskManagerDE.cpp',
        'Client/game_de/CTaskManagerDE.h',
        'Client/game_de/CWeatherScalarsDE.h',
        'Client/game_de/CWeatherTransitionDE.h',
        'Client/game_de/NativeAnimationBlockAccess.h',
        'Client/game_de/NativeAnimationManagerAccess.h',
        'Client/game_de/NativeApplicationFrameProfile.h',
        'Client/game_de/NativeCameraRowBorrow.h',
        'Client/game_de/NativeCameraSphereVisibleAccess.h',
        'Client/game_de/NativeCheatProfile.h',
        'Client/game_de/NativeClassChainAccess.h',
        'Client/game_de/NativeClockProfile.h',
        'Client/game_de/NativeClothesDescriptorCalls.h',
        'Client/game_de/NativeClothesDescriptorProfile.h',
        'Client/game_de/NativeGameConstructorServices.h',
        'Client/game_de/NativeGameTimingAccess.h',
        'Client/game_de/NativeModelBackingReferenceAccess.h',
        'Client/game_de/NativeModelBankReader.h',
        'Client/game_de/NativeModelClassificationAccess.h',
        'Client/game_de/NativeModelClassificationCalls.h',
        'Client/game_de/NativeModelClassificationProfile.h',
        'Client/game_de/NativeModelLoadedReader.h',
        'Client/game_de/NativeModelReferenceAcquisitionReceipt.h',
        'Client/game_de/NativeModelReferenceMemory.h',
        'Client/game_de/NativeModelReferenceOrigin.h',
        'Client/game_de/NativeModelReferenceRelease.h',
        'Client/game_de/NativeModelUnusedRemoval.h',
        'Client/game_de/NativeNamedGarageAccess.h',
        'Client/game_de/NativeNumericGarageAccess.h',
        'Client/game_de/NativePedAreaAccess.h',
        'Client/game_de/NativePedBikeFlagsAccess.h',
        'Client/game_de/NativePedBleedingAccess.h',
        'Client/game_de/NativePedClothesAccess.h',
        'Client/game_de/NativePedCollisionAccess.h',
        'Client/game_de/NativePedCurrentWeaponSlotAccess.h',
        'Client/game_de/NativePedDuckingAccess.h',
        'Client/game_de/NativePedFightingStyleAccess.h',
        'Client/game_de/NativePedFireAccess.h',
        'Client/game_de/NativePedGogglesAccess.h',
        'Client/game_de/NativePedIntelligenceReader.h',
        'Client/game_de/NativePedMetadataAccess.h',
        'Client/game_de/NativePedMoveAnimAccess.h',
        'Client/game_de/NativePedMutationAccess.h',
        'Client/game_de/NativePedObservationAccess.h',
        'Client/game_de/NativePedOnScreenAccess.h',
        'Client/game_de/NativePedOrientationAccess.h',
        'Client/game_de/NativePedOxygenAccess.h',
        'Client/game_de/NativePedPhysicalScalarsAccess.h',
        'Client/game_de/NativePedPlacementAccess.h',
        'Client/game_de/NativePedPoolProfile.h',
        'Client/game_de/NativePedPoolReader.h',
        'Client/game_de/NativePedRebuildAccess.h',
        'Client/game_de/NativePedReferenceToken.h',
        'Client/game_de/NativePedRotationAccess.h',
        'Client/game_de/NativePedShotFlagsAccess.h',
        'Client/game_de/NativePedStayAccess.h',
        'Client/game_de/NativePedTeleportAccess.h',
        'Client/game_de/NativePedTransformReader.h',
        'Client/game_de/NativePedVelocityAccess.h',
        'Client/game_de/NativePedVisibilityAccess.h',
        'Client/game_de/NativePedVitalsAccess.h',
        'Client/game_de/NativePedWaterFlagsAccess.h',
        'Client/game_de/NativePedWaterTaskAccess.h',
        'Client/game_de/NativePedWeaponAmmoAccess.h',
        'Client/game_de/NativePedWeaponClearAccess.h',
        'Client/game_de/NativePedWeaponFieldsAccess.h',
        'Client/game_de/NativePedWeaponGrantAccess.h',
        'Client/game_de/NativePlayerInfoAccess.h',
        'Client/game_de/NativePreparedGameBorrow.h',
        'Client/game_de/NativeRopesAccess.h',
        'Client/game_de/NativeStartupCompletionAccess.h',
        'Client/game_de/NativeStartupCompletionProfile.h',
        'Client/game_de/NativeStartupMenuAccess.h',
        'Client/game_de/NativeStartupPreparationAccess.h',
        'Client/game_de/NativeStartupStreamingAccess.h',
        'Client/game_de/NativeStatsWriteAccess.h',
        'Client/game_de/NativeStreamingModelAccess.h',
        'Client/game_de/NativeStreamingModelGroupsProfile.h',
        'Client/game_de/NativeStreamingModelGroupsReader.h',
        'Client/game_de/NativeStreamingRemoveCalls.h',
        'Client/game_de/NativeTaskPoolReader.h',
        'Client/game_de/NativeTaskRemovalAccess.h',
        'Client/game_de/NativeUiClassMapProfile.h',
        'Client/game_de/NativeWantedAccess.h',
        'Client/game_de/NativeWeaponConstructorProgress.h',
        'Client/game_de/NativeWeaponInfoVectorAccess.h',
        'Client/game_de/NativeWeaponInfoWriteAccess.h',
        'Client/game_de/NativeWeatherOverrideAccess.h',
        'Client/game_de/NativeWeatherScalarProfile.h',
        'Client/game_de/NativeWeatherTransitionProfile.h',
        'Client/game_de/NativeWorldAreaAccess.h',
        'Client/game_de/NativeWorldGroundAccess.h',
        'Client/game_de/NativeWorldIgnoreEntityAccess.h',
        'Client/game_de/NativeWorldJetpackHeightAccess.h',
        'Client/game_de/NativeWorldLineAccess.h',
        'Client/sdk/game/CGameAdapter.h',
        'Client/sdk/game/CGameClockService.h',
        'Client/sdk/game/CPedIntelligence.h',
        'Client/sdk/game/CPoolSlotPolicy.h',
        'Client/sdk/game/CTaskManager.h',
        'Client/sdk/game/CWeaponInfo.h',
        'Client/sdk/game/CWeaponInfoIndexPolicy.h',
        'Client/sdk/game/CWeather.h',
        'Client/sdk/game/CWorldCheatPolicy.h',
        'Client/sdk/game/Common.h',
        'Shared/sdk/CControlStateSynthesis.h',
        'Shared/sdk/CControllerActions.h',
        'Shared/sdk/CControllerState.h',
        'Shared/sdk/CVector.h',
        'Shared/sdk/CVector4D.h',
        'Shared/sdk/SharedUtil.Defines.h',
        'Shared/sdk/enums/SystemState.h',
        'Tests/definitive/PedIntelligenceSdkTests.cpp',
        'Tests/definitive/Write-PedIntelligenceFixture.ps1',
        'Tests/definitive/test_ped_intelligence_sdk.ps1'
    )
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    . (Join-Path $PSScriptRoot 'Write-PedIntelligenceFixture.ps1')
    $generated=Join-Path $scratchRoot 'PedIntelligenceFixture.generated.h'
    $record.generatedFixture=Write-PedIntelligenceFixture $taskRoot $generated
    $durableGenerated=Join-Path $outputRoot 'PedIntelligenceFixture.generated.h'
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
    foreach($architecture in @('x86','x64')) {
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
        $executable=Join-Path $directory 'ped_intelligence_sdk_tests.exe'
        $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/w14471','/O2','/MD','/DWIN32','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            # Match the existing Windows project dependency order: select
            # the real sparsehash Windows config before its common headers.
            ('/external:I'+(Join-Path $taskRoot 'vendor/sparsehash/src/windows')),('/external:I'+(Join-Path $taskRoot 'vendor/sparsehash/src')),'/external:W0',
            (Join-Path $PSScriptRoot 'PedIntelligenceSdkTests.cpp'))+@($productionSources|ForEach-Object{Join-Path $taskRoot $_})+
            @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
        $result=[ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;checks=0;pointerBits=$null;executable=$executable;executableSha256=$null}
        $record.results+=@($result)
        $result.compile=RunOwned $compiler $arguments $directory 'compile' 60000
        if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw "$architecture ped intelligence SDK compilation failed"}
        $result.executableSha256=Sha $executable
        $result.test=RunOwned $executable @() $directory 'test' 10000
        $stdout=[IO.File]::ReadAllText($result.test.stdout)
        $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) ped intelligence SDK checks \((\d+)-bit; opaque provider doubles only\)$')
        if($summary.Success){$result.checks=[int]$summary.Groups[1].Value;$result.pointerBits=[int]$summary.Groups[2].Value}
        $result['pedIntelligenceSdkTested']=$stdout.Contains('Ped intelligence actual SDK manager borrow fixture completed')
        $result['pedIntelligenceManagerProvenanceTested']=$stdout.Contains('Ped intelligence exact owner and token provenance fixture completed')
        $result['pedIntelligenceIdentityBookendsTested']=$stdout.Contains('Ped intelligence current identity bookends and refusals fixture completed')
        $result['pedIntelligencePureSurfaceTested']=$stdout.Contains('Four intelligence methods and task-manager factory obligations remain abstract')
        $expectedBits=if($architecture -eq 'x86'){32}else{64}
        if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -ne 22 -or $result.pointerBits -ne $expectedBits -or
            -not($result.pedIntelligenceSdkTested -and $result.pedIntelligenceManagerProvenanceTested -and $result.pedIntelligenceIdentityBookendsTested -and $result.pedIntelligencePureSurfaceTested)) {
            throw "$architecture ped intelligence SDK checks or required owner evidence failed"
        }
    }
    foreach($marker in @('pedIntelligenceSdkTested','pedIntelligenceManagerProvenanceTested','pedIntelligenceIdentityBookendsTested','pedIntelligencePureSurfaceTested')) {
        $record[$marker]=@($record.results|Where-Object{-not $_[$marker]}).Count -eq 0
    }
    $record.success=$record.results.Count -eq 2 -and $record.pedIntelligenceSdkTested -and $record.pedIntelligenceManagerProvenanceTested -and $record.pedIntelligenceIdentityBookendsTested -and $record.pedIntelligencePureSurfaceTested


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
if (-not $record.success) { throw "Ped intelligence SDK checks failed; see $outputRoot" }
