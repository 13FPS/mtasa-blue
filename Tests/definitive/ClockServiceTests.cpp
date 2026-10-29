/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual MTA clock interface and checked service boundary
 *****************************************************************************/
#include "../../Client/game_de/CClockDE.h"
#include "../../Client/game_de/NativePresentTransitionGate.h"
#include "../../Client/game_de/CGameTimingDE.h"
#include "../../Client/game_de/NativeGameTimingProfile.h"
#include "../../Client/game_de/NativeGamePauseProfile.h"
#include "../../Client/game_de/NativeGameFPSProfile.h"
#include "../../Client/game_de/NativeGameStateProfile.h"
#include "../../Client/game_de/NativeCheatProfile.h"
#include "../../Client/game_de/NativeWeatherTransitionProfile.h"
#include "../../Client/game_de/NativeWeatherScalarProfile.h"
#include "../../Client/game_de/NativeWeatherOverridePlan.h"
#include "../../Client/game_de/NativeWeatherOverrideAtomic.h"
#include "../../Client/game_de/NativeWeatherOverridePlacement.h"
#include "../../Client/game_de/NativeWeatherRainProfile.h"
#include "../../Client/game_de/NativePedPoolReader.h"
#include "../../Client/game_de/NativePedTransformReader.h"
#include "../../Client/game_de/NativePedTransformProfile.h"
#include "../../Client/game_de/NativePedWaterFlagsMemory.h"
#include "../../Client/game_de/NativePedWaterTaskReader.h"
#include "../../Client/game_de/NativePedWaterTaskProfile.h"
#include "../../Client/game_de/NativePedWaterFlagsProfile.h"
#include "../../Client/game_de/CPedWaterDE.h"
#include "../../Client/game_de/CPedOrientationDE.h"
#include "../../Client/game_de/NativePedOrientationMemory.h"
#include "../../Client/game_de/NativePedOrientationProfile.h"
#include "../../Client/game_de/NativePedAreaMemory.h"
#include "../../Client/game_de/NativePedFireMemory.h"
#include "../../Client/game_de/NativePedFireProfile.h"
#include "../../Client/game_de/NativePedCollisionMemory.h"
#include "../../Client/game_de/NativePedCollisionProfile.h"
#include "../../Client/game_de/CPedCollisionDE.h"
#include "../../Client/game_de/CPedTasksDE.h"
#include "../../Client/game_de/CPedGogglesDE.h"
#include "../../Client/game_de/NativePedGogglesMemory.h"
#include "../../Client/game_de/NativePedGogglesProfile.h"
#include "../../Client/game_de/CPedVisibilityDE.h"
#include "../../Client/game_de/NativePedVisibilityMemory.h"
#include "../../Client/game_de/NativePedVisibilityProfile.h"
#include "../../Client/game_de/CPedOnScreenDE.h"
#include "../../Client/game_de/NativePedOnScreenCalls.h"
#include "NativePedOnScreenFixture.h"
#include "../../Client/game_de/CPedGiveWeaponDE.h"
#include "../../Client/game_de/NativePedWeaponGrantCalls.h"
#include "../../Client/game_de/CPedClearWeaponDE.h"
#include "../../Client/game_de/NativePedWeaponClearCalls.h"
#include "NativePedWeaponClearFixture.h"
#include "../../Client/game_de/CPedClothesDE.h"
#include "../../Client/game_de/NativePedClothesCalls.h"
#include "NativePedClothesFixture.h"
#include "../../Client/game_de/CPedRebuildDE.h"
#include "../../Client/game_de/NativePedRebuildCalls.h"
#include "NativePedRebuildFixture.h"
#include "NativePedWeaponGrantFixture.h"
#include <game/CGame.h>
#include <game/CModelInfo.h>
#include <game/CWeapon.h>
#include "../../Client/game_de/NativePedAreaProfile.h"
#include "../../Client/game_de/CPedAreaDE.h"
#include "../../Client/game_de/NativeStreamingSpecialModelCalls.h"
#include "../../Client/game_de/NativeStreamingSpecialModelProfile.h"
#include "../../Client/game_de/NativeStreamingFileIdsProfile.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <utility>
#include "../../Client/game_de/NativeAnimationBlockMemory.h"
#include "../../Client/game_de/NativeAnimationBlockRefMemory.h"
#include "../../Client/game_de/NativeAnimationBlockFactoryMemory.h"
#include "../../Client/game_de/NativeAnimationHierarchyHashMemory.h"
#include "../../Client/game_de/NativeAnimationHierarchyMemory.h"
#include "../../Client/game_de/NativePrivateAnimationHierarchyStorage.h"
#include "../../Client/game_de/NativePrivateAnimationHierarchyController.h"
#include "../../Client/game_de/NativePrivateAnimationHierarchyPostProcessProfile.h"
#include "PrivateHierarchyProfile.generated.h"
#include "../../Client/game_de/NativeAnimationHierarchyReadProfile.h"
#include "AnimationHierarchyDEFixture.h"
#include "../../Client/game_de/CKeyGenDE.h"
#include "../../Client/game_de/NativeClassicAnimationNameHash.h"
#include <game/CAnimGatewayNames.h>
#include "../../Client/game_de/CAnimBlockRequestDE.h"
#include "NativeAnimationBlockFactoryFixture.h"
#include "NativeCustomAnimSequenceStorageFixture.h"
#include "../../Client/game_de/NativePedModelMetadataProfile.h"
#include "../../Client/game_de/NativeAnimationSequenceRowCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceRowProfile.h"
#include "../../Client/game_de/NativeAnimationSequenceCopyCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceNameCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameBindCalls.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameBindProfile.h"
#include "../../Client/game_de/NativeAnimationSequenceNameProfile.h"
#include "../../Client/game_de/NativeAnimationSequenceFrameViewCalls.h"
#include "AnimationSequenceDEFixture.h"
#include "../../Client/game_de/NativeAnimationSequenceStorageAccess.h"
#include <cstdlib>
#include "../../Client/game_de/NativeAnimationBlockRefProfile.h"
#include "NativeAnimationBlockRefFixture.h"
#include <game/CAnimBlockRequestPolicy.h>
#include "../../Client/game_de/NativeAnimationBlockProfile.h"
#include "../../Client/game_de/NativeAnimationManagerMemory.h"
#include "../../Client/game_de/NativeAnimationManagerProfile.h"
#include "../../Client/game_de/NativeAnimationGroupArrayProfile.h"
#include "../../Client/game_de/NativeAnimationStaticAssociationProfile.h"
#include "NativeStaticAssociationInitializeFixture.h"
#include "../../Client/game_de/CAnimBlockDE.h"
#include "../../Client/game_de/NativePedPlacementMemory.h"
#include "../../Client/game_de/NativePedPoseMutation.h"
#include "../../Client/game_de/CPedPoseDE.h"
#include "../../Client/game_de/NativePedTeleportMemory.h"
#include "../../Client/game_de/NativePedTeleportProfile.h"
#include "../../Client/game_de/NativePedVelocityMemory.h"
#include "../../Client/game_de/NativePedVelocityProfile.h"
#include "../../Client/game_de/NativePedObservationAccess.h"
#include "../../Client/game_de/NativePedMoveAnimReader.h"
#include "../../Client/game_de/NativePedMoveAnimProfile.h"
#include "../../Client/game_de/CPlayerPedMoveAnimDE.h"
#include "../../Client/game_de/NativeClothesDescriptorCalls.h"
#include "NativeClothesDescriptorFixture.h"
#include "../../Client/game_de/NativePedReferenceMemory.h"
#include "../../Client/game_de/NativePedReferenceProfile.h"
#include "../../Client/game_de/NativePedReferenceController.h"
#include "../../Client/game_de/NativePedVitalsMemory.h"
#include "../../Client/game_de/NativePedPhysicalScalarsMemory.h"
#include "../../Client/game_de/NativePedPhysicalScalarsProfile.h"
#include "../../Client/game_de/NativePedRotationMemory.h"
#include "../../Client/game_de/NativePedRotationProfile.h"
#include "../../Client/game_de/NativePedOxygenMemory.h"
#include "../../Client/game_de/NativePedOxygenProfile.h"
#include "../../Client/game_de/NativePedDuckingMemory.h"
#include "../../Client/game_de/NativePedDuckingProfile.h"
#include "../../Client/game_de/NativePedBikeFlagsMemory.h"
#include "../../Client/game_de/NativePedBikeFlagsProfile.h"
#include "../../Client/game_de/NativePedShotFlagsMemory.h"
#include "../../Client/game_de/NativePedShotFlagsProfile.h"
#include "../../Client/game_de/NativePedBleedingMemory.h"
#include "../../Client/game_de/NativePedBleedingProfile.h"
#include "../../Client/game_de/NativePedStayMemory.h"
#include "../../Client/game_de/NativePedStayProfile.h"
#include "../../Client/game_de/NativePedFightingStyleMemory.h"
#include "../../Client/game_de/NativePedFightingStyleProfile.h"
#include "../../Client/game_de/NativePedWeaponFieldsMemory.h"
#include "../../Client/game_de/NativePedWeaponProfile.h"
#include "../../Client/game_de/NativePedWeaponAmmoMemory.h"
#include "../../Client/game_de/NativePedWeaponAmmoProfile.h"
#include "../../Client/game_de/NativePedCurrentWeaponSlotMemory.h"
#include "../../Client/game_de/NativePedCurrentWeaponSlotProfile.h"
#include "../../Client/game_de/NativeNamedGarageMemory.h"
#include "../../Client/game_de/NativeNamedGarageProfile.h"
#include "../../Client/game_de/NativeNamedGarageGeometryProfile.h"
#include "../../Client/game_de/CPoolsDE.h"
#include "../../Client/game_de/CPedVitalsDE.h"
#include <game/CPedIK.h>
#include "../../Client/game_de/CGarageNamedDE.h"
#include "../../Client/game_de/NativePedVitalsProfile.h"
#include "../../Client/game_de/NativePedMetadataMemory.h"
#include "../../Client/game_de/CPedMetadataDE.h"
#include "../../Client/game_de/NativePedRenderMemory.h"
#include "../../Client/game_de/NativeAnimationAssociationStorage.h"
#include "../../Client/game_de/NativeAnimationAssociationController.h"
#include "../../Client/game_de/NativeAnimationAssociationProfile.h"
#include "../../Client/game_de/NativeAnimationAssociationProgressProfile.h"
#include "../../Client/game_de/CAnimBlendAssociationDE.h"
#include "../../Client/game_de/NativePedRenderProfile.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include "../../Client/game_de/NativePedIntelligenceReader.h"
#include "../../Client/game_de/NativePedIntelligenceProfile.h"
#include "../../Client/game_de/NativeTaskPoolReader.h"
#include "../../Client/game_de/NativeTaskPoolProfile.h"
#include "../../Client/game_de/NativeTaskRemovalCalls.h"
#include "../../Client/game_de/NativeTaskRemovalProfile.h"
#include "../../Client/game_de/NativeModelBankReader.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include "../../Client/game_de/NativeModelLoadedReader.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#include "../../Client/game_de/NativeStreamingRequestCalls.h"
#include "../../Client/game_de/NativeStreamingRequestProfile.h"
#include "../../Client/game_de/NativeStreamingModelContext.h"
#include "../../Client/game_de/NativeModelReferenceMemory.h"
#include "../../Client/game_de/NativeModelReferenceRelease.h"
#include "../../Client/game_de/NativeModelReferenceProfile.h"
#include "../../Client/game_de/NativeStreamingUpgradeReader.h"
#include "../../Client/game_de/NativeStreamingUpgradeCalls.h"
#include "../../Client/game_de/NativeStreamingUpgradeProfile.h"
#include "../../Client/game_de/NativeStreamingLoadAllCalls.h"
#include "../../Client/game_de/NativeStreamingLoadAllProfile.h"
#include "../../Client/game_de/NativeStreamingSceneCalls.h"
#include "../../Client/game_de/NativeWorldGroundCalls.h"
#include "../../Client/game_de/CWorldGroundDE.h"
#include "NativeWorldGroundFixture.h"
#include "../../Client/game_de/NativeWorldLineCalls.h"
#include "../../Client/game_de/CWorldLineDE.h"
#include "NativeWorldLineFixture.h"
#include "../../Client/game_de/NativeWorldAreaMemory.h"
#include "../../Client/game_de/CWorldAreaDE.h"
#include "NativeWorldAreaFixture.h"
#include "../../Client/game_de/NativeWorldJetpackMemory.h"
#include "../../Client/game_de/NativeWorldIgnoredEntityMemory.h"
#include "../../Client/game_de/CWorldIgnoreEntityDE.h"
#include "../../Client/game_de/CCameraSphereVisibleDE.h"
#include "../../Client/game_de/NativeCameraSphereVisibleCalls.h"
#include "../../Client/game_de/NativeStartupMenuCalls.h"
#include "NativeStartupMenuGraphFixture.h"
#include "NativeCameraSphereVisibleFixture.h"
#include "NativeWorldIgnoredEntityFixture.h"
#include "../../Client/game_de/CWorldJetpackHeightDE.h"
#include "NativeWorldJetpackFixture.h"
#include "../../Client/game_de/NativeStreamingSceneProfile.h"
#include "../../Client/game_de/NativeStreamingPriorityCalls.h"
#include "../../Client/game_de/NativeStreamingPriorityProfile.h"
#include "../../Client/game_de/NativeStreamingRemoveCalls.h"
#include "../../Client/game_de/NativeStreamingRemoveProfile.h"
#include "../../Client/game_de/NativeModelClassificationCalls.h"
#include "../../Client/game_de/NativeModelArchiveProfile.h"
#include "../../Client/game_de/NativeStreamingModelGroupsProfile.h"
#include "../../Client/game_de/NativeStreamingResourceCalls.h"
#include "../../Client/game_de/NativeStreamingResourceProfile.h"
#include "../../Client/game_de/NativeModelReferenceTransactions.h"
#include "../../Client/game_de/NativeModelReferenceTransactionProfile.h"
#include "NativeStreamingLoadAllGraphFixture.h"
#include "../../Client/game_de/NativePlayerInfoMemory.h"
#include "../../Client/game_de/NativePlayerInfoProfile.h"
#include "../../Client/game_de/NativePlayerInfoExtraProfile.h"
#include "../../Client/game_de/CPlayerInfoDE.h"
#include "../../Client/game_de/NativeWantedMemory.h"
#include "../../Client/game_de/NativeWantedProfile.h"
#include "../../Client/game_de/CWantedDE.h"
#include "../../Client/game_de/NativeRopesMemory.h"
#include "../../Client/game_de/NativeRopesProfile.h"
#include "../../Client/game_de/CRopesDE.h"
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CWeatherOverridesDE.h"
#include "../../Client/game_de/NativeWeaponInfoProfile.h"
#include "../../Client/game_de/NativeWeaponInfoWriteProfile.h"
#include "../../Client/game_de/NativeWeaponInfoVectorProfile.h"
#include "../../Client/game_de/NativeWeaponInfoExtraProfile.h"
#include "../../Client/game_de/CWeaponStatNativeDE.h"
#include "../../Client/game_de/CWeaponStatManagerDE.h"
#include "../../Client/game_de/NativeStatsWriteCalls.h"
#include "../../Client/game_de/NativeStatsWriteProfile.h"
#include "../../Client/game_de/CStatsDE.h"
#include "../../Client/game_de/NativeStartGameCalls.h"
#include "../../Client/game_de/NativeStartGameProfile.h"
#include "../../Client/game_de/NativeBorrowedGcProfile.h"
#include "../../Client/game_de/NativeUObjectMembershipAccess.h"
#include "../../Client/game_de/NativeUObjectMembershipProfile.h"
#include "../../Client/game_de/NativeStartupCompletionProfile.h"
#include "../../Client/game_de/NativeStartupProvidersProfile.h"
#include "../../Client/game_de/NativeUiClassMapProfile.h"
#include "../../Client/game_de/NativeServiceAdmission.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <intrin.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <tlhelp32.h>
#include <type_traits>
#include <vector>
#include <tuple>
#include <functional>
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlendAssocGroupDE.h"
#include "../../Client/game_de/CAnimBlendStaticAssociationDE.h"
#include "AnimationManagerOwnerFixture.h"
#include "PedPoolRegistryFixture.generated.h"
#include "../../Client/game_de/CGameDE.h"
namespace constructor_services_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified constructor bundle SDK call: %s\n", method);
        std::abort();
    }
}
#include "GameConstructorServicesFixture.generated.h"
namespace world_ground_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified world ground SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldGroundFixture.generated.h"
namespace world_line_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified world line SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldLineFixture.generated.h"
namespace world_area_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified world area SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldAreaFixture.generated.h"
namespace world_jetpack_height_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified world jetpack-height SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldJetpackHeightFixture.generated.h"
namespace ped_pose_fixture
{
    [[noreturn]] inline void Unqualified()
    {
        std::abort();
    }
}
#include "PedPoseFixture.generated.h"
namespace ped_water_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified water fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedWaterFixture.generated.h"
namespace ped_orientation_fixture
{
    // The generated rejecting methods need the same named diagnostic in the
    // combined owner fixture as in the standalone orientation SDK fixture.
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified orientation fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedOrientationFixture.generated.h"
namespace ped_area_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified area fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedAreaFixture.generated.h"
namespace ped_collision_fixture
{
    [[noreturn]] void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unexpected unqualified ped collision fixture call: %s\n", method);
        std::abort();
    }
}
#include "PedCollisionFixture.generated.h"
namespace ped_tasks_fixture
{
    [[noreturn]] void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unexpected unqualified task owner fixture call: %s\n", method);
        std::abort();
    }
}
#include "PedTasksFixture.generated.h"
namespace ped_goggles_fixture
{
    [[noreturn]] void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unexpected unqualified goggles owner fixture call: %s\n", method);
        std::abort();
    }
}
#include "PedGogglesFixture.generated.h"
namespace ped_visibility_fixture
{
    [[noreturn]] void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unexpected unqualified visibility owner fixture call: %s\n", method);
        std::abort();
    }
}
#include "PedVisibilityFixture.generated.h"
namespace ped_on_screen_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified ped on-screen SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedOnScreenFixture.generated.h"
namespace ped_give_weapon_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified ped GiveWeapon SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedGiveWeaponFixture.generated.h"
namespace ped_clear_weapon_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified clear-weapon SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedClearWeaponFixture.generated.h"
namespace ped_clothes_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified clothes SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedClothesFixture.generated.h"
namespace ped_rebuild_fixture
{
    using ped_clothes_fixture::Unqualified;
}
#include "PedRebuildFixture.generated.h"
namespace player_move_anim_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "FAIL: unrelated SDK player method: %s\n", method);
        std::abort();
    }
}
#include "PlayerMoveAnimFixture.generated.h"
namespace ClockPedRebuildOwnerTest
{
    using NativeFunction = mta::game_de::NativePedRebuildCalls::NativeFunction;
    std::uintptr_t                         image = 0, ped = 0, cell = 0, entry = 0, receivedPed = 0;
    unsigned                               calls = 0, effects = 0, scenario = 0;
    mta::game_de::NativePedReferenceToken  selected;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    void (*reenter)() noexcept = nullptr;
    void (*afterNative)(void*) noexcept = nullptr;
    void*                     afterContext = nullptr;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uintptr_t receiver, std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function);
        receivedPed = receiver;
        if (callerToken)
        {
            ++callerToken->version;
            callerToken = nullptr;
        }
        if (reenter)
            reenter();
        if (afterNative)
            afterNative(afterContext);
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockPedClothesOwnerTest
{
    using NativeFunction = mta::game_de::NativePedClothesCalls::NativeFunction;
    std::uintptr_t                         image = 0, ped = 0, cell = 0, entry = 0, receivedClothes = 0;
    unsigned                               calls = 0, effects = 0, scenario = 0;
    std::uint8_t                           receivedPart = 0;
    bool                                   textureNull = false, modelNull = false;
    std::array<char, 1025>                 receivedTexture{}, receivedModel{};
    mta::game_de::NativePedReferenceToken  selected;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    char*                                  callerTexture = nullptr;
    void (*reenter)() noexcept = nullptr;
    void (*afterNative)(void*) noexcept = nullptr;
    void*                     afterContext = nullptr;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uintptr_t clothes, const char* texture, const char* model, std::uint8_t part,
                                     std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function);
        receivedClothes = clothes;
        receivedPart = part;
        textureNull = texture == nullptr;
        modelNull = model == nullptr;
        receivedTexture.fill(0);
        receivedModel.fill(0);
        if (texture)
            std::memcpy(receivedTexture.data(), texture, std::strlen(texture) + 1);
        if (model)
            std::memcpy(receivedModel.data(), model, std::strlen(model) + 1);
        // Native descriptor/name-cache contents are committed effects, not
        // an old byte image for the owner to replay during acknowledgement.
        std::memcpy(reinterpret_cast<void*>(clothes), &effects, sizeof(effects));
        if (callerToken)
        {
            ++callerToken->version;
            callerToken = nullptr;
        }
        if (callerTexture)
        {
            callerTexture[0] = '!';
            callerTexture = nullptr;
        }
        if (reenter)
            reenter();
        if (afterNative)
            afterNative(afterContext);
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockPedWeaponClearOwnerTest
{
    using NativeFunction = mta::game_de::NativePedWeaponClearCalls::NativeFunction;
    std::uintptr_t                         image = 0, ped = 0, cell = 0, entry = 0, receivedPed = 0;
    std::uint32_t                          receivedType = 0;
    unsigned                               calls = 0, effects = 0, scenario = 0;
    mta::game_de::NativePedReferenceToken  selected;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    void (*reenter)() noexcept = nullptr;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uintptr_t receiver, std::uint32_t type, std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function);
        receivedPed = receiver;
        receivedType = type;
        if (callerToken)
        {
            ++callerToken->version;
            callerToken = nullptr;
        }
        if (reenter)
            reenter();
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace world_ignore_entity_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified world ignored-entity SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldIgnoreEntityFixture.generated.h"
namespace camera_sphere_visible_fixture
{
    [[noreturn]] inline void Unqualified(const char* method)
    {
        std::fprintf(stderr, "Unqualified camera sphere SDK call: %s\n", method);
        std::abort();
    }
}
#include "CameraSphereVisibleFixture.generated.h"

namespace ClockCameraSphereOwnerTest
{
    using NativeFunction = mta::game_de::NativeCameraSphereVisibleCalls::NativeFunction;
    std::uintptr_t image = 0, entry = 0, camera = 0, centerAddress = 0;
    unsigned       calls = 0, scenario = 0;
    CVector        receivedCenter;
    float          receivedRadius = 0;
    bool           next = false;
    CVector*       callerCenter = nullptr;
    void (*reenter)() noexcept = nullptr;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uintptr_t receiver, const CVector& center, float radius, bool& output,
                                     std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function);
        camera = receiver;
        centerAddress = reinterpret_cast<std::uintptr_t>(&center);
        std::memcpy(&receivedCenter, &center, sizeof(center));
        std::memcpy(&receivedRadius, &radius, sizeof(radius));
        if (callerCenter)
        {
            callerCenter->fX = 100;
            callerCenter->fY = 200;
            callerCenter->fZ = 300;
            callerCenter = nullptr;
        }
        if (reenter)
            reenter();
        output = next;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
            *reinterpret_cast<std::uint32_t*>(image + mta::game_de::NativeGameStateProfile::stateRva) = 0;
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockModelClassificationOwnerTest
{
    using NativeFunction = mta::game_de::NativeModelClassificationCalls::NativeFunction;
    std::uintptr_t            image = 0, entry = 0, replacementModel = 0;
    std::uint32_t             key = 0;
    unsigned                  calls = 0, scenario = 0;
    unsigned char             value = 0;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uint32_t model, unsigned char& output, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        key = model;
        output = value;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        SIZE_T written = 0;
        if (scenario == 2 &&
            (!::WriteProcessMemory(GetCurrentProcess(),
                                   reinterpret_cast<void*>(image + mta::game_de::NativeModelBankProfile::BankRva + model * sizeof(std::uintptr_t)),
                                   &replacementModel, sizeof(replacementModel), &written) ||
             written != sizeof(replacementModel)))
            std::abort();
        if (scenario == 3)
        {
            const std::uintptr_t missing = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeStreamingRequestProfile::RootRva), &missing,
                                      sizeof(missing), &written) ||
                written != sizeof(missing))
                std::abort();
        }
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockStreamingRemoveOwnerTest
{
    using NativeFunction = mta::game_de::NativeStreamingRemoveCalls::NativeFunction;
    std::uintptr_t            image = 0, entry = 0;
    std::uint32_t             key = 0;
    unsigned                  calls = 0, scenario = 0;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uint32_t model, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        key = model;
        // State0 is the genuine native early return: do not manufacture an
        // unload, clear flags, decrement a counter or run an inverse operation.
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            auto* flags = reinterpret_cast<unsigned char*>(image + mta::game_de::NativeModelLoadedProfile::ArrayRva +
                                                           model * mta::game_de::NativeModelLoadedProfile::RowStride +
                                                           mta::game_de::NativeModelLoadedProfile::FlagsOffset);
            *flags = static_cast<unsigned char>(*flags | 0x40);
        }
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockStreamingPriorityOwnerTest
{
    using Calls = mta::game_de::NativeStreamingPriorityCalls::WaitFunction;
    std::uintptr_t image = 0, root = 0, waitEntry = 0, drainEntry = 0, handle = 0;
    std::uint32_t  model = 17;
    unsigned       waits = 0, drains = 0, scenario = 0;
    mta::game_de::ClockResult (*reenterTrue)() noexcept = nullptr;
    mta::game_de::ClockResult (*reenterFalse)() noexcept = nullptr;
    mta::game_de::ClockResult nestedTrue = mta::game_de::ClockResult::Ok, nestedFalse = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult Wait(Calls function, std::uintptr_t selected, std::int32_t& nativeResult, std::uint32_t& exception) noexcept
    {
        ++waits;
        waitEntry = reinterpret_cast<std::uintptr_t>(function) - image;
        handle = selected;
        nativeResult = 2;
        *reinterpret_cast<unsigned char*>(root + 0x78) = 1;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t missing = 0;
            SIZE_T               written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeStreamingLoadAllProfile::RootRva), &missing,
                                      sizeof(missing), &written) ||
                written != sizeof(missing))
                std::abort();
        }
        if (scenario == 3)
        {
            nestedTrue = reenterTrue();
            nestedFalse = reenterFalse();
        }
        return mta::game_de::ClockResult::Ok;
    }
    mta::game_de::ClockResult Drain(mta::game_de::NativeStreamingPriorityCalls::DrainFunction function, std::uintptr_t selected,
                                    std::uint32_t& exception) noexcept
    {
        ++drains;
        drainEntry = reinterpret_cast<std::uintptr_t>(function) - image;
        if (selected != root)
            std::abort();
        if (scenario == 4)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 5)
            return mta::game_de::ClockResult::Ok;
        // Model the genuine callback's selected completion and detached array.
        // The adapter never writes these flags or holds the retired handle.
        auto* state = reinterpret_cast<unsigned char*>(image + mta::game_de::NativeStreamingPriorityProfile::RowsRva +
                                                       model * mta::game_de::NativeStreamingPriorityProfile::RowBytes + 12);
        state[0] = 0;
        state[1] = 1;
        const std::array<unsigned char, 16> empty{};
        std::memcpy(reinterpret_cast<void*>(root + 0x80), empty.data(), empty.size());
        *reinterpret_cast<unsigned char*>(root + 0x78) = 0;
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockStreamingLoadAllOwnerTest
{
    using NativeFunction = mta::game_de::NativeStreamingLoadAllCalls::NativeFunction;
    std::uintptr_t image = 0, world = 0, level = 0, entry = 0;
    unsigned       calls = 0, effects = 0, scenario = 0;
    mta::game_de::ClockResult (*reenter)() noexcept = nullptr;
    mta::game_de::ClockResult nested = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        auto* mode = reinterpret_cast<unsigned char*>(world + mta::game_de::NativeStreamingLoadAllProfile::WorldModeOffset);
        if (*mode == 2)
            *mode = 1;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(level + 0xd0), &zero, sizeof(zero));
        }
        if (scenario == 3 && reenter)
            nested = reenter();
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockWorldLineOwnerTest
{
    using NativeFunction = mta::game_de::NativeWorldLineCalls::NativeFunction;
    using Flags = mta::game_de::NativeWorldLineCalls::Flags;
    std::uintptr_t image = 0, level = 0, entry = 0;
    unsigned       calls = 0, effects = 0, scenario = 0;
    bool           clear = false, copied = false;
    CVector        receivedStart, receivedEnd;
    Flags          receivedFlags{};
    const CVector* startSource = nullptr;
    const CVector* endSource = nullptr;
    void (*reenter)() noexcept = nullptr;
    mta::game_de::ClockResult Invoke(NativeFunction function, const CVector& start, const CVector& end, const Flags& flags, bool& output,
                                     std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        receivedStart = start;
        receivedEnd = end;
        receivedFlags = flags;
        copied = &start != startSource && &end != endSource;
        output = clear;
        // This final-entry seam records a persistent accepted prefix without
        // implementing native sector traversal, physics or matrix allocation.
        if (scenario == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 3)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(level + 0xd0), &zero, sizeof(zero));
        }
        if (scenario == 4 && reenter)
            reenter();
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockWorldGroundOwnerTest
{
    using NativeFunction = mta::game_de::NativeWorldGroundCalls::NativeFunction;
    std::uintptr_t image = 0, level = 0, entry = 0;
    unsigned       calls = 0, effects = 0, scenario = 0;
    CVector        received;
    const CVector* source = nullptr;
    bool           copied = false;
    void (*reenter)() noexcept = nullptr;
    mta::game_de::ClockResult nested = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult Invoke(NativeFunction function, const CVector& position, bool* found, std::uintptr_t* entity, CVector* normal, float& output,
                                     std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        received = position;
        copied = &position != source;
        if (!found || entity || normal)
            std::abort();
        *found = true;
        constexpr std::uint32_t heightBits = 0x42f68001;
        std::memcpy(&output, &heightBits, sizeof(output));
        // This seam observes a single native entry and accepted effect; it
        // does not implement sector intersection or native matrix allocation.
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(level + 0xd0), &zero, sizeof(zero));
        }
        if (scenario == 3 && reenter)
            reenter();
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockStreamingSceneOwnerTest
{
    using NativeFunction = mta::game_de::NativeStreamingSceneCalls::NativeFunction;
    std::uintptr_t image = 0, target = 0, world = 0, level = 0, entry = 0;
    unsigned       calls = 0, effects = 0, scenario = 0;
    CVector        received;
    const CVector* source = nullptr;
    bool           copied = false;
    void (*reenter)() noexcept = nullptr;
    std::array<mta::game_de::ClockResult, 5> nested{};
    mta::game_de::ClockResult                Invoke(NativeFunction function, const CVector* position, std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        std::memcpy(&received, position, sizeof(received));
        copied = position != source;
        // Model one persistent engine effect at the final native boundary;
        // actual scene/streaming implementations remain genuine engine code.
        reinterpret_cast<unsigned char*>(target)[mta::game_de::NativeStreamingSceneProfile::TargetBytes - 1] = 0x6b;
        auto* mode = reinterpret_cast<unsigned char*>(world + mta::game_de::NativeStreamingLoadAllProfile::WorldModeOffset);
        if (*mode == 2)
            *mode = 1;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(level + 0xd0), &zero, sizeof(zero));
        }
        if (scenario == 3 && reenter)
            reenter();
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockStreamingResourceOwnerTest
{
    using NativeFunction = mta::game_de::NativeStreamingResourceCalls::NativeFunction;
    std::uintptr_t            image = 0, entry = 0;
    unsigned                  calls = 0, scenario = 0;
    std::uint32_t             index = 0, flags = 0;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uint32_t model, std::uint32_t requestFlags, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        index = model;
        flags = requestFlags;
        const auto row = image + mta::game_de::NativeModelLoadedProfile::ArrayRva + model * mta::game_de::NativeModelLoadedProfile::RowStride;
        auto*      bytes = reinterpret_cast<unsigned char*>(row + mta::game_de::NativeModelLoadedProfile::FlagsOffset);
        bytes[0] |= static_cast<unsigned char>(requestFlags);
        bytes[1] = 1;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            SIZE_T               written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeStreamingResourceProfile::RootRva), &zero,
                                      sizeof(zero), &written) ||
                written != sizeof(zero))
                std::abort();
        }
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockModelAcquireOwnerTest
{
    using namespace mta::game_de::NativeModelReferenceTransactions;
    std::uintptr_t image = 0;
    unsigned       reserves = 0, publishes = 0, scenario = 0;
    ReservedRow*   Reserve(std::uintptr_t map, ReservedRow* output)
    {
        ++reserves;
        std::uintptr_t rows = 0;
        std::memcpy(&rows, reinterpret_cast<void*>(map), sizeof(rows));
        const std::int32_t  two = 2;
        const std::uint32_t bits = 3;
        std::memcpy(reinterpret_cast<void*>(map + 8), &two, sizeof(two));
        std::memcpy(reinterpret_cast<void*>(map + 0x28), &two, sizeof(two));
        std::memcpy(reinterpret_cast<void*>(map + 0x10), &bits, sizeof(bits));
        *output = {1, 0, rows + 16};
        if (scenario == 1)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        return output;
    }
    std::int32_t* Publish(std::uintptr_t map, std::int32_t* output, std::int32_t key, std::uintptr_t row, std::int32_t reserved)
    {
        ++publishes;
        if (reserved != 1 || *reinterpret_cast<std::uint32_t*>(row) != static_cast<std::uint32_t>(key) || *reinterpret_cast<std::uint32_t*>(row + 4) != 0)
            std::abort();
        const std::int32_t zero = 0, one = 1;
        std::memcpy(reinterpret_cast<void*>(row + 8), &zero, sizeof(zero));
        std::memcpy(reinterpret_cast<void*>(row + 0xc), &zero, sizeof(zero));
        std::memcpy(reinterpret_cast<void*>(map + 0x38), &one, sizeof(one));
        *output = 1;
        if (scenario == 2)
        {
            const std::uintptr_t missing = 0;
            SIZE_T               written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeApplicationFrameProfile::rootRva), &missing,
                                      sizeof(missing), &written) ||
                written != sizeof(missing))
                std::abort();
        }
        return output;
    }
}

namespace ClockTaskRemovalOwnerTest
{
    using Calls = mta::game_de::NativeTaskRemovalCalls::NativeFunction;
    unsigned                  calls = 0, deletions = 0;
    std::uintptr_t            image = 0, entry = 0, manager = 0, flag = 0, referenceCell = 0;
    std::int32_t              index = -1;
    bool                      clearReference = false, sawLiveManagerCellDuringDeletion = false;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult Invoke(Calls function, std::uintptr_t passedManager, std::int32_t passedIndex, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        manager = passedManager;
        index = passedIndex;
        const auto     cell = manager + 0x28 + static_cast<std::uintptr_t>(index) * sizeof(std::uintptr_t);
        std::uintptr_t task = 0;
        std::memcpy(&task, reinterpret_cast<void*>(cell), sizeof(task));
        if (!task || !flag)
            std::abort();
        // Substitute only the final typed native invocation. Model its proved
        // task-bank retirement before manager-cell clear on owned buffers;
        // all production owner, pool, code and post-call checks still run.
        auto* generation = reinterpret_cast<unsigned char*>(flag);
        *generation = static_cast<unsigned char>(*generation | 0x80);
        ++deletions;
        std::uintptr_t stillPresent = 0;
        std::memcpy(&stillPresent, reinterpret_cast<void*>(cell), sizeof(stillPresent));
        sawLiveManagerCellDuringDeletion = stillPresent == task;
        const std::uintptr_t zero = 0;
        std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        if (clearReference)
        {
            clearReference = false;
            std::memcpy(reinterpret_cast<void*>(referenceCell), &zero, sizeof(zero));
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

namespace ClockModelBankOwnerTest
{
    std::uintptr_t slot = 0, advanced = 0;
    unsigned       changes = 0;
    bool           advanceOnRead = false;
}

namespace ClockModelLoadedOwnerTest
{
    std::uintptr_t               address = 0;
    std::array<unsigned char, 2> advanced{};
    unsigned                     changes = 0;
    bool                         advanceOnRead = false;
}

namespace ClockStreamingRequestOwnerTest
{
    using NativeFunction = mta::game_de::NativeStreamingRequestCalls::NativeFunction;
    std::uintptr_t image = 0, entry = 0, root = 0, replacementRoot = 0;
    std::uint32_t  modelIndex = 0, flags = 0;
    unsigned       calls = 0;
    bool           changeRoot = false;
    mta::game_de::ClockResult (*reenterScene)() noexcept = nullptr;
    mta::game_de::ClockResult nestedScene = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uint32_t model, std::uint32_t requestedFlags, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        modelIndex = model;
        flags = requestedFlags;
        const auto row =
            image + mta::game_de::NativeModelLoadedProfile::ArrayRva + static_cast<std::uintptr_t>(model) * mta::game_de::NativeModelLoadedProfile::RowStride;
        unsigned char before = 0;
        std::memcpy(&before, reinterpret_cast<void*>(row + mta::game_de::NativeModelLoadedProfile::FlagsOffset), 1);
        const std::array<unsigned char, 2> accepted{static_cast<unsigned char>(before | static_cast<unsigned char>(requestedFlags)), 2};
        SIZE_T                             written = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(row + mta::game_de::NativeModelLoadedProfile::FlagsOffset), accepted.data(),
                                  accepted.size(), &written) ||
            written != accepted.size())
            std::abort();
        if (reenterScene)
            nestedScene = reenterScene();
        if (changeRoot)
        {
            changeRoot = false;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeApplicationFrameProfile::rootRva),
                                      &replacementRoot, sizeof(replacementRoot), &written) ||
                written != sizeof(replacementRoot))
                std::abort();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

// These seams replace only the final native operation on owned test memory.
// Image/type/registry/loaded-row/callback admission stays production code.
namespace ClockModelReferenceOwnerTest
{
    std::uintptr_t            image = 0, address = 0;
    std::uint32_t             before = 0, target = 0;
    unsigned                  calls = 0;
    bool                      conflict = false, loseRoot = false;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult Exchange(std::uintptr_t passed, std::uint32_t expected, std::uint32_t next, std::uint32_t& observed,
                                       std::uint32_t& exception) noexcept
    {
        ++calls;
        address = passed;
        before = expected;
        target = next;
        if (conflict)
        {
            const auto advanced = expected + 7;
            std::memcpy(reinterpret_cast<void*>(passed), &advanced, sizeof(advanced));
            conflict = false;
        }
        LONG e = 0, t = 0;
        std::memcpy(&e, &expected, sizeof(e));
        std::memcpy(&t, &next, sizeof(t));
        const auto prior = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(passed), t, e);
        std::memcpy(&observed, &prior, sizeof(observed));
        if (loseRoot)
        {
            loseRoot = false;
            const std::uintptr_t zero = 0;
            SIZE_T               written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeApplicationFrameProfile::rootRva), &zero,
                                      sizeof(zero), &written) ||
                written != sizeof(zero))
                std::abort();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}
namespace ClockStreamingUpgradeOwnerTest
{
    using NativeFunction = mta::game_de::NativeStreamingUpgradeCalls::NativeFunction;
    std::uintptr_t            image = 0, entry = 0;
    std::uint32_t             modelIndex = 0, flags = 0, companion = 1001;
    unsigned                  calls = 0;
    bool                      loseRoot = false;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    mta::game_de::ClockResult Invoke(NativeFunction function, std::uint32_t model, std::uint32_t requestedFlags, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        modelIndex = model;
        flags = requestedFlags;
        const auto accept = [&](std::uint32_t key)
        {
            namespace L = mta::game_de::NativeModelLoadedProfile;
            const auto    address = image + L::ArrayRva + static_cast<std::uintptr_t>(key) * L::RowStride + L::FlagsOffset;
            unsigned char prior = 0;
            std::memcpy(&prior, reinterpret_cast<void*>(address), 1);
            const std::array<unsigned char, 2> accepted{static_cast<unsigned char>(prior | static_cast<unsigned char>(requestedFlags)), 2};
            SIZE_T                             written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), accepted.data(), accepted.size(), &written) ||
                written != accepted.size())
                std::abort();
        };
        accept(model);
        if (companion != 0xffffffff)
            accept(companion);
        if (loseRoot)
        {
            loseRoot = false;
            const std::uintptr_t zero = 0;
            SIZE_T               written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeApplicationFrameProfile::rootRva), &zero,
                                      sizeof(zero), &written) ||
                written != sizeof(zero))
                std::abort();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

namespace ClockStatsOwnerTest
{
    unsigned                  calls = 0;
    std::uintptr_t            image = 0, entry = 0;
    std::uint32_t             index = 0;
    float                     amount = 0;
    bool                      effect = false;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    void (*after)() = nullptr;
    mta::game_de::ClockResult Invoke(mta::game_de::NativeStatsWriteCalls::NativeFunction function, std::uint32_t promotedIndex, float passedAmount,
                                     std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        index = promotedIndex;
        amount = passedAmount;
        if (effect)
        {
            // Only the final native-call boundary is substituted. A deliberately
            // different resulting value stands for an accepted native reaction;
            // owner cleanup must not replay or snapshot-rewind that effect.
            const float value = passedAmount + 10.0f;
            SIZE_T      written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(),
                                      reinterpret_cast<void*>(image + mta::game_de::NativeStatsProfile::FloatBankRva + promotedIndex * sizeof(float)), &value,
                                      sizeof(value), &written) ||
                written != sizeof(value))
                std::abort();
        }
        if (after)
        {
            const auto next = after;
            after = nullptr;
            next();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

namespace ClockPedReferenceOwnerTest
{
    using Memory = mta::game_de::NativePedReferenceMemory;
    unsigned                  registers = 0, unregisters = 0;
    std::uintptr_t            image = 0, entry = 0, cell = 0, node = 0;
    bool                      forced = false, partialUnlink = false;
    mta::game_de::ClockResult unregisterResult = mta::game_de::ClockResult::Ok;
    template <class T>
    T Get(std::uintptr_t at)
    {
        T value{};
        std::memcpy(&value, reinterpret_cast<void*>(at), sizeof(value));
        return value;
    }
    template <class T>
    void Put(std::uintptr_t at, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(at), &value, sizeof(value));
    }
    mta::game_de::ClockResult Register(Memory::RegisterFunction function, void* target, void** word, bool force, std::uint32_t&) noexcept
    {
        namespace P = mta::game_de::NativePedReferenceProfile;
        ++registers;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        const auto ped = reinterpret_cast<std::uintptr_t>(target);
        cell = reinterpret_cast<std::uintptr_t>(word);
        forced = force;
        node = Get<std::uintptr_t>(image + P::FreeHeadRva);
        if (node)
        {
            Put(image + P::FreeHeadRva, Get<std::uintptr_t>(node));
            Put(node, Get<std::uintptr_t>(ped + P::EntityHeadOffset));
            Put(node + 8, cell);
            Put(ped + P::EntityHeadOffset, node);
        }
        // The real registrar consumes a preassigned stable word; it does not
        // assign that word and can return without allocating a native node.
        return mta::game_de::ClockResult::Ok;
    }
    mta::game_de::ClockResult Unregister(Memory::UnregisterFunction function, void* target, void** word, std::uint32_t& exception) noexcept
    {
        namespace P = mta::game_de::NativePedReferenceProfile;
        ++unregisters;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        cell = reinterpret_cast<std::uintptr_t>(word);
        auto link = reinterpret_cast<std::uintptr_t>(target) + P::EntityHeadOffset;
        auto current = Get<std::uintptr_t>(link);
        for (std::size_t i = 0; current && i < P::ReferenceCount; ++i)
        {
            if (Get<std::uintptr_t>(current + 8) == cell)
            {
                node = current;
                Put(link, Get<std::uintptr_t>(current));
                Put(current, Get<std::uintptr_t>(image + P::FreeHeadRva));
                Put(image + P::FreeHeadRva, current);
                if (!partialUnlink)
                    Put(current + 8, std::uintptr_t{0});
                break;
            }
            link = current;
            current = Get<std::uintptr_t>(current);
        }
        // Schedule only an entered final-call fault. Production bank scans,
        // allocation ownership and the real callback recovery grant remain.
        if (unregisterResult != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return unregisterResult;
    }
}
namespace ClockPedVitalsOwnerTest
{
    unsigned       stores = 0;
    std::uintptr_t image = 0, flag = 0;
    bool           loseIdentity = false;
    LONG WINAPI    Exchange(volatile LONG* address, LONG target, LONG expected)
    {
        ++stores;
        const auto observed = _InterlockedCompareExchange(address, target, expected);
        if (loseIdentity && observed == expected)
        {
            loseIdentity = false;
            // Only the actual CAS is wrapped. Its accepted effect survives;
            // the unchanged production postcheck must detect pool identity loss.
            unsigned char previous = 0;
            std::memcpy(&previous, reinterpret_cast<void*>(flag), sizeof(previous));
            const auto different = static_cast<unsigned char>(previous ^ 1);
            std::memcpy(reinterpret_cast<void*>(flag), &different, sizeof(different));
        }
        return observed;
    }
}
namespace ClockTaskPoolOwnerTest
{
    std::uintptr_t address = 0, flag = 0;
    bool           changeOnRead = false;
    unsigned       changes = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = ::ReadProcessMemory(process, source, destination, bytes, copied);
        if (result && changeOnRead && reinterpret_cast<std::uintptr_t>(source) == address && bytes == sizeof(std::uintptr_t))
        {
            changeOnRead = false;
            unsigned char current = 0;
            std::memcpy(&current, reinterpret_cast<void*>(flag), 1);
            current = static_cast<unsigned char>((current + 1) & 0x7f);
            // Only the actual task-bank generation advances after a real OS
            // vptr copy; no fake callback grant or native task method exists.
            std::memcpy(reinterpret_cast<void*>(flag), &current, 1);
            ++changes;
        }
        return result;
    }
}
namespace ClockPedIntelligenceOwnerTest
{
    std::uintptr_t address = 0, cell = 0;
    bool           clearOnRead = false;
    unsigned       invalidations = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = ::ReadProcessMemory(process, source, destination, bytes, copied);
        if (result && clearOnRead && reinterpret_cast<std::uintptr_t>(source) == address && bytes == mta::game_de::NativePedIntelligenceProfile::PrefixBytes)
        {
            clearOnRead = false;
            const std::uintptr_t zero = 0;
            // Only registered-cell acknowledgement is lost after the real
            // native prefix copy. The qualified ped/intelligence remain live.
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
            ++invalidations;
        }
        return result;
    }
}

namespace ClockModuleClothesInitializerTest
{
    std::uintptr_t                                image = 0, entry = 0, descriptor = 0;
    unsigned                                      calls = 0, effects = 0, scenario = 0;
    mta::game_de::NativeClothesDescriptorStorage* nested = nullptr;
    mta::game_de::ClockResult (*reenter)(mta::game_de::NativeClothesDescriptorStorage&) noexcept = nullptr;
    mta::game_de::ClockResult nestedResult = mta::game_de::ClockResult::NotInitialized;
    void (*after)(void*) noexcept = nullptr;
    void*                     opaque = nullptr;
    mta::game_de::ClockResult Invoke(mta::game_de::NativeClothesDescriptorCalls::NativeFunction function, std::uintptr_t storage,
                                     std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function);
        descriptor = storage;
        // Substitute only the final native invocation, with the exact proved
        // descriptor footprint. The genuine owner/world/name guards still run.
        std::memset(reinterpret_cast<void*>(storage), 0, mta::game_de::NativeClothesDescriptorProfile::DescriptorBytes);
        ++effects;
        if (nested && reenter)
            nestedResult = reenter(*nested);
        if (after)
            after(opaque);
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const LONG active = 1;
            std::memcpy(reinterpret_cast<void*>(image + mta::game_de::NativeBorrowedGcProfile::markerRva), &active, sizeof(active));
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockPedMoveAnimOwnerTest
{
    std::uintptr_t                         address = 0, cell = 0, flagAddress = 0;
    unsigned                               reads = 0, failAt = 0, changeAt = 0, recycleAt = 0, loseAt = 0;
    std::int32_t                           replacement = 0;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    bool                                   changeCaller = false;
    void                                   Reset() noexcept
    {
        reads = failAt = changeAt = recycleAt = loseAt = 0;
        callerToken = nullptr;
        changeCaller = false;
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const bool selected = reinterpret_cast<std::uintptr_t>(source) == address && bytes == sizeof(std::int32_t);
        if (selected && ++reads == failAt)
        {
            if (copied)
                *copied = 0;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, source, destination, bytes, copied);
        if (result && selected)
        {
            // Damage only the exact live fixture buffers after the real OS
            // copy. All native identity and registered-cell bookends remain.
            if (reads == changeAt)
                std::memcpy(reinterpret_cast<void*>(address), &replacement, sizeof(replacement));
            if (reads == recycleAt)
                *reinterpret_cast<unsigned char*>(flagAddress) = 2;
            if (reads == loseAt)
            {
                const std::uintptr_t zero = 0;
                std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
            }
            if (changeCaller && callerToken)
            {
                changeCaller = false;
                ++callerToken->version;
            }
        }
        return result;
    }
}
namespace ClockPedObservationOwnerTest
{
    std::uintptr_t address = 0, cell = 0;
    bool           clearOnRead = false;
    unsigned       invalidations = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = ::ReadProcessMemory(process, source, destination, bytes, copied);
        if (result && clearOnRead && reinterpret_cast<std::uintptr_t>(source) == address && bytes == sizeof(CVector))
        {
            clearOnRead = false;
            const std::uintptr_t zero = 0;
            // The raw reader still has a valid pool row. Only its independently
            // registered weak word is invalidated after a successful OS copy.
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
            ++invalidations;
        }
        return result;
    }
}
namespace ClockPedPhysicalScalarsOwnerTest
{
    unsigned       stores = 0, failAt = 0, loseAt = 0;
    std::uintptr_t cell = 0;
    bool           loseToken = false;
    bool           Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++stores;
        if (stores == failAt)
        {
            // Only the actual component-CAS boundary faults. A prior accepted
            // component remains in owned native-style memory for the real
            // callback owner to acknowledge as persistent partial intent.
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        LONG compare = 0, desired = 0;
        std::memcpy(&compare, &expected, sizeof(compare));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), desired, compare);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (loseToken && observed == expected && (!loseAt || stores == loseAt))
        {
            loseToken = false;
            // The real scalar store succeeded. Clearing only its native weak
            // word tests the Runtime's independent token acknowledgement; the
            // node still names the cell, so no storage may be released yet.
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return true;
    }
}
namespace ClockPedFireOwnerTest
{
    std::uintptr_t watched = 0, cell = 0, replacement = 0;
    unsigned       reads = 0, changeAfter = 0;
    bool           failNext = false, loseToken = false;
    BOOL WINAPI    Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const bool selected = reinterpret_cast<std::uintptr_t>(source) == watched && bytes == sizeof(std::uintptr_t);
        if (selected && failNext)
        {
            failNext = false;
            if (copied)
                *copied = 0;
            return FALSE;
        }
        if (!::ReadProcessMemory(process, source, destination, bytes, copied))
            return FALSE;
        if (selected)
        {
            ++reads;
            if (changeAfter && reads == changeAfter)
                std::memcpy(reinterpret_cast<void*>(watched), &replacement, sizeof(replacement));
            if (loseToken && reads == 2)
            {
                loseToken = false;
                const std::uintptr_t zero = 0;
                std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
            }
        }
        return TRUE;
    }
}
namespace ClockPedGogglesOwnerTest
{
    std::uintptr_t                         watched = 0, replacement = 0, cell = 0;
    unsigned                               reads = 0, changeAfter = 0;
    bool                                   failNext = false, loseToken = false;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    BOOL WINAPI                            Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const bool selected = reinterpret_cast<std::uintptr_t>(source) == watched && bytes == sizeof(std::uintptr_t);
        if (selected && failNext)
        {
            failNext = false;
            if (copied)
                *copied = 0;
            return FALSE;
        }
        if (!::ReadProcessMemory(process, source, destination, bytes, copied))
            return FALSE;
        if (selected)
        {
            ++reads;
            if (callerToken)
            {
                ++callerToken->version;
                callerToken = nullptr;
            }
            if (changeAfter && reads == changeAfter)
                std::memcpy(reinterpret_cast<void*>(watched), &replacement, sizeof(replacement));
            if (loseToken && reads == 2)
            {
                loseToken = false;
                const std::uintptr_t zero = 0;
                std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
            }
        }
        return TRUE;
    }
}
namespace ClockPedWeaponGrantOwnerTest
{
    using Calls = mta::game_de::NativePedWeaponGrantCalls::NativeFunction;
    std::uintptr_t                         image = 0, ped = 0, cell = 0, entry = 0, receivedPed = 0;
    std::uint32_t                          receivedType = 0, receivedAmmo = 0, nextSlot = 2;
    std::int32_t                           receivedUnused = 0;
    unsigned                               calls = 0, effects = 0, scenario = 0;
    mta::game_de::NativePedReferenceToken  selected;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    void (*reenter)() noexcept = nullptr;
    mta::game_de::ClockResult Invoke(Calls function, std::uintptr_t receiver, std::uint32_t type, std::uint32_t ammo, std::int32_t unused, std::uint32_t& slot,
                                     std::uint32_t& exception) noexcept
    {
        ++calls;
        ++effects;
        entry = reinterpret_cast<std::uintptr_t>(function);
        receivedPed = receiver;
        receivedType = type;
        receivedAmmo = ammo;
        receivedUnused = unused;
        if (callerToken)
        {
            ++callerToken->version;
            callerToken = nullptr;
        }
        if (reenter)
            reenter();
        slot = nextSlot;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockPedOnScreenOwnerTest
{
    namespace Calls = mta::game_de::NativePedOnScreenCalls;
    std::uintptr_t                         image = 0, ped = 0, cell = 0;
    std::array<unsigned, 3>                calls{};
    unsigned                               scenario = 0;
    bool                                   nativeValue = false;
    mta::game_de::NativePedReferenceToken  selected;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    void (*reenter)() noexcept = nullptr;
    const std::array<float, 3> centerValue{1.25f, -2.5f, 3.75f};
    mta::game_de::ClockResult  Invoke(Calls::RadiusFunction function, std::uintptr_t receiver, float& radius, std::uint32_t& exception) noexcept
    {
        ++calls[0];
        std::fprintf(stderr, "OnScreen Radius entry: fn=%llx expected=%llx receiver=%llx ped=%llx image=%llx calls=%u,%u,%u scenario=%u\n",
                     static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(function)),
                     static_cast<unsigned long long>(image + mta::game_de::NativePedOnScreenProfile::RadiusRva), static_cast<unsigned long long>(receiver),
                     static_cast<unsigned long long>(ped), static_cast<unsigned long long>(image), calls[0], calls[1], calls[2], scenario);
        if (reinterpret_cast<std::uintptr_t>(function) != image + mta::game_de::NativePedOnScreenProfile::RadiusRva || receiver != ped)
            std::abort();
        if (callerToken)
        {
            ++callerToken->version;
            callerToken = nullptr;
        }
        if (reenter)
            reenter();
        radius = 8.5f;
        if (scenario == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return mta::game_de::ClockResult::Ok;
    }
    mta::game_de::ClockResult Invoke(Calls::CenterFunction function, std::uintptr_t receiver, CVector& center, CVector*& returned,
                                     std::uint32_t& exception) noexcept
    {
        ++calls[1];
        std::fprintf(stderr, "OnScreen Center entry: fn=%llx expected=%llx receiver=%llx ped=%llx image=%llx calls=%u,%u,%u scenario=%u\n",
                     static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(function)),
                     static_cast<unsigned long long>(image + mta::game_de::NativePedOnScreenProfile::CenterRva), static_cast<unsigned long long>(receiver),
                     static_cast<unsigned long long>(ped), static_cast<unsigned long long>(image), calls[0], calls[1], calls[2], scenario);
        if (reinterpret_cast<std::uintptr_t>(function) != image + mta::game_de::NativePedOnScreenProfile::CenterRva || receiver != ped)
            std::abort();
        std::memcpy(&center, centerValue.data(), sizeof(center));
        returned = &center;
        if (scenario == 2)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return mta::game_de::ClockResult::Ok;
    }
    mta::game_de::ClockResult Invoke(Calls::ScreenFunction function, std::uintptr_t camera, const CVector& center, float radius, bool& value,
                                     std::uint32_t& exception) noexcept
    {
        ++calls[2];
        std::fprintf(
            stderr,
            "OnScreen Screen entry: fn=%llx expected=%llx camera=%llx expectedCamera=%llx image=%llx radius=%g center=%g,%g,%g calls=%u,%u,%u scenario=%u\n",
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(function)),
            static_cast<unsigned long long>(image + mta::game_de::NativePedOnScreenProfile::ScreenRva), static_cast<unsigned long long>(camera),
            static_cast<unsigned long long>(image + mta::game_de::NativePedOnScreenProfile::CameraRva), static_cast<unsigned long long>(image),
            static_cast<double>(radius), static_cast<double>(center.fX), static_cast<double>(center.fY), static_cast<double>(center.fZ), calls[0], calls[1],
            calls[2], scenario);
        if (reinterpret_cast<std::uintptr_t>(function) != image + mta::game_de::NativePedOnScreenProfile::ScreenRva ||
            camera != image + mta::game_de::NativePedOnScreenProfile::CameraRva || radius != 8.5f || std::memcmp(&center, centerValue.data(), sizeof(center)))
            std::abort();
        value = nativeValue;
        if (scenario == 3)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (scenario == 4)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockPedVisibilityOwnerTest
{
    std::uintptr_t                         watched = 0, cell = 0;
    std::uint32_t                          replacement = 0;
    unsigned                               reads = 0, stores = 0, changeAfter = 0;
    bool                                   failNextRead = false, loseAfterRead = false, neighborRace = false, failAfter = false, loseAfterWrite = false;
    mta::game_de::NativePedReferenceToken* callerReadToken = nullptr;
    mta::game_de::NativePedReferenceToken* callerWriteToken = nullptr;
    BOOL WINAPI                            Read(HANDLE process, LPCVOID source, LPVOID destination, SIZE_T bytes, SIZE_T* copied)
    {
        const bool selected = reinterpret_cast<std::uintptr_t>(source) == watched && bytes == sizeof(std::uint32_t);
        if (selected && failNextRead)
        {
            failNextRead = false;
            if (copied)
                *copied = 0;
            return FALSE;
        }
        if (!::ReadProcessMemory(process, source, destination, bytes, copied))
            return FALSE;
        if (selected)
        {
            ++reads;
            if (callerReadToken)
            {
                ++callerReadToken->version;
                callerReadToken = nullptr;
            }
            if (changeAfter && reads == changeAfter)
                std::memcpy(reinterpret_cast<void*>(watched), &replacement, sizeof(replacement));
            if (loseAfterRead && reads == 2)
            {
                loseAfterRead = false;
                const std::uintptr_t zero = 0;
                std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
            }
        }
        return TRUE;
    }
    bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++stores;
        if (neighborRace)
        {
            neighborRace = false;
            const auto competing = expected ^ 2u;
            std::memcpy(reinterpret_cast<void*>(address), &competing, sizeof(competing));
        }
        LONG before = 0, after = 0;
        std::memcpy(&before, &expected, sizeof(before));
        std::memcpy(&after, &target, sizeof(after));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), after, before);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (callerWriteToken)
        {
            ++callerWriteToken->version;
            callerWriteToken = nullptr;
        }
        if (loseAfterWrite && observed == expected)
        {
            loseAfterWrite = false;
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        if (failAfter && observed == expected)
        {
            failAfter = false;
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
}

namespace ClockPedDuckingSetterOwnerTest
{
    unsigned                               stores = 0;
    std::uintptr_t                         cell = 0;
    bool                                   neighborRace = false, failAfter = false, loseToken = false;
    mta::game_de::NativePedReferenceToken* callerToken = nullptr;
    bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++stores;
        if (neighborRace)
        {
            neighborRace = false;
            const auto competing = expected ^ 2u;
            std::memcpy(reinterpret_cast<void*>(address), &competing, sizeof(competing));
        }
        LONG before = 0, after = 0;
        std::memcpy(&before, &expected, sizeof(before));
        std::memcpy(&after, &target, sizeof(after));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), after, before);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (callerToken)
        {
            ++callerToken->version;
            callerToken = nullptr;
        }
        if (observed == expected && loseToken)
        {
            loseToken = false;
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
}
namespace ClockPedCollisionOwnerTest
{
    unsigned       stores = 0;
    std::uintptr_t cell = 0;
    bool           neighborRace = false, loseToken = false, failAfter = false;
    bool           Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++stores;
        if (neighborRace)
        {
            neighborRace = false;
            std::uint32_t competing = expected ^ 2u;
            std::memcpy(reinterpret_cast<void*>(address), &competing, sizeof(competing));
        }
        LONG compare = 0, desired = 0;
        std::memcpy(&compare, &expected, sizeof(compare));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), desired, compare);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (observed == expected && loseToken)
        {
            loseToken = false;
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
}
namespace ClockPedAreaOwnerTest
{
    unsigned       stores = 0;
    std::uintptr_t cell = 0;
    bool           loseToken = false, failAfter = false;
    bool           Exchange(std::uintptr_t at, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, std::uint32_t& exception) noexcept
    {
        ++stores;
        const auto previous = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(at), static_cast<char>(target), static_cast<char>(expected));
        observed = static_cast<std::uint8_t>(previous);
        if (observed == expected && loseToken)
        {
            loseToken = false;
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
}
namespace ClockSpecialModelOwnerTest
{
    unsigned                                                       calls = 0, mode = 0;
    std::uintptr_t                                                 image = 0, metadata = 0;
    std::uint32_t                                                  model = 0, channel = 0;
    mta::game_de::NativeStreamingSpecialModelCalls::NativeFunction entry = nullptr;
    std::array<char, 9>                                            copied{};
    void (*during)() = nullptr;
    mta::game_de::ClockResult Invoke(mta::game_de::NativeStreamingSpecialModelCalls::NativeFunction function, std::uint32_t selected, const char* name,
                                     std::uint32_t flags, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = function;
        model = selected;
        channel = flags;
        copied.fill(0);
        std::memcpy(copied.data(), name, std::strlen(name) + 1);
        std::memcpy(reinterpret_cast<void*>(metadata + 0xc), name, std::strlen(name) + 1);
        auto* row = reinterpret_cast<unsigned char*>(image + mta::game_de::NativeModelLoadedProfile::ArrayRva +
                                                     selected * mta::game_de::NativeModelLoadedProfile::RowStride);
        row[mta::game_de::NativeModelLoadedProfile::FlagsOffset] = static_cast<unsigned char>(flags);
        row[mta::game_de::NativeModelLoadedProfile::StateOffset] = 2;
        if (during)
            during();
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        if (mode == 2)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(image + mta::game_de::NativeStreamingRequestProfile::RootRva), &zero, sizeof(zero));
        }
        return mta::game_de::ClockResult::Ok;
    }
}
namespace ClockPedWaterOwnerTest
{
    bool failAfter = false;
    bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        const bool acknowledged = ClockPedPhysicalScalarsOwnerTest::Exchange(address, expected, target, observed, exception);
        if (acknowledged && observed == expected && failAfter)
        {
            // Only final CAS acknowledgement is lost. The actual Runtime must
            // stop with persistent native water bits and no accepted replay.
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return acknowledged;
    }
}
namespace ClockPedRotationOwnerTest
{
    unsigned       stores = 0;
    std::uintptr_t cell = 0;
    bool           loseToken = false;
    bool           Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t&) noexcept
    {
        ++stores;
        LONG compare = 0, desired = 0;
        std::memcpy(&compare, &expected, sizeof(compare));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), desired, compare);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (loseToken && observed == expected)
        {
            loseToken = false;
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return true;
    }
}
namespace ClockPedOxygenOwnerTest
{
    unsigned       stores = 0;
    std::uintptr_t cell = 0;
    bool           loseToken = false;
    bool           Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t&) noexcept
    {
        ++stores;
        LONG compare = 0, desired = 0;
        std::memcpy(&compare, &expected, sizeof(compare));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), desired, compare);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (loseToken && observed == expected)
        {
            loseToken = false;
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        return true;
    }
}
namespace ClockPedFlagsOwnerTest
{
    struct Host
    {
        unsigned       stores = 0;
        std::uintptr_t cell = 0;
        bool           neighborOnce = false, loseToken = false;
    };
    Host bike, shot;
    static_assert(((mta::game_de::NativePedBikeFlagsProfile::KnockoffMask | mta::game_de::NativePedShotFlagsProfile::CanBeShotMask) & 1) == 0);
    bool Exchange(Host& host, std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed) noexcept
    {
        ++host.stores;
        if (host.neighborOnce)
        {
            host.neighborOnce = false;
            // This genuine unaccepted conflict changes only an unowned bit.
            // The production masked writer must retain it on any bounded retry.
            std::uint32_t current = 0;
            std::memcpy(&current, reinterpret_cast<void*>(address), sizeof(current));
            current ^= 1;
            std::memcpy(reinterpret_cast<void*>(address), &current, sizeof(current));
        }
        LONG compare = 0, desired = 0;
        std::memcpy(&compare, &expected, sizeof(compare));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), desired, compare);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (host.loseToken && observed == expected)
        {
            host.loseToken = false;
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(host.cell), &zero, sizeof(zero));
        }
        return true;
    }
    bool ExchangeBike(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t&) noexcept
    {
        return Exchange(bike, at, expected, target, observed);
    }
    bool ExchangeShot(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t&) noexcept
    {
        return Exchange(shot, at, expected, target, observed);
    }
}
namespace ClockPedBehaviorOwnerTest
{
    // Reuse the existing final masked-CAS test boundary. Separate host state
    // ensures the older bike/shot counters and native effects stay unrelated.
    using Host = ClockPedFlagsOwnerTest::Host;
    Host bleeding, stay;
    static_assert(((mta::game_de::NativePedBleedingProfile::BleedingMask | mta::game_de::NativePedStayProfile::StayMask) & 1) == 0);
    bool ExchangeBleeding(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t&) noexcept
    {
        return ClockPedFlagsOwnerTest::Exchange(bleeding, at, expected, target, observed);
    }
    bool ExchangeStay(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t&) noexcept
    {
        return ClockPedFlagsOwnerTest::Exchange(stay, at, expected, target, observed);
    }
}
namespace ClockPedCombatOwnerTest
{
    ClockPedFlagsOwnerTest::Host host;
    bool                         failAfter = false;
    static_assert((mta::game_de::NativePedFightingStyleProfile::StyleMask & 1) == 0);
    bool Exchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        const auto result = ClockPedFlagsOwnerTest::Exchange(host, at, expected, target, observed);
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return result;
    }
}
namespace ClockPedWeaponOwnerTest
{
    ClockPedFlagsOwnerTest::Host fields, ammo;
    bool                         failFieldsAfter = false, failAmmoAfter = false;
    std::uintptr_t               infoTypeAddress = 0;
    void                         ChangeInfoType()
    {
        const std::uint32_t changed = 23;
        std::memcpy(reinterpret_cast<void*>(infoTypeAddress), &changed, sizeof(changed));
    }

    bool Exchange(ClockPedFlagsOwnerTest::Host& host, bool failAfter, std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed,
                  std::uint32_t& exception) noexcept
    {
        const auto result = ClockPedFlagsOwnerTest::Exchange(host, at, expected, target, observed);
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return result;
    }
    bool ExchangeFields(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        return Exchange(fields, failFieldsAfter, at, expected, target, observed, exception);
    }
    bool ExchangeAmmo(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        return Exchange(ammo, failAmmoAfter, at, expected, target, observed, exception);
    }
}
namespace ClockNamedGarageOwnerTest
{
    std::uintptr_t nameAddress = 0, countAddress = 0, geometryAddress = 0;
    unsigned       geometryReadAction = 0;
    unsigned       stores = 0, action = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID address, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = ::ReadProcessMemory(process, address, output, bytes, copied);
        if (result && geometryReadAction == 1 && reinterpret_cast<std::uintptr_t>(address) == geometryAddress && bytes == 56)
        {
            geometryReadAction = 0;
            *reinterpret_cast<volatile std::uint32_t*>(geometryAddress + 7 * sizeof(std::uint32_t)) = 0x12345678;
        }
        return result;
    }
    bool Exchange(std::uintptr_t address, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, std::uint32_t& exception) noexcept
    {
        ++stores;
        if (action == 4)
            *reinterpret_cast<volatile std::uint8_t*>(address) = static_cast<std::uint8_t>(expected ^ 0x80u);
        char before = 0, after = 0;
        std::memcpy(&before, &expected, 1);
        std::memcpy(&after, &target, 1);
        const auto previous = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address), after, before);
        std::memcpy(&observed, &previous, 1);
        if (action == 1)
        {
            const unsigned char renamed[8] = {'n', 'e', 'w', 0, 0, 0, 0, 0};
            std::memcpy(reinterpret_cast<void*>(nameAddress), renamed, sizeof(renamed));
        }
        if (action == 2)
            *reinterpret_cast<volatile std::uint32_t*>(countAddress) = 2;
        if (action == 3)
        {
            exception = 0xc0000005;
            return false;
        }
        return true;
    }
}

namespace ClockPedCurrentWeaponOwnerTest
{
    std::uintptr_t address = 0, cell = 0;
    unsigned       action = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID source, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        const auto result = ::ReadProcessMemory(process, source, output, bytes, copied);
        if (result && reinterpret_cast<std::uintptr_t>(source) == address && bytes == 1 && action)
        {
            const auto selected = action;
            action = 0;
            if (selected == 1)
            {
                const std::uint8_t changed = 12;
                std::memcpy(reinterpret_cast<void*>(address), &changed, sizeof(changed));
            }
            if (selected == 2)
            {
                const std::uintptr_t zero = 0;
                std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
            }
        }
        return result;
    }
}
namespace ClockPedVelocityOwnerTest
{
    unsigned stores = 0, failAt = 0, afterAt = 0;
    void (*after)() = nullptr;
    LONG WINAPI Exchange(volatile LONG* address, LONG target, LONG expected)
    {
        ++stores;
        if (stores == failAt)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        // Exercise the actual aligned scalar CAS. Only its fault boundary is
        // scheduled; no fake callback grant or alternative setter is supplied.
        const auto observed = _InterlockedCompareExchange(address, target, expected);
        if (stores == afterAt && after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        return observed;
    }
}

namespace ClockPedOrientationOwnerTest
{
    using Memory = mta::game_de::NativePedOrientationMemory;
    unsigned                      calls = 0, failAt = 0;
    std::uintptr_t                cell = 0;
    bool                          loseOnAdd = false;
    std::array<Memory::Phase, 3>  phases{};
    std::array<std::uintptr_t, 3> entries{}, receivers{};
    std::array<CVector, 3>        arguments{};
    std::function<void()>         afterRemove;
    void                          Reset()
    {
        calls = failAt = 0;
        cell = 0;
        loseOnAdd = false;
        phases = {};
        entries = {};
        receivers = {};
        arguments = {};
        afterRemove = {};
    }
    mta::game_de::ClockResult Invoke(Memory::Phase phase, const Memory::Entries& functions, void* ped, const CVector& angles, std::uint32_t& exception) noexcept
    {
        const auto index = calls++;
        if (index >= phases.size())
            std::abort();
        phases[index] = phase;
        receivers[index] = reinterpret_cast<std::uintptr_t>(ped);
        arguments[index] = angles;
        entries[index] = phase == Memory::Phase::Remove   ? reinterpret_cast<std::uintptr_t>(functions.remove)
                         : phase == Memory::Phase::Rotate ? reinterpret_cast<std::uintptr_t>(functions.rotate)
                                                          : reinterpret_cast<std::uintptr_t>(functions.add);
        if (phase == Memory::Phase::Remove && afterRemove)
        {
            const auto action = std::move(afterRemove);
            afterRemove = {};
            action();
        }
        if (phase == Memory::Phase::Rotate)
        {
            std::uintptr_t matrix = 0;
            std::memcpy(&matrix, static_cast<unsigned char*>(ped) + 0x18, sizeof(matrix));
            // This is an accepted-prefix marker at the final call seam, not
            // an emulation of native trig or a claim of rendered GTA output.
            if (matrix)
                std::memcpy(reinterpret_cast<void*>(matrix), &angles, sizeof(angles));
            else
                std::memcpy(static_cast<unsigned char*>(ped) + 0x14, &angles.fZ, sizeof(float));
        }
        if (phase == Memory::Phase::Add && loseOnAdd)
        {
            const std::uintptr_t zero = 0;
            std::memcpy(reinterpret_cast<void*>(cell), &zero, sizeof(zero));
        }
        if (failAt && calls == failAt)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockPedTeleportOwnerTest
{
    using Memory = mta::game_de::NativePedTeleportMemory;
    unsigned                  calls = 0;
    std::uintptr_t            image = 0, entry = 0, receiver = 0;
    CVector                   requested;
    bool                      argument = false, effect = true;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    void (*after)() = nullptr;
    mta::game_de::ClockResult Invoke(Memory::NativeFunction function, void* ped, const CVector* position, bool reset, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        receiver = reinterpret_cast<std::uintptr_t>(ped);
        requested = *position;
        argument = reset;
        if (effect)
        {
            std::uintptr_t matrix = 0;
            SIZE_T         copied = 0;
            if (!::ReadProcessMemory(GetCurrentProcess(), static_cast<unsigned char*>(ped) + mta::game_de::NativePedTransformProfile::MatrixPointerOffset,
                                     &matrix, sizeof(matrix), &copied) ||
                copied != sizeof(matrix))
                std::abort();
            const auto address = matrix ? matrix + mta::game_de::NativePedTransformProfile::MatrixPositionOffset
                                        : receiver + mta::game_de::NativePedTransformProfile::InlinePositionOffset;
            // This single visible effect distinguishes accepted native intent
            // from cleanup. It does not emulate task/list callbacks or claim
            // that their actual in-game reactions were executed by this test.
            const CVector zero(0, 0, 0);
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), position, sizeof(*position), &copied) ||
                copied != sizeof(*position) ||
                !::WriteProcessMemory(GetCurrentProcess(), static_cast<unsigned char*>(ped) + mta::game_de::NativePedTransformProfile::VelocityOffset, &zero,
                                      sizeof(zero), &copied) ||
                copied != sizeof(zero))
                std::abort();
        }
        if (after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

namespace ClockRopesOwnerTest
{
    using Memory = mta::game_de::NativeRopesMemory;
    std::uintptr_t            image = 0, entry = 0, identity = 0, row = 0;
    std::uint32_t             duration = 0;
    unsigned                  creates = 0, removes = 0;
    std::int32_t              nativeResult = 1;
    bool                      effect = true;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    void (*after)() = nullptr;
    void Write(std::uintptr_t address, const void* value, std::size_t bytes)
    {
        SIZE_T copied = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), value, bytes, &copied) || copied != bytes)
            std::abort();
    }
    mta::game_de::ClockResult Create(Memory::CreateFunction function, std::uintptr_t key, const Memory::Vector12&, std::uint32_t timeout, std::int32_t& output,
                                     std::uint32_t& exception) noexcept
    {
        ++creates;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        identity = key;
        duration = timeout;
        output = nativeResult;
        if (effect)
        {
            const auto          address = image + mta::game_de::NativeRopesProfile::TableRva;
            const unsigned char type = 8;
            Write(address + mta::game_de::NativeRopesProfile::IdentityOffset, &key, sizeof(key));
            Write(address + mta::game_de::NativeRopesProfile::TypeOffset, &type, sizeof(type));
        }
        if (after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
    mta::game_de::ClockResult Remove(Memory::RemoveFunction function, void* object, std::uint32_t& exception) noexcept
    {
        ++removes;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        row = reinterpret_cast<std::uintptr_t>(object);
        if (effect)
        {
            const unsigned char  type = 0;
            const std::uintptr_t zero = 0;
            Write(row + mta::game_de::NativeRopesProfile::TypeOffset, &type, sizeof(type));
            Write(row + mta::game_de::NativeRopesProfile::HookOffset, &zero, sizeof(zero));
            Write(row + mta::game_de::NativeRopesProfile::PickedUpOffset, &zero, sizeof(zero));
        }
        if (after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

namespace ClockWantedOwnerTest
{
    std::uintptr_t            image = 0, entry = 0, receiver = 0;
    std::uint32_t             level = 0;
    unsigned                  calls = 0;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    void (*after)() = nullptr;
    mta::game_de::ClockResult Invoke(mta::game_de::NativeWantedMemory::NativeFunction function, void* object, std::uint32_t requested,
                                     std::uint32_t& exception) noexcept
    {
        namespace W = mta::game_de::NativeWantedProfile;
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        receiver = reinterpret_cast<std::uintptr_t>(object);
        level = requested;
        const std::uint32_t nativeLevel = requested > 6 ? 6 : requested;
        std::uint32_t       timer = 0;
        SIZE_T              copied = 0;
        const auto          write = [&](std::uintptr_t address, std::uint32_t value)
        { return ::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &value, sizeof(value), &copied) && copied == sizeof(value); };
        // The exact final native target is observed, never executed. Effects
        // still precede exception/post-call failure so Runtime retirement must
        // preserve native intent and never replay the requested wanted level.
        if (!::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + W::TimerRva), &timer, sizeof(timer), &copied) ||
            copied != sizeof(timer) || !write(receiver + W::LevelOffset, nativeLevel) || !write(receiver + W::TimestampOffset, timer))
            std::abort();
        if (after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

namespace ClockStartGameOwnerTest
{
    std::uintptr_t            image = 0, entry = 0;
    void*                     receiver = nullptr;
    unsigned                  calls = 0;
    bool                      retireGraph = false;
    mta::game_de::ClockResult result = mta::game_de::ClockResult::Ok;
    void (*after)() = nullptr;
    mta::game_de::ClockResult Invoke(mta::game_de::NativeStartGameCalls::NativeFunction function, void* object, std::uint32_t& exception) noexcept
    {
        ++calls;
        entry = reinterpret_cast<std::uintptr_t>(function) - image;
        receiver = object;
        const auto write = [](std::uintptr_t rva, const auto& value)
        {
            SIZE_T copied = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), &value, sizeof(value), &copied) || copied != sizeof(value))
                std::abort();
        };
        // This substitutes only the last native call. The real Apply and
        // Runtime owner bookends still run, and accepted native effects must
        // survive even a later exception/refusal rather than being replayed.
        write(mta::game_de::NativeStartGameProfile::RequestRva, static_cast<unsigned char>(1));
        write(mta::game_de::NativeStartGameProfile::SelectionRva, std::uint32_t{0});
        write(mta::game_de::NativeGamePauseProfile::userPauseRva, static_cast<unsigned char>(0));
        write(mta::game_de::NativeGameStateProfile::stateRva, std::uint32_t{8});
        const unsigned char pump = 1;
        SIZE_T              pumpWritten = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<unsigned char*>(object) + mta::game_de::NativeStartupCompletionProfile::PumpOffset,
                                  &pump, sizeof(pump), &pumpWritten) ||
            pumpWritten != sizeof(pump))
            std::abort();
        if (retireGraph)
        {
            write(mta::game_de::NativeStartupProvidersProfile::engineRva, std::uintptr_t{0});
            write(mta::game_de::NativeUiClassMapProfile::uiTransitionClassCacheRva, std::uintptr_t{0});
        }
        if (after)
        {
            const auto next = after;
            after = nullptr;
            next();
        }
        if (result != mta::game_de::ClockResult::Ok)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
}

namespace ClockTestFaults
{
    std::atomic<unsigned> partialDataWrites{0}, failFlushes{0}, protectionCountdown{0};
    std::atomic<bool>     failEnumeration{false}, failProtectionRestores{false};
    std::atomic<unsigned> failResumes{0};
    std::atomic<unsigned> failSuspends{0}, suspendCalls{0};
    std::atomic<DWORD>    suspendTarget{0};
    std::atomic<unsigned> clockDataWrites{0}, clockCodeExchanges{0};
    std::atomic<unsigned> partialTimingWrites{0}, failTimingWrites{0}, timingDataWrites{0};
    std::uintptr_t        timingWriteAddress = 0;
    std::atomic<DWORD>    resumeTarget{0};
    std::uintptr_t        patchAddress = 0, dataAddress = 0;
    std::uintptr_t        uiReadAddress = 0, uiRootChangeAddress = 0;
    std::atomic<bool>     failUiRead{false};
    std::uintptr_t        cheatWriteAddress = 0;
    void (*cheatAfterRead)() = nullptr;
    std::atomic<bool>             advanceCheatBeforeAtomic{false}, failCheatReadback{false}, failNextCheatRead{false};
    std::uintptr_t                weatherFailAddress = 0, weatherRollbackAddress = 0;
    std::atomic<bool>             failWeatherPair{false};
    std::array<std::uintptr_t, 2> weatherProtectedPages{};
    std::array<DWORD, 2>          weatherOldProtections{};
    std::size_t                   weatherProtectedCount = 0;
    std::uintptr_t                scalarReadAddress = 0;
    std::atomic<bool>             failScalarRead{false};
    void (*scalarAfterRead)() = nullptr;
    std::uintptr_t        weaponWriteAddress = 0, weaponRowAddress = 0;
    std::atomic<bool>     advanceWeaponBeforeAtomic{false}, protectWeaponBeforeAtomic{false}, failWeaponReadback{false}, failNextWeaponRow{false};
    std::atomic<unsigned> weaponExchanges{0};
    DWORD                 weaponOldProtection = 0;
    void (*weaponAfterRowRead)() = nullptr;
    void (*weaponAfterAtomic)() = nullptr;

    BOOL WINAPI Read(HANDLE process, LPCVOID address, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == scalarReadAddress && bytes == sizeof(float) && failScalarRead.exchange(false))
        {
            if (copied)
                *copied = 0;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        if (reinterpret_cast<std::uintptr_t>(address) == cheatWriteAddress && bytes == 1 && failNextCheatRead.exchange(false))
        {
            if (copied)
                *copied = 0;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        if (reinterpret_cast<std::uintptr_t>(address) == uiReadAddress && failUiRead.exchange(false))
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, address, output, bytes, copied);
        if (reinterpret_cast<std::uintptr_t>(address) == scalarReadAddress && bytes == sizeof(float) && scalarAfterRead)
        {
            const auto after = scalarAfterRead;
            scalarAfterRead = nullptr;
            after();
        }
        if (result && reinterpret_cast<std::uintptr_t>(address) == cheatWriteAddress && bytes == 1 && cheatAfterRead)
        {
            const auto after = cheatAfterRead;
            cheatAfterRead = nullptr;
            after();
        }
        if (reinterpret_cast<std::uintptr_t>(address) == uiReadAddress && uiRootChangeAddress)
        {
            const std::uintptr_t absent = 0;
            SIZE_T               written = 0;
            ::WriteProcessMemory(process, reinterpret_cast<void*>(uiRootChangeAddress), &absent, sizeof(absent), &written);
            uiRootChangeAddress = 0;
        }
        return result;
    }

    BOOL WINAPI Write(HANDLE process, LPVOID address, LPCVOID bytes, SIZE_T size, SIZE_T* copied)
    {
        clockDataWrites.fetch_add(1);
        if (reinterpret_cast<std::uintptr_t>(address) == timingWriteAddress && (size == 4 || size == 1))
        {
            timingDataWrites.fetch_add(1);
            if (failTimingWrites.load())
            {
                failTimingWrites.fetch_sub(1);
                if (copied)
                    *copied = 0;
                SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            }
            if (partialTimingWrites.load())
            {
                partialTimingWrites.fetch_sub(1);
                if (size == 1)
                {
                    if (copied)
                        *copied = 0;
                    SetLastError(ERROR_PARTIAL_COPY);
                    return FALSE;
                }
                return ::WriteProcessMemory(process, address, bytes, 2, copied);
            }
        }
        if (reinterpret_cast<std::uintptr_t>(address) == dataAddress && size == 4 && partialDataWrites.load())
        {
            partialDataWrites.fetch_sub(1);
            SIZE_T     actual = 0;
            const auto result = ::WriteProcessMemory(process, address, bytes, 2, &actual);
            if (copied)
                *copied = actual;
            SetLastError(ERROR_PARTIAL_COPY);
            return result;
        }
        return ::WriteProcessMemory(process, address, bytes, size, copied);
    }
    LONG64 WINAPI CompareExchange(volatile LONG64* address, LONG64 desired, LONG64 expected)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == patchAddress)
            clockCodeExchanges.fetch_add(1);
        return ::InterlockedCompareExchange64(address, desired, expected);
    }
    char CheatCompareExchange(volatile char* address, char desired, char expected)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == cheatWriteAddress && advanceCheatBeforeAtomic.exchange(false))
            ::_InterlockedExchange8(address, static_cast<char>(expected ? 0 : 1));
        const auto observed = ::_InterlockedCompareExchange8(address, desired, expected);
        if (reinterpret_cast<std::uintptr_t>(address) == cheatWriteAddress && failCheatReadback.exchange(false))
            failNextCheatRead.store(true);
        return observed;
    }
    void WeatherProtectionRace(std::uintptr_t address)
    {
        if (address != weatherFailAddress || !failWeatherPair.exchange(false))
            return;
        weatherProtectedCount = 0;
        for (const auto target : {weatherFailAddress, weatherRollbackAddress})
        {
            const auto page = target & ~std::uintptr_t{0xfff};
            if (weatherProtectedCount && weatherProtectedPages[0] == page)
                continue;
            DWORD prior = 0;
            if (!::VirtualProtect(reinterpret_cast<void*>(page), 0x1000, PAGE_READONLY, &prior))
                std::abort();
            weatherProtectedPages[weatherProtectedCount] = page;
            weatherOldProtections[weatherProtectedCount++] = prior;
        }
    }
    short WeatherCompareExchange16(volatile short* address, short desired, short expected)
    {
        WeatherProtectionRace(reinterpret_cast<std::uintptr_t>(address));
        return ::_InterlockedCompareExchange16(address, desired, expected);
    }
    LONG WeatherCompareExchange32(volatile LONG* address, LONG desired, LONG expected)
    {
        WeatherProtectionRace(reinterpret_cast<std::uintptr_t>(address));
        return ::_InterlockedCompareExchange(address, desired, expected);
    }
    void WeaponProtectionRace(std::uintptr_t address)
    {
        if (address == weaponWriteAddress && protectWeaponBeforeAtomic.exchange(false) &&
            !::VirtualProtect(reinterpret_cast<void*>(address), sizeof(LONG), PAGE_READONLY, &weaponOldProtection))
            std::abort();
    }
    SHORT WeaponCompareExchange16(volatile SHORT* address, SHORT desired, SHORT expected)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == weaponWriteAddress)
        {
            ++weaponExchanges;
            if (advanceWeaponBeforeAtomic.exchange(false))
                ::_InterlockedExchange16(address, static_cast<SHORT>(expected ^ 0x40));
        }
        WeaponProtectionRace(reinterpret_cast<std::uintptr_t>(address));
        const auto observed = ::_InterlockedCompareExchange16(address, desired, expected);
        if (reinterpret_cast<std::uintptr_t>(address) == weaponWriteAddress && failWeaponReadback.exchange(false))
            failNextWeaponRow.store(true);
        if (reinterpret_cast<std::uintptr_t>(address) == weaponWriteAddress && weaponAfterAtomic)
        {
            const auto after = weaponAfterAtomic;
            weaponAfterAtomic = nullptr;
            after();
        }
        return observed;
    }
    LONG WeaponCompareExchange32(volatile LONG* address, LONG desired, LONG expected)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == weaponWriteAddress)
        {
            ++weaponExchanges;
            if (advanceWeaponBeforeAtomic.exchange(false))
                ::_InterlockedExchange(address, expected ^ 0x40);
        }
        WeaponProtectionRace(reinterpret_cast<std::uintptr_t>(address));
        const auto observed = ::_InterlockedCompareExchange(address, desired, expected);
        if (reinterpret_cast<std::uintptr_t>(address) == weaponWriteAddress && failWeaponReadback.exchange(false))
            failNextWeaponRow.store(true);
        if (reinterpret_cast<std::uintptr_t>(address) == weaponWriteAddress && weaponAfterAtomic)
        {
            const auto after = weaponAfterAtomic;
            weaponAfterAtomic = nullptr;
            after();
        }
        return observed;
    }
    BOOL WINAPI Protect(LPVOID address, SIZE_T size, DWORD protection, PDWORD previous)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == patchAddress && size == 8)
        {
            if (protection != PAGE_EXECUTE_READWRITE && failProtectionRestores.load())
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            }
            auto remaining = protectionCountdown.load();
            if (remaining && protectionCountdown.fetch_sub(1) == 1)
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            }
        }
        return ::VirtualProtect(address, size, protection, previous);
    }
    BOOL WINAPI Flush(HANDLE process, LPCVOID address, SIZE_T size)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == patchAddress && size == 8 && failFlushes.load())
        {
            failFlushes.fetch_sub(1);
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return ::FlushInstructionCache(process, address, size);
    }
    BOOL WINAPI Next(HANDLE snapshot, LPTHREADENTRY32 entry)
    {
        if (failEnumeration.exchange(false))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return ::Thread32Next(snapshot, entry);
    }
    DWORD WINAPI Suspend(HANDLE thread)
    {
        suspendCalls.fetch_add(1);
        // Fail only the selected disposable peer before its real suspend.
        // All successful calls and the existing recovery path stay native.
        if (GetThreadId(thread) == suspendTarget.load() && failSuspends.load())
        {
            failSuspends.fetch_sub(1);
            SetLastError(ERROR_ACCESS_DENIED);
            return static_cast<DWORD>(-1);
        }
        return ::SuspendThread(thread);
    }
    DWORD WINAPI Resume(HANDLE thread)
    {
        if (GetThreadId(thread) == resumeTarget.load() && failResumes.load())
        {
            failResumes.fetch_sub(1);
            SetLastError(ERROR_ACCESS_DENIED);
            return static_cast<DWORD>(-1);
        }
        return ::ResumeThread(thread);
    }
}

#if defined(MTA_TEST_PRESENT_TRANSITION_CLOCK)
namespace PresentTransitionWeatherFault
{
    BOOL WINAPI Flush(HANDLE, LPCVOID, SIZE_T);
}
    // Compile the unchanged platform before the clock's global runtime
    // declaration, so its existing local names remain warning-clean.
    // Only the same final OS cache-flush hook is active for this source.
    #define FlushInstructionCache PresentTransitionWeatherFault::Flush
    #include "../../Client/game_de/NativeWeatherOverrideWin32.cpp"
    #undef FlushInstructionCache
#endif

// Fault injection exists only in this ordinary test translation unit. The
// shipping executable gate, actual native access and clock implementation are
// unchanged; each wrapper otherwise calls the real Win32 operation.
#define WriteProcessMemory    ClockTestFaults::Write
#define ReadProcessMemory     ClockTestFaults::Read
#define VirtualProtect        ClockTestFaults::Protect
#define FlushInstructionCache ClockTestFaults::Flush
#define Thread32Next          ClockTestFaults::Next
#define ResumeThread          ClockTestFaults::Resume
#define SuspendThread         ClockTestFaults::Suspend
// Restore the Windows alias after the clock-code fault seam. Other real
// native memory helpers in this same translation unit still need that API.
#pragma push_macro("InterlockedCompareExchange64")
#ifdef InterlockedCompareExchange64
    #undef InterlockedCompareExchange64
#endif
#define InterlockedCompareExchange64 ClockTestFaults::CompareExchange
#pragma push_macro("_InterlockedCompareExchange8")
#define _InterlockedCompareExchange8 ClockTestFaults::CheatCompareExchange
#pragma push_macro("_InterlockedCompareExchange16")
#pragma push_macro("_InterlockedCompareExchange")
#define _InterlockedCompareExchange16 ClockTestFaults::WeatherCompareExchange16
#define _InterlockedCompareExchange   ClockTestFaults::WeatherCompareExchange32
#include "../../Client/game_de/NativeClockService.cpp"
#pragma pop_macro("_InterlockedCompareExchange")
#pragma pop_macro("_InterlockedCompareExchange16")
#pragma pop_macro("_InterlockedCompareExchange8")
#pragma pop_macro("InterlockedCompareExchange64")
#undef SuspendThread
#undef ResumeThread
#undef Thread32Next
#undef FlushInstructionCache
#undef VirtualProtect
#undef WriteProcessMemory
#undef ReadProcessMemory

namespace ClockReaderTestHost
{
    std::uintptr_t        image = 0;
    std::atomic<unsigned> samples{0}, mismatchSample{0};
    std::atomic<unsigned> timingSamples{0}, mismatchTimingSample{0};
    std::atomic<unsigned> fpsSamples{0}, mismatchFPSSample{0}, failFPSSample{0};
    std::atomic<unsigned> systemStateSamples{0}, transitionSystemStateSample{0};

    HMODULE WINAPI Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }
}

namespace ClockAnimationOwnerTest
{
    std::uintptr_t row = 0;
    unsigned       reads = 0;
    bool           failNext = false;
    void (*afterRead)() = nullptr;
    mta::game_de::NativeAnimationBlockIdentity* identity = nullptr;
    std::uintptr_t                              changedAddress = 0;
    std::uint32_t                               changedValue = 0;
    void                                        StoreWord() noexcept
    {
        SIZE_T written = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(changedAddress), &changedValue, sizeof(changedValue), &written) ||
            written != sizeof(changedValue))
            std::abort();
    }
    void ChangeAffinity() noexcept
    {
        ++identity->ownerGeneration;
    }
    void ChangeIndex() noexcept
    {
        ++identity->index;
    }
    void ChangeName() noexcept
    {
        identity->name[0] ^= 1;
    }
}

namespace ClockAnimationBlockRefOwnerTest
{
    std::uintptr_t                              address = 0;
    unsigned                                    wordReads = 0, failWordAt = 0, exchanges = 0, scenario = 0;
    mta::game_de::NativeAnimationBlockIdentity* callerIdentity = nullptr;
    bool                                        changeCaller = false;
    void (*afterAccepted)() noexcept = nullptr;
    void Reset() noexcept
    {
        wordReads = failWordAt = exchanges = scenario = 0;
        callerIdentity = nullptr;
        changeCaller = false;
        afterAccepted = nullptr;
    }
    mta::game_de::ClockResult Exchange(std::uintptr_t at, std::uint16_t expected, std::uint16_t target, std::uint16_t& observed,
                                       std::uint32_t& exception) noexcept
    {
        ++exchanges;
        SHORT compare = 0, desired = 0;
        std::memcpy(&compare, &expected, sizeof(compare));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto previous = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(at), desired, compare);
        std::memcpy(&observed, &previous, sizeof(observed));
        if (observed == expected)
        {
            // Only damage the actual published fixture buffers after the real
            // one-shot CAS. All current block and callback bookends remain.
            if (changeCaller && callerIdentity)
            {
                changeCaller = false;
                ++callerIdentity->index;
            }
            if (scenario == 1)
            {
                exception = EXCEPTION_ACCESS_VIOLATION;
                return mta::game_de::ClockResult::OriginalException;
            }
            if (scenario == 2)
                *reinterpret_cast<unsigned char*>(ClockAnimationOwnerTest::row) ^= 1;
            if (scenario == 3)
                ClockAnimationOwnerTest::failNext = true;
            if (const auto after = afterAccepted)
            {
                afterAccepted = nullptr;
                after();
            }
        }
        return mta::game_de::ClockResult::Ok;
    }
}

namespace ClockFreshCompressedSequenceOwnerTest
{
    namespace Bind = mta::game_de::NativeAnimationSequenceFrameBindCalls;
    unsigned                                             writes = 0, mode = 0;
    mta::game_de::NativeAnimationSequenceReferenceToken* caller = nullptr;
    mta::game_de::ClockResult                            Write(std::uintptr_t, const Bind::Value&, std::uint32_t&) noexcept;
}

namespace ClockSequenceNameOwnerTest
{
    namespace Calls = mta::game_de::NativeAnimationSequenceNameCalls;
    unsigned                                             writes = 0, mode = 0;
    mta::game_de::NativeAnimationSequenceReferenceToken* caller = nullptr;
    mta::game_de::NativeAnimationSequenceName*           callerName = nullptr;
    mta::game_de::ClockResult                            Write(std::uintptr_t, const Calls::Name&, std::size_t, bool, std::uint16_t, std::uint32_t&) noexcept;
}

namespace ClockSequenceCopyFrameOwnerTest
{
    namespace Copy = mta::game_de::NativeAnimationSequenceCopyCalls;
    unsigned                                             copies = 0, mode = 0;
    mta::game_de::NativeAnimationSequenceReferenceToken* caller = nullptr;
    mta::game_de::ClockResult                            Write(std::uintptr_t, const Copy::Record&, std::uint32_t&) noexcept;
}

namespace ClockSequenceRowOwnerTest
{
    namespace Rows = mta::game_de::NativeAnimationSequenceRowCalls;
    unsigned                                             writes = 0, mode = 0;
    std::uintptr_t                                       row = 0;
    mta::game_de::NativeAnimationSequenceReferenceToken* caller = nullptr;
    void (*afterFrame)() noexcept = nullptr;
    mta::game_de::ClockResult Write(std::uintptr_t, Rows::Operation, const Rows::Value&, std::uint32_t&) noexcept;
}

namespace ClockAnimationHierarchyBankOwnerTest
{
    std::uintptr_t locator = 0, cookie = 0;
    unsigned       scalarReads = 0, failScalar = 0, childReads = 0, failChild = 0;
    void (*afterScalar)() noexcept = nullptr;
    void (*afterChild)() noexcept = nullptr;
    bool Copy(std::uintptr_t, void*, std::size_t) noexcept;
    void Reset() noexcept
    {
        scalarReads = failScalar = childReads = failChild = 0;
        afterScalar = afterChild = nullptr;
    }
}

namespace ClockAnimationHierarchyHashOwnerTest
{
    std::uintptr_t address = 0;
    unsigned       reads = 0, failAt = 0;
    void (*afterRead)() noexcept = nullptr;
    void Reset() noexcept
    {
        reads = failAt = 0;
        afterRead = nullptr;
    }
}

namespace ClockAnimationManagerOwnerTest
{
    std::uintptr_t watched = 0, changedAddress = 0;
    // Name-cell bookends read the full field; existing scalar watchers retain
    // their four-byte selection unless the focused fixture changes the width.
    std::size_t   watchedBytes = sizeof(std::int32_t);
    std::uint32_t changedValue = 0;
    unsigned      reads = 0;
    bool          failNext = false;
    void (*afterRead)() = nullptr;
    mta::game_de::NativeAnimationBlockName* query = nullptr;
    void                                    ChangeQuery() noexcept
    {
        (*query)[0] ^= 1;
    }
    void StoreWord() noexcept
    {
        SIZE_T written = 0;
        if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(changedAddress), &changedValue, sizeof(changedValue), &written) ||
            written != sizeof(changedValue))
            std::abort();
    }
}

namespace mta::game_de::NativeImageAccess
{
    bool ClockReaderFixtureExecutable() noexcept
    {
        // A positive reader fixture selects only its owned SEC_IMAGE mapping;
        // the ordinary wrong-host test below still executes the shipping hash
        // gate. This does not admit this process as a supported GTA build.
        return ClockReaderTestHost::image != 0 || CheckExecutableBuild();
    }

    bool TimingReaderFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                      bool executable = false) noexcept
    {
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (address == image + NativeGameFPSProfile::fpsRva && bytes == sizeof(float))
        {
            const auto sample = ClockReaderTestHost::fpsSamples.fetch_add(1) + 1;
            if (sample == ClockReaderTestHost::failFPSSample.load())
                return false;
            if (sample == ClockReaderTestHost::mismatchFPSSample.load())
            {
                float value = 0;
                std::memcpy(&value, destination, sizeof(value));
                value += 1;
                std::memcpy(destination, &value, sizeof(value));
            }
        }
        if (address == image + NativeGameTimingProfile::timeScaleRva && bytes == sizeof(float) &&
            ClockReaderTestHost::timingSamples.fetch_add(1) + 1 == ClockReaderTestHost::mismatchTimingSample.load())
        {
            float value = 0;
            std::memcpy(&value, destination, sizeof(value));
            value += 1;
            std::memcpy(destination, &value, sizeof(value));
        }
        return true;
    }

    bool SystemStateFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                     bool executable = false) noexcept
    {
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (address == image + NativeGameStateProfile::stateRva && bytes == sizeof(std::uint32_t) &&
            ClockReaderTestHost::systemStateSamples.fetch_add(1) + 1 == ClockReaderTestHost::transitionSystemStateSample.load())
        {
            // Advance the real owned DWORD after the last successful sample.
            // The actual production CAS must preserve it and report Busy.
            const std::uint32_t advanced = 9;
            SIZE_T              written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &advanced, sizeof(advanced), &written) ||
                written != sizeof(advanced))
                std::abort();
            ClockReaderTestHost::transitionSystemStateSample.store(0);
        }
        return true;
    }
    bool ModelBankOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                        bool executable = false) noexcept
    {
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (ClockModelBankOwnerTest::advanceOnRead && address == ClockModelBankOwnerTest::slot && bytes == sizeof(std::uintptr_t))
        {
            ClockModelBankOwnerTest::advanceOnRead = false;
            SIZE_T written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &ClockModelBankOwnerTest::advanced,
                                      sizeof(ClockModelBankOwnerTest::advanced), &written) ||
                written != sizeof(std::uintptr_t))
                std::abort();
            ++ClockModelBankOwnerTest::changes;
        }
        return true;
    }

    bool ModelLoadedOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                          bool executable = false) noexcept
    {
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (ClockModelLoadedOwnerTest::advanceOnRead && address == ClockModelLoadedOwnerTest::address && bytes == 2)
        {
            ClockModelLoadedOwnerTest::advanceOnRead = false;
            SIZE_T written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), ClockModelLoadedOwnerTest::advanced.data(),
                                      ClockModelLoadedOwnerTest::advanced.size(), &written) ||
                written != 2)
                std::abort();
            ++ClockModelLoadedOwnerTest::changes;
        }
        return true;
    }

    bool AnimationBlockOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                             bool executable = false) noexcept
    {
        const bool selected = address == ClockAnimationOwnerTest::row && bytes == NativeAnimationBlockProfile::RowBytes;
        if (selected && ClockAnimationOwnerTest::failNext)
        {
            ClockAnimationOwnerTest::failNext = false;
            return false;
        }
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (selected)
        {
            ++ClockAnimationOwnerTest::reads;
            if (const auto after = ClockAnimationOwnerTest::afterRead)
            {
                ClockAnimationOwnerTest::afterRead = nullptr;
                after();
            }
        }
        return true;
    }

    bool AnimationBlockRefOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                                bool executable = false) noexcept
    {
        const bool selected = address == ClockAnimationBlockRefOwnerTest::address && bytes == sizeof(std::uint16_t);
        if (selected && ++ClockAnimationBlockRefOwnerTest::wordReads == ClockAnimationBlockRefOwnerTest::failWordAt)
            return false;
        return ReadImage(image, address, destination, bytes, writable, executable);
    }

    bool AnimationHierarchyHashOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                                     bool executable = false) noexcept
    {
        namespace Host = ClockAnimationHierarchyHashOwnerTest;
        const bool selected = address == Host::address && bytes == sizeof(std::uint32_t);
        if (selected && ++Host::reads == Host::failAt)
            return false;
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (selected && Host::afterRead)
        {
            const auto action = Host::afterRead;
            Host::afterRead = nullptr;
            action();
        }
        return true;
    }

    bool AnimationManagerOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                               bool executable = false) noexcept
    {
        const bool selected = address == ClockAnimationManagerOwnerTest::watched && bytes == ClockAnimationManagerOwnerTest::watchedBytes;
        if (selected && ClockAnimationManagerOwnerTest::failNext)
        {
            ClockAnimationManagerOwnerTest::failNext = false;
            return false;
        }
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (selected)
        {
            ++ClockAnimationManagerOwnerTest::reads;
            if (const auto after = ClockAnimationManagerOwnerTest::afterRead)
            {
                ClockAnimationManagerOwnerTest::afterRead = nullptr;
                after();
            }
        }
        return true;
    }

    bool AnimationHierarchyBankOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* output, std::size_t bytes, bool writable = false,
                                                     bool executable = false) noexcept
    {
        namespace Host = ClockAnimationHierarchyBankOwnerTest;
        const bool selected = address == Host::locator && bytes == 28;
        if (selected && ++Host::scalarReads == Host::failScalar)
            return false;
        if (!ReadImage(image, address, output, bytes, writable, executable))
            return false;
        if (selected && Host::afterScalar)
        {
            const auto after = Host::afterScalar;
            Host::afterScalar = nullptr;
            after();
        }
        return true;
    }

    bool WeaponOwnerFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                     bool executable = false) noexcept
    {
        if (address == ClockTestFaults::weaponRowAddress && bytes == NativeWeaponInfoProfile::RowBytes && ClockTestFaults::failNextWeaponRow.exchange(false))
            return false;
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (address == ClockTestFaults::weaponRowAddress && bytes == NativeWeaponInfoProfile::RowBytes && ClockTestFaults::weaponAfterRowRead)
        {
            const auto after = ClockTestFaults::weaponAfterRowRead;
            ClockTestFaults::weaponAfterRowRead = nullptr;
            after();
        }
        return true;
    }
}

// Only the OS read boundary can schedule an owned-image transition here. Both
// native ownership checks and the four-byte atomic replacement stay unchanged.
#define ReadImage SystemStateFixtureReadImage
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#undef ReadImage

// Qualify the complete production cheat profile against the owned image while
// retaining its real PE sections, pages, instruction/alias and table checks.
// Public clock Begin still uses its unchanged shipping executable identity.
#define CheckExecutableBuild ClockReaderFixtureExecutable
#define GetModuleHandleW     ClockReaderTestHost::Module
#include "../../Client/game_de/NativeCheatProfile.cpp"
#include "../../Client/game_de/NativeWeatherTransitionProfile.cpp"
#include "../../Client/game_de/NativeWeatherScalarProfile.cpp"
#undef GetModuleHandleW
#undef CheckExecutableBuild

// Only the module identity selection and guarded OS boundary select the owned
// SEC_IMAGE. Bank qualification, typed field CAS/readback and the actual native
// callback authority remain production code, including each retry's scope check.
#define CheckExecutableBuild ClockReaderFixtureExecutable
#define GetModuleHandleW     ClockReaderTestHost::Module
#define ReadImage            WeaponOwnerFixtureReadImage
#define Profile              WeaponReaderProfile
#include "../../Client/game_de/NativeWeaponInfoReader.cpp"
#undef Profile
#pragma push_macro("InterlockedCompareExchange16")
#pragma push_macro("InterlockedCompareExchange")
#undef InterlockedCompareExchange16
#undef InterlockedCompareExchange
#define InterlockedCompareExchange16 ClockTestFaults::WeaponCompareExchange16
#define InterlockedCompareExchange   ClockTestFaults::WeaponCompareExchange32
#include "../../Client/game_de/NativeWeaponInfoWriteMemory.cpp"
#include "../../Client/game_de/NativeWeaponInfoVectorMemory.cpp"
#pragma pop_macro("InterlockedCompareExchange")
#pragma pop_macro("InterlockedCompareExchange16")
#undef ReadImage
#undef GetModuleHandleW
#undef CheckExecutableBuild

// The actual stat reader and call primitive select only this owned SEC_IMAGE.
// All callback/generation, code, loaded-player and post-call bookends remain
// production code. The final typed call observes ABI routing without executing
// any GTA instruction or substituting CheckTimingOwner.
#define CheckExecutableBuild ClockReaderFixtureExecutable
#define GetModuleHandleW     ClockReaderTestHost::Module
#define Profile              ClockStatsReaderProfile
#define Binding              ClockStatsReaderBinding
#include "../../Client/game_de/NativeStatsReader.cpp"
#undef Binding
#undef Profile
#define MTA_DE_STATS_NATIVE_INVOKE ClockStatsOwnerTest::Invoke
#include "../../Client/game_de/NativeStatsWriteCalls.cpp"
#undef MTA_DE_STATS_NATIVE_INVOKE
#define MTA_DE_START_GAME_NATIVE_INVOKE ClockStartGameOwnerTest::Invoke
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#undef MTA_DE_START_GAME_NATIVE_INVOKE
#undef GetModuleHandleW
#undef CheckExecutableBuild

// Include the real player primitive once. Only the current-image selection
// and private TU helper names differ; live player, exact CAS and callback
// ownership are unchanged. Wanted substitutes only its final native call.
#define CheckExecutableBuild ClockReaderFixtureExecutable
#define GetModuleHandleW     ClockReaderTestHost::Module
#define P                    ClockPlayerInfoProfile
#define Binding              ClockPlayerInfoBinding
#define Context              ClockPlayerInfoContext
#define Object               ClockPlayerInfoObject
#define ObjectPages          ClockPlayerInfoObjectPages
#define Global               ClockPlayerInfoGlobal
#define Owner                ClockPlayerInfoOwner
#define Exchange             ClockPlayerInfoExchange
#define PlayerIdentity       ClockPlayerInfoIdentity
#include "../../Client/game_de/NativePlayerInfoMemory.cpp"
#undef PlayerIdentity
#undef Exchange
#undef Owner
#undef Global
#undef ObjectPages
#undef Object
#undef Context
#undef Binding
#undef P
#define MTA_DE_WANTED_NATIVE_INVOKE ClockWantedOwnerTest::Invoke
#include "../../Client/game_de/NativeWantedMemory.cpp"
#undef MTA_DE_WANTED_NATIVE_INVOKE
#undef GetModuleHandleW
#undef CheckExecutableBuild

// The rope transaction keeps its true image/reference/pool checks and the
// real retained owner. Only the final native action boundaries are observed.
#define CheckExecutableBuild       ClockReaderFixtureExecutable
#define GetModuleHandleW           ClockReaderTestHost::Module
#define MTA_DE_ROPES_CREATE_INVOKE ClockRopesOwnerTest::Create
#define MTA_DE_ROPES_REMOVE_INVOKE ClockRopesOwnerTest::Remove
#include "../../Client/game_de/NativeRopesMemory.cpp"
#undef MTA_DE_ROPES_REMOVE_INVOKE
#undef MTA_DE_ROPES_CREATE_INVOKE
#undef GetModuleHandleW
#undef CheckExecutableBuild

// The actual resolver keeps its checked OS image/bank reads and the Runtime's
// real callback check. Only the current module selection admits owned bytes.
#define GetModuleHandleW ClockReaderTestHost::Module
#include "../../Client/game_de/NativePedPoolReader.cpp"
#include "../../Client/game_de/NativePedTransformReader.cpp"
#include "../../Client/game_de/NativePedPoseContext.cpp"
#define MTA_DE_PED_TELEPORT_NATIVE_INVOKE ClockPedTeleportOwnerTest::Invoke
#include "../../Client/game_de/NativePedTeleportMemory.cpp"
#undef MTA_DE_PED_TELEPORT_NATIVE_INVOKE
#define MTA_DE_PED_ORIENTATION_NATIVE_INVOKE ClockPedOrientationOwnerTest::Invoke
#include "../../Client/game_de/NativePedOrientationMemory.cpp"
#undef MTA_DE_PED_ORIENTATION_NATIVE_INVOKE
#pragma push_macro("InterlockedCompareExchange")
#ifdef InterlockedCompareExchange
    #undef InterlockedCompareExchange
#endif
#define InterlockedCompareExchange ClockPedVelocityOwnerTest::Exchange
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedObservationOwnerTest::Read
#include "../../Client/game_de/NativePedVelocityMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#pragma pop_macro("InterlockedCompareExchange")
#define MTA_DE_PED_REFERENCE_REGISTER_INVOKE   ClockPedReferenceOwnerTest::Register
#define MTA_DE_PED_REFERENCE_UNREGISTER_INVOKE ClockPedReferenceOwnerTest::Unregister
#include "../../Client/game_de/NativePedReferenceMemory.cpp"
#include "../../Client/game_de/NativePedMetadataMemory.cpp"
// Observe only the qualified ped's raw render field. This hook delegates the
// actual process copy before scheduling fixture faults, never reading its target.
namespace ClockPedRenderOwnerTest
{
    BOOL WINAPI Read(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
}
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedRenderOwnerTest::Read
#include "../../Client/game_de/NativePedRenderMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedMoveAnimOwnerTest::Read
#include "../../Client/game_de/NativePedMoveAnimReader.cpp"
#pragma pop_macro("ReadProcessMemory")
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedIntelligenceOwnerTest::Read
#include "../../Client/game_de/NativePedIntelligenceReader.cpp"
#pragma pop_macro("ReadProcessMemory")
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockTaskPoolOwnerTest::Read
#include "../../Client/game_de/NativeTaskPoolReader.cpp"
#pragma pop_macro("ReadProcessMemory")
// Include each new primitive once. Only final native invocation or a guarded
// bank read is substituted; the real callback/token and native checks remain.
#define MTA_DE_TASK_REMOVAL_NATIVE_INVOKE ClockTaskRemovalOwnerTest::Invoke
#include "../../Client/game_de/NativeTaskRemovalCalls.cpp"
#undef MTA_DE_TASK_REMOVAL_NATIVE_INVOKE
#define ReadImage ModelBankOwnerFixtureReadImage
#include "../../Client/game_de/NativeModelBankReader.cpp"
#undef ReadImage
#define ReadImage ModelLoadedOwnerFixtureReadImage
#include "../../Client/game_de/NativeModelLoadedReader.cpp"
#undef ReadImage
// Shared model context is compiled once under the same current-image seam.
#include "../../Client/game_de/NativeStreamingModelContext.cpp"
#include "../../Client/game_de/NativeModelArchiveReader.cpp"
#include "../../Client/game_de/NativeStreamingModelGroupsReader.cpp"
#include "../../Client/game_de/NativeStreamingCompletionContext.cpp"
namespace ClockStartupInitializationReadinessTest
{
    std::uintptr_t watched = 0;
    void (*afterRead)() noexcept = nullptr;
}
namespace mta::game_de::NativeImageAccess
{
    bool StartupInitializationMenuFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* output, std::size_t bytes, bool writable = false,
                                                   bool executable = false) noexcept
    {
        if (!ReadImage(image, address, output, bytes, writable, executable))
            return false;
        // Schedule only an owner transition after the real guarded read.
        // Native prerequisite and Runtime bookends keep their original results.
        if (address == ClockStartupInitializationReadinessTest::watched && bytes == 1)
        {
            const auto after = ClockStartupInitializationReadinessTest::afterRead;
            ClockStartupInitializationReadinessTest::afterRead = nullptr;
            if (after)
                after();
        }
        return true;
    }
}
namespace ClockStartupMenuInvokeTest
{
    namespace Calls = mta::game_de::NativeStartupMenuCalls;
    std::uintptr_t                   image = 0;
    unsigned                         calls = 0, scenario = 0;
    NativeStartupMenuFixture::Graph* menu = nullptr;
    mta::game_de::ClockResult        Invoke(Calls::NativeFunction, std::uintptr_t, bool&, std::uint32_t&) noexcept;
}
#define MTA_DE_STARTUP_MENU_NATIVE_INVOKE ClockStartupMenuInvokeTest::Invoke
#define ReadImage                         StartupInitializationMenuFixtureReadImage
#include "../../Client/game_de/NativeStartupMenuCalls.cpp"
#undef ReadImage
#undef MTA_DE_STARTUP_MENU_NATIVE_INVOKE
namespace ClockStartupCollisionAtomicTest
{
    std::uintptr_t image = 0;
    unsigned       calls = 0, scenario = 0;
    void*          Exchange(void* volatile* field, void* replacement, void* expected) noexcept
    {
        ++calls;
        // Only the final guarded OS effect is faulted on owned CPU pages.
        // The real NativeAccess method and native controller qualify it.
        if (scenario == 1)
            ::RaiseException(0xe0570328, 0, 0, nullptr);
        auto* observed = ::InterlockedCompareExchangePointer(field, replacement, expected);
        if (scenario == 2)
            ::RaiseException(0xe0570329, 0, 0, nullptr);
        if (scenario == 3)
            ::InterlockedExchange(reinterpret_cast<volatile LONG*>(image + mta::game_de::NativeBorrowedGcProfile::markerRva), LONG{1});
        return observed;
    }
}
#pragma push_macro("InterlockedCompareExchangePointer")
#undef InterlockedCompareExchangePointer
#define InterlockedCompareExchangePointer ClockStartupCollisionAtomicTest::Exchange
#include "../../Client/game_de/NativeStartupModelCollision.cpp"
#pragma pop_macro("InterlockedCompareExchangePointer")
#include "../../Client/game_de/NativeWorldGroundContext.cpp"
#define MTA_DE_WORLD_GROUND_NATIVE_INVOKE ClockWorldGroundOwnerTest::Invoke
#include "../../Client/game_de/NativeWorldGroundCalls.cpp"
#undef MTA_DE_WORLD_GROUND_NATIVE_INVOKE
#define MTA_DE_WORLD_LINE_NATIVE_INVOKE ClockWorldLineOwnerTest::Invoke
#include "../../Client/game_de/NativeWorldLineCalls.cpp"
#undef MTA_DE_WORLD_LINE_NATIVE_INVOKE
namespace ClockWorldIgnoreOwnerTest
{
    std::uintptr_t image = 0, address = 0, expected = 0, desired = 0;
    unsigned       calls = 0, scenario = 0, unregistersAtCas = 0;
    DWORD          priorProtection = PAGE_READWRITE;
    bool           protectionChanged = false;
    void (*reenter)() noexcept = nullptr;
    mta::game_de::NativePedReferenceToken selected{};
    // Substitute only the final OS CAS in the owned image. All actual image,
    // thread/world/ped checks, publication and journal code remain production.
    void* Compare(void* volatile* target, void* next, void* prior) noexcept
    {
        ++calls;
        address = reinterpret_cast<std::uintptr_t>(target);
        expected = reinterpret_cast<std::uintptr_t>(prior);
        desired = reinterpret_cast<std::uintptr_t>(next);
        unregistersAtCas = ClockPedReferenceOwnerTest::unregisters;
        if (reenter)
            reenter();
        auto* old = InterlockedCompareExchangePointer(target, next, prior);
        if (next && scenario == 1)
            protectionChanged = VirtualProtect(reinterpret_cast<void*>(address), sizeof(void*), PAGE_NOACCESS, &priorProtection) != FALSE;
        if (next && scenario == 2)
            *reinterpret_cast<std::uint32_t*>(image + mta::game_de::NativeGameStateProfile::stateRva) = 0;
        return old;
    }
}

#include "../../Client/game_de/NativeWorldAreaMemory.cpp"
#include "../../Client/game_de/NativeWorldJetpackMemory.cpp"
#define MTA_DE_CAMERA_SPHERE_VISIBLE_NATIVE_INVOKE ClockCameraSphereOwnerTest::Invoke
#include "../../Client/game_de/NativeCameraSphereVisibleCalls.cpp"
#undef MTA_DE_CAMERA_SPHERE_VISIBLE_NATIVE_INVOKE
#pragma push_macro("InterlockedCompareExchangePointer")
#undef InterlockedCompareExchangePointer
#define InterlockedCompareExchangePointer ClockWorldIgnoreOwnerTest::Compare
#include "../../Client/game_de/NativeWorldIgnoredEntityMemory.cpp"
#pragma pop_macro("InterlockedCompareExchangePointer")
#define MTA_DE_PED_ONSCREEN_NATIVE_INVOKE ClockPedOnScreenOwnerTest::Invoke
#include "../../Client/game_de/NativePedOnScreenCalls.cpp"
#undef MTA_DE_PED_ONSCREEN_NATIVE_INVOKE
#define MTA_DE_PED_WEAPON_GRANT_NATIVE_INVOKE ClockPedWeaponGrantOwnerTest::Invoke
#include "../../Client/game_de/NativePedWeaponGrantCalls.cpp"
#undef MTA_DE_PED_WEAPON_GRANT_NATIVE_INVOKE
#define MTA_DE_PED_WEAPON_CLEAR_NATIVE_INVOKE ClockPedWeaponClearOwnerTest::Invoke
#include "../../Client/game_de/NativePedWeaponClearCalls.cpp"
#undef MTA_DE_PED_WEAPON_CLEAR_NATIVE_INVOKE
#define MTA_DE_PED_CLOTHES_NATIVE_INVOKE ClockPedClothesOwnerTest::Invoke
#include "../../Client/game_de/NativePedClothesCalls.cpp"
#undef MTA_DE_PED_CLOTHES_NATIVE_INVOKE
#define MTA_DE_PED_REBUILD_NATIVE_INVOKE ClockPedRebuildOwnerTest::Invoke
#include "../../Client/game_de/NativePedRebuildCalls.cpp"
#undef MTA_DE_PED_REBUILD_NATIVE_INVOKE
#define MTA_DE_CLOTHES_DESCRIPTOR_NATIVE_INVOKE ClockModuleClothesInitializerTest::Invoke
#include "../../Client/game_de/NativeClothesDescriptorCalls.cpp"
#undef MTA_DE_CLOTHES_DESCRIPTOR_NATIVE_INVOKE
#define MTA_DE_MODEL_REFERENCE_EXCHANGE ClockModelReferenceOwnerTest::Exchange
#include "../../Client/game_de/NativeModelReferenceMemory.cpp"
#undef MTA_DE_MODEL_REFERENCE_EXCHANGE
#include "../../Client/game_de/NativeStreamingUpgradeReader.cpp"
#define MTA_DE_STREAMING_UPGRADE_NATIVE_INVOKE ClockStreamingUpgradeOwnerTest::Invoke
#include "../../Client/game_de/NativeStreamingUpgradeCalls.cpp"
#undef MTA_DE_STREAMING_UPGRADE_NATIVE_INVOKE
#define MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE ClockStreamingRequestOwnerTest::Invoke
#include "../../Client/game_de/NativeStreamingRequestCalls.cpp"
#undef MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE
#define MTA_DE_STREAMING_LOADALL_NATIVE_INVOKE ClockStreamingLoadAllOwnerTest::Invoke
#include "../../Client/game_de/NativeStreamingLoadAllCalls.cpp"
#undef MTA_DE_STREAMING_LOADALL_NATIVE_INVOKE
#define MTA_DE_STREAMING_SCENE_NATIVE_INVOKE ClockStreamingSceneOwnerTest::Invoke
#include "../../Client/game_de/NativeStreamingSceneCalls.cpp"
#undef MTA_DE_STREAMING_SCENE_NATIVE_INVOKE
#define MTA_DE_STREAMING_PRIORITY_WAIT_INVOKE  ClockStreamingPriorityOwnerTest::Wait
#define MTA_DE_STREAMING_PRIORITY_DRAIN_INVOKE ClockStreamingPriorityOwnerTest::Drain
#include "../../Client/game_de/NativeStreamingPriorityCalls.cpp"
#undef MTA_DE_STREAMING_PRIORITY_DRAIN_INVOKE
#undef MTA_DE_STREAMING_PRIORITY_WAIT_INVOKE
#define MTA_DE_STREAMING_REMOVE_NATIVE_INVOKE ClockStreamingRemoveOwnerTest::Invoke
#include "../../Client/game_de/NativeStreamingRemoveCalls.cpp"
#undef MTA_DE_STREAMING_REMOVE_NATIVE_INVOKE
#define MTA_DE_MODEL_REFERENCE_RELEASE_EXCHANGE ClockModelReferenceOwnerTest::Exchange
#include "../../Client/game_de/NativeModelReferenceRelease.cpp"
#undef MTA_DE_MODEL_REFERENCE_RELEASE_EXCHANGE
#define MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE ClockModelClassificationOwnerTest::Invoke
#include "../../Client/game_de/NativeModelClassificationCalls.cpp"
#undef MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE
#include "../../Client/game_de/NativeStreamingResourceContext.cpp"
#define MTA_DE_STREAMING_RESOURCE_NATIVE_INVOKE ClockStreamingResourceOwnerTest::Invoke
#include "../../Client/game_de/NativeStreamingResourceCalls.cpp"
#define MTA_DE_STREAMING_SPECIAL_MODEL_NATIVE_INVOKE ClockSpecialModelOwnerTest::Invoke
#include "../../Client/game_de/NativeStreamingSpecialModelCalls.cpp"
#undef MTA_DE_STREAMING_SPECIAL_MODEL_NATIVE_INVOKE
#undef MTA_DE_STREAMING_RESOURCE_NATIVE_INVOKE
#define MTA_DE_MODEL_REFERENCE_NATIVE_RESERVE ClockModelAcquireOwnerTest::Reserve
#define MTA_DE_MODEL_REFERENCE_NATIVE_PUBLISH ClockModelAcquireOwnerTest::Publish
#include "../../Client/game_de/NativeModelReferenceTransactions.cpp"
#undef MTA_DE_MODEL_REFERENCE_NATIVE_PUBLISH
#undef MTA_DE_MODEL_REFERENCE_NATIVE_RESERVE
#undef MTA_DE_PED_REFERENCE_UNREGISTER_INVOKE
#undef MTA_DE_PED_REFERENCE_REGISTER_INVOKE
#pragma push_macro("InterlockedCompareExchange")
#ifdef InterlockedCompareExchange
    #undef InterlockedCompareExchange
#endif
#define InterlockedCompareExchange ClockPedVitalsOwnerTest::Exchange
#include "../../Client/game_de/NativePedVitalsMemory.cpp"
#define MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE ClockPedPhysicalScalarsOwnerTest::Exchange
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedObservationOwnerTest::Read
#include "../../Client/game_de/NativePedPhysicalScalarsMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#undef MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE
#define MTA_DE_PED_WATER_FLAGS_EXCHANGE ClockPedWaterOwnerTest::Exchange
#define MTA_DE_PED_AREA_EXCHANGE        ClockPedAreaOwnerTest::Exchange
#include "../../Client/game_de/NativePedAreaMemory.cpp"
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedFireOwnerTest::Read
#include "../../Client/game_de/NativePedFireMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#define MTA_DE_PED_COLLISION_EXCHANGE ClockPedCollisionOwnerTest::Exchange
#include "../../Client/game_de/NativePedCollisionMemory.cpp"
#undef MTA_DE_PED_COLLISION_EXCHANGE
#undef MTA_DE_PED_AREA_EXCHANGE
#include "../../Client/game_de/NativePedWaterFlagsMemory.cpp"
#include "../../Client/game_de/NativePedWaterTaskReader.cpp"
// Only the selected owned-image read boundary schedules transitions. Native
// identity, current callback and the shipping reader's bookends stay intact.
#define ReadImage AnimationBlockOwnerFixtureReadImage
#include "../../Client/game_de/NativeAnimationBlockMemory.cpp"
#undef ReadImage
#define ReadImage                           AnimationBlockRefOwnerFixtureReadImage
#define MTA_DE_ANIMATION_BLOCK_REF_EXCHANGE ClockAnimationBlockRefOwnerTest::Exchange
#include "../../Client/game_de/NativeAnimationBlockRefMemory.cpp"
#undef MTA_DE_ANIMATION_BLOCK_REF_EXCHANGE
#undef ReadImage
// The focused writer delegates to the genuine guarded word exchange before
// scheduling conflicts or lost acknowledgements in its owned private rows.
namespace ClockStaticAssociationInitializationInvokeTest
{
    namespace Calls = mta::game_de::NativeStaticAssociationInitializeCalls;
    mta::game_de::ClockResult Invoke(Calls::NativeFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
}
// Include the real original-entry boundary once. Only its final typed native
// call is substituted by owned effects; all profile, membership and gate checks
// remain production. Other selected scopes never execute this new fixture.
namespace ClockGroupEmptyInitializationInvokeTest
{
    mta::game_de::ClockResult Invoke(mta::game_de::NativeStaticAssociationInitializeCalls::NativeFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                     std::uint32_t&) noexcept;
}
// Historical scopes delegate unchanged. Only the new group-loop fixture
// substitutes the same final original-entry seam with per-physical-row effects.
#define MTA_DE_STATIC_ASSOCIATION_INITIALIZE_NATIVE_INVOKE ClockGroupEmptyInitializationInvokeTest::Invoke
#include "../../Client/game_de/NativeStaticAssociationInitializeCalls.cpp"
#undef MTA_DE_STATIC_ASSOCIATION_INITIALIZE_NATIVE_INVOKE
namespace ClockAnimationStaticAssociationWriteTest
{
    mta::game_de::ClockResult Exchange(std::uintptr_t, std::uint16_t, std::uint16_t, std::uint16_t&, bool&, std::uint32_t&) noexcept;
}
namespace ClockAnimationAssociationGroupOffsetTest
{
    mta::game_de::ClockResult Exchange(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, bool&, std::uint32_t&) noexcept;
}
#define MTA_DE_ANIMATION_GROUP_OFFSET_EXCHANGE       ::ClockAnimationAssociationGroupOffsetTest::Exchange
#define ReadImage                                    AnimationManagerOwnerFixtureReadImage
#define MTA_DE_ANIMATION_STATIC_ASSOCIATION_EXCHANGE ::ClockAnimationStaticAssociationWriteTest::Exchange
#include "../../Client/game_de/NativeAnimationManagerMemory.cpp"
#undef MTA_DE_ANIMATION_STATIC_ASSOCIATION_EXCHANGE
#undef MTA_DE_ANIMATION_GROUP_OFFSET_EXCHANGE
#undef ReadImage
#define ReadImage AnimationBlockOwnerFixtureReadImage
#include "../../Client/game_de/NativeAnimationBlockFactoryMemory.cpp"
#undef ReadImage
#define ReadImage AnimationHierarchyHashOwnerFixtureReadImage
#include "../../Client/game_de/NativeAnimationHierarchyHashMemory.cpp"
#undef ReadImage
#define ReadImage                       AnimationHierarchyBankOwnerFixtureReadImage
#define MTA_DE_ANIMATION_HIERARCHY_COPY ClockAnimationHierarchyBankOwnerTest::Copy
#include "../../Client/game_de/NativeAnimationHierarchyMemory.cpp"
#undef MTA_DE_ANIMATION_HIERARCHY_COPY
#undef ReadImage
namespace ClockAnimationHierarchyBankOwnerTest
{
    bool Copy(std::uintptr_t source, void* target, std::size_t bytes) noexcept
    {
        const bool selected = source == cookie && bytes == 8;
        if (selected && ++childReads == failChild)
            return false;
        if (!mta::game_de::AnimationHierarchyReadDetail::Copy(source, target, bytes))
            return false;
        if (selected && afterChild)
        {
            const auto after = afterChild;
            afterChild = nullptr;
            after();
        }
        return true;
    }
}

#include "../../Client/game_de/NativeModelReferenceMap.h"
#include "../../Client/game_de/NativeModelReferenceHeap.h"
namespace ClockPrivateFactoryOwnerReadFixture
{
    std::uintptr_t address = 0;
    unsigned       reads = 0;
    void (*afterCopy)() noexcept = nullptr;
}
namespace mta::game_de::NativeModelReferenceMap
{
    bool PrivateFactoryOwnerFixtureCopy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        if (!Copy(address, output, bytes))
            return false;
        if (address == ClockPrivateFactoryOwnerReadFixture::address && bytes == sizeof(std::uint64_t))
        {
            ++ClockPrivateFactoryOwnerReadFixture::reads;
            if (const auto after = ClockPrivateFactoryOwnerReadFixture::afterCopy)
            {
                ClockPrivateFactoryOwnerReadFixture::afterCopy = nullptr;
                after();
            }
        }
        return true;
    }
}

namespace mta::game_de::PrivateFactoryOwnerFixtureMap
{
    using namespace NativeModelReferenceMap;
    bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        return NativeModelReferenceMap::PrivateFactoryOwnerFixtureCopy(address, output, bytes);
    }
}
namespace mta::game_de::PrivateFactoryOwnerFixtureHeap
{
    // Reuse every genuine heap function and Value type. Only direct private
    // copies in this included fixture TU select the observing map alias.
    using namespace NativeModelReferenceHeap;
    namespace M = PrivateFactoryOwnerFixtureMap;
}

#define MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE  ::NativeCustomAnimSequenceStorageFixture::Malloc
#define MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE    ::NativeCustomAnimSequenceStorageFixture::Free
#define MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE        ClockSequenceRowOwnerTest::Write
#define MTA_DE_ANIMATION_SEQUENCE_COPY_WRITE       ClockSequenceCopyFrameOwnerTest::Write
#define MTA_DE_ANIMATION_SEQUENCE_NAME_WRITE       ClockSequenceNameOwnerTest::Write
#define MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE ClockFreshCompressedSequenceOwnerTest::Write
#define CheckFramePointer                          ClockRowsOriginalFrameQualifier
// Delegate actual private-page copying; only this selected fixture cookie
// schedules deterministic caller/admission changes after a real OS read.
#define NativeModelReferenceHeap PrivateFactoryOwnerFixtureHeap
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.cpp"
#undef NativeModelReferenceHeap
#undef CheckFramePointer
#undef MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE
#undef MTA_DE_ANIMATION_SEQUENCE_COPY_WRITE
#undef MTA_DE_ANIMATION_SEQUENCE_NAME_WRITE
#undef MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE

namespace mta::game_de::NativeAnimationSequenceRowCalls
{
    ClockResult CheckFramePointer(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::uintptr_t pointer, CheckOwner owner) noexcept
    {
        const auto result = ClockRowsOriginalFrameQualifier(image, storage, generation, pointer, owner);
        if (result == ClockResult::Ok && ClockSequenceRowOwnerTest::afterFrame)
        {
            const auto after = ClockSequenceRowOwnerTest::afterFrame;
            ClockSequenceRowOwnerTest::afterFrame = nullptr;
            after();
        }
        return result;
    }
}
namespace ClockSequenceRowOwnerTest
{
    mta::game_de::ClockResult Write(std::uintptr_t rowAddress, Rows::Operation operation, const Rows::Value& target, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteRow(rowAddress, operation, target, exception);
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        if (mode == 3 && caller)
        {
            caller->ownerGeneration = 0;
            caller->firstTag = 1;
            caller->index = (std::numeric_limits<std::size_t>::max)();
        }
        if (mode == 1 || mode == 2)
        {
            const LONG gc = 1;
            SIZE_T     written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(),
                                      reinterpret_cast<void*>(ClockReaderTestHost::image + mta::game_de::NativeBorrowedGcProfile::markerRva), &gc, sizeof(gc),
                                      &written) ||
                written != sizeof(gc))
                std::abort();
        }
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return result;
    }
}

namespace ClockSequenceCopyFrameOwnerTest
{
    mta::game_de::ClockResult Write(std::uintptr_t target, const Copy::Record& source, std::uint32_t& exception) noexcept
    {
        ++copies;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::CopyRow(target, source, exception);
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        if (mode == 3 && caller)
        {
            caller->ownerGeneration = 0;
            caller->firstTag = 1;
            caller->index = (std::numeric_limits<std::size_t>::max)();
        }
        if (mode == 1 || mode == 2)
        {
            const LONG gc = 1;
            SIZE_T     written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(),
                                      reinterpret_cast<void*>(ClockReaderTestHost::image + mta::game_de::NativeBorrowedGcProfile::markerRva), &gc, sizeof(gc),
                                      &written) ||
                written != sizeof(gc))
                std::abort();
        }
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return result;
    }
}

namespace ClockSequenceNameOwnerTest
{
    mta::game_de::ClockResult Write(std::uintptr_t row, const Calls::Name& name, std::size_t terminatedBytes, bool projectBone, std::uint16_t bone,
                                    std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteName(row, name, terminatedBytes, projectBone, bone, exception);
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        if (mode == 3)
        {
            if (caller)
            {
                caller->ownerGeneration = 0;
                caller->firstTag = 1;
                caller->index = (std::numeric_limits<std::size_t>::max)();
            }
            if (callerName)
                callerName->fill('X');
        }
        if (mode == 1 || mode == 2)
        {
            const LONG gc = 1;
            SIZE_T     written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(),
                                      reinterpret_cast<void*>(ClockReaderTestHost::image + mta::game_de::NativeBorrowedGcProfile::markerRva), &gc, sizeof(gc),
                                      &written) ||
                written != sizeof(gc))
                std::abort();
        }
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return result;
    }
}

namespace ClockFreshCompressedSequenceOwnerTest
{
    mta::game_de::ClockResult Write(std::uintptr_t row, const Bind::Value& target, std::uint32_t& exception) noexcept
    {
        ++writes;
        const auto result = mta::game_de::CustomAnimSequenceStorageDetail::WriteCompressedFrames(row, target, exception);
        if (result != mta::game_de::ClockResult::Ok)
            return result;
        if (mode == 3 && caller)
        {
            caller->ownerGeneration = 0;
            caller->firstTag = 1;
            caller->index = (std::numeric_limits<std::size_t>::max)();
        }
        if (mode == 1 || mode == 2)
        {
            const LONG gc = 1;
            SIZE_T     written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(),
                                      reinterpret_cast<void*>(ClockReaderTestHost::image + mta::game_de::NativeBorrowedGcProfile::markerRva), &gc, sizeof(gc),
                                      &written) ||
                written != sizeof(gc))
                std::abort();
        }
        if (mode == 1)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return mta::game_de::ClockResult::OriginalException;
        }
        return result;
    }
}

#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE
#undef MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE
#include "../../Client/game_de/NativeCustomAnimSequenceController.cpp"
// The new owner fixture executes the exact admitted copied bodies. Its hooks
// route only the owned native heap and inject failures after actual return.
namespace ClockPrivateAnimationAssociationTest
{
    mta::game_de::ClockResult Construct(mta::game_de::NativeAnimationAssociationCalls::ConstructorFunction, std::uintptr_t, std::uintptr_t, std::uintptr_t&,
                                        std::uint32_t&) noexcept;
    mta::game_de::ClockResult Destroy(mta::game_de::NativeAnimationAssociationCalls::DestructorFunction, std::uintptr_t, std::uint32_t, std::uintptr_t&,
                                      std::uint32_t&) noexcept;
    mta::game_de::ClockResult Exchange(std::uintptr_t, std::size_t, std::uint32_t, std::uint32_t, std::uint32_t&, bool&, std::uint32_t&) noexcept;
}
namespace ClockAnimationAssociationBonesTest
{
    mta::game_de::ClockResult ExchangeNode(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t&, bool&, std::uint32_t&) noexcept;
}
namespace ClockAnimationAssociationProgressTest
{
    mta::game_de::ClockResult Invoke(mta::game_de::NativeAnimationAssociationCalls::ProgressFunction, std::uintptr_t, float, std::uint32_t&) noexcept;
}
namespace ClockAnimationAssociationNodeArrayTest
{
    mta::game_de::ClockResult Free(mta::game_de::NativeCustomAnimSequenceStorageCalls::FreeFunction, std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
    mta::game_de::ClockResult Clear(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t&, bool&, std::uint32_t&) noexcept;
    mta::game_de::ClockResult Destroy(mta::game_de::NativeAnimationAssociationCalls::DestructorFunction, std::uintptr_t, std::uint32_t, std::uintptr_t&,
                                      std::uint32_t&) noexcept;
}
#define MTA_DE_ANIMATION_ASSOCIATION_NODE_FREE_INVOKE   ::ClockAnimationAssociationNodeArrayTest::Free
#define MTA_DE_ANIMATION_ASSOCIATION_NODE_POINTER_CLEAR ::ClockAnimationAssociationNodeArrayTest::Clear
#define MTA_DE_ANIMATION_ASSOCIATION_PROGRESS_INVOKE    ::ClockAnimationAssociationProgressTest::Invoke
#define MTA_DE_ANIMATION_ASSOCIATION_NODE_EXCHANGE      ::ClockAnimationAssociationBonesTest::ExchangeNode
#define MTA_DE_ANIMATION_ASSOCIATION_CONSTRUCTOR_INVOKE ::ClockPrivateAnimationAssociationTest::Construct
#define MTA_DE_ANIMATION_ASSOCIATION_DESTRUCTOR_INVOKE  ::ClockAnimationAssociationNodeArrayTest::Destroy
#define MTA_DE_ANIMATION_ASSOCIATION_EXCHANGE           ::ClockPrivateAnimationAssociationTest::Exchange
#include "../../Client/game_de/NativeAnimationAssociationStorage.cpp"
#undef MTA_DE_ANIMATION_ASSOCIATION_NODE_FREE_INVOKE
#undef MTA_DE_ANIMATION_ASSOCIATION_NODE_POINTER_CLEAR
#undef MTA_DE_ANIMATION_ASSOCIATION_PROGRESS_INVOKE
#undef MTA_DE_ANIMATION_ASSOCIATION_NODE_EXCHANGE
#undef MTA_DE_ANIMATION_ASSOCIATION_EXCHANGE
#undef MTA_DE_ANIMATION_ASSOCIATION_DESTRUCTOR_INVOKE
#undef MTA_DE_ANIMATION_ASSOCIATION_CONSTRUCTOR_INVOKE
#undef MTA_DE_PED_WATER_FLAGS_EXCHANGE
#define MTA_DE_PED_PLACEMENT_EXCHANGE ClockPedPhysicalScalarsOwnerTest::Exchange
#include "../../Client/game_de/NativePedPlacementMemory.cpp"
#undef MTA_DE_PED_PLACEMENT_EXCHANGE
#define MTA_DE_PED_ROTATION_EXCHANGE ClockPedRotationOwnerTest::Exchange
#include "../../Client/game_de/NativePedRotationMemory.cpp"
#undef MTA_DE_PED_ROTATION_EXCHANGE
#define MTA_DE_PED_OXYGEN_EXCHANGE ClockPedOxygenOwnerTest::Exchange
#include "../../Client/game_de/NativePedOxygenMemory.cpp"
#undef MTA_DE_PED_OXYGEN_EXCHANGE
#define MTA_DE_PED_DUCKING_EXCHANGE ClockPedDuckingSetterOwnerTest::Exchange
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedGogglesOwnerTest::Read
#include "../../Client/game_de/NativePedGogglesMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#define MTA_DE_PED_VISIBILITY_EXCHANGE ClockPedVisibilityOwnerTest::Exchange
#pragma push_macro("ReadProcessMemory")
#define ReadProcessMemory ClockPedVisibilityOwnerTest::Read
#include "../../Client/game_de/NativePedVisibilityMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#undef MTA_DE_PED_VISIBILITY_EXCHANGE
#include "../../Client/game_de/NativePedDuckingMemory.cpp"
#undef MTA_DE_PED_DUCKING_EXCHANGE
#define MTA_DE_PED_BIKE_FLAGS_EXCHANGE ClockPedFlagsOwnerTest::ExchangeBike
#include "../../Client/game_de/NativePedBikeFlagsMemory.cpp"
#undef MTA_DE_PED_BIKE_FLAGS_EXCHANGE
#define MTA_DE_PED_SHOT_FLAGS_EXCHANGE ClockPedFlagsOwnerTest::ExchangeShot
#include "../../Client/game_de/NativePedShotFlagsMemory.cpp"
#undef MTA_DE_PED_SHOT_FLAGS_EXCHANGE
#define MTA_DE_PED_BLEEDING_EXCHANGE ClockPedBehaviorOwnerTest::ExchangeBleeding
#include "../../Client/game_de/NativePedBleedingMemory.cpp"
#undef MTA_DE_PED_BLEEDING_EXCHANGE
#define MTA_DE_PED_STAY_EXCHANGE ClockPedBehaviorOwnerTest::ExchangeStay
#include "../../Client/game_de/NativePedStayMemory.cpp"
#undef MTA_DE_PED_STAY_EXCHANGE
#define MTA_DE_PED_FIGHTING_STYLE_EXCHANGE ClockPedCombatOwnerTest::Exchange
#include "../../Client/game_de/NativePedFightingStyleMemory.cpp"
#undef MTA_DE_PED_FIGHTING_STYLE_EXCHANGE
#define MTA_DE_NAMED_GARAGE_EXCHANGE ClockNamedGarageOwnerTest::Exchange
#pragma push_macro("ReadProcessMemory")
#undef ReadProcessMemory
#define ReadProcessMemory ClockNamedGarageOwnerTest::Read
#include "../../Client/game_de/NativeNamedGarageMemory.cpp"
#include "../../Client/game_de/NativeNumericGarageMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#undef MTA_DE_NAMED_GARAGE_EXCHANGE
#include "../../Client/game_de/NativePedWeaponContext.cpp"
#pragma push_macro("ReadProcessMemory")
#undef ReadProcessMemory
#define ReadProcessMemory ClockPedCurrentWeaponOwnerTest::Read
#include "../../Client/game_de/NativePedCurrentWeaponSlotMemory.cpp"
#pragma pop_macro("ReadProcessMemory")
#define MTA_DE_PED_WEAPON_FIELDS_EXCHANGE ClockPedWeaponOwnerTest::ExchangeFields
#include "../../Client/game_de/NativePedWeaponFieldsMemory.cpp"
#undef MTA_DE_PED_WEAPON_FIELDS_EXCHANGE
#define MTA_DE_PED_WEAPON_AMMO_EXCHANGE ClockPedWeaponOwnerTest::ExchangeAmmo
#include "../../Client/game_de/NativePedWeaponAmmoMemory.cpp"
#undef MTA_DE_PED_WEAPON_AMMO_EXCHANGE

#pragma pop_macro("InterlockedCompareExchange")
#undef GetModuleHandleW

namespace mta::game_de::GameTimeReaderFixtureMemory
{
    bool Read(std::uintptr_t image, ClockValues& output) noexcept
    {
        if (!NativeClockMemory::Read(image, output))
            return false;
        if (ClockReaderTestHost::samples.fetch_add(1) + 1 == ClockReaderTestHost::mismatchSample.load())
            ++output.seconds;
        return true;
    }
    bool Equal(const ClockValues& first, const ClockValues& second) noexcept
    {
        return NativeClockMemory::Equal(first, second);
    }
}

// These test-TU-only wrappers select the same owned native image used by the
// clock fixture and introduce a deterministic mismatch between complete reads.
// Shipping code keeps the real exact-executable gate and raw shared primitive.
#define CheckExecutableBuild ClockReaderFixtureExecutable
#define GetModuleHandleW     ClockReaderTestHost::Module
#define NativeClockMemory    GameTimeReaderFixtureMemory
#include "../../Client/game_de/NativeGameTimeReader.cpp"
#undef NativeClockMemory
#undef GetModuleHandleW
#undef CheckExecutableBuild

// Select only the owned image and one deterministic sample mismatch for the
// timing prerequisite. All pin/page checks and real CGame method bodies remain
// production code; no complete CGame implementation is fabricated for tests.
#define GetModuleHandleW ClockReaderTestHost::Module
#define ReadImage        TimingReaderFixtureReadImage
#include "../../Client/game_de/NativeGameTimingReader.cpp"
#undef ReadImage
#undef GetModuleHandleW

using namespace mta::game_de;

namespace
{
    std::atomic<unsigned> checks{0};
    void                  Check(bool condition, const char* why)
    {
        const auto count = checks.fetch_add(1) + 1;
        if (!condition)
        {
            std::fprintf(stderr, "Clock service failed after %u checks: %s\n", count, why);
            std::exit(1);
        }
        std::printf("Check %u: %s\n", count, why);
    }

    class ClockImageFixture
    {
    public:
        static constexpr std::uint32_t TextRva = 0x1000000, TextSize = 0x300000, DataRva = 0x5200000, DataSize = 0x40000;
        std::uintptr_t                 image = 0;
        HANDLE                         file = INVALID_HANDLE_VALUE, mapping = nullptr;
        void (*invoke)() = nullptr;
        void (*invokeTimer)() = nullptr;
        void (*invokeAlternateTimer)() = nullptr;
        NativeApplicationFrameProfile::FrameFunction invokeFrame = nullptr, invokeLoadingFrame = nullptr;
        std::array<unsigned char, 0x300>             frameCaller{};
        std::array<std::uintptr_t, (NativeStartupCompletionProfile::PumpOffset + sizeof(std::uintptr_t)) / sizeof(std::uintptr_t)> frameOwner{};
        std::array<unsigned char, NativeUiClassMapProfile::entryBytes * 2>                                                         uiEntries{};
        std::array<char16_t, NativeUiClassMapProfile::keyLoadingCount>    uiLoading{u'L', u'o', u'a', u'd', u'i', u'n', u'g', 0};
        std::array<char16_t, NativeUiClassMapProfile::keySimpleFadeCount> uiSimpleFade{u'S', u'i', u'm', u'p', u'l', u'e', u'F', u'a', u'd', u'e', 0};
        std::uintptr_t                                                    frameReceiver = 0;
        alignas(std::uintptr_t) std::array<unsigned char, 0xb00> statsPed{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x3c8> statsData{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x20> statsScript{};

        ~ClockImageFixture()
        {
            if (image)
                UnmapViewOfFile(reinterpret_cast<void*>(image));
            if (mapping)
                CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
        void Create(const std::filesystem::path& root)
        {
            const auto path = root / L"owned-clock-image.bin";
            Check(!std::filesystem::exists(path), "owned native image evidence is created once without overwriting an earlier run");
            std::vector<unsigned char> bytes(0x1000 + TextSize + DataSize, 0);
            IMAGE_DOS_HEADER           dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 18;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.ImageBase = UINT64_C(0x180000000);
            nt.OptionalHeader.SectionAlignment = 0x1000;
            nt.OptionalHeader.FileAlignment = 0x200;
            nt.OptionalHeader.SizeOfHeaders = 0x1000;
            nt.OptionalHeader.SizeOfCode = TextSize;
            nt.OptionalHeader.SizeOfInitializedData = DataSize;
            nt.OptionalHeader.BaseOfCode = TextRva;
            nt.OptionalHeader.SizeOfImage = 0x5ca5000;
            nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
            nt.OptionalHeader.MajorOperatingSystemVersion = 6;
            nt.OptionalHeader.MajorSubsystemVersion = 6;
            nt.OptionalHeader.SizeOfStackReserve = nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
            nt.OptionalHeader.SizeOfStackCommit = nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            std::memcpy(bytes.data() + dos.e_lfanew, &nt, sizeof(nt));
            std::array<IMAGE_SECTION_HEADER, 18> sections{};
            // A Windows image section table must describe adjacent virtual
            // ranges. Sparse zero-filled sections preserve the actual native
            // RVAs without leaving loader-invalid gaps or a huge raw file.
            const auto zeroSection = [&](std::size_t index, const char* name, std::uint32_t rva, std::uint32_t size)
            {
                std::memcpy(sections[index].Name, name, std::strlen(name));
                sections[index].VirtualAddress = rva;
                sections[index].Misc.VirtualSize = size;
                sections[index].Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            };
            zeroSection(0, ".head", 0x1000, TextRva - 0x1000);
            zeroSection(2, ".late", TextRva + TextSize, 0x1400000 - TextRva - TextSize);
            // The real flag array needs an independent writable image section.
            // Keep every registration literal and dispatch slot in immutable
            // sections instead of weakening their policy to admit this page.
            constexpr std::uint32_t cheatPageRva = static_cast<std::uint32_t>(NativeCheatProfile::FlagsRva & ~std::uintptr_t{0xfff});
            constexpr std::uint32_t animationPageRva = static_cast<std::uint32_t>(NativeWeaponInfoWriteProfile::AnimationCountRva & ~std::uintptr_t{0xfff});
            // The completed native player profile independently calls the
            // atanf import thunk at 0x3e98842. Give that exact page its own RX
            // section; changing page protection alone cannot qualify code in
            // an otherwise read-only PE section, and surrounding SDK literals
            // must retain their non-executable section contract.
            constexpr std::uint32_t mathPageRva = 0x3e98000;
            // The Teleport prerequisite reuses real UObject membership and
            // logical-world profiles whose instructions lie in three later
            // native ranges. Model just those pages as code; surrounding SDK
            // literals/banks keep their established immutable section policy.
            zeroSection(3, ".gap", 0x1400000, 0x1ca8000 - 0x1400000);
            zeroSection(4, ".obj", 0x1ca8000, 0x1000);
            zeroSection(5, ".gapO", 0x1ca9000, 0x2fa9000 - 0x1ca9000);
            // The 171-byte native Level initializer crosses into 2faa000.
            zeroSection(6, ".level", 0x2fa9000, 0x2000);
            zeroSection(7, ".gapL", 0x2fab000, 0x3002000 - 0x2fab000);
            zeroSection(8, ".actor", 0x3002000, 0x1000);
            zeroSection(9, ".gapA", 0x3003000, mathPageRva - 0x3003000);
            zeroSection(10, ".math", mathPageRva, 0x1000);
            zeroSection(11, ".gapM", mathPageRva + 0x1000, animationPageRva - mathPageRva - 0x1000);
            zeroSection(12, ".anims", animationPageRva, 0x1000);
            zeroSection(13, ".gap2", animationPageRva + 0x1000, cheatPageRva - animationPageRva - 0x1000);
            zeroSection(14, ".cheats", cheatPageRva, 0x1000);
            zeroSection(15, ".gap3", cheatPageRva + 0x1000, DataRva - cheatPageRva - 0x1000);
            zeroSection(17, ".tail", DataRva + DataSize, nt.OptionalHeader.SizeOfImage - DataRva - DataSize);
            // Only bounded admitted pages gain executable/writable protection
            // below. These virtual-section flags also describe the earlier
            // application caller and later root/state fields to the real gate.
            sections[0].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[2].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[4].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[6].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[8].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[10].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[12].Characteristics |= IMAGE_SCN_MEM_WRITE;
            sections[14].Characteristics |= IMAGE_SCN_MEM_WRITE;
            sections[17].Characteristics |= IMAGE_SCN_MEM_WRITE;
            std::memcpy(sections[1].Name, ".text", 5);
            sections[1].VirtualAddress = TextRva;
            sections[1].Misc.VirtualSize = sections[1].SizeOfRawData = TextSize;
            sections[1].PointerToRawData = 0x1000;
            sections[1].Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            std::memcpy(sections[16].Name, ".data", 5);
            sections[16].VirtualAddress = DataRva;
            sections[16].Misc.VirtualSize = sections[16].SizeOfRawData = DataSize;
            sections[16].PointerToRawData = 0x1000 + TextSize;
            sections[16].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            std::memcpy(bytes.data() + dos.e_lfanew + sizeof(nt), sections.data(), sizeof(sections));
            const auto code = [&](std::uintptr_t rva, const auto& data) { std::memcpy(bytes.data() + 0x1000 + rva - TextRva, data.data(), data.size()); };
            code(NativeClockProfile::clockCodeRva, NativeClockProfile::clockCode);
            code(NativeClockProfile::hookEntryRva, NativeClockProfile::hookEntry);
            code(NativeClockProfile::hookTailRva, NativeClockProfile::hookTail);
            code(NativeClockProfile::clockCallRva, NativeClockProfile::clockCall);
            code(NativeClockProfile::frameCallRva, NativeClockProfile::frameCall);
            code(NativeClockProfile::getterCaseRva, NativeClockProfile::getterCase);
            code(NativeGameTimingProfile::initializeRva, NativeGameTimingProfile::initialize);
            code(NativeGameTimingProfile::elapsedMultiplierRva, NativeGameTimingProfile::elapsedMultiplier);
            code(NativeGameTimingProfile::stepUpdateRva, NativeGameTimingProfile::stepUpdate);
            code(NativeGameTimingProfile::frameGetterRva, NativeGameTimingProfile::frameGetter);
            code(NativeGameTimingProfile::stepConsumerRva, NativeGameTimingProfile::stepConsumer);
            code(NativeGameTimingProfile::scriptScaleRva, NativeGameTimingProfile::scriptScale);
            code(NativeGamePauseProfile::entryRva, NativeGamePauseProfile::entry);
            code(NativeGamePauseProfile::tailRva, NativeGamePauseProfile::tail);
            code(NativeGamePauseProfile::pauseReadsRva, NativeGamePauseProfile::pauseReads);
            code(NativeGamePauseProfile::pauseBranchRva, NativeGamePauseProfile::pauseBranch);
            code(NativeGamePauseProfile::frameRva, NativeGamePauseProfile::frame);
            code(NativeGamePauseProfile::returnRva, NativeGamePauseProfile::returnCode);
            code(NativeGamePauseProfile::normalCallRva, NativeGamePauseProfile::normalCall);
            code(NativeGamePauseProfile::alternateCallRva, NativeGamePauseProfile::alternateCall);
            code(NativeGamePauseProfile::gameplayTailRva, NativeGamePauseProfile::gameplayTail);
            // Preserve the independently pinned original prologue and supply
            // only an owned return body. Its native clock update is not executed.
            const std::array<unsigned char, 8> epilogue{0x48, 0x81, 0xc4, 0xd8, 0, 0, 0, 0xc3};
            code(NativeClockProfile::hookTailRva + NativeClockProfile::hookTail.size(), epilogue);
            const std::array<unsigned char, 5> callReturn{0x48, 0x83, 0xc4, 0x28, 0xc3};
            code(NativeClockProfile::clockCallRva + NativeClockProfile::clockCall.size(), callReturn);
            code(NativeGamePauseProfile::normalReturnRva, callReturn);
            code(0x129702a, callReturn);
            // Reproduce the independently admitted alternate tail-call origin
            // in this owned image. No native state dispatcher is executed.
            std::array<unsigned char, 5> timerTail{0xe9};
            const auto                   timerDisplacement = static_cast<std::int32_t>(NativeGamePauseProfile::entryRva - 0x1209e10 - timerTail.size());
            std::memcpy(timerTail.data() + 1, &timerDisplacement, sizeof(timerDisplacement));
            code(0x1209e10, timerTail);
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save the owned synthetic PE used by actual Windows SEC_IMAGE page validation");
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "open only the task-owned synthetic native image");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            const auto mappingError = GetLastError();
            std::printf("Owned image SEC_IMAGE mapping GetLastError=%lu\n", mappingError);
            Check(mapping != nullptr, "create actual Windows image mapping without weakening shipping MEM_IMAGE checks");
            image = reinterpret_cast<std::uintptr_t>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != 0, "map the owned image with actual executable and writable native sections");
            // The production executable records a non-page-aligned SizeOfImage.
            // Keep the kernel mapping legal, then seed those exact observed
            // metadata bytes inside this independently owned image fixture.
            nt.OptionalHeader.SizeOfImage = 97141760;
            Patch(static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt));
            std::array<unsigned char, 16> thunk{0x48, 0x83, 0xec, 0x28, 0x48, 0xb8};
            const auto                    target = image + NativeClockProfile::clockCallRva;
            std::memcpy(thunk.data() + 6, &target, sizeof(target));
            thunk[14] = 0xff;
            thunk[15] = 0xe0;
            Patch(TextRva + 0x1000, thunk.data(), thunk.size());
            invoke = reinterpret_cast<void (*)()>(image + TextRva + 0x1000);
            const auto timerThunk = [&](std::uintptr_t rva, std::uintptr_t call)
            {
                auto       timerCode = thunk;
                const auto destination = image + call;
                std::memcpy(timerCode.data() + 6, &destination, sizeof(destination));
                Patch(rva, timerCode.data(), timerCode.size());
                return reinterpret_cast<void (*)()>(image + rva);
            };
            invokeTimer = timerThunk(TextRva + 0x1100, NativeGamePauseProfile::normalCallRva);
            invokeAlternateTimer = timerThunk(TextRva + 0x1200, NativeGamePauseProfile::alternateCallRva);
            ClockTestFaults::patchAddress = image + NativeClockProfile::patchRva - 3;
            ClockTestFaults::dataAddress = image + 0x522bcf8;
            AdmitFPSPins();
        }
        void AdmitFPSPins()
        {
            // FPS lives far from the compact clock sections. Populate only
            // this owned SEC_IMAGE's zero-filled virtual pages, then mark its
            // code RX, constants RO and data RW. This keeps MEM_IMAGE/page
            // checks real without copying GTA or growing a huge sparse raw file.
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t length, DWORD protection)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, PAGE_READWRITE, &previous) != FALSE,
                      "admit only the synthetic SEC_IMAGE FPS fixture page for initialization");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, length);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), length),
                      "set exact synthetic FPS code/data protections without executable writable pages");
            };
            const auto code = [&](std::uintptr_t rva, const auto& bytes) { admit(rva, bytes.data(), bytes.size(), PAGE_EXECUTE_READ); };
            code(NativeGameFPSProfile::frequencyScaleRva, NativeGameFPSProfile::frequencyScale);
            code(NativeGameFPSProfile::computationRva, NativeGameFPSProfile::computation);
            code(NativeGameFPSProfile::consumerRva, NativeGameFPSProfile::consumer);
            admit(NativeGameFPSProfile::labelRva, NativeGameFPSProfile::label.data(), NativeGameFPSProfile::label.size(), PAGE_READONLY);
            const auto constant = [&](std::uintptr_t rva, const auto& value) { admit(rva, &value, sizeof(value), PAGE_READONLY); };
            constant(NativeGameFPSProfile::previousWeightRva, NativeGameFPSProfile::previousWeight);
            constant(NativeGameFPSProfile::currentWeightRva, NativeGameFPSProfile::currentWeight);
            constant(NativeGameFPSProfile::fpsNumeratorRva, NativeGameFPSProfile::fpsNumerator);
            constant(NativeGameFPSProfile::millisecondsRva, NativeGameFPSProfile::milliseconds);
            constant(NativeGameFPSProfile::secondsOriginRva, NativeGameFPSProfile::secondsOrigin);
            constant(NativeGameFPSProfile::frequencyNumeratorRva, NativeGameFPSProfile::frequencyNumerator);
            const std::array<float, 2> values{16.0f, 62.5f};
            const double               previousSeconds = 16777216.0, secondsPerCycle = 0.000001;
            admit(NativeGameFPSProfile::averageMsRva, values.data(), sizeof(values), PAGE_READWRITE);
            admit(NativeGameFPSProfile::previousSecondsRva, &previousSeconds, sizeof(previousSeconds), PAGE_READWRITE);
            admit(NativeGameFPSProfile::secondsPerCycleRva, &secondsPerCycle, sizeof(secondsPerCycle), PAGE_READWRITE);
        }
        void AdmitCheatPins()
        {
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &previous) != FALSE,
                      "initialize only the owned cheat profile's bounded image pages");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), size),
                      "restore owned cheat proof code RX, immutable data RO and flag array RW");
            };
            for (const auto& pin : NativeCheatProfile::Code)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& registration : NativeCheatProfile::Registrations)
            {
                std::array<unsigned char, 7> instruction{0x4c, 0x8d, 0x05};
                const auto                   relative = static_cast<std::int32_t>(registration.nameRva - registration.instructionRva - instruction.size());
                std::memcpy(instruction.data() + 3, &relative, sizeof(relative));
                admit(registration.instructionRva, instruction.data(), instruction.size(), PAGE_EXECUTE_READ);
                admit(registration.nameRva, registration.name, std::strlen(registration.name) + 1, PAGE_READONLY);
            }
            std::array<unsigned char, NativeCheatProfile::NativeFlagCount> flags;
            flags.fill(0xa5);
            for (const auto& member : NativeCheatProfile::Members)
            {
                const std::uint64_t absent = 0;
                admit(NativeCheatProfile::DispatchTableRva + member.nativeIndex * sizeof(absent), &absent, sizeof(absent), PAGE_READONLY);
                flags[member.nativeIndex] = 0;
            }
            admit(NativeCheatProfile::FlagsRva, flags.data(), flags.size(), PAGE_READWRITE);
        }
        void AdmitWeatherPins()
        {
            for (const auto& pin : NativeWeatherTransitionProfile::Code)
                Patch(pin.rva, pin.bytes.data(), pin.size);
            SeedWeatherFields();
        }
        void SeedWeatherFields()
        {
            constexpr std::array<std::uint32_t, 5> values{1, 2, 3, 1000, 0x3f000000};
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                const auto& field = NativeWeatherTransitionProfile::Fields[index];
                DWORD       prior = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + field.rva), field.size, PAGE_READWRITE, &prior) != FALSE,
                      "admit only the owned weather transition field page");
                std::memcpy(reinterpret_cast<void*>(image + field.rva), &values[index], field.size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + field.rva), field.size, PAGE_READWRITE, &ignored) != FALSE,
                      "weather fixture uses readable writable non-executable image data");
            }
        }
        void AdmitWeatherScalarPins()
        {
            // Preserve the real section/page policies for scalar producers and
            // their independent named consumers. The sparse owned image needs
            // only these bounded pages, never a second copy of the game.
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
            {
                DWORD prior = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &prior) != FALSE,
                      "initialize only bounded owned scalar profile image pages");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), size),
                      "restore scalar proof code RX, named metadata RO and native float data RW");
            };
            for (const auto& pin : NativeWeatherScalarProfile::Code)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            // The later override-aware qualifier checks complete native store
            // instructions even when a prior producer pin covered only part.
            // Stock getter fixtures must supply that same full real proof.
            for (const auto& writer : NativeWeatherOverrideProfile::Writers)
                admit(writer.rva, writer.original.data(), writer.size, PAGE_EXECUTE_READ);
            for (const auto& window : NativeWeatherOverrideAtomicProfile::Windows)
                admit(window.windowRva, window.stock.data(), window.windowSize, PAGE_EXECUTE_READ);
            for (const auto& name : NativeWeatherScalarProfile::Names)
            {
                admit(name.nameRva, name.name, std::strlen(name.name) + 1, PAGE_READONLY);
                const std::uint64_t pointer = image + name.nameRva;
                admit(name.recordRva, &pointer, sizeof(pointer), PAGE_READONLY);
                admit(name.recordRva + (name.propertyOffset ? 36 : 8), &name.value, sizeof(name.value), PAGE_READONLY);
            }
            const wchar_t intensity[] = L"RainbowIntensity", mesh[] = L"RainbowMesh";
            admit(NativeWeatherScalarProfile::RainbowIntensityNameRva, intensity, sizeof(intensity), PAGE_READONLY);
            admit(NativeWeatherScalarProfile::RainbowMeshNameRva, mesh, sizeof(mesh), PAGE_READONLY);
            constexpr std::array<std::uint32_t, 8> values{0x80000000, 0xbf000000, 0x3fc00000, 0x3e800000, 0x40000000, 0x3f000000, 0xbf800000, 0x3f800000};
            for (std::size_t index = 0; index < values.size(); ++index)
                admit(NativeWeatherScalarProfile::Fields[index], &values[index], sizeof(values[index]), PAGE_READWRITE);
        }
        void AdmitRainPins()
        {
            namespace Rain = NativeWeatherRainProfile;
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
            {
                DWORD prior = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &prior) != FALSE,
                      "initialize only bounded Rain proof pages in the owned SEC_IMAGE fixture");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), size),
                      "retain real RX code, RO unwind/slots and the current resolved copy target without executing native serializer instructions");
            };
            for (const auto& pin : Rain::Code)
                admit(pin.rva, pin.bytes, pin.size, pin.rva == Rain::SerializerUnwindRva ? PAGE_READONLY : PAGE_EXECUTE_READ);
            for (const auto& slot : Rain::Slots)
            {
                const auto target = image + slot.target;
                admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
            }
            const auto library = GetModuleHandleW(L"VCRUNTIME140.dll");
            const auto copy = library ? GetProcAddress(library, "memcpy") : nullptr;
            Check(copy != nullptr, "the ordinary fixture uses its actual loaded CRT copy import for native IAT qualification");
            const auto target = reinterpret_cast<std::uintptr_t>(copy);
            admit(Rain::CopyIatRva, &target, sizeof(target), PAGE_READONLY);
        }
        void AdmitSystemStatePins()
        {
            for (const auto& pin : NativeGameStateProfile::pins)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + pin.rva), pin.size, PAGE_READWRITE, &previous) != FALSE,
                      "populate only owned native state-dispatch fixture pins");
                std::memcpy(reinterpret_cast<void*>(image + pin.rva), pin.bytes, pin.size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + pin.rva), pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + pin.rva), pin.size),
                      "native state pins use actual RX or immutable RO image pages");
            }
        }
        void AdmitStatsPins()
        {
            namespace Stats = NativeStatsProfile;
            namespace Writes = NativeStatsWriteProfile;
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
            {
                DWORD prior = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &prior) != FALSE,
                      "populate only bounded native stats proof pages in the owned image");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), size),
                      "restore actual stats instruction RX, mission-name RO and bank/player-global RW pages");
            };
            for (const auto& pin : Stats::Code)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : Writes::Code)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            admit(Writes::MissionGuardRva, Writes::MissionGuard.data(), Writes::MissionGuard.size(), PAGE_READONLY);
            const std::array<float, Stats::FloatCount>                 floats{};
            const std::array<std::int32_t, Stats::IntegerStorageCount> integers{};
            admit(Stats::FloatBankRva, floats.data(), sizeof(floats), PAGE_READWRITE);
            admit(Stats::IntegerBankRva, integers.data(), sizeof(integers), PAGE_READWRITE);
            const auto store = [](unsigned char* bytes, std::size_t offset, const auto& value) { std::memcpy(bytes + offset, &value, sizeof(value)); };
            statsPed.fill(0);
            statsData.fill(0);
            statsScript.fill(0);
            store(statsPed.data(), 0, image + Writes::PlayerVtableRva);
            store(statsPed.data(), 0x5f0, image + Writes::PlayerInfoRva);
            store(statsPed.data(), 0x76c, 100.0f);
            store(statsPed.data(), 0x7e0, std::int32_t{0});
            store(statsPed.data(), 0xa10, static_cast<unsigned char>(0));
            const std::array<unsigned char, 7> scriptName{'f', 'r', 'e', 'e', 'r', 'o', 0};
            std::memcpy(statsScript.data() + 0x10, scriptName.data(), scriptName.size());
            const auto          ped = reinterpret_cast<std::uintptr_t>(statsPed.data());
            const auto          data = reinterpret_cast<std::uintptr_t>(statsData.data());
            const auto          script = reinterpret_cast<std::uintptr_t>(statsScript.data());
            const auto          control = image + Writes::PlayerControlRva;
            const unsigned char selector = 0;
            admit(Writes::PlayerRootRva, &ped, sizeof(ped), PAGE_READWRITE);
            admit(Writes::PlayerInfoRva, &data, sizeof(data), PAGE_READWRITE);
            admit(Writes::PlayerIndexRva, &selector, sizeof(selector), PAGE_READWRITE);
            admit(Writes::ScriptRootRva, &script, sizeof(script), PAGE_READWRITE);
            admit(Writes::PlayerVtableRva + 0x50, &control, sizeof(control), PAGE_READONLY);
        }
        void AdmitWeaponPins()
        {
            // This bank uses its own native layout/loader qualification. Keep
            // its animation-count cell in a narrow writable image section and
            // every immutable selector/loader literal under the real RO gate.
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
            {
                DWORD prior = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &prior) != FALSE,
                      "initialize only bounded weapon bank proof pages in the owned image");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), size),
                      "restore independent weapon code RX, selector literals RO and bank/count data RW");
            };
            for (const auto& pin : NativeWeaponInfoProfile::Code)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            for (const auto& literal : NativeWeaponInfoProfile::Literals)
                admit(literal.rva, literal.bytes.data(), literal.size, PAGE_READONLY);
            std::array<NativeWeaponInfoValues, NativeWeaponInfoProfile::RowCount> rows{};
            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                auto& row = rows[index];
                row.fireType = 1;
                row.targetRange = static_cast<float>(index + 1);
                row.weaponRange = static_cast<float>(index + 2);
                row.model = static_cast<std::int32_t>(100 + index);
                row.model2 = -1;
                row.slot = static_cast<std::int32_t>(index % 12);
                row.flags = static_cast<std::int32_t>(0x40008000);
                row.animGroup = 1;
                row.damage = 23;
                row.clipAmmo = 17;
                row.skill = 0xaabbcc01;
                row.fireOffset[2] = 9.0f;
                row.aimOffset = 7;
                row.defaultCombo = 3;
                row.combosAvailable = 4;
            }
            admit(NativeWeaponInfoProfile::BankRva, rows.data(), sizeof(rows), PAGE_READWRITE);
            const std::int32_t animations = 37;
            admit(NativeWeaponInfoWriteProfile::AnimationCountRva, &animations, sizeof(animations), PAGE_READWRITE);
        }
        void AdmitWeaponVectorPins()
        {
            for (const auto& pin : NativeWeaponInfoVectorProfile::Code)
                Patch(pin.rva, pin.bytes.data(), pin.size);
        }
        void AdmitApplicationPins()
        {
            namespace Frame = NativeApplicationFrameProfile;
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &previous) != FALSE,
                      "initialize only bounded owned SEC_IMAGE application fixture pages");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), size),
                      "restore synthetic application code RX, vtable RO and state/root RW protections");
            };
            for (const auto& pin : Frame::pins)
                admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto entry = image + Frame::entryRva;
            admit(Frame::slotRva, &entry, sizeof(entry), PAGE_READONLY);
            const std::uint32_t state = 7;
            admit(Frame::stateRva, &state, sizeof(state), PAGE_READWRITE);
            admit(Frame::rootRva, &frameReceiver, sizeof(frameReceiver), PAGE_READWRITE);

            // Retain every entry/tail pin and exercise the real Detours
            // trampoline, but return before any native engine work. Synthetic
            // callers deliberately have no invented exception-unwind metadata.
            const std::array<unsigned char, 10> originalReturn{0x48, 0x81, 0xc4, 0xb8, 0, 0, 0, 0x5d, 0x5b, 0xc3};
            Patch(Frame::tailRva + Frame::tail.size(), originalReturn.data(), originalReturn.size());
            const std::array<unsigned char, 3> benignReturn{0x33, 0xc0, 0xc3};
            constexpr std::uintptr_t           benignRva = TextRva + 0x1600;
            Patch(benignRva, benignReturn.data(), benignReturn.size());
            const auto benign = image + benignRva;
            admit(Frame::vtableRva + 0x300, &benign, sizeof(benign), PAGE_READONLY);
            admit(Frame::vtableRva + 0x388, &benign, sizeof(benign), PAGE_READONLY);
            // The pinned caller's suffix is kept intact. Its ordinary helper
            // targets become owned leaf returns, so the exact normal call can
            // run without pretending to create a UE world or GUI.
            for (const std::uintptr_t rva : {0xc227b0u, 0xb65960u, 0x3004080u})
                admit(rva, benignReturn.data(), benignReturn.size(), PAGE_EXECUTE_READ);
            std::vector<unsigned char> normal{0x53, 0x48, 0x83, 0xec, 0x50, 0x0f, 0x29, 0x74, 0x24, 0x40, 0x0f, 0x28, 0xf1, 0x48, 0xbb};
            const auto                 append = [](std::vector<unsigned char>& code, std::uintptr_t value)
            {
                const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
                code.insert(code.end(), bytes, bytes + sizeof(value));
            };
            append(normal, reinterpret_cast<std::uintptr_t>(frameCaller.data()));
            normal.insert(normal.end(), {0x48, 0x8b, 0x01, 0x49, 0xba});
            append(normal, image + Frame::normalReturnRva - 6);
            normal.insert(normal.end(), {0x41, 0xff, 0xe2});
            Patch(TextRva + 0x1400, normal.data(), normal.size());
            invokeFrame = reinterpret_cast<Frame::FrameFunction>(image + TextRva + 0x1400);
            std::vector<unsigned char> loading{0x48, 0x83, 0xec, 0x28, 0x48, 0x8b, 0x01, 0x49, 0xba};
            append(loading, image + Frame::excludedLoadingReturnRva - 6);
            loading.insert(loading.end(), {0x41, 0xff, 0xe2});
            Patch(TextRva + 0x1500, loading.data(), loading.size());
            const std::array<unsigned char, 11> loadingCall{0xff, 0x90, 0xb0, 0x03, 0, 0, 0x48, 0x83, 0xc4, 0x28, 0xc3};
            Patch(Frame::excludedLoadingReturnRva - 6, loadingCall.data(), loadingCall.size());
            invokeLoadingFrame = reinterpret_cast<Frame::FrameFunction>(image + TextRva + 0x1500);
        }
        void SetFrameReceiver(void* receiver)
        {
            frameReceiver = reinterpret_cast<std::uintptr_t>(receiver);
            Put(NativeApplicationFrameProfile::rootRva, frameReceiver);
            std::memcpy(frameCaller.data() + 0x2c0, &frameReceiver, sizeof(frameReceiver));
        }
        void AdmitUiClassMapPins()
        {
            for (const auto& pin : NativeUiClassMapProfile::pins)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + pin.rva), pin.size, PAGE_READWRITE, &previous) != FALSE,
                      "initialize only owned dictionary proof pages");
                std::memcpy(reinterpret_cast<void*>(image + pin.rva), pin.bytes, pin.size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + pin.rva), pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + pin.rva), pin.size),
                      "restore dictionary proof code RX and immutable tables/literals RO");
            }
        }
        void SeedUiClassMap()
        {
            namespace Ui = NativeUiClassMapProfile;
            auto* map = reinterpret_cast<unsigned char*>(frameOwner.data()) + Ui::ownerOffset;
            std::memset(map, 0, Ui::headerBytes);
            uiEntries.fill(0);
            const auto put = [](unsigned char* target, std::size_t offset, const auto& value) { std::memcpy(target + offset, &value, sizeof(value)); };
            put(map, Ui::slotsOffset, reinterpret_cast<std::uintptr_t>(uiEntries.data()));
            put(map, Ui::slotCountOffset, std::int32_t{2});
            put(map, Ui::inlineHashOffset, std::int32_t{0});
            put(map, Ui::hashSizeOffset, std::int32_t{1});
            put(uiEntries.data(), Ui::keyDataOffset, reinterpret_cast<std::uintptr_t>(uiLoading.data()));
            put(uiEntries.data(), Ui::keyCountOffset, static_cast<std::int32_t>(uiLoading.size()));
            put(uiEntries.data(), Ui::valueOffset, std::uintptr_t{0x12345678});
            put(uiEntries.data(), Ui::nextOffset, std::int32_t{1});
            auto* fade = uiEntries.data() + Ui::entryBytes;
            put(fade, Ui::keyDataOffset, reinterpret_cast<std::uintptr_t>(uiSimpleFade.data()));
            put(fade, Ui::keyCountOffset, static_cast<std::int32_t>(uiSimpleFade.size()));
            put(fade, Ui::valueOffset, std::uintptr_t{0x87654321});
            put(fade, Ui::nextOffset, std::int32_t{-1});
        }
        void AdmitStartupPins()
        {
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t count, DWORD protection)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), count, PAGE_READWRITE, &previous) != FALSE,
                      "populate only the owned synthetic startup profile pages");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, count);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), count, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), count),
                      "restore synthetic startup code RX and immutable pins/getter slot RO");
            };
            for (const auto& pin : NativeStartupOwnerProfile::pins)
                admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeStartupWorldProfile::pins)
                admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeStartupProvidersProfile::pins)
                admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            const auto getter = image + NativeStartupWorldProfile::getterRva;
            admit(NativeStartupWorldProfile::ownerGetWorldSlotRva, &getter, sizeof(getter), PAGE_READONLY);
            const std::uintptr_t absent = 0;
            admit(NativeStartupWorldProfile::objectChunksRva, &absent, sizeof(absent), PAGE_READWRITE);
        }
        void Patch(std::uintptr_t rva, const void* bytes, std::size_t length)
        {
            DWORD previous = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, PAGE_EXECUTE_READWRITE, &previous) != FALSE,
                  "temporarily admit writes to this owned image fixture range");
            std::memcpy(reinterpret_cast<void*>(image + rva), bytes, length);
            Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, previous, &ignored) != FALSE &&
                      ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), length),
                  "restore actual fixture protection and instruction-cache coherence");
        }
        template <typename T>
        void Put(std::uintptr_t rva, const T& value)
        {
            SIZE_T written = 0;
            Check(::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), &value, sizeof(value), &written) && written == sizeof(value),
                  "write only a task-owned synthetic native data field");
        }
        void Seed()
        {
            Put(0x521c917, std::uint8_t{12});
            Put(0x521c923, std::uint8_t{34});
            Put(0x522bcf0, std::uint16_t{27});
            Put(0x523a458, std::uint32_t{10000});
            Put(0x522bcf8, std::uint32_t{9900});
            Put(0x5227bac, std::uint32_t{1000});
            Put(NativeGameTimingProfile::timeScaleRva, 1.0f);
            Put(NativeGamePauseProfile::userPauseRva, std::uint8_t{0});
        }
        template <typename T>
        T Get(std::uintptr_t rva) const
        {
            T      value{};
            SIZE_T copied = 0;
            Check(ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(image + rva), &value, sizeof(value), &copied) && copied == sizeof(value),
                  "read an actual owned native image field for independent verification");
            return value;
        }
    };

    // This receiver tests the real SDK implementation without pretending to be
    // a CGame or shipping native binding. Native memory/hook behavior belongs to
    // the separate production-runtime fixture below, not these receiver values.
    class AccessFixture final : public NativeClockAccess
    {
    public:
        std::atomic<DWORD>            owner{0};
        ClockValues                   values{12, 34, 56, 0x12345678, 0x12345670, 1000};
        ClockResult                   readResult = ClockResult::Ok, setResult = ClockResult::Ok;
        ClockResult                   freezeResult = ClockResult::Ok, frozenResult = ClockResult::Ok;
        bool                          frozen = false;
        std::atomic<unsigned>         reads{0}, sets{0}, freezes{0};
        mutable std::atomic<unsigned> frozenReads{0};
        std::uint8_t                  lastHour = 0, lastMinute = 0;

        ClockResult Owner() const noexcept
        {
            const auto thread = owner.load();
            return !thread ? ClockResult::NotInitialized : thread != GetCurrentThreadId() ? ClockResult::WrongThread : ClockResult::Ok;
        }
        ClockResult Read(ClockValues& output) noexcept override
        {
            ++reads;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            output = values;
            // Deliberately publish into the temporary even on a failed backend
            // read. The actual CClockDE must preserve its caller's two outputs.
            return readResult;
        }
        ClockResult Set(std::uint8_t hour, std::uint8_t minute) noexcept override
        {
            ++sets;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            if (setResult == ClockResult::Ok)
            {
                lastHour = hour;
                lastMinute = minute;
                values.hour = hour;
                values.minute = minute;
            }
            return setResult;
        }
        ClockResult Freeze(bool value) noexcept override
        {
            ++freezes;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            if (freezeResult == ClockResult::Ok)
                frozen = value;
            return freezeResult;
        }
        ClockResult Frozen(bool& output) const noexcept override
        {
            ++frozenReads;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            output = frozen;
            return frozenResult;
        }
    };

    void ClockInterfaceContract()
    {
        static_assert(std::is_base_of_v<CClock, CClockDE> && !std::is_abstract_v<CClockDE>);
        static_assert(!std::has_virtual_destructor_v<CClock>);
        AccessFixture access;
        CClockDE      clock(access);
        CClock&       sdk = clock;
        const CClock& constSdk = clock;
        BYTE          hour = 0xa5, minute = 0x5a;
        Check(clock.LastResult() == ClockResult::NotInitialized, "a new concrete MTA clock contains no fabricated native time");
        sdk.Get(&hour, &minute);
        Check(hour == 0xa5 && minute == 0x5a && clock.LastResult() == ClockResult::NotInitialized,
              "unbound SDK Get preserves both caller outputs and records the backend refusal");
        Check(!sdk.SetTimeFrozen(true) && clock.LastResult() == ClockResult::NotInitialized, "unbound SDK freeze never reports success");
        access.owner = GetCurrentThreadId();
        sdk.Get(&hour, &minute);
        Check(hour == 12 && minute == 34 && clock.LastResult() == ClockResult::Ok,
              "the existing CClock virtual interface reads a coherent successful native-access result");
        BYTE alias = 0;
        sdk.Get(&alias, &alias);
        Check(alias == 12, "aliased Get outputs retain the existing classic minute-first publication order");
        const auto readCalls = access.reads.load();
        Check(clock.TryGet(nullptr, &minute) == ClockResult::InvalidArgument && clock.TryGet(&hour, nullptr) == ClockResult::InvalidArgument &&
                  clock.TryGet(nullptr, nullptr) == ClockResult::InvalidArgument && hour == 12 && minute == 34 && access.reads.load() == readCalls,
              "null output arguments refuse before native access without changing a valid output");
        for (const auto value : {std::array<BYTE, 2>{0, 0}, std::array<BYTE, 2>{23, 59}, std::array<BYTE, 2>{23, 60}})
        {
            sdk.Set(value[0], value[1]);
            Check(clock.LastResult() == ClockResult::Ok && access.lastHour == value[0] && access.lastMinute == value[1],
                  "actual SDK Set delegates canonical times and classic minute 60 without normalization");
        }
        const auto setCalls = access.sets.load();
        for (const auto value : {std::array<BYTE, 2>{24, 0}, std::array<BYTE, 2>{255, 0}, std::array<BYTE, 2>{0, 61}, std::array<BYTE, 2>{0, 255}})
        {
            sdk.Set(value[0], value[1]);
            Check(clock.LastResult() == ClockResult::InvalidArgument && access.sets.load() == setCalls,
                  "out-of-range legacy SDK times cannot reach native writes");
        }
        access.readResult = ClockResult::ReadFailure;
        hour = 0xa5;
        minute = 0x5a;
        Check(clock.TryGet(&hour, &minute) == ClockResult::ReadFailure && hour == 0xa5 && minute == 0x5a,
              "a failed backend read cannot partially publish or return an earlier cached time");
        access.readResult = ClockResult::Ok;
        access.values.hour = 6;
        access.values.minute = 7;
        sdk.Get(&hour, &minute);
        Check(hour == 6 && minute == 7, "the next SDK Get reads the new backend values rather than a retained prior read");
        access.setResult = ClockResult::WriteFailure;
        sdk.Set(15, 16);
        Check(clock.LastResult() == ClockResult::WriteFailure && access.values.hour == 6 && access.values.minute == 7,
              "the void SDK Set retains a separately readable native write failure");
        access.setResult = ClockResult::Ok;
        Check(sdk.SetTimeFrozen(true) && constSdk.IsTimeFrozen() && clock.LastResult() == ClockResult::Ok,
              "existing SDK freeze and const query delegate to the real checked backend boundary");
        Check(sdk.SetTimeFrozen(true) && access.frozen, "repeated SDK freeze remains an idempotent backend request");
        bool observed = false;
        access.frozenResult = ClockResult::ReadFailure;
        Check(clock.TryIsTimeFrozen(observed) == ClockResult::ReadFailure && !observed, "failed frozen-state reads preserve the caller's existing false value");
        observed = true;
        Check(clock.TryIsTimeFrozen(observed) == ClockResult::ReadFailure && observed, "failed frozen-state reads also preserve an existing true value");
        Check(!constSdk.IsTimeFrozen() && clock.LastResult() == ClockResult::ReadFailure,
              "the legacy bool query records errors and never reuses a cached successful frozen value");
        access.frozenResult = ClockResult::Ok;
        access.freezeResult = ClockResult::PatchFailure;
        Check(!sdk.ResetTimeFrozen() && clock.LastResult() == ClockResult::PatchFailure && access.frozen,
              "a failed native unfreeze cannot falsely complete the SDK reset");
        access.freezeResult = ClockResult::Ok;
        Check(sdk.ResetTimeFrozen() && !constSdk.IsTimeFrozen() && sdk.ResetTimeFrozen(),
              "SDK ResetTimeFrozen means idempotent unfreeze rather than restoring an earlier cached state");
        bool        foreignRejected = false;
        std::thread foreign(
            [&]
            {
                BYTE otherHour = 0xab, otherMinute = 0xcd;
                bool otherFrozen = true;
                foreignRejected = clock.TryGet(&otherHour, &otherMinute) == ClockResult::WrongThread && otherHour == 0xab && otherMinute == 0xcd &&
                                  clock.TrySet(1, 2) == ClockResult::WrongThread && clock.TrySetTimeFrozen(true) == ClockResult::WrongThread &&
                                  clock.TryIsTimeFrozen(otherFrozen) == ClockResult::WrongThread && otherFrozen &&
                                  clock.TryResetTimeFrozen() == ClockResult::WrongThread;
            });
        foreign.join();
        Check(foreignRejected && !access.frozen && access.values.hour == 6 && access.values.minute == 7,
              "every checked SDK operation preserves the backend's adopted engine-thread policy");
        Check(clock.LastResult() == ClockResult::WrongThread, "cross-thread diagnostic observation is atomic without sharing cached native fields");
        Check(clock.TrySet(8, 9) == ClockResult::Ok && clock.TryGet(&hour, &minute) == ClockResult::Ok && hour == 8 && minute == 9,
              "an unauthorized call does not revoke a later valid engine-owner operation");
    }
    ClockServiceStateV1 State(NativeClockService& service)
    {
        ClockServiceStateV1 state;
        Check(service.Read(state), "read the production service's coherent published state");
        return state;
    }

    void BeginOwned(NativeClockService& service, ClockImageFixture& fixture, bool adoptNow = true, bool attachNow = true, bool timingMode = false,
                    GameTimingHandler handler = nullptr, void* context = nullptr)
    {
        ClockServiceStateV1 rejected;
        const bool          shippingAdmitted = timingMode ? service.BeginGameTiming({}, rejected, handler, context) : service.Begin({}, rejected);
        Check(!shippingAdmitted && rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked && !rejected.modulePinned &&
                  rejected.generation && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "shipping Begin rejects this actual test executable before binding, hooks or module pinning");
        fixture.Seed();
        NativeServiceAdmission::Token claim;
        Check(NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Clock, claim),
              "the owned native image fixture holds the actual shared clock admission throughout setup and cleanup");
        runtime.admissionSerial.store(claim.serial);
        runtime.admissionGeneration.store(runtime.generation.load());
        Check(!ReleaseAdmission(runtime.generation.load() - 1) && NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "a stale native cleanup generation cannot release the current fixture's real process-wide clock claim");
        // Whitebox admission replaces only the test-owned runtime's image after
        // proving that public shipping admission refused it. All subsequent
        // data/code validation, Detours and callback ownership remain real.
        runtime.image = fixture.image;
        runtime.failure.store(ClockResult::Ok);
        runtime.nativeTouched.store(false);
        runtime.stopping.store(false);
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.started = GetTickCount64();
        runtime.refreshed.store(runtime.started);
        runtime.originalContinuation = reinterpret_cast<ContinuationFunction>(fixture.image + NativeClockProfile::hookEntryRva);
        runtime.originalTimer = timingMode ? reinterpret_cast<ContinuationFunction>(fixture.image + NativeGamePauseProfile::entryRva) : nullptr;
        runtime.timingHandler = handler;
        runtime.timingContext = context;
        Check(CheckBinding(true), "exact owned MEM_IMAGE code, page and section pins validate before any native hook transaction");
        if (!attachNow)
            return;
        Check(ChangeHooks(true), "actual Detours attaches after validated native ownership");
        runtime.hooked.store(true);
        if (!adoptNow)
            return;
        fixture.invoke();
        const auto state = State(service);
        Check(state.engineThread == GetCurrentThreadId() && state.producerThread == GetCurrentThreadId() && state.ready && state.active && !state.restored &&
                  state.qualifiedCallbacks == 1 && state.callbacks == 1 && state.hour == 12 && state.minute == 34 && state.seconds == 27,
              "the actual qualified native return address adopts the owner and captures fresh engine clock fields");
    }

    ClockCommandV1 Command(const ClockServiceStateV1& state, std::uint64_t sequence, ClockOperation operation = ClockOperation::Read)
    {
        ClockCommandV1 command;
        command.generation = state.generation;
        command.sequence = sequence;
        command.operation = operation;
        return command;
    }

    ClockServiceStateV1 Execute(NativeClockService& service, ClockImageFixture& fixture, ClockOperation operation, std::uint64_t sequence = 1)
    {
        const auto before = State(service);
        Check(service.Submit(Command(before, sequence, operation)), "the producer queues one generation-bound production clock command");
        fixture.invoke();
        const auto after = State(service);
        Check(after.lastCompletedSequence == sequence && !after.pendingSequence && !after.inFlight,
              "actual callback completes the queued sequence and releases callback lifetime accounting");
        return after;
    }

    void FinishOwned(NativeClockService& service, ClockImageFixture& fixture)
    {
        NativeServiceAdmission::Token refused;
        Check(NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) &&
                  !NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Input, refused) &&
                  !NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Render, refused),
              "native setup or cleanup debt keeps the actual shared clock claim exclusive against input and renderer startup");
        if (!runtime.stopping.load())
            Check(!service.End() && service.IsRetained(), "End retains the real hook until native-owner restoration runs");
        fixture.invoke();
        Check(service.End() && !service.IsRetained(), "native restoration acknowledges before real Detours detach and owner release");
        const auto state = State(service);
        Check(state.restored && !state.ready && !state.bound && !state.hooked && !state.patchInstalled && !state.patchRecoveryPending &&
                  !state.dataRecoveryPending && !runtime.timingDirty.load() && !runtime.timingRecovery.load() && !runtime.timingCaptured.load() &&
                  !runtime.timingGeneration.load() && !state.retainedPeerThreads && !state.protectionRestorePending && !state.inFlight,
              "completed cleanup has no native data, branch, protection, peer-handle or callback debt");
        Check(CheckBinding(true), "exact original clock and continuation bytes survive complete cleanup");
        Check(!NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && !runtime.admissionSerial.load() &&
                  !runtime.admissionGeneration.load(),
              "only confirmed native cleanup releases its exact shared service admission generation");
    }

    void NativeContractValidation()
    {
        ClockServiceRequestV1 request;
        Check(NativeClockService::ValidateRequest(request), "default bounded native clock request is valid");
        for (unsigned field = 0; field != 6; ++field)
        {
            auto invalid = request;
            if (field == 0)
                --invalid.size;
            if (field == 1)
                ++invalid.version;
            if (field == 2)
                invalid.maximumDurationMs = 0;
            if (field == 3)
                invalid.maximumDurationMs = ClockServiceMaximumDurationMsV1 + 1;
            if (field == 4)
                invalid.reserved[0] = 1;
            if (field == 5)
                invalid.reserved[2] = 1;
            Check(!NativeClockService::ValidateRequest(invalid), "invalid request shape, lease duration or reserved data refuses admission");
        }
        ClockCommandV1 command;
        command.generation = command.sequence = 1;
        Check(NativeClockService::ValidateCommand(command), "canonical native read command is valid");
        for (unsigned field = 0; field != 9; ++field)
        {
            auto invalid = command;
            if (field == 0)
                --invalid.size;
            if (field == 1)
                ++invalid.version;
            if (field == 2)
                invalid.generation = 0;
            if (field == 3)
                invalid.sequence = 0;
            if (field == 4)
                invalid.reserved = 1;
            if (field == 5)
                invalid.operation = static_cast<ClockOperation>(5);
            if (field == 6)
                invalid.hour = 1;
            if (field == 7)
            {
                invalid.operation = ClockOperation::Set;
                invalid.hour = 24;
            }
            if (field == 8)
            {
                invalid.operation = ClockOperation::Set;
                invalid.minute = 61;
            }
            Check(!NativeClockService::ValidateCommand(invalid), "invalid command identity, operation or time cannot enter the native mailbox");
        }
    }

    void NativeServiceAdmissionContract()
    {
        namespace Admission = NativeServiceAdmission;
        using Admission::Category;
        using Admission::Token;
        Token input, render, clock, refused;
        Check(!Admission::IsClaimed(Category::Input) && !Admission::IsClaimed(Category::Render) && !Admission::IsClaimed(Category::Clock),
              "real shared native service admission starts without a retained service owner");
        refused = {Category::Render, 987};
        Check(!Admission::TryAcquire(Category::None, refused) && refused.category == Category::Render && refused.serial == 987 && !Admission::Release(refused),
              "invalid admission preserves output and an unowned token cannot release any service category");
        Check(Admission::TryAcquire(Category::Input, input) && Admission::TryAcquire(Category::Render, render) && input.serial != render.serial,
              "the real shared helper admits frontend input and rendering together with independent ownership serials");
        refused = {};
        Check(!Admission::TryAcquire(Category::Clock, clock) && !Admission::TryAcquire(Category::Input, refused) && !refused && !Admission::Release(refused) &&
                  Admission::IsClaimed(Category::Input) && Admission::IsClaimed(Category::Render),
              "clock and duplicate input startup refuse without granting a token capable of releasing the active frontend owners");
        Check(!Admission::Release({Category::Render, input.serial}) && Admission::IsClaimed(Category::Render),
              "a token from one category cannot release another category's independently owned admission");
        Check(Admission::Release(render) && !Admission::TryAcquire(Category::Clock, clock),
              "releasing rendering alone cannot clear input's clock exclusion during retained input cleanup");
        Check(Admission::Release(input) && Admission::TryAcquire(Category::Clock, clock),
              "clock startup becomes available only after every conflicting frontend claim is explicitly released");
        Check(!Admission::Release(input) && !Admission::Release(render) && Admission::IsClaimed(Category::Clock) &&
                  !Admission::TryAcquire(Category::Input, refused) && !Admission::TryAcquire(Category::Render, refused),
              "stale frontend cleanup cannot release a newer exclusive clock owner or admit another service");
        // No success/failure guess releases a claim: callers keep the token
        // throughout their explicit cleanup debt and release it only on proof.
        Check(Admission::IsClaimed(Category::Clock) && !Admission::TryAcquire(Category::Clock, refused),
              "a retained clock claim survives failed cleanup and duplicate startup until its exact owner explicitly releases it");
        const auto oldClock = clock;
        Check(Admission::Release(clock) && Admission::TryAcquire(Category::Clock, clock) && clock.serial != oldClock.serial && !Admission::Release(oldClock) &&
                  Admission::IsClaimed(Category::Clock),
              "a stale same-category token cannot release a later clock generation after admission reuse");
        Check(Admission::Release(clock), "the exact current clock token releases its own admission once cleanup is confirmed");
        Check(Admission::TryAcquire(Category::Input, input), "hold the real input service category before an actual native clock Begin");
        NativeClockService  blockedClock;
        ClockServiceStateV1 blockedState;
        Check(!blockedClock.Begin({}, blockedState) && !runtime.hooked.load() && !Admission::IsClaimed(Category::Clock) &&
                  Admission::IsClaimed(Category::Input) && blockedClock.End() && Admission::IsClaimed(Category::Input),
              "actual native clock admission refuses an existing input owner and its failed-owner End cannot release that foreign claim");
        Check(Admission::Release(input), "the actual input token remains releasable only by its original service owner");
        for (unsigned round = 0; round != 4; ++round)
        {
            std::atomic<unsigned> ready{0}, completed{0};
            std::atomic<bool>     start{false}, release{false}, inputWon{false}, clockWon{false}, inputReleased{false}, clockReleased{false};
            const auto            compete = [&](Category category, std::atomic<bool>& won, std::atomic<bool>& released)
            {
                Token token;
                ready.fetch_add(1);
                while (!start.load())
                    SwitchToThread();
                const bool acquired = Admission::TryAcquire(category, token);
                won.store(acquired);
                completed.fetch_add(1);
                while (!release.load())
                    SwitchToThread();
                released.store(acquired ? Admission::Release(token) : !Admission::Release(token));
            };
            std::thread inputThread([&] { compete(Category::Input, inputWon, inputReleased); });
            std::thread clockThread([&] { compete(Category::Clock, clockWon, clockReleased); });
            while (ready.load() != 2)
                SwitchToThread();
            start.store(true);
            while (completed.load() != 2)
                SwitchToThread();
            Check(inputWon.load() != clockWon.load() && Admission::IsClaimed(Category::Input) == inputWon.load() &&
                      Admission::IsClaimed(Category::Clock) == clockWon.load(),
                  "synchronized competing input and clock startup admits exactly one owner while its token is retained");
            release.store(true);
            inputThread.join();
            clockThread.join();
            Check(inputReleased.load() && clockReleased.load() && !Admission::IsClaimed(Category::Input) && !Admission::IsClaimed(Category::Clock),
                  "only the racing winner releases its actual claim while a failed contender cannot affect ownership");
        }
    }

    void NativeReadSetAndRestoration(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        auto state = State(service);
        BYTE hour = 0xaa, minute = 0xbb;
        Check(runtime.clock.TryGet(&hour, &minute) == ClockResult::WrongThread && hour == 0xaa && minute == 0xbb &&
                  runtime.clock.TrySet(1, 2) == ClockResult::WrongThread && ChangeFreezePatch(false) == ClockResult::WrongThread,
              "even the producer on the adopted OS thread cannot access native SDK values outside its callback scope");
        ClockServiceStateV1 unchanged;
        unchanged.hour = 99;
        unchanged.lastCompletedSequence = 123;
        unchanged.lastCommandResult = ClockResult::OwnerConflict;
        runtime.publishedClock.version.fetch_add(1);
        const bool busyRead = service.Read(unchanged);
        runtime.publishedClock.version.fetch_add(1);
        Check(!busyRead && unchanged.hour == 99 && unchanged.lastCompletedSequence == 123 && unchanged.lastCommandResult == ClockResult::OwnerConflict,
              "an in-progress combined clock/command publication refuses boundedly without changing the caller's values or acknowledgement");
        auto command = Command(state, 1);
        Check(service.Submit(command) && !service.Submit(Command(state, 2)), "an occupied production mailbox cannot silently overwrite a pending command");
        fixture.invoke();
        state = State(service);
        Check(state.lastCommandResult == ClockResult::Ok && state.reads == 1 && state.lastCompletedSequence == 1 && !service.Submit(command),
              "actual SDK Read completes once and rejects replayed sequence numbers");
        command = Command(state, 2, ClockOperation::Set);
        command.hour = 23;
        command.minute = 60;
        std::vector<unsigned char> before(ClockImageFixture::DataSize);
        std::memcpy(before.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), before.size());
        Check(service.Submit(command), "classic-compatible minute 60 is accepted without DE script-setter normalization");
        fixture.invoke();
        state = State(service);
        Check(state.lastCommandResult == ClockResult::Ok && state.sets == 1 && state.hour == 23 && state.minute == 60 && state.seconds == 27 &&
                  state.nativeTimer == 10000 && state.lastMinuteChange == 10000,
              "the actual SDK Set changes displayed time and minute timestamp while preserving seconds and native timer");
        bool onlyClockFields = true;
        for (std::size_t index = 0; index != before.size(); ++index)
        {
            const auto rva = ClockImageFixture::DataRva + index;
            const bool owned = rva == hoursRva || rva == minutesRva || (rva >= lastChangeRva && rva < lastChangeRva + 4);
            if (!owned && before[index] != *reinterpret_cast<const unsigned char*>(fixture.image + rva))
                onlyClockFields = false;
        }
        Check(onlyClockFields, "independent whole-data-section comparison finds no timer, calendar, store-slot or unrelated writes");
        state = Execute(service, fixture, ClockOperation::ResetTimeFrozen, 3);
        Check(state.lastCommandResult == ClockResult::Ok && state.resets == 1 && !state.frozen && !state.patchInstalled,
              "already-unfrozen SDK reset succeeds even when branch writes have not been independently admitted");
        auto stale = Command(state, 4);
        --stale.generation;
        Check(!service.Submit(stale) && !service.Renew(state.generation + 1) && service.Renew(state.generation),
              "stale generations cannot queue or renew another owner's native clock lifetime");
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            {
                ClockServiceStateV1 unchanged;
                unchanged.hour = 99;
                foreignRefused = !service.Submit(Command(state, 4)) && !service.Renew(state.generation) && !service.Read(unchanged) && unchanged.hour == 99 &&
                                 !service.End();
            });
        foreign.join();
        Check(foreignRefused, "foreign producer calls cannot publish output, enqueue, renew or end the adopted owner's service");
        fixture.Put(nativeTimerRva, std::uint32_t{15000});
        FinishOwned(service, fixture);
        Check(fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 && fixture.Get<std::uint16_t>(secondsRva) == 27 &&
                  fixture.Get<std::uint32_t>(nativeTimerRva) == 15000 && fixture.Get<std::uint32_t>(lastChangeRva) == 14900,
              "normal owner release restores captured display and fractional phase without rewinding the native game timer");
    }

    void NativeCoherentCommandPublication(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        auto state = State(service);
        auto command = Command(state, 1, ClockOperation::Set);
        command.hour = 18;
        command.minute = 19;
        // This owned fixture already adopted the real SDK/native owner. Split
        // its command and callback publication phases deliberately, so the
        // regression observes the exact gap without a shipping test seam or
        // timing a racing OS thread. Other cases exercise the real mailbox.
        ++callbackDepth;
        CompleteCommand(command);
        --callbackDepth;
        if (runtime.lastCompletedSequence.load() != 1 || runtime.lastCommandResult.load() != ClockResult::Ok || fixture.Get<std::uint8_t>(hoursRva) != 18 ||
            fixture.Get<std::uint8_t>(minutesRva) != 19)
        {
            // Keep strict publication assertions while retaining the actual
            // refusal and page state when an earlier fixture leaves debt.
            MEMORY_BASIC_INFORMATION page{};
            ::VirtualQuery(reinterpret_cast<void*>(fixture.image + hoursRva), &page, sizeof(page));
            std::fprintf(stderr, "Clock Set diagnostic: sequence=%llu result=%u failure=%u hour=%u minute=%u protection=%lu age=%llu\n",
                         static_cast<unsigned long long>(runtime.lastCompletedSequence.load()), static_cast<unsigned>(runtime.lastCommandResult.load()),
                         static_cast<unsigned>(runtime.failure.load()), static_cast<unsigned>(fixture.Get<std::uint8_t>(hoursRva)),
                         static_cast<unsigned>(fixture.Get<std::uint8_t>(minutesRva)), static_cast<unsigned long>(page.Protect),
                         static_cast<unsigned long long>(GetTickCount64() - runtime.refreshed.load()));
        }
        Check(runtime.lastCompletedSequence.load() == 1 && runtime.lastCommandResult.load() == ClockResult::Ok && fixture.Get<std::uint8_t>(hoursRva) == 18 &&
                  fixture.Get<std::uint8_t>(minutesRva) == 19,
              "the actual SDK Set completes and changes owned native memory before callback readback is published");
        state = State(service);
        Check(state.lastCompletedSequence == 0 && state.lastCommandResult == ClockResult::NotInitialized && state.hour == 12 && state.minute == 34 &&
                  state.seconds == 27 && state.nativeTimer == 10000 && state.lastMinuteChange == 9900,
              "delayed native readback cannot expose a new Set acknowledgement paired with the previous clock values");
        fixture.invoke();
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 19 && state.seconds == 27 &&
                  state.nativeTimer == 10000 && state.lastMinuteChange == 10000,
              "the qualified callback publishes the completed Set and its actual native readback in one epoch");

        fixture.Put(minutesRva, std::uint8_t{20});
        fixture.Put(nativeTimerRva, std::uint32_t{10500});
        fixture.Put(lastChangeRva, std::uint32_t{10400});
        fixture.invoke();
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 20 &&
                  state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "ordinary captures carry the latest completed command forward while publishing natural clock advancement");

        command = Command(state, 2, ClockOperation::Set);
        command.hour = 20;
        command.minute = 21;
        ClockTestFaults::partialDataWrites.store(1);
        ++callbackDepth;
        CompleteCommand(command);
        --callbackDepth;
        Check(runtime.lastCompletedSequence.load() == 2 && runtime.lastCommandResult.load() == ClockResult::WriteFailure && runtime.dataRecovery.load(),
              "an actual partial Set records its failed completion and retains native restoration before publication");
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 20 &&
                  state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "an unpublished failed command cannot mix its result with the preceding successful clock publication");
        fixture.invoke();
        state = State(service);
        Check(state.lastCompletedSequence == 2 && state.lastCommandResult == ClockResult::WriteFailure && state.restored && state.hour == 12 &&
                  state.minute == 34 && state.seconds == 27 && state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "the recovery callback pairs the latest failed completion with its verified restored clock values");
        FinishOwned(service, fixture);
        state = State(service);
        Check(state.lastCompletedSequence == 2 && state.lastCommandResult == ClockResult::WriteFailure && state.restored && state.hour == 12 &&
                  state.minute == 34 && state.seconds == 27 && state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "End retains the final coherent completion and restoration values after native detachment");
    }

    void NativeFailureAndLease(ClockImageFixture& fixture)
    {
        for (bool maximum : {false, true})
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto state = State(service);
            if (maximum)
                runtime.started = GetTickCount64() - ClockServiceMaximumDurationMsV1 - 1;
            else
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
            Check(!service.Renew(state.generation) && !runtime.active.load() && runtime.failure.load() == ClockResult::Expired,
                  "elapsed renewable lease and maximum duration each stop rather than reviving an expired owner");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Execute(service, fixture, ClockOperation::Set);
            auto modified = NativeClockProfile::getterCase;
            modified[0] ^= 1;
            fixture.Patch(NativeClockProfile::getterCaseRva, modified.data(), modified.size());
            fixture.invoke();
            const auto state = State(service);
            Check(state.failure == ClockResult::BindingChanged && !state.active && !state.restored && state.restoreFailures && !service.End() &&
                      service.IsRetained(),
                  "changed native byte pins retain the hook and snapshot instead of claiming unverified restoration");
            fixture.Patch(NativeClockProfile::getterCaseRva, NativeClockProfile::getterCase.data(), NativeClockProfile::getterCase.size());
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            auto command = Command(State(service), 1, ClockOperation::Set);
            command.hour = 20;
            command.minute = 45;
            Check(service.Submit(command), "queue the actual native write before injecting a later clock-field page failure");
            DWORD protection = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + lastChangeRva), 4, PAGE_READONLY, &protection) != FALSE,
                  "remove write permission only from the owned timestamp/seconds image page");
            fixture.invoke();
            auto state = State(service);
            Check(state.failure == ClockResult::WriteFailure && state.dataRecoveryPending && !state.restored && state.restoreFailures &&
                      fixture.Get<std::uint8_t>(hoursRva) == 12 && !service.End() && service.IsRetained(),
                  "partial native Set and failed seconds restoration preserve explicit data recovery ownership");
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + lastChangeRva), 4, protection, &ignored) != FALSE,
                  "restore the owned data page so the actual native owner can retry its saved snapshot");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            ClockTestFaults::partialDataWrites.store(1);
            const auto state = Execute(service, fixture, ClockOperation::Set);
            Check(state.lastCommandResult == ClockResult::WriteFailure && state.failure == ClockResult::WriteFailure && state.restored &&
                      !state.dataRecoveryPending && fixture.Get<std::uint32_t>(lastChangeRva) == 9900,
                  "an actual short WriteProcessMemory data copy cannot report success and is restored by the saved native snapshot");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            ClockTestFaults::failEnumeration.store(true);
            const auto state = Execute(service, fixture, ClockOperation::Set);
            Check(state.lastCommandResult == ClockResult::Busy && state.failure == ClockResult::Busy && !state.sets && !state.retainedPeerThreads,
                  "Thread32Next failure is rejected as incomplete enumeration instead of admitting an unparked native write");
            FinishOwned(service, fixture);
        }
    }

    void NativeCaptureAndWraparound(ClockImageFixture& fixture)
    {
        for (unsigned invalid = 0; invalid != 3; ++invalid)
        {
            NativeClockService service;
            BeginOwned(service, fixture, false);
            if (invalid == 0)
                fixture.Put(minutesRva, std::uint8_t{60});
            if (invalid == 1)
                fixture.Put(secondsRva, std::uint16_t{61});
            if (invalid == 2)
                fixture.Put(lastChangeRva, std::uint32_t{8000});
            fixture.invoke();
            const auto state = State(service);
            Check(state.failure == ClockResult::BindingChanged && !state.ready && state.restored && !state.sets && !state.dataRecoveryPending,
                  "noncanonical initial minute, seconds or fractional phase refuses ownership before any native data writes");
            FinishOwned(service, fixture);
        }
        NativeClockService service;
        BeginOwned(service, fixture, false);
        fixture.Put(nativeTimerRva, std::uint32_t{50});
        fixture.Put(lastChangeRva, std::uint32_t{0xffffffce});
        fixture.invoke();
        Check(State(service).ready, "native DWORD timer wraparound retains a valid one-hundred-millisecond captured phase");
        Execute(service, fixture, ClockOperation::Set);
        fixture.Put(nativeTimerRva, std::uint32_t{125});
        FinishOwned(service, fixture);
        Check(fixture.Get<std::uint32_t>(nativeTimerRva) == 125 && fixture.Get<std::uint32_t>(lastChangeRva) == 25,
              "restore uses modular native timer arithmetic instead of rejecting or rewinding a wrapped timer");
    }

    void NativeReadOnlyRetirement(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        ClockTestFaults::clockDataWrites.store(0);
        ClockTestFaults::clockCodeExchanges.store(0);
        const auto state = Execute(service, fixture, ClockOperation::Read);
        Check(state.lastCommandResult == ClockResult::Ok && state.reads == 1 && !runtime.nativeTouched.load(),
              "actual SDK Read leaves the native clock mutation obligation untouched");
        // Model natural engine advancement after capture. Read-only cleanup
        // must retire ownership without replaying or rebasing the old snapshot.
        fixture.Put(hoursRva, std::uint8_t{17});
        fixture.Put(minutesRva, std::uint8_t{18});
        fixture.Put(secondsRva, std::uint16_t{19});
        fixture.Put(nativeTimerRva, std::uint32_t{25000});
        fixture.Put(lastChangeRva, std::uint32_t{24750});
        std::vector<unsigned char> natural(ClockImageFixture::DataSize);
        std::memcpy(natural.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), natural.size());
        FinishOwned(service, fixture);
        Check(!ClockTestFaults::clockDataWrites.load() && !ClockTestFaults::clockCodeExchanges.load() && !runtime.nativeTouched.load(),
              "actual read-only retirement makes zero native clock WriteProcessMemory calls or atomic code exchanges");
        Check(std::memcmp(natural.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), natural.size()) == 0 &&
                  fixture.Get<std::uint8_t>(hoursRva) == 17 && fixture.Get<std::uint8_t>(minutesRva) == 18 && fixture.Get<std::uint16_t>(secondsRva) == 19 &&
                  fixture.Get<std::uint32_t>(nativeTimerRva) == 25000 && fixture.Get<std::uint32_t>(lastChangeRva) == 24750,
              "read-only cleanup preserves the complete naturally advanced data image instead of restoring old display or fractional-minute fields");
    }

    void NativePeerResumeRecovery(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        const auto stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(stop != nullptr, "create an owned disposable peer lifetime for real suspend/resume recovery");
        std::atomic<DWORD> peerId{0};
        std::thread        peer(
            [&]
            {
                peerId.store(GetCurrentThreadId());
                WaitForSingleObject(stop, INFINITE);
            });
        while (!peerId.load())
            SwitchToThread();
        const auto handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, peerId.load());
        Check(handle != nullptr, "observe only the disposable fixture peer's suspension count");
        ClockTestFaults::resumeTarget.store(peerId.load());
        ClockTestFaults::failResumes.store(64);
        const auto before = State(service);
        Check(service.Submit(Command(before, 1, ClockOperation::Set)), "queue the actual SDK Set before holding its peer-resume recovery debt");
        fixture.invoke();
        const auto state = State(service);
        // A retained peer prevents the callback from reaching coherent native
        // readback/publication. The internal SDK completion is not yet a
        // producer acknowledgement; Execute's ordinary completion assertion
        // would incorrectly require exposing it with the old clock values.
        Check(runtime.lastCompletedSequence.load() == 1 && runtime.lastCommandResult.load() == ClockResult::RestoreFailure &&
                  state.lastCompletedSequence == before.lastCompletedSequence && state.lastCommandResult == before.lastCommandResult &&
                  state.hour == before.hour && state.minute == before.minute && state.seconds == before.seconds && !state.pendingSequence && !state.inFlight,
              "retained peer recovery withholds a newly completed SDK result until coherent callback readback can be published");
        Check(state.failure == ClockResult::RestoreFailure && state.retainedPeerThreads == 1 && state.dataRecoveryPending && !state.restored &&
                  !service.End() && service.IsRetained(),
              "failed ResumeThread keeps the actual suspended handle and native snapshot owned across callback and End");
        const auto count = ::SuspendThread(handle);
        Check(count == 1 && ::ResumeThread(handle) == 2, "independent suspend-count probe proves the retained peer was not falsely acknowledged as running");
        ClockTestFaults::failResumes.store(0);
        fixture.invoke();
        const auto recovered = State(service);
        Check(!recovered.retainedPeerThreads && runtime.recoveryPeers.empty() && recovered.restored && recovered.lastCompletedSequence == 1 &&
                  recovered.lastCommandResult == ClockResult::RestoreFailure && recovered.hour == before.hour && recovered.minute == before.minute &&
                  recovered.seconds == before.seconds,
              "the next native callback releases the exact retained peer and publishes its failed SDK result with verified restored clock values");
        SetEvent(stop);
        peer.join();
        CloseHandle(handle);
        CloseHandle(stop);
        FinishOwned(service, fixture);
    }

    void NativeEngineThreadHandoff(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture, false);
        const auto run = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        const auto done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Check(run && done, "create bounded handoff events for a distinct native engine fixture thread");
        std::atomic<bool>  stop{false};
        std::atomic<DWORD> engine{0};
        std::thread        worker(
            [&]
            {
                engine.store(GetCurrentThreadId());
                while (WaitForSingleObject(run, 5000) == WAIT_OBJECT_0)
                {
                    if (stop.load())
                        break;
                    fixture.invoke();
                    SetEvent(done);
                }
            });
        const auto tick = [&]
        {
            Check(service.Renew(State(service).generation) && SetEvent(run) && WaitForSingleObject(done, 5000) == WAIT_OBJECT_0,
                  "producer hands one real qualified callback to a separate engine thread within a bounded wait");
        };
        tick();
        auto state = State(service);
        Check(state.ready && state.engineThread == engine.load() && state.engineThread != state.producerThread,
              "the first native callback adopts its actual thread rather than the producer constructor thread");
        auto command = Command(state, 1, ClockOperation::Set);
        command.hour = 18;
        command.minute = 19;
        Check(service.Submit(command), "the producer queues an actual SDK write for the distinct engine owner");
        tick();
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 19,
              "the separate native owner consumes the mailbox and performs actual SDK data writes");
        fixture.invoke();
        state = State(service);
        Check(state.failure == ClockResult::WrongThread && state.foreignCallbacks == 1 && !state.active && !state.restored,
              "a later qualified callback on another thread stops safely without taking over the adopted native owner");
        Check(!service.End() && service.IsRetained(), "producer End waits for separate engine-owner restoration");
        Check(SetEvent(run) && WaitForSingleObject(done, 5000) == WAIT_OBJECT_0, "stopped owners still receive their final native restoration callback");
        Check(service.End() && !service.IsRetained() && State(service).restored,
              "producer detaches only after the separate native engine owner acknowledges restoration");
        Check(!NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "separate-thread cleanup releases its process-wide claim after native restoration and detachment");
        stop.store(true);
        SetEvent(run);
        worker.join();
        CloseHandle(run);
        CloseHandle(done);
        Check(CheckBinding(true), "separate-thread handoff leaves original native code bytes restored");
    }

    void NativeCommittedHookResumeRecovery(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture, false, false);
        const auto stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(stop != nullptr, "create a disposable parked peer for committed hook recovery");
        std::atomic<DWORD> peerId{0};
        std::thread        peer(
            [&]
            {
                peerId.store(GetCurrentThreadId());
                WaitForSingleObject(stop, INFINITE);
            });
        while (!peerId.load())
            SwitchToThread();
        ClockTestFaults::resumeTarget.store(peerId.load());
        ClockTestFaults::failResumes.store(64);
        const bool changed = ChangeHooks(true);
        const auto state = State(service);
        Check(!changed && state.hooked && state.retainedPeerThreads == 1 && service.IsRetained() &&
                  NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) &&
                  !CheckCode(NativeClockProfile::hookEntryRva, NativeClockProfile::hookEntry),
              "successful Detours commit followed by resume failure keeps the actual installed hook recorded and its suspended peer retained");
        Check(!service.End() && service.IsRetained(), "End cannot acknowledge a committed hook while a real suspended peer remains owned");
        ClockTestFaults::failResumes.store(0);
        Check(service.End() && !service.IsRetained() && !State(service).hooked && CheckBinding(true) &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "retained peer recovery then detaches the truly committed hook instead of losing it behind a failed transaction result");
        SetEvent(stop);
        peer.join();
        CloseHandle(stop);
    }

    void OriginalRaises()
    {
        RaiseException(0xe001c10c, 0, 0, nullptr);
    }

    bool InvokeOriginalException()
    {
        // Keep the SEH boundary in a normal compiler-owned frame: the synthetic
        // native code deliberately has no invented unwind metadata.
        __try
        {
            HookClockContinuation();
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    void NativeCallbackLifetime(ClockImageFixture& fixture)
    {
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            ++callbackDepth;
            fixture.invoke();
            --callbackDepth;
            const auto state = State(service);
            Check(state.failure == ClockResult::Busy && state.nestedCallbacks == 1 && !state.inFlight && !callbackDepth && !state.restored,
                  "nested actual hook callbacks stop without consuming SDK work or losing outer lifetime accounting");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto original = runtime.originalContinuation;
            runtime.originalContinuation = &OriginalRaises;
            const bool caught = InvokeOriginalException();
            runtime.originalContinuation = original;
            const auto state = State(service);
            Check(caught && state.failure == ClockResult::OriginalException && !state.inFlight && !callbackDepth && !state.restored,
                  "an original-continuation exception propagates while production finally releases callback debt and retains native restoration");
            FinishOwned(service, fixture);
        }
    }

    void NativeReadOnlyGameTimeReader(ClockImageFixture& fixture)
    {
        NativeGameTimeReader reader;
        GameTimeStateV1      state;
        std::memset(&state, 0x5a, sizeof(state));
        const auto untouched = state;
        const auto unchanged = [&] { return std::memcmp(&state, &untouched, sizeof(state)) == 0; };
        Check(reader.Read(state) == Status::NotInitialized && unchanged() && !reader.IsBound(),
              "an unbound read-only game-time service preserves every caller output byte");
        Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "the shipping game-time executable hash refuses this ordinary host");
        fixture.Seed();
        ClockReaderTestHost::image = fixture.image;
        Check(reader.Bind() == Status::Ok && reader.IsBound(), "the production reader admits the exact code, metadata and pages of its owned image fixture");
        const auto writesBefore = ClockTestFaults::clockDataWrites.load(), patchesBefore = ClockTestFaults::clockCodeExchanges.load();
        Check(reader.Read(state) == Status::Ok && state.size == sizeof(GameTimeStateV1) && state.version == GameTimeServiceVersionV1 &&
                  state.producerThread == GetCurrentThreadId() && state.hour == 12 && state.minute == 34 && state.seconds == 27 && state.nativeTimer == 10000 &&
                  state.lastMinuteChange == 9900 && state.millisecondsPerGameMinute == 1000 && !state.reserved[0] && !state.reserved[1] && !state.reserved[2],
              "two matching complete native reads publish the real timer, clock and minute duration without synthesized time");
        std::thread foreign(
            [&]
            {
                GameTimeStateV1 output = untouched;
                Check(
                    reader.Bind() == Status::WrongThread && reader.Read(output) == Status::WrongThread && std::memcmp(&output, &untouched, sizeof(output)) == 0,
                    "a foreign caller cannot steal reader ownership or publish a native sample");
            });
        foreign.join();
        state = untouched;
        ClockReaderTestHost::samples.store(0);
        ClockReaderTestHost::mismatchSample.store(2);
        Check(reader.Read(state) == Status::Unsupported && unchanged() && ClockReaderTestHost::samples.load() == 2,
              "a mismatch between complete native samples refuses publication without retries or a cached success");
        ClockReaderTestHost::mismatchSample.store(0);
        for (unsigned field = 0; field != 4; ++field)
        {
            fixture.Seed();
            if (field == 0)
                fixture.Put(NativeClockProfile::hoursRva, std::uint8_t{24});
            if (field == 1)
                fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{61});
            if (field == 2)
                fixture.Put(NativeClockProfile::minuteDurationRva, std::uint32_t{0});
            if (field == 3)
                fixture.Put(NativeClockProfile::minuteDurationRva, std::uint32_t{3600001});
            state = untouched;
            Check(reader.Read(state) == Status::Unsupported && unchanged(), "invalid native clock fields cannot escape as a successful game-time sample");
        }
        fixture.Seed();
        fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{60});
        fixture.Put(NativeClockProfile::secondsRva, std::uint16_t{65535});
        Check(reader.Read(state) == Status::Ok && state.minute == 60 && state.seconds == 65535,
              "raw native seconds and the existing SDK minute-60 boundary are preserved instead of inventing normalized time");
        fixture.Seed();
        auto changed = NativeClockProfile::getterCase;
        changed[0] ^= 1;
        fixture.Patch(NativeClockProfile::getterCaseRva, changed.data(), changed.size());
        state = untouched;
        Check(reader.Read(state) == Status::Unsupported && unchanged(), "changed live getter instructions invalidate an already bound reader");
        fixture.Patch(NativeClockProfile::getterCaseRva, NativeClockProfile::getterCase.data(), NativeClockProfile::getterCase.size());
        DWORD originalProtection = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeClockProfile::nativeTimerRva), sizeof(std::uint32_t), PAGE_NOACCESS,
                               &originalProtection) != FALSE,
              "deny native timer pages only inside the independently owned image");
        state = untouched;
        Check(reader.Read(state) == Status::Unsupported && unchanged(), "lost native page access refuses a fresh read while preserving output");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeClockProfile::nativeTimerRva), sizeof(std::uint32_t), originalProtection,
                               &ignored) != FALSE,
              "restore the owned native timer page after its read-only refusal test");
        Check(reader.Read(state) == Status::Ok && ClockTestFaults::clockDataWrites.load() == writesBefore &&
                  ClockTestFaults::clockCodeExchanges.load() == patchesBefore,
              "successful and failed game-time reads perform no clock writes, branch patches or hook ownership changes");

        NativeClockService service;
        BeginOwned(service, fixture);
        Check(reader.Read(state) == Status::Ok,
              "read-only game time coexists with the qualified clock continuation hook while its observed calculation span remains original");
        if (NativeClockProfile::clockBoundaryReviewed)
        {
            const auto frozen = Execute(service, fixture, ClockOperation::Freeze);
            Check(frozen.lastCommandResult == ClockResult::Ok && frozen.patchInstalled, "install only the existing clock owner's reviewed freeze branch");
            state = untouched;
            Check(reader.Read(state) == Status::Unsupported && unchanged(),
                  "read-only time refuses an altered calculation span without claiming or changing the clock writer's ownership");
        }
        FinishOwned(service, fixture);
        Check(reader.Read(state) == Status::Ok, "clock-owner retirement restores the original reader binding without a new time trial");
        reader.Reset();
        state = untouched;
        Check(!reader.IsBound() && reader.Read(state) == Status::NotInitialized && unchanged(),
              "reader reset discards only its local binding and does not return the earlier native sample");
        ClockReaderTestHost::image = 0;
        std::puts("Production read-only game-time reader fixture completed");
    }

    struct TimingInvocation
    {
        bool                 minute = false;
        float                scale = 0;
        std::uint32_t        duration = 0;
        unsigned             refusal = 0;
        ClockResult          result = ClockResult::NotInitialized;
        ContinuationFunction original = nullptr;
    } timingInvocation;

    void TimingContinuation()
    {
        // The real qualified hook has already adopted its owner. This test
        // replaces only the owned fixture's original continuation, just as the
        // existing exception fixture does; production gets no test command path.
        const auto priorGeneration = timingCallbackGeneration;
        const auto priorAdmission = runtime.admissionGeneration.load();
        if (timingInvocation.refusal == 1)
            timingCallbackGeneration = priorGeneration + 1;
        if (timingInvocation.refusal == 2)
            runtime.admissionGeneration.store(priorAdmission + 1);
        if (timingInvocation.refusal == 3)
            ++callbackDepth;
        NativeGameTimingAccess& access = runtime.access;
        timingInvocation.result = timingInvocation.minute ? access.SetMinuteDuration(timingInvocation.duration) : access.SetTimeScale(timingInvocation.scale);
        if (timingInvocation.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(priorAdmission);
        timingCallbackGeneration = priorGeneration;
        timingInvocation.original();
    }

    ClockResult InvokeTiming(ClockImageFixture& fixture, bool minute, float scale, std::uint32_t duration, unsigned refusal = 0)
    {
        timingInvocation = {minute, scale, duration, refusal, ClockResult::NotInitialized, runtime.originalContinuation};
        runtime.originalContinuation = &TimingContinuation;
        fixture.invoke();
        runtime.originalContinuation = timingInvocation.original;
        Check(!runtime.inFlight.load() && !callbackDepth && !timingCallbackGeneration,
              "the actual hook finally retires the qualified timing generation and callback scope");
        return timingInvocation.result;
    }

    void NativeTimingWritesAndRestoration(ClockImageFixture& fixture)
    {
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            NativeGameTimingAccess& access = runtime.access;
            const auto              beforeWrites = ClockTestFaults::clockDataWrites.load();
            Check(access.SetTimeScale(2.0f) == ClockResult::WrongThread && access.SetMinuteDuration(2000) == ClockResult::WrongThread &&
                      ClockTestFaults::clockDataWrites.load() == beforeWrites && !runtime.timingCaptured.load(),
                  "matching the adopted OS thread outside its natural callback does not grant timing write authority");
            std::thread foreign(
                [&]
                {
                    Check(access.SetTimeScale(2.0f) == ClockResult::WrongThread && access.SetMinuteDuration(2000) == ClockResult::WrongThread,
                          "a foreign producer cannot invoke the actual timing writer");
                });
            foreign.join();
            for (unsigned refusal = 1; refusal <= 3; ++refusal)
                Check(InvokeTiming(fixture, false, 2.0f, 0, refusal) == (refusal == 3 ? ClockResult::WrongThread : ClockResult::OwnerConflict) &&
                          !runtime.timingCaptured.load() && !runtime.timingDirty.load() && ClockTestFaults::clockDataWrites.load() == beforeWrites,
                      "stale callback generations, mismatched admission tokens and nested scopes cannot capture or write timing values");
            Check(runtime.active.load(), "unauthorized timing calls leave the actual current engine owner active");
            for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
                Check(InvokeTiming(fixture, false, invalid, 0) == ClockResult::InvalidArgument && !runtime.timingCaptured.load(),
                      "nonfinite scale values refuse without acquiring a native restoration snapshot");
            for (std::uint32_t invalid : {0u, 3600001u})
                Check(InvokeTiming(fixture, true, 0, invalid) == ClockResult::InvalidArgument && !runtime.timingCaptured.load(),
                      "minute-duration writes preserve the existing checked native reader domain");
            ClockTestFaults::failEnumeration.store(true);
            Check(InvokeTiming(fixture, false, 2.0f, 0) == ClockResult::Busy && !runtime.timingDirty.load() &&
                      ClockTestFaults::clockDataWrites.load() == beforeWrites,
                  "a refused peer transaction performs no timing write or dirty capture");
            Check(InvokeTiming(fixture, false, -0.5f, 0) == ClockResult::Ok && fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == -0.5f &&
                      runtime.originalTimeScale == 1.0f && runtime.timingDirty.load() == timingScaleBit && !runtime.timingRecovery.load() &&
                      !runtime.nativeTouched.load(),
                  "the real native writer preserves finite signed scale semantics and keeps successful ownership distinct from failed-write recovery");
            Check(InvokeTiming(fixture, false, 0.0f, 0) == ClockResult::Ok && fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 0 &&
                      runtime.originalTimeScale == 1.0f,
                  "repeated native scale writes retain the first original instead of recapturing an already mutated scalar");
            Check(InvokeTiming(fixture, true, 0, 2500) == ClockResult::Ok && fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 2500 &&
                      runtime.originalMinuteDuration == 1000 && runtime.timingDirty.load() == (timingScaleBit | timingMinuteBit) &&
                      !runtime.timingRecovery.load(),
                  "minute duration uses the same real native owner while retaining its own original value");
            const auto mutated = State(service);
            Check(mutated.active && mutated.ready && !mutated.restored && mutated.dataRecoveryPending && !runtime.dataRecovery.load(),
                  "unchanged V1 diagnostics aggregate successful timing restoration debt without pretending the displayed clock was mutated");

            fixture.Put(NativeClockProfile::hoursRva, std::uint8_t{13});
            fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{40});
            fixture.Put(NativeClockProfile::secondsRva, std::uint16_t{31});
            fixture.Put(NativeClockProfile::nativeTimerRva, std::uint32_t{10800});
            fixture.Put(NativeClockProfile::lastChangeRva, std::uint32_t{10500});
            std::vector<unsigned char> expected(ClockImageFixture::DataSize);
            std::memcpy(expected.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), expected.size());
            const float         originalScale = 1;
            const std::uint32_t originalDuration = 1000;
            std::memcpy(expected.data() + NativeGameTimingProfile::timeScaleRva - ClockImageFixture::DataRva, &originalScale, sizeof(originalScale));
            std::memcpy(expected.data() + NativeClockProfile::minuteDurationRva - ClockImageFixture::DataRva, &originalDuration, sizeof(originalDuration));
            const auto endWrites = ClockTestFaults::clockDataWrites.load();
            const auto endPatches = ClockTestFaults::clockCodeExchanges.load();
            Check(!service.End() && service.IsRetained() && State(service).dataRecoveryPending,
                  "producer End retains timing originals and cannot restore them from the caller's thread");
            FinishOwned(service, fixture);
            Check(std::memcmp(expected.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), expected.size()) == 0 &&
                      ClockTestFaults::clockDataWrites.load() == endWrites + 2 && ClockTestFaults::clockCodeExchanges.load() == endPatches,
                  "timing-only native retirement restores exactly scale and duration while preserving progressed H/M/S, last-minute change and global timer");
        }
        for (bool minute : {false, true})
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto target = minute ? NativeClockProfile::minuteDurationRva : NativeGameTimingProfile::timeScaleRva;
            const auto bit = minute ? timingMinuteBit : timingScaleBit;
            ClockTestFaults::timingWriteAddress = fixture.image + target;
            ClockTestFaults::partialTimingWrites.store(1);
            Check(InvokeTiming(fixture, minute, 1.2345f, 4321) == ClockResult::WriteFailure && runtime.timingDirty.load() == bit &&
                      runtime.timingRecovery.load() == bit && (runtime.timingCaptured.load() & bit) && !runtime.active.load() && !runtime.nativeTouched.load(),
                  "a partial real timing write retains its original before reporting failure and does not claim a CClock time mutation");
            const auto failed = State(service);
            Check(failed.failure == ClockResult::WriteFailure && failed.dataRecoveryPending && !failed.ready && !failed.restored && !service.End() &&
                      service.IsRetained(),
                  "V1 cleanup/readiness and producer End cannot hide an unconfirmed native timing write");
            ClockTestFaults::failTimingWrites.store(100);
            fixture.invoke();
            const auto pending = State(service);
            Check(pending.restoreFailures && pending.dataRecoveryPending && !pending.restored && runtime.timingRecovery.load() == bit && !service.End() &&
                      service.IsRetained(),
                  "a real native timing rollback failure keeps the same hook, snapshot and admission owner for retry");
            ClockTestFaults::failTimingWrites.store(0);
            FinishOwned(service, fixture);
            Check(minute ? fixture.Get<std::uint32_t>(target) == 1000 : fixture.Get<float>(target) == 1.0f,
                  "a later qualified callback restores the actual first timing value before native ownership retires");
            ClockTestFaults::timingWriteAddress = 0;
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Check(InvokeTiming(fixture, false, 1.5f, 0) == ClockResult::Ok && InvokeTiming(fixture, true, 0, 2000) == ClockResult::Ok,
                  "one existing native owner can own both timing scalars before a real SDK clock command");
            auto command = Command(State(service), 1, ClockOperation::Set);
            command.hour = 18;
            command.minute = 25;
            Check(service.Submit(command), "mixed timing ownership keeps the existing CClock mailbox usable without extending its protocol");
            fixture.invoke();
            Check(State(service).lastCommandResult == ClockResult::Ok && runtime.nativeTouched.load() && runtime.timingDirty.load(),
                  "a real SDK CClock mutation keeps separate timing and displayed-clock restoration obligations");
            fixture.Put(NativeClockProfile::nativeTimerRva, std::uint32_t{10800});
            FinishOwned(service, fixture);
            Check(fixture.Get<std::uint8_t>(NativeClockProfile::hoursRva) == 12 && fixture.Get<std::uint8_t>(NativeClockProfile::minutesRva) == 34 &&
                      fixture.Get<std::uint16_t>(NativeClockProfile::secondsRva) == 27 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::lastChangeRva) == 10700 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::nativeTimerRva) == 10800 &&
                      fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 1000,
                  "mixed retirement restores original timing scalars and preserves the real CClock fractional-minute rebase without rewinding global time");
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            auto changed = NativeGameTimingProfile::elapsedMultiplier;
            changed[0] ^= 1;
            fixture.Patch(NativeGameTimingProfile::elapsedMultiplierRva, changed.data(), changed.size());
            const auto writes = ClockTestFaults::clockDataWrites.load();
            Check(InvokeTiming(fixture, false, 2.0f, 0) == ClockResult::BindingChanged && !runtime.timingCaptured.load() && !runtime.timingDirty.load() &&
                      ClockTestFaults::clockDataWrites.load() == writes,
                  "changed native timing dataflow refuses mutation before capturing a value or writing memory");
            fixture.Patch(NativeGameTimingProfile::elapsedMultiplierRva, NativeGameTimingProfile::elapsedMultiplier.data(),
                          NativeGameTimingProfile::elapsedMultiplier.size());
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            fixture.Put(NativeGameTimingProfile::timeScaleRva, -0.0f);
            Check(InvokeTiming(fixture, false, 1.5f, 0) == ClockResult::Ok,
                  "the native writer captures the actual original scalar immediately before the first owned write");
            auto changed = NativeGameTimingProfile::scriptScale;
            changed[0] ^= 1;
            fixture.Patch(NativeGameTimingProfile::scriptScaleRva, changed.data(), changed.size());
            Check(!service.End(), "changed timing pins cannot retire a dirty writer from the producer thread");
            fixture.invoke();
            Check(!State(service).restored && State(service).dataRecoveryPending && !service.End() && service.IsRetained(),
                  "a changed corroborating timing binding withholds native rollback and retains the same generation owner");
            fixture.Patch(NativeGameTimingProfile::scriptScaleRva, NativeGameTimingProfile::scriptScale.data(), NativeGameTimingProfile::scriptScale.size());
            FinishOwned(service, fixture);
            const auto restored = fixture.Get<float>(NativeGameTimingProfile::timeScaleRva);
            Check(restored == 0 && std::signbit(restored), "confirmed native restoration preserves original signed-zero bits");
        }
        std::puts("Production native CGame timing writer fixture completed");
    }

    struct PauseHandlerContext
    {
        unsigned    calls = 0, operation = 0;
        bool        paused = false;
        ClockResult result = ClockResult::NotInitialized;
    };

    void PauseHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PauseHandlerContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "the real post-original timer handler receives only a qualified same-generation native owner");
        if (context.operation == 1)
            context.result = access.Pause(context.paused);
        else if (context.operation == 2)
            context.result = access.SetTimeScale(1.5f);
        else if (context.operation == 3)
            context.result = access.SetMinuteDuration(2000);
    }

    ContinuationFunction countedTimer = nullptr;
    unsigned             originalTimerInvocations = 0;
    void                 CountOriginalTimer()
    {
        ++originalTimerInvocations;
        countedTimer();
    }

    bool InvokeTimerOriginalException()
    {
        // The deliberate native exception crosses compiler-owned frames only;
        // no fabricated unwind metadata is assigned to synthetic game callers.
        __try
        {
            HookTimerUpdate();
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    void FinishTimingOwned(NativeClockService& service, ClockImageFixture& fixture)
    {
        const auto handler = runtime.timingHandler;
        const auto context = runtime.timingContext;
        Check(!service.End() && service.IsRetained() && runtime.timerHooked.load() && runtime.timingHandler == handler && runtime.timingContext == context,
              "End retains both actual hooks, callback context and pause originals while native cleanup is pending");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.timerHooked.load() && !runtime.hooked.load() && !runtime.timerReady.load() &&
                  !runtime.timingHandler && !runtime.timingContext && !runtime.inFlight.load() && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !runtime.timingCaptured.load() && !runtime.timingGeneration.load() && !timingCallbackGeneration &&
                  !timingTimerScope && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && CheckBinding(true),
              "the pause-safe native callback restores all debt before both real Detours hooks and handler context retire");
    }

    void NativePauseSafeTimingOwner(ClockImageFixture& fixture)
    {
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Check(runtime.access.Pause(true) == ClockResult::NotInitialized && !runtime.timingDirty.load() && !runtime.timerHooked.load(),
                  "legacy V1 clock-only admission cannot acquire pause debt without its recovery callback");
            FinishOwned(service, fixture);
        }
        for (bool initialPause : {false, true})
        {
            NativeClockService  service;
            PauseHandlerContext context;
            context.operation = 1;
            context.paused = !initialPause;
            BeginOwned(service, fixture, false, true, true, &PauseHandler, &context);
            fixture.Put(NativeGamePauseProfile::userPauseRva, static_cast<std::uint8_t>(initialPause));
            countedTimer = runtime.originalTimer;
            runtime.originalTimer = &CountOriginalTimer;
            originalTimerInvocations = 0;
            HookTimerUpdate();
            Check(originalTimerInvocations == 1 && !context.calls && !runtime.engineThread.load() && !runtime.timerReady.load(),
                  "unknown-origin timer invocations still execute their original update once without adopting or dispatching client work");
            fixture.invokeTimer();
            Check(originalTimerInvocations == 2 && context.calls == 1 && context.result == ClockResult::Ok && runtime.timerReady.load() &&
                      !runtime.captured.load() && !State(service).ready && runtime.originalUserPause == static_cast<std::uint8_t>(initialPause) &&
                      fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == static_cast<std::uint8_t>(!initialPause),
                  "a real normal-origin timer callback can change either initially paused or unpaused state without inventing clock readiness");
            Check(!service.Submit(Command(State(service), 1)) && !runtime.pendingSequence.load(),
                  "timer ownership alone does not admit CClock mailbox commands before its real postclock capture");
            context.operation = 0;
            fixture.invokeAlternateTimer();
            Check(originalTimerInvocations == 3 && context.calls == 2 && !runtime.captured.load(),
                  "the independently proved alternate gameplay tail return dispatches the same owner without losing its original update");
            context.operation = 1;
            context.paused = initialPause;
            fixture.invokeTimer();
            Check(
                context.result == ClockResult::Ok && fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == static_cast<std::uint8_t>(initialPause),
                "the actual native handler can reverse pause without waiting for the skipped weather continuation");
            context.paused = !initialPause;
            fixture.invokeTimer();
            context.operation = 2;
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "paused timer scope shares the actual time-scale writer without clock-ready masquerading");
            context.operation = 3;
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && runtime.timingDirty.load() == (timingPauseBit | timingScaleBit | timingMinuteBit) &&
                      !runtime.timingRecovery.load() && !runtime.nativeTouched.load() && !runtime.captured.load(),
                  "pause, scale and duration share one restoration owner while genuine CClock state remains uncaptured");
            runtime.originalTimer = countedTimer;
            fixture.Put(NativeClockProfile::hoursRva, std::uint8_t{14});
            fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{45});
            fixture.Put(NativeClockProfile::secondsRva, std::uint16_t{10});
            fixture.Put(NativeClockProfile::nativeTimerRva, std::uint32_t{20000});
            fixture.Put(NativeClockProfile::lastChangeRva, std::uint32_t{19950});
            const auto calls = context.calls;
            FinishTimingOwned(service, fixture);
            Check(context.calls == calls && fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == static_cast<std::uint8_t>(initialPause) &&
                      fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 1000 &&
                      fixture.Get<std::uint8_t>(NativeClockProfile::hoursRva) == 14 && fixture.Get<std::uint8_t>(NativeClockProfile::minutesRva) == 45 &&
                      fixture.Get<std::uint16_t>(NativeClockProfile::secondsRva) == 10 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::nativeTimerRva) == 20000 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::lastChangeRva) == 19950,
                  "stopped handlers never run again and timer-only cleanup restores original pause/timing without rewinding naturally progressed clock fields");
        }
        for (unsigned failure = 0; failure != 4; ++failure)
        {
            NativeClockService  service;
            PauseHandlerContext context;
            context.operation = 1;
            context.paused = true;
            BeginOwned(service, fixture, false, true, true, &PauseHandler, &context);
            fixture.invokeTimer();
            const auto calls = context.calls;
            if (failure == 0)
            {
                ClockTestFaults::timingWriteAddress = fixture.image + NativeGamePauseProfile::userPauseRva;
                ClockTestFaults::failTimingWrites.store(100);
                Check(!service.End(), "pause rollback first waits for the same actual native owner");
                fixture.invokeTimer();
                Check(!State(service).restored && State(service).dataRecoveryPending && runtime.timingRecovery.load() && service.IsRetained(),
                      "a real failed pause-byte rollback retains both hooks and context rather than reporting a restored runtime");
                ClockTestFaults::failTimingWrites.store(0);
                ClockTestFaults::timingWriteAddress = 0;
            }
            if (failure == 1)
            {
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
                fixture.invokeTimer();
                Check(State(service).failure == ClockResult::Expired && !runtime.timingDirty.load() && context.calls == calls,
                      "an expired paused owner restores from the live timer callback without dispatching further client work");
            }
            if (failure == 2)
            {
                std::thread foreign([&] { fixture.invokeAlternateTimer(); });
                foreign.join();
                Check(State(service).failure == ClockResult::WrongThread && context.calls == calls && runtime.timingDirty.load(),
                      "a foreign natural timer origin cannot take over the existing engine owner or discard its pause snapshot");
            }
            if (failure == 3)
            {
                const auto original = runtime.originalTimer;
                runtime.originalTimer = &OriginalRaises;
                const bool caught = InvokeTimerOriginalException();
                runtime.originalTimer = original;
                Check(caught && State(service).failure == ClockResult::OriginalException && !runtime.inFlight.load() && !callbackDepth &&
                          !timingCallbackGeneration && !timingTimerScope && runtime.timingDirty.load() && context.calls == calls,
                      "native timer exceptions propagate while actual hook finally releases callback scope and retains pause recovery");
            }
            if (!runtime.timingDirty.load())
            {
                Check(service.End() && !service.IsRetained() && !runtime.timingHandler && !runtime.timingContext,
                      "a lease-stop callback that already restored pause can retire both hooks without another mutation");
            }
            else
                FinishTimingOwned(service, fixture);
            Check(fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == 0 && context.calls == calls,
                  "all failed or stopped pause owners preserve the original byte and never resume handler dispatch");
        }
        std::puts("Production pause-safe native timing owner fixture completed");
    }

    struct SystemStateContext
    {
        unsigned          calls = 0, refusal = 0;
        SystemState::Enum requested = SystemState::GS_INIT_PLAYING_GAME;
        ClockResult       result = ClockResult::NotInitialized;
    };

    void SystemStateHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<SystemStateContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "system-state writes are requested from the actual admitted post-original timing handler");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        context.result = access.SetSystemState(context.requested);
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeSystemStateOwnedWrites(ClockImageFixture& fixture)
    {
        namespace StateProfile = NativeGameStateProfile;
        // This earlier timer fixture has a small synthetic alternate-origin
        // tail at the dispatcher entry. Preserve it while the independent state
        // cohort seeds every real profile pin, then return it to later tests.
        std::array<std::array<unsigned char, 94>, StateProfile::pins.size()> originals{};
        for (std::size_t index = 0; index < originals.size(); ++index)
            originals[index] = fixture.Get<std::array<unsigned char, 94>>(StateProfile::pins[index].rva);
        fixture.AdmitSystemStatePins();
        DWORD priorStateProtection = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + StateProfile::stateRva), sizeof(std::uint32_t), PAGE_READWRITE, &priorStateProtection) !=
                  FALSE,
              "the native state cohort admits only its actual owned DWORD page");
        fixture.Put(StateProfile::stateRva, std::uint32_t{9});
        NativeClockService service;
        SystemStateContext context;
        BeginOwned(service, fixture, false, true, true, &SystemStateHandler, &context);
        fixture.invokeTimer();
        Check(context.calls == 1 && context.result == ClockResult::Ok && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8 && !runtime.captured.load() &&
                  !runtime.timingCaptured.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load(),
              "the real timer owner accepts a system-state request without capturing temporary clock or timing restoration debt");
        Check(runtime.access.SetSystemState(SystemState::GS_FRONTEND) != ClockResult::Ok && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8,
              "a producer on the adopted OS thread cannot borrow system-state write authority outside its natural handler");
        bool        foreignRefused = false;
        std::thread foreign([&] { foreignRefused = runtime.access.SetSystemState(SystemState::GS_FRONTEND) != ClockResult::Ok; });
        foreign.join();
        Check(foreignRefused && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8,
              "a foreign producer cannot change the dispatcher through the active native owner");
        for (const auto target : {SystemState::GS_INIT_FRONTEND, SystemState::GS_FRONTEND, SystemState::GS_INIT_PLAYING_GAME, SystemState::GS_PLAYING_GAME})
        {
            context.requested = target;
            fixture.invokeTimer();
            std::uint32_t encoded = 0;
            Check(NativeGameStateMemory::Encode(target, encoded) && context.result == ClockResult::Ok &&
                      fixture.Get<std::uint32_t>(StateProfile::stateRva) == encoded,
                  "each established SDK state reaches the checked native DWORD through the real timing access implementation");
        }
        context.requested = SystemState::GS_INIT_PLAYING_GAME;
        fixture.invokeTimer();
        for (const auto invalid : {SystemState::GS_START_UP, SystemState::GS_INIT_LOGO_MPEG, SystemState::GS_LOGO_MPEG, SystemState::GS_INIT_INTRO_MPEG,
                                   SystemState::GS_INTRO_MPEG, SystemState::GS_INIT_ONCE})
        {
            context.requested = invalid;
            fixture.invokeTimer();
            Check(context.result == ClockResult::InvalidArgument && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8,
                  "unproved early SDK states refuse before any native system-state write");
        }
        context.requested = SystemState::GS_FRONTEND;
        for (unsigned refusal = 1; refusal <= 4; ++refusal)
        {
            context.refusal = refusal;
            fixture.invokeTimer();
            const auto expected = refusal <= 2 ? ClockResult::OwnerConflict : refusal == 3 ? ClockResult::WrongThread : ClockResult::Expired;
            Check(context.result == expected && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8,
                  "stale generations, wrong admission, nested callbacks and expiry cannot borrow a system-state write");
        }
        context.refusal = 0;
        auto changed = StateProfile::frontend_texture_label;
        changed[0] ^= 1;
        fixture.Patch(StateProfile::frontend_texture_labelRva, changed.data(), changed.size());
        fixture.invokeTimer();
        Check(context.result == ClockResult::BindingChanged && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8,
              "system-state writes independently require their complete semantic profile on the admitted timing owner");
        fixture.Patch(StateProfile::frontend_texture_labelRva, StateProfile::frontend_texture_label.data(), StateProfile::frontend_texture_label.size());
        fixture.Put(StateProfile::stateRva, std::uint32_t{0});
        fixture.invokeTimer();
        Check(context.result == ClockResult::BindingChanged && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 0,
              "an unproved current native state cannot be overwritten by a known target");
        fixture.Put(StateProfile::stateRva, std::uint32_t{7});
        ClockReaderTestHost::systemStateSamples.store(0);
        ClockReaderTestHost::transitionSystemStateSample.store(2);
        context.requested = SystemState::GS_INIT_PLAYING_GAME;
        fixture.invokeTimer();
        Check(context.result == ClockResult::Busy && !ClockReaderTestHost::transitionSystemStateSample.load() &&
                  fixture.Get<std::uint32_t>(StateProfile::stateRva) == 9,
              "a native advance after the owned read is preserved by the real CAS and reported as Busy through timing access");
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8,
              "a subsequent natural callback can submit fresh persistent state intent after a refused stale request");
        const auto calls = context.calls;
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "state-owner retirement can finish on its existing native callback");
        }
        Check(!service.IsRetained() && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext &&
                  context.calls == calls && fixture.Get<std::uint32_t>(StateProfile::stateRva) == 8 &&
                  runtime.access.SetSystemState(SystemState::GS_FRONTEND) == ClockResult::NotInitialized && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "End retires the actual owner and hooks without restoring an old system state over persistent dispatcher intent");
        fixture.Put(StateProfile::stateRva, std::uint32_t{9});
        for (std::size_t index = 0; index < originals.size(); ++index)
            fixture.Patch(StateProfile::pins[index].rva, originals[index].data(), StateProfile::pins[index].size);
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + StateProfile::stateRva), sizeof(std::uint32_t), priorStateProtection, &ignored) != FALSE,
              "restore only the owned fixture's previous state-page protection for later cohorts");
        std::puts("Production native CGame system-state owner fixture completed");
    }

    struct CheatFlagContext
    {
        NativeCheatProfile::Flag flag = NativeCheatProfile::Flag::HoverCars;
        unsigned                 calls = 0, operations = 3, refusal = 0;
        bool                     value = false, output = true;
        ClockResult              readResult = ClockResult::NotInitialized, writeResult = ClockResult::NotInitialized;
    };

    void CheatFlagHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<CheatFlagContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "four-flag access uses the actual admitted post-original timer handler");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        if (context.operations & 1)
            context.readResult = access.ReadCheatFlag(context.flag, context.output);
        if (context.operations & 2)
            context.writeResult = access.SetCheatFlag(context.flag, context.value);
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeCheatFlagOwnedAccess(ClockImageFixture& fixture)
    {
        namespace Cheats = NativeCheatProfile;
        fixture.AdmitCheatPins();
        ClockReaderTestHost::image = fixture.image;
        bool outside = true;
        Check(runtime.access.ReadCheatFlag(Cheats::Flag::HoverCars, outside) == ClockResult::NotInitialized && outside &&
                  runtime.access.SetCheatFlag(Cheats::Flag::HoverCars, true) == ClockResult::NotInitialized,
              "an unbound owner cannot read or mutate native cheat flags merely because their image profile is available");
        NativeClockService service;
        CheatFlagContext   context;
        BeginOwned(service, fixture, false, true, true, &CheatFlagHandler, &context);
        for (std::size_t index = 0; index < Cheats::Members.size(); ++index)
        {
            context.flag = static_cast<Cheats::Flag>(index);
            context.operations = 1;
            context.output = true;
            fixture.invokeTimer();
            Check(context.readResult == ClockResult::Ok && !context.output,
                  "each genuine SDK flag reads its initially clear native byte from the real callback");
            context.operations = 2;
            context.value = true;
            fixture.invokeTimer();
            Check(context.writeResult == ClockResult::Ok && fixture.Get<unsigned char>(Cheats::FlagsRva + Cheats::Members[index].nativeIndex) == 1,
                  "the actual engine owner sets each independently qualified one-byte cheat property");
            fixture.invokeTimer();
            Check(context.writeResult == ClockResult::Ok, "repeated native cheat intent is an explicit idempotent success");
            context.operations = 1;
            context.output = false;
            fixture.invokeTimer();
            Check(context.readResult == ClockResult::Ok && context.output, "each set flag is read fresh through the same actual owner");
        }
        context.flag = Cheats::Flag::HoverCars;
        context.operations = 3;
        context.value = false;
        outside = false;
        Check(runtime.access.ReadCheatFlag(context.flag, outside) != ClockResult::Ok && !outside &&
                  runtime.access.SetCheatFlag(context.flag, false) != ClockResult::Ok,
              "a producer on the adopted thread cannot borrow cheat access outside its native callback");
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            {
                bool value = false;
                foreignRefused = runtime.access.ReadCheatFlag(context.flag, value) != ClockResult::Ok && !value &&
                                 runtime.access.SetCheatFlag(context.flag, false) != ClockResult::Ok;
            });
        foreign.join();
        Check(foreignRefused, "a foreign producer cannot borrow the active native flag owner or publish a guessed read");
        for (unsigned refusal = 1; refusal <= 4; ++refusal)
        {
            context.refusal = refusal;
            context.output = false;
            fixture.invokeTimer();
            const auto expected = refusal <= 2 ? ClockResult::OwnerConflict : refusal == 3 ? ClockResult::WrongThread : ClockResult::Expired;
            Check(context.readResult == expected && context.writeResult == expected && !context.output &&
                      fixture.Get<unsigned char>(Cheats::FlagsRva + Cheats::Members[0].nativeIndex) == 1,
                  "generation, admission, nesting and expiry guards preserve the native flag and caller read output");
        }
        context.refusal = 0;
        context.flag = static_cast<Cheats::Flag>(255);
        context.output = false;
        fixture.invokeTimer();
        Check(context.readResult == ClockResult::InvalidArgument && context.writeResult == ClockResult::InvalidArgument && !context.output,
              "unknown flag identifiers refuse before native reads or writes");
        context.flag = Cheats::Flag::HoverCars;
        const auto& registration = Cheats::Registrations[0];
        const char  changed = '?';
        fixture.Patch(registration.nameRva, &changed, 1);
        fixture.invokeTimer();
        Check(context.readResult == ClockResult::BindingChanged && context.writeResult == ClockResult::BindingChanged && !context.output &&
                  fixture.Get<unsigned char>(Cheats::FlagsRva + Cheats::Members[0].nativeIndex) == 1,
              "both native access paths require the real alias and dispatch profile after existing exact-build admission");
        fixture.Patch(registration.nameRva, registration.name, 1);
        ClockTestFaults::cheatWriteAddress = fixture.image + Cheats::FlagsRva + Cheats::Members[0].nativeIndex;
        context.operations = 1;
        ClockTestFaults::failNextCheatRead.store(true);
        fixture.invokeTimer();
        Check(context.readResult == ClockResult::ReadFailure && !context.output && !ClockTestFaults::failNextCheatRead.load(),
              "a real guarded OS-copy failure preserves caller output rather than publishing false cheat state");
        context.operations = 2;
        context.value = true;
        ClockTestFaults::advanceCheatBeforeAtomic.store(true);
        fixture.invokeTimer();
        Check(context.writeResult == ClockResult::Busy && !ClockTestFaults::advanceCheatBeforeAtomic.load() &&
                  fixture.Get<unsigned char>(Cheats::FlagsRva + Cheats::Members[0].nativeIndex) == 0,
              "an intervening native flag change survives the real byte CAS and is reported as a conflict");
        ClockTestFaults::failCheatReadback.store(true);
        fixture.invokeTimer();
        Check(context.writeResult == ClockResult::WriteFailure && !ClockTestFaults::failCheatReadback.load() && !ClockTestFaults::failNextCheatRead.load() &&
                  fixture.Get<unsigned char>(Cheats::FlagsRva + Cheats::Members[0].nativeIndex) == 1,
              "an actual committed byte with failed readback remains unconfirmed and cannot be reported as success");
        Check(!runtime.nativeTouched.load() && !runtime.timingCaptured.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load(),
              "persistent flag setters never create temporary clock or timing restoration debt");
        const auto flagsBeforeEnd = fixture.Get<std::array<unsigned char, Cheats::NativeFlagCount>>(Cheats::FlagsRva);
        const auto calls = context.calls;
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "native flag ownership can retire through its existing callback");
        }
        const auto flagsAfterEnd = fixture.Get<std::array<unsigned char, Cheats::NativeFlagCount>>(Cheats::FlagsRva);
        Check(!service.IsRetained() && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext &&
                  context.calls == calls && flagsAfterEnd == flagsBeforeEnd && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "End retires hooks and authority without rewinding persistent native cheat properties or foreign edits");
        bool expectedArray = true;
        for (std::size_t index = 0; index < flagsAfterEnd.size(); ++index)
        {
            bool member = false;
            for (const auto& selected : Cheats::Members)
                member = member || selected.nativeIndex == index;
            expectedArray = expectedArray && flagsAfterEnd[index] == (member ? 1 : 0xa5);
        }
        Check(expectedArray, "four native property setters preserve every unrelated cheat byte instead of calling the native whole-registry reset");
        ClockTestFaults::cheatWriteAddress = 0;
        ClockReaderTestHost::image = 0;
        std::puts("Production native four-cheat-flag owner fixture completed");
    }

    struct WeatherContext
    {
        unsigned      calls = 0, operation = 1, refusal = 0;
        std::uint8_t  primary = 21, secondary = 22;
        std::uint16_t output = 0xabcd;
        ClockResult   result = ClockResult::NotInitialized;
    };

    void WeatherHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<WeatherContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "weather access is dispatched by the actual qualified post-original timer owner");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        if (context.operation == 1)
            context.result = access.ReadForced(context.output);
        if (context.operation == 2)
            context.result = access.SetTypes(context.primary, context.secondary);
        if (context.operation == 3)
            context.result = access.ResyncWithClock();
        if (context.operation == 4)
            context.result = access.ReleaseForced();
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeWeatherOwnedTransactions(ClockImageFixture& fixture)
    {
        namespace Weather = NativeWeatherTransitionProfile;
        using Field = Weather::Field;
        fixture.AdmitWeatherPins();
        ClockReaderTestHost::image = fixture.image;
        const auto field = [&](Field selected)
        {
            const auto& info = Weather::Fields[static_cast<std::size_t>(selected)];
            return info.size == 2 ? fixture.Get<std::uint16_t>(info.rva) : fixture.Get<std::uint32_t>(info.rva);
        };
        const auto snapshot = [&]
        {
            std::array<std::uint32_t, Weather::Fields.size()> result{};
            for (std::size_t index = 0; index < result.size(); ++index)
                result[index] = field(static_cast<Field>(index));
            return result;
        };
        {
            std::uint16_t outside = 0xabcd;
            Check(runtime.access.ReadForced(outside) == ClockResult::NotInitialized && outside == 0xabcd &&
                      runtime.access.SetTypes(21, 22) == ClockResult::NotInitialized && runtime.access.ResyncWithClock() == ClockResult::NotInitialized &&
                      runtime.access.ReleaseForced() == ClockResult::NotInitialized,
                  "weather image qualification cannot grant reads or any setter to an unbound producer");
            NativeClockService service;
            WeatherContext     context;
            BeginOwned(service, fixture, false, true, true, &WeatherHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "the producer renews only its actual weather callback generation");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.output == 3, "the actual weather owner reads the independently qualified forced WORD");
            context.operation = 2;
            invoke();
            Check(context.result == ClockResult::Ok && field(Field::OldType) == 21 && field(Field::NewType) == 22 && field(Field::ForcedType) == 3,
                  "both upper valid native weather types are written without calling force-now or changing forced weather");
            const auto valid = snapshot();
            for (const auto invalid : {std::uint8_t{23}, std::uint8_t{255}})
            {
                context.primary = invalid;
                invoke();
                Check(context.result == ClockResult::InvalidArgument && snapshot() == valid && !runtime.weatherRecovery.load(),
                      "weather values outside the native 23-column domain refuse without mutation or recovery debt");
            }
            context.primary = 21;
            context.operation = 3;
            invoke();
            Check(context.result == ClockResult::Ok && field(Field::PreviousClockSeconds) == 34 * 60 + 27 && field(Field::InterpolationBits) == 0 &&
                      field(Field::OldType) == 21 && field(Field::NewType) == 22,
                  "weather interpolation resync uses the current native minute and low seconds byte while preserving selected types");
            context.operation = 4;
            invoke();
            Check(context.result == ClockResult::Ok && field(Field::ForcedType) == 0xffff && field(Field::OldType) == 21 && field(Field::NewType) == 22,
                  "Release writes the full native signed-WORD sentinel instead of a classic byte or force-now side effects");
            const auto persistent = snapshot();
            outside = 0xabcd;
            Check(runtime.access.ReadForced(outside) != ClockResult::Ok && outside == 0xabcd && runtime.access.SetTypes(1, 2) != ClockResult::Ok &&
                      runtime.access.ResyncWithClock() != ClockResult::Ok && runtime.access.ReleaseForced() != ClockResult::Ok && snapshot() == persistent,
                  "the matching producer OS thread cannot borrow weather authority between natural callbacks");
            bool        foreignRefused = false;
            std::thread foreign(
                [&]
                {
                    std::uint16_t value = 0xabcd;
                    foreignRefused = runtime.access.ReadForced(value) != ClockResult::Ok && value == 0xabcd &&
                                     runtime.access.SetTypes(1, 2) != ClockResult::Ok && runtime.access.ResyncWithClock() != ClockResult::Ok &&
                                     runtime.access.ReleaseForced() != ClockResult::Ok;
                });
            foreign.join();
            Check(foreignRefused && snapshot() == persistent, "a foreign thread cannot borrow any of the four native weather operations");
            for (unsigned refusal = 1; refusal <= 4; ++refusal)
                for (unsigned operation = 1; operation <= 4; ++operation)
                {
                    context.refusal = refusal;
                    context.operation = operation;
                    context.output = 0xabcd;
                    invoke();
                    const auto expected = refusal <= 2 ? ClockResult::OwnerConflict : refusal == 3 ? ClockResult::WrongThread : ClockResult::Expired;
                    Check(context.result == expected && context.output == 0xabcd && snapshot() == persistent && !runtime.weatherRecovery.load(),
                          "all four weather operations reject stale generations, admission mismatch, nesting and expiry without touching fields");
                }
            context.refusal = 0;
            context.operation = 1;
            const auto& pin = Weather::Code[0];
            const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            fixture.Patch(pin.rva, &changed, 1);
            invoke();
            Check(context.result == ClockResult::BindingChanged && context.output == 0xabcd && snapshot() == persistent,
                  "native weather queries require their independent exact live profile on the otherwise valid timing owner");
            fixture.Patch(pin.rva, pin.bytes.data(), 1);
            const auto calls = context.calls;
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "successful persistent weather ownership can retire on its existing callback");
            }
            Check(!service.IsRetained() && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.weatherRecovery.load() &&
                      !runtime.weatherGeneration.load() && context.calls == calls && snapshot() == persistent,
                  "End retains successful weather changes and clears only native lifetime ownership, without a generic weather rewind");
        }
        for (const unsigned operation : {2u, 3u})
        {
            fixture.SeedWeatherFields();
            NativeClockService service;
            WeatherContext     context;
            context.operation = operation;
            BeginOwned(service, fixture, false, true, true, &WeatherHandler, &context);
            const auto first = operation == 2 ? Field::OldType : Field::PreviousClockSeconds;
            const auto second = operation == 2 ? Field::NewType : Field::InterpolationBits;
            const auto firstRva = Weather::Fields[static_cast<std::size_t>(first)].rva;
            ClockTestFaults::weatherRollbackAddress = fixture.image + firstRva;
            ClockTestFaults::weatherFailAddress = fixture.image + Weather::Fields[static_cast<std::size_t>(second)].rva;
            ClockTestFaults::failWeatherPair.store(true);
            fixture.invokeTimer();
            const auto debt = State(service);
            Check(context.result == ClockResult::RestoreFailure && !ClockTestFaults::failWeatherPair.load() && runtime.weatherRecovery.load() &&
                      runtime.weatherGeneration.load() == runtime.generation.load() && runtime.weather.HasPending() && debt.dataRecoveryPending &&
                      !debt.restored && !debt.ready && debt.failure == ClockResult::RestoreFailure && !service.End() && service.IsRetained() &&
                      runtime.timerHooked.load() && runtime.timingHandler == &WeatherHandler && runtime.timingContext == &context,
                  "a failed second weather write and failed first-field rollback retain the actual timer, journal, generation and handler context");
            for (std::size_t index = 0; index < ClockTestFaults::weatherProtectedCount; ++index)
            {
                DWORD ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::weatherProtectedPages[index]), 0x1000,
                                       ClockTestFaults::weatherOldProtections[index], &ignored) != FALSE,
                      "restore only owned pages changed by the deterministic weather protection race");
            }
            const auto          calls = context.calls;
            const std::uint16_t original = operation == 2 ? 1 : 1000;
            const auto          generation = runtime.weatherGeneration.load();
            runtime.weatherGeneration.store(generation + 1);
            fixture.invokeTimer();
            Check(runtime.weatherRecovery.load() && runtime.weather.HasPending() && !service.End(),
                  "a stale weather recovery generation cannot borrow or clear the partial native transaction");
            runtime.weatherGeneration.store(generation);
            if (operation == 3)
                fixture.Put(firstRva, std::uint16_t{1500});
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.weatherRecovery.load() && !runtime.weatherGeneration.load() &&
                      !runtime.weather.HasPending() && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.timingHandler &&
                      !runtime.timingContext && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && context.calls == calls &&
                      field(first) == (operation == 2 ? static_cast<std::uint32_t>(original) : std::uint32_t{1500}),
                  "only the qualified same-generation callback resolves owned weather debt before hooks, context and admission retire");
            const auto final = State(service);
            Check(operation == 2 ? final.restored && !runtime.weatherSuperseded.load()
                                 : !final.restored && runtime.weatherSuperseded.load() && final.failure == ClockResult::OwnerConflict,
                  "exact rollback is distinguished from terminal foreign supersession without retaining an owner that has no remaining owned data");
            ClockTestFaults::weatherFailAddress = ClockTestFaults::weatherRollbackAddress = 0;
            ClockTestFaults::weatherProtectedCount = 0;
        }
        ClockReaderTestHost::image = 0;
        std::puts("Production native weather transaction owner fixture completed");
    }

    struct WeatherScalarContext
    {
        NativeWeatherScalarProfile::Field field = NativeWeatherScalarProfile::Field::Rain;
        unsigned                          calls = 0, refusal = 0;
        float                             output = 123.0f;
        ClockResult                       result = ClockResult::NotInitialized;
    };

    void WeatherScalarHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<WeatherScalarContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "scalar SDK reads enter through the actual qualified post-original timer callback");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        const auto weatherGeneration = runtime.weatherGeneration.load();
        const auto weatherRecovery = runtime.weatherRecovery.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        if (context.refusal == 5)
        {
            runtime.weatherGeneration.store(generation);
            runtime.weatherRecovery.store(true);
        }
        context.result = access.ReadScalar(context.field, context.output);
        runtime.weatherGeneration.store(weatherGeneration);
        runtime.weatherRecovery.store(weatherRecovery);
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeWeatherScalarOwnedReads(ClockImageFixture& fixture)
    {
        namespace Scalars = NativeWeatherScalarProfile;
        fixture.AdmitWeatherPins();
        fixture.AdmitWeatherScalarPins();
        ClockReaderTestHost::image = fixture.image;
        const auto bits = [](float value)
        {
            std::uint32_t output = 0;
            std::memcpy(&output, &value, sizeof(output));
            return output;
        };
        const auto snapshot = [&]
        {
            std::array<std::uint32_t, Scalars::Fields.size()> result{};
            for (std::size_t index = 0; index < result.size(); ++index)
                result[index] = fixture.Get<std::uint32_t>(Scalars::Fields[index]);
            return result;
        };
        const auto original = snapshot();
        float      outside = 123.0f;
        Check(runtime.access.ReadScalar(Scalars::Field::Rain, outside) == ClockResult::NotInitialized && outside == 123.0f,
              "a qualified scalar image cannot authorize an unbound SDK reader");
        NativeClockService   service;
        WeatherScalarContext context;
        BeginOwned(service, fixture, false, true, true, &WeatherScalarHandler, &context);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew only the actual scalar callback owner's current generation");
            fixture.invokeTimer();
        };
        for (std::size_t index = 0; index < Scalars::Fields.size(); ++index)
        {
            context.field = static_cast<Scalars::Field>(index);
            context.output = 123.0f;
            invoke();
            Check(context.result == ClockResult::Ok && bits(context.output) == original[index],
                  "all eight actual native scalar reads preserve finite negatives, values above one and signed zero without clamping");
        }
        context.field = Scalars::Field::Rain;
        Check(runtime.access.ReadScalar(context.field, outside) != ClockResult::Ok && outside == 123.0f,
              "the adopted producer cannot borrow scalar authority between natural engine callbacks");
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            {
                float value = 123.0f;
                foreignRefused = runtime.access.ReadScalar(context.field, value) != ClockResult::Ok && value == 123.0f;
            });
        foreign.join();
        Check(foreignRefused, "a foreign producer cannot borrow the active scalar owner or publish a guessed observation");
        for (unsigned refusal = 1; refusal <= 5; ++refusal)
        {
            context.refusal = refusal;
            context.output = 123.0f;
            invoke();
            const auto expected = refusal <= 2   ? ClockResult::OwnerConflict
                                  : refusal == 3 ? ClockResult::WrongThread
                                  : refusal == 4 ? ClockResult::Expired
                                                 : ClockResult::Busy;
            Check(context.result == expected && context.output == 123.0f && snapshot() == original,
                  "scalar access rejects stale generations, mismatched admission, nested callbacks, expiry and incomplete weather mutations");
        }
        context.refusal = 0;
        context.field = static_cast<Scalars::Field>(255);
        invoke();
        Check(context.result == ClockResult::InvalidArgument && context.output == 123.0f, "an unknown scalar identifier refuses before field access");
        context.field = Scalars::Field::Rain;
        const auto& pin = Scalars::Code.front();
        const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.Patch(pin.rva, &changed, sizeof(changed));
        invoke();
        Check(context.result == ClockResult::BindingChanged && context.output == 123.0f,
              "scalar access checks its actual live native producer beyond the shared clock and weather qualification");
        fixture.Patch(pin.rva, pin.bytes.data(), sizeof(changed));
        const auto&         name = Scalars::Names.front();
        const std::uint64_t absent = 0, namedAddress = fixture.image + name.nameRva;
        fixture.Patch(name.recordRva, &absent, sizeof(absent));
        invoke();
        Check(context.result == ClockResult::BindingChanged && context.output == 123.0f,
              "a changed ASLR-aware named native consumer refuses without returning an unrelated float");
        fixture.Patch(name.recordRva, &namedAddress, sizeof(namedAddress));
        const std::uint32_t nonfinite = 0x7fc00000;
        fixture.Patch(Scalars::Fields[0], &nonfinite, sizeof(nonfinite));
        invoke();
        Check(context.result == ClockResult::ReadFailure && context.output == 123.0f,
              "a current native NaN is an explicit read refusal and preserves caller output");
        fixture.Patch(Scalars::Fields[0], &original[0], sizeof(original[0]));
        ClockTestFaults::scalarReadAddress = fixture.image + Scalars::Fields[0];
        ClockTestFaults::failScalarRead.store(true);
        invoke();
        Check(context.result == ClockResult::ReadFailure && context.output == 123.0f && !ClockTestFaults::failScalarRead.load(),
              "an actual guarded scalar OS-copy failure cannot publish a cached or default value");
        ClockTestFaults::scalarAfterRead = [] { ++timingCallbackGeneration; };
        invoke();
        Check(context.result == ClockResult::OwnerConflict && context.output == 123.0f && !ClockTestFaults::scalarAfterRead,
              "the actual owner is checked again after native data was copied before caller output can be committed");
        const std::uint32_t fresh = 0x40200000;
        fixture.Patch(Scalars::Fields[0], &fresh, sizeof(fresh));
        invoke();
        Check(context.result == ClockResult::Ok && bits(context.output) == fresh,
              "a later qualified scalar read observes the current native value rather than its prior sample");
        const auto beforeEnd = snapshot();
        Check(!runtime.nativeTouched.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load() && !runtime.weatherRecovery.load() &&
                  !runtime.weatherGeneration.load() && !runtime.weather.HasPending(),
              "readonly scalar getters create neither timing restoration debt nor weather transaction ownership");
        const auto calls = context.calls;
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "the existing native callback retires readonly scalar ownership");
        }
        Check(!service.IsRetained() && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext &&
                  context.calls == calls && snapshot() == beforeEnd && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "End releases hooks, callback context and admission without rewriting any native weather scalar");
        ClockTestFaults::scalarReadAddress = 0;
        ClockReaderTestHost::image = 0;
        std::puts("Production native weather scalar owner fixture completed");
    }

    struct WeaponWriteContext
    {
        std::size_t            slot = 77;
        unsigned               calls = 0, operation = 1, refusal = 0;
        NativeWeaponFloatField floatField = NativeWeaponFloatField::WeaponRange;
        NativeWeaponShortField shortField = NativeWeaponShortField::Damage;
        NativeWeaponDwordField dwordField = NativeWeaponDwordField::Flags;
        float                  floatValue = 12.5f;
        std::int16_t           shortValue = -123;
        std::uint32_t          dwordValue = 0, mask = 1;
        bool                   enabled = true;
        ClockResult            result = ClockResult::NotInitialized;
    };

    void WeaponWriteHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<WeaponWriteContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "typed weapon intents execute only through the real post-original engine timer callback");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        if (context.operation == 1)
            context.result = access.SetWeaponFloat(context.slot, context.floatField, context.floatValue);
        if (context.operation == 2)
            context.result = access.SetWeaponShort(context.slot, context.shortField, context.shortValue);
        if (context.operation == 3)
            context.result = access.SetWeaponDword(context.slot, context.dwordField, context.dwordValue);
        if (context.operation == 4)
            context.result = access.UpdateWeaponFlags(context.slot, context.mask, context.enabled);
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeWeaponOwnedWrites(ClockImageFixture& fixture)
    {
        namespace Weapons = NativeWeaponInfoProfile;
        fixture.AdmitWeaponPins();
        ClockReaderTestHost::image = fixture.image;
        using Bank = std::array<NativeWeaponInfoValues, Weapons::RowCount>;
        const auto bank = [&] { return fixture.Get<Bank>(Weapons::BankRva); };
        auto       expected = bank();
        const auto unchanged = [&]
        {
            const auto current = bank();
            return std::memcmp(current.data(), expected.data(), sizeof(expected)) == 0;
        };
        const auto refused = [&]
        {
            return runtime.access.SetWeaponFloat(77, NativeWeaponFloatField::WeaponRange, 1.0f) != ClockResult::Ok &&
                   runtime.access.SetWeaponShort(77, NativeWeaponShortField::Damage, 1) != ClockResult::Ok &&
                   runtime.access.SetWeaponDword(77, NativeWeaponDwordField::Flags, 1) != ClockResult::Ok &&
                   runtime.access.UpdateWeaponFlags(77, 1, true) != ClockResult::Ok;
        };
        Check(refused() && unchanged() && !runtime.weaponWriterReader.IsBound(),
              "an available native bank cannot grant any typed setter to an unbound producer");
        NativeClockService service;
        WeaponWriteContext context;
        BeginOwned(service, fixture, false, true, true, &WeaponWriteHandler, &context);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the actual native weapon callback generation");
            fixture.invokeTimer();
        };
        const std::array<std::size_t, 10> floatOffsets{offsetof(NativeWeaponInfoValues, weaponRange),  offsetof(NativeWeaponInfoValues, targetRange),
                                                       offsetof(NativeWeaponInfoValues, accuracy),     offsetof(NativeWeaponInfoValues, moveSpeed),
                                                       offsetof(NativeWeaponInfoValues, speed),        offsetof(NativeWeaponInfoValues, radius),
                                                       offsetof(NativeWeaponInfoValues, lifeSpan),     offsetof(NativeWeaponInfoValues, spread),
                                                       offsetof(NativeWeaponInfoValues, animBreakout), offsetof(NativeWeaponInfoValues, animFireTime)};
        for (std::size_t index = 0; index < floatOffsets.size(); ++index)
        {
            context.operation = 1;
            context.floatField = static_cast<NativeWeaponFloatField>(index);
            context.floatValue = 10.25f + static_cast<float>(index);
            invoke();
            std::memcpy(reinterpret_cast<unsigned char*>(&expected[context.slot]) + floatOffsets[index], &context.floatValue, sizeof(context.floatValue));
            Check(context.result == ClockResult::Ok && unchanged(),
                  "every typed float setter updates only its actual native four-byte field and preserves the other79 rows");
        }
        for (unsigned index = 0; index < 2; ++index)
        {
            context.operation = 2;
            context.shortField = static_cast<NativeWeaponShortField>(index);
            context.shortValue = static_cast<std::int16_t>(index ? 32767 : -123);
            invoke();
            if (index)
                expected[context.slot].clipAmmo = context.shortValue;
            else
                expected[context.slot].damage = context.shortValue;
            Check(context.result == ClockResult::Ok && unchanged(),
                  "each genuine signed-WORD setter preserves its neighboring native WORD and every unrelated field");
        }
        for (unsigned index = 0; index < 4; ++index)
        {
            context.operation = 3;
            context.dwordField = static_cast<NativeWeaponDwordField>(index);
            context.dwordValue = index == 0 ? 3 : index == 1 ? 0xfffffffe : index == 2 ? 0x80000810 : 36;
            invoke();
            auto& row = expected[context.slot];
            if (index == 0)
                row.skill = (row.skill & 0xffffff00) | context.dwordValue;
            if (index == 1)
                std::memcpy(&row.requiredStat, &context.dwordValue, sizeof(row.requiredStat));
            if (index == 2)
                std::memcpy(&row.flags, &context.dwordValue, sizeof(row.flags));
            if (index == 3)
                row.animGroup = context.dwordValue;
            Check(context.result == ClockResult::Ok && unchanged(),
                  "typed DWORD intent preserves the skill enum's upper bytes and bounds animation indices by the actual native count");
        }
        context.operation = 4;
        context.mask = 0x20;
        invoke();
        expected[context.slot].flags |= 0x20;
        Check(context.result == ClockResult::Ok && unchanged(), "masked flag enable preserves every unrelated native flag bit");
        context.enabled = false;
        context.mask = 0x10;
        invoke();
        expected[context.slot].flags &= ~0x10;
        Check(context.result == ClockResult::Ok && unchanged(), "masked flag disable clears only its explicitly owned native mask");
        Check(runtime.weaponWriterReader.IsBound() && runtime.weaponWriterGeneration == runtime.generation.load(),
              "all typed intents reuse the reader bound by this actual engine generation");
        Check(refused() && unchanged(), "even the adopted producer cannot borrow weapon write authority between native callbacks");
        bool        foreignRefused = false;
        std::thread foreign([&] { foreignRefused = refused(); });
        foreign.join();
        Check(foreignRefused && unchanged(), "a foreign thread cannot borrow any typed setter from the active native weapon owner");
        for (unsigned refusal = 1; refusal <= 4; ++refusal)
            for (unsigned operation = 1; operation <= 4; ++operation)
            {
                context.refusal = refusal;
                context.operation = operation;
                invoke();
                const auto result = refusal <= 2 ? ClockResult::OwnerConflict : refusal == 3 ? ClockResult::WrongThread : ClockResult::Expired;
                Check(context.result == result && unchanged(),
                      "all typed weapon setter families reject stale scope, mismatched admission, nested callbacks and expiry before mutation");
            }
        context.refusal = 0;
        for (unsigned operation = 1; operation <= 3; ++operation)
        {
            context.operation = operation;
            context.floatField = static_cast<NativeWeaponFloatField>(255);
            context.shortField = static_cast<NativeWeaponShortField>(255);
            context.dwordField = static_cast<NativeWeaponDwordField>(255);
            invoke();
            Check(context.result == ClockResult::InvalidArgument && unchanged(), "unknown typed descriptors refuse without touching the weapon bank");
        }
        context.floatField = NativeWeaponFloatField::WeaponRange;
        context.shortField = NativeWeaponShortField::Damage;
        context.operation = 3;
        context.dwordField = NativeWeaponDwordField::Skill;
        context.dwordValue = 4;
        invoke();
        Check(context.result == ClockResult::InvalidArgument && unchanged(),
              "a stored skill outside the four native enum values cannot use lookup fallback semantics");
        context.dwordField = NativeWeaponDwordField::AnimGroup;
        context.dwordValue = 37;
        invoke();
        Check(context.result == ClockResult::InvalidArgument && unchanged(), "the first animation index beyond the live native count refuses before CAS");
        context.slot = Weapons::RowCount;
        context.operation = 1;
        invoke();
        Check(context.result == ClockResult::InvalidArgument && unchanged(),
              "a native weapon slot outside the complete80-row bank cannot wrap to writable memory");
        context.slot = 77;
        const auto& pin = Weapons::Code.front();
        const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.Patch(pin.rva, &changed, sizeof(changed));
        invoke();
        Check(context.result == ClockResult::BindingChanged && unchanged(),
              "typed intents requalify the actual native bank selector and loader before mutation");
        fixture.Patch(pin.rva, pin.bytes.data(), sizeof(changed));
        const auto rowRva = Weapons::BankRva + context.slot * Weapons::RowBytes;
        ClockTestFaults::weaponRowAddress = fixture.image + rowRva;
        ClockTestFaults::weaponWriteAddress = fixture.image + rowRva + offsetof(NativeWeaponInfoValues, weaponRange);
        ClockTestFaults::weaponAfterRowRead = [] { ++timingCallbackGeneration; };
        const auto beforeScopeChange = ClockTestFaults::weaponExchanges.load();
        invoke();
        Check(context.result == ClockResult::OwnerConflict && unchanged() && !ClockTestFaults::weaponAfterRowRead &&
                  ClockTestFaults::weaponExchanges.load() == beforeScopeChange,
              "the real callback guard rejects a scope change after native row sampling and before the actual CAS");
        ClockTestFaults::advanceWeaponBeforeAtomic.store(true);
        invoke();
        std::uint32_t changedRange = 0;
        std::memcpy(&changedRange, &expected[context.slot].weaponRange, sizeof(changedRange));
        changedRange ^= 0x40;
        std::memcpy(&expected[context.slot].weaponRange, &changedRange, sizeof(changedRange));
        Check(context.result == ClockResult::Busy && unchanged() && !ClockTestFaults::advanceWeaponBeforeAtomic.load(),
              "a foreign four-byte field edit survives the real whole-field CAS instead of being overwritten or restored");
        context.operation = 2;
        ClockTestFaults::weaponWriteAddress = fixture.image + rowRva + offsetof(NativeWeaponInfoValues, damage);
        ClockTestFaults::advanceWeaponBeforeAtomic.store(true);
        invoke();
        std::uint16_t changedDamage = 0;
        std::memcpy(&changedDamage, &expected[context.slot].damage, sizeof(changedDamage));
        changedDamage ^= 0x40;
        std::memcpy(&expected[context.slot].damage, &changedDamage, sizeof(changedDamage));
        Check(context.result == ClockResult::Busy && unchanged(), "a foreign signed-WORD edit survives the actual exact-width16-bit CAS");
        context.operation = 4;
        context.mask = 1;
        context.enabled = true;
        ClockTestFaults::weaponWriteAddress = fixture.image + rowRva + offsetof(NativeWeaponInfoValues, flags);
        ClockTestFaults::advanceWeaponBeforeAtomic.store(true);
        const auto beforeFlagRetry = ClockTestFaults::weaponExchanges.load();
        invoke();
        expected[context.slot].flags = (expected[context.slot].flags ^ 0x40) | 1;
        Check(context.result == ClockResult::Ok && unchanged() && ClockTestFaults::weaponExchanges.load() == beforeFlagRetry + 2,
              "a masked intent retries from the actual changed flag word and preserves the native owner's unrelated bits");
        context.operation = 3;
        context.dwordField = NativeWeaponDwordField::RequiredStatLevel;
        context.dwordValue = 456;
        ClockTestFaults::weaponWriteAddress = fixture.image + rowRva + offsetof(NativeWeaponInfoValues, requiredStat);
        ClockTestFaults::failWeaponReadback.store(true);
        invoke();
        expected[context.slot].requiredStat = 456;
        Check(context.result == ClockResult::WriteFailure && unchanged() && !ClockTestFaults::failNextWeaponRow.load(),
              "a committed field with failed actual readback remains persistent but is not falsely reported as confirmed");
        context.dwordValue = 789;
        ClockTestFaults::protectWeaponBeforeAtomic.store(true);
        invoke();
        DWORD ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::weaponWriteAddress), sizeof(LONG), ClockTestFaults::weaponOldProtection, &ignored) !=
                  FALSE,
              "restore only the protection deliberately changed on the owned weapon fixture page");
        Check(context.result == ClockResult::WriteFailure && unchanged() && !ClockTestFaults::protectWeaponBeforeAtomic.load(),
              "an actual page-protection race at CAS refuses without widening engine permissions or creating row-restore debt");
        Check(!runtime.nativeTouched.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load() && !runtime.weatherRecovery.load() &&
                  !runtime.weatherGeneration.load(),
              "persistent typed weapon intents do not capture clock fields or create unrelated weather cleanup debt");
        const auto calls = context.calls;
        if (!service.End())
        {
            fixture.invokeTimer();
            Check(service.End(), "the existing natural callback retires typed weapon authority");
        }
        Check(!service.IsRetained() && !runtime.hooked.load() && !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext &&
                  !runtime.weaponWriterReader.IsBound() && !runtime.weaponWriterGeneration && context.calls == calls && unchanged() &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "End retires reader and callback authority while preserving persistent fields, foreign changes and every untouched weapon row");
        ClockTestFaults::weaponWriteAddress = ClockTestFaults::weaponRowAddress = 0;
        ClockReaderTestHost::image = 0;
        std::puts("Production native typed weapon write owner fixture completed");
    }

    struct WeaponVectorContext
    {
        std::size_t                                                 slot = 77;
        std::size_t                                                 bankIndex = 0;
        unsigned                                                    calls = 0, operation = 1, refusal = 0;
        CVector                                                     source{1.25f, -2.5f, 3.75f};
        const CVector*                                              input = &source;
        eWeaponSlot                                                 targetSlot = WEAPONSLOT_TYPE_DETONATOR;
        CVector*                                                    vector = reinterpret_cast<CVector*>(std::uintptr_t{0x1234});
        CWeaponInfo*                                                wrapper = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
        std::array<CWeaponInfo*, NativeWeaponInfoProfile::RowCount> wrappers{};
        ClockResult                                                 result = ClockResult::NotInitialized;
    };

    void WeaponVectorHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<WeaponVectorContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "native wrapper, vector and slot operations use the actual natural engine callback");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        if (context.operation == 1)
            context.result = access.BorrowWeaponFireOffset(context.slot, context.vector);
        if (context.operation == 2)
            context.result = access.SetWeaponFireOffset(context.slot, context.input);
        if (context.operation == 3)
            context.result = access.SetWeaponSlot(context.slot, context.targetSlot);
        if (context.operation == 4)
            context.result = access.GetWeaponInfoSlot(context.slot, context.wrapper);
        if (context.operation == 5)
        {
            context.result = ClockResult::Ok;
            // Exercise one row per real callback. A full verbose bank walk
            // must not accidentally turn the production 500ms lease into a
            // host-speed requirement; this also proves identity across frames.
            const auto index = context.bankIndex;
            {
                CWeaponInfo* selected = nullptr;
                Check(access.GetWeaponInfoSlot(index, selected) == ClockResult::Ok && selected,
                      "every native SDK bank row publishes a genuine concrete MTA wrapper inside its admitted callback");
                CWeaponInfo* repeated = nullptr;
                Check(access.GetWeaponInfoSlot(index, repeated) == ClockResult::Ok && repeated == selected,
                      "the same native row reuses its stable MTA wrapper rather than allocating a copied row object");
                Check(selected->GetTargetRange() == static_cast<float>(index + 1) &&
                          selected->GetFireOffset() ==
                              reinterpret_cast<CVector*>(runtime.image + NativeWeaponInfoProfile::BankRva + index * NativeWeaponInfoProfile::RowBytes + 0x24),
                      "actual SDK getter dispatch observes the corresponding native row and returns its real float3 alias");
                bool distinct = true;
                for (std::size_t prior = 0; prior < index; ++prior)
                    distinct = distinct && context.wrappers[prior] != selected;
                Check(distinct, "distinct native rows own distinct genuine SDK wrapper identities");
                context.wrappers[index] = selected;
            }
        }
        if (context.operation == 6 || context.operation == 7)
        {
            context.result = access.GetWeaponInfoSlot(context.slot, context.wrapper);
            if (context.result == ClockResult::Ok)
            {
                if (context.operation == 6)
                    context.wrapper->SetFireOffset(const_cast<CVector*>(context.input));
                else
                    context.wrapper->SetSlot(context.targetSlot);
            }
        }
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeWeaponVectorAndBankOwner(ClockImageFixture& fixture)
    {
        namespace Weapons = NativeWeaponInfoProfile;
        fixture.AdmitWeaponPins();
        fixture.AdmitWeaponVectorPins();
        ClockReaderTestHost::image = fixture.image;
        using Bank = std::array<NativeWeaponInfoValues, Weapons::RowCount>;
        const auto bank = [&] { return fixture.Get<Bank>(Weapons::BankRva); };
        auto       expected = bank();
        const auto unchanged = [&]
        {
            const auto current = bank();
            return std::memcmp(current.data(), expected.data(), sizeof(expected)) == 0;
        };
        CVector    source{1.0f, 2.0f, 3.0f};
        const auto refused = [&]
        {
            auto* vector = reinterpret_cast<CVector*>(std::uintptr_t{0x1234});
            auto* wrapper = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
            return runtime.access.BorrowWeaponFireOffset(77, vector) != ClockResult::Ok && vector == reinterpret_cast<CVector*>(std::uintptr_t{0x1234}) &&
                   runtime.access.SetWeaponFireOffset(77, &source) != ClockResult::Ok &&
                   runtime.access.SetWeaponSlot(77, WEAPONSLOT_TYPE_DETONATOR) != ClockResult::Ok &&
                   runtime.access.GetWeaponInfoSlot(77, wrapper) != ClockResult::Ok && wrapper == reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
        };
        Check(refused() && unchanged(), "neither an available bank nor stable wrapper allocation grants an unbound producer native authority");
        {
            NativeClockService  service;
            WeaponVectorContext context;
            context.operation = 5;
            BeginOwned(service, fixture, false, true, true, &WeaponVectorHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew only the current native vector/bank callback generation");
                fixture.invokeTimer();
            };
            for (std::size_t index = 0; index < context.wrappers.size(); ++index)
            {
                context.bankIndex = index;
                invoke();
            }
            Check(context.result == ClockResult::Ok && unchanged(), "all80 actual wrappers are read-only observations until a real SDK setter is called");
            context.operation = 1;
            invoke();
            const auto rowRva = Weapons::BankRva + context.slot * Weapons::RowBytes;
            Check(context.result == ClockResult::Ok && context.vector == reinterpret_cast<CVector*>(fixture.image + rowRva + 0x24),
                  "the native owner borrows the actual main-image vector and never substitutes cached backing storage");
            context.operation = 6;
            invoke();
            std::memcpy(expected[context.slot].fireOffset, &context.source, sizeof(context.source));
            Check(context.result == ClockResult::Ok && unchanged() && !runtime.weaponVectorRecovery.load() && !runtime.weaponVectorGeneration.load(),
                  "the complete actual CWeaponInfo SDK object dispatches XYZ through its real owner and commits persistent intent without rollback debt");
            context.operation = 7;
            invoke();
            expected[context.slot].slot = WEAPONSLOT_TYPE_DETONATOR;
            Check(context.result == ClockResult::Ok && unchanged(),
                  "the complete actual SDK SetSlot reaches the independently proved native thirteen-slot field");
            context.operation = 2;
            context.input = context.vector;
            invoke();
            Check(context.result == ClockResult::Ok && unchanged(), "the real borrowed vector can be used as its own source without partial alias corruption");
            context.input = &context.source;
            Check(refused() && unchanged(), "the adopted producer cannot borrow wrapper, vector or slot operations between natural callbacks");
            bool        foreignRefused = false;
            std::thread foreign([&] { foreignRefused = refused(); });
            foreign.join();
            Check(foreignRefused && unchanged(), "a foreign thread cannot borrow actual native wrappers or vector/slot mutation rights");
            for (unsigned refusal = 1; refusal <= 4; ++refusal)
                for (unsigned operation = 1; operation <= 4; ++operation)
                {
                    context.refusal = refusal;
                    context.operation = operation;
                    context.vector = reinterpret_cast<CVector*>(std::uintptr_t{0x1234});
                    context.wrapper = reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678});
                    invoke();
                    const auto result = refusal <= 2 ? ClockResult::OwnerConflict : refusal == 3 ? ClockResult::WrongThread : ClockResult::Expired;
                    Check(context.result == result && unchanged() && context.vector == reinterpret_cast<CVector*>(std::uintptr_t{0x1234}) &&
                              context.wrapper == reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678}),
                          "generation, admission, nesting and expiry refusals preserve bank fields and both borrowed caller outputs");
                }
            context.refusal = 0;
            context.operation = 3;
            context.targetSlot = static_cast<eWeaponSlot>(13);
            invoke();
            Check(context.result == ClockResult::InvalidArgument && unchanged(),
                  "slot13 is outside the independently decoded native array and refuses without CAS");
            context.targetSlot = WEAPONSLOT_TYPE_DETONATOR;
            context.operation = 2;
            context.input = nullptr;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && unchanged() && !runtime.weaponVectorRecovery.load(),
                  "a null vector source refuses without retaining nonexistent partial ownership");
            context.input = reinterpret_cast<const CVector*>(std::uintptr_t{1});
            invoke();
            Check(context.result == ClockResult::ReadFailure && unchanged() && !runtime.weaponVectorRecovery.load(),
                  "an unreadable caller vector is safely refused through the actual OS source-copy boundary");
            context.input = &context.source;
            context.operation = 4;
            context.slot = Weapons::RowCount;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && context.wrapper == reinterpret_cast<CWeaponInfo*>(std::uintptr_t{0x5678}) && unchanged(),
                  "out-of-range native bank lookup preserves caller output rather than returning an allocated dummy wrapper");
            context.slot = 77;
            context.operation = 1;
            const auto& pin = NativeWeaponInfoVectorProfile::Code.front();
            const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            fixture.Patch(pin.rva, &changed, sizeof(changed));
            invoke();
            Check(context.result == ClockResult::BindingChanged && context.vector == reinterpret_cast<CVector*>(std::uintptr_t{0x1234}) && unchanged(),
                  "borrowed vector qualification requires the actual native slot consumers in addition to the base weapon loader");
            fixture.Patch(pin.rva, pin.bytes.data(), sizeof(changed));
            ClockTestFaults::weaponRowAddress = fixture.image + rowRva;
            ClockTestFaults::weaponAfterRowRead = [] { ++timingCallbackGeneration; };
            invoke();
            Check(context.result == ClockResult::OwnerConflict && context.vector == reinterpret_cast<CVector*>(std::uintptr_t{0x1234}) && unchanged(),
                  "a generation change after native row qualification cannot publish a borrowed native vector");
            const auto calls = context.calls;
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "persistent vector/slot and real bank ownership retire on the existing native callback");
            }
            Check(!service.IsRetained() && !runtime.weaponWriterReader.IsBound() && !runtime.weaponVectorRecovery.load() &&
                      !runtime.weaponVectorGeneration.load() && context.calls == calls && unchanged(),
                  "End preserves successful XYZ and slot intent while retiring the reader and actual wrapper access authority");
        }
        for (const bool superseded : {false, true})
        {
            fixture.AdmitWeaponPins();
            fixture.AdmitWeaponVectorPins();
            expected = bank();
            NativeClockService  service;
            WeaponVectorContext context;
            context.operation = 2;
            BeginOwned(service, fixture, false, true, true, &WeaponVectorHandler, &context);
            const auto rowRva = Weapons::BankRva + context.slot * Weapons::RowBytes;
            ClockTestFaults::weaponRowAddress = fixture.image + rowRva;
            ClockTestFaults::weaponWriteAddress = fixture.image + rowRva + 0x28;
            ClockTestFaults::protectWeaponBeforeAtomic.store(true);
            fixture.invokeTimer();
            const auto debt = State(service);
            Check(context.result == ClockResult::RestoreFailure && runtime.weaponVectorRecovery.load() && runtime.weaponVectorJournal.remainingMask == 1 &&
                      runtime.weaponVectorGeneration.load() == runtime.generation.load() && debt.dataRecoveryPending && !debt.restored && !service.End() &&
                      service.IsRetained() && runtime.weaponWriterReader.IsBound() && runtime.timingHandler && runtime.timingContext,
                  "failed partial XYZ restoration retains its exact native generation, journal, reader and callback context before End can detach");
            Check(!RecoverWeaponVector() && runtime.weaponVectorJournal.remainingMask == 1,
                  "the producer cannot service partial native vector debt outside its original callback");
            DWORD ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::weaponWriteAddress), sizeof(LONG), ClockTestFaults::weaponOldProtection,
                                   &ignored) != FALSE,
                  "restore only the owned test page deliberately made readonly at the second XYZ component");
            const auto generation = runtime.weaponVectorGeneration.load();
            runtime.weaponVectorGeneration.store(generation + 1);
            fixture.invokeTimer();
            Check(runtime.weaponVectorRecovery.load() && runtime.weaponVectorJournal.remainingMask == 1 && !service.End(),
                  "even a real native callback cannot recover vector debt assigned to a different owner generation");
            runtime.weaponVectorGeneration.store(generation);
            if (superseded)
            {
                const float foreign = 99.0f;
                fixture.Patch(rowRva + 0x24, &foreign, sizeof(foreign));
                expected[context.slot].fireOffset[0] = foreign;
            }
            const auto calls = context.calls;
            fixture.invokeTimer();
            Check(service.End() && !service.IsRetained() && !runtime.weaponVectorRecovery.load() && !runtime.weaponVectorGeneration.load() &&
                      !runtime.weaponVectorJournal.remainingMask && !runtime.weaponWriterReader.IsBound() && !runtime.hooked.load() &&
                      !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext && context.calls == calls && unchanged(),
                  "only the matching natural callback resolves still-owned XYZ components before hooks, reader and wrapper authority retire");
            const auto final = State(service);
            Check(superseded ? !final.restored && runtime.weaponVectorSuperseded.load() && final.failure == ClockResult::OwnerConflict
                             : final.restored && !runtime.weaponVectorSuperseded.load(),
                  "partial XYZ recovery distinguishes exact restoration from preserved native supersession without retaining debt that no longer exists");
            ClockTestFaults::weaponWriteAddress = ClockTestFaults::weaponRowAddress = 0;
        }
        ClockReaderTestHost::image = 0;
        std::puts("Production native weapon vector and actual80-wrapper bank owner fixture completed");
    }

    struct StatsOwnerContext
    {
        unsigned       calls = 0, operation = 0, refusal = 0;
        unsigned short index = 0;
        float          value = 2.5f, observed = -1;
        CStats*        sdk = nullptr;
        ClockResult    result = ClockResult::NotInitialized;
        bool           threw = false;
    };

    void StatsOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<StatsOwnerContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "actual complete CStats publication and mutations enter the existing natural engine callback");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        context.threw = false;
        try
        {
            if (context.operation == 0)
                context.result = access.GetStats(context.sdk);
            if (context.operation == 1)
            {
                context.sdk->SetStatValue(context.index, context.value);
                context.result = ClockResult::Ok;
            }
            if (context.operation == 2)
            {
                context.sdk->ModifyStat(context.index, context.value);
                context.result = ClockResult::Ok;
            }
            if (context.operation == 3)
                context.result = access.SetStatValue(context.index, context.value);
            if (context.operation == 4)
                context.result = access.ModifyStat(context.index, context.value);
            if (context.operation == 5)
            {
                context.observed = context.sdk->GetStatValue(context.index);
                Check(context.sdk->GetSkillStatIndex(WEAPONTYPE_PISTOL) == 69,
                      "the complete Runtime CStats object uses the actual native weapon skill classifier");
                context.result = ClockResult::Ok;
            }
        }
        catch (const std::runtime_error&)
        {
            context.threw = true;
        }
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeStatsOwnedSdk(ClockImageFixture& fixture)
    {
        namespace Stats = NativeStatsProfile;
        namespace Writes = NativeStatsWriteProfile;
        namespace StateProfile = NativeGameStateProfile;
        NativeStatsReader wrongHost;
        Check(wrongHost.Bind() == Status::Unsupported && !wrongHost.IsBound(),
              "the actual stats reader retains shipping wrong-executable refusal before the owned image is selected");
        std::array<std::array<unsigned char, 94>, StateProfile::pins.size()> statePins{};
        for (std::size_t index = 0; index < statePins.size(); ++index)
            statePins[index] = fixture.Get<std::array<unsigned char, 94>>(StateProfile::pins[index].rva);
        fixture.AdmitSystemStatePins();
        fixture.AdmitStatsPins();
        DWORD priorStateProtection = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + StateProfile::stateRva), sizeof(std::uint32_t), PAGE_READWRITE, &priorStateProtection) !=
                  FALSE,
              "admit only the owned native playing-state DWORD for stat-call bookends");
        const auto oldState = fixture.Get<std::uint32_t>(StateProfile::stateRva);
        ClockReaderTestHost::image = ClockStatsOwnerTest::image = fixture.image;
        ClockStatsOwnerTest::calls = 0;
        ClockStatsOwnerTest::effect = false;
        ClockStatsOwnerTest::result = ClockResult::Ok;
        CStats* sentinel = reinterpret_cast<CStats*>(std::uintptr_t{1});
        Check(runtime.access.GetStats(sentinel) == ClockResult::NotInitialized && sentinel == reinterpret_cast<CStats*>(std::uintptr_t{1}) &&
                  runtime.access.SetStatValue(0, 1) == ClockResult::NotInitialized && !ClockStatsOwnerTest::calls,
              "unbound stats publication and mutation preserve output and never reach the native-call boundary");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "stat effects need no bank rollback before the actual callback and hooks retire");
            }
            Check(!service.IsRetained() && !runtime.statsReader.IsBound() && !runtime.statsGeneration && !runtime.timingHandler && !runtime.timingContext &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "stats retirement discards borrowed reader scope only after the actual owner and hooks retire");
        };
        {
            fixture.Put(StateProfile::stateRva, std::uint32_t{9});
            NativeClockService service;
            StatsOwnerContext  context;
            BeginOwned(service, fixture, false, true, true, &StatsOwnerHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew the current stats owner between distinct natural callbacks");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && context.sdk == &runtime.stats && runtime.statsReader.IsBound() &&
                      runtime.statsGeneration == runtime.generation.load(),
                  "same-callback publication returns the complete actual Runtime CStats object with its qualified reader");
            auto* const stable = context.sdk;
            invoke();
            Check(context.sdk == stable, "subsequent admitted callbacks publish the same complete stats object without rebinding its reader");
            for (unsigned step = 0; step < 3; ++step)
            {
                const auto operation = step ? 2u : 1u;
                context.operation = operation;
                context.value = std::array<float, 3>{2.5f, 3.5f, -4.5f}[step];
                const auto calls = ClockStatsOwnerTest::calls;
                invoke();
                const auto expectedEntry = operation == 1 ? Writes::SetRva : context.value < 0 ? Writes::DecrementRva : Writes::IncrementRva;
                Check(!context.threw && context.result == ClockResult::Ok && ClockStatsOwnerTest::calls == calls + 1 &&
                          ClockStatsOwnerTest::entry == expectedEntry && ClockStatsOwnerTest::index == context.index &&
                          ClockStatsOwnerTest::amount == std::fabs(context.value),
                      "actual CStats setters route Set and signed Modify through the exact typed native entry with promoted index and XMM1 amount");
            }
            context.operation = 5;
            invoke();
            Check(context.result == ClockResult::Ok && context.observed == 0,
                  "actual CStats GetStatValue remains independent from accepted native no-op mutation returns");
            context.operation = 3;
            const auto calls = ClockStatsOwnerTest::calls;
            Check(runtime.access.SetStatValue(0, 1) != ClockResult::Ok && runtime.access.ModifyStat(0, 1) != ClockResult::Ok &&
                      ClockStatsOwnerTest::calls == calls,
                  "producer calls on the adopted OS thread cannot reuse an earlier stats callback lease");
            bool        foreignRefused = false;
            std::thread foreign([&] { foreignRefused = runtime.access.SetStatValue(0, 1) != ClockResult::Ok; });
            foreign.join();
            Check(foreignRefused && ClockStatsOwnerTest::calls == calls, "a foreign thread cannot invoke native stat reactions through a retained SDK owner");
            for (unsigned refusal = 1; refusal <= 4; ++refusal)
            {
                context.refusal = refusal;
                invoke();
                const auto expected = refusal <= 2 ? ClockResult::OwnerConflict : refusal == 3 ? ClockResult::WrongThread : ClockResult::Expired;
                Check(context.result == expected && ClockStatsOwnerTest::calls == calls,
                      "stale generation, admission, nested callback and expired scopes refuse before native stat entry");
            }
            context.refusal = 0;
            const auto generation = runtime.statsGeneration;
            runtime.statsGeneration = generation + 1;
            invoke();
            Check(context.result == ClockResult::OwnerConflict && ClockStatsOwnerTest::calls == calls,
                  "a cached stats reader from another generation never grants native mutation authority");
            runtime.statsGeneration = generation;
            for (const std::uint32_t state : {6u, 7u, 8u, 0u})
            {
                fixture.Put(StateProfile::stateRva, state);
                invoke();
                Check(context.result != ClockResult::Ok && ClockStatsOwnerTest::calls == calls,
                      "native stat reactions require qualified live playing9 rather than a merely writable startup enum");
            }
            fixture.Put(StateProfile::stateRva, std::uint32_t{9});
            fixture.Put(Writes::PlayerRootRva, std::uintptr_t{0});
            invoke();
            Check(context.result == ClockResult::NotInitialized && ClockStatsOwnerTest::calls == calls && !runtime.stopping.load(),
                  "state9 without the actual loaded local-player identity refuses without pretending native effects occurred");
            fixture.Put(Writes::PlayerRootRva, reinterpret_cast<std::uintptr_t>(fixture.statsPed.data()));
            context.index = 82;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && ClockStatsOwnerTest::calls == calls,
                  "the SDK stat gap refuses through the real owner and primitive before any native function entry");
            context.index = 64;
            invoke();
            Check(context.result == ClockResult::Ok && ClockStatsOwnerTest::calls == calls + 1 && ClockStatsOwnerTest::entry == Writes::SetRva &&
                      ClockStatsOwnerTest::index == 64,
                  "special stat64 uses the qualified loaded script input and accepts an intentional native no-op");
            context.index = 0;
            ClockStatsOwnerTest::effect = true;
            context.value = 7.5f;
            invoke();
            Check(context.result == ClockResult::Ok && fixture.Get<float>(Stats::FloatBankRva) == 17.5f && !runtime.timingDirty.load() &&
                      !runtime.timingRecovery.load() && !runtime.captured.load(),
                  "a returned native reaction accepts its own resulting value without capturing a bank or clock restoration snapshot");
            retire(service);
            Check(fixture.Get<float>(Stats::FloatBankRva) == 17.5f, "End preserves successfully accepted native stat effects instead of rewinding the bank");
        }
        for (unsigned failure = 0; failure < 3; ++failure)
        {
            fixture.Put(StateProfile::stateRva, std::uint32_t{9});
            NativeClockService service;
            StatsOwnerContext  context;
            context.operation = 3;
            context.value = 11.5f + static_cast<float>(failure);
            ClockStatsOwnerTest::effect = true;
            ClockStatsOwnerTest::result = failure == 0 ? ClockResult::OriginalException : ClockResult::Ok;
            if (failure == 1)
                ClockStatsOwnerTest::after = [] { runtime.admissionGeneration.fetch_add(1); };
            if (failure == 2)
                ClockStatsOwnerTest::after = []
                {
                    const std::uint32_t state = 8;
                    SIZE_T              copied = 0;
                    if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(runtime.image + NativeGameStateProfile::stateRva), &state,
                                              sizeof(state), &copied) ||
                        copied != sizeof(state))
                        std::abort();
                };
            BeginOwned(service, fixture, false, true, true, &StatsOwnerHandler, &context);
            const auto calls = ClockStatsOwnerTest::calls;
            fixture.invokeTimer();
            const auto expected = failure == 0 ? ClockResult::OriginalException : failure == 1 ? ClockResult::OwnerConflict : ClockResult::NotInitialized;
            Check(context.result == expected && ClockStatsOwnerTest::calls == calls + 1 && runtime.stopping.load() &&
                      fixture.Get<float>(Stats::FloatBankRva) == context.value + 10.0f,
                  "native exception or failed post-call owner/state bookend stops the lease after preserving already-entered effects");
            Check(runtime.access.SetStatValue(0, context.value) != ClockResult::Ok && ClockStatsOwnerTest::calls == calls + 1,
                  "a failed entered stat transaction is never automatically replayed through the stopped native owner");
            retire(service);
            Check(fixture.Get<float>(Stats::FloatBankRva) == context.value + 10.0f &&
                      fixture.Get<std::uint32_t>(StateProfile::stateRva) == (failure == 2 ? 8u : 9u),
                  "retirement preserves native stat and dispatcher advances after an entered failure");
        }
        ClockStatsOwnerTest::effect = false;
        ClockStatsOwnerTest::result = ClockResult::Ok;
        ClockStatsOwnerTest::after = nullptr;
        fixture.Put(StateProfile::stateRva, oldState);
        for (std::size_t index = 0; index < statePins.size(); ++index)
            fixture.Patch(StateProfile::pins[index].rva, statePins[index].data(), StateProfile::pins[index].size);
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + StateProfile::stateRva), sizeof(std::uint32_t), priorStateProtection, &ignored) != FALSE,
              "restore the earlier owned alternate timer/state fixture pins and protection without changing native production profiles");
        ClockReaderTestHost::image = ClockStatsOwnerTest::image = 0;
        std::puts("Production native complete CStats SDK owner fixture completed");
    }

    struct WeatherOverrideContext
    {
        unsigned                          calls = 0, operation = 1, sdkIndex = 2, refusal = 0;
        float                             value = 0.625f, output = 123.0f;
        bool                              sdkResult = false;
        NativeWeatherScalarProfile::Field field = NativeWeatherScalarProfile::Field::Fog;
        ClockResult                       result = ClockResult::NotInitialized;
    };

    void WeatherOverrideHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<WeatherOverrideContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "actual weather SDK suppression methods borrow the existing natural timer callback owner");
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            timingCallbackGeneration = generation + 1;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        CWeatherOverridesDE                            sdk(access, access, access);
        const std::array<bool (CWeather::*)(float), 7> setters{&CWeather::SetWetRoads, &CWeather::SetFoggyness, &CWeather::SetFog,    &CWeather::SetRainFog,
                                                               &CWeather::SetWaterFog, &CWeather::SetSandstorm, &CWeather::SetRainbow};
        const std::array<bool (CWeather::*)(), 7>      resetters{&CWeather::ResetWetRoads, &CWeather::ResetFoggyness, &CWeather::ResetFog,
                                                            &CWeather::ResetRainFog,  &CWeather::ResetWaterFog,  &CWeather::ResetSandstorm,
                                                            &CWeather::ResetRainbow};
        if (context.operation == 1)
            context.sdkResult = (sdk.*setters[context.sdkIndex])(context.value);
        if (context.operation == 2)
            context.sdkResult = (sdk.*resetters[context.sdkIndex])();
        if (context.operation == 3)
            context.result = access.ReadScalar(context.field, context.output);
        if (context.operation == 4)
            context.result = access.SetWeatherScalarOverride(context.field, context.value);
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }

    void NativeWeatherOverrideOwnedSdk(ClockImageFixture& fixture)
    {
        namespace Scalars = NativeWeatherScalarProfile;
        namespace Placement = NativeWeatherOverridePlacementProfile;
        static_assert(NativeWeatherOverrideAtomicProfile::Windows.size() == 37);
        fixture.AdmitWeatherPins();
        fixture.AdmitWeatherScalarPins();
        fixture.AdmitRainPins();
        ClockReaderTestHost::image = fixture.image;
        using Code = std::array<std::array<unsigned char, 16>, NativeWeatherOverrideAtomicProfile::Windows.size()>;
        const auto code = [&]
        {
            Code result{};
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                const auto& window = NativeWeatherOverrideAtomicProfile::Windows[index];
                SIZE_T      copied = 0;
                Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + window.windowRva), result[index].data(),
                                          window.windowSize, &copied) &&
                          copied == window.windowSize,
                      "read the complete actual owned aligned code window without executing its native writer");
            }
            return result;
        };
        const auto stock = code();
        const auto accumulator = [&](Scalars::Field field)
        {
            Placement::Placement placement;
            Check(Placement::Placement::Prepare({fixture.image}, field, placement) == ClockResult::Ok,
                  "the real SDK override field has an independently constrained native accumulator placement");
            std::uintptr_t selected = 0;
            std::size_t    writers = 0;
            for (const auto& writer : NativeWeatherOverrideProfile::Writers)
            {
                if (writer.field != field)
                    continue;
                const auto   bytes = fixture.Get<std::array<unsigned char, 10>>(writer.rva);
                std::int32_t displacement = 0;
                std::memcpy(&displacement, bytes.data() + writer.displacement, sizeof(displacement));
                const auto target = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(fixture.image + writer.rva + writer.size) + displacement);
                MEMORY_BASIC_INFORMATION info{};
                Check(VirtualQuery(reinterpret_cast<const void*>(target), &info, sizeof(info)) == sizeof(info) && info.Type == MEM_PRIVATE &&
                          info.State == MEM_COMMIT && (info.Protect & 0xff) == PAGE_READWRITE && runtime.weatherOverridePlatform.ScratchValid(target),
                      "actual native writer operands resolve to retained private RW accumulator slots rather than executable padding or native storage");
                const Placement::Candidate candidate{reinterpret_cast<std::uintptr_t>(info.AllocationBase), target};
                Check(placement.Accepts(candidate) && candidate.allocation != candidate.accumulator && (!selected || selected == target),
                      "every writer in the native group shares one congruent interior accumulator with its separate real allocation base");
                const auto* window = NativeWeatherOverrideAtomicProfile::Find(writer.rva);
                Check(window && window->windowSize == 8 && window->windowRva % 8 == 0,
                      "all thirty active native writer changes use one complete aligned64 replacement window");
                for (std::size_t index = 0; index < writer.size; ++index)
                    if (writer.rva + index < window->windowRva || writer.rva + index >= window->windowRva + window->windowSize)
                        Check(bytes[index] == writer.original[index],
                              "constrained placement preserves the actual instruction prefix and low displacement bytes outside its atomic word");
                selected = target;
                ++writers;
            }
            Check(selected && writers == NativeWeatherOverrideProfile::WriterCounts[static_cast<std::size_t>(field)],
                  "the actual SDK owner redirects the complete proved native writer group, including all fifteen WetRoads sites");
            return selected;
        };
        const auto bits = [](float value)
        {
            std::uint32_t result = 0;
            std::memcpy(&result, &value, sizeof(result));
            return result;
        };
        const auto refused = [&]
        {
            return runtime.access.SetWeatherScalarOverride(Scalars::Field::Fog, 0.5f) != ClockResult::Ok &&
                   runtime.access.ResetWeatherScalarOverride(Scalars::Field::Fog) != ClockResult::Ok;
        };
        Check(refused() && !WeatherOverrideRetained() && code() == stock,
              "a complete stock weather profile cannot grant override authority to an unbound producer");
        NativeClockService     service;
        WeatherOverrideContext context;
        BeginOwned(service, fixture, false, true, true, &WeatherOverrideHandler, &context);
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew only the actual weather override callback generation");
            fixture.invokeTimer();
        };
        for (unsigned index = 0; index < 7; ++index)
        {
            context.sdkIndex = index;
            context.field = static_cast<Scalars::Field>(index + 1);
            context.operation = 1;
            context.value = 0.25f + static_cast<float>(index);
            const auto before = fixture.Get<std::uint32_t>(Scalars::Fields[index + 1]);
            const bool admitted = runtime.weatherOverrides.Admits(context.field);
            Check(admitted, "congruent aligned64 placement admits every one of the seven complete non-Rain SDK groups without a CPU128 dependency");
            invoke();
            Check(context.sdkResult == admitted && fixture.Get<std::uint32_t>(Scalars::Fields[index + 1]) == (admitted ? bits(context.value) : before) &&
                      runtime.weatherOverrides.HasReferences() == admitted && !runtime.weatherOverrides.NeedsRecovery(),
                  "all seven actual SDK setters report the concrete aligned atomic capability and commit only admitted native scalar intent");
            const auto  target = accumulator(context.field);
            const float nativeWriterValue = -11.5f;
            SIZE_T      written = 0;
            Check(::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(target), &nativeWriterValue, sizeof(nativeWriterValue), &written) &&
                      written == sizeof(nativeWriterValue) && fixture.Get<std::uint32_t>(Scalars::Fields[index + 1]) == bits(context.value),
                  "a simulated native producer store through the real redirected target cannot overwrite the accepted SDK scalar intent");
            const auto redirected = code();
            context.value += 0.125f;
            invoke();
            Check(context.sdkResult == admitted && code() == redirected &&
                      fixture.Get<std::uint32_t>(Scalars::Fields[index + 1]) == (admitted ? bits(context.value) : before),
                  "a repeated accepted setter reuses its retained native writer targets while unsupported groups preserve every instruction");
            context.operation = 3;
            context.output = 123.0f;
            invoke();
            Check(context.result == ClockResult::Ok && bits(context.output) == (admitted ? bits(context.value) : before),
                  "the existing scalar getter revalidates exact same-generation redirected code through the real owner CodeView");
            context.operation = 2;
            invoke();
            Check(context.sdkResult == admitted && !WeatherOverrideRetained() && code() == stock &&
                      fixture.Get<std::uint32_t>(Scalars::Fields[index + 1]) == (admitted ? bits(context.value) : before) &&
                      !runtime.weatherOverridePlatform.ScratchValid(target),
                  "all seven actual SDK resets restore only admitted writers, preserve successful scalar values and expose unsupported groups honestly");
        }
        Check(refused() && !WeatherOverrideRetained() && code() == stock,
              "the adopted producer cannot borrow native code/scalar override authority between callbacks");
        bool        foreignRefused = false;
        std::thread foreign([&] { foreignRefused = refused(); });
        foreign.join();
        Check(foreignRefused && code() == stock, "a foreign thread cannot borrow retained native weather writer authority");
        context.sdkIndex = 2;
        context.field = Scalars::Field::Fog;
        const auto persistentFog = fixture.Get<std::uint32_t>(Scalars::Fields[3]);
        for (unsigned refusal = 1; refusal <= 4; ++refusal)
            for (unsigned operation = 1; operation <= 2; ++operation)
            {
                context.refusal = refusal;
                context.operation = operation;
                invoke();
                Check(!context.sdkResult && !WeatherOverrideRetained() && code() == stock && fixture.Get<std::uint32_t>(Scalars::Fields[3]) == persistentFog,
                      "actual bool SDK methods reject stale generations, mismatched admission, nesting and expiry without code or scalar mutation");
            }
        context.refusal = 0;
        context.operation = 4;
        context.field = static_cast<Scalars::Field>(255);
        invoke();
        Check(context.result == ClockResult::InvalidArgument && !WeatherOverrideRetained() && code() == stock,
              "an unknown weather field cannot select an arbitrary writable native scalar or code plan");
        std::array<std::uintptr_t, 7> concurrentTargets{};
        std::array<std::uint32_t, 7>  concurrentValues{};
        for (unsigned index = 0; index < concurrentTargets.size(); ++index)
        {
            context.sdkIndex = index;
            context.field = static_cast<Scalars::Field>(index + 1);
            context.operation = 1;
            context.value = 12.25f + static_cast<float>(index);
            invoke();
            Check(context.sdkResult && runtime.weatherOverrides.HasReferences() && !runtime.weatherOverrides.NeedsRecovery(),
                  "the same genuine callback owner can retain all seven native weather writer groups concurrently");
            concurrentTargets[index] = accumulator(context.field);
            concurrentValues[index] = bits(context.value);
            for (unsigned prior = 0; prior < index; ++prior)
                Check(concurrentTargets[index] != concurrentTargets[prior] &&
                          (concurrentTargets[index] & ~std::uintptr_t{0xffff}) != (concurrentTargets[prior] & ~std::uintptr_t{0xffff}),
                      "concurrent native groups retain independent real allocation bases even when their native low offsets share a page");
        }
        for (unsigned index = 0; index < concurrentTargets.size(); ++index)
        {
            context.field = static_cast<Scalars::Field>(index + 1);
            context.operation = 3;
            invoke();
            Check(context.result == ClockResult::Ok && bits(context.output) == concurrentValues[index],
                  "actual SDK scalar reads qualify the combined same-generation code view with all thirty writer redirections active");
        }
        Check(!service.End() && service.IsRetained(), "End retains all concurrent native writer and slot references until the existing callback restores them");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !WeatherOverrideRetained() && code() == stock,
              "one original engine callback retires every concurrent writer group and its separate allocation before releasing the hooks");
        for (unsigned index = 0; index < concurrentTargets.size(); ++index)
            Check(!runtime.weatherOverridePlatform.ScratchValid(concurrentTargets[index]) &&
                      fixture.Get<std::uint32_t>(Scalars::Fields[index + 1]) == concurrentValues[index],
                  "concurrent End releases owned allocation bases while preserving all seven successful scalar intents");
        BeginOwned(service, fixture, false, true, true, &WeatherOverrideHandler, &context);
        context.sdkIndex = 2;
        context.field = Scalars::Field::Fog;
        context.operation = 1;
        context.value = 0.875f;
        invoke();
        Check(context.sdkResult && runtime.weatherOverrides.HasReferences(), "retain a real supported Fog override for the native End/recovery lifetime check");
        const auto  redirected = code();
        std::size_t selected = 0;
        while (selected < NativeWeatherOverrideAtomicProfile::Windows.size() &&
               NativeWeatherOverrideAtomicProfile::Windows[selected].field != Scalars::Field::Fog)
            ++selected;
        Check(selected < NativeWeatherOverrideAtomicProfile::Windows.size(), "the actual atomic profile supplies the independently proved Fog writer window");
        const auto& window = NativeWeatherOverrideAtomicProfile::Windows[selected];
        const auto  originalPrefix = fixture.Get<unsigned char>(window.writerRva);
        const auto  changed = static_cast<unsigned char>(originalPrefix ^ 1);
        fixture.Patch(window.writerRva, &changed, sizeof(changed));
        context.operation = 2;
        invoke();
        const auto debt = State(service);
        Check(!context.sdkResult && runtime.weatherOverrides.HasReferences() && runtime.weatherOverrides.NeedsRecovery() && debt.dataRecoveryPending &&
                  !debt.restored && !service.End() && service.IsRetained() && runtime.timingHandler && runtime.timingContext,
              "foreign instruction bytes preserve scratch/code references and the original native callback lifetime instead of claiming Reset success");
        Check(!RestoreWeatherOverrides(), "a producer cannot reclaim native writer references outside their original engine callback");
        fixture.Patch(window.windowRva, redirected[selected].data(), window.windowSize);
        // The full writer can begin before its aligned operand window. Repair
        // the fixture's foreign opcode too; restoring only the CAS word must
        // never masquerade as recovery of a still-corrupted instruction.
        fixture.Patch(window.writerRva, &originalPrefix, sizeof(originalPrefix));
        const auto generation = runtime.weatherOverrideGeneration.load();
        runtime.weatherOverrideGeneration.store(generation + 1);
        fixture.invokeTimer();
        Check(WeatherOverrideRetained() && !service.End(), "a real callback cannot retire weather code references assigned to a different native generation");
        runtime.weatherOverrideGeneration.store(generation);
        const auto calls = context.calls;
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !WeatherOverrideRetained() && !runtime.weatherOverrideGeneration.load() && !runtime.hooked.load() &&
                  !runtime.timerHooked.load() && !runtime.timingHandler && !runtime.timingContext && context.calls == calls && code() == stock &&
                  fixture.Get<std::uint32_t>(Scalars::Fields[3]) == bits(context.value),
              "only the matching natural owner restores all exact writers before releasing hooks/context, while accepted Fog intent persists");
        ClockReaderTestHost::image = 0;
        std::puts("Production native weather atomic override SDK owner fixture completed");
        std::puts("Production native seven-group weather placement owner fixture completed");
    }

    struct ApplicationFrameContext
    {
        unsigned               calls = 0, originalCountAtLastHandler = 0;
        NativeApplicationFrame last;
        NativeClockService*    service = nullptr;
        bool                   endInHandler = false, endResult = true, throwInHandler = false;
        unsigned               uiMode = 0;
        ClockImageFixture*     fixture = nullptr;
    };

    ApplicationFrameFunction countedFrame = nullptr;
    unsigned                 originalFrameInvocations = 0;
    void*                    lastFrameReceiver = nullptr;
    float                    lastFrameDelta = 0;
    HANDLE                   frameOriginalEntered = nullptr, frameOriginalRelease = nullptr;
    ClockImageFixture*       mutateFrameRoot = nullptr;
    NativeClockService*      originalUiService = nullptr;
    unsigned                 originalUiRefusals = 0;

    void CountOriginalFrame(void* receiver, float delta)
    {
        ++originalFrameInvocations;
        lastFrameReceiver = receiver;
        lastFrameDelta = delta;
        if (originalUiService)
        {
            NativeUiClassMapAccess::Value value{0xa5a5, true};
            Check(!frameCallbackGeneration && !frameCallbackReceiver &&
                      !originalUiService->ReadStartupUiClassMap(runtime.generation.load(), NativeUiClassMapAccess::Key::Loading, value) &&
                      value.classAddress == 0xa5a5 && value.keyPresent,
                  "even the engine owner cannot read the UI map during the original frame or an unqualified origin");
            NativeStartupProvidersAccess::Value providers{0xa5, 0x5a, 0x33};
            Check(!originalUiService->ReadStartupProviders(runtime.generation.load(), providers) && providers.controllerAddress == 0xa5 &&
                      providers.viewportAddress == 0x5a && providers.managerAddress == 0x33,
                  "the original frame and unqualified origins cannot borrow provider scope before the post-original admission");
            ++originalUiRefusals;
        }
        if (frameOriginalEntered)
        {
            SetEvent(frameOriginalEntered);
            Check(WaitForSingleObject(frameOriginalRelease, 5000) == WAIT_OBJECT_0,
                  "release the owned original application callback within a bounded fixture wait");
        }
        countedFrame(receiver, delta);
        if (mutateFrameRoot)
            mutateFrameRoot->Put(NativeApplicationFrameProfile::rootRva, std::uintptr_t{0});
    }

    void ObserveApplicationFrame(const NativeApplicationFrame& frame, void* opaque)
    {
        auto& context = *static_cast<ApplicationFrameContext*>(opaque);
        ++context.calls;
        context.originalCountAtLastHandler = originalFrameInvocations;
        context.last = frame;
        Check(callbackDepth == 1 && runtime.inFlight.load() == 1 && runtime.engineThread.load() == GetCurrentThreadId() &&
                  frame.generation == runtime.generation.load() && !timingCallbackGeneration && !timingTimerScope,
              "only an outermost real application callback observes raw state after original execution without timing-writer scope");
        BYTE       hour = 0xa5, minute = 0x5a;
        const auto beforeWrites = ClockTestFaults::clockDataWrites.load();
        Check(runtime.clock.TryGet(&hour, &minute) == ClockResult::NotInitialized && hour == 0xa5 && minute == 0x5a &&
                  runtime.clock.TrySet(1, 2) == ClockResult::NotInitialized && runtime.access.SetTimeScale(2) == ClockResult::NotInitialized &&
                  runtime.access.SetMinuteDuration(2000) == ClockResult::NotInitialized && runtime.access.Pause(true) == ClockResult::NotInitialized &&
                  runtime.access.StartGame() == ClockResult::NotInitialized && ClockTestFaults::clockDataWrites.load() == beforeWrites &&
                  !runtime.captured.load() && !runtime.timingCaptured.load() && !runtime.timingDirty.load() && !runtime.patchInstalled.load(),
              "observer readiness cannot capture CClock, enqueue its writes or borrow any native timing/pause capability");
        if (context.uiMode)
        {
            namespace Ui = NativeUiClassMapProfile;
            using Key = NativeUiClassMapAccess::Key;
            if (context.uiMode == 10)
            {
                // Each object and registry item is owned synthetic memory. The
                // test exercises real callback/page/profile guards without a
                // GTA process, UObject constructor, GC claim or native call.
                namespace Owner = NativeStartupOwnerProfile;
                namespace World = NativeStartupWorldProfile;
                alignas(8) std::array<unsigned char, 0x40>            uiBase{}, gameBase{}, levelBase{}, uiClass{}, gameClass{}, levelClass{};
                alignas(8) std::array<unsigned char, 0x1d0>           world{};
                alignas(8) std::array<unsigned char, 0x2c8>           game{};
                alignas(8) std::array<unsigned char, 0xd8>            level{};
                std::array<std::uintptr_t, 3>                         uiChain{}, gameChain{}, levelChain{};
                std::array<unsigned char, World::objectItemBytes * 2> item{};
                const auto address = [](const auto& value) { return reinterpret_cast<std::uintptr_t>(value.data()); };
                const auto put = [](auto& target, std::size_t offset, const auto& value) { std::memcpy(target.data() + offset, &value, sizeof(value)); };
                const auto type = [&](auto& base, auto& candidate, auto& chain)
                {
                    put(base, Ui::classChainIndexOffset, std::int32_t{1});
                    chain[1] = address(base) + Ui::classChainOffset;
                    put(candidate, Ui::classChainOffset, address(chain));
                    put(candidate, Ui::classChainIndexOffset, std::int32_t{2});
                };
                type(uiBase, uiClass, uiChain);
                type(gameBase, gameClass, gameChain);
                type(levelBase, levelClass, levelChain);
                const auto root = reinterpret_cast<std::uintptr_t>(context.fixture->frameOwner.data());
                const auto gameAddress = address(game), levelAddress = address(level);
                std::memcpy(reinterpret_cast<unsigned char*>(root) + Owner::objectOuterOffset, &gameAddress, sizeof(gameAddress));
                put(game, 0, context.fixture->image + Owner::gameModeVtableRva);
                put(game, Owner::objectClassOffset, address(gameClass));
                put(game, Owner::objectOuterOffset, levelAddress);
                put(game, Owner::gameModeInterfaceOffset, root);
                put(level, World::objectClassOffset, address(levelClass));
                put(level, World::levelWorldOffset, address(world));
                put(item, World::itemObjectOffset, levelAddress);
                std::array<std::uintptr_t, 1> chunks{address(item)};
                context.fixture->Put(Ui::uiTransitionClassCacheRva, address(uiBase));
                context.fixture->Put(Owner::gameModeClassCacheRva, address(gameBase));
                context.fixture->Put(World::levelClassCacheRva, address(levelBase));
                context.fixture->Put(World::objectChunksRva, address(chunks));
                context.fixture->Put(World::objectCountRva, std::int32_t{1});
                put(context.fixture->uiEntries, Ui::valueOffset, address(uiClass));
                put(context.fixture->uiEntries, Ui::entryBytes + Ui::valueOffset, address(uiClass));
                std::uint32_t facts = 0xa5, refused = 0x5a;
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 63 && !refused,
                      "the actual native callback observes typed UI classes, enclosing owner and conservative world link without invoking providers");
                {
                    // The extra links belong to the same synthetic world and
                    // registry as the existing owner fixture. A separate fake
                    // reader could miss the actual native callback gate and
                    // the world/owner bookends around the provider observation.
                    namespace Providers = NativeStartupProvidersProfile;
                    alignas(8) std::array<unsigned char, 0x40>  controller{}, controllerBase{}, controllerClass{}, viewportBase{}, viewportClass{};
                    alignas(8) std::array<unsigned char, 0x788> engine{};
                    alignas(8) std::array<unsigned char, 0x368> viewport{};
                    alignas(8) std::array<unsigned char, 16>    manager{};
                    std::array<std::uintptr_t, 3>               controllerChain{}, viewportChain{};
                    std::array<unsigned char, 8>                firstController{};
                    type(controllerBase, controllerClass, controllerChain);
                    type(viewportBase, viewportClass, viewportChain);
                    put(controller, Providers::objectClassOffset, address(controllerClass));
                    put(viewport, Providers::objectClassOffset, address(viewportClass));
                    put(viewport, Providers::viewportManagerOffset, address(manager));
                    put(engine, Providers::engineViewportOffset, address(viewport));
                    put(world, Providers::worldControllerDataOffset, address(firstController));
                    put(world, Providers::worldControllerCountOffset, std::int32_t{1});
                    put(firstController, Providers::weakIndexOffset, std::int32_t{1});
                    put(firstController, Providers::weakSerialOffset, std::uint32_t{17});
                    put(item, World::objectItemBytes, address(controller));
                    put(item, World::objectItemBytes + Providers::itemFlagsOffset, std::uint32_t{0});
                    put(item, World::objectItemBytes + Providers::itemSerialOffset, std::uint32_t{17});
                    context.fixture->Put(World::objectCountRva, std::int32_t{2});
                    context.fixture->Put(Providers::playerControllerClassCacheRva, address(controllerBase));
                    context.fixture->Put(Providers::viewportClassCacheRva, address(viewportBase));
                    context.fixture->Put(Providers::engineRva, address(engine));
                    NativeStartupProvidersAccess::Value provider;
                    const auto                          beforeProviderWrites = ClockTestFaults::clockDataWrites.load();
                    Check(context.service->ReadStartupProviders(frame.generation, provider) && provider.controllerAddress == address(controller) &&
                              provider.viewportAddress == address(viewport) && provider.managerAddress == address(manager),
                          "the real frame owner observes a typed first controller and typed viewport with an already cached manager");
                    provider = {0xa5, 0x5a, 0x33};
                    const auto preserved = [&]() noexcept
                    { return provider.controllerAddress == 0xa5 && provider.viewportAddress == 0x5a && provider.managerAddress == 0x33; };
                    Check(!context.service->ReadStartupProviders(frame.generation + 1, provider) && preserved(),
                          "another generation cannot borrow the provider graph or alter any caller output");
                    bool        foreignProvider = true;
                    std::thread foreign([&] { foreignProvider = context.service->ReadStartupProviders(frame.generation, provider); });
                    foreign.join();
                    Check(!foreignProvider && preserved(), "a foreign thread cannot reuse a live callback's provider scope");
                    ++callbackDepth;
                    const bool nestedProvider = context.service->ReadStartupProviders(frame.generation, provider);
                    --callbackDepth;
                    Check(!nestedProvider && preserved(), "nested scope refuses providers without replacing the actual outer owner's output");
                    const auto priorStopping = runtime.stopping.exchange(true);
                    const bool stoppingProvider = context.service->ReadStartupProviders(frame.generation, provider);
                    runtime.stopping.store(priorStopping);
                    Check(!stoppingProvider && preserved(), "stopping native ownership cannot publish previously observed provider pointers");
                    put(viewport, Providers::viewportManagerOffset, std::uintptr_t{0});
                    Check(!context.service->ReadStartupProviders(frame.generation, provider) && preserved(),
                          "a missing manager refuses the provider observation instead of invoking native lazy initialization");
                    put(viewport, Providers::viewportManagerOffset, address(manager));
                    put(item, World::objectItemBytes + Providers::itemSerialOffset, std::uint32_t{18});
                    Check(!context.service->ReadStartupProviders(frame.generation, provider) && preserved(),
                          "a recycled weak controller serial refuses all output even when its registry index and pointer remain readable");
                    put(item, World::objectItemBytes + Providers::itemSerialOffset, std::uint32_t{17});
                    const auto& providerPin = Providers::pins[0];
                    const auto  changedProviderPin = static_cast<unsigned char>(providerPin.bytes[0] ^ 1);
                    context.fixture->Patch(providerPin.rva, &changedProviderPin, sizeof(changedProviderPin));
                    Check(!context.service->ReadStartupProviders(frame.generation, provider) && preserved(),
                          "a changed provider dataflow pin refuses the scoped read without publishing a partial provider record");
                    context.fixture->Patch(providerPin.rva, providerPin.bytes, providerPin.size);
                    ClockTestFaults::uiReadAddress = address(controller) + Providers::objectClassOffset;
                    ClockTestFaults::uiRootChangeAddress = address(level) + World::levelWorldOffset;
                    Check(!context.service->ReadStartupProviders(frame.generation, provider) && preserved(),
                          "a world link moving during the provider probe cannot survive the real owner's final world bookend");
                    ClockTestFaults::uiReadAddress = ClockTestFaults::uiRootChangeAddress = 0;
                    put(level, World::levelWorldOffset, address(world));
                    Check(context.service->ReadStartupProviders(frame.generation, provider) && provider.controllerAddress == address(controller) &&
                              provider.viewportAddress == address(viewport) && provider.managerAddress == address(manager) &&
                              ClockTestFaults::clockDataWrites.load() == beforeProviderWrites,
                          "restored synthetic links recover read-only provider observations without any clock or provider initialization write");
                    // Do not leave image caches, a registry item or the world
                    // holding addresses into this nested fixture's stack.
                    context.fixture->Put(Providers::playerControllerClassCacheRva, std::uintptr_t{0});
                    context.fixture->Put(Providers::viewportClassCacheRva, std::uintptr_t{0});
                    context.fixture->Put(Providers::engineRva, std::uintptr_t{0});
                    context.fixture->Put(World::objectCountRva, std::int32_t{1});
                    put(world, Providers::worldControllerDataOffset, std::uintptr_t{0});
                    put(world, Providers::worldControllerCountOffset, std::int32_t{0});
                    std::memset(item.data() + World::objectItemBytes, 0, World::objectItemBytes);
                }
                uiChain[1] = address(uiBase);
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 53 && !refused,
                      "stable unrelated UI classes remain observed negatives rather than read refusals");
                uiChain[1] = address(uiBase) + Ui::classChainOffset;
                put(level, World::levelWorldOffset, std::uintptr_t{0});
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 31 && refused == StartupWorldLinkRefused,
                      "a missing world preserves independent UI and owner observations while refusing only the world prerequisite");
                put(level, World::levelWorldOffset, address(world));
                put(game, Owner::gameModeInterfaceOffset, std::uintptr_t{0});
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 15 && refused == 12,
                      "a broken enclosing-owner correlation refuses both owner-dependent probes without hiding independent UI facts");
                put(game, Owner::gameModeInterfaceOffset, root);
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 48 && refused == 3,
                      "a missing UI class cache is a refusal and cannot run native StaticClass initialization");
                context.fixture->Put(Ui::uiTransitionClassCacheRva, address(uiBase));
                facts = 0xa5;
                refused = 0x5a;
                Check(!context.service->ReadStartupPrerequisites(frame.generation + 1, facts, refused) && facts == 0xa5 && refused == 0x5a,
                      "a different generation cannot borrow startup observations or modify either caller output");
                bool        foreign = true;
                std::thread other([&] { foreign = context.service->ReadStartupPrerequisites(frame.generation, facts, refused); });
                other.join();
                Check(!foreign && facts == 0xa5 && refused == 0x5a, "a foreign thread cannot borrow the native startup callback scope");
                const auto& pin = Owner::pins[0];
                const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
                context.fixture->Patch(pin.rva, &changed, sizeof(changed));
                Check(!context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 0xa5 && refused == 0x5a,
                      "changed owner proof refuses the whole sample before publishing partial facts");
                context.fixture->Patch(pin.rva, pin.bytes, pin.size);
                ClockTestFaults::uiReadAddress = reinterpret_cast<std::uintptr_t>(context.fixture->frameOwner.data()) + Ui::ownerOffset;
                ClockTestFaults::uiRootChangeAddress = context.fixture->image + NativeApplicationFrameProfile::rootRva;
                Check(!context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 0xa5 && refused == 0x5a,
                      "root movement during an independent child probe cannot publish a mixed startup sample");
                ClockTestFaults::uiReadAddress = ClockTestFaults::uiRootChangeAddress = 0;
                context.fixture->SetFrameReceiver(context.fixture->frameOwner.data());
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                context.fixture->Put(Owner::gameModeClassCacheRva, std::uintptr_t{0});
                context.fixture->Put(World::levelClassCacheRva, std::uintptr_t{0});
                context.fixture->Put(World::objectChunksRva, std::uintptr_t{0});
                context.fixture->Put(World::objectCountRva, std::int32_t{0});
                put(context.fixture->uiEntries, Ui::valueOffset, std::uintptr_t{0});
                put(context.fixture->uiEntries, Ui::entryBytes + Ui::valueOffset, std::uintptr_t{0});
                // Do not leave the fixture root pointing at expired local records.
                const std::uintptr_t absent = 0;
                std::memcpy(reinterpret_cast<unsigned char*>(root) + Owner::objectOuterOffset, &absent, sizeof(absent));
                return;
            }
            if (context.uiMode == 9)
            {
                // The image and these class records belong only to this host.
                // Exercise the production callback gate as well as native
                // embedded-chain tokens without constructing any UObject.
                alignas(8) std::array<unsigned char, 0x40> base{}, candidate{};
                std::array<std::uintptr_t, 3>              chain{};
                const auto put = [](unsigned char* target, std::size_t offset, const auto& field) { std::memcpy(target + offset, &field, sizeof(field)); };
                const auto baseAddress = reinterpret_cast<std::uintptr_t>(base.data());
                const auto candidateAddress = reinterpret_cast<std::uintptr_t>(candidate.data());
                chain[1] = baseAddress + Ui::classChainOffset;
                put(base.data(), Ui::classChainIndexOffset, std::int32_t{1});
                put(candidate.data(), Ui::classChainOffset, reinterpret_cast<std::uintptr_t>(chain.data()));
                put(candidate.data(), Ui::classChainIndexOffset, std::int32_t{2});
                put(context.fixture->uiEntries.data(), Ui::valueOffset, candidateAddress);
                put(context.fixture->uiEntries.data() + Ui::entryBytes, Ui::valueOffset, candidateAddress);
                bool compatible = true;
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                Check(!context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && compatible,
                      "an uninitialized class cache cannot call StaticClass or invent a negative type result");
                context.fixture->Put(Ui::uiTransitionClassCacheRva, baseAddress);
                Check(context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && compatible &&
                          context.service->ReadStartupUiTransitionClass(frame.generation, Key::SimpleFade, compatible) && compatible,
                      "both configured classes use the actual native embedded base-chain relation on the qualified callback");
                Check(!context.service->ReadStartupUiTransitionClass(frame.generation + 1, Key::Loading, compatible) && compatible,
                      "type observations cannot borrow a different generation");
                bool        foreign = true;
                std::thread other([&] { foreign = context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible); });
                other.join();
                Check(!foreign && compatible, "type observations cannot borrow the frame scope from another thread");
                chain[1] = baseAddress;
                Check(context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && !compatible,
                      "a stable unrelated class is distinct from a failed observation and never matches the class pointer itself");
                compatible = true;
                put(context.fixture->uiEntries.data(), Ui::valueOffset, std::uintptr_t{0});
                Check(!context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && compatible,
                      "a present dictionary key with no class preserves type output instead of becoming a negative relation");
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                return;
            }
            NativeUiClassMapAccess::Value value{0xa5a5, true};
            Check(frameCallbackGeneration == frame.generation && frameCallbackReceiver == context.fixture->frameOwner.data(),
                  "only the qualified post-original handler holds this receiver and generation for dictionary observations");
            const bool observed = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
            if (context.uiMode == 1 || context.uiMode == 7 || context.uiMode == 8)
            {
                Check(observed && value.keyPresent && value.classAddress == 0x12345678,
                      "the natural frame handler observes Loading as an opaque configured class without dereferencing it");
                Check(context.service->ReadStartupUiClassMap(frame.generation, Key::SimpleFade, value) && value.keyPresent && value.classAddress == 0x87654321,
                      "the qualified owner follows the same bounded dictionary chain for SimpleFade");
                value = {0xa5a5, true};
                Check(!context.service->ReadStartupUiClassMap(frame.generation + 1, Key::Loading, value) && value.classAddress == 0xa5a5 && value.keyPresent,
                      "a stale requested generation cannot borrow the current frame scope or change output");
                ++callbackDepth;
                const bool nested = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
                --callbackDepth;
                runtime.stopping.store(true);
                const bool stopping = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
                runtime.stopping.store(false);
                Check(!nested && !stopping && value.classAddress == 0xa5a5 && value.keyPresent,
                      "nested or stopping callbacks preserve the caller value instead of admitting dictionary access");
                const auto refreshed = runtime.refreshed.load();
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
                const bool expired = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
                runtime.refreshed.store(refreshed);
                Check(!expired && value.classAddress == 0xa5a5 && value.keyPresent,
                      "an expired lease cannot borrow an already-entered handler's dictionary scope");
                bool        foreign = true;
                std::thread other([&] { foreign = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value); });
                other.join();
                Check(!foreign && value.classAddress == 0xa5a5 && value.keyPresent, "another thread cannot borrow a live handler's receiver/generation scope");
                NativeClockService unrelated;
                Check(!unrelated.ReadStartupUiClassMap(frame.generation, Key::Loading, value) && value.classAddress == 0xa5a5 && value.keyPresent,
                      "a separate service object cannot borrow the current owner's dictionary generation");
                if (context.uiMode == 7)
                    OriginalRaises();
            }
            else if (context.uiMode == 2)
                Check(observed && !value.keyPresent && !value.classAddress, "a valid absent UI dictionary key is reported separately from read refusal");
            else if (context.uiMode == 3)
                Check(observed && value.keyPresent && !value.classAddress, "a matching key with null class identity remains distinct from absent membership");
            else
                Check(!observed && value.classAddress == 0xa5a5 && value.keyPresent,
                      "failed OS reads, root movement or changed dictionary pins preserve the complete caller value");
        }
        if (context.endInHandler)
            context.endResult = context.service->End();
        if (context.throwInHandler)
            throw std::runtime_error("owned observer failure");
    }

    void BeginApplicationOwned(NativeClockService& service, ClockImageFixture& fixture, ApplicationFrameContext& context, bool attachNow = true,
                               GameStartupHandler startupHandler = nullptr, void* startupContext = nullptr)
    {
        ClockServiceStateV1 rejected;
        const bool          admitted = startupHandler ? service.BeginGameStartupFrames({}, rejected, startupHandler, startupContext)
                                                      : service.BeginApplicationFrames({}, rejected, &ObserveApplicationFrame, &context);
        Check(!admitted && rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked && !rejected.modulePinned &&
                  rejected.generation && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "internal application Begin preserves the shipping wrong-executable refusal before any native hook or module pin");
        NativeServiceAdmission::Token claim;
        Check(NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Clock, claim),
              "application fixture owns the same exclusive setup/cleanup admission as other native transactions");
        runtime.admissionSerial.store(claim.serial);
        runtime.admissionGeneration.store(runtime.generation.load());
        runtime.image = fixture.image;
        runtime.failure.store(ClockResult::Ok);
        runtime.stopping.store(false);
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.started = GetTickCount64();
        runtime.refreshed.store(runtime.started);
        runtime.frameHandler = startupHandler ? nullptr : &ObserveApplicationFrame;
        runtime.startupMode = startupHandler != nullptr;
        runtime.startupHandler = startupHandler;
        runtime.frameContext = startupHandler ? startupContext : &context;
        runtime.originalFrame = reinterpret_cast<ApplicationFrameFunction>(fixture.image + NativeApplicationFrameProfile::entryRva);
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        fixture.Put(NativeApplicationFrameProfile::stateRva, std::uint32_t{7});
        context.service = &service;
        originalUiService = context.uiMode ? &service : nullptr;
        originalFrameInvocations = 0;
        lastFrameReceiver = nullptr;
        lastFrameDelta = 0;
        Check(runtime.frameMode && !runtime.timingMode && !runtime.timerHooked.load() && !runtime.hooked.load() && !runtime.ready.load() &&
                  !runtime.captured.load() && CheckBinding(true),
              "application-only admission validates its own exact pins without installing clock/timer hooks or capturing CClock");
        if (!attachNow)
            return;
        Check(ChangeHooks(true) && runtime.frameHooked.load() && !runtime.hooked.load() && !runtime.timerHooked.load(),
              "real Detours installs only the native application entry for the observer mode");
        countedFrame = runtime.originalFrame;
        runtime.originalFrame = &CountOriginalFrame;
    }

    void FinishApplicationOwned(NativeClockService& service)
    {
        runtime.originalFrame = countedFrame;
        Check(service.End() && !service.IsRetained() && !State(service).hooked && !runtime.frameHooked.load() && !runtime.hooked.load() &&
                  !runtime.timerHooked.load() && !runtime.frameHandler && !runtime.startupHandler && !runtime.startupMode && !runtime.frameContext &&
                  !runtime.inFlight.load() && !runtime.captured.load() && !runtime.timingCaptured.load() && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !timingCallbackGeneration && !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && CheckBinding(true),
              "an observer retires without a future clock tick after actual hook detach, callback completion and admission release");
        originalUiService = nullptr;
    }

    void FrameOriginalRaises(void* receiver, float delta)
    {
        ++originalFrameInvocations;
        lastFrameReceiver = receiver;
        lastFrameDelta = delta;
        OriginalRaises();
    }

    bool InvokeFrameOriginalException(void* receiver)
    {
        // Deliberate exceptions cross compiler-owned frames only, as the
        // synthetic native caller intentionally has no unwind registration.
        __try
        {
            HookApplicationFrame(receiver, 0.125f);
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    bool InvokeQualifiedHandlerException(void* receiver)
    {
        // Exercise the production handler's SEH cleanup using compiler-owned
        // frames; the synthetic caller intentionally has no invented unwind data.
        __try
        {
            FrameBody(receiver, 0.125f, reinterpret_cast<void*>(runtime.image + NativeApplicationFrameProfile::normalReturnRva), true);
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    void NativeStartupUiClassMapOwner(ClockImageFixture& fixture)
    {
        namespace Ui = NativeUiClassMapProfile;
        using Key = NativeUiClassMapAccess::Key;
        fixture.AdmitUiClassMapPins();
        fixture.AdmitStartupPins();
        for (unsigned mode = 1; mode != 11; ++mode)
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            context.uiMode = mode;
            context.fixture = &fixture;
            context.throwInHandler = mode == 8;
            fixture.SeedUiClassMap();
            BeginApplicationOwned(service, fixture, context);
            NativeUiClassMapAccess::Value outside{0xa5a5, true};
            Check(!service.ReadStartupUiClassMap(runtime.generation.load(), Key::Loading, outside) && outside.classAddress == 0xa5a5 && outside.keyPresent,
                  "producer-side lookup before a frame is refused even with a bound observer and the current generation");
            bool typeOutside = true;
            Check(!service.ReadStartupUiTransitionClass(runtime.generation.load(), Key::Loading, typeOutside) && typeOutside,
                  "producer-side class typing cannot acquire an engine callback scope");
            std::uint32_t startupFacts = 0xa5, startupRefusals = 0x5a;
            Check(
                !service.ReadStartupPrerequisites(runtime.generation.load(), startupFacts, startupRefusals) && startupFacts == 0xa5 && startupRefusals == 0x5a,
                "producer-side startup reads preserve both outputs outside the natural frame callback");
            NativeStartupProvidersAccess::Value providersOutside{0xa5, 0x5a, 0x33};
            if (mode == 10)
                Check(!service.ReadStartupProviders(runtime.generation.load(), providersOutside) && providersOutside.controllerAddress == 0xa5 &&
                          providersOutside.viewportAddress == 0x5a && providersOutside.managerAddress == 0x33,
                      "producer-side provider reads preserve all outputs before the actual native frame");
            if (mode == 1)
            {
                const auto before = originalUiRefusals;
                HookApplicationFrame(fixture.frameOwner.data(), 0.125f);
                fixture.invokeLoadingFrame(fixture.frameOwner.data(), 0.125f);
                Check(originalUiRefusals == before + 2 && !context.calls,
                      "unknown/loading origins forward once but cannot create a UI dictionary handler scope");
            }
            auto* map = reinterpret_cast<unsigned char*>(fixture.frameOwner.data()) + Ui::ownerOffset;
            if (mode == 2)
            {
                const std::int32_t missing = -1;
                std::memcpy(map + Ui::inlineHashOffset, &missing, sizeof(missing));
            }
            if (mode == 3)
            {
                const std::uintptr_t absent = 0;
                std::memcpy(fixture.uiEntries.data() + Ui::valueOffset, &absent, sizeof(absent));
            }
            ClockTestFaults::uiReadAddress = reinterpret_cast<std::uintptr_t>(map);
            if (mode == 4)
                ClockTestFaults::failUiRead.store(true);
            if (mode == 5)
                ClockTestFaults::uiRootChangeAddress = fixture.image + NativeApplicationFrameProfile::rootRva;
            const auto& proof = Ui::pins[0];
            const auto  changed = static_cast<unsigned char>(proof.bytes[0] ^ 1);
            if (mode == 6)
                fixture.Patch(proof.rva, &changed, sizeof(changed));
            if (mode == 7)
            {
                ++callbackDepth;
                runtime.inFlight.fetch_add(1);
                const bool caught = InvokeQualifiedHandlerException(fixture.frameOwner.data());
                runtime.inFlight.fetch_sub(1);
                --callbackDepth;
                Check(caught && !frameCallbackGeneration && !frameCallbackReceiver && State(service).failure == ClockResult::OriginalException,
                      "handler SEH retires both UI scope fields before propagating through compiler-owned frames");
            }
            else
                fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            if (mode == 8)
                Check(State(service).failure == ClockResult::OriginalException && !runtime.active.load(),
                      "C++ handler failures stop the owner after retiring the generation and receiver scope");
            ClockTestFaults::uiReadAddress = ClockTestFaults::uiRootChangeAddress = 0;
            ClockTestFaults::failUiRead.store(false);
            if (mode == 6)
                fixture.Patch(proof.rva, proof.bytes, proof.size);
            fixture.SetFrameReceiver(fixture.frameOwner.data());
            Check(context.calls == 1 && !frameCallbackGeneration && !frameCallbackReceiver &&
                      !service.ReadStartupUiClassMap(runtime.generation.load(), Key::Loading, outside) && outside.classAddress == 0xa5a5 && outside.keyPresent,
                  "normal/refused/exceptional handler completion cannot leak UI scope back to its producer");
            if (mode == 10)
                Check(!service.ReadStartupProviders(runtime.generation.load(), providersOutside) && providersOutside.controllerAddress == 0xa5 &&
                          providersOutside.viewportAddress == 0x5a && providersOutside.managerAddress == 0x33,
                      "a completed callback cannot leak provider scope or expired local fixture addresses back to its producer");
            FinishApplicationOwned(service);
        }
        std::puts("Production qualified startup UI dictionary fixture completed");
    }

    void NativeApplicationFrameOwner(ClockImageFixture& fixture)
    {
        namespace Frame = NativeApplicationFrameProfile;
        fixture.AdmitApplicationPins();
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            const auto              unrelatedClockByte = static_cast<unsigned char>(NativeClockProfile::clockCode[0] ^ 1);
            fixture.Patch(NativeClockProfile::clockCodeRva, &unrelatedClockByte, sizeof(unrelatedClockByte));
            BeginApplicationOwned(service, fixture, context);
            HookApplicationFrame(fixture.frameOwner.data(), 0.125f);
            fixture.invokeLoadingFrame(fixture.frameOwner.data(), 0.25f);
            Check(originalFrameInvocations == 2 && !context.calls && !runtime.engineThread.load() && !runtime.ready.load(),
                  "unknown and actual excluded loading origins preserve original ABI calls exactly once without adopting the observer owner");
            const auto writes = ClockTestFaults::clockDataWrites.load();
            const auto exchanges = ClockTestFaults::clockCodeExchanges.load();
            for (const std::uint32_t state : {7u, 9u})
            {
                fixture.Put(Frame::stateRva, state);
                const float delta = state == 7 ? 0.03125f : -0.0f;
                const auto  before = originalFrameInvocations;
                fixture.invokeFrame(fixture.frameOwner.data(), delta);
                Check(originalFrameInvocations == before + 1 && context.originalCountAtLastHandler == before + 1 &&
                          lastFrameReceiver == fixture.frameOwner.data() && std::memcmp(&lastFrameDelta, &delta, sizeof(delta)) == 0 &&
                          std::memcmp(&context.last.deltaSeconds, &delta, sizeof(delta)) == 0 && context.last.nativeState == state && State(service).ready &&
                          runtime.restored.load() && !runtime.captured.load() && !runtime.timerReady.load(),
                      "the exact pinned normal call preserves this and XMM1 bitwise and observes frontend/gameplay state only after its original once");
            }
            Check(context.calls == 2 && !service.Submit(Command(State(service), 1)) && !runtime.pendingSequence.load() && !runtime.hasPending &&
                      ClockTestFaults::clockDataWrites.load() == writes && ClockTestFaults::clockCodeExchanges.load() == exchanges,
                  "application observer dispatch never admits the public clock mailbox or changes native clock/timing data");
            fixture.frameOwner[0] += sizeof(std::uintptr_t);
            Check(!FrameReceiver(fixture.frameOwner.data()), "typed receiver qualification rejects another vtable without dereferencing any UObject child");
            fixture.frameOwner[0] -= sizeof(std::uintptr_t);
            FinishApplicationOwned(service);
            fixture.Patch(NativeClockProfile::clockCodeRva, NativeClockProfile::clockCode.data(), NativeClockProfile::clockCode.size());
        }
        for (unsigned refusal = 0; refusal != 7; ++refusal)
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            std::array<std::uintptr_t, 2> differentReceiver{fixture.frameOwner[0], 0};
            void*                         receiver = fixture.frameOwner.data();
            float                         delta = 0.0625f;
            if (refusal == 0)
                receiver = differentReceiver.data();
            if (refusal == 1)
                mutateFrameRoot = &fixture;
            if (refusal == 2)
                delta = (std::numeric_limits<float>::quiet_NaN)();
            if (refusal == 3)
                fixture.Put(Frame::stateRva, std::uint32_t{10});
            if (refusal == 4)
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
            if (refusal == 5)
                ++callbackDepth;
            const auto changed = static_cast<unsigned char>(Frame::rootPublication[0] ^ 1);
            if (refusal == 6)
                fixture.Patch(Frame::rootPublicationRva, &changed, sizeof(changed));
            fixture.invokeFrame(receiver, delta);
            if (refusal == 5)
                --callbackDepth;
            if (refusal == 6)
                fixture.Patch(Frame::rootPublicationRva, Frame::rootPublication.data(), Frame::rootPublication.size());
            mutateFrameRoot = nullptr;
            fixture.SetFrameReceiver(fixture.frameOwner.data());
            const auto failure = refusal == 4 ? ClockResult::Expired : refusal == 5 ? ClockResult::Busy : ClockResult::BindingChanged;
            Check(originalFrameInvocations == 1 && !context.calls && !runtime.ready.load() && State(service).failure == failure && !runtime.inFlight.load() &&
                      !callbackDepth && !timingCallbackGeneration && !timingTimerScope && !runtime.captured.load(),
                  "receiver mismatch/bookend, nonfinite delta, invalid raw state, expired lease, nesting or changed exact pins refuse after original once");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            std::thread foreign([&] { fixture.invokeFrame(fixture.frameOwner.data(), 0.25f); });
            foreign.join();
            Check(originalFrameInvocations == 2 && context.calls == 1 && State(service).failure == ClockResult::WrongThread &&
                      State(service).foreignCallbacks == 1 && !runtime.inFlight.load(),
                  "a foreign normal-origin callback preserves its original but cannot take over the established engine thread");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            context.endInHandler = true;
            BeginApplicationOwned(service, fixture, context);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            Check(context.calls == 1 && !context.endResult && service.IsRetained() && runtime.frameHooked.load() && !runtime.inFlight.load(),
                  "End inside an observer cannot discard its own live trampoline or handler context");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            context.throwInHandler = true;
            BeginApplicationOwned(service, fixture, context);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            Check(context.calls == 1 && originalFrameInvocations == 1 && State(service).failure == ClockResult::OriginalException && !runtime.active.load() &&
                      !runtime.inFlight.load() && !callbackDepth,
                  "client observer exceptions stop dispatch without unwinding into the native caller or leaking callback accounting");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            runtime.originalFrame = &FrameOriginalRaises;
            const bool caught = InvokeFrameOriginalException(fixture.frameOwner.data());
            runtime.originalFrame = &CountOriginalFrame;
            Check(caught && originalFrameInvocations == 1 && !context.calls && State(service).failure == ClockResult::OriginalException &&
                      !runtime.inFlight.load() && !callbackDepth && !timingCallbackGeneration && !timingTimerScope && service.IsRetained(),
                  "original native exceptions propagate through compiler-owned SEH while finally retains hook ownership and releases scopes");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            frameOriginalEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            frameOriginalRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(frameOriginalEntered && frameOriginalRelease, "create bounded application in-flight lifetime events");
            std::thread engine([&] { fixture.invokeFrame(fixture.frameOwner.data(), 0.125f); });
            Check(WaitForSingleObject(frameOriginalEntered, 5000) == WAIT_OBJECT_0 && runtime.inFlight.load() == 1 && !service.End() && service.IsRetained() &&
                      runtime.frameHooked.load() && runtime.frameHandler == &ObserveApplicationFrame,
                  "producer End retains the actual trampoline/context while another thread is inside the original native application call");
            SetEvent(frameOriginalRelease);
            engine.join();
            CloseHandle(frameOriginalEntered);
            CloseHandle(frameOriginalRelease);
            frameOriginalEntered = frameOriginalRelease = nullptr;
            Check(!context.calls && !runtime.inFlight.load(), "a stopped in-flight original finishes once without dispatching late client observation");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context, false);
            const auto stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(stop != nullptr, "create the owned observer-transaction peer lifetime");
            std::atomic<DWORD> peerId{0};
            std::thread        peer(
                [&]
                {
                    peerId.store(GetCurrentThreadId());
                    WaitForSingleObject(stop, INFINITE);
                });
            while (!peerId.load())
                SwitchToThread();
            ClockTestFaults::resumeTarget.store(peerId.load());
            ClockTestFaults::failResumes.store(64);
            Check(!ChangeHooks(true) && runtime.frameHooked.load() && State(service).retainedPeerThreads == 1 && service.IsRetained() && !service.End() &&
                      NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "committed application hook plus failed peer resume retains its actual hook and exclusive cleanup admission");
            ClockTestFaults::failResumes.store(0);
            countedFrame = runtime.originalFrame;
            FinishApplicationOwned(service);
            SetEvent(stop);
            peer.join();
            CloseHandle(stop);
        }
        NativeStartupUiClassMapOwner(fixture);
        std::puts("Production natural application-frame observer fixture completed");
    }

    class TimingBaseProbe : public CGameTimingDE
    {
    public:
        using CGameTimingDE::RequireFPS;
        using CGameTimingDE::RequireTiming;
    };
    static_assert(std::is_abstract_v<TimingBaseProbe> && std::is_base_of_v<CGame, TimingBaseProbe>);

    void NativeGameTimingPrerequisite(ClockImageFixture& fixture)
    {
        NativeGameTimingReader reader;
        NativeGameTimingValues values;
        std::memset(&values, 0x5a, sizeof(values));
        const auto untouched = values;
        const auto unchanged = [&] { return std::memcmp(&values, &untouched, sizeof(values)) == 0; };
        const auto refusesGetter = [&]
        {
            try
            {
                TimingBaseProbe::RequireTiming(reader);
                return false;
            }
            catch (const std::runtime_error& error)
            {
                return std::strstr(error.what(), "DE CGame timing unavailable") != nullptr;
            }
        };
        Check(reader.Read(values) == Status::NotInitialized && unchanged() && !reader.IsBound() && refusesGetter(),
              "the real CGame getter read boundary throws for unavailable timing instead of returning synthetic zero values");
        Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "native CGame timing preserves the shipping wrong-executable refusal");
        fixture.Seed();
        fixture.Put(NativeGameTimingProfile::frameCounterRva, std::uint32_t{0xfffffffe});
        fixture.Put(NativeGameTimingProfile::timeStepRva, 0.75f);
        fixture.Put(NativeGameTimingProfile::oldTimeStepRva, 0.625f);
        fixture.Put(NativeGameTimingProfile::timeScaleRva, 1.25f);
        ClockReaderTestHost::image = fixture.image;
        Check(reader.Bind() == Status::Ok && reader.IsBound(), "native timing binds genuine timer update/getter pins over the same owned native clock image");
        const auto writes = ClockTestFaults::clockDataWrites.load(), patches = ClockTestFaults::clockCodeExchanges.load();
        Check(reader.Read(values) == Status::Ok && values.clock.producerThread == GetCurrentThreadId() && values.clock.nativeTimer == 10000 &&
                  values.clock.millisecondsPerGameMinute == 1000 && values.frameCounter == 0xfffffffe && values.timeStep == 0.75f &&
                  values.oldTimeStep == 0.625f && values.timeScale == 1.25f,
              "matching complete samples preserve native counter bits and all independently corroborated timing values");
        fixture.Put(NativeGameTimingProfile::frameCounterRva, std::uint32_t{2});
        fixture.Put(NativeGameTimingProfile::timeStepRva, 0.5f);
        const auto fresh = TimingBaseProbe::RequireTiming(reader);
        Check(fresh.frameCounter == 2 && fresh.timeStep == 0.5f,
              "the actual CGame timing overrides' shared read path requests current native values instead of retaining a successful snapshot");
        std::thread foreign(
            [&]
            {
                NativeGameTimingValues output = untouched;
                Check(reader.Bind() == Status::WrongThread && reader.Read(output) == Status::WrongThread &&
                          std::memcmp(&output, &untouched, sizeof(output)) == 0 && refusesGetter(),
                      "foreign-thread native timing calls preserve output and the CGame getter boundary refuses access");
            });
        foreign.join();
        values = untouched;
        ClockReaderTestHost::timingSamples.store(0);
        ClockReaderTestHost::mismatchTimingSample.store(2);
        Check(reader.Read(values) == Status::Unsupported && unchanged() && ClockReaderTestHost::timingSamples.load() == 2,
              "a changed timing scalar between complete samples refuses the entire 64-byte native timing result");
        ClockReaderTestHost::mismatchTimingSample.store(0);
        for (const auto field : {NativeGameTimingProfile::timeStepRva, NativeGameTimingProfile::oldTimeStepRva, NativeGameTimingProfile::timeScaleRva})
        {
            fixture.Put(field, std::numeric_limits<float>::quiet_NaN());
            values = untouched;
            Check(reader.Read(values) == Status::Unsupported && unchanged() && refusesGetter(),
                  "each nonfinite native timing scalar refuses publication and cannot be hidden by the SDK getter boundary");
            fixture.Put(field, 1.0f);
        }
        fixture.Put(NativeGameTimingProfile::timeStepRva, 99.0f);
        fixture.Put(NativeGameTimingProfile::oldTimeStepRva, -1.0f);
        fixture.Put(NativeGameTimingProfile::timeScaleRva, -0.5f);
        Check(reader.Read(values) == Status::Ok && values.timeStep == 99.0f && values.oldTimeStep == -1.0f && values.timeScale == -0.5f,
              "finite native values are observed without inventing script-independent clamp or time-scale policies");
        auto changed = NativeGameTimingProfile::frameGetter;
        changed[0] ^= 1;
        fixture.Patch(NativeGameTimingProfile::frameGetterRva, changed.data(), changed.size());
        values = untouched;
        Check(reader.Read(values) == Status::Unsupported && unchanged() && refusesGetter(),
              "changed corroborating getter instructions revoke fresh timing reads after an earlier success");
        fixture.Patch(NativeGameTimingProfile::frameGetterRva, NativeGameTimingProfile::frameGetter.data(), NativeGameTimingProfile::frameGetter.size());
        DWORD previous = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameTimingProfile::oldTimeStepRva), sizeof(float), PAGE_NOACCESS, &previous) !=
                  FALSE,
              "deny native timing data only in the independently owned fixture image");
        values = untouched;
        Check(reader.Read(values) == Status::Unsupported && unchanged() && refusesGetter(),
              "a native timing page loss preserves output and never fabricates readable CGame state");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameTimingProfile::oldTimeStepRva), sizeof(float), previous, &ignored) != FALSE,
              "restore the fixture timing page after verified refusal");
        Check(reader.Read(values) == Status::Ok && ClockTestFaults::clockDataWrites.load() == writes && ClockTestFaults::clockCodeExchanges.load() == patches,
              "native CGame timing reads and failure recovery neither write time nor install or renew engine hooks");
        reader.Reset();
        values = untouched;
        Check(!reader.IsBound() && reader.Read(values) == Status::NotInitialized && unchanged() && refusesGetter(),
              "timing retirement clears the reader rather than keeping earlier values available to CGame getters");
        ClockReaderTestHost::image = 0;
        std::puts("Production CGame native timing prerequisite fixture completed");
    }

#include "NativeStartGameOwnerTests.inl"

    void NativeStoredFPSReader(ClockImageFixture& fixture)
    {
        NativeGameTimingReader reader;
        float                  value = 123.25f;
        const auto             refusesGetter = [&]
        {
            try
            {
                TimingBaseProbe::RequireFPS(reader);
                return false;
            }
            catch (const std::runtime_error& error)
            {
                return std::strstr(error.what(), "DE CGame FPS unavailable (status ") != nullptr;
            }
        };
        const auto refused = [&](Status expected)
        {
            value = 123.25f;
            return reader.ReadFPS(value) == expected && value == 123.25f;
        };
        Check(refused(Status::NotInitialized) && refusesGetter(), "unbound native FPS preserves output and the CGame getter explicitly refuses");
        Check(reader.Bind() == Status::Unsupported && refused(Status::NotInitialized), "native FPS cannot bypass the shipping executable gate");
        fixture.Seed();
        ClockReaderTestHost::image = fixture.image;
        Check(reader.Bind() == Status::Ok && reader.IsBound(), "FPS reuses the existing exact-image native timing binding and owner");
        const auto writes = ClockTestFaults::clockDataWrites.load(), patches = ClockTestFaults::clockCodeExchanges.load();
        ClockReaderTestHost::fpsSamples.store(0);
        Check(reader.ReadFPS(value) == Status::Ok && value == 62.5f && ClockReaderTestHost::fpsSamples.load() == 2 &&
                  TimingBaseProbe::RequireFPS(reader) == 62.5f,
              "the real CGame FPS getter boundary reads matching native FPS floats without constructing a partial CGame");
        fixture.Put(NativeGameFPSProfile::fpsRva, 37.25f);
        fixture.Put(NativeGameTimingProfile::timeStepRva, 99.0f);
        fixture.Put(NativeGameTimingProfile::timeScaleRva, -0.5f);
        Check(reader.ReadFPS(value) == Status::Ok && value == 37.25f,
              "FPS returns the fresh native field and cannot be replaced by a timestep or time-scale estimate");
        fixture.Put(NativeGameFPSProfile::fpsRva, 0.0f);
        Check(reader.ReadFPS(value) == Status::Ok && value == 0.0f, "a stored finite zero FPS remains a native value rather than an invented minimum");
        fixture.Put(NativeGameFPSProfile::fpsRva, 62.5f);
        std::thread foreign(
            [&] { Check(refused(Status::WrongThread) && refusesGetter(), "foreign FPS reads preserve output and the actual getter refusal policy"); });
        foreign.join();
        ClockReaderTestHost::fpsSamples.store(0);
        ClockReaderTestHost::mismatchFPSSample.store(2);
        Check(refused(Status::Unsupported) && ClockReaderTestHost::fpsSamples.load() == 2,
              "two unequal native FPS samples refuse publication rather than publishing a torn value");
        ClockReaderTestHost::mismatchFPSSample.store(0);
        for (unsigned failed = 1; failed <= 2; ++failed)
        {
            ClockReaderTestHost::fpsSamples.store(0);
            ClockReaderTestHost::failFPSSample.store(failed);
            Check(refused(Status::Unsupported), "failure of either native FPS read preserves the caller's output");
        }
        ClockReaderTestHost::failFPSSample.store(0);
        for (const auto invalid :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -1.0f})
        {
            fixture.Put(NativeGameFPSProfile::fpsRva, invalid);
            Check(refused(Status::Unsupported) && refusesGetter(), "nonfinite or negative stored FPS is refused by both reader and CGame getter boundary");
        }
        fixture.Put(NativeGameFPSProfile::fpsRva, 62.5f);
        const auto changedPin = [&](std::uintptr_t rva, const auto& original)
        {
            auto changed = original;
            changed[0] ^= 1;
            fixture.Patch(rva, changed.data(), changed.size());
            Check(refused(Status::Unsupported) && refusesGetter(), "a changed FPS computation, scale or named consumer pin revokes an earlier successful read");
            fixture.Patch(rva, original.data(), original.size());
        };
        changedPin(NativeGameFPSProfile::frequencyScaleRva, NativeGameFPSProfile::frequencyScale);
        changedPin(NativeGameFPSProfile::computationRva, NativeGameFPSProfile::computation);
        changedPin(NativeGameFPSProfile::consumerRva, NativeGameFPSProfile::consumer);
        changedPin(NativeGameFPSProfile::labelRva, NativeGameFPSProfile::label);
        const auto changedConstant = [&](std::uintptr_t rva, const auto& original)
        {
            auto changed = original + 1;
            fixture.Patch(rva, &changed, sizeof(changed));
            Check(refused(Status::Unsupported), "changing a native FPS arithmetic constant refuses the stored value's meaning");
            fixture.Patch(rva, &original, sizeof(original));
        };
        changedConstant(NativeGameFPSProfile::previousWeightRva, NativeGameFPSProfile::previousWeight);
        changedConstant(NativeGameFPSProfile::currentWeightRva, NativeGameFPSProfile::currentWeight);
        changedConstant(NativeGameFPSProfile::fpsNumeratorRva, NativeGameFPSProfile::fpsNumerator);
        changedConstant(NativeGameFPSProfile::millisecondsRva, NativeGameFPSProfile::milliseconds);
        changedConstant(NativeGameFPSProfile::secondsOriginRva, NativeGameFPSProfile::secondsOrigin);
        changedConstant(NativeGameFPSProfile::frequencyNumeratorRva, NativeGameFPSProfile::frequencyNumerator);
        for (const auto protection : {PAGE_NOACCESS, PAGE_EXECUTE_READWRITE})
        {
            DWORD previous = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::fpsRva), sizeof(float), protection, &previous) != FALSE,
                  "change only the owned fixture FPS data page's access policy");
            Check(refused(Status::Unsupported) && refusesGetter(),
                  "unreadable or executable native FPS data cannot satisfy writable non-executable qualification");
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::fpsRva), sizeof(float), previous, &ignored) != FALSE,
                  "restore the owned FPS data page after checked refusal");
        }
        DWORD previous = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::computationRva), NativeGameFPSProfile::computation.size(),
                               PAGE_READWRITE, &previous) != FALSE,
              "remove execute access only from the synthetic FPS computation page");
        Check(refused(Status::Unsupported), "a readable matching computation pin still requires an executable native image page");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::computationRva), NativeGameFPSProfile::computation.size(),
                               previous, &ignored) != FALSE,
              "restore synthetic FPS code page execute access");
        Check(reader.ReadFPS(value) == Status::Ok && value == 62.5f && ClockTestFaults::clockDataWrites.load() == writes &&
                  ClockTestFaults::clockCodeExchanges.load() == patches,
              "FPS failure recovery performs no engine writes, patch exchanges or lease operations");
        reader.Reset();
        Check(refused(Status::NotInitialized) && refusesGetter() && !reader.IsBound(),
              "retired native FPS access does not retain an earlier successful sample");
        ClockReaderTestHost::image = 0;
        std::puts("Production native stored FPS reader fixture completed");
    }

    void NativePatchTransactions(ClockImageFixture& fixture)
    {
        if (!NativeClockProfile::clockBoundaryReviewed)
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::UnsupportedBuild && !state.patchInstalled && !state.patchRecoveryPending && state.restored,
                  "an unreviewed branch boundary refuses actual freeze writes but still completes safe original-state cleanup");
            FinishOwned(service, fixture);
            std::puts("Native freeze patch capability: withheld by production review gate");
            return;
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            DWORD protection = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::patchAddress), 8, PAGE_EXECUTE_READWRITE, &protection) != FALSE,
                  "admit a direct atomic comparison test only on the owned code image");
            const auto original = fixture.Get<LONG64>(NativeClockProfile::patchRva - 3);
            Check(!ExchangeCode(original ^ 1, original ^ 2) && fixture.Get<LONG64>(NativeClockProfile::patchRva - 3) == original,
                  "the real aligned atomic code store refuses a changed expected instruction without writing any bytes");
            Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::patchAddress), 8, protection, &ignored) != FALSE,
                  "restore fixture code protection after comparison-refusal verification");
            MEMORY_BASIC_INFORMATION before{}, after{};
            VirtualQuery(reinterpret_cast<const void*>(ClockTestFaults::patchAddress), &before, sizeof(before));
            auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::Ok && state.frozen && state.patchInstalled && state.freezes == 1,
                  "SDK freeze installs the reviewed branch through real quiescent protection/write/cache operations");
            const auto frozen = fixture.Get<LONG64>(NativeClockProfile::patchRva - 3);
            Check((frozen & 0xffffff) == (original & 0xffffff) && CheckBinding(),
                  "independent native readback preserves the three neighboring instruction bytes and changes only the reviewed branch");
            state = Execute(service, fixture, ClockOperation::Freeze, 2);
            Check(state.lastCommandResult == ClockResult::Ok && state.frozen, "repeated actual SDK freeze is idempotent");
            state = Execute(service, fixture, ClockOperation::Unfreeze, 3);
            Check(state.lastCommandResult == ClockResult::Ok && !state.frozen && state.unfreezes == 1,
                  "actual SDK unfreeze restores the original native branch");
            VirtualQuery(reinterpret_cast<const void*>(ClockTestFaults::patchAddress), &after, sizeof(after));
            Check(before.Protect == after.Protect, "successful branch transactions restore the actual original executable page protection");
            FinishOwned(service, fixture);
        }
        for (unsigned fault = 0; fault != 3; ++fault)
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            if (fault == 0)
                ClockTestFaults::failFlushes.store(1);
            if (fault == 1)
                ClockTestFaults::protectionCountdown.store(1);
            if (fault == 2)
                ClockTestFaults::protectionCountdown.store(2);
            const auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::PatchFailure && state.failure == ClockResult::PatchFailure && !state.patchInstalled &&
                      !state.patchRecoveryPending && !state.protectionRestorePending && state.restored,
                  "cache failure and protection acquisition/restoration failures roll back before releasing the native owner");
            FinishOwned(service, fixture);
        }
        for (bool protectionFailure : {false, true})
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            if (protectionFailure)
                ClockTestFaults::failProtectionRestores.store(true);
            else
                ClockTestFaults::failFlushes.store(100);
            const auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::PatchFailure && state.patchRecoveryPending && state.dataRecoveryPending && !state.restored &&
                      state.restoreFailures && !service.End() && service.IsRetained() && (!protectionFailure || state.protectionRestorePending),
                  "persistent cache or executable-protection restoration failure retains explicit branch, data and protection recovery ownership");
            ClockTestFaults::failFlushes.store(0);
            ClockTestFaults::failProtectionRestores.store(false);
            FinishOwned(service, fixture);
        }
        std::puts("Native freeze patch capability: verified with production review gate");
    }
}

#include "NativeWeaponStatSdkTests.inl"
#include "NativeWeaponStatManagerOwnerTests.inl"
#include "NativeWeatherRainOwnerTests.inl"
#include "NativePedPoolOwnerTests.inl"
#include "NativePlayerInfoWantedOwnerTests.inl"
#include "NativeRopesOwnerTests.inl"
#include "NativePedTeleportOwnerTests.inl"
#include "NativePedVelocityOwnerTests.inl"
#include "NativePedReferenceOwnerTests.inl"
#include "NativePedMutationOwnerTests.inl"
#include "NativePedVitalsOwnerTests.inl"
#include "NativePedPhysicalScalarsOwnerTests.inl"
#include "NativePedPhysicalDynamicsOwnerTests.inl"
#include "NativePedMetadataOwnerTests.inl"
#include "PedRenderFixture.generated.h"
#include "NativePedRenderOwnerTests.inl"
#include "NativePedObservationOwnerTests.inl"
#include "NativePedIntelligenceOwnerTests.inl"
#include "NativeTaskPoolOwnerTests.inl"
#include "NativeTaskSecondaryOwnerTests.inl"
#include "NativeModelBankOwnerTests.inl"
#include "NativeModelLoadedOwnerTests.inl"
#include "NativeModelOwnershipOwnerTests.inl"
#include "NativeWorldCheatOwnerTests.inl"
#include "NativeNamedGarageOwnerTests.inl"

#include "NativeGameConstructorWeaponOwnerTests.inl"
#include "NativeGameConstructorServicesOwnerTests.inl"
#include "NativeWorldGroundOwnerTests.inl"
// These fixtures reuse the existing callback, image writer and retirement
// helpers; define those helpers before their new consumers.
#include "NativePedMoveAnimOwnerTests.inl"
#include "NativeModuleClothesInitializerOwnerTests.inl"
#include "NativeWorldLineOwnerTests.inl"
#include "NativeWorldAreaOwnerTests.inl"
#include "NativeWorldJetpackOwnerTests.inl"
#include "NativePedOnScreenOwnerTests.inl"
#include "NativePedWeaponGrantOwnerTests.inl"
#include "NativePedWeaponClearOwnerTests.inl"
#include "NativePedClothesOwnerTests.inl"
#include "NativePedRebuildOwnerTests.inl"
#include "NativeWorldIgnoreEntityOwnerTests.inl"
#include "NativeCameraSphereVisibleOwnerTests.inl"
#include "NativeAnimationBlockOwnerTests.inl"
#include "AnimBlockRefsFixture.generated.h"
#include "NativeAnimationBlockRefsOwnerTests.inl"
#include "AnimationBlockFactoryManager.generated.h"
#include "NativeAnimationBlockFactoryOwnerTests.inl"
#include "NativeAnimationBlockNameOwnerTests.inl"
#include "NativeAnimationManagerOwnerTests.inl"

#include "NativeCustomSequenceStorageOwnerTests.inl"
#include "NativePedModelCreationRuntimeOwnerTests.inl"
#include "NativeStartupModelCollisionRuntimeOwnerTests.inl"
#include "NativeStartupMenuRuntimeOwnerTests.inl"
#include "NativeStartupPreparationReadinessRuntimeOwnerTests.inl"
#include "NativeStartupNumericGaragesRuntimeOwnerTests.inl"
// The joint readiness scope reuses the actual numeric/default-collision helpers.
#include "NativeStreamingRequestOwnerTests.inl"
#include "AnimationSequenceStorageManager.generated.h"
#include "NativeCustomAnimationResourcesOwnerTests.inl"
#include "AnimationManagerRefs.generated.h"
#include "NativeAnimationManagerRefsOwnerTests.inl"
#include "AnimationGateway.generated.h"
#include "NativeAnimationGatewayOwnerTests.inl"
#include "NativeAnimationHierarchyBankOwnerTests.inl"
#include "NativeCustomAnimationSequenceOwnerTests.inl"
#include "NativeCustomAnimationSequenceCopyFrameOwnerTests.inl"
#include "NativeCustomAnimationSequenceNameOwnerTests.inl"
#include "NativeFreshCompressedSequenceOwnerTests.inl"
#include "NativePrivateHierarchyOwnerTests.inl"
#include "NativePrivateHierarchyScalarOwnerTests.inl"
#include "NativePrivateHierarchyNameOwnerTests.inl"
#include "NativePrivateHierarchyParentOwnerTests.inl"
#include "NativePrivateHierarchyEmptyOwnerTests.inl"
#include "NativeFreshUncompressedSequenceOwnerTests.inl"
#include "NativePrivateHierarchyPostProcessOwnerTests.inl"
#include "PrivateAnimationFactoryManager.generated.h"
#include "NativePrivateAnimationFactoryOwnerTests.inl"
#include "NativeCustomAnimationCleanupControllerTests.inl"
#include "NativeCustomAnimationCleanupOwnerTests.inl"
#include "NativeStandaloneSequenceOperationsTests.inl"
#include "NativeBankAnimationFactoryOwnerTests.inl"
#include "NativeSequenceInterfaceOwnerTests.inl"
#include "NativeAnimationManagerCacheOwnerTests.inl"
#include "NativeAnimationBlockHashFactoryOwnerTests.inl"
#include "NativeAnimationDefinitionsOwnerTests.inl"
#include "NativeAnimationDefinitionBlockNamesOwnerTests.inl"
#include "NativeAnimationGroupValidityOwnerTests.inl"
#include "NativeAnimationIdValidityOwnerTests.inl"
#include "AnimationAssociationGroupFixture.generated.h"
#include "NativeAnimationAssociationGroupOwnerTests.inl"
#include "NativeAnimationAssociationGroupOffsetOwnerTests.inl"
#include "AnimationStaticAssociationFixture.generated.h"
#include "NativeAnimationStaticAssociationOwnerTests.inl"
#include "NativeStaticAssociationInitializationOwnerTests.inl"
#include "NativeAnimationAssociationGroupEmptyInitializationOwnerTests.inl"
#include "NativeAnimationAssociationGroupChildProductsOwnerTests.inl"
#include "NativeAnimationStaticAssociationWritesOwnerTests.inl"
#include "PrivateAnimationAssociationFixture.generated.h"
#include "NativePrivateAnimationAssociationOwnerTests.inl"
#include "NativeAnimationAssociationBonesOwnerTests.inl"
#include "NativeAnimationAssociationFloatOwnerTests.inl"
#include "NativeAnimationAssociationProgressOwnerTests.inl"
#include "NativeAnimationAssociationNodeArrayOwnerTests.inl"

// The strict receiver must execute production typed tickets and the distinct
// metadata-only delete route. Deleting an interface acknowledges only its own
// allocation; frame cleanup still requires the separate ownership operation.
static_assert(std::is_same_v<decltype(&PrivateAnimationFactoryManagerReceiver::BeginCustomAnimationSequenceCleanup),
                             decltype(&CAnimManagerDE::BeginCustomAnimationSequenceCleanup)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationFactoryManagerReceiver::BeginCustomAnimationSequenceTransfer),
                             decltype(&CAnimManagerDE::BeginCustomAnimationSequenceTransfer)>);
static_assert(std::is_same_v<decltype(&PrivateAnimationFactoryManagerReceiver::BeginCustomAnimationFrameAttachment),
                             decltype(&CAnimManagerDE::BeginCustomAnimationFrameAttachment)>);
static_assert(std::is_abstract_v<CAnimManagerDE>);
static_assert(std::is_same_v<decltype(&PrivateAnimationFactoryManagerReceiver::DeleteCustomAnimSequenceInterface),
                             decltype(&CAnimManagerDE::DeleteCustomAnimSequenceInterface)>);

// A bank observer is supplied by the same actual owner as the other factories;
// the strict receiver must not replace its raw locator route with an abortor.
static_assert(std::is_same_v<decltype(&PrivateAnimationFactoryManagerReceiver::GetAnimBlendHierarchy), decltype(&CAnimManagerDE::GetAnimBlendHierarchy)>);

namespace PresentTransitionWeatherFault
{
    std::atomic<std::uintptr_t> address{0};
    std::atomic<bool>           fail{false};
    std::atomic<unsigned>       failures{0};
    std::atomic<unsigned>       calls{0};
    BOOL WINAPI                 Flush(HANDLE process, LPCVOID where, SIZE_T bytes)
    {
        if (reinterpret_cast<std::uintptr_t>(where) == address.load() && bytes == 8)
            calls.fetch_add(1);
        if (fail.load() && reinterpret_cast<std::uintptr_t>(where) == address.load() && bytes == 8)
        {
            failures.fetch_add(1);
            SetLastError(ERROR_GEN_FAILURE);
            return FALSE;
        }
        return ::FlushInstructionCache(process, where, bytes);
    }
}
namespace PresentTransitionModuleFault
{
    std::atomic<HMODULE>  acquired{nullptr}, released{nullptr};
    std::atomic<unsigned> acquisitions{0}, releaseAttempts{0}, releases{0};
    std::atomic<bool>     pinRequested{false}, failRelease{false}, releaseGateClosed{true};
    BOOL WINAPI           Acquire(DWORD flags, LPCWSTR address, HMODULE* output)
    {
        pinRequested.store((flags & GET_MODULE_HANDLE_EX_FLAG_PIN) != 0);
        const auto result = ::GetModuleHandleExW(flags, address, output);
        if (result && output && *output)
        {
            acquired.store(*output);
            acquisitions.fetch_add(1);
        }
        return result;
    }
    BOOL WINAPI Release(HMODULE module)
    {
        releaseAttempts.fetch_add(1);
        // These call the actual control API during the exact loader release
        // boundary; a registration change must still exclude Clock/startup.
        mta::game_de::NativePresentTransitionGate::Transition   clock;
        mta::game_de::NativePresentTransitionGate::Registration contender;
        const auto                                              transition = mta::game_de::NativePresentTransitionGate::TryAcquireTransition(99, clock);
        const auto                                              registration = mta::game_de::NativePresentTransitionGate::RegisterOwner(99, contender);
        if (transition != mta::game_de::ClockResult::Busy || registration != mta::game_de::ClockResult::Busy || clock.Held() || contender.IsSet())
            releaseGateClosed.store(false);
        if (failRelease.exchange(false))
        {
            SetLastError(ERROR_BUSY);
            return FALSE;
        }
        released.store(module);
        const auto result = ::FreeLibrary(module);
        if (result)
            releases.fetch_add(1);
        return result;
    }
}
#if defined(MTA_TEST_PRESENT_TRANSITION_CLOCK)
    // Gate owns an ordinary module reference. Observe its real returned handle,
    // then fault only one release acknowledgement before the actual retry.
    #define GetModuleHandleExW PresentTransitionModuleFault::Acquire
    #define FreeLibrary        PresentTransitionModuleFault::Release
    #include "../../Client/game_de/NativePresentTransitionGate.cpp"
    #undef FreeLibrary
    #undef GetModuleHandleExW
#endif
#include "NativePresentTransitionClockOwnerTests.inl"

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const bool presentTransitionClockOnly = argc == 3 && std::strcmp(argv[2], "--present-transition-clock") == 0;
    const bool startupInitializationReadinessOnly = argc == 3 && std::strcmp(argv[2], "--startup-initialization-readiness") == 0;
    const bool startupStreamingReceiverOnly = argc == 3 && std::strcmp(argv[2], "--startup-streaming-receiver") == 0;
    const bool startupNumericGaragesOnly = argc == 3 && std::strcmp(argv[2], "--startup-numeric-garages") == 0;
    const bool startupPreparationReadinessOnly = argc == 3 && std::strcmp(argv[2], "--startup-preparation-readiness") == 0;
    const bool startupMenuSuccess = argc == 3 && std::strcmp(argv[2], "--startup-menu-runtime-success") == 0;
    const bool startupMenuAbsent = argc == 3 && std::strcmp(argv[2], "--startup-menu-runtime-no-widget") == 0;
    const bool startupMenuBefore = argc == 3 && std::strcmp(argv[2], "--startup-menu-runtime-unknown-before") == 0;
    const bool startupMenuAfter = argc == 3 && std::strcmp(argv[2], "--startup-menu-runtime-unknown-after") == 0;
    const bool startupMenuPost = argc == 3 && std::strcmp(argv[2], "--startup-menu-runtime-post-owner-loss") == 0;
    const bool startupMenuOnly = startupMenuSuccess || startupMenuAbsent || startupMenuBefore || startupMenuAfter || startupMenuPost;
    const bool startupCollisionSuccess = argc == 3 && std::strcmp(argv[2], "--startup-collision-runtime-success") == 0;
    const bool startupCollisionBefore = argc == 3 && std::strcmp(argv[2], "--startup-collision-runtime-unknown-before") == 0;
    const bool startupCollisionAfter = argc == 3 && std::strcmp(argv[2], "--startup-collision-runtime-unknown-after") == 0;
    const bool startupCollisionPost = argc == 3 && std::strcmp(argv[2], "--startup-collision-runtime-post-owner-loss") == 0;
    const bool startupCollisionOnly = startupCollisionSuccess || startupCollisionBefore || startupCollisionAfter || startupCollisionPost;
    const bool pedCreationLifecycleWritten = argc == 3 && std::strcmp(argv[2], "--ped-model-creation-runtime-written") == 0;
    const bool pedCreationLifecycleReadOnly = argc == 3 && std::strcmp(argv[2], "--ped-model-creation-runtime-read-only") == 0;
    const bool pedCreationLifecycleUnknown = argc == 3 && std::strcmp(argv[2], "--ped-model-creation-runtime-unknown-allocation") == 0;
    const bool pedCreationLifecycleUnpublished = argc == 3 && std::strcmp(argv[2], "--ped-model-creation-runtime-unpublished") == 0;
    const bool pedCreationLifecycleOnly =
        pedCreationLifecycleWritten || pedCreationLifecycleReadOnly || pedCreationLifecycleUnknown || pedCreationLifecycleUnpublished;

    const bool associationNodeArrayUnknownFree = argc == 3 && std::strcmp(argv[2], "--animation-association-node-array-unknown-free") == 0;
    const bool associationNodeArrayUnknownClear = argc == 3 && std::strcmp(argv[2], "--animation-association-node-array-unknown-clear") == 0;
    const bool associationNodeArrayOwnerLoss = argc == 3 && std::strcmp(argv[2], "--animation-association-node-array-clear-owner-loss") == 0;
    const bool associationNodeArrayOnly = associationNodeArrayUnknownFree || associationNodeArrayUnknownClear || associationNodeArrayOwnerLoss ||
                                          (argc == 3 && std::strcmp(argv[2], "--animation-association-node-array-success") == 0);
    const bool associationProgressUnknown = argc == 3 && std::strcmp(argv[2], "--animation-association-progress-unknown") == 0;
    const bool associationProgressOnly = associationProgressUnknown || (argc == 3 && std::strcmp(argv[2], "--animation-association-progress-success") == 0);
    const bool associationFloatOnly = argc == 3 && std::strcmp(argv[2], "--animation-association-float") == 0;
    const bool associationBonesUnknownWrite = argc == 3 && std::strcmp(argv[2], "--animation-association-bones-unknown-write") == 0;
    const bool associationBonesPostLoss = argc == 3 && std::strcmp(argv[2], "--animation-association-bones-post-loss") == 0;
    const bool associationBonesOnly =
        associationBonesUnknownWrite || associationBonesPostLoss || (argc == 3 && std::strcmp(argv[2], "--animation-association-bones-success") == 0);
    const bool privateAssociationConstructorUnknown = argc == 3 && std::strcmp(argv[2], "--private-animation-associations-constructor-unknown") == 0;
    const bool privateAssociationDestructorUnknown = argc == 3 && std::strcmp(argv[2], "--private-animation-associations-destructor-unknown") == 0;
    const bool privateAssociationPostAckLoss = argc == 3 && std::strcmp(argv[2], "--private-animation-associations-retire-owner-loss") == 0;
    const bool privateAssociationsOnly = privateAssociationConstructorUnknown || privateAssociationDestructorUnknown || privateAssociationPostAckLoss ||
                                         (argc == 3 && std::strcmp(argv[2], "--private-animation-associations-success") == 0);
    const unsigned privateAssociationScenario = privateAssociationConstructorUnknown  ? 1u
                                                : privateAssociationDestructorUnknown ? 2u
                                                : privateAssociationPostAckLoss       ? 3u
                                                                                      : 0u;
    const bool     pedRenderOnly = argc == 3 && std::strcmp(argv[2], "--ped-render") == 0;
    const bool     staticAssociationWriteConflict = argc == 3 && std::strcmp(argv[2], "--animation-static-association-writes-conflict") == 0;
    const bool     staticAssociationWriteLostAck = argc == 3 && std::strcmp(argv[2], "--animation-static-association-writes-lost-ack") == 0;
    const bool     staticAssociationWriteUnknown = argc == 3 && std::strcmp(argv[2], "--animation-static-association-writes-unknown") == 0;
    const bool     staticAssociationWritesOnly = staticAssociationWriteConflict || staticAssociationWriteLostAck || staticAssociationWriteUnknown ||
                                             (argc == 3 && std::strcmp(argv[2], "--animation-static-association-writes-success") == 0);
    const unsigned staticAssociationWriteScenario = staticAssociationWriteConflict  ? 1u
                                                    : staticAssociationWriteLostAck ? 2u
                                                    : staticAssociationWriteUnknown ? 3u
                                                                                    : 0u;
    const bool     staticAssociationInitializationUnknown = argc == 3 && std::strcmp(argv[2], "--static-association-initialization-unknown") == 0;
    const bool     staticAssociationInitializationOnly =
        staticAssociationInitializationUnknown || (argc == 3 && std::strcmp(argv[2], "--static-association-initialization-success") == 0);
    const bool animationStaticAssociationReadsOnly = argc == 3 && std::strcmp(argv[2], "--animation-static-association-reads") == 0;
    const bool animationAssociationGroupReadsOnly = argc == 3 && std::strcmp(argv[2], "--animation-association-group-reads") == 0;
    const bool groupEmptyInitializationUnknown = argc == 3 && std::strcmp(argv[2], "--group-empty-initialization-unknown") == 0;
    const bool groupEmptyInitializationOnly =
        groupEmptyInitializationUnknown || (argc == 3 && std::strcmp(argv[2], "--group-empty-initialization-success") == 0);
    const bool animationAssociationGroupChildProductsOnly = argc == 3 && std::strcmp(argv[2], "--animation-association-group-child-products") == 0;
    const bool animationAssociationGroupOffsetLostAck = argc == 3 && std::strcmp(argv[2], "--animation-association-group-offset-lost-ack") == 0;
    const bool animationAssociationGroupOffsetConflict = argc == 3 && std::strcmp(argv[2], "--animation-association-group-offset-conflict") == 0;
    const bool animationAssociationGroupOffsetOnly = animationAssociationGroupOffsetLostAck || animationAssociationGroupOffsetConflict ||
                                                     (argc == 3 && std::strcmp(argv[2], "--animation-association-group-offset-success") == 0);
    const bool     animationIdValidityOnly = argc == 3 && std::strcmp(argv[2], "--animation-id-validity") == 0;
    const bool     animationGroupValidityOnly = argc == 3 && std::strcmp(argv[2], "--animation-group-validity") == 0;
    const bool     animationDefinitionBlockNamesOnly = argc == 3 && std::strcmp(argv[2], "--animation-definition-block-names") == 0;
    const bool     animationDefinitionsOnly = argc == 3 && std::strcmp(argv[2], "--animation-definitions") == 0;
    const bool     animationBlockHashFactoryOnly = argc == 3 && std::strcmp(argv[2], "--animation-block-hash-factory") == 0;
    const bool     animationManagerCacheForeign = argc == 3 && std::strcmp(argv[2], "--animation-manager-cache-foreign") == 0;
    const bool     animationManagerCacheOnly = animationManagerCacheForeign || (argc == 3 && std::strcmp(argv[2], "--animation-manager-cache-success") == 0);
    const bool     sequenceInterfaceUnknown = argc == 3 && std::strcmp(argv[2], "--sequence-interface-unknown") == 0;
    const bool     sequenceInterfaceOnly = sequenceInterfaceUnknown || (argc == 3 && std::strcmp(argv[2], "--sequence-interface-success") == 0);
    const bool     animationBankFactoriesOnly = argc == 3 && std::strcmp(argv[2], "--animation-bank-factory") == 0;
    const bool     standaloneSequenceOperationsOnly = argc == 3 && std::strncmp(argv[2], "--standalone-sequence-operations-", 33) == 0;
    const unsigned standaloneSequenceOperationsScenario = !standaloneSequenceOperationsOnly                                            ? 0u
                                                          : std::strcmp(argv[2], "--standalone-sequence-operations-unknown") == 0      ? 1u
                                                          : std::strcmp(argv[2], "--standalone-sequence-operations-foreign-drop") == 0 ? 2u
                                                          : std::strcmp(argv[2], "--standalone-sequence-operations-bind-loss") == 0    ? 3u
                                                          : std::strcmp(argv[2], "--standalone-sequence-operations-copy-loss") == 0    ? 4u
                                                                                                                                       : 0u;
    const bool     customAnimationCleanupOnly = argc == 3 && std::strncmp(argv[2], "--custom-animation-cleanup-", 27) == 0;
    const unsigned customAnimationCleanupScenario = !customAnimationCleanupOnly                                             ? 0u
                                                    : std::strcmp(argv[2], "--custom-animation-cleanup-returned-loss") == 0 ? 1u
                                                    : std::strcmp(argv[2], "--custom-animation-cleanup-unknown") == 0       ? 2u
                                                    : std::strcmp(argv[2], "--custom-animation-cleanup-foreign-drop") == 0  ? 3u
                                                                                                                            : 0u;
    const bool     privateAnimationFactoriesOnly = argc == 3 && std::strcmp(argv[2], "--private-animation-factories") == 0;
    const bool     privateHierarchyPostprocessOnly = argc == 3 && std::strncmp(argv[2], "--private-hierarchy-postprocess-", 32) == 0;
    const unsigned privateHierarchyPostprocessScenario = !privateHierarchyPostprocessOnly                                       ? 0u
                                                         : std::strcmp(argv[2], "--private-hierarchy-postprocess-unknown") == 0 ? 1u
                                                         : std::strcmp(argv[2], "--private-hierarchy-postprocess-lostack") == 0 ? 2u
                                                                                                                                : 0u;
    const bool     freshUncompressedSequenceOnly = argc == 3 && std::strcmp(argv[2], "--fresh-uncompressed-sequence") == 0;
    const bool     privateHierarchyEmptyOnly = argc == 3 && std::strncmp(argv[2], "--private-hierarchy-empty-", 26) == 0;
    const unsigned privateHierarchyEmptyScenario = !privateHierarchyEmptyOnly                                             ? 0u
                                                   : std::strcmp(argv[2], "--private-hierarchy-empty-accepted-loss") == 0 ? 1u
                                                   : std::strcmp(argv[2], "--private-hierarchy-empty-unknown-count") == 0 ? 2u
                                                   : std::strcmp(argv[2], "--private-hierarchy-empty-foreign-cache") == 0 ? 3u
                                                                                                                          : 0u;
    const bool     privateHierarchyParentOnly = argc == 3 && std::strncmp(argv[2], "--private-hierarchy-parent-", 27) == 0;
    const unsigned privateHierarchyParentScenario = !privateHierarchyParentOnly                                               ? 0u
                                                    : std::strcmp(argv[2], "--private-hierarchy-parent-lost-detach") == 0     ? 1u
                                                    : std::strcmp(argv[2], "--private-hierarchy-parent-unknown-attach") == 0  ? 2u
                                                    : std::strcmp(argv[2], "--private-hierarchy-parent-foreign-pointer") == 0 ? 3u
                                                    : std::strcmp(argv[2], "--private-hierarchy-parent-foreign-cache") == 0   ? 4u
                                                    : std::strcmp(argv[2], "--private-hierarchy-parent-published") == 0       ? 5u
                                                                                                                              : 0u;
    const bool     privateHierarchyNamePinned = argc == 3 && std::strcmp(argv[2], "--private-hierarchy-name-pinned") == 0;
    const bool     privateHierarchyNameOnly = privateHierarchyNamePinned || (argc == 3 && std::strcmp(argv[2], "--private-hierarchy-name-success") == 0);
    const bool     privateHierarchyScalarPinned = argc == 3 && std::strcmp(argv[2], "--private-hierarchy-scalars-pinned") == 0;
    const bool privateHierarchyScalarsOnly = privateHierarchyScalarPinned || (argc == 3 && std::strcmp(argv[2], "--private-hierarchy-scalars-success") == 0);
    const bool privateHierarchyPublished = argc == 3 && std::strcmp(argv[2], "--private-hierarchy-published") == 0;
    const bool privateHierarchyPointer = argc == 3 && std::strcmp(argv[2], "--private-hierarchy-pointer") == 0;
    const bool privateHierarchyCache = argc == 3 && std::strcmp(argv[2], "--private-hierarchy-cache") == 0;
    const bool privateHierarchyOnly = privateHierarchyPublished || privateHierarchyPointer || privateHierarchyCache ||
                                      (argc == 3 && std::strcmp(argv[2], "--private-hierarchy-success") == 0);
    const bool freshCompressedSequenceOnly = argc == 3 && std::strcmp(argv[2], "--fresh-compressed-sequence") == 0;
    const bool customAnimationSequenceNameOnly = argc == 3 && std::strcmp(argv[2], "--custom-animation-sequence-name") == 0;
    const bool animationHierarchyBankOnly = argc == 3 && std::strcmp(argv[2], "--animation-hierarchy-bank") == 0;
    const bool customAnimationSequenceCopyFrameOnly = argc == 3 && std::strcmp(argv[2], "--custom-animation-sequence-copy-frame") == 0;
    const bool customAnimationSequenceOnly = argc == 3 && std::strcmp(argv[2], "--custom-animation-sequence") == 0;
    const bool animationGatewayOnly = argc == 3 && std::strcmp(argv[2], "--animation-gateway") == 0;
    const bool animationManagerRefsOnly = argc == 3 && std::strcmp(argv[2], "--animation-manager-refs") == 0;
    const bool customAnimationResourcesSuccess = argc == 3 && std::strcmp(argv[2], "--custom-animation-resources-success") == 0;
    const bool customAnimationResourcesUnknownAllocate = argc == 3 && std::strcmp(argv[2], "--custom-animation-resources-unknown-allocate") == 0;
    const bool customAnimationResourcesUnknownFree = argc == 3 && std::strcmp(argv[2], "--custom-animation-resources-unknown-free") == 0;
    const bool customAnimationResourcesOnly = customAnimationResourcesSuccess || customAnimationResourcesUnknownAllocate || customAnimationResourcesUnknownFree;
    const bool customSequenceStorageSuccess = argc == 3 && std::strcmp(argv[2], "--custom-sequence-storage-success") == 0;
    const bool customSequenceStorageUnknownAllocate = argc == 3 && std::strcmp(argv[2], "--custom-sequence-storage-unknown-allocate") == 0;
    const bool customSequenceStorageUnknownFree = argc == 3 && std::strcmp(argv[2], "--custom-sequence-storage-unknown-free") == 0;
    const bool customSequenceStorageOnly = customSequenceStorageSuccess || customSequenceStorageUnknownAllocate || customSequenceStorageUnknownFree;
    const bool animationBlockFactoryOnly = argc == 3 && std::strcmp(argv[2], "--animation-block-factory") == 0;
    const bool animationBlockRefsOnly = argc == 3 && std::strcmp(argv[2], "--animation-block-refs") == 0;
    const bool playerNativePrerequisitesOnly = argc == 3 && std::strcmp(argv[2], "--player-native-prerequisites") == 0;
    const bool pedMoveAnimOnly = argc == 3 && std::strcmp(argv[2], "--ped-move-anim") == 0;
    const bool pedRebuildOnly = argc == 3 && std::strcmp(argv[2], "--ped-rebuild") == 0;
    const bool pedClothesOnly = argc == 3 && std::strcmp(argv[2], "--ped-clothes") == 0;
    const bool pedWeaponClearOnly = argc == 3 && std::strcmp(argv[2], "--ped-weapon-clear") == 0;
    const bool cameraSphereOnly = argc == 3 && std::strcmp(argv[2], "--camera-sphere-visible") == 0;
    const bool worldIgnoreOnly = argc == 3 && std::strcmp(argv[2], "--world-ignore-entity") == 0;
    const bool pedWeaponGrantOnly = argc == 3 && std::strcmp(argv[2], "--ped-weapon-grant") == 0;
    const bool pedOnScreenOnly = argc == 3 && std::strcmp(argv[2], "--ped-on-screen") == 0;
    const bool worldJetpackOnly = argc == 3 && std::strcmp(argv[2], "--world-jetpack") == 0;
    const bool worldAreaOnly = argc == 3 && std::strcmp(argv[2], "--world-area") == 0;
    const bool worldQueriesOnly = argc == 3 && std::strcmp(argv[2], "--world-queries") == 0;
    const bool worldLineOnly = argc == 3 && std::strcmp(argv[2], "--world-line") == 0;
    const bool worldGroundOnly = argc == 3 && std::strcmp(argv[2], "--world-ground") == 0;
    const bool constructorServicesOnly = argc == 3 && std::strcmp(argv[2], "--constructor-services") == 0;
    const bool visibilityOnly = argc == 3 && std::strcmp(argv[2], "--ped-visibility") == 0;
    const bool gogglesOnly = argc == 3 && std::strcmp(argv[2], "--ped-goggles") == 0;
    const bool duckingSetterOnly = argc == 3 && std::strcmp(argv[2], "--ducking-setter") == 0;
    const bool fireCollisionOnly = argc == 3 && std::strcmp(argv[2], "--fire-collision") == 0;
    const bool fireOnly = argc == 3 && std::strcmp(argv[2], "--ped-fire") == 0;
    const bool collisionOnly = argc == 3 && std::strcmp(argv[2], "--ped-collision") == 0;
    const bool areaSpecialOnly = argc == 3 && std::strcmp(argv[2], "--area-special-model") == 0;
    const bool areaOnly = argc == 3 && std::strcmp(argv[2], "--ped-area") == 0;
    const bool specialOnly = argc == 3 && std::strcmp(argv[2], "--special-model") == 0;
    const bool animationServicesOnly = argc == 3 && std::strcmp(argv[2], "--animation-services") == 0;
    const bool animationManagerOnly = argc == 3 && std::strcmp(argv[2], "--animation-manager") == 0;
    const bool animationNameOnly = argc == 3 && std::strcmp(argv[2], "--animation-block-name") == 0;
    const bool animationOnly = argc == 3 && std::strcmp(argv[2], "--animation-block") == 0;
    const bool constructorSuccess = argc == 3 && std::strcmp(argv[2], "--weapon-constructor-success") == 0;
    const bool constructorPartial = argc == 3 && std::strcmp(argv[2], "--weapon-constructor-partial") == 0;
    const bool constructorRecovery = argc == 3 && std::strcmp(argv[2], "--weapon-constructor-recovery") == 0;
    const bool constructorTimer = argc == 3 && std::strcmp(argv[2], "--weapon-constructor-timer") == 0;
    const bool constructorOnly = constructorSuccess || constructorPartial || constructorRecovery || constructorTimer;
    const bool pedOwnerOnly = argc == 3 && std::strcmp(argv[2], "--ped-owner") == 0;
    const bool currentWeaponOnly = argc == 3 && std::strcmp(argv[2], "--ped-current-weapon") == 0;
    const bool garageNamedOnly = argc == 3 && std::strcmp(argv[2], "--garage-named") == 0;
    const bool garagePoolsOnly = argc == 3 && std::strcmp(argv[2], "--garage-pools") == 0;
    const bool orientationWithWater = argc == 3 && std::strcmp(argv[2], "--ped-water-orientation") == 0;
    const bool waterOnly = orientationWithWater || (argc == 3 && std::strcmp(argv[2], "--ped-water") == 0);
    const bool poseOnly = argc == 3 && std::strcmp(argv[2], "--ped-pose") == 0;
    const bool poolRegistryOnly = argc == 3 && std::strcmp(argv[2], "--ped-pool-registry") == 0;
    Check((argc == 2 || presentTransitionClockOnly || startupInitializationReadinessOnly || startupStreamingReceiverOnly || startupNumericGaragesOnly ||
           startupPreparationReadinessOnly || startupMenuOnly || startupCollisionOnly || pedCreationLifecycleOnly || constructorOnly || pedOwnerOnly ||
           currentWeaponOnly || garageNamedOnly || garagePoolsOnly || poolRegistryOnly || poseOnly || waterOnly || animationOnly || animationNameOnly ||
           animationManagerOnly || animationServicesOnly || areaSpecialOnly || areaOnly || specialOnly || fireCollisionOnly || fireOnly || collisionOnly ||
           duckingSetterOnly || gogglesOnly || visibilityOnly || constructorServicesOnly || worldGroundOnly || worldLineOnly || worldAreaOnly ||
           worldQueriesOnly || worldJetpackOnly || pedOnScreenOnly || pedWeaponGrantOnly || worldIgnoreOnly || cameraSphereOnly || pedWeaponClearOnly ||
           pedClothesOnly || pedRebuildOnly || pedMoveAnimOnly || playerNativePrerequisitesOnly || animationBlockRefsOnly || animationBlockFactoryOnly ||
           customSequenceStorageOnly || customAnimationResourcesOnly || animationManagerRefsOnly || animationGatewayOnly || customAnimationSequenceOnly ||
           customAnimationSequenceCopyFrameOnly || animationHierarchyBankOnly || customAnimationSequenceNameOnly || freshCompressedSequenceOnly ||
           privateHierarchyOnly || privateHierarchyScalarsOnly || privateHierarchyNameOnly || privateHierarchyParentOnly || privateHierarchyEmptyOnly ||
           freshUncompressedSequenceOnly || privateHierarchyPostprocessOnly || privateAnimationFactoriesOnly || customAnimationCleanupOnly ||
           standaloneSequenceOperationsOnly || animationBankFactoriesOnly || sequenceInterfaceOnly || animationManagerCacheOnly ||
           animationBlockHashFactoryOnly || animationDefinitionsOnly || animationDefinitionBlockNamesOnly || animationGroupValidityOnly ||
           animationIdValidityOnly || animationAssociationGroupReadsOnly || groupEmptyInitializationOnly || animationAssociationGroupChildProductsOnly ||
           animationAssociationGroupOffsetOnly || animationStaticAssociationReadsOnly || staticAssociationWritesOnly || staticAssociationInitializationOnly ||
           pedRenderOnly || privateAssociationsOnly || associationBonesOnly || associationFloatOnly || associationProgressOnly || associationNodeArrayOnly) &&
              std::filesystem::path(argv[1]).is_absolute() && std::filesystem::equivalent(std::filesystem::path(argv[0]).parent_path(), argv[1]),
          "durable fixture evidence is restricted to the explicit owned executable output directory");
    if (presentTransitionClockOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PresentTransitionClockFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (startupInitializationReadinessOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeStreamingRequestOwnedSdk(fixture, true, false, true);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (startupStreamingReceiverOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeStreamingRequestOwnedSdk(fixture, true, true);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (startupNumericGaragesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        StartupNumericGaragesRuntimeFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (startupPreparationReadinessOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        StartupPreparationReadinessRuntimeFixture::Run(fixture);
        return 0;
    }
    if (startupMenuOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        StartupMenuRuntimeFixture::Run(fixture, startupMenuAbsent ? 1u : startupMenuBefore ? 2u : startupMenuAfter ? 3u : startupMenuPost ? 4u : 0u);
        std::puts("Production startup menu runtime lifecycle fixture completed");
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (startupCollisionOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        StartupCollisionRuntimeFixture::Run(fixture, startupCollisionBefore ? 1u : startupCollisionAfter ? 2u : startupCollisionPost ? 3u : 0u);
        std::puts("Production startup collision runtime lifecycle fixture completed");
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedCreationLifecycleOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedMetadataLifecycleFixture::Run(fixture, pedCreationLifecycleReadOnly      ? 1u
                                                  : pedCreationLifecycleUnknown     ? 2u
                                                  : pedCreationLifecycleUnpublished ? 3u
                                                                                    : 0u);
        std::puts("Production ped metadata runtime lifecycle fixture completed");
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (associationNodeArrayOnly)
    {
        AnimationAssociationNodeArrayOwnerFixture::scenario = associationNodeArrayUnknownFree    ? 1u
                                                              : associationNodeArrayUnknownClear ? 2u
                                                              : associationNodeArrayOwnerLoss    ? 3u
                                                                                                 : 0u;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        AnimationAssociationNodeArrayOwnerFixture::RunPrepared(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (associationProgressOnly)
    {
        // This selected process admits only the focused copied-body closure;
        // unknown entered effects retain every live fixture until process exit.
        AnimationAssociationProgressOwnerFixture::scenario = associationProgressUnknown ? 1u : 0u;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        AnimationAssociationProgressOwnerFixture::RunPrepared(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (associationFloatOnly)
    {
        // One focused owned process executes the genuine private constructor,
        // current float SDK reads and explicit retirement, not an old suite.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        AnimationAssociationFloatOwnerFixture::RunPrepared(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (associationBonesOnly)
    {
        AnimationAssociationBonesOwnerFixture::scenario = associationBonesUnknownWrite ? 2u : associationBonesPostLoss ? 1u : 0u;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        AnimationAssociationBonesOwnerFixture::RunPrepared(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateAssociationsOnly)
    {
        PrivateAnimationAssociationOwnerFixture::scenario = privateAssociationScenario;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PrivateAnimationAssociationOwnerFixture::RunPrepared(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedRenderOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedRenderOwnerFixture::RunPrepared(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (staticAssociationWritesOnly)
    {
        AnimationStaticAssociationWritesOwnerFixture::scenario = staticAssociationWriteScenario;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationStaticAssociationWritesOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (staticAssociationInitializationOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        // A fresh process preserves BeginOwned's pristine public rejection
        // contract. Module pins and entered debt are never erased to squeeze
        // a second independent lifetime into the same ordinary fixture host.
        StaticAssociationInitializationOwnerFixture::scenario = staticAssociationInitializationUnknown ? 1u : 0u;
        NativeAnimationBlocksOwnedSdk(fixture, &StaticAssociationInitializationOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationStaticAssociationReadsOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationStaticAssociationOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationAssociationGroupOffsetOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        // Each module-pinned owner lifetime keeps BeginOwned's pristine
        // admission checks. Independent scenarios run in fresh processes;
        // neither native owner fields nor public rejection guards are reset.
        AnimationAssociationGroupOffsetOwnerFixture::scenario = animationAssociationGroupOffsetLostAck ? 1u : animationAssociationGroupOffsetConflict ? 2u : 0u;
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationAssociationGroupOffsetOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationAssociationGroupChildProductsOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        // One real module-pinned lifetime needs one pristine host process.
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationAssociationGroupChildProductsOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (groupEmptyInitializationOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        // Actual sticky module-pin and unknown-entry lifetimes stay isolated.
        AnimationAssociationGroupEmptyInitializationOwnerFixture::scenario = groupEmptyInitializationUnknown ? 1u : 0u;
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationAssociationGroupEmptyInitializationOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationAssociationGroupReadsOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationAssociationGroupOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationIdValidityOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationIdValidityOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationGroupValidityOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationGroupValidityOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationDefinitionBlockNamesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationDefinitionBlockNamesOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationDefinitionsOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationDefinitionsOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationBlockHashFactoryOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationBlockHashFactoryOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationManagerCacheOnly)
    {
        AnimationManagerCacheOwnerFixture::scenario = animationManagerCacheForeign ? 1u : 0u;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationManagerCacheOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (sequenceInterfaceOnly)
    {
        SequenceInterfaceOwnerFixture::scenario = sequenceInterfaceUnknown ? 1u : 0u;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &SequenceInterfaceOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationBankFactoriesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &BankAnimationFactoryOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (standaloneSequenceOperationsOnly)
    {
        StandaloneSequenceOperationsOwnerFixture::scenario = standaloneSequenceOperationsScenario;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &StandaloneSequenceOperationsOwnerFixture::Run);
        std::puts("Production standalone strict factory dispatch and metadata delete routing verified");
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (customAnimationCleanupOnly)
    {
        CustomAnimationCleanupOwnerFixture::scenario = customAnimationCleanupScenario;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &CustomAnimationCleanupOwnerFixture::Run);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateAnimationFactoriesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &PrivateAnimationFactoryOwnerFixture::Run<PrivateAnimationFactoryManagerReceiver>);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateHierarchyPostprocessOnly)
    {
        PrivateHierarchyPostProcessOwnerFixture::scenario = privateHierarchyPostprocessScenario;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &PrivateHierarchyPostProcessOwnerFixture::Run);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (freshUncompressedSequenceOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        FreshUncompressedSequenceOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateHierarchyEmptyOnly)
    {
        PrivateHierarchyEmptyOwnerFixture::scenario = privateHierarchyEmptyScenario;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &PrivateHierarchyEmptyOwnerFixture::Run);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateHierarchyParentOnly)
    {
        PrivateHierarchyParentOwnerFixture::scenario = privateHierarchyParentScenario;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &PrivateHierarchyParentOwnerFixture::Run);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateHierarchyNameOnly)
    {
        PrivateHierarchyNameOwnerFixture::pinned = privateHierarchyNamePinned;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &PrivateHierarchyNameOwnerFixture::Prepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateHierarchyScalarsOnly)
    {
        PrivateHierarchyScalarOwnerFixture::pinned = privateHierarchyScalarPinned;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &PrivateHierarchyScalarOwnerFixture::Prepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (privateHierarchyOnly)
    {
        PrivateHierarchyOwnerFixture::scenario = privateHierarchyPublished ? 1u : privateHierarchyPointer ? 2u : privateHierarchyCache ? 3u : 0u;
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &PrivateHierarchyOwnerFixture::Prepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (freshCompressedSequenceOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        FreshCompressedSequenceOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (customAnimationSequenceNameOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        CustomAnimationSequenceNameOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationHierarchyBankOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationHierarchyBankOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (customAnimationSequenceCopyFrameOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        CustomAnimationSequenceCopyFrameOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (customAnimationSequenceOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        CustomAnimationSequenceOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationGatewayOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationGatewayOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationManagerRefsOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationManagerRefsOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (customAnimationResourcesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        CustomAnimationResourcesOwnerFixture::Run(fixture, customAnimationResourcesUnknownAllocate ? 1u : customAnimationResourcesUnknownFree ? 2u : 0u);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (customSequenceStorageOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        CustomSequenceStorageOwnerFixture::Run(fixture, customSequenceStorageUnknownAllocate ? 1u : customSequenceStorageUnknownFree ? 2u : 0u);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationBlockFactoryOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationBlockFactoryOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationBlockRefsOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &AnimationBlockRefsOwnerFixture::RunPrepared);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (playerNativePrerequisitesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedMoveAnimOwnerFixture::Run(fixture);
        ModuleClothesInitializerOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedMoveAnimOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedMoveAnimOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedRebuildOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedRebuildOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedClothesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedClothesOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedWeaponClearOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedWeaponClearOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (cameraSphereOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        CameraSphereVisibleOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (worldIgnoreOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        WorldIgnoreEntityOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedWeaponGrantOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedWeaponGrantOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedOnScreenOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        PedOnScreenOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (worldJetpackOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        // This new guarded getter is independent of historical effect suites.
        WorldJetpackOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (worldQueriesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        // One compiled process exercises only these two new groups. The line
        // group retires all owned runtime/image state before the area group.
        WorldLineOwnerFixture::Run(fixture);
        WorldAreaOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (worldAreaOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        WorldAreaOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (worldLineOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        WorldLineOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (worldGroundOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        WorldGroundOwnerFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (constructorServicesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        ConstructorServicesFixture::Run(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (visibilityOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativePedPhysicalScalarsOwnedSdk(fixture, false, false, false, false, false, false, 0, false, false, true);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (gogglesOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativePedPhysicalScalarsOwnedSdk(fixture, false, false, false, false, false, false, 0, false, true);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (duckingSetterOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativePedPhysicalScalarsOwnedSdk(fixture, false, false, false, false, false, false, 0, true);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (fireCollisionOnly || fireOnly || collisionOnly)
    {
        if (fireCollisionOnly || fireOnly)
        {
            const auto directory = std::filesystem::path(argv[1]) / L"ped-fire-owner";
            Check(std::filesystem::create_directory(directory), "fresh durable registered fire owner setup");
            ClockImageFixture fixture;
            fixture.Create(directory);
            NativePedPhysicalScalarsOwnedSdk(fixture, false, false, false, false, false, false, 1);
        }
        if (fireCollisionOnly || collisionOnly)
        {
            const auto directory = std::filesystem::path(argv[1]) / L"ped-collision-owner";
            Check(std::filesystem::create_directory(directory), "fresh durable registered collision owner setup");
            ClockImageFixture fixture;
            fixture.Create(directory);
            NativePedPhysicalScalarsOwnedSdk(fixture, false, false, false, false, false, false, 2);
        }
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (areaSpecialOnly || areaOnly || specialOnly)
    {
        if (areaSpecialOnly || areaOnly)
        {
            const auto directory = std::filesystem::path(argv[1]) / L"ped-area-owner";
            Check(std::filesystem::create_directory(directory), "fresh durable registered area owner setup");
            ClockImageFixture fixture;
            fixture.Create(directory);
            NativePedPhysicalScalarsOwnedSdk(fixture, false, false, false, false, false, true);
        }
        if (areaSpecialOnly || specialOnly)
        {
            const auto directory = std::filesystem::path(argv[1]) / L"special-model-owner";
            Check(std::filesystem::create_directory(directory), "fresh durable special-model owner setup");
            ClockImageFixture fixture;
            fixture.Create(directory);
            NativeStreamingRequestOwnedSdk(fixture, true);
        }
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationNameOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &NativeAnimationBlockNamesOwnedSdk);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationServicesOnly)
    {
        // One compilation/run executes both current services, each with a
        // fresh owned image and ordinary owner setup. No prior suite state or
        // historical metadata test execution is borrowed as current evidence.
        const auto nameRoot = std::filesystem::path(argv[1]) / L"animation-name-owner";
        Check(std::filesystem::create_directory(nameRoot), "create fresh durable name-owner image directory");
        {
            ClockImageFixture fixture;
            fixture.Create(nameRoot);
            NativeAnimationBlocksOwnedSdk(fixture, &NativeAnimationBlockNamesOwnedSdk);
        }
        const auto managerRoot = std::filesystem::path(argv[1]) / L"animation-manager-owner";
        Check(std::filesystem::create_directory(managerRoot), "create fresh durable manager-owner image directory");
        {
            ClockImageFixture fixture;
            fixture.Create(managerRoot);
            NativeAnimationBlocksOwnedSdk(fixture, &NativeAnimationManagerOwnedSdk);
        }
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationManagerOnly)
    {
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture, &NativeAnimationManagerOwnedSdk);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (animationOnly)
    {
        // This cohort observes only animation metadata through the real owner;
        // earlier ped, streaming and constructor receipts stay historical.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeAnimationBlocksOwnedSdk(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (constructorOnly)
    {
        // Each process supplies a fresh real runtime manager. Failure remains
        // terminal rather than resetting production one-shot state for tests.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        if (constructorTimer)
            ConstructorWeaponFixture::TimerJournal(fixture);
        else
            ConstructorWeaponFixture::Run(fixture, constructorPartial ? 1u : constructorRecovery ? 2u : 0u);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (waterOnly)
    {
        // Focus the new real physical-water SDK/token owner. Existing pose,
        // constructor and garage receipts stay separate historical evidence.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativePedPhysicalScalarsOwnedSdk(fixture, false, false, false, true, orientationWithWater);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (poseOnly)
    {
        // Execute only this actual SDK/token/placement completion cohort;
        // historical constructor/garage/weapon tests keep separate receipts.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativePedPhysicalScalarsOwnedSdk(fixture, false, false, true);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (garagePoolsOnly || poolRegistryOnly)
    {
        // One focused current cohort uses the actual named SDK and sole ped
        // reference owner without replaying unrelated historical owner suites.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        if (garagePoolsOnly)
            NativeNamedGaragesOwnedSdk(fixture);
        NativePedPhysicalScalarsOwnedSdk(fixture, false, true);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (garageNamedOnly)
    {
        // This native name/property fixture supplies all its own admission.
        // Historical ped, model, weather and peer-recovery suites stay separate.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativeNamedGaragesOwnedSdk(fixture);
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    if (pedOwnerOnly || currentWeaponOnly)
    {
        // This fixture creates its pool, reference nodes and callback admission
        // itself. New ped-owned services need that real owner without replaying
        // unrelated weather, streaming and peer-recovery suites on every batch.
        ClockImageFixture fixture;
        fixture.Create(argv[1]);
        NativePedPhysicalScalarsOwnedSdk(fixture, currentWeaponOnly);
        std::puts("Production native registered-ped owner slice fixture completed");
        std::printf("Passed %u production clock service checks\n", checks.load());
        return 0;
    }
    ClockInterfaceContract();
    NativeContractValidation();
    NativeServiceAdmissionContract();
    ClockImageFixture fixture;
    fixture.Create(argv[1]);
    NativeReadOnlyGameTimeReader(fixture);
    NativeGameTimingPrerequisite(fixture);
    NativeStoredFPSReader(fixture);
    NativeTimingWritesAndRestoration(fixture);
    NativePauseSafeTimingOwner(fixture);
    NativeSystemStateOwnedWrites(fixture);
    NativeCheatFlagOwnedAccess(fixture);
    NativeWorldCheatRegistryOwner(fixture);
    NativeWeatherOwnedTransactions(fixture);
    NativeWeatherScalarOwnedReads(fixture);
    NativeWeaponOwnedWrites(fixture);
    NativeWeaponVectorAndBankOwner(fixture);
    NativeWeaponStatSdkOwner(fixture);
    NativeWeaponStatManagerOwner(fixture);
    NativeWeatherOverrideOwnedSdk(fixture);
    NativeRainOwnerAndWeatherSdk(fixture);
    NativeStatsOwnedSdk(fixture);
    NativePedPoolOwnedReads(fixture);
    NativePlayerInfoWantedOwnedSdk(fixture);
    NativeRopesOwnedSdk(fixture);
    NativeNamedGaragesOwnedSdk(fixture);
    NativePedTeleportOwnedSdk(fixture);
    NativePedVelocityOwnedSdk(fixture);
    NativePedReferenceOwnedSdk(fixture);
    NativePedMutationOwnedSdk(fixture);
    NativePedVitalsOwnedSdk(fixture);
    NativePedPhysicalScalarsOwnedSdk(fixture);
    NativePedPhysicalDynamicsOwnedSdk(fixture);
    NativePedMetadataOwnedSdk(fixture);
    NativePedObservationOwnedSdk(fixture);
    NativePedIntelligenceOwnedSdk(fixture);
    NativeTaskPoolOwnedSdk(fixture);
    NativeTaskSecondaryOwnedSdk(fixture);
    NativeModelBankOwnedSdk(fixture);
    NativeModelLoadedOwnedSdk(fixture);
    NativeStreamingRequestOwnedSdk(fixture);
    NativeModelOwnershipOwnedSdk(fixture);
    NativeReadSetAndRestoration(fixture);
    NativeCoherentCommandPublication(fixture);
    NativeReadOnlyRetirement(fixture);
    NativeFailureAndLease(fixture);
    NativeCaptureAndWraparound(fixture);
    NativePeerResumeRecovery(fixture);
    NativeCommittedHookResumeRecovery(fixture);
    NativeEngineThreadHandoff(fixture);
    NativeCallbackLifetime(fixture);
    NativePatchTransactions(fixture);
    NativeApplicationFrameOwner(fixture);
    NativeStartGameOwnedFrame(fixture);
    std::puts("Production native clock runtime fixture completed");
    std::printf("Passed %u production clock service checks\n", checks.load());
    return 0;
}
