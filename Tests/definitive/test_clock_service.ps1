#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('Full','PedOwner','PedCurrentWeapon','GarageNamed','GaragePools','PedPoolRegistry','ConstructorWeapons','PedPose','PedWater','PedWaterOrientation','AnimationBlock','AnimationBlockName','AnimationManager','AnimationServices','AreaSpecialModel','PedArea','SpecialModel','FireCollision','PedFire','PedCollision','DuckingSetter','PedGoggles','PedVisibility','ConstructorServices','WorldGround','WorldLine','WorldArea','WorldQueries','WorldJetpack','PedOnScreen','PedWeaponGrant','WorldIgnoreEntity','CameraSphereVisible','PedWeaponClear','PedClothes','PedRebuild','PedMoveAnim','PlayerNativePrerequisites','AnimationBlockRefs','AnimationBlockFactory','CustomSequenceStorage','CustomAnimationResources','AnimationManagerRefs','AnimationGateway')]
    [string]$Scope = 'Full'
)
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
    productionFreezePatchTested=$false;readOnlyGameTimeReaderTested=$false;nativeCGameTimingPrerequisiteTested=$false;nativeCGameTimingWriterTested=$false;nativeStoredFPSReaderTested=$false;nativePauseSafeOwnerTested=$false;nativeApplicationFrameObserverTested=$false;nativeSystemStateOwnerTested=$false;nativeCheatFlagOwnerTested=$false;nativeWorldCheatRegistryTested=$false;nativeWeatherOwnerTested=$false;nativeWeatherScalarOwnerTested=$false;nativeTypedWeaponWriteOwnerTested=$false;nativeWeaponVectorAndBankOwnerTested=$false;nativeWeatherOverrideOwnerTested=$false;nativeWeatherPlacementOwnerTested=$false;nativeStatsSdkOwnerTested=$false;nativeWeaponStatSdkOwnerTested=$false;nativeWeaponStatManagerOwnerTested=$false;nativeStartGameOwnerTested=$false;nativeStartupCompletionOwnerTested=$false;nativeRainOwnerTested=$false;nativePedPoolOwnerTested=$false;nativePedTeleportOwnerTested=$false;nativePedVelocityOwnerTested=$false;nativePedReferenceOwnerTested=$false;nativePedMutationOwnerTested=$false;nativePedVitalsOwnerTested=$false;nativePedPhysicalScalarsOwnerTested=$false;nativePedCurrentRotationOwnerTested=$false;nativePedOxygenOwnerTested=$false;nativePedDuckingOwnerTested=$false;nativePedBikeFlagsOwnerTested=$false;nativePedShotFlagsOwnerTested=$false;nativePedBleedingOwnerTested=$false;nativePedStayOwnerTested=$false;nativePedFightingStyleOwnerTested=$false;nativeStreamingSceneOwnerTested=$false;nativeStreamingSharedReentryOwnerTested=$false;nativeGenericPedVitalsOwnerTested=$false;nativePedMetadataOwnerTested=$false;nativePedObservationOwnerTested=$false;nativePedIntelligenceOwnerTested=$false;nativeTaskPoolOwnerTested=$false;nativeTaskSecondaryRemovalOwnerTested=$false;nativeModelBankOwnerTested=$false;nativeModelLoadedOwnerTested=$false;nativeStreamingRequestOwnerTested=$false;nativeModelStreamingOwnerTested=$false;nativeModelReferenceOwnerTested=$false;nativeModelFinalReleaseOwnerTested=$false;nativeStreamingUpgradeOwnerTested=$false;nativeModelOwnershipOwnerTested=$false;nativeStreamingLoadAllOwnerTested=$false;nativeStreamingResourcesOwnerTested=$false;nativeModelAcquireOwnerTested=$false;nativeModelClassificationOwnerTested=$false;nativeModelTypeOwnerTested=$false;nativeStreamingRemovalOwnerTested=$false;nativeStreamingPriorityOwnerTested=$false;nativeStreamingModelGroupsOwnerTested=$false;nativeModelArchiveOwnerTested=$false;nativePlayerInfoWantedOwnerTested=$false;nativeRopesOwnerTested=$false;executionArtifacts=@();
    gameLaunched=$false;gameAttached=$false;gameClockWritten=$false;gameHooksInstalled=$false;fullMtaReady=$false}
