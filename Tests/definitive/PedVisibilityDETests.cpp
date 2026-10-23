/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual visibility virtuals on the borrowed ped SDK chain
 *****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedVisibilityDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ped_visibility_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified visibility fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedVisibilityFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned visibilityChecks = 0;
    void     CheckVisibility(bool condition, const char* reason)
    {
        ++visibilityChecks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    struct VisibilityAccesses : NativePedVitalsAccess,
                                NativePedMetadataAccess,
                                NativePedMutationAccess,
                                NativePedObservationAccess,
                                NativePedRotationAccess,
                                NativePedOxygenAccess,
                                NativePedDuckingAccess,
                                NativePedBikeFlagsAccess,
                                NativePedShotFlagsAccess,
                                NativePedBleedingAccess,
                                NativePedStayAccess,
                                NativePedFightingStyleAccess,
                                NativePedCurrentWeaponSlotAccess,
                                NativePedPlacementAccess,
                                NativePedWaterFlagsAccess,
                                NativePedWaterTaskAccess,
                                NativePedOrientationAccess,
                                NativePedAreaAccess,
                                NativePedFireAccess,
                                NativePedCollisionAccess,
                                NativePedGogglesAccess,
                                NativePedVisibilityAccess
    {
    };
    struct VisibilityOwner final : VisibilityAccesses
    {
        unsigned                reads = 0, writes = 0;
        bool                    value = false, requested = false;
        NativePedReferenceToken received;
        ClockResult             result = ClockResult::Ok;
        std::function<void()>   onWrite;
        ClockResult             ReadPedVisible(const NativePedReferenceToken& token, bool& output) noexcept override
        {
            ++reads;
            received = token;
            output = value;
            return result;
        }
        ClockResult SetPedVisible(const NativePedReferenceToken& token, bool visible) noexcept override
        {
            ++writes;
            received = token;
            requested = visible;
            // A native owner can have accepted effects before a later refusal.
            // This SDK fixture verifies forwarding never retries/rewinds them.
            value = visible;
            if (onWrite)
                onWrite();
            return result;
        }
    };
    bool SameVisibilityToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    template <class Operation>
    bool VisibilityRefuses(Operation operation, const char* method)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            return std::string(error.what()).find(method) != std::string::npos;
        }
        return false;
    }
}
static_assert(std::is_same_v<decltype(&mta::game_de::CPedVisibilityDE::IsVisible), bool (mta::game_de::CPedVisibilityDE::*)()>);
static_assert(std::is_same_v<decltype(&mta::game_de::CPedVisibilityDE::SetVisible), void (mta::game_de::CPedVisibilityDE::*)(bool)>);

int main()
{
    try
    {
        ped_visibility_fixture::Game         game;
        ped_visibility_fixture::TaskScenario scenario;
        scenario.available = true;
        VisibilityOwner                              owner;
        NativePedReferenceToken                      token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto                                   expected = token;
        ped_visibility_fixture::PedVisibilityFixture ped(game, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                                                         owner, owner, owner, owner, owner, owner, owner, owner, token, 0x12345678, scenario);
        token = {};
        CEntity& sdk = ped;
        CheckVisibility(!owner.reads && !owner.writes && !scenario.eventCount && ped.GetType() == 0x12345678,
                        "visibility construction copies CPU token/type with no native, game or task effects");
        for (const bool visible : {false, true, false, true})
        {
            owner.value = visible;
            const auto reads = owner.reads;
            CheckVisibility(sdk.IsVisible() == visible && owner.reads == reads + 1 && SameVisibilityToken(owner.received, expected),
                            "actual nonconst CEntity visibility dispatch freshly reads once with the immutable registered token");
            const auto writes = owner.writes;
            sdk.SetVisible(!visible);
            CheckVisibility(owner.writes == writes + 1 && owner.reads == reads + 1 && owner.requested == !visible && owner.value == !visible &&
                                SameVisibilityToken(owner.received, expected) && !scenario.eventCount,
                            "actual entity setter forwards one exact bool without reading back, task fallback or SDK retry");
        }
        CPedVisibilityDE& direct = ped;
        owner.value = true;
        const auto reads = owner.reads;
        CheckVisibility(direct.IsVisible() && static_cast<CPed&>(ped).IsVisible() && owner.reads == reads + 2,
                        "direct production and actual CPed static types share the same nonconst entity property");
        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.result = status;
            owner.value = true;
            const auto beforeReads = owner.reads, beforeWrites = owner.writes;
            CheckVisibility(VisibilityRefuses([&] { sdk.IsVisible(); }, "IsVisible") && owner.reads == beforeReads + 1,
                            "every refused read throws instead of reporting successful invisibility or cached presence");
            CheckVisibility(VisibilityRefuses([&] { sdk.SetVisible(false); }, "SetVisible") && owner.writes == beforeWrites + 1 && !owner.value &&
                                owner.reads == beforeReads + 1 && SameVisibilityToken(owner.received, expected) && !scenario.eventCount,
                            "refused entered setter preserves its mock accepted prefix with no second intent, SDK rollback or task calls");
        }
        owner.result = ClockResult::Ok;
        owner.onWrite = [&] { token = {99, 100, 101}; };
        sdk.SetVisible(true);
        CheckVisibility(owner.value && SameVisibilityToken(owner.received, expected), "later caller-token mutation cannot rebind the actual SDK object");
        VisibilityAccesses                           olderOwner;
        ped_visibility_fixture::PedVisibilityFixture older(game, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner,
                                                           olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner,
                                                           olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, olderOwner, expected, 0, scenario);
        CheckVisibility(VisibilityRefuses([&] { older.IsVisible(); }, "IsVisible") && VisibilityRefuses([&] { older.SetVisible(true); }, "SetVisible") &&
                            !scenario.eventCount,
                        "older default-refusing owners supply neither successful visibility nor an unsupported setter success");
        std::puts("Actual ped visibility SDK fixture completed");
        std::printf("Passed %u actual ped visibility SDK checks (%zu-bit; no native engine code executed)\n", visibilityChecks, sizeof(void*) * 8);
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped visibility SDK failed after %u checks: %s\n", visibilityChecks, error.what());
        return EXIT_FAILURE;
    }
}
