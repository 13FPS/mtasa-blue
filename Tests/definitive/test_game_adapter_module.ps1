#!/usr/bin/env pwsh
#requires -Version 7.0
# Build the actual x64 game-module DLL and exercise its exported factory. These
# checks validate MTA ownership/storage without loading a game or rebuilding CLEO.
[CmdletBinding()]
param(
    [string]$GameDeDll,
    [string]$ExpectedGameDeSha256,
    [switch]$FrontendAdmissionOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$reuseModule = [bool]$GameDeDll
if ($reuseModule -ne [bool]$ExpectedGameDeSha256) { throw 'GameDeDll and ExpectedGameDeSha256 must be supplied together.' }
if ($FrontendAdmissionOnly -and -not $reuseModule) { throw 'The focused admission checks reuse a pinned normal-built backend.' }
if ($reuseModule) {
    $GameDeDll = (Resolve-Path -LiteralPath $GameDeDll).Path
    if ($ExpectedGameDeSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        (Get-FileHash -LiteralPath $GameDeDll -Algorithm SHA256).Hash -ne $ExpectedGameDeSha256) {
        throw 'The supplied normal backend differs from its required SHA256.'
    }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
$developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
$outputRoot = Join-Path $taskRoot ('Build/mta-game-de-module/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$sourceNames = @('Client/sdk/game/CGameAdapter.h', 'Client/game_de/GameAdapter.cpp', 'Client/game_de/ControllerSnapshot.h',
    'Client/sdk/multiplayer/CRemoteDataStorage.h', 'Client/sdk/multiplayer/CRemoteDataStorageImpl.h', 'Client/game_de/RemoteDataStorage.h',
    'Client/sdk/game/Common.h', 'Shared/sdk/SharedUtil.Defines.h',
    'Client/sdk/game/CStats.h', 'Client/sdk/game/CWeaponInfo.h', 'Shared/sdk/Common.h',
    'Shared/sdk/SharedUtil.IntTypes.h', 'Shared/sdk/CVector.h', 'Shared/sdk/CVector4D.h',
    'Client/game_de/premake5.lua', 'vendor/detours/premake5.lua',
    'Client/game_de/NativePadReader.h', 'Client/game_de/NativePadReader.cpp',
    'Client/game_de/NativePadUpdateAccess.h', 'Client/game_de/NativePadUpdateControl.h', 'Client/game_de/NativePadUpdateControl.cpp',
    'Client/game_de/NativePadUpdateProfile.h', 'Client/game_de/NativePadUpdateProfile.cpp',
    'Client/game_de/NativePeerThreads.h', 'Client/game_de/NativePeerThreads.cpp',
    'Client/game_de/NativeFunctionRange.h', 'Client/game_de/NativeFunctionRange.cpp',
    'Client/game_de/NativeImageAccess.h', 'Client/game_de/NativeImageAccess.cpp',
    'Client/game_de/CClockDE.h', 'Client/game_de/CClockDE.cpp', 'Client/sdk/game/CClock.h',
    'Client/game_de/CKeyGenDE.h', 'Client/game_de/CKeyGenDE.cpp', 'Client/sdk/game/CKeyGen.h',
    'Shared/sdk/CAsyncLoadingPolicy.h', 'Client/game_de/CAsyncLoadingDE.h', 'Client/game_de/CAsyncLoadingDE.cpp',
    'Client/game_de/CGameDE.h', 'Client/game_de/CGameDE.cpp', 'Client/game_de/CGameTimingDE.h', 'Client/game_de/CGameTimingDE.cpp', 'Client/sdk/game/CGame.h',
    'Client/game_de/CGameWeaponInfoDE.h', 'Client/game_de/CGameWeaponInfoDE.cpp', 'Client/sdk/game/CWeaponInfoIndexPolicy.h',
    'Client/game_de/CWeaponStatFactoryDE.h', 'Client/game_de/CWeaponStatFactoryDE.cpp',
    'Client/game_de/CWeaponStatManagerPolicyDE.h','Client/game_de/CWeaponStatManagerPolicyDE.cpp','Client/sdk/game/CWeaponStatListPolicy.h','Client/sdk/game/CWeaponStatManager.h','Client/game_de/CWeaponStatNativeDE.h', 'Client/game_de/CWeaponStatNativeDE.cpp',
    'Client/game_de/NativeWeaponInfoExtraProfile.h',
    'Client/sdk/game/CWeaponStatData.h', 'Client/sdk/game/CWeaponStatDefaults.h',
    'Client/sdk/game/CWeaponStatFlagPolicy.h', 'Client/sdk/game/CWeaponStatOwned.h', 'Client/sdk/game/CWeaponStat.h',
    'Client/game_de/NativeWeaponInfoProfile.h', 'Client/game_de/NativeWeaponInfoReader.h', 'Client/game_de/NativeWeaponInfoReader.cpp',
    'Client/game_de/CWeaponInfoReadDE.h', 'Client/game_de/CWeaponInfoReadDE.cpp',
    'Client/game_de/NativeWeaponInfoWriteProfile.h', 'Client/game_de/NativeWeaponInfoWriteMemory.h', 'Client/game_de/NativeWeaponInfoWriteMemory.cpp',
    'Client/game_de/NativeWeaponInfoWriteAccess.h', 'Client/game_de/CWeaponInfoWriteDE.h', 'Client/game_de/CWeaponInfoWriteDE.cpp',
    'Client/game_de/NativeWeaponInfoVectorAccess.h', 'Client/game_de/NativeWeaponInfoVectorProfile.h',
    'Client/game_de/NativeWeaponInfoVectorMemory.h', 'Client/game_de/NativeWeaponInfoVectorMemory.cpp',
    'Client/game_de/CWeaponInfoDE.h', 'Client/game_de/CWeaponInfoDE.cpp',
    'Client/game_de/CWeaponInfoBankDE.h', 'Client/game_de/CWeaponInfoBankDE.cpp',
    'Client/game_de/NativeWeatherOverridePlan.h', 'Client/game_de/NativeWeatherOverridePlan.cpp',
    'Client/game_de/NativeWeatherOverrideProfile.cpp',
    'Client/game_de/NativeWeatherOverrideAtomic.h', 'Client/game_de/NativeWeatherOverrideAtomic.cpp',
    'Client/game_de/NativeWeatherOverrideOwner.h', 'Client/game_de/NativeWeatherOverrideOwner.cpp',
    'Client/game_de/NativeWeatherOverrideWin32.h', 'Client/game_de/NativeWeatherOverrideWin32.cpp',
    'Client/game_de/NativeWeatherOverridePlacement.h', 'Client/game_de/NativeWeatherOverridePlacement.cpp',
    'Client/game_de/NativeWeatherOverrideSlot.h', 'Client/game_de/NativeWeatherOverrideSlot.cpp',
    'Client/game_de/NativeWeatherRainLeaf.h','Client/game_de/NativeWeatherRainLeaf.cpp','Client/game_de/NativeWeatherRainProfile.h','Client/game_de/NativeWeatherRainBridge.h','Client/game_de/NativeWeatherRainBridge.cpp','Client/game_de/NativeWeatherRainBorrowGuard.h','Client/game_de/NativeWeatherRainBorrowGuard.cpp','Client/game_de/NativeWeatherOverrideAccess.h', 'Client/game_de/CWeatherOverridesDE.h', 'Client/game_de/CWeatherOverridesDE.cpp',
    'Client/game_de/NativeStatsProfile.h', 'Client/game_de/NativeStatsReader.h', 'Client/game_de/NativeStatsReader.cpp',
    'Client/game_de/CStatsReadDE.h', 'Client/game_de/CStatsReadDE.cpp',
    'Client/game_de/NativeStatsWriteAccess.h', 'Client/game_de/NativeStatsWriteProfile.h',
    'Client/game_de/NativeStatsWriteCalls.h', 'Client/game_de/NativeStatsWriteCalls.cpp', 'Client/game_de/CStatsDE.h', 'Client/game_de/CStatsDE.cpp',
    'Client/game_de/NativeGameTimingAccess.h','Client/game_de/NativeWeaponConstructorProgress.h','Client/sdk/game/CGameConstructorPreparation.h', 'Client/game_de/NativeGravityReadAccess.h','Client/game_de/NativeGravityReadProfile.h','Client/game_de/NativeGravityReadAccess.cpp','Client/game_de/NativeGameTimingReader.h', 'Client/game_de/NativeGameTimingReader.cpp',
    'Client/game_de/NativeGameTimingProfile.h', 'Client/game_de/NativeGameFPSProfile.h', 'Client/game_de/NativeGamePauseProfile.h',
    'Client/game_de/NativeStreamingFileIdsProfile.h', 'Client/game_de/NativeStreamingFileIdsReader.h', 'Client/game_de/NativeStreamingFileIdsReader.cpp',
    'Client/game_de/NativeGameStateProfile.h', 'Client/game_de/NativeFrontendMenuProfile.h', 'Client/game_de/NativeGameStateReader.h', 'Client/game_de/NativeGameStateReader.cpp',
    'Client/game_de/NativeGameStateMemory.h', 'Client/game_de/NativeGameStateMemory.cpp',
    'Client/game_de/NativeDataFileNamesReader.h', 'Client/game_de/NativeDataFileNamesReader.cpp', 'Client/game_de/NativeDataFileNamesProfile.h',
    'Client/game_de/NativeCheatProfile.h', 'Client/game_de/NativeCheatProfile.cpp', 'Client/game_de/NativeCheatFlagAccess.h',
    'Client/game_de/CWeatherTransitionDE.h', 'Client/game_de/CWeatherTransitionDE.cpp', 'Client/sdk/game/CWeather.h',
    'Client/game_de/NativeWeatherTransitionProfile.h', 'Client/game_de/NativeWeatherTransitionProfile.cpp',
    'Client/game_de/NativeWeatherTransitionMemory.h', 'Client/game_de/NativeWeatherTransitionJournal.h',
    'Client/game_de/NativeWeatherScalarProfile.h', 'Client/game_de/NativeWeatherScalarProfile.cpp', 'Client/game_de/NativeWeatherScalarMemory.h',
    'Client/game_de/CWeatherScalarsDE.h', 'Client/game_de/CWeatherScalarsDE.cpp',
    'Shared/sdk/CJetpackWeaponPolicy.h',
    'Client/sdk/game/CGameClockService.h', 'Client/sdk/game/CGameApplicationFrameObservation.h', 'Client/sdk/game/CGameStartupObservation.h',
    'Client/sdk/game/CGameLocalInputLease.h', 'Client/game_de/LocalInputLeasePolicy.h',
    'Client/game_de/NativeClockService.h', 'Client/game_de/NativeClockService.cpp',
    'Client/game_de/NativeStartGameCalls.h', 'Client/game_de/NativeStartGameCalls.cpp',
    'Client/game_de/NativeStartGameProfile.h', 'Client/game_de/NativeBorrowedGcProfile.h',
    'Client/game_de/NativeUObjectMembershipAccess.h', 'Client/game_de/NativeUObjectMembershipProfile.h',
    'Client/game_de/NativeStartupCompletionAccess.h', 'Client/game_de/NativeStartupCompletionProfile.h',
    'Client/game_de/NativeApplicationFrame.h', 'Client/game_de/NativeApplicationFrameProfile.h',
    'Client/game_de/NativeUiClassMapAccess.h', 'Client/game_de/NativeUiClassMapProfile.h',
    'Client/game_de/NativeClassChainAccess.h', 'Client/game_de/NativeStartupWorldProfile.h', 'Client/game_de/NativeStartupWorldAccess.h',
    'Client/game_de/NativeStartupProvidersAccess.h','Client/game_de/NativeStartupProvidersProfile.h',
    'Client/sdk/game/CGameTimeService.h', 'Client/sdk/game/CGameFrontendAdmission.h', 'Client/game_de/NativeGameTimeReader.h', 'Client/game_de/NativeGameTimeReader.cpp',
    'Client/game_de/NativeClockProfile.h', 'Client/game_de/NativeClockMemory.h', 'Client/game_de/NativeServiceAdmission.h',
    'Client/game_de/NativeMovementInput.h', 'Client/game_de/NativeMovementInput.cpp', 'Client/game_de/NativeInputRouting.h', 'Client/game_de/NativePadStateAccess.h',
    'Client/sdk/game/CGameRenderObservation.h', 'Client/sdk/game/CGameRenderEndpointDiagnostic.h', 'Client/sdk/game/CGameRenderSetupDiagnostic.h', 'Client/sdk/game/CGameRenderRootsDiagnostic.h', 'Client/sdk/game/CGameRenderDeviceLease.h', 'Client/sdk/game/CGameRenderDeviceWindowLease.h',
    'Client/game_de/NativeRenderObservation.h', 'Client/game_de/NativeRenderObservation.cpp',
    'Client/game_de/NativeD3D12OwnershipProbe.h', 'Client/game_de/NativeD3D12OwnershipProbe.cpp',
    'Client/game_de/NativeCustomPresentProbe.h', 'Client/game_de/NativeCustomPresentProbe.cpp',
    'Client/game_de/NativeFrameBoundaryProbe.h', 'Client/game_de/NativeFrameBoundaryProbe.cpp',
    'Client/definitive/MtaControllerSnapshot.h', 'Shared/sdk/CControllerState.h', 'Shared/sdk/enums/PedState.h',
    'Shared/sdk/CControllerActions.h', 'Shared/sdk/CControlStateSynthesis.h',
    'Client/game_de/NativePedPoolProfile.h','Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolReader.cpp',
    'Client/game_de/NativePedTransformProfile.h','Client/game_de/NativePedTransformReader.h','Client/game_de/NativePedTransformReader.cpp',
    'Client/game_de/NativeAnimationManagerAccess.h','Client/game_de/NativeAnimationManagerMemory.h','Client/game_de/NativeAnimationManagerMemory.cpp','Client/game_de/NativeAnimationManagerProfile.h','Client/game_de/CAnimManagerDE.h','Client/game_de/CAnimManagerDE.cpp','Client/sdk/game/CAnimGatewayNames.h','Client/sdk/game/CAnimManager.h',
        'Client/game_de/NativePedVisibilityAccess.h','Client/game_de/NativePedVisibilityMemory.h','Client/game_de/NativePedVisibilityMemory.cpp','Client/game_de/NativePedVisibilityProfile.h','Client/game_de/CPedVisibilityDE.h','Client/game_de/CPedVisibilityDE.cpp',
        'Client/game_de/NativePedGogglesAccess.h','Client/game_de/NativePedGogglesMemory.h','Client/game_de/NativePedGogglesMemory.cpp','Client/game_de/NativePedGogglesProfile.h','Client/game_de/CPedGogglesDE.h','Client/game_de/CPedGogglesDE.cpp',
        'Client/game_de/CPedTasksDE.h','Client/game_de/CPedTasksDE.cpp','Client/sdk/game/CPedTaskPolicy.h','Client/sdk/game/CPedIntelligence.h','Client/sdk/game/CTaskManager.h','Client/sdk/game/Task.h','Client/sdk/game/TaskTypes.h',
        'Client/sdk/game/CEntityFirePolicy.h','Client/game_de/NativePedFireAccess.h','Client/game_de/NativePedFireMemory.h','Client/game_de/NativePedFireMemory.cpp','Client/game_de/NativePedFireProfile.h','Client/game_de/CPedFireDE.h','Client/game_de/CPedFireDE.cpp','Client/game_de/NativePedCollisionAccess.h','Client/game_de/NativePedCollisionMemory.h','Client/game_de/NativePedCollisionMemory.cpp','Client/game_de/NativePedCollisionProfile.h','Client/game_de/CPedCollisionDE.h','Client/game_de/CPedCollisionDE.cpp',
        'Client/game_de/NativePedAreaAccess.h','Client/game_de/NativePedAreaMemory.h','Client/game_de/NativePedAreaMemory.cpp','Client/game_de/NativePedAreaProfile.h','Client/game_de/CPedAreaDE.h','Client/game_de/CPedAreaDE.cpp','Client/game_de/NativeStreamingSpecialModelCalls.h','Client/game_de/NativeStreamingSpecialModelCalls.cpp','Client/game_de/NativeStreamingSpecialModelProfile.h','Client/game_de/NativeStreamingFileIdsProfile.h','Client/game_de/NativeStreamingRemoveAdmission.h','Client/game_de/NativeStreamingRemoveMemory.h','Client/game_de/NativeModelReferenceHeap.h',
        'Client/game_de/NativeAnimationBlockAccess.h','Client/game_de/NativeAnimationBlockMemory.h','Client/game_de/NativeAnimationBlockMemory.cpp','Client/game_de/NativeAnimationBlockProfile.h','Client/game_de/CAnimBlockDE.h','Client/game_de/CAnimBlockDE.cpp','Client/sdk/game/CAnimBlock.h','Client/game_de/CModelInfoRequestDE.h','Client/game_de/CModelInfoRequestDE.cpp','Client/sdk/game/CModelRequestPolicy.h','Client/game_de/CAnimBlockRequestDE.h','Client/game_de/CAnimBlockRequestDE.cpp','Client/sdk/game/CAnimBlockRequestPolicy.h',
        'Client/game_de/NativePedOrientationAccess.h','Client/game_de/NativePedOrientationMemory.h','Client/game_de/NativePedOrientationMemory.cpp','Client/game_de/NativePedOrientationProfile.h','Client/game_de/NativePedPoseContext.h','Client/game_de/NativePedPoseContext.cpp','Client/game_de/CPedOrientationDE.h','Client/game_de/CPedOrientationDE.cpp','Client/sdk/game/CGameModelPreparationPolicy.h','Client/game_de/NativePedWaterTaskAccess.h','Client/game_de/NativePedWaterTaskReader.h','Client/game_de/NativePedWaterTaskReader.cpp','Client/game_de/NativePedWaterTaskProfile.h','Client/game_de/NativePedWaterFlagsAccess.h','Client/game_de/NativePedWaterFlagsMemory.h','Client/game_de/NativePedWaterFlagsMemory.cpp','Client/game_de/NativePedWaterFlagsProfile.h','Client/game_de/CPedWeaponStatsDE.h','Client/game_de/CPedWeaponStatsDE.cpp','Client/sdk/game/CPedWeaponStatPolicy.h','Client/game_de/CPedWaterDE.h','Client/game_de/CPedWaterDE.cpp','Client/game_de/CGameInitializationDE.h','Client/game_de/CGameInitializationDE.cpp','Client/sdk/game/CGameInitializationPolicy.h','Client/game_de/NativePedPlacementAccess.h','Client/game_de/NativePedPlacementMemory.h','Client/game_de/NativePedPlacementMemory.cpp','Client/game_de/NativePedPoseMutation.h','Client/game_de/CPedPoseDE.h','Client/game_de/CPedPoseDE.cpp','Client/sdk/game/CEntityPlacementPolicy.h','Client/game_de/NativePedTeleportAccess.h','Client/game_de/NativePedTeleportProfile.h','Client/game_de/NativePedTeleportMemory.h','Client/game_de/NativePedTeleportMemory.cpp',
    'Client/game_de/NativePedVelocityAccess.h','Client/game_de/NativePedVelocityProfile.h','Client/game_de/NativePedVelocityMemory.h','Client/game_de/NativePedVelocityMemory.cpp',
    'Client/game_de/NativePedReferenceAccess.h','Client/game_de/NativePedReferenceProfile.h','Client/game_de/NativePedReferenceMemory.h','Client/game_de/NativePedReferenceMemory.cpp',
    'Client/game_de/NativePedReferenceToken.h','Client/game_de/NativePedReferenceController.h','Client/game_de/NativePedReferenceController.cpp',
    'Client/game_de/NativePedVitalsAccess.h','Client/game_de/NativePedVitalsProfile.h','Client/game_de/NativePedVitalsMemory.h','Client/game_de/NativePedVitalsMemory.cpp',
    'Client/game_de/CPedVitalsDE.h','Client/game_de/CPedVitalsDE.cpp','Client/sdk/game/CPed.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CEntity.h',
    'Client/game_de/NativePedMetadataAccess.h','Client/game_de/NativePedMetadataProfile.h','Client/game_de/NativePedMetadataMemory.h','Client/game_de/NativePedMetadataMemory.cpp',
    'Client/game_de/CPedMetadataDE.h','Client/game_de/CPedMetadataDE.cpp',
    'Client/game_de/NativePedPhysicalScalarsAccess.h','Client/game_de/NativePedPhysicalScalarsMemory.h','Client/game_de/NativePedPhysicalScalarsMemory.cpp','Client/game_de/NativePedPhysicalScalarsProfile.h',
    'Client/game_de/NativePedIntelligenceReader.h','Client/game_de/NativePedIntelligenceReader.cpp','Client/game_de/NativePedIntelligenceProfile.h',
    'Client/game_de/NativeTaskPoolReader.h','Client/game_de/NativeTaskPoolReader.cpp','Client/game_de/NativeTaskPoolProfile.h',
    'Client/game_de/NativeTaskRemovalAccess.h','Client/game_de/NativeTaskRemovalCalls.h','Client/game_de/NativeTaskRemovalCalls.cpp','Client/game_de/NativeTaskRemovalProfile.h','Client/game_de/CTaskManagerDE.h','Client/game_de/CTaskManagerDE.cpp',
    'Client/game_de/NativeModelBankReader.h','Client/game_de/NativeModelBankReader.cpp','Client/game_de/NativeModelBankProfile.h',
    'Client/game_de/NativeModelLoadedReader.h','Client/game_de/NativeModelLoadedReader.cpp','Client/game_de/NativeModelLoadedProfile.h',
    'Client/game_de/NativeStreamingRequestCalls.h','Client/game_de/NativeStreamingRequestCalls.cpp','Client/game_de/NativeStreamingRequestProfile.h',
    'Client/game_de/NativeStreamingModelContext.h','Client/game_de/NativeStreamingModelContext.cpp','Client/game_de/NativeModelReferenceMemory.h','Client/game_de/NativeModelReferenceMemory.cpp','Client/game_de/NativeModelReferenceProfile.h','Client/game_de/NativeStreamingUpgradeReader.h','Client/game_de/NativeStreamingUpgradeReader.cpp','Client/game_de/NativeStreamingUpgradeProfile.h','Client/game_de/NativeStreamingUpgradeCalls.h','Client/game_de/NativeStreamingUpgradeCalls.cpp',
        'Client/game_de/NativeStreamingModelAccess.h','Client/game_de/CStreamingDE.h','Client/game_de/CStreamingDE.cpp','Client/sdk/game/CStreaming.h','Client/sdk/game/CModelRequestPolicy.h',
    'Build/definitive-recovery/mta-native-model-loaded-proof-20261022/model-loaded-qualified.json',
    'Build/definitive-recovery/mta-native-streaming-request-proof-20261022/native-proof/streaming-request-qualified.json',
    'Client/game_de/NativePedMutationAccess.h','Client/game_de/NativePedObservationAccess.h','Client/game_de/CPedMutationDE.h','Client/game_de/CPedMutationDE.cpp',
    'Client/sdk/game/CEntityPhysicsState.h','Client/game_de/CPedPhysicsDE.h','Client/game_de/CPedPhysicsDE.cpp',
    'Client/game_de/NativePlayerInfoProfile.h','Client/game_de/NativePlayerInfoExtraProfile.h','Client/game_de/NativePlayerInfoAccess.h','Client/game_de/NativePlayerInfoMemory.h','Client/game_de/NativePlayerInfoMemory.cpp',
    'Client/game_de/CPlayerInfoDE.h','Client/game_de/CPlayerInfoDE.cpp','Client/sdk/game/CPlayerInfo.h',
    'Client/game_de/NativeWantedProfile.h','Client/game_de/NativeWantedAccess.h','Client/game_de/NativeWantedMemory.h','Client/game_de/NativeWantedMemory.cpp',
    'Client/game_de/CWantedDE.h','Client/game_de/CWantedDE.cpp','Client/sdk/game/CWanted.h',
    'Client/game_de/CRopesDE.h','Client/game_de/CRopesDE.cpp','Client/sdk/game/CRopes.h',
    'Client/game_de/NativeRopesAccess.h','Client/game_de/NativeRopesProfile.h','Client/game_de/NativeRopesMemory.h','Client/game_de/NativeRopesMemory.cpp',
    'Client/sdk/game/CPools.h','Client/sdk/game/CPoolClientEntity.h','Client/sdk/game/CPedRegistryPolicy.h','Client/sdk/game/CPoolSlotPolicy.h',
    'Tests/definitive/GameAdapterModuleTests.cpp', 'Tests/definitive/test_game_adapter_module.ps1')
$detoursSourceRoot = Join-Path $taskRoot 'vendor/detours/4.0.1/src'
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
    'Client/game_de/CModelInfoDE.h',
    'Client/game_de/CModelInfoDE.cpp',
    'Client/sdk/game/CModelInfo.h'
)
$sourceNames += @(
    'Client/game_de/NativeStreamingModelGroupsReader.h',
    'Client/game_de/NativeStreamingModelGroupsReader.cpp',
    'Client/game_de/NativeStreamingModelGroupsProfile.h',
    'Client/game_de/NativeModelArchiveReader.h',
    'Client/game_de/NativeModelArchiveReader.cpp',
    'Client/game_de/NativeModelArchiveProfile.h'
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
    'Client/sdk/game/CWeaponStatData.h'
)
$sourceNames += @(
    'Client/game_de/NativeModelReferenceRelease.h',
    'Client/game_de/NativeModelReferenceRelease.cpp',
    'Client/game_de/NativeStreamingRemoveAdmission.h'
)
$sourceNames += @(
    'Client/game_de/NativePedRotationAccess.h',
    'Client/game_de/NativePedRotationMemory.h',
    'Client/game_de/NativePedRotationMemory.cpp',
    'Client/game_de/NativePedRotationProfile.h',
    'Client/game_de/NativeStreamingSceneCalls.h',
    'Client/game_de/NativeStreamingSceneCalls.cpp',
    'Client/game_de/NativeStreamingSceneProfile.h',
    'Client/game_de/CPedRotationDE.h',
    'Client/game_de/CPedRotationDE.cpp'
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
    'Client/game_de/CPedStateDE.h',
    'Client/game_de/CPedStateDE.cpp'
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
    'Client/game_de/CPedFlagsDE.h',
    'Client/game_de/CPedFlagsDE.cpp'
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
    'Client/game_de/CPedBehaviorDE.h',
    'Client/game_de/CPedBehaviorDE.cpp'
)
$sourceNames += @(
    'Client/game_de/NativePedFightingStyleAccess.h',
    'Client/game_de/NativePedFightingStyleMemory.h',
    'Client/game_de/NativePedFightingStyleMemory.cpp',
    'Client/game_de/NativePedFightingStyleProfile.h',
    'Client/sdk/game/CPedFightingStylePolicy.h',
    'Build/definitive-recovery/mta-native-ped-fighting-style-20261023/fighting-style-qualified.json',
    'Client/game_de/CPedCombatDE.h',
    'Client/game_de/CPedCombatDE.cpp'
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
    'Build/definitive-recovery/mta-weapon-info-lookup-reuse-20261023/weapon-info-lookup-qualified-v2.json'
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
    'Client/game_de/NativeNamedGarageGeometryProfile.h',
    'Client/game_de/CPoolsDE.h',
    'Client/game_de/CPoolsDE.cpp',
    'Client/sdk/game/CPedIK.h',
    'Build/definitive-recovery/mta-native-garage-geometry-proof-20261023/garage-geometry-proof-ready.json',
    'Build/definitive-recovery/mta-native-named-garage-geometry-20261023/named-garage-geometry-qualified.json',
    'Build/definitive-recovery/mta-pools-ped-registry-de-20261023/pools-ped-registry-source-ready.json'
)
$sourceNames += @(Get-ChildItem -LiteralPath $detoursSourceRoot -File | ForEach-Object {
    [IO.Path]::GetRelativePath($taskRoot, $_.FullName).Replace('\', '/')
})
$sourceNames += @('Client/game_de/NativeGameConstructorServices.h','Client/game_de/CGameDE.h','Client/game_de/CGameDE.cpp','Client/game_de/CGameWeaponInfoDE.h','Client/game_de/CGameWeaponInfoDE.cpp','Client/game_de/CWeaponStatFactoryDE.h','Client/game_de/CWeaponStatFactoryDE.cpp','Client/game_de/CKeyGenDE.h','Client/game_de/CKeyGenDE.cpp','Client/game_de/CAsyncLoadingDE.h','Client/game_de/CAsyncLoadingDE.cpp','Client/game_de/NativeDataFileNamesReader.h','Client/game_de/NativeDataFileNamesReader.cpp','Client/game_de/NativeDataFileNamesProfile.h','Client/game_de/NativeStreamingFileIdsReader.h','Client/game_de/NativeStreamingFileIdsReader.cpp','Client/game_de/NativeGameStateReader.h','Client/game_de/NativeGameStateReader.cpp','Client/game_de/CGameResetDE.h','Client/game_de/CGameResetDE.cpp','Client/sdk/game/CGameResetPolicy.h')
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
$sourceNames += @('Client/game_de/WeaponFireHandlerAccess.h','Client/game_de/CWeaponFireBulletDE.h','Client/game_de/CWeaponFireBulletDE.cpp','Client/sdk/game/CWeaponFireBulletPolicy.h')
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
$common = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
    ('/I' + (Join-Path $taskRoot 'Shared/sdk')), ('/I' + (Join-Path $taskRoot 'Client/sdk')))
$moduleExit = $null
$detoursCompilerExit = $null
$detoursLibraryExit = $null
$detoursDirectory = Join-Path $outputRoot 'detours'
$detoursLibrary = Join-Path $detoursDirectory 'detours.lib'
if ($reuseModule) {
    Copy-Item -LiteralPath $GameDeDll -Destination $module
    # Reconstruct only the import library from the pinned DLL's real exports.
    # The normal backend is not recompiled for this focused consumer check.
    $dumpbin = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/dumpbin.exe'
    $exports = @(& $dumpbin /nologo /exports $module 2>&1)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the supplied normal backend exports.' }
    $exports | Set-Content -LiteralPath (Join-Path $outputRoot 'module-exports.log') -Encoding utf8
    # Normal MSVC DLLs can print an undecorated alias after the export name.
    # Accept that presentation without treating the alias as another export.
    $names = @($exports | ForEach-Object { if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)(?:\s+=\s+\S+)?\s*$') { $Matches[1] } })
    if ('CreateGameAdapter' -notin $names -or 'ReadGameFrontendAdmissionV1' -notin $names) {
        throw 'The supplied backend lacks the actual factory or admission export.'
    }
    $definition = Join-Path $outputRoot 'game_de.def'
    @('LIBRARY game_de.dll','EXPORTS') + $names | Set-Content -LiteralPath $definition -Encoding ascii
    $librarian = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/lib.exe'
    $importOutput = @(& $librarian /nologo /machine:x64 ('/def:' + $definition) ('/out:' + $library) 2>&1)
    $moduleExit = $LASTEXITCODE
    $importOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'import-library.log') -Encoding utf8
}
else {
New-Item -ItemType Directory -Path $detoursDirectory | Out-Null
# Compile the repository's actual vendor sources separately, so vendor warning
# policy never weakens /W4 /WX for the adapter that owns the native lifetime.
$detoursSources = @('creatwth.cpp', 'detours.cpp', 'image.cpp', 'modules.cpp', 'disolx86.cpp', 'disolx64.cpp', 'disasm.cpp') |
    ForEach-Object { Join-Path $detoursSourceRoot $_ }
$detoursArguments = @('/nologo','/c','/EHsc','/O2','/MD','/W0','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
    ('/Fo' + $detoursDirectory + [IO.Path]::DirectorySeparatorChar)) + $detoursSources
$detoursOutput = @(& $compiler @detoursArguments 2>&1)
$detoursCompilerExit = $LASTEXITCODE
$detoursOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'detours-compile.log') -Encoding utf8
$detoursLibraryExit = $null
if ($detoursCompilerExit -eq 0) {
    $librarian = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/lib.exe'
    $detoursObjects = @(Get-ChildItem -LiteralPath $detoursDirectory -Filter '*.obj' -File | ForEach-Object FullName)
    $libraryOutput = @(& $librarian /nologo ('/OUT:' + $detoursLibrary) @detoursObjects 2>&1)
    $detoursLibraryExit = $LASTEXITCODE
    $libraryOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'detours-library.log') -Encoding utf8
}
$moduleSource = Join-Path $taskRoot 'Client/game_de/GameAdapter.cpp'
if ((Test-Path -LiteralPath $moduleSource) -and $detoursLibraryExit -eq 0) {
    $moduleArguments = $common + @('/LD','/DMTA_GAME_DE_EXPORTS', $moduleSource, (Join-Path $taskRoot 'Client/game_de/NativePadReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeImageAccess.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedPoolReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedTransformReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedPlacementMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedPoseContext.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedOrientationMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedAreaMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedFireMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedCollisionMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingSpecialModelCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedWaterTaskReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeAnimationBlockMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeAnimationManagerMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedWaterFlagsMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedTeleportMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedVelocityMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedReferenceMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedReferenceController.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedVitalsMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedVitalsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedMetadataMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedMetadataDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedMutationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedPhysicsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedRotationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedStateDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedFlagsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedBehaviorDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedBleedingMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedStayMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedFightingStyleMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPedCombatDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedWeaponContext.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedWeaponFieldsMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedWeaponAmmoMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponNativeDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponFireBulletDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedCurrentWeaponSlotMemory.cpp'),
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
        (Join-Path $taskRoot 'Client/game_de/CWorldGroundDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldLineDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldAreaDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldJetpackHeightDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWorldIgnoreEntityDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CCameraSphereVisibleDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameInitializationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameResetDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameWorldDE.cpp'),
        (Join-Path $taskRoot 'Client/sdk/game/CAudioContainerFiles.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlockDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimManagerDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlockRequestDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CModelInfoRequestDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponPolicyDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeNamedGarageMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGaragesDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGarageNamedDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CPoolsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedBikeFlagsMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedShotFlagsMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedOxygenMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedDuckingMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedGogglesMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedVisibilityMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedOnScreenCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedWeaponGrantCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedWeaponClearCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedRotationMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingSceneCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWorldGroundContext.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWorldGroundCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWorldLineCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWorldAreaMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWorldJetpackMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWorldIgnoredEntityMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeCameraSphereVisibleCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedPhysicalScalarsMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedIntelligenceReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeTaskPoolReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeTaskRemovalCalls.cpp'),(Join-Path $taskRoot 'Client/game_de/CTaskManagerDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelBankReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelLoadedReader.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeStreamingRequestCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CStreamingDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingModelContext.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelReferenceMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelReferenceRelease.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingUpgradeReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingUpgradeCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingLoadAllCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingCompletionContext.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingModelGroupsReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelArchiveReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingPriorityCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingRemoveCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelClassificationCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CModelInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingResourceContext.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingResourceCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelReferenceTransactions.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeRopesMemory.cpp'),(Join-Path $taskRoot 'Client/game_de/CRopesDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePlayerInfoMemory.cpp'),(Join-Path $taskRoot 'Client/game_de/CPlayerInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWantedMemory.cpp'),(Join-Path $taskRoot 'Client/game_de/CWantedDE.cpp'), (Join-Path $taskRoot 'Client/game_de/CClockDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CKeyGenDE.cpp'), (Join-Path $taskRoot 'Client/game_de/CAsyncLoadingDE.cpp'), (Join-Path $taskRoot 'Client/game_de/CGameDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameWeaponInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponStatFactoryDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponStatNativeDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CWeaponStatManagerPolicyDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponStatManagerDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeaponInfoReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponInfoReadDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeaponInfoWriteMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponInfoWriteDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeaponInfoVectorMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeaponInfoBankDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlan.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideAtomic.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideOwner.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBridge.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBorrowGuard.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainLeaf.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideWin32.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlacement.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideSlot.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeatherOverridesDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStatsReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CStatsReadDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStatsWriteCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CStatsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CGameTimingDE.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeGravityReadAccess.cpp'), (Join-Path $taskRoot 'Client/game_de/NativeGameTimingReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingFileIdsReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeGameStateReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeGameStateMemory.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeDataFileNamesReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeCheatProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherTransitionProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeatherTransitionDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherScalarProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CWeatherScalarsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePadUpdateControl.cpp'), (Join-Path $taskRoot 'Client/game_de/NativePadUpdateProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePeerThreads.cpp'), (Join-Path $taskRoot 'Client/game_de/NativeFunctionRange.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeClockService.cpp'), (Join-Path $taskRoot 'Client/game_de/NativeStartGameCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeGameTimeReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeMovementInput.cpp'), (Join-Path $taskRoot 'Client/game_de/NativeRenderObservation.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeD3D12OwnershipProbe.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeCustomPresentProbe.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeFrameBoundaryProbe.cpp'), ('/I' + $detoursSourceRoot),
        ('/Fo' + $outputRoot + [IO.Path]::DirectorySeparatorChar), ('/Fe' + $module), '/link', ('/IMPLIB:' + $library), 'bcrypt.lib', 'd3d12.lib', 'dxgi.lib', 'user32.lib', $detoursLibrary)
    $moduleOutput = @(& $compiler @moduleArguments 2>&1)
    $moduleExit = $LASTEXITCODE
    $moduleOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'module-compile.log') -Encoding utf8
}
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
$testOutput = @()
if ($compilerExit -eq 0 -and $moduleExit -eq 0) {
    $testArguments = @()
    if ($FrontendAdmissionOnly) { $testArguments += '--frontend-admission-only' }
    $testOutput = @(& $executable @testArguments 2>&1)
    $testExit = $LASTEXITCODE
    $testOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'test.log') -Encoding utf8
    if (($testOutput -join "`n") -match 'Passed (\d+) MTA Game DE adapter checks') { $testCount = [int]$Matches[1] }
}
$constructorAdapterContractTested = -not $FrontendAdmissionOnly -and
    (($testOutput -join "`n").Contains('Production constructor preparation optional adapter contract fixture completed'))