# Historical markers start false and are set only by the branch that actually
# executed. Focus mode must never inherit the cumulative suite's claims.
$record['runScope'] = $Scope
$record['nativeGameConstructorServicesOwnerTested'] = $false
$record['actualGameConstructorServicesSdkOwnerTested'] = $false
$record['nativePedOwnerSliceTested'] = $false
$record['nativePedWaterTaskOwnerTested'] = $false
$record['actualPedWaterTaskOwnerTested'] = $false
$record['nativeWorldJetpackHeightOwnerTested'] = $false
$record['actualWorldJetpackHeightSdkOwnerTested'] = $false
$record['nativeWorldAreaOwnerTested'] = $false
$record['actualWorldAreaSdkOwnerTested'] = $false
$record['nativeWorldLineOwnerTested'] = $false
$record['actualWorldLineSdkOwnerTested'] = $false
$record['nativeWorldGroundOwnerTested'] = $false
$record['actualWorldGroundSdkOwnerTested'] = $false
$record['nativePedWeaponGrantOwnerTested'] = $false
$record['nativeAnimationHierarchyHashOwnerTested'] = $false
$record['actualAnimationGatewaySdkOwnerTested'] = $false
$record['nativeAnimationManagerRefsOwnerTested'] = $false
$record['actualAnimationManagerRefsSdkOwnerTested'] = $false
$record['nativeCustomAnimationResourcesOwnerTested'] = $false
$record['nativeCustomAnimationResourcesEndTested'] = $false
$record['nativeCustomAnimationResourcesRetainedLifetimeTested'] = $false
$record['actualCustomAnimationStorageSdkOwnerTested'] = $false
$record['nativeCustomSequenceStorageOwnerTested'] = $false
$record['nativeCustomSequenceStorageEndTested'] = $false
$record['nativeCustomSequenceStorageRetainedLifetimeTested'] = $false
$record['customSequenceStorageShippingModulePinAcquisitionTested'] = $false
$record['customSequenceStorageFourSlotLimitTested'] = $false
$record['actualAnimationBlockFactorySdkOwnerTested'] = $false
$record['nativeAnimationBlockFactoryOwnerTested'] = $false
$record['opaqueAnimationBlockRoundtripOwnerTested'] = $false
$record['nativeAnimationBlockRefsOwnerTested'] = $false
$record['actualAnimationBlockRefsSdkOwnerTested'] = $false
$record['sharedAnimBlockRequestRefPolicyOwnerTested'] = $false
$record['nativeModuleClothesInitializerOwnerTested'] = $false
$record['nativePedMoveAnimOwnerTested'] = $false
$record['actualPlayerMoveAnimSdkOwnerTested'] = $false
$record['nativePedRebuildOwnerTested'] = $false
$record['actualPedRebuildSdkOwnerTested'] = $false
$record['nativePedClothesOwnerTested'] = $false
$record['actualPedClothesSdkOwnerTested'] = $false
$record['nativePedWeaponClearOwnerTested'] = $false
$record['actualPedClearWeaponSdkOwnerTested'] = $false
$record['nativeWorldIgnoreEntityOwnerTested'] = $false
$record['nativeCameraSphereVisibleOwnerTested'] = $false
$record['actualCameraSphereVisibleSdkOwnerTested'] = $false
$record['actualWorldIgnoreEntitySdkOwnerTested'] = $false
$record['actualPedGiveWeaponSdkOwnerTested'] = $false
$record['nativePedOnScreenOwnerTested'] = $false
$record['actualPedOnScreenSdkOwnerTested'] = $false
$record['nativePedVisibilityOwnerTested'] = $false
$record['actualPedVisibilitySdkOwnerTested'] = $false
$record['nativePedGogglesOwnerTested'] = $false
$record['actualPedGogglesSdkOwnerTested'] = $false
$record['nativePedDuckingSetterOwnerTested'] = $false
$record['actualPedDuckingSetterSdkOwnerTested'] = $false
$record['nativePedFireOwnerTested'] = $false
$record['actualPedFireSdkOwnerTested'] = $false
$record['nativePedCollisionOwnerTested'] = $false
$record['actualPedCollisionSdkOwnerTested'] = $false
$record['nativePedAreaOwnerTested'] = $false
$record['actualPedAreaSdkOwnerTested'] = $false
$record['nativeStreamingSpecialModelOwnerTested'] = $false
$record['actualStreamingSpecialModelSdkOwnerTested'] = $false
$record['nativeAnimationManagerOwnerTested'] = $false
$record['actualAnimationManagerSdkOwnerTested'] = $false
$record['nativeAnimationBlockNameOwnerTested'] = $false
$record['actualAnimationBlockNameSdkOwnerTested'] = $false
$record['nativeAnimationBlockOwnerTested'] = $false
$record['actualAnimationBlockSdkOwnerTested'] = $false
$record['nativePedOrientationOwnerTested'] = $false
$record['actualPedOrientationOwnerTested'] = $false
$record['nativePedWeaponInstanceOwnerTested'] = $false
$record['nativePedCurrentWeaponSlotOwnerTested'] = $false
$record['nativeNamedGarageOwnerTested'] = $false
$record['actualNamedGarageSdkOwnerTested'] = $false
$record['nativeNamedGarageGeometryOwnerTested'] = $false
$record['nativePoolsPedRegistryOwnerTested'] = $false
$record['nativeConstructorWeaponPreparationOwnerTested'] = $false
$record['nativeConstructorWeaponFailureOwnerTested'] = $false
$record['nativeConstructorWeaponFrameRecoveryOwnerTested'] = $false
$record['nativeWeaponTimingRecoveryRegressionTested'] = $false
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
        'Client/game_de/NativeRopesAccess.h','Client/game_de/NativeRopesMemory.h','Client/game_de/NativeRopesMemory.cpp',
        'Client/game_de/NativeRopesProfile.h','Client/game_de/CRopesDE.h','Client/game_de/CRopesDE.cpp','Client/sdk/game/CRopes.h','Shared/sdk/SharedUtil.Defines.h',
        'Tests/definitive/NativeRopesOwnerTests.inl',
        'Client/game_de/NativePedTransformReader.h','Client/game_de/NativePedTransformReader.cpp','Client/game_de/NativePedTransformProfile.h',
        'Client/game_de/NativeAnimationManagerAccess.h','Client/game_de/NativeAnimationManagerMemory.h','Client/game_de/NativeAnimationManagerMemory.cpp','Client/game_de/NativeAnimationManagerProfile.h','Client/game_de/CAnimManagerDE.h','Client/game_de/CAnimManagerDE.cpp','Client/sdk/game/CAnimManager.h',
        'Client/game_de/NativePedVisibilityAccess.h','Client/game_de/NativePedVisibilityMemory.h','Client/game_de/NativePedVisibilityMemory.cpp','Client/game_de/NativePedVisibilityProfile.h','Client/game_de/CPedVisibilityDE.h','Client/game_de/CPedVisibilityDE.cpp',
        'Client/game_de/NativePedGogglesAccess.h','Client/game_de/NativePedGogglesMemory.h','Client/game_de/NativePedGogglesMemory.cpp','Client/game_de/NativePedGogglesProfile.h','Client/game_de/CPedGogglesDE.h','Client/game_de/CPedGogglesDE.cpp',
        'Client/game_de/CPedTasksDE.h','Client/game_de/CPedTasksDE.cpp','Client/sdk/game/CPedTaskPolicy.h','Client/sdk/game/CPedIntelligence.h','Client/sdk/game/CTaskManager.h','Client/sdk/game/Task.h','Client/sdk/game/TaskTypes.h',
        'Client/sdk/game/CEntityFirePolicy.h','Client/game_de/NativePedFireAccess.h','Client/game_de/NativePedFireMemory.h','Client/game_de/NativePedFireMemory.cpp','Client/game_de/NativePedFireProfile.h','Client/game_de/CPedFireDE.h','Client/game_de/CPedFireDE.cpp','Client/game_de/NativePedCollisionAccess.h','Client/game_de/NativePedCollisionMemory.h','Client/game_de/NativePedCollisionMemory.cpp','Client/game_de/NativePedCollisionProfile.h','Client/game_de/CPedCollisionDE.h','Client/game_de/CPedCollisionDE.cpp',
        'Client/game_de/NativePedAreaAccess.h','Client/game_de/NativePedAreaMemory.h','Client/game_de/NativePedAreaMemory.cpp','Client/game_de/NativePedAreaProfile.h','Client/game_de/CPedAreaDE.h','Client/game_de/CPedAreaDE.cpp','Client/game_de/NativeStreamingSpecialModelCalls.h','Client/game_de/NativeStreamingSpecialModelCalls.cpp','Client/game_de/NativeStreamingSpecialModelProfile.h','Client/game_de/NativeStreamingFileIdsProfile.h','Client/game_de/NativeStreamingRemoveAdmission.h','Client/game_de/NativeStreamingRemoveMemory.h','Client/game_de/NativeModelReferenceHeap.h',
        'Client/game_de/NativeAnimationBlockAccess.h','Client/game_de/NativeAnimationBlockMemory.h','Client/game_de/NativeAnimationBlockMemory.cpp','Client/game_de/NativeAnimationBlockProfile.h','Client/game_de/CAnimBlockDE.h','Client/game_de/CAnimBlockDE.cpp','Client/sdk/game/CAnimBlock.h','Client/game_de/CModelInfoRequestDE.h','Client/game_de/CModelInfoRequestDE.cpp','Client/sdk/game/CModelRequestPolicy.h','Client/game_de/CAnimBlockRequestDE.h','Client/game_de/CAnimBlockRequestDE.cpp','Client/sdk/game/CAnimBlockRequestPolicy.h',
        'Client/game_de/NativePedOrientationAccess.h','Client/game_de/NativePedOrientationMemory.h','Client/game_de/NativePedOrientationMemory.cpp','Client/game_de/NativePedOrientationProfile.h','Client/game_de/NativePedPoseContext.h','Client/game_de/NativePedPoseContext.cpp','Client/game_de/CPedOrientationDE.h','Client/game_de/CPedOrientationDE.cpp','Client/sdk/game/CGameModelPreparationPolicy.h','Client/game_de/NativePedWaterTaskAccess.h','Client/game_de/NativePedWaterTaskReader.h','Client/game_de/NativePedWaterTaskReader.cpp','Client/game_de/NativePedWaterTaskProfile.h','Client/game_de/NativePedWaterFlagsAccess.h','Client/game_de/NativePedWaterFlagsMemory.h','Client/game_de/NativePedWaterFlagsMemory.cpp','Client/game_de/NativePedWaterFlagsProfile.h','Client/game_de/CPedWeaponStatsDE.h','Client/game_de/CPedWeaponStatsDE.cpp','Client/sdk/game/CPedWeaponStatPolicy.h','Client/game_de/CPedWaterDE.h','Client/game_de/CPedWaterDE.cpp','Client/game_de/CGameInitializationDE.h','Client/game_de/CGameInitializationDE.cpp','Client/sdk/game/CGameInitializationPolicy.h','Client/game_de/NativePedPlacementAccess.h','Client/game_de/NativePedPlacementMemory.h','Client/game_de/NativePedPlacementMemory.cpp','Client/game_de/NativePedPoseMutation.h','Client/game_de/CPedPoseDE.h','Client/game_de/CPedPoseDE.cpp','Client/sdk/game/CEntityPlacementPolicy.h','Client/game_de/NativePedTeleportAccess.h','Client/game_de/NativePedTeleportMemory.h','Client/game_de/NativePedTeleportMemory.cpp','Client/game_de/NativePedTeleportProfile.h',
        'Client/game_de/NativePedVelocityAccess.h','Client/game_de/NativePedVelocityMemory.h','Client/game_de/NativePedVelocityMemory.cpp','Client/game_de/NativePedVelocityProfile.h',
        'Client/sdk/game/CPoolSlotPolicy.h','Tests/definitive/NativePedVelocityOwnerTests.inl',
        'Tests/definitive/NativePedTeleportOwnerTests.inl',
        'Client/game_de/NativePedReferenceAccess.h','Client/game_de/NativePedReferenceMemory.h','Client/game_de/NativePedReferenceMemory.cpp','Client/game_de/NativePedReferenceProfile.h',
        'Client/game_de/NativePedReferenceToken.h','Client/game_de/NativePedReferenceController.h','Client/game_de/NativePedReferenceController.cpp','Tests/definitive/NativePedReferenceOwnerTests.inl',
        'Client/game_de/NativePedMutationAccess.h','Tests/definitive/NativePedMutationOwnerTests.inl',
        'Client/game_de/NativePedVitalsAccess.h','Client/game_de/NativePedVitalsMemory.h','Client/game_de/NativePedVitalsMemory.cpp','Client/game_de/NativePedVitalsProfile.h','Tests/definitive/NativePedVitalsOwnerTests.inl',
        'Client/game_de/NativePedPhysicalScalarsAccess.h','Client/game_de/NativePedPhysicalScalarsMemory.h','Client/game_de/NativePedPhysicalScalarsMemory.cpp','Client/game_de/NativePedPhysicalScalarsProfile.h','Tests/definitive/NativePedPhysicalScalarsOwnerTests.inl','Tests/definitive/NativePedPhysicalDynamicsOwnerTests.inl',
        'Client/game_de/NativePedMetadataAccess.h','Client/game_de/NativePedMetadataMemory.h','Client/game_de/NativePedMetadataMemory.cpp','Client/game_de/NativePedMetadataProfile.h','Tests/definitive/NativePedMetadataOwnerTests.inl',
        'Client/game_de/NativePedObservationAccess.h','Tests/definitive/NativePedObservationOwnerTests.inl',
        'Client/game_de/NativePedIntelligenceReader.h','Client/game_de/NativePedIntelligenceReader.cpp','Client/game_de/NativePedIntelligenceProfile.h','Tests/definitive/NativePedIntelligenceOwnerTests.inl',
        'Client/game_de/NativeTaskPoolReader.h','Client/game_de/NativeTaskPoolReader.cpp','Client/game_de/NativeTaskPoolProfile.h','Tests/definitive/NativeTaskPoolOwnerTests.inl',
        'Client/game_de/NativeTaskRemovalAccess.h','Client/game_de/NativeTaskRemovalCalls.h','Client/game_de/NativeTaskRemovalCalls.cpp','Client/game_de/NativeTaskRemovalProfile.h','Tests/definitive/NativeTaskSecondaryOwnerTests.inl',
        'Client/game_de/NativeModelBankReader.h','Client/game_de/NativeModelBankReader.cpp','Client/game_de/NativeModelBankProfile.h','Tests/definitive/NativeModelBankOwnerTests.inl',
        'Client/game_de/NativeModelLoadedReader.h','Client/game_de/NativeModelLoadedReader.cpp','Client/game_de/NativeModelLoadedProfile.h','Tests/definitive/NativeModelLoadedOwnerTests.inl',
        'Client/game_de/NativeStreamingRequestCalls.h','Client/game_de/NativeStreamingRequestCalls.cpp','Client/game_de/NativeStreamingRequestProfile.h','Tests/definitive/NativeStreamingRequestOwnerTests.inl','Tests/definitive/NativeModelOwnershipOwnerTests.inl',
        'Client/game_de/NativeStreamingModelContext.h','Client/game_de/NativeStreamingModelContext.cpp','Client/game_de/NativeModelReferenceMemory.h','Client/game_de/NativeModelReferenceMemory.cpp','Client/game_de/NativeModelReferenceProfile.h','Client/game_de/NativeStreamingUpgradeReader.h','Client/game_de/NativeStreamingUpgradeReader.cpp','Client/game_de/NativeStreamingUpgradeProfile.h','Client/game_de/NativeStreamingUpgradeCalls.h','Client/game_de/NativeStreamingUpgradeCalls.cpp',
        'Client/game_de/NativeStreamingModelAccess.h','Client/game_de/CStreamingDE.h','Client/game_de/CStreamingDE.cpp','Client/sdk/game/CStreaming.h','Client/sdk/game/CModelRequestPolicy.h',
        'Build/definitive-recovery/mta-native-model-loaded-proof-20261022/model-loaded-qualified.json',
        'Build/definitive-recovery/mta-native-streaming-request-proof-20261022/native-proof/streaming-request-qualified.json',
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
    $sourceNames += @(
        'Client/game_de/NativeStreamingLoadAllCalls.h',
        'Client/game_de/NativeStreamingLoadAllCalls.cpp',
        'Client/game_de/NativeStreamingLoadAllProfile.h',
        'Tests/definitive/NativeStreamingLoadAllGraphFixture.h',
        'Tests/definitive/NativeStreamingCompletionOwnerTests.inl',
        'Client/game_de/NativeStreamingResourceCalls.h',
        'Client/game_de/NativeStreamingResourceCalls.cpp',
        'Client/game_de/NativeStreamingResourceContext.h',
        'Client/game_de/NativeStreamingResourceContext.cpp',
        'Client/game_de/NativeStreamingResourceProfile.h',
        'Client/game_de/NativeModelReferenceTransactions.h',
        'Client/game_de/NativeModelReferenceTransactions.cpp',
        'Client/game_de/NativeModelReferenceTransactionProfile.h',
        'Client/game_de/NativeModelReferenceMap.h',
        'Client/game_de/NativeModelReferenceHeap.h'
    )
    $sourceNames += @(
        'Client/game_de/NativeModelClassificationProfile.h',
        'Client/game_de/NativeModelClassificationCalls.h',
        'Client/game_de/NativeModelClassificationCalls.cpp',
        'Client/game_de/NativeModelClassificationAccess.h',
        'Client/game_de/NativeModelTypeLeaf.h',
        'Client/game_de/NativeStreamingCompletionContext.h',
        'Client/game_de/NativeStreamingCompletionContext.cpp',
        'Client/game_de/NativeStreamingPriorityCalls.h',
        'Client/game_de/NativeStreamingPriorityCalls.cpp',
        'Client/game_de/NativeStreamingPriorityProfile.h',
        'Client/game_de/NativeStreamingRemoveCalls.h',
        'Client/game_de/NativeStreamingRemoveCalls.cpp',
        'Client/game_de/NativeStreamingRemoveProfile.h',
        'Client/game_de/NativeStreamingRemoveMemory.h',
        'Tests/definitive/NativeModelClassificationOwnerTests.inl',
        'Tests/definitive/NativeStreamingRemovalOwnerTests.inl',
        'Tests/definitive/NativeStreamingPriorityOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativeStreamingModelGroupsReader.h',
        'Client/game_de/NativeStreamingModelGroupsReader.cpp',
        'Client/game_de/NativeStreamingModelGroupsProfile.h',
        'Client/game_de/NativeModelArchiveReader.h',
        'Client/game_de/NativeModelArchiveReader.cpp',
        'Client/game_de/NativeModelArchiveProfile.h',
        'Tests/definitive/NativeStreamingObserversOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/CWeaponStatManagerDE.h',
        'Client/game_de/CWeaponStatManagerDE.cpp',
        'Client/game_de/CWeaponStatManagerPolicyDE.h',
        'Client/game_de/CWeaponStatManagerPolicyDE.cpp',
        'Client/sdk/game/CWeaponStatManager.h',
        'Client/sdk/game/CWeaponStatListPolicy.h',
        'Client/sdk/game/CWeaponStatDefaults.h',
        'Client/sdk/game/CWeaponStatOwned.h',
        'Client/sdk/game/CWeaponStatData.h',
        'Tests/definitive/NativeWeaponStatManagerOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativeModelReferenceRelease.h',
        'Client/game_de/NativeModelReferenceRelease.cpp',
        'Client/game_de/NativeStreamingRemoveAdmission.h',
        'Tests/definitive/NativeModelFinalReleaseOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativePedRotationAccess.h',
        'Client/game_de/NativePedRotationMemory.h',
        'Client/game_de/NativePedRotationMemory.cpp',
        'Client/game_de/NativePedRotationProfile.h',
        'Client/game_de/NativeStreamingSceneCalls.h',
        'Client/game_de/NativeStreamingSceneCalls.cpp',
        'Client/game_de/NativeStreamingSceneProfile.h',
        'Tests/definitive/NativePedRotationOwnerTests.inl',
        'Tests/definitive/NativeStreamingSceneOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativePedOxygenAccess.h',
        'Client/game_de/NativePedOxygenMemory.h',
        'Client/game_de/NativePedOxygenMemory.cpp',
        'Client/game_de/NativePedOxygenProfile.h',
        'Client/game_de/NativePedDuckingAccess.h',
        'Client/game_de/NativePedDuckingMemory.h',
        'Client/game_de/NativePedDuckingMemory.cpp',
        'Client/game_de/NativePedDuckingProfile.h',
        'Tests/definitive/NativePedOxygenStateOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativePedBikeFlagsAccess.h',
        'Client/game_de/NativePedBikeFlagsMemory.h',
        'Client/game_de/NativePedBikeFlagsMemory.cpp',
        'Client/game_de/NativePedBikeFlagsProfile.h',
        'Client/game_de/NativePedShotFlagsAccess.h',
        'Client/game_de/NativePedShotFlagsMemory.h',
        'Client/game_de/NativePedShotFlagsMemory.cpp',
        'Client/game_de/NativePedShotFlagsProfile.h',
        'Tests/definitive/NativePedFlagsOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativePedBleedingAccess.h',
        'Client/game_de/NativePedBleedingMemory.h',
        'Client/game_de/NativePedBleedingMemory.cpp',
        'Client/game_de/NativePedBleedingProfile.h',
        'Client/game_de/NativePedStayAccess.h',
        'Client/game_de/NativePedStayMemory.h',
        'Client/game_de/NativePedStayMemory.cpp',
        'Client/game_de/NativePedStayProfile.h',
        'Tests/definitive/NativePedBehaviorOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativePedFightingStyleAccess.h',
        'Client/game_de/NativePedFightingStyleMemory.h',
        'Client/game_de/NativePedFightingStyleMemory.cpp',
        'Client/game_de/NativePedFightingStyleProfile.h',
        'Client/sdk/game/CPedFightingStylePolicy.h',
        'Build/definitive-recovery/mta-native-ped-fighting-style-20261023/fighting-style-qualified.json',
        'Tests/definitive/NativePedCombatOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativePedWeaponContext.h',
        'Client/game_de/NativePedWeaponContext.cpp',
        'Client/game_de/NativePedWeaponProfile.h',
        'Client/game_de/NativePedWeaponFieldsAccess.h',
        'Client/game_de/NativePedWeaponFieldsMemory.h',
        'Client/game_de/NativePedWeaponFieldsMemory.cpp',
        'Client/game_de/NativePedWeaponAmmoAccess.h',
        'Client/game_de/NativePedWeaponAmmoMemory.h',
        'Client/game_de/NativePedWeaponAmmoMemory.cpp',
        'Client/game_de/NativePedWeaponAmmoProfile.h',
        'Client/game_de/CWeaponPolicyDE.h',
        'Client/game_de/CWeaponNativeDE.h',
        'Client/game_de/CWeaponNativeDE.cpp',
        'Client/sdk/game/CWeapon.h',
        'Client/sdk/game/CWeaponInfo.h',
        'Client/sdk/game/CWeaponInfoIndexPolicy.h',
        'Build/definitive-recovery/mta-native-weapon-instance-fields-20261023/native-proof/weapon-instance-fields-qualified.json',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/ped-weapon-ammo-qualified.json',
        'Build/definitive-recovery/mta-weapon-info-lookup-reuse-20261023/weapon-info-lookup-qualified-v2.json',
        'Tests/definitive/NativePedWeaponOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativePedCurrentWeaponSlotAccess.h',
        'Client/game_de/NativePedCurrentWeaponSlotMemory.h',
        'Client/game_de/NativePedCurrentWeaponSlotMemory.cpp',
        'Client/game_de/NativePedCurrentWeaponSlotProfile.h',
        'Client/game_de/CPedWeaponSelectionDE.h',
        'Client/game_de/CPedWeaponSelectionDE.cpp',
        'Client/game_de/CWeaponPolicyDE.cpp',
        'Build/definitive-recovery/mta-native-ped-current-weapon-slot-20261023/current-slot-read-qualified.json',
        'Build/definitive-recovery/mta-native-ped-current-weapon-slot-20261023/current-weapon-slot-setter-gap.json',
        'Tests/definitive/NativePedCurrentWeaponOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/sdk/game/CGarage.h',
        'Client/sdk/game/CGarageDoorStatePolicy.h',
        'Client/sdk/game/CGarageInitializationPolicy.h',
        'Client/game_de/CGaragesDE.h',
        'Client/game_de/CGaragesDE.cpp',
        'Client/game_de/CGarageNamedDE.h',
        'Client/game_de/CGarageNamedDE.cpp',
        'Client/game_de/NativeNamedGarageAccess.h',
        'Client/game_de/NativeNamedGarageMemory.h',
        'Client/game_de/NativeNamedGarageMemory.cpp',
        'Client/game_de/NativeNamedGarageProfile.h',
        'Build/definitive-recovery/mta-native-garage-bank-20261023/named-garage-properties-qualified.json',
        'Build/definitive-recovery/mta-native-garage-bank-20261023/garage-sdk-mapping-gap.json',
        'Tests/definitive/NativeNamedGarageOwnerTests.inl'
    )
    $sourceNames += @(
        'Client/game_de/NativeNamedGarageGeometryProfile.h',
        'Client/game_de/CPoolsDE.h',
        'Client/game_de/CPoolsDE.cpp',
        'Client/game_de/CPedVitalsDE.h',
        'Client/game_de/CPedVitalsDE.cpp',
        'Client/sdk/game/CPools.h',
        'Client/sdk/game/CPed.h',
        'Client/sdk/game/CPhysical.h',
        'Client/sdk/game/CEntity.h',
        'Client/sdk/game/CPedIK.h',
        'Tests/definitive/Write-PedPoolRegistryFixture.ps1',
        'Tests/definitive/NativePoolsPedRegistryOwnerTests.inl',
        'Build/definitive-recovery/mta-native-garage-geometry-proof-20261023/garage-geometry-proof-ready.json',
        'Build/definitive-recovery/mta-native-named-garage-geometry-20261023/named-garage-geometry-qualified.json',
        'Build/definitive-recovery/mta-pools-ped-registry-de-20261023/pools-ped-registry-source-ready.json'
    )
    $sourceNames += @('Tests/definitive/NativePedVisibilityOwnerTests.inl','Tests/definitive/Write-PedVisibilityFixture.ps1')
    $sourceNames += @('Tests/definitive/NativePedGogglesOwnerTests.inl','Tests/definitive/Write-PedGogglesFixture.ps1')
    $sourceNames += @('Tests/definitive/NativePedDuckingSetterOwnerTests.inl','Tests/definitive/Write-PedTasksFixture.ps1')
    $sourceNames += @('Tests/definitive/NativePedFireCollisionOwnerTests.inl','Tests/definitive/Write-PedFireFixture.ps1','Tests/definitive/Write-PedCollisionFixture.ps1')
    $sourceNames += @('Tests/definitive/NativePedAreaOwnerTests.inl','Tests/definitive/NativeStreamingSpecialModelOwnerTests.inl','Tests/definitive/Write-PedAreaFixture.ps1')
    $sourceNames += @('Tests/definitive/NativePedPoseOwnerTests.inl','Tests/definitive/Write-PedPoseFixture.ps1')
    $sourceNames += @('Client/game_de/NativeAnimationHierarchyHashMemory.h','Client/game_de/NativeAnimationHierarchyHashMemory.cpp','Tests/definitive/NativeAnimationGatewayOwnerTests.inl','Tests/definitive/Write-AnimationGatewayFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeAnimationManagerRefsOwnerTests.inl','Tests/definitive/Write-AnimationManagerRefsFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeCustomAnimationResourcesOwnerTests.inl','Tests/definitive/Write-AnimationSequenceStorageFixture.ps1')
    $sourceNames += @('Client/game_de/NativeAnimationSequenceStorageAccess.h','Client/game_de/NativeCustomAnimSequenceController.h','Client/game_de/NativeCustomAnimSequenceController.cpp','Client/game_de/NativeCustomAnimSequenceStorage.h','Client/game_de/NativeCustomAnimSequenceStorage.cpp','Client/game_de/NativeCustomAnimSequenceStorageProfile.h','Tests/definitive/NativeCustomAnimSequenceStorageFixture.h','Tests/definitive/NativeCustomSequenceStorageOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativeAnimationBlockFactoryMemory.h','Client/game_de/NativeAnimationBlockFactoryMemory.cpp','Client/game_de/NativeAnimationBlockHierarchyProfile.h','Tests/definitive/NativeAnimationBlockFactoryFixture.h','Tests/definitive/NativeAnimationBlockFactoryOwnerTests.inl','Tests/definitive/Write-AnimationBlockFactoryFixture.ps1','Tests/definitive/Write-AnimationManagerFixture.ps1','Client/sdk/game/CAnimBlock.h')
    $sourceNames += @('Client/game_de/NativeAnimationBlockRefMemory.h','Client/game_de/NativeAnimationBlockRefMemory.cpp','Client/game_de/NativeAnimationBlockRefProfile.h','Tests/definitive/NativeAnimationBlockRefFixture.h','Tests/definitive/NativeAnimationBlockRefsOwnerTests.inl','Tests/definitive/Write-AnimBlockRefsFixture.ps1','Tests/definitive/AnimBlockNameDETests.cpp')
    $sourceNames += @('Tests/definitive/NativeAnimationBlockOwnerTests.inl','Tests/definitive/NativeAnimationBlockNameOwnerTests.inl','Tests/definitive/NativeAnimationManagerOwnerTests.inl','Tests/definitive/AnimationManagerOwnerFixture.h')
    $sourceNames += @('Tests/definitive/NativePedOrientationOwnerTests.inl','Tests/definitive/Write-PedOrientationFixture.ps1','Tests/definitive/NativePedWaterOwnerTests.inl','Tests/definitive/NativePedWaterTaskOwnerTests.inl','Tests/definitive/Write-PedWaterFixture.ps1')
    $sourceNames += @($nativeFiles | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName).Replace('\','/') })
    $detoursNames = @('detours.cpp','modules.cpp','disasm.cpp','image.cpp','creatwth.cpp')
    $sourceNames += @($detoursNames + @('detours.h') | ForEach-Object { 'vendor/detours/4.0.1/src/' + $_ })
    $sourceNames += @('Client/game_de/NativeWeaponConstructorProgress.h','Tests/definitive/NativeGameConstructorWeaponOwnerTests.inl',
        'Client/sdk/game/CWeaponStartupPolicy.h')
    $sourceNames += @('Client/game_de/NativeGameConstructorServices.h','Client/game_de/CGameDE.h','Client/game_de/CGameDE.cpp','Client/game_de/CGameWeaponInfoDE.h','Client/game_de/CGameWeaponInfoDE.cpp','Client/game_de/CWeaponStatFactoryDE.h','Client/game_de/CWeaponStatFactoryDE.cpp','Client/game_de/CKeyGenDE.h','Client/game_de/CKeyGenDE.cpp','Client/game_de/CAsyncLoadingDE.h','Client/game_de/CAsyncLoadingDE.cpp','Client/game_de/NativeDataFileNamesReader.h','Client/game_de/NativeDataFileNamesReader.cpp','Client/game_de/NativeDataFileNamesProfile.h','Client/game_de/NativeStreamingFileIdsReader.h','Client/game_de/NativeStreamingFileIdsReader.cpp','Client/game_de/NativeGameStateReader.h','Client/game_de/NativeGameStateReader.cpp','Client/game_de/CGameResetDE.h','Client/game_de/CGameResetDE.cpp','Client/sdk/game/CGameResetPolicy.h','Client/sdk/game/CWeaponInfoIndexPolicy.h','Client/sdk/game/CKeyGen.h','Tests/definitive/NativeGameConstructorServicesOwnerTests.inl','Tests/definitive/Write-GameConstructorServicesFixture.ps1')
$sourceNames += @('Client/game_de/CGameWorldDE.h','Client/game_de/CGameWorldDE.cpp','Client/sdk/game/CGameWorldPolicy.h')
$sourceNames += @('Client/sdk/game/CAudioContainerFiles.h','Client/sdk/game/CAudioContainerFiles.cpp','Client/sdk/game/CAudioContainer.h')
$sourceNames += @('Client/game_de/NativeWorldGroundAccess.h','Client/game_de/CWorldGroundDE.h','Client/game_de/CWorldGroundDE.cpp','Client/game_de/NativeWorldGroundCalls.h','Client/game_de/NativeWorldGroundCalls.cpp','Client/game_de/NativeWorldGroundContext.h','Client/game_de/NativeWorldGroundContext.cpp','Client/game_de/NativeWorldGroundProfile.h','Client/sdk/game/CWorld.h','Shared/sdk/CVector2D.h')
$sourceNames += @('Client/game_de/NativeWorldLineAccess.h','Client/game_de/CWorldLineDE.h','Client/game_de/CWorldLineDE.cpp','Client/game_de/NativeWorldLineCalls.h','Client/game_de/NativeWorldLineCalls.cpp','Client/game_de/NativeWorldLineProfile.h')
$sourceNames += @('Client/game_de/NativeWorldAreaAccess.h','Client/game_de/CWorldAreaDE.h','Client/game_de/CWorldAreaDE.cpp','Client/game_de/NativeWorldAreaMemory.h','Client/game_de/NativeWorldAreaMemory.cpp','Client/game_de/NativeWorldAreaProfile.h')
$sourceNames += @('Client/game_de/NativeWorldJetpackHeightAccess.h','Client/game_de/CWorldJetpackHeightDE.h','Client/game_de/CWorldJetpackHeightDE.cpp','Client/game_de/NativeWorldJetpackMemory.h','Client/game_de/NativeWorldJetpackMemory.cpp','Client/game_de/NativeWorldJetpackProfile.h')
$sourceNames += @('Client/game_de/NativePedOnScreenAccess.h','Client/game_de/CPedOnScreenDE.h','Client/game_de/CPedOnScreenDE.cpp','Client/game_de/NativePedOnScreenCalls.h','Client/game_de/NativePedOnScreenCalls.cpp','Client/game_de/NativePedOnScreenProfile.h')
$sourceNames += @('Client/game_de/NativePedWeaponGrantAccess.h','Client/game_de/CPedGiveWeaponDE.h','Client/game_de/CPedGiveWeaponDE.cpp','Client/game_de/NativePedWeaponGrantCalls.h','Client/game_de/NativePedWeaponGrantCalls.cpp','Client/game_de/NativePedWeaponGrantProfile.h','Client/sdk/game/CPedGiveWeaponPolicy.h')
$sourceNames += @('Client/game_de/NativePedWeaponClearAccess.h','Client/game_de/CPedClearWeaponDE.h','Client/game_de/CPedClearWeaponDE.cpp','Client/game_de/NativePedWeaponClearCalls.h','Client/game_de/NativePedWeaponClearCalls.cpp','Client/game_de/NativePedWeaponClearProfile.h')
$sourceNames += @('Client/game_de/NativeWorldIgnoreEntityAccess.h','Client/game_de/CWorldIgnoreEntityDE.h','Client/game_de/CWorldIgnoreEntityDE.cpp','Client/game_de/NativeWorldIgnoredEntityMemory.h','Client/game_de/NativeWorldIgnoredEntityMemory.cpp','Client/game_de/NativeWorldIgnoredEntityProfile.h')
$sourceNames += @('Client/game_de/NativeCameraSphereVisibleAccess.h','Client/game_de/CCameraSphereVisibleDE.h','Client/game_de/CCameraSphereVisibleDE.cpp','Client/game_de/NativeCameraSphereVisibleCalls.h','Client/game_de/NativeCameraSphereVisibleCalls.cpp','Client/game_de/NativeCameraSphereVisibleProfile.h')
    $sourceNames += @('Tests/definitive/NativePedWeaponGrantFixture.h','Tests/definitive/NativePedWeaponGrantOwnerTests.inl','Tests/definitive/Write-PedGiveWeaponFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeWorldIgnoredEntityFixture.h','Tests/definitive/NativeWorldIgnoreEntityOwnerTests.inl','Tests/definitive/Write-WorldIgnoreEntityFixture.ps1','Client/game_de/WeaponFireHandlerAccess.h')
    $sourceNames += @('Tests/definitive/NativePedOnScreenFixture.h','Tests/definitive/NativePedOnScreenOwnerTests.inl','Tests/definitive/Write-PedOnScreenFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeWorldJetpackFixture.h','Tests/definitive/NativeWorldJetpackOwnerTests.inl','Tests/definitive/Write-WorldJetpackHeightFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeWorldAreaFixture.h','Tests/definitive/NativeWorldAreaOwnerTests.inl','Tests/definitive/Write-WorldAreaFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeWorldLineFixture.h','Tests/definitive/NativeWorldLineOwnerTests.inl','Tests/definitive/Write-WorldLineFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeWorldGroundFixture.h','Tests/definitive/NativeWorldGroundOwnerTests.inl','Tests/definitive/Write-WorldGroundFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeCameraSphereVisibleFixture.h','Tests/definitive/NativeCameraSphereVisibleOwnerTests.inl','Tests/definitive/Write-CameraSphereVisibleFixture.ps1')
    $sourceNames += @('Tests/definitive/NativePedWeaponClearFixture.h','Tests/definitive/NativePedWeaponClearOwnerTests.inl','Tests/definitive/Write-PedClearWeaponFixture.ps1')
    $sourceNames += @('Client/game_de/NativePedClothesAccess.h','Client/game_de/CPedClothesDE.h','Client/game_de/CPedClothesDE.cpp','Client/game_de/NativePedClothesCalls.h','Client/game_de/NativePedClothesCalls.cpp','Client/game_de/NativePedClothesProfile.h','Client/game_de/NativeNameConstructionContext.h','Tests/definitive/NativePedClothesFixture.h','Tests/definitive/NativePedClothesOwnerTests.inl','Tests/definitive/Write-PedClothesFixture.ps1')
    $sourceNames += @('Client/game_de/NativeClothesDescriptorProfile.h','Client/game_de/NativeClothesDescriptorCalls.h','Client/game_de/NativeClothesDescriptorCalls.cpp','Tests/definitive/NativeClothesDescriptorFixture.h','Tests/definitive/NativeModuleClothesInitializerOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativePedMoveAnimAccess.h','Client/game_de/NativePedMoveAnimReader.h','Client/game_de/NativePedMoveAnimReader.cpp','Client/game_de/NativePedMoveAnimProfile.h','Client/game_de/CPlayerPedMoveAnimDE.h','Client/game_de/CPlayerPedMoveAnimDE.cpp','Client/game_de/CPlayerPedWantedDE.h','Tests/definitive/NativePedMoveAnimOwnerTests.inl','Tests/definitive/Write-PlayerMoveAnimFixture.ps1')
    $sourceNames += @('Client/game_de/NativePedRebuildAccess.h','Client/game_de/CPedRebuildDE.h','Client/game_de/CPedRebuildDE.cpp','Client/game_de/NativePedRebuildCalls.h','Client/game_de/NativePedRebuildCalls.cpp','Client/game_de/NativePedRebuildProfile.h','Tests/definitive/NativePedRebuildFixture.h','Tests/definitive/NativePedRebuildOwnerTests.inl','Tests/definitive/Write-PedRebuildFixture.ps1')
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
    . (Join-Path $PSScriptRoot 'Write-PedPoolRegistryFixture.ps1')
    $generatedFixture = Write-PedPoolRegistryFixture $taskRoot (Join-Path $scratchRoot 'PedPoolRegistryFixture.generated.h')
    $generatedEvidence = Join-Path $outputRoot 'PedPoolRegistryFixture.generated.h'
    Copy-Item -LiteralPath $generatedFixture.path -Destination $generatedEvidence
    $record['pedPoolRegistryGeneratedFixture'] = [ordered]@{path=$generatedEvidence;sha256=(Sha $generatedEvidence);
        rejectingPedMethods=$generatedFixture.rejectingPedMethods;rejectingPoolMethods=$generatedFixture.rejectingPoolMethods}
    . (Join-Path $PSScriptRoot 'Write-PedPoseFixture.ps1')
    $poseFixture = Write-PedPoseFixture $taskRoot (Join-Path $scratchRoot 'PedPoseFixture.generated.h')
    $poseEvidence = Join-Path $outputRoot 'PedPoseFixture.generated.h'
    Copy-Item -LiteralPath $poseFixture.path -Destination $poseEvidence
    $record['pedPoseGeneratedFixture'] = [ordered]@{path=$poseEvidence;sha256=(Sha $poseEvidence);rejectingOverrides=$poseFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedWaterFixture.ps1')
    $waterFixture = Write-PedWaterFixture $taskRoot (Join-Path $scratchRoot 'PedWaterFixture.generated.h')
    $waterEvidence = Join-Path $outputRoot 'PedWaterFixture.generated.h'
    Copy-Item -LiteralPath $waterFixture.path -Destination $waterEvidence
    $record['pedWaterGeneratedFixture'] = [ordered]@{path=$waterEvidence;sha256=(Sha $waterEvidence);rejectingOverrides=$waterFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedOrientationFixture.ps1')
    $orientationFixture = Write-PedOrientationFixture $taskRoot (Join-Path $scratchRoot 'PedOrientationFixture.generated.h')
    $orientationEvidence = Join-Path $outputRoot 'PedOrientationFixture.generated.h'
    Copy-Item -LiteralPath $orientationFixture.path -Destination $orientationEvidence
    $record['pedOrientationGeneratedFixture'] = [ordered]@{path=$orientationEvidence;sha256=(Sha $orientationEvidence);rejectingOverrides=$orientationFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedAreaFixture.ps1')
    $areaFixture = Write-PedAreaFixture $taskRoot (Join-Path $scratchRoot 'PedAreaFixture.generated.h')
    $areaEvidence = Join-Path $outputRoot 'PedAreaFixture.generated.h'
    Copy-Item -LiteralPath $areaFixture.path -Destination $areaEvidence
    $record['pedAreaGeneratedFixture'] = [ordered]@{path=$areaEvidence;sha256=(Sha $areaEvidence);rejectingOverrides=$areaFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedCollisionFixture.ps1')
    $collisionFixture = Write-PedCollisionFixture $taskRoot (Join-Path $scratchRoot 'PedCollisionFixture.generated.h')
    $collisionEvidence = Join-Path $outputRoot 'PedCollisionFixture.generated.h'
    Copy-Item -LiteralPath $collisionFixture.path -Destination $collisionEvidence
    $record['pedCollisionGeneratedFixture'] = [ordered]@{path=$collisionEvidence;sha256=(Sha $collisionEvidence);rejectingOverrides=$collisionFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedTasksFixture.ps1')
    $tasksFixture = Write-PedTasksFixture $taskRoot (Join-Path $scratchRoot 'PedTasksFixture.generated.h')
    $tasksEvidence = Join-Path $outputRoot 'PedTasksFixture.generated.h'
    Copy-Item -LiteralPath $tasksFixture.path -Destination $tasksEvidence
    $record['pedTasksGeneratedFixture'] = [ordered]@{path=$tasksEvidence;sha256=(Sha $tasksEvidence);rejectingOverrides=$tasksFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-CameraSphereVisibleFixture.ps1')
    $cameraFixture = Write-CameraSphereVisibleFixture $taskRoot (Join-Path $scratchRoot 'CameraSphereVisibleFixture.generated.h')
    $cameraEvidence = Join-Path $outputRoot 'CameraSphereVisibleFixture.generated.h'
    Copy-Item -LiteralPath $cameraFixture.path -Destination $cameraEvidence
    $record['cameraSphereGeneratedFixture'] = [ordered]@{path=$cameraEvidence;sha256=(Sha $cameraEvidence)}
    . (Join-Path $PSScriptRoot 'Write-WorldIgnoreEntityFixture.ps1')
    $ignoreFixture = Write-WorldIgnoreEntityFixture $taskRoot (Join-Path $scratchRoot 'WorldIgnoreEntityFixture.generated.h')
    $ignoreEvidence = Join-Path $outputRoot 'WorldIgnoreEntityFixture.generated.h'
    Copy-Item -LiteralPath $ignoreFixture.path -Destination $ignoreEvidence
    $record['worldIgnoreGeneratedFixture'] = [ordered]@{path=$ignoreEvidence;sha256=(Sha $ignoreEvidence)}
    . (Join-Path $PSScriptRoot 'Write-PedRebuildFixture.ps1')
    $rebuildFixture = Write-PedRebuildFixture $taskRoot (Join-Path $scratchRoot 'PedRebuildFixture.generated.h')
    $rebuildEvidence = Join-Path $outputRoot 'PedRebuildFixture.generated.h'
    Copy-Item -LiteralPath $rebuildFixture.path -Destination $rebuildEvidence
    $record['pedRebuildGeneratedFixture'] = [ordered]@{path=$rebuildEvidence;sha256=(Sha $rebuildEvidence)}
    . (Join-Path $PSScriptRoot 'Write-PedClothesFixture.ps1')
    $clothesFixture = Write-PedClothesFixture $taskRoot (Join-Path $scratchRoot 'PedClothesFixture.generated.h')
    $clothesEvidence = Join-Path $outputRoot 'PedClothesFixture.generated.h'
    Copy-Item -LiteralPath $clothesFixture.path -Destination $clothesEvidence
    $record['pedClothesGeneratedFixture'] = [ordered]@{path=$clothesEvidence;sha256=(Sha $clothesEvidence)}
    . (Join-Path $PSScriptRoot 'Write-PedClearWeaponFixture.ps1')
    $clearWeaponFixture = Write-PedClearWeaponFixture $taskRoot (Join-Path $scratchRoot 'PedClearWeaponFixture.generated.h')
    $clearWeaponEvidence = Join-Path $outputRoot 'PedClearWeaponFixture.generated.h'
    Copy-Item -LiteralPath $clearWeaponFixture.path -Destination $clearWeaponEvidence
    $record['pedClearWeaponGeneratedFixture'] = [ordered]@{path=$clearWeaponEvidence;sha256=(Sha $clearWeaponEvidence)}
    . (Join-Path $PSScriptRoot 'Write-PedGiveWeaponFixture.ps1')
    $giveWeaponFixture = Write-PedGiveWeaponFixture $taskRoot (Join-Path $scratchRoot 'PedGiveWeaponFixture.generated.h')
    $giveWeaponEvidence = Join-Path $outputRoot 'PedGiveWeaponFixture.generated.h'
    Copy-Item -LiteralPath $giveWeaponFixture.path -Destination $giveWeaponEvidence
    $record['pedGiveWeaponGeneratedFixture'] = [ordered]@{path=$giveWeaponEvidence;sha256=(Sha $giveWeaponEvidence);rejectingOverrides=$giveWeaponFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedOnScreenFixture.ps1')
    $onScreenFixture = Write-PedOnScreenFixture $taskRoot (Join-Path $scratchRoot 'PedOnScreenFixture.generated.h')
    $onScreenEvidence = Join-Path $outputRoot 'PedOnScreenFixture.generated.h'
    Copy-Item -LiteralPath $onScreenFixture.path -Destination $onScreenEvidence
    $record['pedOnScreenGeneratedFixture'] = [ordered]@{path=$onScreenEvidence;sha256=(Sha $onScreenEvidence);rejectingOverrides=$onScreenFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedVisibilityFixture.ps1')
    $visibilityFixture = Write-PedVisibilityFixture $taskRoot (Join-Path $scratchRoot 'PedVisibilityFixture.generated.h')
    $visibilityEvidence = Join-Path $outputRoot 'PedVisibilityFixture.generated.h'
    Copy-Item -LiteralPath $visibilityFixture.path -Destination $visibilityEvidence
    $record['pedVisibilityGeneratedFixture'] = [ordered]@{path=$visibilityEvidence;sha256=(Sha $visibilityEvidence);rejectingOverrides=$visibilityFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-PedGogglesFixture.ps1')
    $gogglesFixture = Write-PedGogglesFixture $taskRoot (Join-Path $scratchRoot 'PedGogglesFixture.generated.h')
    $gogglesEvidence = Join-Path $outputRoot 'PedGogglesFixture.generated.h'
    Copy-Item -LiteralPath $gogglesFixture.path -Destination $gogglesEvidence
    $record['pedGogglesGeneratedFixture'] = [ordered]@{path=$gogglesEvidence;sha256=(Sha $gogglesEvidence);rejectingOverrides=$gogglesFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-WorldJetpackHeightFixture.ps1')
    $jetpackHeightFixture = Write-WorldJetpackHeightFixture $taskRoot (Join-Path $scratchRoot 'WorldJetpackHeightFixture.generated.h')
    $jetpackHeightEvidence = Join-Path $outputRoot 'WorldJetpackHeightFixture.generated.h'
    Copy-Item -LiteralPath $jetpackHeightFixture.path -Destination $jetpackHeightEvidence
    $record['worldJetpackHeightGeneratedFixture'] = [ordered]@{path=$jetpackHeightEvidence;sha256=(Sha $jetpackHeightEvidence);rejectingOverrides=$jetpackHeightFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-WorldAreaFixture.ps1')
    $areaFixture = Write-WorldAreaFixture $taskRoot (Join-Path $scratchRoot 'WorldAreaFixture.generated.h')
    $areaEvidence = Join-Path $outputRoot 'WorldAreaFixture.generated.h'
    Copy-Item -LiteralPath $areaFixture.path -Destination $areaEvidence
    $record['worldAreaGeneratedFixture'] = [ordered]@{path=$areaEvidence;sha256=(Sha $areaEvidence);rejectingOverrides=$areaFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-WorldLineFixture.ps1')
    $lineFixture = Write-WorldLineFixture $taskRoot (Join-Path $scratchRoot 'WorldLineFixture.generated.h')
    $lineEvidence = Join-Path $outputRoot 'WorldLineFixture.generated.h'
    Copy-Item -LiteralPath $lineFixture.path -Destination $lineEvidence
    $record['worldLineGeneratedFixture'] = [ordered]@{path=$lineEvidence;sha256=(Sha $lineEvidence);rejectingOverrides=$lineFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-WorldGroundFixture.ps1')
    $groundFixture = Write-WorldGroundFixture $taskRoot (Join-Path $scratchRoot 'WorldGroundFixture.generated.h')
    $groundEvidence = Join-Path $outputRoot 'WorldGroundFixture.generated.h'
    Copy-Item -LiteralPath $groundFixture.path -Destination $groundEvidence
    $record['worldGroundGeneratedFixture'] = [ordered]@{path=$groundEvidence;sha256=(Sha $groundEvidence);rejectingOverrides=$groundFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-GameConstructorServicesFixture.ps1')
    $constructorFixture = Write-GameConstructorServicesFixture $taskRoot (Join-Path $scratchRoot 'GameConstructorServicesFixture.generated.h')
    $constructorEvidence = Join-Path $outputRoot 'GameConstructorServicesFixture.generated.h'
    Copy-Item -LiteralPath $constructorFixture.path -Destination $constructorEvidence
    $record['constructorServicesGeneratedFixture'] = [ordered]@{path=$constructorEvidence;sha256=(Sha $constructorEvidence);rejectingOverrides=$constructorFixture.rejectingOverrides}
    . (Join-Path $PSScriptRoot 'Write-AnimBlockRefsFixture.ps1')
    $refsFixture = Write-AnimBlockRefsFixture $taskRoot (Join-Path $scratchRoot 'AnimBlockRefsFixture.generated.h')
    $refsEvidence = Join-Path $outputRoot 'AnimBlockRefsFixture.generated.h'
    Copy-Item -LiteralPath $refsFixture.path -Destination $refsEvidence
    $record['animationBlockRefsSdkFixture'] = $refsFixture
    $record.animationBlockRefsSdkFixture.path = $refsEvidence
    . (Join-Path $PSScriptRoot 'Write-AnimationBlockFactoryFixture.ps1')
    $blockFactoryFixture = Write-AnimationBlockFactoryFixture $taskRoot (Join-Path $scratchRoot 'AnimationBlockFactoryManager.generated.h')
    $blockFactoryEvidence = Join-Path $outputRoot 'AnimationBlockFactoryManager.generated.h'
    Copy-Item -LiteralPath $blockFactoryFixture.path -Destination $blockFactoryEvidence
    $record['animationBlockFactoryManagerFixture'] = $blockFactoryFixture
    $record.animationBlockFactoryManagerFixture.path = $blockFactoryEvidence
    . (Join-Path $PSScriptRoot 'Write-AnimationSequenceStorageFixture.ps1')
    $animationStorageFixture = Write-AnimationSequenceStorageFixture $taskRoot (Join-Path $scratchRoot 'AnimationSequenceStorageManager.generated.h')
    $animationStorageEvidence = Join-Path $outputRoot 'AnimationSequenceStorageManager.generated.h'
    Copy-Item -LiteralPath $animationStorageFixture.path -Destination $animationStorageEvidence
    $record['animationSequenceStorageManagerFixture'] = $animationStorageFixture
    $record.animationSequenceStorageManagerFixture.path = $animationStorageEvidence
    . (Join-Path $PSScriptRoot 'Write-AnimationManagerRefsFixture.ps1')
    $managerRefsFixture = Write-AnimationManagerRefsFixture $taskRoot (Join-Path $scratchRoot 'AnimationManagerRefs.generated.h')
    $managerRefsEvidence = Join-Path $outputRoot 'AnimationManagerRefs.generated.h'
    Copy-Item -LiteralPath $managerRefsFixture.path -Destination $managerRefsEvidence
    $record['animationManagerRefsFixture'] = $managerRefsFixture
    $record.animationManagerRefsFixture.path = $managerRefsEvidence
    . (Join-Path $PSScriptRoot 'Write-AnimationGatewayFixture.ps1')
    $gatewayFixture = Write-AnimationGatewayFixture $taskRoot (Join-Path $scratchRoot 'AnimationGateway.generated.h')
    $gatewayEvidence = Join-Path $outputRoot 'AnimationGateway.generated.h'
    Copy-Item -LiteralPath $gatewayFixture.path -Destination $gatewayEvidence
    $gatewayParentEvidence = Join-Path $outputRoot ([IO.Path]::GetFileName($gatewayFixture.parentFixture.path))
    Copy-Item -LiteralPath $gatewayFixture.parentFixture.path -Destination $gatewayParentEvidence
    $gatewayFixture.parentFixture.path = $gatewayParentEvidence
    $record['animationGatewayFixture'] = $gatewayFixture
    $record.animationGatewayFixture.path = $gatewayEvidence
    # This generator makes the shared Rebuild leaf extendable. Run it last
    # and refresh the actual durable siblings; an earlier hash must not name
    # bytes subsequently overwritten by another strict-fixture generator.
    . (Join-Path $PSScriptRoot 'Write-PlayerMoveAnimFixture.ps1')
    $moveFixture = Write-PlayerMoveAnimFixture $taskRoot (Join-Path $scratchRoot 'PlayerMoveAnimFixture.generated.h')
    foreach ($item in @(
        @('PedClearWeaponFixture.generated.h','pedClearWeaponGeneratedFixture'),
        @('PedClothesFixture.generated.h','pedClothesGeneratedFixture'),
        @('PedRebuildFixture.generated.h','pedRebuildGeneratedFixture'),
        @('PlayerMoveAnimFixture.generated.h','playerMoveAnimSdkFixture'))) {
        $source = Join-Path $scratchRoot $item[0]
        $durable = Join-Path $outputRoot $item[0]
        Copy-Item -LiteralPath $source -Destination $durable
        $record[$item[1]] = [ordered]@{path=$durable;sha256=(Sha $durable)}
    }

    $common = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
        ('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'vendor/detours/4.0.1/src')),('/I'+$scratchRoot))
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
        (Join-Path $taskRoot 'Client/game_de/CWeaponStatManagerPolicyDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CWeaponStatManagerDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWantedDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CPlayerInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CRopesDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGaragesDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CGarageNamedDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPoolsDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CPedVitalsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedMetadataDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedMutationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedPhysicsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedRotationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedStateDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedFlagsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedBehaviorDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedCombatDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedWeaponSelectionDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedPoseDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedWeaponStatsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedWaterDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedOrientationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedAreaDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedFireDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedCollisionDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedTasksDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedGogglesDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedVisibilityDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedOnScreenDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedGiveWeaponDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedClearWeaponDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedClothesDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPlayerPedMoveAnimDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedRebuildDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldGroundDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldLineDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldAreaDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldJetpackHeightDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldIgnoreEntityDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CCameraSphereVisibleDE.cpp'),
        # The new owner scope instantiates the actual SDK streaming receiver;
        # link its production implementation rather than a fixture substitute.
        (Join-Path $taskRoot 'Client/game_de/CStreamingDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameInitializationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameWeaponInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponStatFactoryDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CKeyGenDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAsyncLoadingDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingFileIdsReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeGameStateReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeDataFileNamesReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameResetDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameWorldDE.cpp'),
        (Join-Path $taskRoot 'Client/sdk/game/CAudioContainerFiles.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlockDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimManagerDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlockRequestDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CModelInfoRequestDE.cpp'),

        (Join-Path $taskRoot 'Client/game_de/CStatsReadDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CStatsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlan.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideAtomic.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideOwner.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideWin32.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlacement.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideSlot.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBridge.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBorrowGuard.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainLeaf.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedReferenceController.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeImageAccess.cpp')) +
        $runtimeDependencies + @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib','user32.lib') + $objects
    [ordered]@{compiler=$record.compiler;detoursArguments=$detoursArguments;arguments=$arguments} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8NoBOM
    $record.compileRuns += RunBounded $compiler $arguments 'compile' 60000
    if ($record.compileRuns[-1].exitCode -ne 0 -or $record.compileRuns[-1].timedOut) { throw 'Clock SDK/runtime fixture compilation failed.' }
    # The cumulative callback fixture includes current owner and retirement cases.
    # Keep it bounded while allowing the full set to finish on a busy build host.
    $fixtureArguments = @($outputRoot)
    if ($Scope -eq 'WorldJetpack') { $fixtureArguments += '--world-jetpack' }
    if ($Scope -eq 'WorldArea') { $fixtureArguments += '--world-area' }
    if ($Scope -eq 'WorldQueries') { $fixtureArguments += '--world-queries' }
    if ($Scope -eq 'WorldLine') { $fixtureArguments += '--world-line' }
    if ($Scope -eq 'WorldGround') { $fixtureArguments += '--world-ground' }
    if ($Scope -eq 'ConstructorServices') { $fixtureArguments += '--constructor-services' }
    if ($Scope -eq 'AnimationGateway') { $fixtureArguments += '--animation-gateway' }
    if ($Scope -eq 'AnimationManagerRefs') { $fixtureArguments += '--animation-manager-refs' }
    if ($Scope -eq 'AnimationBlockFactory') { $fixtureArguments += '--animation-block-factory' }
    if ($Scope -eq 'AnimationBlockRefs') { $fixtureArguments += '--animation-block-refs' }
    if ($Scope -eq 'PlayerNativePrerequisites') { $fixtureArguments += '--player-native-prerequisites' }
    if ($Scope -eq 'PedMoveAnim') { $fixtureArguments += '--ped-move-anim' }
    if ($Scope -eq 'PedRebuild') { $fixtureArguments += '--ped-rebuild' }
    if ($Scope -eq 'PedClothes') { $fixtureArguments += '--ped-clothes' }
    if ($Scope -eq 'PedWeaponClear') { $fixtureArguments += '--ped-weapon-clear' }
    if ($Scope -eq 'PedWeaponGrant') { $fixtureArguments += '--ped-weapon-grant' }
    if ($Scope -eq 'WorldIgnoreEntity') { $fixtureArguments += '--world-ignore-entity' }
    if ($Scope -eq 'CameraSphereVisible') { $fixtureArguments += '--camera-sphere-visible' }
    if ($Scope -eq 'PedOnScreen') { $fixtureArguments += '--ped-on-screen' }
    if ($Scope -eq 'PedVisibility') { $fixtureArguments += '--ped-visibility' }
    if ($Scope -eq 'PedGoggles') { $fixtureArguments += '--ped-goggles' }
    if ($Scope -eq 'DuckingSetter') { $fixtureArguments += '--ducking-setter' }
    if ($Scope -eq 'FireCollision') { $fixtureArguments += '--fire-collision' }
    if ($Scope -eq 'PedFire') { $fixtureArguments += '--ped-fire' }
    if ($Scope -eq 'PedCollision') { $fixtureArguments += '--ped-collision' }
    if ($Scope -eq 'AreaSpecialModel') { $fixtureArguments += '--area-special-model' }
    if ($Scope -eq 'PedArea') { $fixtureArguments += '--ped-area' }
    if ($Scope -eq 'SpecialModel') { $fixtureArguments += '--special-model' }
    if ($Scope -eq 'AnimationServices') { $fixtureArguments += '--animation-services' }
    if ($Scope -eq 'AnimationManager') { $fixtureArguments += '--animation-manager' }
    if ($Scope -eq 'AnimationBlockName') { $fixtureArguments += '--animation-block-name' }
    if ($Scope -eq 'AnimationBlock') { $fixtureArguments += '--animation-block' }
    if ($Scope -eq 'PedWater') { $fixtureArguments += '--ped-water' }
    if ($Scope -eq 'PedWaterOrientation') { $fixtureArguments += '--ped-water-orientation' }
    if ($Scope -eq 'PedPose') { $fixtureArguments += '--ped-pose' }
    if ($Scope -eq 'PedOwner') { $fixtureArguments += '--ped-owner' }
    if ($Scope -eq 'PedCurrentWeapon') { $fixtureArguments += '--ped-current-weapon' }
    if ($Scope -eq 'GarageNamed') { $fixtureArguments += '--garage-named' }
    if ($Scope -eq 'GaragePools') { $fixtureArguments += '--garage-pools' }
    if ($Scope -eq 'PedPoolRegistry') { $fixtureArguments += '--ped-pool-registry' }
    if ($Scope -eq 'CustomAnimationResources') {
        # Unknown entered effects retain the actual global journal until the
        # ordinary host process exits. Compile once, isolate those cases rather
        # than erase owner debt to make a following scenario fit.
        $record['testRuns'] = @()
        $record['testScenarioExecutables'] = @()
        $texts = @()
        foreach ($scenario in @('success','unknown-allocate','unknown-free')) {
            $scenarioRoot = Join-Path $outputRoot ('scenario-custom-animation-resources-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot) | Out-Null
            $scenarioExecutable = Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if (Test-Path -LiteralPath $scenarioExecutable) { throw 'Scenario executable already exists; preserve prior evidence.' }
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioSha = Sha $scenarioExecutable
            if ($scenarioSha -cne (Sha $executable)) { throw 'Copied custom sequence scenario differs from the once-compiled test binary.' }
            $record.testScenarioExecutables += @{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioSha;directory=$scenarioRoot}
            $run = RunBounded $scenarioExecutable @($scenarioRoot,('--custom-animation-resources-'+$scenario)) ('test-custom-animation-resources-'+$scenario) 60000
            $record.testRuns += $run
            $part = [IO.File]::ReadAllText($run.stdout) + [IO.File]::ReadAllText($run.stderr)
            if ($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks') {
                throw "Mixed custom animation resource $scenario fixture failed."
            }
            $record.checks += [int]$Matches[1]
            $texts += $part
        }
        $record.testRun = $record.testRuns[-1]
        $text = $texts -join [Environment]::NewLine
    } elseif ($Scope -eq 'CustomSequenceStorage') {
        # Unknown entered effects retain the actual global journal until the
        # ordinary host process exits. Compile once, isolate those cases rather
        # than erase owner debt to make a following scenario fit.
        $record['testRuns'] = @()
        $record['testScenarioExecutables'] = @()
        $texts = @()
        foreach ($scenario in @('success','unknown-allocate','unknown-free')) {
            $scenarioRoot = Join-Path $outputRoot ('scenario-custom-sequence-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot) | Out-Null
            $scenarioExecutable = Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if (Test-Path -LiteralPath $scenarioExecutable) { throw 'Scenario executable already exists; preserve prior evidence.' }
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioSha = Sha $scenarioExecutable
            if ($scenarioSha -cne (Sha $executable)) { throw 'Copied custom sequence scenario differs from the once-compiled test binary.' }
            $record.testScenarioExecutables += @{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioSha;directory=$scenarioRoot}
            $run = RunBounded $scenarioExecutable @($scenarioRoot,('--custom-sequence-storage-'+$scenario)) ('test-custom-sequence-'+$scenario) 60000
            $record.testRuns += $run
            $part = [IO.File]::ReadAllText($run.stdout) + [IO.File]::ReadAllText($run.stderr)
            if ($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks') {
                throw "Custom sequence storage $scenario fixture failed."
            }
            $record.checks += [int]$Matches[1]
            $texts += $part
        }
        $record.testRun = $record.testRuns[-1]
        $text = $texts -join [Environment]::NewLine
    } elseif ($Scope -eq 'ConstructorWeapons') {
        # Compile once; each new ordinary-host process owns a fresh one-shot
        # production manager. Historical Clock/weather/ped suites are skipped.
        $record['testRuns'] = @()
        $record['testScenarioExecutables'] = @()
        $texts = @()
        foreach ($scenario in @('success','partial','recovery','timer')) {
            # Each fresh process preserves its own synthetic image. The fixture
            # deliberately refuses overwriting earlier evidence and requires
            # that image beside its executable, so reuse the compiled test
            # binary in distinct owned directories rather than weaken either
            # guard or rebuild it for every scenario.
            $scenarioRoot = Join-Path $outputRoot ('scenario-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot) | Out-Null
            $scenarioExecutable = Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if (Test-Path -LiteralPath $scenarioExecutable) { throw 'Scenario executable already exists; preserve prior evidence.' }
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioSha = Sha $scenarioExecutable
            if ($scenarioSha -cne (Sha $executable)) { throw 'Copied scenario differs from the once-compiled test binary.' }
            $record.testScenarioExecutables += @{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioSha;directory=$scenarioRoot}
            $run = RunBounded $scenarioExecutable @($scenarioRoot,('--weapon-constructor-'+$scenario)) ('test-'+$scenario) 60000
            $record.testRuns += $run
            $part = [IO.File]::ReadAllText($run.stdout) + [IO.File]::ReadAllText($run.stderr)
            if ($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks') {
                throw "Constructor weapon $scenario fixture failed."
            }
            $record.checks += [int]$Matches[1]
            $texts += $part
        }
        $record.testRun = $record.testRuns[-1]
        $text = $texts -join [Environment]::NewLine
    } else {
        $record.testRun = RunBounded $executable $fixtureArguments 'test' 60000
        $text = [IO.File]::ReadAllText($record.testRun.stdout) + [IO.File]::ReadAllText($record.testRun.stderr)
        if ($text -match 'Passed (\d+) production clock service checks') { $record.checks = [int]$Matches[1] }
        if ($record.testRun.exitCode -ne 0 -or $record.testRun.timedOut -or $record.checks -eq 0) { throw 'Clock service fixture failed.' }
    }
    if ($Scope -eq 'AnimationGateway') {
        $record['nativeAnimationHierarchyHashOwnerTested'] = $text.Contains('Production native animation hierarchy hash owner fixture completed')
        $record['actualAnimationGatewaySdkOwnerTested'] = $text.Contains('Production actual animation gateway SDK owner fixture completed')
        if (-not ($record.nativeAnimationHierarchyHashOwnerTested -and $record.actualAnimationGatewaySdkOwnerTested)) { throw 'New gateway hash native/SDK owner scope did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'AnimationManagerRefs') {
        $record['nativeAnimationManagerRefsOwnerTested'] = $text.Contains('Production native animation manager reference owner fixture completed')
        $record['actualAnimationManagerRefsSdkOwnerTested'] = $text.Contains('Production actual animation manager reference SDK owner fixture completed')
        if (-not ($record.nativeAnimationManagerRefsOwnerTested -and $record.actualAnimationManagerRefsSdkOwnerTested)) { throw 'New animation manager refs native/SDK owner scope did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'CustomAnimationResources') {
        $record['nativeCustomAnimationResourcesOwnerTested'] = $text.Contains('Production mixed custom animation resources callback owner fixture completed')
        $record['nativeCustomAnimationResourcesEndTested'] = $text.Contains('Production mixed custom animation resources natural End fixture completed')
        $record['nativeCustomAnimationResourcesRetainedLifetimeTested'] = $text.Contains('Production mixed custom animation resources unknown frame allocation lifetime retained') -and $text.Contains('Production mixed custom animation resources unknown frame free lifetime retained')
        if (-not ($record.nativeCustomAnimationResourcesOwnerTested -and $record.nativeCustomAnimationResourcesEndTested -and $record.nativeCustomAnimationResourcesRetainedLifetimeTested)) { throw 'New native custom sequence storage owner/End/retained-lifetime observations did not complete.' }
        $record['actualCustomAnimationStorageSdkOwnerTested'] = $text.Contains('Production actual custom animation storage SDK callback fixture completed')
        if (-not $record.actualCustomAnimationStorageSdkOwnerTested) { throw 'Actual mixed animation storage SDK callback dispatch did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'CustomSequenceStorage') {
        $record['nativeCustomSequenceStorageOwnerTested'] = $text.Contains('Production native custom sequence storage callback owner fixture completed')
        $record['nativeCustomSequenceStorageEndTested'] = $text.Contains('Production native custom sequence storage natural End fixture completed')
        $record['nativeCustomSequenceStorageRetainedLifetimeTested'] = $text.Contains('Production native custom sequence storage unknown allocation lifetime retained') -and $text.Contains('Production native custom sequence storage unknown free lifetime retained')
        if (-not ($record.nativeCustomSequenceStorageOwnerTested -and $record.nativeCustomSequenceStorageEndTested -and $record.nativeCustomSequenceStorageRetainedLifetimeTested)) { throw 'New native custom sequence storage owner/End/retained-lifetime observations did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'AnimationBlockFactory') {
        $record['actualAnimationBlockFactorySdkOwnerTested'] = $text.Contains('Production actual animation block factory SDK owner fixture completed')
        $record['nativeAnimationBlockFactoryOwnerTested'] = $text.Contains('Production native animation block factory owner fixture completed')
        $record['opaqueAnimationBlockRoundtripOwnerTested'] = $text.Contains('Production opaque animation block roundtrip owner fixture completed')
        if (-not ($record.actualAnimationBlockFactorySdkOwnerTested -and $record.nativeAnimationBlockFactoryOwnerTested -and $record.opaqueAnimationBlockRoundtripOwnerTested)) { throw 'New native/SDK/opaque block factory owner observations did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'AnimationBlockRefs') {
        $record['nativeAnimationBlockRefsOwnerTested'] = $text.Contains('Production native animation block reference owner fixture completed')
        $record['actualAnimationBlockRefsSdkOwnerTested'] = $text.Contains('Production actual animation block reference SDK callback fixture completed')
        $record['sharedAnimBlockRequestRefPolicyOwnerTested'] = $text.Contains('Production shared animation request policy native reference owner ordering completed')
        if (-not ($record.nativeAnimationBlockRefsOwnerTested -and $record.actualAnimationBlockRefsSdkOwnerTested -and $record.sharedAnimBlockRequestRefPolicyOwnerTested)) { throw 'New native/SDK/shared request-policy reference owner observations did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PlayerNativePrerequisites') {
        $record['nativePedMoveAnimOwnerTested'] = $text.Contains('Production native registered move-animation observation owner fixture completed')
        $record['actualPlayerMoveAnimSdkOwnerTested'] = $text.Contains('Production actual CPlayerPed move-animation SDK callback fixture completed')
        $record['nativeModuleClothesInitializerOwnerTested'] = $text.Contains('Production native module-owned clothes initializer callback owner fixture completed')
        if (-not ($record.nativePedMoveAnimOwnerTested -and $record.actualPlayerMoveAnimSdkOwnerTested -and $record.nativeModuleClothesInitializerOwnerTested)) { throw 'New combined player-native prerequisite observations did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedMoveAnim') {
        $record['nativePedMoveAnimOwnerTested'] = $text.Contains('Production native registered move-animation observation owner fixture completed')
        $record['actualPlayerMoveAnimSdkOwnerTested'] = $text.Contains('Production actual CPlayerPed move-animation SDK callback fixture completed')
        if (-not ($record.nativePedMoveAnimOwnerTested -and $record.actualPlayerMoveAnimSdkOwnerTested)) { throw 'New native/actual SDK move-animation observations did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedRebuild') {
        $record.nativePedRebuildOwnerTested = $text.Contains('Production native registered rebuild callback owner fixture completed')
        $record.actualPedRebuildSdkOwnerTested = $text.Contains('Production actual CPed rebuild SDK callback fixture completed')
        if (-not ($record.nativePedRebuildOwnerTested -and $record.actualPedRebuildSdkOwnerTested)) { throw 'New native/actual SDK rebuild owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedClothes') {
        $record.nativePedClothesOwnerTested = $text.Contains('Production native registered clothes callback owner fixture completed')
        $record.actualPedClothesSdkOwnerTested = $text.Contains('Production actual CPed clothes SDK callback fixture completed')
        if (-not ($record.nativePedClothesOwnerTested -and $record.actualPedClothesSdkOwnerTested)) { throw 'New native/actual SDK clothes owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedWeaponClear') {
        $record.nativePedWeaponClearOwnerTested = $text.Contains('Production native registered weapon-clear callback owner fixture completed')
        $record.actualPedClearWeaponSdkOwnerTested = $text.Contains('Production actual CPed ClearWeapon SDK callback fixture completed')
        if (-not ($record.nativePedWeaponClearOwnerTested -and $record.actualPedClearWeaponSdkOwnerTested)) { throw 'New native/actual SDK weapon clear owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'CameraSphereVisible') {
        $record.nativeCameraSphereVisibleOwnerTested = $text.Contains('Production native camera sphere callback owner fixture completed')
        $record.actualCameraSphereVisibleSdkOwnerTested = $text.Contains('Production actual CCamera sphere SDK owner fixture completed')
        if (-not ($record.nativeCameraSphereVisibleOwnerTested -and $record.actualCameraSphereVisibleSdkOwnerTested)) { throw 'New native/actual SDK camera sphere owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'WorldIgnoreEntity') {
        $record.nativeWorldIgnoreEntityOwnerTested = $text.Contains('Production native ignored-entity journal owner fixture completed')
        $record.actualWorldIgnoreEntitySdkOwnerTested = $text.Contains('Production actual CWorld IgnoreEntity SDK owner fixture completed')
        if (-not ($record.nativeWorldIgnoreEntityOwnerTested -and $record.actualWorldIgnoreEntitySdkOwnerTested)) { throw 'New native and actual SDK ignored-entity owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedWeaponGrant') {
        $record.nativePedWeaponGrantOwnerTested = $text.Contains('Production native registered weapon-grant callback owner fixture completed')
        $record.actualPedGiveWeaponSdkOwnerTested = $text.Contains('Production actual CPed GiveWeapon SDK callback fixture completed')
        if (-not ($record.nativePedWeaponGrantOwnerTested -and $record.actualPedGiveWeaponSdkOwnerTested)) { throw 'New native/actual SDK weapon grant owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedOnScreen') {
        $record.nativePedOnScreenOwnerTested = $text.Contains('Production native registered on-screen callback owner fixture completed')
        $record.actualPedOnScreenSdkOwnerTested = $text.Contains('Production actual CEntity CPed on-screen SDK callback fixture completed')
        if (-not ($record.nativePedOnScreenOwnerTested -and $record.actualPedOnScreenSdkOwnerTested)) { throw 'New native/actual SDK on-screen owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'WorldJetpack') {
        $record.nativeWorldJetpackHeightOwnerTested = $text.Contains('Production native jetpack-height guarded callback owner fixture completed')
        $record.actualWorldJetpackHeightSdkOwnerTested = $text.Contains('Production actual CWorld jetpack-height SDK callback fixture completed')
        if (-not ($record.nativeWorldJetpackHeightOwnerTested -and $record.actualWorldJetpackHeightSdkOwnerTested)) { throw 'New native/actual SDK jetpack-height getter owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'WorldQueries') {
        $record.nativeWorldLineOwnerTested = $text.Contains('Production native whole line-clear callback owner fixture completed')
        $record.actualWorldLineSdkOwnerTested = $text.Contains('Production actual CWorld line-clear SDK callback fixture completed')
        $record.nativeWorldAreaOwnerTested = $text.Contains('Production native current-area DWORD callback owner fixture completed')
        $record.actualWorldAreaSdkOwnerTested = $text.Contains('Production actual CWorld current-area SDK callback fixture completed')
        if (-not ($record.nativeWorldLineOwnerTested -and $record.actualWorldLineSdkOwnerTested -and $record.nativeWorldAreaOwnerTested -and $record.actualWorldAreaSdkOwnerTested)) { throw 'Combined new native/actual SDK Line and Area owners did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'WorldArea') {
        $record.nativeWorldAreaOwnerTested = $text.Contains('Production native current-area DWORD callback owner fixture completed')
        $record.actualWorldAreaSdkOwnerTested = $text.Contains('Production actual CWorld current-area SDK callback fixture completed')
        if (-not ($record.nativeWorldAreaOwnerTested -and $record.actualWorldAreaSdkOwnerTested)) { throw 'New native/actual SDK current-area owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'WorldLine') {
        $record.nativeWorldLineOwnerTested = $text.Contains('Production native whole line-clear callback owner fixture completed')
        $record.actualWorldLineSdkOwnerTested = $text.Contains('Production actual CWorld line-clear SDK callback fixture completed')
        if (-not ($record.nativeWorldLineOwnerTested -and $record.actualWorldLineSdkOwnerTested)) { throw 'New native/actual SDK line query owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'WorldGround') {
        $record.nativeWorldGroundOwnerTested = $text.Contains('Production native whole ground query callback owner fixture completed')
        $record.actualWorldGroundSdkOwnerTested = $text.Contains('Production actual CWorld ground query SDK callback fixture completed')
        if (-not ($record.nativeWorldGroundOwnerTested -and $record.actualWorldGroundSdkOwnerTested)) { throw 'New native/actual SDK ground query owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'ConstructorServices') {
        $record.nativeGameConstructorServicesOwnerTested = $text.Contains('Production native constructor service bundle callback owner fixture completed')
        $record.actualGameConstructorServicesSdkOwnerTested = $text.Contains('Production actual prepared constructor service bundle SDK fixture completed')
        if (-not ($record.nativeGameConstructorServicesOwnerTested -and $record.actualGameConstructorServicesSdkOwnerTested)) { throw 'New native/actual SDK constructor-service bundle owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedVisibility') {
        $record.nativePedVisibilityOwnerTested = $text.Contains('Production native registered visibility callback owner fixture completed')
        $record.actualPedVisibilitySdkOwnerTested = $text.Contains('Production actual ped visibility SDK callback fixture completed')
        if (-not ($record.nativePedVisibilityOwnerTested -and $record.actualPedVisibilitySdkOwnerTested)) { throw 'New native/actual SDK visibility owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedGoggles') {
        $record.nativePedGogglesOwnerTested = $text.Contains('Production native registered goggles callback owner fixture completed')
        $record.actualPedGogglesSdkOwnerTested = $text.Contains('Production actual ped goggles SDK callback fixture completed')
        if (-not ($record.nativePedGogglesOwnerTested -and $record.actualPedGogglesSdkOwnerTested)) { throw 'New native/actual SDK goggles query owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'DuckingSetter') {
        $record.nativePedDuckingSetterOwnerTested = $text.Contains('Production native registered ducking setter callback owner fixture completed')
        $record.actualPedDuckingSetterSdkOwnerTested = $text.Contains('Production actual ped ducking setter SDK callback fixture completed')
        if (-not ($record.nativePedDuckingSetterOwnerTested -and $record.actualPedDuckingSetterSdkOwnerTested)) { throw 'New native/actual SDK ducking setter owner did not complete.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -in @('FireCollision','PedFire','PedCollision')) {
        if ($Scope -in @('FireCollision','PedFire')) {
            $record.nativePedFireOwnerTested = $text.Contains('Production native registered fire callback owner fixture completed')
            $record.actualPedFireSdkOwnerTested = $text.Contains('Production actual const ped fire SDK callback fixture completed')
            if (-not ($record.nativePedFireOwnerTested -and $record.actualPedFireSdkOwnerTested)) { throw 'New registered fire native/const SDK owner did not complete.' }
        }
        if ($Scope -in @('FireCollision','PedCollision')) {
            $record.nativePedCollisionOwnerTested = $text.Contains('Production native registered collision callback owner fixture completed')
            $record.actualPedCollisionSdkOwnerTested = $text.Contains('Production actual ped collision SDK callback fixture completed')
            if (-not ($record.nativePedCollisionOwnerTested -and $record.actualPedCollisionSdkOwnerTested)) { throw 'New registered collision native/SDK owner did not complete.' }
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -in @('AreaSpecialModel','PedArea','SpecialModel')) {
        if ($Scope -in @('AreaSpecialModel','PedArea')) {
            $record.nativePedAreaOwnerTested = $text.Contains('Production native registered area callback owner fixture completed')
            $record.actualPedAreaSdkOwnerTested = $text.Contains('Production actual ped area SDK callback fixture completed')
            if (-not ($record.nativePedAreaOwnerTested -and $record.actualPedAreaSdkOwnerTested)) { throw 'New registered area native/SDK owner did not complete.' }
        }
        if ($Scope -in @('AreaSpecialModel','SpecialModel')) {
            $record.nativeStreamingSpecialModelOwnerTested = $text.Contains('Production native special model callback owner fixture completed')
            $record.actualStreamingSpecialModelSdkOwnerTested = $text.Contains('Production actual special model SDK callback fixture completed')
            if (-not ($record.nativeStreamingSpecialModelOwnerTested -and $record.actualStreamingSpecialModelSdkOwnerTested)) { throw 'New genuine special-model native/SDK owner did not complete.' }
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'AnimationServices') {
        $record.nativeAnimationBlockNameOwnerTested = $text.Contains('Production native immediate animation name callback owner fixture completed')
        $record.actualAnimationBlockNameSdkOwnerTested = $text.Contains('Production actual mutable animation name SDK callback fixture completed')
        $record.nativeAnimationManagerOwnerTested = $text.Contains('Production native animation manager callback owner fixture completed')
        $record.actualAnimationManagerSdkOwnerTested = $text.Contains('Production actual animation manager SDK callback fixture completed')
        if (-not ($record.nativeAnimationBlockNameOwnerTested -and $record.actualAnimationBlockNameSdkOwnerTested -and
                  $record.nativeAnimationManagerOwnerTested -and $record.actualAnimationManagerSdkOwnerTested)) {
            throw 'Combined current Name/Manager owner did not execute all four native/SDK paths.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'AnimationManager') {
        $record.nativeAnimationManagerOwnerTested = $text.Contains('Production native animation manager callback owner fixture completed')
        $record.actualAnimationManagerSdkOwnerTested = $text.Contains('Production actual animation manager SDK callback fixture completed')
        if (-not ($record.nativeAnimationManagerOwnerTested -and $record.actualAnimationManagerSdkOwnerTested)) {
            throw 'Native animation manager counts/lookup and actual three SDK methods did not execute.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'AnimationBlockName') {
        $record.nativeAnimationBlockNameOwnerTested = $text.Contains('Production native immediate animation name callback owner fixture completed')
        $record.actualAnimationBlockNameSdkOwnerTested = $text.Contains('Production actual mutable animation name SDK callback fixture completed')
        if (-not ($record.nativeAnimationBlockNameOwnerTested -and $record.actualAnimationBlockNameSdkOwnerTested)) {
            throw 'Immediate native animation name and actual SDK owner did not execute.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'AnimationBlock') {
        $record.nativeAnimationBlockOwnerTested = $text.Contains('Production native animation block callback owner fixture completed')
        $record.actualAnimationBlockSdkOwnerTested = $text.Contains('Production actual animation block SDK callback fixture completed')
        if (-not ($record.nativeAnimationBlockOwnerTested -and $record.actualAnimationBlockSdkOwnerTested)) {
            throw 'Current native animation identity/generation and actual four SDK getters did not execute.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'ConstructorWeapons') {
        $record.nativeConstructorWeaponPreparationOwnerTested = $text.Contains('Production bounded constructor weapon preparation owner fixture completed')
        $record.nativeConstructorWeaponFailureOwnerTested = $text.Contains('Production constructor persistent row failure fixture completed')
        $record.nativeConstructorWeaponFrameRecoveryOwnerTested = $text.Contains('Production constructor startup-frame vector recovery fixture completed')
        $record.nativeWeaponTimingRecoveryRegressionTested = $text.Contains('Production existing timing weapon-vector recovery regression fixture completed')
        if (-not ($record.nativeConstructorWeaponPreparationOwnerTested -and $record.nativeConstructorWeaponFailureOwnerTested -and
                  $record.nativeConstructorWeaponFrameRecoveryOwnerTested -and $record.nativeWeaponTimingRecoveryRegressionTested)) {
            throw 'Current bounded constructor, persistent prefix, normal-frame debt or existing timing journal regression did not complete.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedWater' -or $Scope -eq 'PedWaterOrientation') {
        $record.nativePedWaterFlagsOwnerTested = $text.Contains('Production native ped physical water callback owner fixture completed')
        $record.actualPedWaterOwnerTested = $text.Contains('Production actual physical water SDK callback fixture completed')
        $record.nativePedWaterTaskOwnerTested = $text.Contains('Production native ped primary water task callback owner fixture completed')
        $record.actualPedWaterTaskOwnerTested = $text.Contains('Production actual task-based water SDK callback fixture completed')
        if (-not ($record.nativePedWaterFlagsOwnerTested -and $record.actualPedWaterOwnerTested -and $record.nativePedWaterTaskOwnerTested -and $record.actualPedWaterTaskOwnerTested)) {
            throw 'Actual physical water storage and SDK token owner did not complete.'
        }
        if ($Scope -eq 'PedWaterOrientation') {
            $record.nativePedOrientationOwnerTested = $text.Contains('Production native ped three-phase orientation callback owner fixture completed')
            $record.actualPedOrientationOwnerTested = $text.Contains('Production actual ped three-axis orientation SDK callback fixture completed')
            if (-not ($record.nativePedOrientationOwnerTested -and $record.actualPedOrientationOwnerTested)) { throw 'Combined current water/orientation owner did not execute both new orientation paths.' }
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedPose') {
        $record.nativePedPlacementOwnerTested = $text.Contains('Production native ped placement callback owner fixture completed')
        $record.actualPedPoseOwnerTested = $text.Contains('Production actual ped pose SDK callback fixture completed')
        $record.nativePedTeleportCompletionOwnerTested = $text.Contains('Production native ped teleport completion intent fixture completed')
        if (-not ($record.nativePedPlacementOwnerTested -and $record.actualPedPoseOwnerTested -and $record.nativePedTeleportCompletionOwnerTested)) {
            throw 'Actual ped placement, SDK ordering and same-callback teleport completion did not finish.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'GaragePools') {
        $record.nativeNamedGarageOwnerTested = $text.Contains('Production native named garage callback owner fixture completed')
        $record.actualNamedGarageSdkOwnerTested = $text.Contains('Actual named garage SDK callback owner fixture completed')
        $record.nativeNamedGarageGeometryOwnerTested = $text.Contains('Production native named garage geometry callback owner fixture completed')
        $record.nativePoolsPedRegistryOwnerTested = $text.Contains('Production actual native borrowed ped pool registry callback fixture completed')
        if (-not ($record.nativeNamedGarageOwnerTested -and $record.actualNamedGarageSdkOwnerTested -and
                  $record.nativeNamedGarageGeometryOwnerTested -and $record.nativePoolsPedRegistryOwnerTested)) {
            throw 'Current named8properties and borrowed ped pool registry did not finish through their actual SDK/current callback owners.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedPoolRegistry') {
        $record.nativePoolsPedRegistryOwnerTested = $text.Contains('Production actual native borrowed ped pool registry callback fixture completed')
        if (-not $record.nativePoolsPedRegistryOwnerTested) { throw 'Actual borrowed ped pool registry owner did not finish.' }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'GarageNamed') {
        $record.nativeNamedGarageOwnerTested = $text.Contains('Production native named garage callback owner fixture completed')
        $record.actualNamedGarageSdkOwnerTested = $text.Contains('Actual named garage SDK callback owner fixture completed')
        $record.nativeNamedGarageGeometryOwnerTested = $text.Contains('Production native named garage geometry callback owner fixture completed')
        if (-not ($record.nativeNamedGarageOwnerTested -and $record.actualNamedGarageSdkOwnerTested -and $record.nativeNamedGarageGeometryOwnerTested)) {
            throw 'Named garage properties did not finish through the actual SDK and retained callback owner.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedCurrentWeapon') {
        $record.nativePedOwnerSliceTested = $text.Contains('Production native registered-ped owner slice fixture completed')
        $record.nativePedCurrentWeaponSlotOwnerTested = $text.Contains('Production native current weapon slot callback owner fixture completed')
        if (-not ($record.nativePedOwnerSliceTested -and $record.nativePedCurrentWeaponSlotOwnerTested)) {
            throw 'Current weapon slot read did not finish through the actual selected-ped token callback.'
        }
        $record.productionNativeCallbackTested = $true
    } elseif ($Scope -eq 'PedOwner') {
        $record.nativePedOwnerSliceTested = $text.Contains('Production native registered-ped owner slice fixture completed')
        $record.nativePedPhysicalScalarsOwnerTested = $text.Contains('Production native ped physical scalars callback owner fixture completed')
        $record.nativePedWeaponInstanceOwnerTested = $text.Contains('Production native ped weapon instance callback owner fixture completed')
        if (-not ($record.nativePedOwnerSliceTested -and $record.nativePedPhysicalScalarsOwnerTested -and $record.nativePedWeaponInstanceOwnerTested)) {
            throw 'Current ped weapon owner cases did not complete through the real registered callback fixture.'
        }
        $record.productionNativeCallbackTested = $true
    } else {
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
    $record.nativeRopesOwnerTested = $text.Contains('Production complete native ropes SDK owner fixture completed')
    if (-not $record.nativeRopesOwnerTested) { throw 'Actual complete native ropes SDK owner fixture did not complete.' }
    $record.nativePedPoolOwnerTested = $text.Contains('Production native ped-pool handle callback owner fixture completed')
    if (-not $record.nativePedPoolOwnerTested) { throw 'Actual native ped-pool handle callback owner fixture did not complete.' }
    $record['nativePedTransformOwnerTested'] = $text.Contains('Production native ped-transform callback owner fixture completed')
    if (-not $record.nativePedTransformOwnerTested) { throw 'Actual native ped-transform callback owner fixture did not complete.' }
    $record['nativePedTeleportOwnerTested'] = $text.Contains('Production native ped Teleport callback owner fixture completed')
    if (-not $record.nativePedTeleportOwnerTested) { throw 'Actual native ped Teleport callback owner fixture did not complete.' }
    $record['nativePedVelocityOwnerTested'] = $text.Contains('Production native ped velocity callback owner fixture completed')
    if (-not $record.nativePedVelocityOwnerTested) { throw 'Actual native ped velocity callback owner fixture did not complete.' }
    $record['nativePedReferenceOwnerTested'] = $text.Contains('Production native ped reference callback owner fixture completed')
    if (-not $record.nativePedReferenceOwnerTested) { throw 'Actual native ped reference callback owner fixture did not complete.' }
    $record['nativePedMutationOwnerTested'] = $text.Contains('Production native referenced ped velocity mutation callback owner fixture completed')
    if (-not $record.nativePedMutationOwnerTested) { throw 'Actual registered-token ped mutation callback owner fixture did not complete.' }
    $record['nativePedVitalsOwnerTested'] = $text.Contains('Production native ped vitals callback owner fixture completed')
    if (-not $record.nativePedVitalsOwnerTested) { throw 'Actual native ped vitals callback owner fixture did not complete.' }
    $record['nativePedPhysicalScalarsOwnerTested'] = $text.Contains('Production native ped physical scalars callback owner fixture completed')
    if (-not $record.nativePedPhysicalScalarsOwnerTested) { throw 'Actual native ped physical scalar callback owner fixture did not complete.' }
    $record['nativePedPhysicalDynamicsOwnerTested'] = $text.Contains('Production native ped physical dynamics callback owner fixture completed')
    if (-not $record.nativePedPhysicalDynamicsOwnerTested) { throw 'Actual native ped physical dynamics callback owner fixture did not complete.' }
    $record['nativeGenericPedVitalsOwnerTested'] = $text.Contains('Production native generic ped vitals callback owner fixture completed')
    if (-not $record.nativeGenericPedVitalsOwnerTested) { throw 'Actual native generic ped vitals callback owner fixture did not complete.' }
    $record['nativePedMetadataOwnerTested'] = $text.Contains('Production native ped metadata callback owner fixture completed')
    if (-not $record.nativePedMetadataOwnerTested) { throw 'Actual native ped metadata callback owner fixture did not complete.' }
    $record['nativePedObservationOwnerTested'] = $text.Contains('Production native ped observation callback owner fixture completed')
    if (-not $record.nativePedObservationOwnerTested) { throw 'Actual native ped observation callback owner fixture did not complete.' }
    $record['nativePedIntelligenceOwnerTested'] = $text.Contains('Production native ped intelligence callback owner fixture completed')
    if (-not $record.nativePedIntelligenceOwnerTested) { throw 'Actual registered-token Intelligence callback owner fixture did not complete.' }
    $record['nativeTaskPoolOwnerTested'] = $text.Contains('Production native task pool callback owner fixture completed')
    if (-not $record.nativeTaskPoolOwnerTested) { throw 'Actual immediate task-pool callback owner fixture did not complete.' }
    $record['nativeTaskSecondaryRemovalOwnerTested'] = $text.Contains('Production native secondary-task removal callback owner fixture completed')
    if (-not $record.nativeTaskSecondaryRemovalOwnerTested) { throw 'Actual retained-token secondary-task deletion callback owner fixture did not complete.' }
    $record['nativeModelBankOwnerTested'] = $text.Contains('Production native model-bank callback owner fixture completed')
    if (-not $record.nativeModelBankOwnerTested) { throw 'Actual immediate model-bank callback owner fixture did not complete.' }
    $record.nativeModelLoadedOwnerTested = $text.Contains('Production native model-loaded callback owner fixture completed')
    $record.nativeStreamingRequestOwnerTested = $text.Contains('Production native streaming request callback owner fixture completed')
    $record.nativeModelStreamingOwnerTested = $record.nativeModelLoadedOwnerTested -and $record.nativeStreamingRequestOwnerTested
    if (-not $record.nativeModelStreamingOwnerTested) { throw 'Actual native streaming request and loaded-state owner fixtures did not complete.' }
    $record.nativeStreamingLoadAllOwnerTested = $text.Contains('Production native streaming load-all callback owner fixture completed')
    $record.nativeStreamingResourcesOwnerTested = $text.Contains('Production native streaming resource callback owner fixture completed')
    $record.nativeModelAcquireOwnerTested = $text.Contains('Production native model acquisition callback owner fixture completed')
    if (-not ($record.nativeStreamingLoadAllOwnerTested -and $record.nativeStreamingResourcesOwnerTested -and $record.nativeModelAcquireOwnerTested)) {
        throw 'Actual native blocking-all, non-DFF resource and missing-key acquisition callback owner fixtures did not all complete.'
    }
    $record.nativeModelClassificationOwnerTested = $text.Contains('Production native model classification callback owner fixture completed')
    $record.nativeModelTypeOwnerTested = $text.Contains('Production native model type callback owner fixture completed')
    $record.nativeStreamingRemovalOwnerTested = $text.Contains('Production native streaming removal callback owner fixture completed')
    $record.nativeStreamingPriorityOwnerTested = $text.Contains('Production native streaming priority callback owner fixture completed')
    if (-not ($record.nativeModelClassificationOwnerTested -and $record.nativeModelTypeOwnerTested -and $record.nativeStreamingRemovalOwnerTested -and $record.nativeStreamingPriorityOwnerTested)) {
        throw 'Native classification/type, selective completion or direct removal did not execute on the actual retained callback owner.'
    }
    $record.nativeStreamingModelGroupsOwnerTested = $text.Contains('Production native streaming groups callback owner fixture completed')
    $record.nativeModelArchiveOwnerTested = $text.Contains('Production native model archive callback owner fixture completed')
    if (-not ($record.nativeStreamingModelGroupsOwnerTested -and $record.nativeModelArchiveOwnerTested)) {
        throw 'Native groups/archive observations did not execute on the actual retained callback owner.'
    }
    $record.nativeWeaponStatManagerOwnerTested = $text.Contains('Production complete weapon-stat manager callback owner fixture completed')
    if (-not $record.nativeWeaponStatManagerOwnerTested) { throw 'Complete weapon-stat manager did not execute on the actual native callback owner.' }
    $record.nativePedCurrentWeaponSlotOwnerTested = $text.Contains('Production native current weapon slot callback owner fixture completed')
    if (-not $record.nativePedCurrentWeaponSlotOwnerTested) { throw 'Current weapon slot read did not finish through the real selected-ped callback.' }
    $record.nativeNamedGarageOwnerTested = $text.Contains('Production native named garage callback owner fixture completed')
    $record.actualNamedGarageSdkOwnerTested = $text.Contains('Actual named garage SDK callback owner fixture completed')
    $record.nativeNamedGarageGeometryOwnerTested = $text.Contains('Production native named garage geometry callback owner fixture completed')
    if (-not ($record.nativeNamedGarageOwnerTested -and $record.actualNamedGarageSdkOwnerTested -and $record.nativeNamedGarageGeometryOwnerTested)) { throw 'Named garage SDK/callback owner did not complete.' }

    $record.nativePedWeaponInstanceOwnerTested = $text.Contains('Production native ped weapon instance callback owner fixture completed')
    if (-not $record.nativePedWeaponInstanceOwnerTested) { throw 'Selected ped weapon scalars and actual bank lookup did not execute through the registered token callback owner.' }
    $record.nativePedFightingStyleOwnerTested = $text.Contains('Production native fighting-style callback owner fixture completed')
    if (-not $record.nativePedFightingStyleOwnerTested) { throw 'Fighting style did not execute on the actual registered-token callback owner.' }
    $record.nativePedBleedingOwnerTested = $text.Contains('Production native bleeding callback owner fixture completed')
    $record.nativePedStayOwnerTested = $text.Contains('Production native stay callback owner fixture completed')
    if (-not ($record.nativePedBleedingOwnerTested -and $record.nativePedStayOwnerTested)) {
        throw 'Named bleeding/stay flags did not execute on the actual registered callback owner.'
    }
    $record.nativePedBikeFlagsOwnerTested = $text.Contains('Production native bike flags callback owner fixture completed')
    $record.nativePedShotFlagsOwnerTested = $text.Contains('Production native shot flags callback owner fixture completed')
    if (-not ($record.nativePedBikeFlagsOwnerTested -and $record.nativePedShotFlagsOwnerTested)) {
        throw 'Masked native ped bike/shot flags did not execute on the actual registered callback owner.'
    }
    $record.nativePedOxygenOwnerTested = $text.Contains('Production native oxygen callback owner fixture completed')
    $record.nativePedDuckingOwnerTested = $text.Contains('Production native ducking callback owner fixture completed')
    if (-not ($record.nativePedOxygenOwnerTested -and $record.nativePedDuckingOwnerTested)) {
        throw 'Selected-ped oxygen or named ducking fixture did not execute on the actual native owner.'
    }
    $record.nativePedCurrentRotationOwnerTested = $text.Contains('Production native current ped rotation callback owner fixture completed')
    $record.nativeStreamingSceneOwnerTested = $text.Contains('Production native scene callback owner fixture completed')
    $record.nativeStreamingSharedReentryOwnerTested = $text.Contains('Production native streaming shared reentry callback owner fixture completed')
    if (-not ($record.nativePedCurrentRotationOwnerTested -and $record.nativeStreamingSceneOwnerTested -and $record.nativeStreamingSharedReentryOwnerTested)) {
        throw 'Current rotation, genuine scene or shared reentry owner fixture did not execute.'
    }
    $record.nativeModelFinalReleaseOwnerTested = $text.Contains('Production native final model-reference callback owner fixture completed')
    if (-not $record.nativeModelFinalReleaseOwnerTested) { throw 'Explicit final-reference policy did not execute on the actual native callback owner.' }
    $record.nativeModelReferenceOwnerTested = $text.Contains('Production native model reference callback owner fixture completed')
    $record.nativeStreamingUpgradeOwnerTested = $text.Contains('Production native streaming upgrade callback owner fixture completed')
    $record.nativeModelOwnershipOwnerTested = $record.nativeModelReferenceOwnerTested -and $record.nativeStreamingUpgradeOwnerTested
    if (-not $record.nativeModelOwnershipOwnerTested) { throw 'Actual native reference CAS and paired-upgrade callback owner fixture did not complete.' }
    $record.nativeRainOwnerTested = $text.Contains('Production native complete Rain weather SDK owner fixture completed')
    if (-not $record.nativeRainOwnerTested) { throw 'Actual complete Rain weather SDK owner fixture did not complete.' }
    $record.nativeStartupCompletionOwnerTested = $text.Contains('Production native asynchronous startup completion owner fixture completed')
    if (-not $record.nativeStartupCompletionOwnerTested) { throw 'Actual native asynchronous startup completion owner fixture did not complete.' }
    $record.nativeApplicationFrameObserverTested = $text.Contains('Production natural application-frame observer fixture completed')
    if (-not $record.nativeApplicationFrameObserverTested) { throw 'Actual application-frame observer fixture did not complete.' }
    }
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
