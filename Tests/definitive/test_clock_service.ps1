#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('CameraBasics','ModelTimeGetter','VehiclePredicates','EmptyBlockRemoval','BlockRegistration','BankPostprocess','BankEviction','BankDecompression','ApplicationTiming','AnimationHierarchyUncompress','NumericAssociationOwner','DynamicClumpOwner','AnimationAssociationGroupFactories','GroupSourceCreationOwner','JointTemporaryStaticInitializerOwner','GroupReplacementOwner','GroupCopyAnimationOwner','GroupEmptyInitialization','AnimationAssociationGroupChildProducts','AnimationAssociationGroupOffset','StaticAssociationInitialization','PresentTransitionClock','StartupInitializationReadiness','StartupStreamingReceiver','StartupNumericGarages','StartupPreparationReadiness','StartupNativeMenuLifecycle','StartupModelCollisionLifecycle','PedModelCreationLifecycle','Full','PedOwner','PedCurrentWeapon','GarageNamed','GaragePools','PedPoolRegistry','ConstructorWeapons','PedPose','PedWater','PedWaterOrientation','AnimationBlock','AnimationBlockName','AnimationManager','AnimationServices','AreaSpecialModel','PedArea','SpecialModel','FireCollision','PedFire','PedCollision','DuckingSetter','PedGoggles','PedVisibility','ConstructorServices','WorldGround','WorldLine','WorldArea','WorldQueries','WorldJetpack','PedOnScreen','PedWeaponGrant','WorldIgnoreEntity','CameraSphereVisible','PedWeaponClear','PedClothes','PedRebuild','PedMoveAnim','PlayerNativePrerequisites','AnimationBlockRefs','AnimationBlockFactory','CustomSequenceStorage','CustomAnimationResources','AnimationManagerRefs','AnimationGateway','CustomAnimationSequence','CustomAnimationSequenceCopyFrame','AnimationHierarchyBank','CustomAnimationSequenceName','FreshCompressedSequence','PrivateHierarchy','PrivateHierarchyScalars','PrivateHierarchyName','PrivateHierarchyParent','PrivateHierarchyEmptyOperations','FreshUncompressedSequence','PrivateHierarchyPostprocess','FramesAndHierarchyPostprocess','PrivateAnimationFactories','CustomAnimationCleanup','StandaloneSequenceOperations','AnimationBankFactories','SequenceInterfaceCompatibility','AnimationManagerCache','AnimationBlockHashFactory','AnimationDefinitions','AnimationDefinitionBlockNames','AnimationGroupValidity','AnimationIdValidity','AnimationAssociationGroupReads','AnimationStaticAssociationReads','AnimationStaticAssociationWrites','PedRender','PrivateAnimationAssociations','AnimationAssociationBones','AnimationAssociationFloat','AnimationAssociationProgress','AnimationAssociationNodeArray')]
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
$record['nativeCameraBasicsActualSdkOwnerTested']=$false
$record['nativeCameraRestoreOriginalTested']=$false
$record['nativeCameraBasicsRefusalsTested']=$false
$record['nativeCameraRestoreDebtTested']=$false
$record['nativeModelTimeGetterActualSdkOwnerTested']=$false
$record['nativeModelTimeGetterRefusalsTested']=$false
$record['nativeModelTimeGetterOutputsPreserved']=$false
$record['nativeNamedVehiclePredicatesActualSdkOwnerTested']=$false
$record['nativeNamedVehiclePredicatesEffectsTested']=$false
$record['nativeNamedVehiclePredicatesRefusalsTested']=$false
$record['existingFourNativeClassifiersPreserved']=$false
$record['nativeEmptyBlockRemovalActualSdkOwnerTested']=$false
$record['nativeEmptyBlockRemovalEffectsTested']=$false
$record['nativeEmptyBlockRemovalRefusalsTested']=$false
$record['nativeEmptyBlockRemovalDebtTested']=$false
$record['nativeBankPostprocessActualSdkOwnerTested']=$false
$record['nativeBankPostprocessEffectsTested']=$false
$record['nativeBankPostprocessRefusalsTested']=$false
$record['nativeBankPostprocessUnknownRetained']=$false
$record['nativeBankEvictionActualSdkOwnerTested']=$false
$record['nativeBankEvictionConversionTested']=$false
$record['nativeBankEvictionRefusalsTested']=$false
$record['nativeBankEvictionUnknownRetained']=$false
$record['nativeBankDecompressionActualSdkOwnerTested']=$false
$record['nativeBankDecompressionConversionTested']=$false
$record['nativeBankDecompressionRefusalsTested']=$false
$record['nativeBankDecompressionUnknownRetained']=$false
$record['nativeHierarchyUncompressActualSdkTested']=$false
$record['nativeHierarchyUncompressCoupledRefusalsTested']=$false
$record['nativeHierarchyUncompressUnknownRetained']=$false
$record['nativeHierarchyUncompressPostOwnerLossTested']=$false
$record['nativeHierarchyUncompressEffectMismatchTested']=$false
$record['originalHierarchyUncompressInstructionsExecuted']=$false
$record['numericAssociationActualFactoryTested']=$false
$record['numericAssociationCodeChildLifetimeTested']=$false
$record['numericAssociationNullTested']=$false
$record['numericAssociationUnknownRetained']=$false
$record['numericAssociationForeignCodeRetained']=$false
$record['numericAssociationCpuFirstLifetimeTested']=$false
$record['numericAssociationClosureTested']=$false
$record['nativeDynamicClumpActualSdkOwnerTested']=$false
$record['nativeDynamicClumpZeroAllocationTested']=$false
$record['nativeDynamicClumpReferenceProvenanceTested']=$false
$record['nativeDynamicClumpHierarchyLifetimeTested']=$false
$record['nativeDynamicClumpEnteredFactsRetained']=$false
$record['nativeDynamicClumpTemporaryOriginTested']=$false
$record['nativeDynamicClumpSharedStaticRegressionTested']=$false
$record['nativeGroupFactoryActualSdkOwnerTested']=$false
$record['nativeGroupFactoryPhysicalResolverRefusalsTested']=$false
$record['nativeGroupFactoryCpuLifetimeNativeFactsTested']=$false
$record['nativeGroupSourceCreationOwnerTested']=$false
$record['nativeGroupSourceCreationSourceOrderTested']=$false
$record['nativeGroupSourceCreationRuntimeDrainTested']=$false
$record['nativeGroupSourceCreationEnteredFactsRetained']=$false
$record['nativeGroupSourceCreationScalarFactsTested']=$false
$record['nativeJointTemporaryStaticInitializerOwnerTested']=$false
$record['nativeJointTemporaryStaticClosedOriginsTested']=$false
$record['nativeJointTemporaryStaticZeroExtentTested']=$false
$record['nativeJointTemporaryStaticEnteredFactsRetained']=$false
$record['publicPedInitializerRegressionTested']=$false
$record['actualFEB5F0InstructionsExecuted']=$false
$record['nativeGroupReplacementOwnerTested']=$false
$record['nativeGroupReplacementSourceOrderTested']=$false
$record['nativeGroupReplacementOriginalRetirementTested']=$false
$record['nativeGroupReplacementPublicationOutcomesTested']=$false
$record['nativeGroupCopyAnimationOwnerTested']=$false
$record['actualGroupCopyAnimationSdkOwnerTested']=$false
$record['nativeGroupCopyAnimationNullableTested']=$false
$record['nativeGroupCopyAnimationEnteredDebtRetained']=$false
$record['nativeGroupEmptyInitializationOwnerTested']=$false
$record['actualGroupEmptyInitializationSdkOwnerTested']=$false
$record['nativeGroupEmptyInitializationPhaseAndPrefixTested']=$false
$record['nativeGroupEmptyInitializationEnteredDebtRetained']=$false
$record['nativeAnimationAssociationGroupChildProductsOwnerTested']=$false
$record['actualAnimationAssociationGroupStaticChildSdkOwnerTested']=$false
$record['nativeAnimationAssociationGroupChildProductsCoupledRefusalsTested']=$false
$record['nativeAnimationAssociationGroupOffsetOwnerTested']=$false
$record['actualAnimationAssociationGroupOffsetSdkOwnerTested']=$false
$record['animationAssociationGroupOffsetPhysicalTokenTested']=$false
$record['animationAssociationGroupOffsetLostAckStopTested']=$false
$record['animationAssociationGroupOffsetConflictStopTested']=$false
$record['nativeStaticAssociationInitializationRuntimeTested']=$false
$record['nativeStaticAssociationInitializationRegisteredBankTested']=$false
$record['nativeStaticAssociationInitializationEnteredDebtRetained']=$false
$record['nativePresentTransitionClockRuntimeTested']=$false
$record['nativePresentTransitionPeerHoldReaderExclusionTested']=$false
$record['nativePresentTransitionPeerFailureRecoveryTested']=$false
$record['nativePresentTransitionPatchRecoveryTested']=$false
$record['nativePresentTransitionWeatherRecoveryTested']=$false
$record['nativePresentTransitionWeatherHoldTested']=$false
$record['nativeStartupInitializationReadinessRuntimeTested']=$false
$record['nativeStartupInitializationJointReadinessTested']=$false
$record['nativeStartupInitializationFreshRefusalsTested']=$false
$record['nativeStartupInitializationBookendRefusalTested']=$false
$record['nativeStartupInitializationNoEffectsTested']=$false
$record['nativeStartupStreamingReceiverRuntimeTested']=$false
$record['nativeStartupStreamingReceiverFreshPublicationTested']=$false
$record['nativeStartupStreamingReceiverNativeDispatchTested']=$false
$record['nativeStartupNumericGaragesRuntimeTested']=$false
$record['nativeStartupNumericGaragesRuntimeStopTested']=$false
$record['nativeStartupPreparationReadinessRuntimeTested']=$false
$record['nativeStartupPreparationReadinessRefusalsPreserveOwner']=$false
$record['nativeStartupPreparationReadinessPublishedAllowed']=$false
$record['nativeStartupPreparationReadinessDebtDenied']=$false
$record['nativeStartupMenuRuntimeTested']=$false
$record['nativeStartupMenuRuntimeDebtRetained']=$false
$record['nativeStartupMenuRuntimeIndependentRestoreTested']=$false
$record['nativeStartupCollisionRuntimeTested']=$false
$record['nativeStartupCollisionRuntimeDebtRetained']=$false
$record['nativeStartupCollisionRuntimeIndependentRestoreTested']=$false
$record['nativePedModelCreationRuntimeTested']=$false
$record['nativePedModelCreationRuntimeNoRewindTested']=$false
$record['nativePedModelCreationRuntimeUnpublishedCleanupTested']=$false
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
$record['nativePrivateHierarchyEmptyOperationsOwnerTested']=$false
$record['actualPrivateHierarchyEmptyOperationsSdkOwnerTested']=$false
$record['privateHierarchyEmptyRecoveryTested']=$false
$record['privateHierarchyEmptyRetainedLifetimeTested']=$false
$record['nativePrivateHierarchyParentOwnerTested']=$false
$record['actualPrivateHierarchyParentSdkOwnerTested']=$false
$record['privateHierarchyParentRecoveryTested']=$false
$record['privateHierarchyParentRetainedLifetimeTested']=$false
$record['nativePrivateHierarchyNameOwnerTested']=$false
$record['actualPrivateHierarchyNameSdkOwnerTested']=$false
$record['privateHierarchyNamePinnedRefusalTested']=$false
$record['nativePrivateHierarchyScalarOwnerTested']=$false
$record['actualPrivateHierarchyScalarSdkOwnerTested']=$false
$record['privateHierarchyScalarPinnedRefusalTested']=$false
$record['nativePrivateHierarchyOwnerTested']=$false
$record['actualPrivateHierarchySdkOwnerTested']=$false
$record['privateHierarchyRetainedLifetimeTested']=$false
$record['nativePrivateAnimationFactoryOwnerTested']=$false
$record['actualPrivateAnimationFactorySdkOwnerTested']=$false
$record['privateAnimationFactoryOutputBookendsTested']=$false
$record['nativePrivateAnimationAssociationOwnerTested']=$false
$record['actualPrivateAnimationAssociationSdkOwnerTested']=$false
$record['privateAnimationAssociationConstructorUnknownRetained']=$false
$record['privateAnimationAssociationDestructorUnknownRetained']=$false
$record['privateAnimationAssociationRetirementPostAckRetained']=$false
$record['nativeAnimationAssociationNodeArrayOwnerTested']=$false
$record['actualAnimationAssociationNodeArraySdkOwnerTested']=$false
$record['animationAssociationNodeArrayUnknownFreeRetained']=$false
$record['animationAssociationNodeArrayUnknownClearRetained']=$false
$record['animationAssociationNodeArrayAcceptedClearRecoveryTested']=$false
$record['nativeAnimationAssociationProgressOwnerTested']=$false
$record['actualAnimationAssociationProgressSdkOwnerTested']=$false
$record['animationAssociationProgressUnknownRetained']=$false
$record['nativeAnimationAssociationFloatOwnerTested']=$false
$record['actualAnimationAssociationFloatSdkOwnerTested']=$false
$record['nativeAnimationAssociationBonesOwnerTested']=$false
$record['actualAnimationAssociationBonesSdkOwnerTested']=$false
$record['animationAssociationBonesPostFaultRetained']=$false
$record['animationAssociationBonesAcceptedCleanupTested']=$false
$record['animationAssociationBonesUnknownWriteRetained']=$false
$record['copiedNativeBodiesExecutedInOwnedFixture']=$false
$record['existingCustomSequenceStorageContractTested']=$false
$record['originalGameProcessAccess']=$false
$record['originalGameProcessCodeExecuted']=$false
$record['hardwareSehThroughCopiedNativeBodiesTested']=$false
$record['nativePedRenderOwnerTested']=$false
$record['actualPedRenderSdkOwnerTested']=$false
$record['nativeAnimationStaticAssociationWritesOwnerTested']=$false
$record['actualAnimationStaticAssociationWritesSdkOwnerTested']=$false
$record['animationStaticAssociationWriteConflictTested']=$false
$record['animationStaticAssociationWriteLostAckTested']=$false
$record['animationStaticAssociationWriteUnknownRetained']=$false
$record['nativeAnimationStaticAssociationOwnerTested']=$false
$record['actualAnimationStaticAssociationSdkOwnerTested']=$false
$record['nativeAnimationAssociationGroupOwnerTested']=$false
$record['actualAnimationAssociationGroupSdkOwnerTested']=$false
$record['nativeAnimationIdValidityOwnerTested']=$false
$record['actualAnimationIdValiditySdkOwnerTested']=$false
$record['nativeAnimationGroupValidityOwnerTested']=$false
$record['actualAnimationGroupValiditySdkOwnerTested']=$false
$record['nativeAnimationDefinitionBlockNamesOwnerTested']=$false
$record['actualAnimationDefinitionBlockNamesSdkOwnerTested']=$false
$record['nativeAnimationDefinitionsOwnerTested']=$false
$record['actualAnimationDefinitionsSdkOwnerTested']=$false
$record['nativeAnimationBlockHashFactoryOwnerTested']=$false
$record['actualAnimationBlockHashFactorySdkOwnerTested']=$false
$record['nativeAnimationManagerCacheOwnerTested']=$false
$record['actualAnimationManagerCacheSdkOwnerTested']=$false
$record['animationManagerCacheForeignStateRetained']=$false
$record['nativeSequenceInterfaceOwnerTested']=$false
$record['actualSequenceInterfaceSdkOwnerTested']=$false
$record['sequenceInterfaceUnknownMetadataFreeRetained']=$false
$record['nativeBankAnimationFactoryOwnerTested']=$false
$record['actualBankAnimationFactorySdkOwnerTested']=$false
$record['bankAnimationFactoryGenerationBookendsTested']=$false
$record['actualStandaloneSequenceOperationsOwnerTested']=$false
$record['standaloneSequenceBindLostAckTested']=$false
$record['standaloneSequenceCopyLostAckTested']=$false
$record['standaloneSequenceUnknownWriteRetained']=$false
$record['standaloneSequenceForeignDropRetained']=$false
$record['standaloneSequenceStrictFactoryDispatchTested']=$false
$record['actualPrivateAnimationCleanupControllerTested']=$false
$record['actualPrivateAnimationCleanupSdkOwnerTested']=$false
$record['privateAnimationCleanupEndLifetimeTested']=$false
$record['privateAnimationCleanupReturnedAckRecoveryTested']=$false
$record['privateAnimationCleanupUnknownDebtRetained']=$false
$record['privateAnimationCleanupForeignDropRetained']=$false
$record['privateAnimationCleanupActivePumpTested']=$false
$record['nativePrivateHierarchyPostprocessOwnerTested']=$false
$record['actualPrivateHierarchyPostprocessSdkOwnerTested']=$false
$record['privateHierarchyPostprocessRecoveryTested']=$false
$record['nativeFreshUncompressedSequenceOwnerTested']=$false
$record['actualFreshUncompressedSequenceServiceOwnerTested']=$false
$record['nativeFreshCompressedSequenceOwnerTested']=$false
$record['actualFreshCompressedSequenceServiceOwnerTested']=$false
$record['nativeCustomAnimationSequenceNameOwnerTested']=$false
$record['actualCustomAnimationSequenceNameSdkOwnerTested']=$false
$record['nativeAnimationHierarchyBankOwnerTested']=$false
$record['actualAnimationHierarchyBankSdkOwnerTested']=$false
$record['nativeCustomAnimationSequenceCopyFrameOwnerTested']=$false
$record['actualCustomAnimationSequenceCopyFrameSdkOwnerTested']=$false
$record['nativeCustomAnimationSequenceOwnerTested']=$false
$record['actualCustomAnimationSequenceSdkOwnerTested']=$false
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
        'Client/game_de/NativeAnimationBlockAccess.h','Client/game_de/NativeAnimationBlockMemory.h','Client/game_de/NativeAnimationBlockMemory.cpp','Client/game_de/NativeAnimationBlockProfile.h','Client/game_de/CAnimBlockDE.h','Client/game_de/CAnimBlockDE.cpp','Client/sdk/game/CAnimBlock.h','Client/game_de/CModelInfoDE.h','Client/game_de/CModelInfoDE.cpp','Client/game_de/CModelInfoRequestDE.h','Client/game_de/CModelInfoRequestDE.cpp','Client/sdk/game/CModelRequestPolicy.h','Client/game_de/CAnimBlockRequestDE.h','Client/game_de/CAnimBlockRequestDE.cpp','Client/sdk/game/CAnimBlockRequestPolicy.h',
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
        'Client/game_de/NativeModelTimeProfile.h','Tests/definitive/NativeModelTimeGetterOwnerTests.inl','Tests/definitive/Write-ModelTimeFixture.ps1',
        'Client/game_de/NativeModelVehicleNamedProfile.h','Tests/definitive/NativeVehicleIdePredicatesOwnerTests.inl','Tests/definitive/Write-ModelNamedPredicateFixture.ps1',
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
        'Client/game_de/NativeModelObjectReader.h',
        'Client/game_de/NativeModelObjectReader.cpp',
        'Client/game_de/NativeModelObjectProfile.h',
        'Client/game_de/NativePedModelMetadataStorageCalls.h',
        'Client/game_de/NativePedModelCreation.h',
        'Client/game_de/NativePedModelCreation.cpp',
        'Client/game_de/NativeStartupModelCollision.h',
        'Client/game_de/NativeStartupModelCollision.cpp',
        'Client/game_de/NativePedModelDefaultCollision.h',
        'Client/game_de/NativePedModelDefaultCollision.cpp',
        'Client/game_de/NativePedModelMetadataProfile.h',
        'Client/game_de/NativeModelAnimationReader.h',
        'Client/game_de/NativeModelAnimationReader.cpp',
        'Client/game_de/NativeModelAnimationProfile.h',
        'Client/game_de/NativeModelReferenceOrigin.h',
        'Client/game_de/NativeModelReferenceAcquisitionReceipt.h',
        'Client/game_de/NativeModelBackingReferenceAccess.h',
        'Client/game_de/NativeModelBackingReferenceController.h',
        'Client/game_de/NativeModelBackingReferenceController.cpp',
        'Client/game_de/NativeModelUnusedRemoval.h',
        'Client/game_de/NativeModelUnusedRemoval.cpp',
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
        'Client/game_de/NativeStreamingCollisionSceneCalls.h',
        'Client/game_de/NativeStreamingCollisionSceneCalls.cpp',
        'Client/game_de/NativeStreamingCollisionSceneProfile.h',
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
    $sourceNames += @('Client/game_de/NativeAnimationSequenceAccess.h','Client/game_de/NativeAnimationSequenceLogicalState.h','Client/game_de/NativeAnimationSequenceRowCalls.h','Client/game_de/NativeAnimationSequenceRowProfile.h','Client/game_de/CAnimBlendSequenceDE.h','Client/game_de/CAnimBlendSequenceDE.cpp','Tests/definitive/AnimationSequenceDEFixture.h','Tests/definitive/NativeCustomAnimationSequenceOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativeAnimationSequenceCopyCalls.h','Client/game_de/NativeAnimationSequenceFrameViewCalls.h','Tests/definitive/NativeCustomAnimationSequenceCopyFrameOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativeAnimationHierarchyAccess.h','Client/game_de/NativeAnimationHierarchyMemory.h','Client/game_de/NativeAnimationHierarchyMemory.cpp','Client/game_de/NativeAnimationHierarchyOrigin.h','Client/game_de/NativeAnimationHierarchyReadProfile.h','Client/game_de/CAnimBlendHierarchyDE.h','Client/game_de/CAnimBlendHierarchyDE.cpp','Tests/definitive/AnimationHierarchyDEFixture.h','Tests/definitive/NativeAnimationHierarchyBankOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativeAnimationSequenceName.h','Client/game_de/NativeAnimationSequenceName.cpp','Client/game_de/NativeAnimationSequenceNameCalls.h','Client/game_de/NativeAnimationSequenceNameProfile.h','Tests/definitive/NativeCustomAnimationSequenceNameOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativeAnimationSequenceFrameBindCalls.h','Client/game_de/NativeAnimationSequenceFrameBindProfile.h','Tests/definitive/NativeFreshCompressedSequenceOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativePrivateAnimationHierarchyStorage.h','Client/game_de/NativePrivateAnimationHierarchyStorage.cpp','Client/game_de/NativePrivateAnimationHierarchyController.h','Client/game_de/NativePrivateAnimationHierarchyController.cpp','Tests/definitive/NativePrivateHierarchyOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativePrivateAnimationAssociationOwnerTests.inl','Tests/definitive/Write-PrivateAnimationAssociationFixture.ps1','Client/game_de/NativeAnimationAssociationAccess.h','Client/game_de/CAnimBlendAssociationDE.h','Client/game_de/CAnimBlendAssociationDE.cpp','Client/game_de/NativeAnimationAssociationStorage.h','Client/game_de/NativeAnimationAssociationStorage.cpp','Client/game_de/NativeAnimationAssociationController.h','Client/game_de/NativeAnimationAssociationController.cpp','Client/game_de/NativeAnimationAssociationOuterStorageCalls.h','Client/game_de/NativeAnimationAssociationProfile.h')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationBonesOwnerTests.inl','Tests/definitive/NativeAnimationAssociationFloatOwnerTests.inl','Tests/definitive/NativeAnimationAssociationProgressOwnerTests.inl','Tests/definitive/NativeAnimationAssociationNodeArrayOwnerTests.inl','Client/game_de/NativeAnimationAssociationProgressProfile.h')
    $sourceNames += @('Tests/definitive/NativePedRenderOwnerTests.inl','Tests/definitive/Write-PedRenderFixture.ps1','Client/game_de/NativePedRenderMemory.h','Client/game_de/NativePedRenderMemory.cpp','Client/game_de/NativePedRenderProfile.h','Client/game_de/NativePedMetadataAccess.h','Client/game_de/CPedMetadataDE.h','Client/game_de/CPedMetadataDE.cpp')
    $sourceNames += @('Tests/definitive/NativeAnimationStaticAssociationWritesOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeStaticAssociationInitializationOwnerTests.inl','Tests/definitive/NativeStaticAssociationInitializeFixture.h',
        'Client/game_de/NativeStaticAssociationInitializeCalls.h','Client/game_de/NativeStaticAssociationInitializeCalls.cpp','Client/game_de/NativeStaticAssociationInitializeProfile.h')
    $sourceNames += @('Tests/definitive/NativeAnimationStaticAssociationOwnerTests.inl','Tests/definitive/Write-AnimationStaticAssociationFixture.ps1','Client/game_de/NativeAnimationStaticAssociationAccess.h','Client/game_de/CAnimBlendStaticAssociationDE.h','Client/game_de/CAnimBlendStaticAssociationDE.cpp','Client/sdk/game/CAnimBlendStaticAssociation.h')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupOffsetOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupChildProductsOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupEmptyInitializationOwnerTests.inl','Client/game_de/NativeAnimationAssociationGroupInitialization.h')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupCopyOwnerTests.inl','Client/game_de/NativeAnimationAssociationGroupCopy.h','Client/game_de/NativeAnimationAssociationGroupCopyProfile.h')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupReplacementOwnerTests.inl','Client/game_de/NativeAnimationAssociationGroupReplacement.h','Client/game_de/NativeAnimationAssociationGroupReplacement.cpp','Client/game_de/NativeAnimationAssociationGroupChildBorrow.h','Client/game_de/NativeStaticAssociationArrayStorageCalls.h','Client/game_de/NativeStaticAssociationArrayProfile.h')
    $sourceNames += @('Client/game_de/NativeAnimationHierarchyCacheRemove.h','Client/game_de/NativeAnimationHierarchyCacheRemove.cpp','Client/game_de/NativeAnimationHierarchyCacheRemoveProfile.h')
    $sourceNames += @('Tests/definitive/ApplicationTimingOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeBankDecompressionOwnerTests.inl','Client/game_de/NativeAnimationHierarchyDecompressProfile.h','Tests/definitive/NativeBankEvictionOwnerTests.inl','Client/game_de/NativeAnimationHierarchyEvictionProfile.h')
    $sourceNames += @('Tests/definitive/NativeEmptyAnimationBlockRemovalOwnerTests.inl','Tests/definitive/Write-AnimationManagerEmptyBlockRemovalFixture.ps1','Client/game_de/NativeAnimationBlockRemoval.h','Client/game_de/NativeAnimationBlockRemoval.cpp')
    $sourceNames += @('Tests/definitive/NativeBankPostprocessOwnerTests.inl','Tests/definitive/NativeAnimationBlockRegistrationOwnerTests.inl','Tests/definitive/Write-AnimationManagerRegistrationFixture.ps1','Client/game_de/NativeAnimationBlockRegistration.h','Client/game_de/NativeAnimationBlockRegistration.cpp')
    $sourceNames += @('Client/game_de/NativeAnimationHierarchyUncompress.h','Client/game_de/NativeAnimationHierarchyUncompress.cpp','Tests/definitive/NativeAnimationHierarchyUncompressOwnerTests.inl','Tests/definitive/Write-AnimationManagerUncompressFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeNumericAssociationOwnerTests.inl','Tests/definitive/Write-AnimationAssociationNumericFactoryFixture.ps1','Tests/definitive/Write-AnimationAssociationRawFactoryFixture.ps1','Client/game_de/NativeAnimationAssociationIssuedView.h')
    $sourceNames += @('Tests/definitive/NativeDynamicClumpOwnerTests.inl','Client/game_de/NativeAnimationAssociationClumpTypes.h','Client/game_de/NativeAnimationAssociationClumpCalls.h','Client/game_de/NativeAnimationClumpInitializationPlan.h','Client/game_de/NativeAnimationClumpInitializationPlan.cpp','Client/game_de/NativeAnimationAssociationClumpVerification.h','Client/game_de/NativeAnimationAssociationClumpVerification.cpp','Client/game_de/NativeAnimationAssociationClumpOrigin.h','Client/game_de/NativeAnimationAssociationClumpOrigin.cpp','Client/game_de/NativeAnimationAssociationClumpStorage.inl','Client/game_de/NativeAnimationAssociationHierarchyDependency.h','Client/game_de/NativeAnimationAssociationClumpStorageState.h','Client/game_de/NativeTemporaryDynamicAssociationInitializeCalls.h','Client/game_de/NativeTemporaryDynamicAssociationInitializeCalls.cpp')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupFactoriesOwnerTests.inl','Tests/definitive/Write-AnimationAssociationGroupFactoryFixture.ps1')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupSourceCreationOwnerTests.inl','Client/game_de/NativeAnimationAssociationGroupCreationController.h','Client/game_de/NativeAnimationAssociationGroupCreationController.cpp','Client/game_de/NativeModelHashReader.h','Client/game_de/NativeModelHashReader.cpp','Client/game_de/NativeModelHashProfile.h')
    $sourceNames += @('Tests/definitive/NativeJointTemporaryStaticInitializerOwnerTests.inl','Client/game_de/NativeTemporaryStaticAssociationInitializeCalls.h','Client/game_de/NativeTemporaryStaticAssociationInitializeCalls.cpp','Client/game_de/NativeStaticAssociationInitializeCore.h','Client/game_de/NativeTemporaryModelClump.h','Client/game_de/NativeTemporaryModelClump.cpp','Client/game_de/NativeTemporaryModelClumpProfile.h')
    $sourceNames += @('Tests/definitive/NativeAnimationAssociationGroupOwnerTests.inl','Tests/definitive/Write-AnimationAssociationGroupFixture.ps1','Client/game_de/NativeAnimationAssociationGroupAccess.h','Client/game_de/CAnimBlendAssocGroupDE.h','Client/game_de/CAnimBlendAssocGroupDE.cpp','Client/sdk/game/CAnimBlendAssocGroup.h')
    $sourceNames += @('Tests/definitive/NativeAnimationIdValidityOwnerTests.inl','Client/game_de/NativeAnimationStaticAssociationProfile.h','Client/sdk/game/CAnimValidityPolicy.h')
    $sourceNames += @('Tests/definitive/NativeAnimationGroupValidityOwnerTests.inl','Client/game_de/NativeAnimationGroupArrayProfile.h','Client/sdk/game/CAnimGroupValidationPolicy.h')
    $sourceNames += @('Tests/definitive/NativeAnimationDefinitionBlockNamesOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeAnimationDefinitionsOwnerTests.inl','Client/game_de/NativeAnimationAssocDefinitionProfile.h')
    $sourceNames += @('Tests/definitive/NativeAnimationBlockHashFactoryOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeAnimationManagerCacheOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeSequenceInterfaceOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeBankAnimationFactoryOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativeStandaloneSequenceOperationsTests.inl','Client/game_de/NativeCustomAnimationSequenceOperationAccess.h','Client/sdk/game/CCustomAnimationSequenceOperation.h','Client/mods/deathmatch/logic/CIFPAnimationStorage.h')
    $sourceNames += @('Tests/definitive/NativeCustomAnimationCleanupControllerTests.inl','Tests/definitive/NativeCustomAnimationCleanupOwnerTests.inl','Client/game_de/NativeCustomAnimationCleanupAccess.h','Client/sdk/game/CCustomAnimationCleanup.h')
    $sourceNames += @('Tests/definitive/NativePrivateAnimationFactoryOwnerTests.inl','Tests/definitive/Write-PrivateAnimationFactoryFixture.ps1','Client/game_de/NativeAnimationSequenceFactoryCalls.h','Client/game_de/NativeModelReferenceMap.h')
    $sourceNames += @('Tests/definitive/NativePrivateHierarchyPostProcessOwnerTests.inl','Client/game_de/NativePrivateAnimationHierarchyPostProcessProfile.h','Client/game_de/NativeAnimationSequencePayloadCalls.h')
    $sourceNames += @('Tests/definitive/NativeFreshUncompressedSequenceOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativePrivateHierarchyEmptyOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativePrivateHierarchyParentOwnerTests.inl','Client/game_de/NativeAnimationSequenceDependencyCalls.h')
    $sourceNames += @('Tests/definitive/NativePrivateHierarchyScalarOwnerTests.inl')
    $sourceNames += @('Tests/definitive/NativePrivateHierarchyNameOwnerTests.inl','Client/game_de/NativeClassicAnimationNameHash.h','Client/game_de/NativeClassicAnimationNameHash.cpp')
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
$sourceNames += @('Client/game_de/NativeCameraDataProfile.h','Client/game_de/NativeCameraDataMemory.h','Client/game_de/NativeCameraDataMemory.cpp','Client/game_de/NativeCameraRestore.h','Client/game_de/NativeCameraRestore.cpp','Client/game_de/NativeCameraRestoreProfile.h','Client/game_de/CCamFovDE.h','Client/game_de/CCamFovDE.cpp','Tests/definitive/NativeCameraBasicsOwnerTests.inl','Tests/definitive/Write-CameraBasicsFixture.ps1')
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
    $sourceNames += @('Tests/definitive/NativePedModelCreationRuntimeOwnerTests.inl','Tests/definitive/NativeStartupModelCollisionRuntimeOwnerTests.inl',
        'Build/definitive-recovery/mta-startup-model-collision-20261028/startup-model-collision-runtime-admission-ready.json',
        'Build/definitive-recovery/mta-startup-model-collision-20261028/startup-model-collision-runtime-tests-source-ready.json')
    $sourceNames += @('Client/game_de/NativeStartupMenuAccess.h','Client/game_de/NativeStartupMenuCalls.h','Client/game_de/NativeStartupMenuCalls.cpp',
        'Client/game_de/NativeStartupMenuProfile.h','Tests/definitive/NativeStartupMenuGraphFixture.h','Tests/definitive/NativeStartupMenuRuntimeOwnerTests.inl',
        'Build/definitive-recovery/mta-native-menu-close-20261028/native-menu-close-source-ready.json',
        'Build/definitive-recovery/mta-native-menu-close-20261028/tests/native-menu-tests-source-ready.json')
    $sourceNames += @('Client/game_de/NativeStartupPreparationAccess.h',
        'Tests/definitive/NativeStartupPreparationReadinessRuntimeOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativeNumericGarageAccess.h','Client/game_de/NativeNumericGarageProfile.h',
        'Client/game_de/NativeNumericGarageMemory.h','Client/game_de/NativeNumericGarageMemory.cpp','Client/game_de/NativeGarageMemoryShared.h',
        'Client/game_de/CGaragesStartupDE.h','Client/game_de/CGaragesStartupDE.cpp','Client/game_de/CGameNumericGaragesDE.h',
        'Tests/definitive/NativeStartupNumericGaragesRuntimeOwnerTests.inl',
        'Build/definitive-recovery/mta-full-numeric-garage-service-20261028/full-numeric-garage-service-source-ready.json',
        'Build/definitive-recovery/mta-garage-full-normalization-20261028/qualified-numeric50-garage-reference-ready.json',
        'Build/definitive-recovery/mta-garage-full-normalization-20261028/exact-full-comparison.json',
        'Build/definitive-recovery/mta-garage-full-normalization-20261028/tuple-qualification.json',
        'Build/definitive-recovery/mta-garage-full-normalization-20261028/normalized-full-reference.json',
        'Build/definitive-recovery/mta-garage-full-normalization-20261028/normalization-contract.json')
    $sourceNames += @('Client/game_de/NativeStartupStreamingAccess.h','Client/game_de/CStreamingStartupDE.h',
        'Client/game_de/CStreamingStartupDE.cpp','Client/game_de/CGameStartupStreamingDE.h',
        'Tests/definitive/NativeStartupStreamingReceiverRuntimeOwnerTests.inl')
    $sourceNames += @('Client/game_de/CGameStartupInitializationDE.h',
        'Tests/definitive/NativeStartupInitializationReadinessRuntimeOwnerTests.inl')
    $sourceNames += @('Client/game_de/NativePresentTransitionGate.h','Client/game_de/NativePresentTransitionGate.cpp',
        'Tests/definitive/NativePresentTransitionClockOwnerTests.inl')
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
    . (Join-Path $PSScriptRoot 'Write-CameraBasicsFixture.ps1')
    $basicsFixture=Write-CameraBasicsFixture $taskRoot (Join-Path $scratchRoot 'CameraBasicsFixture.generated.h')
    $fovFixture=Write-CamFovFixture $taskRoot (Join-Path $scratchRoot 'CamFovFixture.generated.h')
    foreach($generated in @($basicsFixture,$fovFixture)){
        $evidence=Join-Path $outputRoot ([IO.Path]::GetFileName($generated.path))
        Copy-Item -LiteralPath $generated.path -Destination $evidence
        $record.executionArtifacts+=@{path=$evidence;sha256=(Sha $evidence)}
    }
    $record['cameraBasicsGeneratedFixture']=$basicsFixture
    $record['camFovGeneratedFixture']=$fovFixture
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
    . (Join-Path $PSScriptRoot 'Write-ModelTimeFixture.ps1')
    $timeFixture=Write-ModelTimeFixture $taskRoot (Join-Path $scratchRoot 'ModelTimeFixture.generated.h')
    if($timeFixture.rejectingMethods -ne 74 -or @($timeFixture.inheritedProductionMethods).Count -ne 18){throw 'Time receiver must inherit18 actual methods and reject74 unrelated SDK methods'}
    $timeEvidence=Join-Path $outputRoot 'ModelTimeFixture.generated.h'
    Copy-Item -LiteralPath $timeFixture.path -Destination $timeEvidence
    $record['modelTimeGeneratedFixture']=@{path=$timeEvidence;sha256=(Sha $timeEvidence);rejectingMethods=$timeFixture.rejectingMethods;inheritedProductionMethods=$timeFixture.inheritedProductionMethods}
    . (Join-Path $PSScriptRoot 'Write-ModelNamedPredicateFixture.ps1')
    $namedVehicleFixture=Write-ModelNamedPredicateFixture $taskRoot (Join-Path $scratchRoot 'ModelNamedPredicateFixture.generated.h')
    if($namedVehicleFixture.rejectingMethods -ne 75 -or @($namedVehicleFixture.inheritedProductionMethods).Count -ne 17){throw 'Named vehicle receiver must inherit17 actual observations and reject75 unrelated SDK methods'}
    $namedVehicleEvidence=Join-Path $outputRoot 'ModelNamedPredicateFixture.generated.h'
    Copy-Item -LiteralPath $namedVehicleFixture.path -Destination $namedVehicleEvidence
    $record['namedVehicleGeneratedFixture']=@{path=$namedVehicleEvidence;sha256=(Sha $namedVehicleEvidence);rejectingMethods=$namedVehicleFixture.rejectingMethods;inheritedProductionMethods=$namedVehicleFixture.inheritedProductionMethods}
    . (Join-Path $PSScriptRoot 'Write-AnimationManagerRegistrationFixture.ps1')
    $registrationFixture=Write-AnimationManagerRegistrationFixture $taskRoot (Join-Path $scratchRoot 'AnimationManagerRegistrationFixture.generated.h')
    $registrationEvidence=Join-Path $outputRoot 'AnimationManagerRegistrationFixture.generated.h'
    Copy-Item -LiteralPath $registrationFixture.path -Destination $registrationEvidence
    $record['registrationGeneratedFixture']=@{path=$registrationEvidence;sha256=(Sha $registrationEvidence);rejectingMethods=$registrationFixture.rejectingMethods;inheritedMethods=$registrationFixture.inheritedMethods}
    . (Join-Path $PSScriptRoot 'Write-AnimationManagerEmptyBlockRemovalFixture.ps1')
    $emptyRemovalFixture=Write-AnimationManagerEmptyBlockRemovalFixture $taskRoot (Join-Path $scratchRoot 'AnimationManagerEmptyBlockRemovalFixture.generated.h')
    if(@($emptyRemovalFixture.inheritedMethods).Count -ne 9 -or $emptyRemovalFixture.rejectingMethods -ne 63){throw 'Empty block removal receiver must inherit exactly actual manager registration/reference observations plus RemoveAnimBlock'}
    $emptyRemovalEvidence=Join-Path $outputRoot 'AnimationManagerEmptyBlockRemovalFixture.generated.h'
    Copy-Item -LiteralPath $emptyRemovalFixture.path -Destination $emptyRemovalEvidence
    $record['emptyBlockRemovalGeneratedFixture']=@{path=$emptyRemovalEvidence;sha256=(Sha $emptyRemovalEvidence);rejectingMethods=$emptyRemovalFixture.rejectingMethods;inheritedMethods=$emptyRemovalFixture.inheritedMethods}
    . (Join-Path $PSScriptRoot 'Write-AnimationManagerUncompressFixture.ps1')
    $uncompressFixture=Write-AnimationManagerUncompressFixture $taskRoot (Join-Path $scratchRoot 'AnimationManagerUncompressFixture.generated.h')
    if(@($uncompressFixture.inheritedMethods).Count -ne 5 -or $uncompressFixture.rejectingMethods -ne 64){throw 'Uncompress receiver must inherit actual manager dispatch and two bank factories'}
    $uncompressEvidence=Join-Path $outputRoot 'AnimationManagerUncompressFixture.generated.h'
    Copy-Item -LiteralPath $uncompressFixture.path -Destination $uncompressEvidence
    $record['animationManagerUncompressFixture']=$uncompressFixture
    $record.animationManagerUncompressFixture.path=$uncompressEvidence
    . (Join-Path $PSScriptRoot 'Write-AnimationAssociationRawFactoryFixture.ps1')
    $numericFactoryFixture=Write-AnimationAssociationRawFactoryFixture $taskRoot (Join-Path $scratchRoot 'AnimationAssociationRawFactoryFixture.generated.h')
    if(@($numericFactoryFixture.inheritedFactories).Count -ne 2 -or $numericFactoryFixture.rejectingMethods -ne 67){throw 'Issued raw receiver must inherit exactly actual numeric Create and raw Get'}
    $numericFactoryEvidence=Join-Path $outputRoot 'AnimationAssociationRawFactoryFixture.generated.h'
    Copy-Item -LiteralPath $numericFactoryFixture.path -Destination $numericFactoryEvidence
    $record['animationAssociationRawFactoryFixture']=$numericFactoryFixture
    $record.animationAssociationRawFactoryFixture.path=$numericFactoryEvidence
    . (Join-Path $PSScriptRoot 'Write-AnimationAssociationGroupFactoryFixture.ps1')
    $groupFactoryFixture=Write-AnimationAssociationGroupFactoryFixture $taskRoot (Join-Path $scratchRoot 'AnimationAssociationGroupFactoryManagerFixture.generated.h')
    $groupFactoryEvidence=Join-Path $outputRoot 'AnimationAssociationGroupFactoryManagerFixture.generated.h'
    Copy-Item -LiteralPath $groupFactoryFixture.path -Destination $groupFactoryEvidence
    $record['animationAssociationGroupFactoryManagerFixture']=$groupFactoryFixture
    $record.animationAssociationGroupFactoryManagerFixture.path=$groupFactoryEvidence
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

    # Use the actual qualified profile literal; keep generated fixture bytes
    # and their production source hash as durable evidence after formatting.
    . (Join-Path $PSScriptRoot 'Write-PrivateAnimationFactoryFixture.ps1')
    $factoryFixture=Write-PrivateAnimationFactoryFixture $taskRoot (Join-Path $scratchRoot 'PrivateAnimationFactoryManager.generated.h')
    $factoryEvidence=@()
    foreach($generated in @(Get-ChildItem -LiteralPath $scratchRoot -File | Where-Object{$_.Name -like 'PrivateAnimationFactoryManager.generated.h*'})){
        $durable=Join-Path $outputRoot $generated.Name
        Copy-Item -LiteralPath $generated.FullName -Destination $durable
        $factoryEvidence+=@{path=$durable;sha256=(Sha $durable)}
    }
    $record['privateAnimationFactoryManagerFixture']=[ordered]@{metadata=$factoryFixture;files=$factoryEvidence}
    # This stack-only receiver inherits the real five group reads; the remaining
    # mandatory operations reject and no native factory/base deletion is faked.
    . (Join-Path $PSScriptRoot 'Write-AnimationAssociationGroupFixture.ps1')
    $associationGroupFixture=Write-AnimationAssociationGroupFixture $taskRoot (Join-Path $scratchRoot 'AnimationAssociationGroupFixture.generated.h')
    $associationGroupEvidence=Join-Path $outputRoot 'AnimationAssociationGroupFixture.generated.h'
    Copy-Item -LiteralPath $associationGroupFixture.path -Destination $associationGroupEvidence
    $record['animationAssociationGroupFixture']=$associationGroupFixture
    $record.animationAssociationGroupFixture.path=$associationGroupEvidence
    # Keep the six real getters visible on a stack-only receiver; remaining
    # mutation/initialization methods reject and no ownership is fabricated.
    . (Join-Path $PSScriptRoot 'Write-AnimationStaticAssociationFixture.ps1')
    $staticAssociationFixture=Write-AnimationStaticAssociationFixture $taskRoot (Join-Path $scratchRoot 'AnimationStaticAssociationFixture.generated.h')
    $staticAssociationEvidence=Join-Path $outputRoot 'AnimationStaticAssociationFixture.generated.h'
    Copy-Item -LiteralPath $staticAssociationFixture.path -Destination $staticAssociationEvidence
    $record['animationStaticAssociationFixture']=$staticAssociationFixture
    $record.animationStaticAssociationFixture.path=$staticAssociationEvidence
    # Preserve the actual seven-method ped receiver as evidence; unsupported
    # inherited entity operations reject instead of manufacturing a full CGame.
    . (Join-Path $PSScriptRoot 'Write-PedRenderFixture.ps1')
    $pedRenderFixture=Write-PedRenderFixture $taskRoot (Join-Path $scratchRoot 'PedRenderFixture.generated.h')
    $pedRenderEvidence=Join-Path $outputRoot 'PedRenderFixture.generated.h'
    Copy-Item -LiteralPath $pedRenderFixture.path -Destination $pedRenderEvidence
    $record['pedRenderFixture']=$pedRenderFixture
    $record.pedRenderFixture.path=$pedRenderEvidence
    # This CPU-only strict leaf keeps the source's three noexcept getters and
    # other unqualified contracts genuinely unsupported, without base deletion.
    . (Join-Path $PSScriptRoot 'Write-PrivateAnimationAssociationFixture.ps1')
    $privateAssociationFixture=Write-PrivateAnimationAssociationFixture $taskRoot (Join-Path $scratchRoot 'PrivateAnimationAssociationFixture.generated.h')
    if($Scope -eq 'AnimationAssociationFloat' -and ($privateAssociationFixture.rejectingMethods -ne 0 -or @($privateAssociationFixture.inheritedProductionMethods).Count -ne 22)){throw 'Actual float getter receiver must inherit22 real methods with zero fixture replacements'}
    if($Scope -eq 'AnimationAssociationProgress' -and ($privateAssociationFixture.rejectingMethods -ne 0 -or @($privateAssociationFixture.inheritedProductionMethods).Count -ne 22)){throw 'Actual progress receiver must inherit22 real methods with zero fixture replacements'}
    if($Scope -eq 'AnimationAssociationNodeArray' -and ($privateAssociationFixture.rejectingMethods -ne 0 -or @($privateAssociationFixture.inheritedProductionMethods).Count -ne 22)){throw 'Actual node-only release receiver must inherit22 real methods with zero fixture replacements'}
    $privateAssociationEvidence=Join-Path $outputRoot 'PrivateAnimationAssociationFixture.generated.h'
    Copy-Item -LiteralPath $privateAssociationFixture.path -Destination $privateAssociationEvidence
    $record['privateAnimationAssociationFixture']=$privateAssociationFixture
    $record.privateAnimationAssociationFixture.path=$privateAssociationEvidence
    $privateSource=Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp'
    $privateText=[IO.File]::ReadAllText($privateSource)
    $privateMatch=[regex]::Match($privateText,'(?s)inline\s+constexpr\s+unsigned\s+char\s+CacheProof\s*\[\s*\]\s*=\s*\{(?<values>.*?)\};')
    if(-not $privateMatch.Success){throw 'Actual private hierarchy CacheProof literal missing'}
    $privateValues=@([regex]::Matches($privateMatch.Groups['values'].Value,'0x[0-9a-fA-F]{2}')|ForEach-Object{$_.Value})
    if($privateValues.Count -ne 84){throw 'Actual private hierarchy proof extent changed'}
    $privateHeader=Join-Path $scratchRoot 'PrivateHierarchyProfile.generated.h'
    [IO.File]::WriteAllText($privateHeader,('#pragma once'+[Environment]::NewLine+'namespace private_hierarchy_fixture { inline constexpr unsigned char CacheProof[] = {'+($privateValues -join ',')+'}; }'),[Text.UTF8Encoding]::new($false))
    $privateEvidence=Join-Path $outputRoot 'PrivateHierarchyProfile.generated.h'
    Copy-Item -LiteralPath $privateHeader -Destination $privateEvidence
    $record['privateHierarchyProfileFixture']=[ordered]@{production=$privateSource;productionSha256=(Sha $privateSource);generated=$privateEvidence;generatedSha256=(Sha $privateEvidence);bytes=$privateValues.Count}
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
    # The cumulative ClockServiceTests translation unit exceeded MSVC's normal
    # COFF section limit (fatal C1128); bigobj changes only the object format.
    $arguments = $common + @('/bigobj','/W4','/WX',(Join-Path $PSScriptRoot 'ClockServiceTests.cpp'),
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
        (Join-Path $taskRoot 'Client/game_de/CGaragesStartupDE.cpp'),
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
        (Join-Path $taskRoot 'Client/game_de/CCamFovDE.cpp'),
        # The new owner scope instantiates the actual SDK streaming receiver;
        # link its production implementation rather than a fixture substitute.
        (Join-Path $taskRoot 'Client/game_de/CStreamingDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CStreamingStartupDE.cpp'),
        # Runtime also references collision-only loading. Link its separate
        # production TU once; the full-scene fixture keeps its aggregate seam.
        (Join-Path $taskRoot 'Client/game_de/NativeStreamingCollisionSceneCalls.cpp'),
        # Runtime's opaque model observers reference this real reader even
        # when a selected Clock scope never calls those optional methods.
        (Join-Path $taskRoot 'Client/game_de/NativeModelObjectReader.cpp'),
        # Runtime now owns these optional services in every selected scope;
        # pin their complete inputs above and link each implementation once.
        (Join-Path $taskRoot 'Client/game_de/NativeModelAnimationReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelBackingReferenceController.cpp'),
        # The shared controller now calls the separate zero-only removal helper.
        (Join-Path $taskRoot 'Client/game_de/NativeModelUnusedRemoval.cpp'),
        # Runtime references the real creator/default-collision qualifiers in
        # every scope. Link them once even when the selected fixture is older.
        (Join-Path $taskRoot 'Client/game_de/NativePedModelCreation.cpp'),
        # Collision is compiled once inside the actual Clock fixture with
        # only its final OS atomic effect faulted on owned CPU pages.
        (Join-Path $taskRoot 'Client/game_de/NativePedModelDefaultCollision.cpp'),
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
        (Join-Path $taskRoot 'Client/game_de/CAnimBlendSequenceDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeAnimationSequenceName.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlendHierarchyDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeClassicAnimationNameHash.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyController.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlockDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimManagerDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlendAssocGroupDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlendStaticAssociationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlendAssociationDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeAnimationAssociationController.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeAnimationClumpInitializationPlan.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeAnimationAssociationClumpVerification.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeAnimationAssociationClumpOrigin.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeTemporaryDynamicAssociationInitializeCalls.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeModelHashReader.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CAnimBlockRequestDE.cpp'),
        # RequestDE's inherited dependency getter and creator dispatch need
        # the actual base implementation, even in a focused runtime scope.
        (Join-Path $taskRoot 'Client/game_de/CModelInfoDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CModelInfoRequestDE.cpp'),

        (Join-Path $taskRoot 'Client/game_de/CStatsReadDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CStatsDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlan.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideProfile.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideAtomic.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideOwner.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideWin32.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverridePlacement.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideSlot.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBridge.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainBorrowGuard.cpp'),(Join-Path $taskRoot 'Client/game_de/NativeWeatherRainLeaf.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePedReferenceController.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePresentTransitionGate.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeImageAccess.cpp')) +
        $runtimeDependencies + @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib','user32.lib') + $objects
    if($Scope -in @('PrivateHierarchyPostprocess','FramesAndHierarchyPostprocess','BankPostprocess','BlockRegistration','EmptyBlockRemoval')){
        # All selected positive calls execute the real admitted saved body;
        # the same delegate schedules intentional post-return failures only.
        $postHook=Join-Path $scratchRoot 'PrivateHierarchyPostprocessHook.generated.h'
        $postHookText=@'
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace ClockPrivateHierarchyPostProcessFixture {
 mta::game_de::ClockResult Invoke(void(*)(std::uintptr_t),std::uintptr_t,std::uint32_t&) noexcept;
}
'@
        [IO.File]::WriteAllText($postHook,$postHookText,[Text.UTF8Encoding]::new($false))
        $postEvidence=Join-Path $outputRoot 'PrivateHierarchyPostprocessHook.generated.h'
        Copy-Item -LiteralPath $postHook -Destination $postEvidence
        $record['privateHierarchyPostprocessInvokerFixture']=[ordered]@{path=$postEvidence;sha256=(Sha $postEvidence);productionInvoker='PrivateAnimationHierarchyDetail::NativePostProcessCall';production=(Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp');productionSha256=(Sha (Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp'));profileSha256=(Sha (Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyPostProcessProfile.h'))}
        $arguments=@('/DMTA_DE_PRIVATE_ANIMATION_HIERARCHY_POSTPROCESS_INVOKE=::ClockPrivateHierarchyPostProcessFixture::Invoke',('/FI'+$postHook))+$arguments
    }
    if($Scope -eq 'PrivateHierarchyEmptyOperations'){
        # Keep the actual empty-store implementation linked once and observe
        # only its completed guarded CPU stores and post-acknowledgement loss.
        $emptyHook=Join-Path $scratchRoot 'PrivateHierarchyEmptyHook.generated.h'
        $emptyHookText=@'
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace ClockPrivateHierarchyEmptyWriteFixture {
 mta::game_de::ClockResult Write(std::uintptr_t,std::uint32_t&) noexcept;
}
'@
        [IO.File]::WriteAllText($emptyHook,$emptyHookText,[Text.UTF8Encoding]::new($false))
        $emptyEvidence=Join-Path $outputRoot 'PrivateHierarchyEmptyHook.generated.h'
        Copy-Item -LiteralPath $emptyHook -Destination $emptyEvidence
        $record['privateHierarchyEmptyWriterFixture']=[ordered]@{path=$emptyEvidence;sha256=(Sha $emptyEvidence);productionWriter='PrivateAnimationHierarchyDetail::ClearEmptySequences';production=(Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp');productionSha256=(Sha (Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp'))}
        $arguments=@('/DMTA_DE_PRIVATE_ANIMATION_HIERARCHY_REMOVE_EMPTY=::ClockPrivateHierarchyEmptyWriteFixture::Write',('/FI'+$emptyHook))+$arguments
    }
    if($Scope -eq 'PrivateHierarchyParent'){
        # The normal production storage TU stays linked once. Only its final
        # guarded CPU pointer writer is observed; real epoch/ACK code runs.
        $parentHook=Join-Path $scratchRoot 'PrivateHierarchyParentHook.generated.h'
        $parentHookText=@'
#pragma once
#include <game/CGameClockService.h>
#include <cstdint>
namespace ClockPrivateHierarchyParentWriteFixture {
 mta::game_de::ClockResult Write(std::uintptr_t,std::uintptr_t,std::uint32_t&) noexcept;
}
'@
        [IO.File]::WriteAllText($parentHook,$parentHookText,[Text.UTF8Encoding]::new($false))
        $parentEvidence=Join-Path $outputRoot 'PrivateHierarchyParentHook.generated.h'
        Copy-Item -LiteralPath $parentHook -Destination $parentEvidence
        $record['privateHierarchyParentWriterFixture']=[ordered]@{path=$parentEvidence;sha256=(Sha $parentEvidence);productionWriter='PrivateAnimationHierarchyDetail::WriteSequences';production=(Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp');productionSha256=(Sha (Join-Path $taskRoot 'Client/game_de/NativePrivateAnimationHierarchyStorage.cpp'))}
        $arguments=@('/DMTA_DE_PRIVATE_ANIMATION_HIERARCHY_SEQUENCES_WRITE=::ClockPrivateHierarchyParentWriteFixture::Write',('/FI'+$parentHook))+$arguments
    }
    if ($Scope -in @('CustomAnimationCleanup','StandaloneSequenceOperations')) { $arguments=@('/DMTA_TEST_CUSTOM_ANIMATION_CLEANUP')+$arguments }
    if($Scope -eq 'PresentTransitionClock'){
        # Compile the unmodified production weather platform once inside
        # the ordinary runtime fixture under its narrow OS flush seam.
        $arguments=@('/DMTA_TEST_PRESENT_TRANSITION_CLOCK')+@($arguments | Where-Object { $_ -cne (Join-Path $taskRoot 'Client/game_de/NativeWeatherOverrideWin32.cpp') -and $_ -cne (Join-Path $taskRoot 'Client/game_de/NativePresentTransitionGate.cpp') })
        $record['presentTransitionWeatherPlatformOnceWithOsFlushSeam']=$true
        $record['presentTransitionGateOnceWithOwnedModuleSeam']=$true
    }
    [ordered]@{compiler=$record.compiler;detoursArguments=$detoursArguments;arguments=$arguments} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8NoBOM
    # Every selected scope compiles the cumulative fixture/native source graph.
    # Its measured build exceeded 60s without a diagnostic. Extend only the
    # compiler allowance; executable test budgets and compiler flags stay fixed.
    $record['compileDeadlineMs']=120000
    $record.compileRuns += RunBounded $compiler $arguments 'compile' 120000
    if ($record.compileRuns[-1].exitCode -ne 0 -or $record.compileRuns[-1].timedOut) { throw 'Clock SDK/runtime fixture compilation failed.' }
    # The cumulative callback fixture includes current owner and retirement cases.
    # Keep it bounded while allowing the full set to finish on a busy build host.
    # This new isolated readiness child leaves genuine retained journals for
    # process retirement; keep its large synthetic PE in owned disposable scratch.
    # Preserve the argument vector: an if result with one item becomes a
    # scalar string, so later += would append the switch to the path.
    $fixtureArguments = @(if ($Scope -in @('AnimationAssociationGroupChildProducts','AnimationAssociationGroupOffset','StaticAssociationInitialization','StartupPreparationReadiness','StartupNumericGarages','StartupStreamingReceiver','StartupInitializationReadiness','PresentTransitionClock')) { $scratchRoot } else { $outputRoot })
    if ($Scope -eq 'ModelTimeGetter') { $fixtureArguments += '--model-time-getter' }
    if ($Scope -eq 'VehiclePredicates') { $fixtureArguments += '--vehicle-predicates' }
    if ($Scope -eq 'PresentTransitionClock') { $fixtureArguments += '--present-transition-clock' }
    if ($Scope -eq 'StartupInitializationReadiness') { $fixtureArguments += '--startup-initialization-readiness' }
    if ($Scope -eq 'StartupStreamingReceiver') { $fixtureArguments += '--startup-streaming-receiver' }
    if ($Scope -eq 'StartupNumericGarages') { $fixtureArguments += '--startup-numeric-garages' }
    if ($Scope -eq 'StartupPreparationReadiness') { $fixtureArguments += '--startup-preparation-readiness' }
    if ($Scope -eq 'AnimationAssociationFloat') { $fixtureArguments += '--animation-association-float' }
    if ($Scope -eq 'PedRender') { $fixtureArguments += '--ped-render' }
    if ($Scope -eq 'WorldJetpack') { $fixtureArguments += '--world-jetpack' }
    if ($Scope -eq 'WorldArea') { $fixtureArguments += '--world-area' }
    if ($Scope -eq 'WorldQueries') { $fixtureArguments += '--world-queries' }
    if ($Scope -eq 'WorldLine') { $fixtureArguments += '--world-line' }
    if ($Scope -eq 'WorldGround') { $fixtureArguments += '--world-ground' }
    if ($Scope -eq 'ConstructorServices') { $fixtureArguments += '--constructor-services' }
    if ($Scope -eq 'AnimationStaticAssociationReads') { $fixtureArguments += '--animation-static-association-reads' }
    if ($Scope -eq 'AnimationAssociationGroupReads') { $fixtureArguments += '--animation-association-group-reads' }
    if ($Scope -eq 'AnimationAssociationGroupChildProducts') { $fixtureArguments += '--animation-association-group-child-products' }
    if ($Scope -eq 'AnimationAssociationGroupOffset') { $fixtureArguments += '--animation-association-group-offset-success' }
    if ($Scope -eq 'AnimationIdValidity') { $fixtureArguments += '--animation-id-validity' }
    if ($Scope -eq 'AnimationGroupValidity') { $fixtureArguments += '--animation-group-validity' }
    if ($Scope -eq 'AnimationDefinitionBlockNames') { $fixtureArguments += '--animation-definition-block-names' }
    if ($Scope -eq 'AnimationDefinitions') { $fixtureArguments += '--animation-definitions' }
    if ($Scope -eq 'AnimationBlockHashFactory') { $fixtureArguments += '--animation-block-hash-factory' }
    if ($Scope -eq 'AnimationBankFactories') { $fixtureArguments += '--animation-bank-factory' }
    if ($Scope -eq 'PrivateAnimationFactories') { $fixtureArguments += '--private-animation-factories' }
    if ($Scope -eq 'FreshUncompressedSequence') { $fixtureArguments += '--fresh-uncompressed-sequence' }
    if ($Scope -eq 'FreshCompressedSequence') { $fixtureArguments += '--fresh-compressed-sequence' }
    if ($Scope -eq 'CustomAnimationSequenceName') { $fixtureArguments += '--custom-animation-sequence-name' }
    if ($Scope -eq 'AnimationHierarchyBank') { $fixtureArguments += '--animation-hierarchy-bank' }
    if ($Scope -eq 'CustomAnimationSequenceCopyFrame') { $fixtureArguments += '--custom-animation-sequence-copy-frame' }
    if ($Scope -eq 'CustomAnimationSequence') { $fixtureArguments += '--custom-animation-sequence' }
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
    if($Scope -eq 'CameraBasics'){
        # Compile once and isolate genuine entered debt in fresh processes;
        # native camera/reference journals are never reset between scenarios.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        foreach($scenario in @('success','entered-before','entered-after','owner-loss','effect-mismatch','sphere-regression')){
            $scenarioRoot=Join-Path $scratchRoot ('camera-basics-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Camera basics execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Camera basics process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            # The prior module-pinned sphere owner requires a pristine host.
            # Reuse this exact binary with its existing CLI in a sixth process.
            $flag=if($scenario -eq 'sphere-regression'){'--camera-sphere-visible'}else{'--camera-basics-'+$scenario}
            $run=RunBounded $copy @($scenarioRoot,$flag) ('test-camera-basics-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Camera basics '+$scenario+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Camera basics execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        if($record.testRuns.Count -ne 6){throw 'Camera basics requires five camera lifetimes and one independent sphere lifetime'}
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'StartupNativeMenuLifecycle'){
        # Compile the actual runtime once; an unresolved native effect keeps
        # its real journal until the corresponding isolated child exits.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','no-widget','unknown-before','unknown-after','post-owner-loss')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-startup-menu-runtime-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Existing native menu runtime evidence must not be overwritten'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            if((Sha $scenarioExecutable) -cne (Sha $executable)){throw 'Native menu scenario differs from once-compiled actual runtime fixture'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=(Sha $scenarioExecutable);directory=$scenarioRoot}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--startup-menu-runtime-'+$scenario)) ('test-startup-menu-runtime-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Native menu runtime lifecycle '+$scenario+' failed')}
            $record.checks += [int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'StartupModelCollisionLifecycle'){
        # Compile once, then preserve each uncertain real runtime journal in
        # its own bounded child; do not reset it for a subsequent scenario.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','unknown-before','unknown-after','post-owner-loss')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-startup-collision-runtime-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Existing collision runtime evidence must not be overwritten'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            if((Sha $scenarioExecutable) -cne (Sha $executable)){throw 'Runtime scenario executable differs from once-compiled fixture'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=(Sha $scenarioExecutable);directory=$scenarioRoot}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--startup-collision-runtime-'+$scenario)) ('test-startup-collision-runtime-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Startup collision Runtime lifecycle '+$scenario+' failed')}
            $record.checks += [int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'PedModelCreationLifecycle'){
        # Published and unknown records cannot be reset to fit a following case.
        # Compile the actual runtime once and isolate only these four new cases.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('written','read-only','unknown-allocation','unpublished')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-ped-model-runtime-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Existing lifecycle evidence must not be overwritten'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            if((Sha $scenarioExecutable) -cne (Sha $executable)){throw 'Lifecycle child differs from the once-compiled actual runtime fixture'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=(Sha $scenarioExecutable);directory=$scenarioRoot}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--ped-model-creation-runtime-'+$scenario)) ('test-ped-model-runtime-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Ped metadata Runtime lifecycle '+$scenario+' failed')}
            $record.checks += [int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationAssociationNodeArray'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $selected=@(
            @{name='success';flag='--animation-association-node-array-success'},
            @{name='unknown-free';flag='--animation-association-node-array-unknown-free'},
            @{name='unknown-clear';flag='--animation-association-node-array-unknown-clear'},
            @{name='clear-owner-loss';flag='--animation-association-node-array-clear-owner-loss'})
        foreach($scenario in $selected){
            $scenarioRoot=Join-Path $outputRoot ('scenario-animation-association-node-array-'+$scenario.name)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Association node-array scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Association node-array scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario.name;flag=$scenario.flag;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,$scenario.flag) ('test-animation-association-node-array-'+$scenario.name) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Association node-array '+$scenario.name+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # Retain the exact original executable/hash and durable logs.
                # Delete only this verified task-created identical payload.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Association node-array scenario ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationAssociationProgress'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $selected=@(
            @{name='success';flag='--animation-association-progress-success'},
            @{name='unknown';flag='--animation-association-progress-unknown'})
        foreach($scenario in $selected){
            $scenarioRoot=Join-Path $outputRoot ('scenario-animation-association-progress-'+$scenario.name)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Association progress scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Association progress scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario.name;flag=$scenario.flag;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,$scenario.flag) ('test-animation-association-progress-'+$scenario.name) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Association progress '+$scenario.name+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # Retain the exact original executable/hash and durable logs.
                # Delete only this verified task-created identical payload.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Association progress scenario ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationAssociationBones'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $selected=@(
            @{name='success';flag='--animation-association-bones-success'},
            @{name='post-loss';flag='--animation-association-bones-post-loss'},
            @{name='unknown-write';flag='--animation-association-bones-unknown-write'})
        foreach($scenario in $selected){
            $scenarioRoot=Join-Path $outputRoot ('scenario-animation-association-bones-'+$scenario.name)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Association bones scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Association bones scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario.name;flag=$scenario.flag;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,$scenario.flag) ('test-animation-association-bones-'+$scenario.name) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Association bones '+$scenario.name+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # Retain the exact original executable/hash and durable logs.
                # Delete only this verified task-created identical payload.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Association bones scenario ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'PrivateAnimationAssociations'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $selected=@(
            @{name='success';flag='--private-animation-associations-success'},
            @{name='constructor-unknown';flag='--private-animation-associations-constructor-unknown'},
            @{name='destructor-unknown';flag='--private-animation-associations-destructor-unknown'},
            @{name='retire-owner-loss';flag='--private-animation-associations-retire-owner-loss'},
            @{name='storage-success';flag='--custom-sequence-storage-success'},
            @{name='storage-unknown-allocate';flag='--custom-sequence-storage-unknown-allocate'},
            @{name='storage-unknown-free';flag='--custom-sequence-storage-unknown-free'})
        foreach($scenario in $selected){
            $scenarioRoot=Join-Path $outputRoot ('scenario-private-animation-associations-'+$scenario.name)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Private association scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Private association scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario.name;flag=$scenario.flag;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,$scenario.flag) ('test-private-animation-associations-'+$scenario.name) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private association '+$scenario.name+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # Retain the exact original executable/hash and durable logs.
                # Delete only this verified task-created identical payload.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Private association scenario ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationAssociationGroupOffset'){
        # Reuse the one exact compiled binary. Module-pinned callback owners
        # cannot start a second pristine lifetime in the same fixture process.
        # All execution copies and synthetic images belong to task scratch.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $compiledHash=Sha $executable
        foreach($scenario in @('success','lost-ack','conflict')){
            $scenarioRoot=Join-Path $scratchRoot ('association-group-offset-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Group offset scenario execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne $compiledHash){throw 'Group offset scenario differs from the once-compiled exact executable'}
            $record.testScenarioExecutables+=@([ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;removedWithOwnedScratch=$true})
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--animation-association-group-offset-'+$scenario)) ('test-association-group-offset-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){
                throw ('Group offset '+$scenario+' failed')
            }
            if((Sha $scenarioExecutable) -cne $compiledHash){throw 'Group offset execution copy changed while running'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'ApplicationTiming'){
        # One exact binary runs fresh native-debt lifetimes in task-owned
        # directories; no process can reset an ambiguous entered operation.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        foreach($scenario in @('success','restore-refusal','throw','regression')){
            $scenarioRoot=Join-Path $scratchRoot ('application-timing-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Application timing execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Application timing process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,('--application-timing-'+$scenario)) ('test-application-timing-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Application timing '+$scenario+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Application timing execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -in @('BlockRegistration','EmptyBlockRemoval')){
        # The one compiled binary covers new actual post-frame conversions and
        # all37 component regressions and the existing private success owner, each in a fresh host.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        $cases=@()
        if($Scope -eq 'EmptyBlockRemoval'){
            foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','partial','owner-loss','group-before','group-after','group-drift','effect-mismatch')){
                $cases+=@{key=('empty-block-removal-'+$scenario);cli=('--empty-block-removal-'+$scenario)}
            }
        }
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','partial','owner-loss','mismatch','count-change','prefix-change','group-change')){
            $cases+=@{key=('block-registration-'+$scenario);cli=('--block-registration-'+$scenario)}
        }
        foreach($scenario in @('quaternion-success','time-success','refusals','timer-owner','quaternion-before','quaternion-after','time-before','time-after','quaternion-owner','time-owner','quaternion-mismatch','time-mismatch')){
            $cases+=@{key=('bank-postprocess-'+$scenario);cli=('--bank-postprocess-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','late-owner','victim-mismatch','target-mismatch','victim-fallback','target-fallback','victim-no-output','target-no-output','cache-mismatch')){
            $cases+=@{key=('bank-eviction-'+$scenario);cli=('--bank-eviction-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','late-owner','effect-mismatch','allocation-fallback','no-output')){
            $cases+=@{key=('bank-decompression-'+$scenario);cli=('--bank-decompression-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','entered-before','entered-after','late-owner','effect-mismatch','cache-before','cache-after','cache-mismatch','remove-success','remove-before','remove-after','remove-mismatch','remove-partial','remove-owner')){
            $cases+=@{key=('hierarchy-uncompress-'+$scenario);cli=('--hierarchy-uncompress-'+$scenario)}
        }
        # Reuse the existing actual private SDK success case in this binary.
        # Runtime's new bank dispatch must preserve the private owner branch.
        $cases+=@{key='private-hierarchy-postprocess-success';cli='--private-hierarchy-postprocess-success'}
        foreach($case in $cases){
            $scenarioRoot=Join-Path $scratchRoot $case.key
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Block registration execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Block registration process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$case.key;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,$case.cli) ('test-'+$case.key) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Block registration '+$case.key+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Block registration execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'BankPostprocess'){
        # The one compiled binary covers new actual post-frame conversions and
        # all37 component regressions and the existing private success owner, each in a fresh host.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        $cases=@()
        foreach($scenario in @('quaternion-success','time-success','refusals','timer-owner','quaternion-before','quaternion-after','time-before','time-after','quaternion-owner','time-owner','quaternion-mismatch','time-mismatch')){
            $cases+=@{key=('bank-postprocess-'+$scenario);cli=('--bank-postprocess-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','late-owner','victim-mismatch','target-mismatch','victim-fallback','target-fallback','victim-no-output','target-no-output','cache-mismatch')){
            $cases+=@{key=('bank-eviction-'+$scenario);cli=('--bank-eviction-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','late-owner','effect-mismatch','allocation-fallback','no-output')){
            $cases+=@{key=('bank-decompression-'+$scenario);cli=('--bank-decompression-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','entered-before','entered-after','late-owner','effect-mismatch','cache-before','cache-after','cache-mismatch','remove-success','remove-before','remove-after','remove-mismatch','remove-partial','remove-owner')){
            $cases+=@{key=('hierarchy-uncompress-'+$scenario);cli=('--hierarchy-uncompress-'+$scenario)}
        }
        # Reuse the existing actual private SDK success case in this binary.
        # Runtime's new bank dispatch must preserve the private owner branch.
        $cases+=@{key='private-hierarchy-postprocess-success';cli='--private-hierarchy-postprocess-success'}
        foreach($case in $cases){
            $scenarioRoot=Join-Path $scratchRoot $case.key
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Bank postprocess execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Bank postprocess process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$case.key;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,$case.cli) ('test-'+$case.key) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Bank postprocess '+$case.key+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Bank postprocess execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'BankEviction'){
        # The one compiled binary covers new actual post-frame conversions and
        # all24 retained component regressions, each in a fresh ordinary host.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        $cases=@()
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','late-owner','victim-mismatch','target-mismatch','victim-fallback','target-fallback','victim-no-output','target-no-output','cache-mismatch')){
            $cases+=@{key=('bank-eviction-'+$scenario);cli=('--bank-eviction-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','late-owner','effect-mismatch','allocation-fallback','no-output')){
            $cases+=@{key=('bank-decompression-'+$scenario);cli=('--bank-decompression-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','entered-before','entered-after','late-owner','effect-mismatch','cache-before','cache-after','cache-mismatch','remove-success','remove-before','remove-after','remove-mismatch','remove-partial','remove-owner')){
            $cases+=@{key=('hierarchy-uncompress-'+$scenario);cli=('--hierarchy-uncompress-'+$scenario)}
        }
        foreach($case in $cases){
            $scenarioRoot=Join-Path $scratchRoot $case.key
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Bank eviction execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Bank eviction process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$case.key;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,$case.cli) ('test-'+$case.key) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Bank eviction '+$case.key+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Bank eviction execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'BankDecompression'){
        # The one compiled binary covers new actual post-frame conversions and
        # all15 retained component regressions, each in a fresh ordinary host.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        $cases=@()
        foreach($scenario in @('success','refusals','timer-owner','entered-before','entered-after','late-owner','effect-mismatch','allocation-fallback','no-output')){
            $cases+=@{key=('bank-decompression-'+$scenario);cli=('--bank-decompression-'+$scenario)}
        }
        foreach($scenario in @('success','refusals','entered-before','entered-after','late-owner','effect-mismatch','cache-before','cache-after','cache-mismatch','remove-success','remove-before','remove-after','remove-mismatch','remove-partial','remove-owner')){
            $cases+=@{key=('hierarchy-uncompress-'+$scenario);cli=('--hierarchy-uncompress-'+$scenario)}
        }
        foreach($case in $cases){
            $scenarioRoot=Join-Path $scratchRoot $case.key
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Bank decompression execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Bank decompression process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$case.key;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,$case.cli) ('test-'+$case.key) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Bank decompression '+$case.key+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Bank decompression execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationHierarchyUncompress'){
        # One exact binary runs fresh native-debt lifetimes in task-owned
        # directories; no process can reset an ambiguous entered operation.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        foreach($scenario in @('success','refusals','entered-before','entered-after','late-owner','effect-mismatch','cache-before','cache-after','cache-mismatch','remove-success','remove-before','remove-after','remove-mismatch','remove-partial','remove-owner')){
            $scenarioRoot=Join-Path $scratchRoot ('hierarchy-uncompress-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Hierarchy Uncompress execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Hierarchy Uncompress process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,('--hierarchy-uncompress-'+$scenario)) ('test-hierarchy-uncompress-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Hierarchy Uncompress '+$scenario+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Hierarchy Uncompress execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'NumericAssociationOwner'){
        # Reuse one actual binary in fresh debt/lifetime processes. The exact
        # executable's parent is also its explicit synthetic-image directory.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        foreach($scenario in @('success','null','entered-unknown','late-owner','closed','close-during','foreign','cpu-first')){
            $scenarioRoot=Join-Path $scratchRoot ('numeric-association-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Numeric association execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Numeric association process differs from compiled binary'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,('--numeric-association-'+$scenario)) ('test-numeric-association-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Numeric association '+$scenario+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Numeric association execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'DynamicClumpOwner'){
        # One build, fresh exact-binary processes for native/GC lifetime debt.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@();$compiledHash=Sha $executable
        foreach($scenario in @('success','zero','copy-ref','private-h','original-unknown','defaults-unknown','owner-loss','private-h-unknown','temporary','static-public','static-joint')){
            $scenarioRoot=Join-Path $scratchRoot ('dynamic-clump-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $copy=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $copy){throw 'Dynamic clump execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $copy
            if((Sha $copy) -cne $compiledHash){throw 'Dynamic clump process differs from once-compiled actual binary'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$copy;sha256=$compiledHash;removedWithOwnedScratch=$true}
            $run=RunBounded $copy @($scenarioRoot,('--dynamic-clump-'+$scenario)) ('test-dynamic-clump-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Dynamic clump '+$scenario+' failed')}
            if((Sha $copy) -cne $compiledHash){throw 'Dynamic clump execution copy changed'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationAssociationGroupFactories'){
        $record.testRun=RunBounded $executable @($outputRoot,'--animation-association-group-factories') 'test-group-factories' 60000
        $text=[IO.File]::ReadAllText($record.testRun.stdout)+[IO.File]::ReadAllText($record.testRun.stderr)
        if($record.testRun.exitCode -ne 0 -or $record.testRun.timedOut -or $text -notmatch 'Passed (\d+) production clock service checks'){throw 'Actual association group factories failed'}
        $record.checks=[int]$Matches[1]
    } elseif($Scope -eq 'GroupSourceCreationOwner'){
        # One compiled binary, pristine processes for actual retained debt.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $compiledHash=Sha $executable
        foreach($scenario in @('success','child-rejected','initialize-unknown','initialize-owner-loss','word-unknown')){
            $scenarioRoot=Join-Path $scratchRoot ('group-source-creation-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Group source creation scenario copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne $compiledHash){throw 'Group source creation scenario differs from once-compiled exact binary'}
            $record.testScenarioExecutables+=@([ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;removedWithOwnedScratch=$true})
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--group-source-creation-'+$scenario)) ('test-group-source-creation-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Group source creation '+$scenario+' failed')}
            if((Sha $scenarioExecutable) -cne $compiledHash){throw 'Group source creation execution copy changed while running'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'JointTemporaryStaticInitializerOwner'){
        # Compile once; each real module-pinned lifetime keeps pristine host
        # admission. Unknown native effects are never reset for another case.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $compiledHash=Sha $executable
        foreach($scenario in @('success','entered-unknown','owner-loss','group-drift','public-ped-regression')){
            $scenarioRoot=Join-Path $scratchRoot ('joint-temporary-static-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Joint temporary static scenario copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne $compiledHash){throw 'Joint temporary static scenario differs from once-compiled exact binary'}
            $record.testScenarioExecutables+=@([ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;removedWithOwnedScratch=$true})
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--joint-temporary-static-'+$scenario)) ('test-joint-temporary-static-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Joint temporary static '+$scenario+' failed')}
            if((Sha $scenarioExecutable) -cne $compiledHash){throw 'Joint temporary static execution copy changed while running'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'GroupReplacementOwner'){
        # Compile once; each real module-pinned lifetime keeps pristine host
        # admission. Unknown native effects are never reset for another case.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $compiledHash=Sha $executable
        foreach($scenario in @('success','child-rejected','child-unknown','child-owner-loss','retire-unknown')){
            $scenarioRoot=Join-Path $scratchRoot ('group-replacement-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Group replacement scenario copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne $compiledHash){throw 'Group replacement scenario differs from once-compiled exact binary'}
            $record.testScenarioExecutables+=@([ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;removedWithOwnedScratch=$true})
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--group-replacement-'+$scenario)) ('test-group-replacement-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Group replacement '+$scenario+' failed')}
            if((Sha $scenarioExecutable) -cne $compiledHash){throw 'Group replacement execution copy changed while running'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'GroupCopyAnimationOwner'){
        # Compile once; each real module-pinned lifetime keeps pristine host
        # admission. Unknown native effects are never reset for another case.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $compiledHash=Sha $executable
        foreach($scenario in @('success','null-owner-loss','bad-layout','bad-ref','unknown','owner-loss','source-drift')){
            $scenarioRoot=Join-Path $scratchRoot ('group-copy-animation-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Group copy animation scenario copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne $compiledHash){throw 'Group copy animation scenario differs from once-compiled exact binary'}
            $record.testScenarioExecutables+=@([ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;removedWithOwnedScratch=$true})
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--group-copy-animation-'+$scenario)) ('test-group-copy-animation-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Group copy animation '+$scenario+' failed')}
            if((Sha $scenarioExecutable) -cne $compiledHash){throw 'Group copy animation execution copy changed while running'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'GroupEmptyInitialization'){
        # Compile once; each real module-pinned lifetime keeps pristine host
        # admission. Unknown native effects are never reset for another case.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $compiledHash=Sha $executable
        foreach($scenario in @('success','unknown')){
            $scenarioRoot=Join-Path $scratchRoot ('group-empty-initialization-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Group empty initializer scenario copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne $compiledHash){throw 'Group empty initializer scenario differs from once-compiled exact binary'}
            $record.testScenarioExecutables+=@([ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;removedWithOwnedScratch=$true})
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--group-empty-initialization-'+$scenario)) ('test-group-empty-initialization-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Group empty initializer '+$scenario+' failed')}
            if((Sha $scenarioExecutable) -cne $compiledHash){throw 'Group empty initializer execution copy changed while running'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'StaticAssociationInitialization'){
        # Compile once, then isolate accepted retirement and entered-unknown
        # lifetimes. Do not reset sticky module pins or actual native debt to
        # let BeginOwned's pristine wrong-executable assertions pass again.
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $compiledHash=Sha $executable
        foreach($scenario in @('success','unknown')){
            $scenarioRoot=Join-Path $scratchRoot ('static-initialization-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Static initialization scenario execution copy already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne $compiledHash){throw 'Static initialization scenario differs from the once-compiled exact executable'}
            $record.testScenarioExecutables+=@([ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;removedWithOwnedScratch=$true})
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--static-association-initialization-'+$scenario)) ('test-static-initialization-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){
                throw ('Static initialization '+$scenario+' failed')
            }
            if((Sha $scenarioExecutable) -cne $compiledHash){throw 'Static initialization execution copy changed while running'}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationStaticAssociationWrites'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','conflict','lost-ack','unknown')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-animation-static-association-writes-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Static association write scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Static association write scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,('--animation-static-association-writes-'+$scenario)) ('test-animation-static-association-writes-'+$scenario) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Static association write '+$scenario+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # The original executable and exact hash remain durable. Only
                # this task-created identical animation-static-association-writes scenario payload is disposable.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Static association write scenario executable ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'AnimationManagerCache'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','foreign')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-animation-manager-cache-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Animation manager cache scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Animation manager cache scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,('--animation-manager-cache-'+$scenario)) ('test-animation-manager-cache-'+$scenario) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Animation manager cache '+$scenario+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # The original executable and exact hash remain durable. Only
                # this task-created identical animation-manager-cache scenario payload is disposable.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Animation manager cache scenario executable ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'SequenceInterfaceCompatibility'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','unknown')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-sequence-interface-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Sequence interface scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Sequence interface scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,('--sequence-interface-'+$scenario)) ('test-sequence-interface-'+$scenario) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Sequence interface '+$scenario+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # The original executable and exact hash remain durable. Only
                # this task-created identical sequence-interface scenario payload is disposable.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Sequence interface scenario executable ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'StandaloneSequenceOperations'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','bind-loss','copy-loss','unknown','foreign-drop')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-standalone-sequence-operations-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Standalone scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Standalone scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,('--standalone-sequence-operations-'+$scenario)) ('test-standalone-sequence-operations-'+$scenario) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Standalone sequence '+$scenario+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # The original executable and exact hash remain durable. Only
                # this task-created identical standalone scenario payload is disposable.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Standalone scenario executable ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'CustomAnimationCleanup'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','returned-loss','unknown','foreign-drop')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-custom-animation-cleanup-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Cleanup scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Cleanup scenario executable changed'}
            $entry=[ordered]@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash;copyRetired=$false}
            $record.testScenarioExecutables+=@($entry)
            try{
                $run=RunBounded $scenarioExecutable @($scenarioRoot,('--custom-animation-cleanup-'+$scenario)) ('test-custom-animation-cleanup-'+$scenario) 60000
                $record.testRuns+=$run
                $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
                if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private cleanup '+$scenario+' failed')}
                $record.checks+=[int]$Matches[1];$texts+=$part
            }finally{
                # The original executable and exact hash remain durable. Only
                # this task-created identical scenario payload is disposable.
                $expected=(Resolve-Path -LiteralPath $scenarioRoot).Path.TrimEnd('\')+'\'
                $resolved=(Resolve-Path -LiteralPath $scenarioExecutable).Path
                if(-not $resolved.StartsWith($expected,[StringComparison]::OrdinalIgnoreCase) -or (Sha $scenarioExecutable) -cne $scenarioHash){throw 'Cleanup scenario executable ownership changed'}
                Remove-Item -LiteralPath $scenarioExecutable -Force
                $entry.copyRetired=$true
            }
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -in @('PrivateHierarchyPostprocess','FramesAndHierarchyPostprocess')){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        $scenarios=@('success','unknown','lostack')
        if($Scope -eq 'FramesAndHierarchyPostprocess'){$scenarios=@('fresh')+$scenarios}
        foreach($scenario in $scenarios){
            $scenarioRoot=Join-Path $outputRoot ('scenario-private-hierarchy-postprocess-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Postprocess evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Postprocess scenario executable changed'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash}
            $selector=if($scenario -eq 'fresh'){'--fresh-uncompressed-sequence'}else{'--private-hierarchy-postprocess-'+$scenario}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,$selector) ('test-private-hierarchy-postprocess-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private postprocess '+$scenario+' failed')}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'PrivateHierarchyEmptyOperations'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','accepted-loss','unknown-count','foreign-cache')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-private-hierarchy-empty-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Empty scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Empty scenario executable changed'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--private-hierarchy-empty-'+$scenario)) ('test-private-hierarchy-empty-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private empty '+$scenario+' failed')}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'PrivateHierarchyParent'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','lost-detach','unknown-attach','foreign-pointer','foreign-cache','published')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-private-hierarchy-parent-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Parent scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Parent scenario executable changed'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--private-hierarchy-parent-'+$scenario)) ('test-private-hierarchy-parent-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private parent '+$scenario+' failed')}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'PrivateHierarchyName'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','pinned')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-private-hierarchy-name-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Name scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Name scenario executable changed'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--private-hierarchy-name-'+$scenario)) ('test-private-hierarchy-name-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private name '+$scenario+' failed')}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'PrivateHierarchyScalars'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','pinned')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-private-hierarchy-scalars-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Scalar scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Scalar scenario executable changed'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--private-hierarchy-scalars-'+$scenario)) ('test-private-hierarchy-scalars-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private scalar '+$scenario+' failed')}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif($Scope -eq 'PrivateHierarchy'){
        $record['testRuns']=@();$record['testScenarioExecutables']=@();$texts=@()
        foreach($scenario in @('success','published','pointer','cache')){
            $scenarioRoot=Join-Path $outputRoot ('scenario-private-hierarchy-'+$scenario)
            [IO.Directory]::CreateDirectory($scenarioRoot)|Out-Null
            $scenarioExecutable=Join-Path $scenarioRoot ([IO.Path]::GetFileName($executable))
            if(Test-Path -LiteralPath $scenarioExecutable){throw 'Private hierarchy scenario evidence already exists'}
            Copy-Item -LiteralPath $executable -Destination $scenarioExecutable
            $scenarioHash=Sha $scenarioExecutable
            if($scenarioHash -cne (Sha $executable)){throw 'Private hierarchy scenario executable changed'}
            $record.testScenarioExecutables+=@{scenario=$scenario;path=$scenarioExecutable;sha256=$scenarioHash}
            $run=RunBounded $scenarioExecutable @($scenarioRoot,('--private-hierarchy-'+$scenario)) ('test-private-hierarchy-'+$scenario) 60000
            $record.testRuns+=$run
            $part=[IO.File]::ReadAllText($run.stdout)+[IO.File]::ReadAllText($run.stderr)
            if($run.exitCode -ne 0 -or $run.timedOut -or $part -notmatch 'Passed (\d+) production clock service checks'){throw ('Private hierarchy '+$scenario+' failed')}
            $record.checks+=[int]$Matches[1];$texts+=$part
        }
        $record.testRun=$record.testRuns[-1];$text=$texts -join [Environment]::NewLine
    } elseif ($Scope -eq 'CustomAnimationResources') {
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
        $testExecutable = $executable
    if ($Scope -in @('AnimationAssociationGroupChildProducts','AnimationAssociationGroupOffset','StaticAssociationInitialization','StartupPreparationReadiness','StartupNumericGarages','StartupStreamingReceiver','StartupInitializationReadiness','PresentTransitionClock')) {
        # The fixture's existing path guard requires its backing-image directory
        # to equal its executable directory. Run an exact owned scratch copy so
        # that guard stays intact and neither retained synthetic PE survives.
        $testExecutable = Join-Path $scratchRoot ([IO.Path]::GetFileName($executable))
        if (Test-Path -LiteralPath $testExecutable) { throw 'Fresh readiness execution copy already exists.' }
        Copy-Item -LiteralPath $executable -Destination $testExecutable
        if ((Sha $testExecutable) -cne (Sha $executable)) { throw 'Readiness execution copy differs from the once-compiled actual runtime fixture.' }
        $record['readinessExecutionCopy'] = [ordered]@{path=$testExecutable;sha256=(Sha $testExecutable);removedWithOwnedScratch=$true}
    }
    $record.testRun = RunBounded $testExecutable $fixtureArguments 'test' 60000
        $text = [IO.File]::ReadAllText($record.testRun.stdout) + [IO.File]::ReadAllText($record.testRun.stderr)
        if ($text -match 'Passed (\d+) production clock service checks') { $record.checks = [int]$Matches[1] }
        if ($record.testRun.exitCode -ne 0 -or $record.testRun.timedOut -or $record.checks -eq 0) { throw 'Clock service fixture failed.' }
    }
    if($Scope -eq 'PresentTransitionClock'){
        $record.nativePresentTransitionPeerHoldReaderExclusionTested=$text.Contains('Production present transition actual peer hold and reader exclusion completed')
        $record.nativePresentTransitionPeerFailureRecoveryTested=$text.Contains('Production present transition actual peer suspension recovery completed')
        $record.nativePresentTransitionPatchRecoveryTested=$text.Contains('Production present transition actual patch protection recovery completed')
        $record.nativePresentTransitionWeatherRecoveryTested=$text.Contains('Production present transition actual weather cache recovery completed') -and $text.Contains('Production present transition actual weather cleanup Busy deferred completed')
        $record.nativePresentTransitionWeatherHoldTested=$text.Contains('Production present transition persistent weather and rain entry exclusion completed')
        $record.nativePresentTransitionClockRuntimeTested=$record.nativePresentTransitionPeerHoldReaderExclusionTested -and $record.nativePresentTransitionPeerFailureRecoveryTested -and $record.nativePresentTransitionPatchRecoveryTested -and $record.nativePresentTransitionWeatherRecoveryTested -and $record.nativePresentTransitionWeatherHoldTested -and $text.Contains('Production present transition exact module reference retirement completed') -and $text.Contains('Production present transition clock runtime fixture completed')
        if(-not $record.nativePresentTransitionClockRuntimeTested){throw 'Actual present transition clock owner fixture incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StartupInitializationReadiness'){
        $record.nativeStartupInitializationJointReadinessTested=$text.Contains('Production startup initialization joint readiness actual owner completed')
        $record.nativeStartupInitializationFreshRefusalsTested=$text.Contains('Production startup initialization current refusals and owner bookends completed')
        $record.nativeStartupInitializationBookendRefusalTested=$text.Contains('Production startup initialization current refusals and owner bookends completed')
        $record.nativeStartupInitializationNoEffectsTested=$text.Contains('Production startup initialization readiness no-effects and clean retirement completed')
        $record.nativeStartupInitializationReadinessRuntimeTested=$record.nativeStartupInitializationJointReadinessTested -and $record.nativeStartupInitializationFreshRefusalsTested -and
            $record.nativeStartupInitializationBookendRefusalTested -and $record.nativeStartupInitializationNoEffectsTested -and $text.Contains('Production startup initialization readiness runtime fixture completed')
        if(-not $record.nativeStartupInitializationReadinessRuntimeTested){throw 'Actual joint startup initialization readiness runtime scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StartupStreamingReceiver'){
        $record.nativeStartupStreamingReceiverFreshPublicationTested=$text.Contains('Production startup streaming receiver guards and fresh borrowed publication completed')
        $record.nativeStartupStreamingReceiverNativeDispatchTested=$text.Contains('Production startup streaming receiver actual inherited request and clean retirement completed')
        $record.nativeStartupStreamingReceiverRuntimeTested=$record.nativeStartupStreamingReceiverFreshPublicationTested -and
            $record.nativeStartupStreamingReceiverNativeDispatchTested -and $text.Contains('Production startup streaming receiver runtime fixture completed')
        if(-not $record.nativeStartupStreamingReceiverRuntimeTested){throw 'Actual startup streaming receiver runtime scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StartupNumericGarages'){
        $record.nativeStartupNumericGaragesRuntimeStopTested=$text.Contains('Production startup numeric garages entered failure stopped without replay and independent timing restored')
        $record.nativeStartupNumericGaragesRuntimeTested=$record.nativeStartupNumericGaragesRuntimeStopTested -and
            $text.Contains('Production startup numeric garages actual owner and SDK fixture completed') -and
            $text.Contains('Production startup numeric garages runtime fixture completed')
        if(-not $record.nativeStartupNumericGaragesRuntimeTested){throw 'Actual startup numeric garage runtime scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StartupPreparationReadiness'){
        $record.nativeStartupPreparationReadinessRefusalsPreserveOwner=$text.Contains('Production startup preparation readiness guards and real refusals preserve owner')
        $record.nativeStartupPreparationReadinessPublishedAllowed=$text.Contains('Production startup preparation readiness accepts healthy same-generation publication')
        $record.nativeStartupPreparationReadinessDebtDenied=$text.Contains('Production startup preparation readiness rejects genuine uncertainty without mutating debt')
        $record.nativeStartupPreparationReadinessRuntimeTested=$record.nativeStartupPreparationReadinessRefusalsPreserveOwner -and
            $record.nativeStartupPreparationReadinessPublishedAllowed -and $record.nativeStartupPreparationReadinessDebtDenied -and
            $text.Contains('Production startup preparation readiness runtime fixture completed')
        if(-not $record.nativeStartupPreparationReadinessRuntimeTested){throw 'Actual startup preparation readiness runtime scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StartupNativeMenuLifecycle'){
        $record.nativeStartupMenuRuntimeDebtRetained=$text.Contains('Production startup menu runtime unknown-before retained without replay') -and
            $text.Contains('Production startup menu runtime unknown-after retained without rollback') -and
            $text.Contains('Production startup menu runtime post-owner-loss retained without rewind')
        $record.nativeStartupMenuRuntimeIndependentRestoreTested=$record.nativeStartupMenuRuntimeDebtRetained -and
            $text.Contains('Production startup menu runtime verified close retired') -and $text.Contains('Production startup menu runtime false no-widget retired')
        $record.nativeStartupMenuRuntimeTested=$record.nativeStartupMenuRuntimeIndependentRestoreTested
        if(-not $record.nativeStartupMenuRuntimeTested){throw 'Actual native menu runtime lifecycle scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StartupModelCollisionLifecycle'){
        $record.nativeStartupCollisionRuntimeDebtRetained=$text.Contains('Production startup collision runtime unknown-before retained without replay') -and
            $text.Contains('Production startup collision runtime unknown-after retained without rollback') -and
            $text.Contains('Production startup collision runtime post-owner-loss retained without rewind')
        $record.nativeStartupCollisionRuntimeIndependentRestoreTested=$record.nativeStartupCollisionRuntimeDebtRetained -and
            $text.Contains('Production startup collision runtime verified copy retired without rollback')
        $record.nativeStartupCollisionRuntimeTested=$record.nativeStartupCollisionRuntimeIndependentRestoreTested
        if(-not $record.nativeStartupCollisionRuntimeTested){throw 'Actual startup collision Runtime lifecycle scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PedModelCreationLifecycle'){
        $record.nativePedModelCreationRuntimeNoRewindTested=$text.Contains('Production ped metadata runtime write restoration retained without rewind') -and
            $text.Contains('Production ped metadata runtime read-only restoration retained without rewind') -and
            $text.Contains('Production ped metadata runtime unknown allocation retained without replay')
        $record.nativePedModelCreationRuntimeUnpublishedCleanupTested=$text.Contains('Production ped metadata runtime unpublished cleanup completed')
        $record.nativePedModelCreationRuntimeTested=$record.nativePedModelCreationRuntimeNoRewindTested -and $record.nativePedModelCreationRuntimeUnpublishedCleanupTested
        if(-not $record.nativePedModelCreationRuntimeTested){throw 'New actual Runtime lifecycle scope is incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationNodeArray'){
        $record.nativeAnimationAssociationNodeArrayOwnerTested=$text.Contains('Production native animation association node-array owner fixture completed')
        $record.actualAnimationAssociationNodeArraySdkOwnerTested=$text.Contains('Production actual animation association node-array SDK owner fixture completed')
        $record.animationAssociationNodeArrayUnknownFreeRetained=$text.Contains('Production animation association node-array unknown free retained')
        $record.animationAssociationNodeArrayUnknownClearRetained=$text.Contains('Production animation association node-array unknown clear retained')
        $record.animationAssociationNodeArrayAcceptedClearRecoveryTested=$text.Contains('Production animation association node-array accepted clear recovery completed')
        $record.copiedNativeBodiesExecutedInOwnedFixture=$text.Contains('Production copied constructor allocator destructor and native node-free heap protocol executed in owned fixture')
        if(-not($record.nativeAnimationAssociationNodeArrayOwnerTested -and $record.actualAnimationAssociationNodeArraySdkOwnerTested -and
            $record.animationAssociationNodeArrayUnknownFreeRetained -and $record.animationAssociationNodeArrayUnknownClearRetained -and
            $record.animationAssociationNodeArrayAcceptedClearRecoveryTested -and $record.copiedNativeBodiesExecutedInOwnedFixture)){throw 'Node-array release selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationProgress'){
        $record.nativeAnimationAssociationProgressOwnerTested=$text.Contains('Production native animation association progress owner fixture completed')
        $record.actualAnimationAssociationProgressSdkOwnerTested=$text.Contains('Production actual animation association progress SDK owner fixture completed')
        $record.animationAssociationProgressUnknownRetained=$text.Contains('Production animation association progress unknown effects retained')
        $record.copiedNativeBodiesExecutedInOwnedFixture=$text.Contains('Production copied progress parent children leaf math imports and dynamic lifetime bodies executed in owned fixture')
        if(-not($record.nativeAnimationAssociationProgressOwnerTested -and $record.actualAnimationAssociationProgressSdkOwnerTested -and
            $record.animationAssociationProgressUnknownRetained -and $record.copiedNativeBodiesExecutedInOwnedFixture)){throw 'Animation association progress selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationFloat'){
        $record.nativeAnimationAssociationFloatOwnerTested=$text.Contains('Production native animation association float owner fixture completed')
        $record.actualAnimationAssociationFloatSdkOwnerTested=$text.Contains('Production actual animation association float SDK owner fixture completed')
        if(-not($record.nativeAnimationAssociationFloatOwnerTested -and $record.actualAnimationAssociationFloatSdkOwnerTested)){throw 'Animation association float selected scope incomplete'}
        $record.copiedNativeBodiesExecutedInOwnedFixture=$true
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationBones'){
        $record.nativeAnimationAssociationBonesOwnerTested=$text.Contains('Production native animation association bones owner fixture completed')
        $record.actualAnimationAssociationBonesSdkOwnerTested=$text.Contains('Production actual animation association bones SDK owner fixture completed')
        $record.animationAssociationBonesPostFaultRetained=$text.Contains('Production animation association bones post fault retained')
        $record.animationAssociationBonesAcceptedCleanupTested=$text.Contains('Production animation association bones accepted cleanup completed')
        $record.animationAssociationBonesUnknownWriteRetained=$text.Contains('Production animation association bones unknown write retained')
        $record.copiedNativeBodiesExecutedInOwnedFixture=$text.Contains('Production copied dynamic constructor helper and destructor executed in owned fixture')
        if(-not($record.nativeAnimationAssociationBonesOwnerTested -and $record.actualAnimationAssociationBonesSdkOwnerTested -and
            $record.animationAssociationBonesPostFaultRetained -and $record.animationAssociationBonesAcceptedCleanupTested -and $record.animationAssociationBonesUnknownWriteRetained -and
            $record.copiedNativeBodiesExecutedInOwnedFixture)){throw 'Animation association bones selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PrivateAnimationAssociations'){
        $record.nativePrivateAnimationAssociationOwnerTested=$text.Contains('Production private animation association owner fixture completed')
        $record.actualPrivateAnimationAssociationSdkOwnerTested=$text.Contains('Production actual private animation association SDK fixture completed')
        $record.privateAnimationAssociationConstructorUnknownRetained=$text.Contains('Production private animation association constructor uncertainty retained')
        $record.privateAnimationAssociationDestructorUnknownRetained=$text.Contains('Production private animation association destructor uncertainty retained')
        $record.privateAnimationAssociationRetirementPostAckRetained=$text.Contains('Production private animation association returned destructor reference debt retained')
        $record.copiedNativeBodiesExecutedInOwnedFixture=$text.Contains('Production copied dynamic constructor helper and destructor executed in owned fixture')
        $record.existingCustomSequenceStorageContractTested=$text.Contains('Production native custom sequence storage callback owner fixture completed') -and
            $text.Contains('Production native custom sequence storage natural End fixture completed') -and
            $text.Contains('Production native custom sequence storage unknown allocation lifetime retained') -and
            $text.Contains('Production native custom sequence storage unknown free lifetime retained')
        if(-not($record.nativePrivateAnimationAssociationOwnerTested -and $record.actualPrivateAnimationAssociationSdkOwnerTested -and
            $record.privateAnimationAssociationConstructorUnknownRetained -and $record.privateAnimationAssociationDestructorUnknownRetained -and
            $record.privateAnimationAssociationRetirementPostAckRetained -and $record.copiedNativeBodiesExecutedInOwnedFixture -and
            $record.existingCustomSequenceStorageContractTested)){throw 'Private association and affected allocator contract scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PedRender'){
        $record.nativePedRenderOwnerTested=$text.Contains('Production native ped render owner fixture completed')
        $record.actualPedRenderSdkOwnerTested=$text.Contains('Production actual ped render SDK fixture completed')
        if(-not($record.nativePedRenderOwnerTested -and $record.actualPedRenderSdkOwnerTested)){throw 'Ped render selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StaticAssociationInitialization'){
        $record.nativeStaticAssociationInitializationRuntimeTested=$text.Contains('Production static association initialization actual owner and SDK fixture completed')
        $record.nativeStaticAssociationInitializationRegisteredBankTested=$text.Contains('Production static association initialization registered same bank refusals completed')
        $record.nativeStaticAssociationInitializationEnteredDebtRetained=$text.Contains('Production static association initialization entered debt retained without replay')
        if(-not($record.nativeStaticAssociationInitializationRuntimeTested -and $record.nativeStaticAssociationInitializationRegisteredBankTested -and $record.nativeStaticAssociationInitializationEnteredDebtRetained)){throw 'Actual static association initialization runtime scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationStaticAssociationWrites'){
        $record.nativeAnimationStaticAssociationWritesOwnerTested=$text.Contains('Production static association scalar writes owner fixture completed')
        $record.actualAnimationStaticAssociationWritesSdkOwnerTested=$text.Contains('Production actual static association scalar SDK fixture completed')
        $record.animationStaticAssociationWriteConflictTested=$text.Contains('Production static association scalar conflict stop fixture completed')
        $record.animationStaticAssociationWriteLostAckTested=$text.Contains('Production static association scalar lost acknowledgement fixture completed')
        $record.animationStaticAssociationWriteUnknownRetained=$text.Contains('Production static association scalar unknown write fixture completed')
        if(-not($record.nativeAnimationStaticAssociationWritesOwnerTested -and $record.actualAnimationStaticAssociationWritesSdkOwnerTested -and $record.animationStaticAssociationWriteConflictTested -and $record.animationStaticAssociationWriteLostAckTested -and $record.animationStaticAssociationWriteUnknownRetained)){throw 'Static association write selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationStaticAssociationReads'){
        $record.nativeAnimationStaticAssociationOwnerTested=$text.Contains('Production animation static association owner fixture completed')
        $record.actualAnimationStaticAssociationSdkOwnerTested=$text.Contains('Production actual animation static association SDK fixture completed')
        if(-not($record.nativeAnimationStaticAssociationOwnerTested -and $record.actualAnimationStaticAssociationSdkOwnerTested)){throw 'Animation static association selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationGroupOffset'){
        $record.nativeAnimationAssociationGroupOffsetOwnerTested=$text.Contains('Production animation association group offset owner fixture completed')
        $record.actualAnimationAssociationGroupOffsetSdkOwnerTested=$text.Contains('Production actual animation association group offset SDK fixture completed')
        $record.animationAssociationGroupOffsetPhysicalTokenTested=$text.Contains('Production animation association group offset physical token fixture completed')
        $record.animationAssociationGroupOffsetLostAckStopTested=$text.Contains('Production animation association group offset lost acknowledgement stop fixture completed')
        $record.animationAssociationGroupOffsetConflictStopTested=$text.Contains('Production animation association group offset conflict stop fixture completed')
        if(-not($record.nativeAnimationAssociationGroupOffsetOwnerTested -and $record.actualAnimationAssociationGroupOffsetSdkOwnerTested -and $record.animationAssociationGroupOffsetPhysicalTokenTested -and $record.animationAssociationGroupOffsetLostAckStopTested -and $record.animationAssociationGroupOffsetConflictStopTested)){throw 'Actual association group offset selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationGroupChildProducts'){
        $record.nativeAnimationAssociationGroupChildProductsOwnerTested=$text.Contains('Production animation association group child products owner fixture completed')
        $record.actualAnimationAssociationGroupStaticChildSdkOwnerTested=$text.Contains('Production actual animation association group static child SDK fixture completed')
        $record.nativeAnimationAssociationGroupChildProductsCoupledRefusalsTested=$text.Contains('Production animation association group child products coupled refusal fixture completed')
        if(-not($record.nativeAnimationAssociationGroupChildProductsOwnerTested -and $record.actualAnimationAssociationGroupStaticChildSdkOwnerTested -and $record.nativeAnimationAssociationGroupChildProductsCoupledRefusalsTested)){throw 'Actual association group child products scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'CameraBasics'){
        $record.nativeCameraBasicsActualSdkOwnerTested=$text.Contains('Production camera basics actual SDK owner fixture completed')
        $record.nativeCameraRestoreOriginalTested=$text.Contains('Production camera restore whole original reference effects fixture completed')
        $record.nativeCameraBasicsRefusalsTested=$text.Contains('Production camera basics current owner refusals fixture completed')
        $record.nativeCameraRestoreDebtTested=$text.Contains('Production camera restore entered-before retained debt fixture completed') -and
            $text.Contains('Production camera restore entered-after retained debt fixture completed') -and
            $text.Contains('Production camera restore owner-loss retained debt fixture completed') -and
            $text.Contains('Production camera restore effect-mismatch retained debt fixture completed')
        $record.nativeCameraSphereVisibleOwnerTested=$text.Contains('Production native camera sphere callback owner fixture completed')
        $record.actualCameraSphereVisibleSdkOwnerTested=$text.Contains('Production actual CCamera sphere SDK owner fixture completed')
        if(-not($record.nativeCameraBasicsActualSdkOwnerTested -and $record.nativeCameraRestoreOriginalTested -and $record.nativeCameraBasicsRefusalsTested -and
            $record.nativeCameraRestoreDebtTested -and $record.nativeCameraSphereVisibleOwnerTested -and $record.actualCameraSphereVisibleSdkOwnerTested)){
            throw 'Camera basics actual owner, original, refusals, retained debt or existing sphere regression incomplete'
        }
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'ModelTimeGetter'){
        $record.nativeModelTimeGetterActualSdkOwnerTested=$text.Contains('Production model time getter actual SDK owner fixture completed')
        $record.nativeModelTimeGetterRefusalsTested=$text.Contains('Production model time getter source current owner refusals fixture completed')
        $record.nativeModelTimeGetterOutputsPreserved=$text.Contains('Production model time getter raw outputs and complete bytes preserved fixture completed')
        $record.nativeNamedVehiclePredicatesActualSdkOwnerTested=$text.Contains('Production named vehicle predicates actual SDK owner fixture completed')
        $record.nativeNamedVehiclePredicatesEffectsTested=$text.Contains('Production named vehicle predicates raw DWORD null nonvehicle effects fixture completed')
        $record.nativeNamedVehiclePredicatesRefusalsTested=$text.Contains('Production named vehicle predicates source owner bookend refusals fixture completed')
        $record.existingFourNativeClassifiersPreserved=$text.Contains('Production existing four native classifier whole originals fixture completed')
        if(-not($record.nativeModelTimeGetterActualSdkOwnerTested -and $record.nativeModelTimeGetterRefusalsTested -and $record.nativeModelTimeGetterOutputsPreserved -and $record.nativeNamedVehiclePredicatesActualSdkOwnerTested -and $record.nativeNamedVehiclePredicatesEffectsTested -and $record.nativeNamedVehiclePredicatesRefusalsTested -and $record.existingFourNativeClassifiersPreserved)){throw 'Combined time getter and prior11 classifier owner scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'VehiclePredicates'){
        $record.nativeNamedVehiclePredicatesActualSdkOwnerTested=$text.Contains('Production named vehicle predicates actual SDK owner fixture completed')
        $record.nativeNamedVehiclePredicatesEffectsTested=$text.Contains('Production named vehicle predicates raw DWORD null nonvehicle effects fixture completed')
        $record.nativeNamedVehiclePredicatesRefusalsTested=$text.Contains('Production named vehicle predicates source owner bookend refusals fixture completed')
        $record.existingFourNativeClassifiersPreserved=$text.Contains('Production existing four native classifier whole originals fixture completed')
        if(-not($record.nativeNamedVehiclePredicatesActualSdkOwnerTested -and $record.nativeNamedVehiclePredicatesEffectsTested -and $record.nativeNamedVehiclePredicatesRefusalsTested -and $record.existingFourNativeClassifiersPreserved)){throw 'Named vehicle predicate focused owner scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'ApplicationTiming'){
        $record['applicationTimingActualOwnerTested']=$text.Contains('Production application timing actual owner and restoration fixture completed')
        $record['applicationTimingCleanupRetryTested']=$text.Contains('Production application timing refused restoration retry fixture completed')
        $record['applicationTimingThrowCleanupTested']=$text.Contains('Production application timing throwing handler cleanup fixture completed')
        $record['applicationTimingLegacyRegressionsTested']=$text.Contains('Production application timing selected legacy regressions fixture completed')
        if(-not($record.applicationTimingActualOwnerTested -and $record.applicationTimingCleanupRetryTested -and $record.applicationTimingThrowCleanupTested -and $record.applicationTimingLegacyRegressionsTested)){throw 'Actual application timing selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'EmptyBlockRemoval'){
        $record.nativeEmptyBlockRemovalActualSdkOwnerTested=$text.Contains('Production empty block removal actual SDK owner fixture completed')
        $record.nativeEmptyBlockRemovalEffectsTested=$text.Contains('Production empty block removal exact empty effects whole FEB580 fixture completed')
        $record.nativeEmptyBlockRemovalRefusalsTested=$text.Contains('Production empty block removal source owner refusals fixture completed') -and $text.Contains('Production empty block removal timer phase refusal fixture completed')
        $record.nativeEmptyBlockRemovalDebtTested=$true
        foreach($debt in @('before','after','partial','owner','group-before','group-after','group-drift','mismatch')){
            $record.nativeEmptyBlockRemovalDebtTested=$record.nativeEmptyBlockRemovalDebtTested -and $text.Contains('Production empty block removal '+$debt+' retained debt fixture completed')
        }
        $record['emptyBlockRemovalExisting61Tested']=$record.testRuns.Count -eq 72 -and $text.Contains('Production block registration actual SDK source policy fixture completed') -and $text.Contains('Production block registration group retained debt fixture completed') -and $text.Contains('Production bank postprocess quaternion actual SDK owner fixture completed') -and $text.Contains('Production bank empty time whole original fixture completed') -and $text.Contains('Production bank eviction actual SDK whole original fixture completed') -and $text.Contains('Production bank decompression actual SDK whole original fixture completed') -and $text.Contains('Production hierarchy cache removal owner loss debt fixture completed') -and $text.Contains('Production actual private hierarchy postprocess SDK owner fixture completed')
        if(-not($record.nativeEmptyBlockRemovalActualSdkOwnerTested -and $record.nativeEmptyBlockRemovalEffectsTested -and $record.nativeEmptyBlockRemovalRefusalsTested -and $record.nativeEmptyBlockRemovalDebtTested -and $record.emptyBlockRemovalExisting61Tested)){throw 'Actual empty-block removal and existing61 source scopes incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'BlockRegistration'){
        $record['nativeBlockRegistrationActualSdkOwnerTested']=$text.Contains('Production block registration actual SDK source policy fixture completed')
        $record['nativeBlockRegistrationEffectsTested']=$text.Contains('Production block registration existing fresh named default boundary effects fixture completed')
        $record['nativeBlockRegistrationRefusalsTested']=$text.Contains('Production block registration coupled source owner refusals fixture completed') -and $text.Contains('Production block registration timer phase refusal fixture completed')
        $record['nativeBlockRegistrationDebtTested']=$true
        foreach($debt in @('before','after','partial','owner','mismatch','count','prefix','group')){
            $record.nativeBlockRegistrationDebtTested=$record.nativeBlockRegistrationDebtTested -and $text.Contains('Production block registration '+$debt+' retained debt fixture completed')
        }
        $record['nativeBankEmptyPostprocessTested']=$text.Contains('Production bank empty quaternion whole original fixture completed') -and $text.Contains('Production bank empty time whole original fixture completed')
        $record['blockRegistrationExisting50Tested']=$record.testRuns.Count -eq 61 -and $text.Contains('Production bank postprocess quaternion actual SDK owner fixture completed') -and $text.Contains('Production bank eviction actual SDK whole original fixture completed') -and $text.Contains('Production bank decompression actual SDK whole original fixture completed') -and $text.Contains('Production hierarchy cache removal owner loss debt fixture completed') -and $text.Contains('Production actual private hierarchy postprocess SDK owner fixture completed')
        if(-not($record.nativeBlockRegistrationActualSdkOwnerTested -and $record.nativeBlockRegistrationEffectsTested -and $record.nativeBlockRegistrationRefusalsTested -and $record.nativeBlockRegistrationDebtTested -and $record.nativeBankEmptyPostprocessTested -and $record.blockRegistrationExisting50Tested)){throw 'Actual registration, empty-bank and existing component scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'BankPostprocess'){
        $record['nativeBankEmptyPostprocessTested']=$text.Contains('Production bank empty quaternion whole original fixture completed') -and $text.Contains('Production bank empty time whole original fixture completed')
        if(-not $record.nativeBankEmptyPostprocessTested){throw 'Both actual empty bank postprocess originals must execute'}
        $record.nativeBankPostprocessActualSdkOwnerTested=$text.Contains('Production bank postprocess quaternion actual SDK owner fixture completed') -and $text.Contains('Production bank postprocess time actual SDK owner fixture completed')
        $record.nativeBankPostprocessEffectsTested=$text.Contains('Production bank postprocess exact quaternion signed-zero effects fixture completed') -and $text.Contains('Production bank postprocess exact timestamp and Htime effects fixture completed')
        $record.nativeBankPostprocessRefusalsTested=$text.Contains('Production bank postprocess coupled current bank refusals fixture completed') -and $text.Contains('Production bank postprocess default timer refusal fixture completed')
        $record.nativeBankPostprocessUnknownRetained=$true
        foreach($debt in @('quaternion-before','quaternion-after','time-before','time-after','quaternion-owner','time-owner','quaternion-mismatch','time-mismatch')){
            $record.nativeBankPostprocessUnknownRetained=$record.nativeBankPostprocessUnknownRetained -and $text.Contains('Production bank postprocess '+$debt+' retained debt fixture completed')
        }
        if(-not($record.nativeBankPostprocessActualSdkOwnerTested -and $record.nativeBankPostprocessEffectsTested -and $record.nativeBankPostprocessRefusalsTested -and $record.nativeBankPostprocessUnknownRetained)){throw 'Actual bank postprocess owner/effects/refusal/debt scope incomplete'}
        $record['bankPostprocessExisting37RegressionsTested']=$record.testRuns.Count -eq 50 -and $text.Contains('Production bank eviction actual SDK whole original fixture completed') -and $text.Contains('Production bank eviction cache-mismatch retained debt fixture completed') -and $text.Contains('Production bank decompression no-output retained debt fixture completed') -and $text.Contains('Production hierarchy cache removal owner loss debt fixture completed')
        if(-not $record.bankPostprocessExisting37RegressionsTested){throw 'Changed component existing37 regression processes incomplete'}
        $record.nativePrivateHierarchyPostprocessOwnerTested=$text.Contains('Production native private hierarchy postprocess owner fixture completed')
        $record.actualPrivateHierarchyPostprocessSdkOwnerTested=$text.Contains('Production actual private hierarchy postprocess SDK owner fixture completed')
        if(-not($record.nativePrivateHierarchyPostprocessOwnerTested -and $record.actualPrivateHierarchyPostprocessSdkOwnerTested)){throw 'Existing actual private postprocess SDK owner regression incomplete'}
        $record.productionNativeCallbackTested=$true
        $record['originalHierarchyPostprocessInstructionsExecuted']=$true
    } elseif($Scope -eq 'BankEviction'){
        $record.nativeBankEvictionActualSdkOwnerTested=$text.Contains('Production bank eviction actual SDK whole original fixture completed')
        $record.nativeBankEvictionConversionTested=$text.Contains('Production bank eviction both lifetime conversion and freed address reuse fixture completed')
        $record.nativeBankEvictionRefusalsTested=$text.Contains('Production bank eviction coupled current bank refusal fixture completed') -and $text.Contains('Production bank eviction default timer refusal fixture completed')
        $record.nativeBankEvictionUnknownRetained=$true
        foreach($debt in @('before','after','owner','victim-mismatch','target-mismatch','victim-fallback','target-fallback','victim-no-output','target-no-output','cache-mismatch')){
            $record.nativeBankEvictionUnknownRetained=$record.nativeBankEvictionUnknownRetained -and $text.Contains('Production bank eviction '+$debt+' retained debt fixture completed')
        }
        if(-not($record.nativeBankEvictionActualSdkOwnerTested -and $record.nativeBankEvictionConversionTested -and $record.nativeBankEvictionRefusalsTested -and $record.nativeBankEvictionUnknownRetained)){throw 'Actual bank eviction owner/conversion/refusal/debt scope incomplete'}
        $record['bankEvictionExisting24RegressionsTested']=$record.testRuns.Count -eq 37 -and $text.Contains('Production bank decompression actual SDK whole original fixture completed') -and $text.Contains('Production bank decompression no-output retained debt fixture completed') -and $text.Contains('Production hierarchy cache removal owner loss debt fixture completed') -and $text.Contains('Production hierarchy Uncompress cache mismatch debt fixture completed')
        if(-not $record.bankEvictionExisting24RegressionsTested){throw 'Changed Uncompress component existing24 regression processes incomplete'}
        $record.productionNativeCallbackTested=$true
        $record.originalHierarchyUncompressInstructionsExecuted=$true
        $record['nativeAllocatorLeafSubstitutionOnly']=$true
    } elseif($Scope -eq 'BankDecompression'){
        $record.nativeBankDecompressionActualSdkOwnerTested=$text.Contains('Production bank decompression actual SDK whole original fixture completed')
        $record.nativeBankDecompressionConversionTested=$text.Contains('Production bank decompression exact individual shared zero signed-zero conversion fixture completed')
        $record.nativeBankDecompressionRefusalsTested=$text.Contains('Production bank decompression actual owner coupled refusals fixture completed') -and $text.Contains('Production bank decompression default timer phase refusal fixture completed')
        $record.nativeBankDecompressionUnknownRetained=$true
        foreach($debt in @('before','after','owner','mismatch','fallback','no-output')){
            $record.nativeBankDecompressionUnknownRetained=$record.nativeBankDecompressionUnknownRetained -and $text.Contains('Production bank decompression '+$debt+' retained debt fixture completed')
        }
        if(-not($record.nativeBankDecompressionActualSdkOwnerTested -and $record.nativeBankDecompressionConversionTested -and $record.nativeBankDecompressionRefusalsTested -and $record.nativeBankDecompressionUnknownRetained)){throw 'Actual bank decompression owner/conversion/refusal/debt scope incomplete'}
        $record['bankDecompressionExisting15RegressionsTested']=$record.testRuns.Count -eq 24 -and $text.Contains('Production hierarchy cache removal owner loss debt fixture completed') -and $text.Contains('Production hierarchy Uncompress actual SDK original compressed conversion fixture completed') -and $text.Contains('Production hierarchy Uncompress cache mismatch debt fixture completed')
        if(-not $record.bankDecompressionExisting15RegressionsTested){throw 'Changed Uncompress component existing15 regression processes incomplete'}
        $record.productionNativeCallbackTested=$true
        $record.originalHierarchyUncompressInstructionsExecuted=$true
        $record['nativeAllocatorLeafSubstitutionOnly']=$true
    } elseif($Scope -eq 'AnimationHierarchyUncompress'){
        $record['nativeHierarchyCacheRemovalOwnerTested']=$text.Contains('Production hierarchy cache removal actual MTA owner fixture completed')
        $record['nativeHierarchyCacheRemovalDebtTested']=$text.Contains('Production hierarchy cache removal before acknowledgement debt fixture completed') -and $text.Contains('Production hierarchy cache removal after acknowledgement debt fixture completed') -and $text.Contains('Production hierarchy cache removal mismatch debt fixture completed') -and $text.Contains('Production hierarchy cache removal partial debt fixture completed') -and $text.Contains('Production hierarchy cache removal owner loss debt fixture completed')
        if(-not($record.nativeHierarchyCacheRemovalOwnerTested -and $record.nativeHierarchyCacheRemovalDebtTested)){throw 'Actual cache removal owner and ordered-store debt scope incomplete'}
        $record['nativePrivateHierarchyWarmUncompressTested']=$text.Contains('Production private hierarchy Uncompress source no-effect fixture completed')
        $record['nativeHierarchyExistingCacheMoveTested']=$text.Contains('Production hierarchy Uncompress existing cache LRU fixture completed')
        $record['nativeHierarchyExistingCacheDebtTested']=$text.Contains('Production hierarchy Uncompress cache entered-before debt fixture completed') -and $text.Contains('Production hierarchy Uncompress cache entered-after debt fixture completed') -and $text.Contains('Production hierarchy Uncompress cache mismatch debt fixture completed')
        $record.nativeHierarchyUncompressActualSdkTested=$text.Contains('Production hierarchy Uncompress actual SDK original compressed conversion fixture completed')
        $record.nativeHierarchyUncompressCoupledRefusalsTested=$text.Contains('Production hierarchy Uncompress actual owner coupled refusal fixture completed')
        $record.nativeHierarchyUncompressUnknownRetained=$text.Contains('Production hierarchy Uncompress entered before acknowledgement debt fixture completed') -and $text.Contains('Production hierarchy Uncompress entered after original debt fixture completed')
        $record.nativeHierarchyUncompressPostOwnerLossTested=$text.Contains('Production hierarchy Uncompress post original owner loss fixture completed')
        $record.nativeHierarchyUncompressEffectMismatchTested=$text.Contains('Production hierarchy Uncompress post original effect mismatch fixture completed')
        if(-not($record.nativeHierarchyExistingCacheMoveTested -and $record.nativeHierarchyExistingCacheDebtTested -and $record.nativePrivateHierarchyWarmUncompressTested -and $record.nativeHierarchyUncompressActualSdkTested -and $record.nativeHierarchyUncompressCoupledRefusalsTested -and $record.nativeHierarchyUncompressUnknownRetained -and $record.nativeHierarchyUncompressPostOwnerLossTested -and $record.nativeHierarchyUncompressEffectMismatchTested)){throw 'Actual hierarchy Uncompress selected owner/original/debt scope incomplete'}
        $record.productionNativeCallbackTested=$true
        $record.originalHierarchyUncompressInstructionsExecuted=$true
    } elseif($Scope -eq 'NumericAssociationOwner'){
$record['rawAssociationIssuedFactoryTested']=$text.Contains('Production issued raw association actual factory and unconstructed lifetime fixture completed')
$record.numericAssociationActualFactoryTested=$text.Contains('Production numeric association actual manager and independent child lifetime fixture completed')
$record.numericAssociationCodeChildLifetimeTested=$text.Contains('Production numeric association actual manager and independent child lifetime fixture completed')
$record.numericAssociationNullTested=$text.Contains('Production numeric association actual nullable factory fixture completed')
$record.numericAssociationUnknownRetained=$text.Contains('Production numeric association entered uncertainty fixture completed')
$record.numericAssociationForeignCodeRetained=$text.Contains('Production numeric association foreign code quarantine fixture completed')
$record.numericAssociationCpuFirstLifetimeTested=$text.Contains('Production numeric association CPU-first lifetime fixture completed')
$record.numericAssociationClosureTested=$text.Contains('Production numeric association pre-entry closure fixture completed') -and $text.Contains('Production numeric association in-flight closure fixture completed')
$record.productionNativeCallbackTested=$true;$record['originalGroupCopyInstructionsExecuted']=$true
if(-not($record.rawAssociationIssuedFactoryTested -and $record.numericAssociationActualFactoryTested -and $record.numericAssociationCodeChildLifetimeTested -and $record.numericAssociationNullTested -and $record.numericAssociationUnknownRetained -and $record.numericAssociationForeignCodeRetained -and $record.numericAssociationCpuFirstLifetimeTested -and $record.numericAssociationClosureTested -and $record.productionNativeCallbackTested)){throw 'Numeric association actual SDK/native/code-lifetime scope incomplete'}
    } elseif($Scope -eq 'DynamicClumpOwner'){
        $record.nativeDynamicClumpActualSdkOwnerTested=$text.Contains('Production dynamic clump actual SDK mixed and source defaults fixture completed')
        $record.nativeDynamicClumpZeroAllocationTested=$text.Contains('Production dynamic clump zero allocation fixture completed')
        $record.nativeDynamicClumpReferenceProvenanceTested=$text.Contains('Production dynamic clump copy reference compatibility fixture completed')
        $record.nativeDynamicClumpHierarchyLifetimeTested=$text.Contains('Production dynamic clump reversible hierarchy lifetime fixture completed')
        $record.nativeDynamicClumpEnteredFactsRetained=$text.Contains('Production dynamic clump entered/default uncertainty retained fixture completed')
        $record.nativeDynamicClumpTemporaryOriginTested=$text.Contains('Production dynamic clump closed temporary origin fixture completed')
        $record.nativeDynamicClumpSharedStaticRegressionTested=$text.Contains('Production static association initialization actual owner and SDK fixture completed') -and $text.Contains('Production joint temporary static initializer owner fixture completed')
        if(-not($record.nativeDynamicClumpActualSdkOwnerTested -and $record.nativeDynamicClumpZeroAllocationTested -and $record.nativeDynamicClumpReferenceProvenanceTested -and $record.nativeDynamicClumpHierarchyLifetimeTested -and $record.nativeDynamicClumpEnteredFactsRetained -and $record.nativeDynamicClumpTemporaryOriginTested -and $record.nativeDynamicClumpSharedStaticRegressionTested)){throw 'Actual dynamic clump selected owner/static regression scope incomplete'}
        $record['originalClumpConstructorInstructionsExecuted']=$false;$record['originalClumpMatcherInstructionsExecuted']=$false;$record['isolatedNativeNodeProducerInstructionsExecuted']=$true;$record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationGroupFactories'){
        $record.nativeGroupFactoryActualSdkOwnerTested=$text.Contains('Production animation association group factories actual SDK owner fixture completed')
        $record.nativeGroupFactoryPhysicalResolverRefusalsTested=$text.Contains('Production animation association group factories physical resolver refusal fixture completed')
        $record.nativeGroupFactoryCpuLifetimeNativeFactsTested=$text.Contains('Production animation association group factories CPU lifetime native facts fixture completed')
        if(-not($record.nativeGroupFactoryActualSdkOwnerTested -and $record.nativeGroupFactoryPhysicalResolverRefusalsTested -and $record.nativeGroupFactoryCpuLifetimeNativeFactsTested)){throw 'Actual group factory selected owner scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'GroupSourceCreationOwner'){
        $record.nativeGroupSourceCreationOwnerTested=$text.Contains('Production group source creation actual SDK runtime owner fixture completed')
        $record.nativeGroupSourceCreationSourceOrderTested=$text.Contains('Production group source creation source order and missing model fixture completed')
        $record.nativeGroupSourceCreationRuntimeDrainTested=$text.Contains('Production group source creation runtime private drain fixture completed')
        $record.nativeGroupSourceCreationEnteredFactsRetained=$text.Contains('Production group source creation retained entered facts fixture completed')
        $record.nativeGroupSourceCreationScalarFactsTested=$text.Contains('Production group source creation scalar facts without private debt fixture completed')
        if(-not($record.nativeGroupSourceCreationOwnerTested -and $record.nativeGroupSourceCreationSourceOrderTested -and $record.nativeGroupSourceCreationRuntimeDrainTested -and $record.nativeGroupSourceCreationEnteredFactsRetained -and $record.nativeGroupSourceCreationScalarFactsTested)){throw 'Actual SDK/runtime group source creation selected scope incomplete'}
        $record['actualFEB5F0InstructionsExecuted']=$false
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'JointTemporaryStaticInitializerOwner'){
        $record.nativeJointTemporaryStaticInitializerOwnerTested=$text.Contains('Production joint temporary static initializer owner fixture completed')
        $record.nativeJointTemporaryStaticClosedOriginsTested=$text.Contains('Production joint temporary static closed origins fixture completed')
        $record.nativeJointTemporaryStaticZeroExtentTested=$text.Contains('Production joint temporary static zero extent fixture completed')
        $record.nativeJointTemporaryStaticEnteredFactsRetained=$text.Contains('Production joint temporary static entered facts retained fixture completed')
        $record.publicPedInitializerRegressionTested=$text.Contains('Production joint initializer public registered ped regression completed')
        if(-not($record.nativeJointTemporaryStaticInitializerOwnerTested -and $record.nativeJointTemporaryStaticClosedOriginsTested -and $record.nativeJointTemporaryStaticZeroExtentTested -and $record.nativeJointTemporaryStaticEnteredFactsRetained -and $record.publicPedInitializerRegressionTested)){throw 'Joint temporary static initializer selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'GroupReplacementOwner'){
        $record.nativeGroupReplacementOwnerTested=$text.Contains('Production group replacement actual owner fixture completed')
        $record.nativeGroupReplacementSourceOrderTested=$text.Contains('Production group replacement source ordered publication fixture completed')
        $record.nativeGroupReplacementOriginalRetirementTested=$text.Contains('Production group replacement original vector and zero cookie retirement fixture completed')
        $record.nativeGroupReplacementPublicationOutcomesTested=$text.Contains('Production group replacement causal publication outcomes fixture completed')
        if(-not($record.nativeGroupReplacementOwnerTested -and $record.nativeGroupReplacementSourceOrderTested -and $record.nativeGroupReplacementOriginalRetirementTested -and $record.nativeGroupReplacementPublicationOutcomesTested)){throw 'Actual group replacement selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'GroupCopyAnimationOwner'){
        $record.nativeGroupCopyAnimationOwnerTested=$text.Contains('Production group copy animation actual owner fixture completed')
        $record.actualGroupCopyAnimationSdkOwnerTested=$text.Contains('Production group copy animation actual SDK raw transport fixture completed')
        $record.nativeGroupCopyAnimationNullableTested=$text.Contains('Production group copy animation nullable authority fixture completed')
        $record.nativeGroupCopyAnimationEnteredDebtRetained=$text.Contains('Production group copy animation retained failures without replay fixture completed')
        if(-not($record.nativeGroupCopyAnimationOwnerTested -and $record.actualGroupCopyAnimationSdkOwnerTested -and $record.nativeGroupCopyAnimationNullableTested -and $record.nativeGroupCopyAnimationEnteredDebtRetained)){throw 'Actual group copy animation selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'GroupEmptyInitialization'){
        $record.nativeGroupEmptyInitializationOwnerTested=$text.Contains('Production group empty initialization actual owner and SDK fixture completed')
        $record.actualGroupEmptyInitializationSdkOwnerTested=$text.Contains('Production group empty initialization actual owner and SDK fixture completed')
        $record.nativeGroupEmptyInitializationPhaseAndPrefixTested=$text.Contains('Production group empty initialization phase and verified prefix fixture completed')
        $record.nativeGroupEmptyInitializationEnteredDebtRetained=$text.Contains('Production group empty initialization entered debt retained without replay')
        if(-not($record.nativeGroupEmptyInitializationOwnerTested -and $record.actualGroupEmptyInitializationSdkOwnerTested -and $record.nativeGroupEmptyInitializationPhaseAndPrefixTested -and $record.nativeGroupEmptyInitializationEnteredDebtRetained)){throw 'Actual group empty initialization selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationAssociationGroupReads'){
        $record.nativeAnimationAssociationGroupOwnerTested=$text.Contains('Production animation association group owner fixture completed')
        $record.actualAnimationAssociationGroupSdkOwnerTested=$text.Contains('Production actual animation association group SDK fixture completed')
        if(-not($record.nativeAnimationAssociationGroupOwnerTested -and $record.actualAnimationAssociationGroupSdkOwnerTested)){throw 'Animation association group selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationIdValidity'){
        $record.nativeAnimationIdValidityOwnerTested=$text.Contains('Production animation ID validity owner fixture completed')
        $record.actualAnimationIdValiditySdkOwnerTested=$text.Contains('Production actual animation ID validity SDK fixture completed')
        if(-not($record.nativeAnimationIdValidityOwnerTested -and $record.actualAnimationIdValiditySdkOwnerTested)){throw 'Animation ID validity selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationGroupValidity'){
        $record.nativeAnimationGroupValidityOwnerTested=$text.Contains('Production animation group validity owner fixture completed')
        $record.actualAnimationGroupValiditySdkOwnerTested=$text.Contains('Production actual animation group validity SDK fixture completed')
        if(-not($record.nativeAnimationGroupValidityOwnerTested -and $record.actualAnimationGroupValiditySdkOwnerTested)){throw 'Animation group validity selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationDefinitionBlockNames'){
        $record.nativeAnimationDefinitionBlockNamesOwnerTested=$text.Contains('Production animation definition block names owner fixture completed')
        $record.actualAnimationDefinitionBlockNamesSdkOwnerTested=$text.Contains('Production actual definition block name SDK fixture completed')
        if(-not($record.nativeAnimationDefinitionBlockNamesOwnerTested -and $record.actualAnimationDefinitionBlockNamesSdkOwnerTested)){throw 'Animation definition block names selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationDefinitions'){
        $record.nativeAnimationDefinitionsOwnerTested=$text.Contains('Production animation definitions owner fixture completed')
        $record.actualAnimationDefinitionsSdkOwnerTested=$text.Contains('Production actual animation definition SDK fixture completed')
        if(-not($record.nativeAnimationDefinitionsOwnerTested -and $record.actualAnimationDefinitionsSdkOwnerTested)){throw 'Animation definitions selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationBlockHashFactory'){
        $record.nativeAnimationBlockHashFactoryOwnerTested=$text.Contains('Production animation block hash factory owner fixture completed')
        $record.actualAnimationBlockHashFactorySdkOwnerTested=$text.Contains('Production actual block hash factory SDK fixture completed')
        if(-not($record.nativeAnimationBlockHashFactoryOwnerTested -and $record.actualAnimationBlockHashFactorySdkOwnerTested)){throw 'Animation block hash factory selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationManagerCache'){
        $record.nativeAnimationManagerCacheOwnerTested=$text.Contains('Production animation manager cache owner fixture completed')
        $record.actualAnimationManagerCacheSdkOwnerTested=$text.Contains('Production animation manager cache refusal ownership completed')
        $record.animationManagerCacheForeignStateRetained=$text.Contains('Production animation manager cache foreign state retained')
        if(-not($record.nativeAnimationManagerCacheOwnerTested -and $record.actualAnimationManagerCacheSdkOwnerTested -and $record.animationManagerCacheForeignStateRetained)){throw 'Animation manager cache selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'SequenceInterfaceCompatibility'){
        $record.nativeSequenceInterfaceOwnerTested=$text.Contains('Production sequence interface compatibility owner fixture completed')
        $record.actualSequenceInterfaceSdkOwnerTested=$text.Contains('Production sequence interface compatibility SDK fixture completed')
        $record.sequenceInterfaceUnknownMetadataFreeRetained=$text.Contains('Production sequence interface unknown metadata free retained')
        if(-not($record.nativeSequenceInterfaceOwnerTested -and $record.actualSequenceInterfaceSdkOwnerTested -and $record.sequenceInterfaceUnknownMetadataFreeRetained)){throw 'Sequence interface selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationBankFactories'){
        $record.nativeBankAnimationFactoryOwnerTested=$text.Contains('Production native bank animation factory owner fixture completed')
        $record.actualBankAnimationFactorySdkOwnerTested=$text.Contains('Production actual bank animation factory SDK owner fixture completed')
        $record.bankAnimationFactoryGenerationBookendsTested=$text.Contains('Production bank animation factory generation bookends completed')
        if(-not($record.nativeBankAnimationFactoryOwnerTested -and $record.actualBankAnimationFactorySdkOwnerTested -and $record.bankAnimationFactoryGenerationBookendsTested)){throw 'Bank animation factory selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'StandaloneSequenceOperations'){
        $record.actualStandaloneSequenceOperationsOwnerTested=$text.Contains('Production standalone sequence operations success verified')
        $record.standaloneSequenceBindLostAckTested=$text.Contains('Production standalone sequence operations bind lost acknowledgement verified')
        $record.standaloneSequenceCopyLostAckTested=$text.Contains('Production standalone sequence operations copy lost acknowledgement verified')
        $record.standaloneSequenceUnknownWriteRetained=$text.Contains('Production standalone sequence operations unknown write retained')
        $record.standaloneSequenceForeignDropRetained=$text.Contains('Production standalone sequence operations foreign ticket drop quarantined')
        $record.standaloneSequenceStrictFactoryDispatchTested=$text.Contains('Production standalone strict factory dispatch and metadata delete routing verified')
        if(-not($record.actualStandaloneSequenceOperationsOwnerTested -and $record.standaloneSequenceBindLostAckTested -and $record.standaloneSequenceCopyLostAckTested -and $record.standaloneSequenceUnknownWriteRetained -and $record.standaloneSequenceForeignDropRetained -and $record.standaloneSequenceStrictFactoryDispatchTested)){throw 'Standalone sequence selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'CustomAnimationCleanup'){
        $record.actualPrivateAnimationCleanupControllerTested=$text.Contains('Production actual private cleanup controller acknowledgements fixture completed')
        $record.actualPrivateAnimationCleanupSdkOwnerTested=$text.Contains('Production actual private cleanup runtime ticket fixture completed')
        $record.privateAnimationCleanupEndLifetimeTested=$text.Contains('Production private cleanup live ticket End gate fixture completed')
        $record.privateAnimationCleanupReturnedAckRecoveryTested=$text.Contains('Production private cleanup returned acknowledgement recovered without replay')
        $record.privateAnimationCleanupUnknownDebtRetained=$text.Contains('Production private cleanup unknown acknowledgement retained without replay')
        $record.privateAnimationCleanupForeignDropRetained=$text.Contains('Production private cleanup foreign thread ticket drop retained provider lifetime')
        $record.privateAnimationCleanupActivePumpTested=$text.Contains('Production private cleanup abandoned intent progressed during active callbacks')
        if(-not($record.actualPrivateAnimationCleanupControllerTested -and $record.actualPrivateAnimationCleanupSdkOwnerTested -and $record.privateAnimationCleanupEndLifetimeTested -and $record.privateAnimationCleanupReturnedAckRecoveryTested -and $record.privateAnimationCleanupUnknownDebtRetained -and $record.privateAnimationCleanupForeignDropRetained -and $record.privateAnimationCleanupActivePumpTested)){throw 'Private cleanup selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PrivateAnimationFactories'){
        $record.nativePrivateAnimationFactoryOwnerTested=$text.Contains('Production native private animation factory owner fixture completed')
        $record.actualPrivateAnimationFactorySdkOwnerTested=$text.Contains('Production actual private animation factory SDK owner fixture completed')
        $record.privateAnimationFactoryOutputBookendsTested=$text.Contains('Production private animation factory output bookends completed')
        if(-not($record.nativePrivateAnimationFactoryOwnerTested -and $record.actualPrivateAnimationFactorySdkOwnerTested -and $record.privateAnimationFactoryOutputBookendsTested)){throw 'Private factory selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -in @('PrivateHierarchyPostprocess','FramesAndHierarchyPostprocess')){
        $record.nativePrivateHierarchyPostprocessOwnerTested=$text.Contains('Production native private hierarchy postprocess owner fixture completed')
        $record.actualPrivateHierarchyPostprocessSdkOwnerTested=$text.Contains('Production actual private hierarchy postprocess SDK owner fixture completed')
        $record.privateHierarchyPostprocessRecoveryTested=(@($texts|Where-Object{$_.Contains('Production private hierarchy postprocess entered failure recovered without replay')}).Count -eq 2)
        if(-not($record.nativePrivateHierarchyPostprocessOwnerTested -and $record.actualPrivateHierarchyPostprocessSdkOwnerTested -and $record.privateHierarchyPostprocessRecoveryTested)){throw 'Private postprocess selected scope incomplete'}
        if($Scope -eq 'FramesAndHierarchyPostprocess'){
            $record.nativeFreshUncompressedSequenceOwnerTested=$text.Contains('Production native fresh uncompressed sequence owner fixture completed')
            $record.actualFreshUncompressedSequenceServiceOwnerTested=$text.Contains('Production actual fresh uncompressed sequence service owner fixture completed')
            if(-not($record.nativeFreshUncompressedSequenceOwnerTested -and $record.actualFreshUncompressedSequenceServiceOwnerTested)){throw 'Combined fresh uncompressed scope incomplete'}
        }
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'FreshUncompressedSequence'){
        $record.nativeFreshUncompressedSequenceOwnerTested=$text.Contains('Production native fresh uncompressed sequence owner fixture completed')
        $record.actualFreshUncompressedSequenceServiceOwnerTested=$text.Contains('Production actual fresh uncompressed sequence service owner fixture completed')
        if(-not($record.nativeFreshUncompressedSequenceOwnerTested -and $record.actualFreshUncompressedSequenceServiceOwnerTested)){throw 'Fresh uncompressed selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PrivateHierarchyEmptyOperations'){
        $record.nativePrivateHierarchyEmptyOperationsOwnerTested=$text.Contains('Production native private hierarchy empty operations owner fixture completed')
        $record.actualPrivateHierarchyEmptyOperationsSdkOwnerTested=$text.Contains('Production actual private hierarchy empty operations SDK owner fixture completed')
        $record.privateHierarchyEmptyRecoveryTested=$text.Contains('Production private hierarchy empty accepted cleanup recovered')
        $record.privateHierarchyEmptyRetainedLifetimeTested=(@($texts|Where-Object{$_.Contains('Production private hierarchy empty unresolved lifetime retained')}).Count -eq 2)
        if(-not($record.nativePrivateHierarchyEmptyOperationsOwnerTested -and $record.actualPrivateHierarchyEmptyOperationsSdkOwnerTested -and $record.privateHierarchyEmptyRecoveryTested -and $record.privateHierarchyEmptyRetainedLifetimeTested)){throw 'Private empty selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PrivateHierarchyParent'){
        $record.nativePrivateHierarchyParentOwnerTested=$text.Contains('Production native private hierarchy parent owner fixture completed')
        $record.actualPrivateHierarchyParentSdkOwnerTested=$text.Contains('Production actual private hierarchy parent SDK owner fixture completed')
        $record.privateHierarchyParentRecoveryTested=$text.Contains('Production private hierarchy parent lost acknowledgement recovered')
        $record.privateHierarchyParentRetainedLifetimeTested=(@($texts|Where-Object{$_.Contains('Production private hierarchy parent unresolved lifetime retained')}).Count -eq 4)
        if(-not($record.nativePrivateHierarchyParentOwnerTested -and $record.actualPrivateHierarchyParentSdkOwnerTested -and $record.privateHierarchyParentRecoveryTested -and $record.privateHierarchyParentRetainedLifetimeTested)){throw 'Private parent selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PrivateHierarchyName'){
        $record.nativePrivateHierarchyNameOwnerTested=$text.Contains('Production native private hierarchy name owner fixture completed')
        $record.actualPrivateHierarchyNameSdkOwnerTested=$text.Contains('Production actual private hierarchy name SDK owner fixture completed')
        $record.privateHierarchyNamePinnedRefusalTested=$text.Contains('Production private hierarchy name publication refusal retained')
        if(-not($record.nativePrivateHierarchyNameOwnerTested -and $record.actualPrivateHierarchyNameSdkOwnerTested -and $record.privateHierarchyNamePinnedRefusalTested)){throw 'Private name selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PrivateHierarchyScalars'){
        $record.nativePrivateHierarchyScalarOwnerTested=$text.Contains('Production native private hierarchy scalar owner fixture completed')
        $record.actualPrivateHierarchyScalarSdkOwnerTested=$text.Contains('Production actual private hierarchy scalar SDK owner fixture completed')
        $record.privateHierarchyScalarPinnedRefusalTested=$text.Contains('Production private hierarchy scalar publication refusal retained')
        if(-not($record.nativePrivateHierarchyScalarOwnerTested -and $record.actualPrivateHierarchyScalarSdkOwnerTested -and $record.privateHierarchyScalarPinnedRefusalTested)){throw 'Private scalar selected scope incomplete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'PrivateHierarchy'){
        $record.nativePrivateHierarchyOwnerTested=$text.Contains('Production native private hierarchy owner fixture completed')
        $record.actualPrivateHierarchySdkOwnerTested=$text.Contains('Production actual private hierarchy SDK owner fixture completed')
        $record.privateHierarchyRetainedLifetimeTested=(@($record.testRuns).Count -eq 4) -and (@($texts|Where-Object{$_.Contains('Production private hierarchy unresolved lifetime retained')}).Count -eq 3)
        if(-not($record.nativePrivateHierarchyOwnerTested -and $record.actualPrivateHierarchySdkOwnerTested -and $record.privateHierarchyRetainedLifetimeTested)){throw 'Private hierarchy selected scope did not complete'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'FreshCompressedSequence'){
        $record.nativeFreshCompressedSequenceOwnerTested=$text.Contains('Production native fresh compressed sequence owner fixture completed')
        $record.actualFreshCompressedSequenceServiceOwnerTested=$text.Contains('Production actual fresh compressed sequence service owner fixture completed')
        if(-not($record.nativeFreshCompressedSequenceOwnerTested -and $record.actualFreshCompressedSequenceServiceOwnerTested)){throw 'New private fresh compressed service owner slice did not complete.'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'CustomAnimationSequenceName'){
        $record.nativeCustomAnimationSequenceNameOwnerTested=$text.Contains('Production native custom animation sequence name owner fixture completed')
        $record.actualCustomAnimationSequenceNameSdkOwnerTested=$text.Contains('Production actual custom animation sequence name SDK owner fixture completed')
        if(-not($record.nativeCustomAnimationSequenceNameOwnerTested -and $record.actualCustomAnimationSequenceNameSdkOwnerTested)){throw 'New sequence name actual owner/SDK slice did not complete.'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'AnimationHierarchyBank'){
        $record.nativeAnimationHierarchyBankOwnerTested=$text.Contains('Production native animation hierarchy bank owner fixture completed')
        $record.actualAnimationHierarchyBankSdkOwnerTested=$text.Contains('Production actual animation hierarchy bank SDK owner fixture completed')
        if(-not($record.nativeAnimationHierarchyBankOwnerTested -and $record.actualAnimationHierarchyBankSdkOwnerTested)){throw 'New hierarchy bank actual owner/SDK slice did not complete.'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'CustomAnimationSequenceCopyFrame'){
        $record.nativeCustomAnimationSequenceCopyFrameOwnerTested=$text.Contains('Production native custom animation sequence copy/frame owner fixture completed')
        $record.actualCustomAnimationSequenceCopyFrameSdkOwnerTested=$text.Contains('Production actual custom animation sequence copy/frame SDK owner fixture completed')
        if(-not($record.nativeCustomAnimationSequenceCopyFrameOwnerTested -and $record.actualCustomAnimationSequenceCopyFrameSdkOwnerTested)){throw 'New custom sequence copy/frame actual owner/SDK slice did not complete.'}
        $record.productionNativeCallbackTested=$true
    } elseif($Scope -eq 'CustomAnimationSequence'){
        $record.nativeCustomAnimationSequenceOwnerTested=$text.Contains('Production native custom animation sequence owner fixture completed')
        $record.actualCustomAnimationSequenceSdkOwnerTested=$text.Contains('Production actual custom animation sequence SDK owner fixture completed')
        if(-not($record.nativeCustomAnimationSequenceOwnerTested -and $record.actualCustomAnimationSequenceSdkOwnerTested)){throw 'New custom sequence actual owner/SDK slice did not complete.'}
        $record.productionNativeCallbackTested=$true
    } elseif ($Scope -eq 'AnimationGateway') {
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