$constructorAdapterContractRequired = [bool]$FrontendAdmissionOnly -or $constructorAdapterContractTested
$sourceUnchanged = $true
foreach ($entry in $sourceHashes) {
    $taskPath = Join-Path $taskRoot $entry.path
    if ((Test-Path -LiteralPath $taskPath) -ne $entry.exists -or
        ($entry.exists -and (Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256)) {
        $sourceUnchanged = $false
    }
}
$normalBackendUnchanged = -not $reuseModule -or
    ((Get-FileHash -LiteralPath $GameDeDll -Algorithm SHA256).Hash -eq $ExpectedGameDeSha256 -and
     (Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash -eq $ExpectedGameDeSha256)
$record = [ordered]@{ recordedAt=[DateTimeOffset]::Now.ToString('o'); directory=$outputRoot;
    success=($moduleExit -eq 0 -and $compilerExit -eq 0 -and $testExit -eq 0 -and $null -ne $testCount -and $sourceUnchanged -and $normalBackendUnchanged -and $constructorAdapterContractRequired);
    sourceHashes=$sourceHashes; sourceUnchanged=$sourceUnchanged; architecture='x64'; moduleCompilerExit=$(if ($reuseModule) { $null } else { $moduleExit });
    normalBackendReused=$reuseModule; normalBackendSource=$(if ($reuseModule) { $GameDeDll } else { $null });
    expectedNormalBackendSha256=$(if ($reuseModule) { $ExpectedGameDeSha256.ToLowerInvariant() } else { $null });
    normalBackendUnchanged=$normalBackendUnchanged;
    frontendAdmissionOnly=[bool]$FrontendAdmissionOnly;
    actualConstructorPreparationAdapterContractTested=$constructorAdapterContractTested;
    detoursCompilerExit=$detoursCompilerExit; detoursLibraryExit=$detoursLibraryExit; detoursLibrary=$detoursLibrary;
    detoursLibrarySha256=$(if (Test-Path -LiteralPath $detoursLibrary) { (Get-FileHash -LiteralPath $detoursLibrary -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null });
    compilerExit=$compilerExit; testExit=$testExit; checks=$testCount; module=$module;
    moduleSha256=$(if (Test-Path -LiteralPath $module) { (Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null });
    executable=$executable; gameLaunched=$false; installationChanged=$false; cleoLoaded=$false;
    standaloneTransportBuilt=$false; fullMtaClientBuilt=$false; engineInputBindingVerified=$false }
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
if (-not $record.success) { throw "MTA Game DE adapter checks failed; see $outputRoot" }
$record | ConvertTo-Json -Depth 8 -Compress
