/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CPed clothes dispatch, nullable names and explicit refusal
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedClothesDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <game/CWeapon.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ped_clothes_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified clothes fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedClothesFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    bool SameToken(const NativePedReferenceToken& first, const NativePedReferenceToken& second) noexcept
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    struct OtherOwners : NativePedVitalsAccess,
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
                         NativePedVisibilityAccess,
                         NativePedOnScreenAccess
    {
    };
    struct Grant final : NativePedWeaponGrantAccess
    {
        ClockResult GrantPedWeapon(const NativePedReferenceToken&, eWeaponType, std::uint32_t, std::uint32_t&) noexcept override
        {
            ped_clothes_fixture::Unqualified("GrantPedWeapon");
        }
    };
    struct Clear final : NativePedWeaponClearAccess
    {
        ClockResult ClearPedWeapon(const NativePedReferenceToken&, eWeaponType) noexcept override { ped_clothes_fixture::Unqualified("ClearPedWeapon"); }
    };
    struct Clothes final : NativePedClothesAccess
    {
        unsigned                       calls = 0;
        ClockResult                    result = ClockResult::Ok;
        NativePedReferenceToken        received{};
        const char*                    texture = nullptr;
        const char*                    model = nullptr;
        std::uint8_t                   part = 0;
        const NativePedReferenceToken* forbiddenBaseAddress = nullptr;
        const NativePedReferenceToken* forbiddenCallerAddress = nullptr;
        bool                           aliased = false;
        bool                           stableAcrossCallback = true;
        void (*callback)(void*) noexcept = nullptr;
        void*       context = nullptr;
        ClockResult SetPedClothes(const NativePedReferenceToken& token, const char* selectedTexture, const char* selectedModel,
                                  std::uint8_t selectedPart) noexcept override
        {
            ++calls;
            received = token;
            texture = selectedTexture;
            model = selectedModel;
            part = selectedPart;
            aliased = aliased || &token == forbiddenBaseAddress || &token == forbiddenCallerAddress;
            const auto before = token;
            if (callback)
                callback(context);
            stableAcrossCallback = stableAcrossCallback && SameToken(before, token);
            return result;
        }
    };
    struct UnavailableClothes final : NativePedClothesAccess
    {
    };
    struct CallbackContext
    {
        NativePedReferenceToken* callerToken = nullptr;
        int*                     callerPart = nullptr;
        const char**             callerTexture = nullptr;
        const char**             callerModel = nullptr;
        CPed*                    nestedPed = nullptr;
        unsigned                 calls = 0;
    };
    void MutateAndDispatchOther(void* opaque) noexcept
    {
        auto& context = *static_cast<CallbackContext*>(opaque);
        ++context.calls;
        *context.callerToken = {};
        *context.callerPart = 0;
        *context.callerTexture = nullptr;
        *context.callerModel = nullptr;
        context.nestedPed->SetClothesTextureAndModel(nullptr, nullptr, 17);
    }
    ped_clothes_fixture::PedClothesFixture MakePed(CGame& game, OtherOwners& other, Grant& grant, Clear& clear, NativePedClothesAccess& clothes,
                                                   const NativePedReferenceToken& token, ped_clothes_fixture::TaskScenario& tasks,
                                                   ped_clothes_fixture::WeaponLookup& lookup)
    {
        return ped_clothes_fixture::PedClothesFixture(game, other, other, other, other, other, other, other, other, other, other, other, other, other, other,
                                                      other, other, other, other, other, other, other, other, other, grant, clear, clothes, token, 7u, tasks,
                                                      lookup);
    }
    bool Refused(CPed& ped, ClockResult expected)
    {
        try
        {
            ped.SetClothesTextureAndModel(nullptr, nullptr, 3);
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            return text.find("SetClothesTextureAndModel") != std::string::npos &&
                   text.find(": " + std::to_string(static_cast<unsigned>(expected))) != std::string::npos;
        }
        return false;
    }
}

static_assert(std::is_abstract_v<mta::game_de::CPedClothesDE>);
static_assert(std::is_base_of_v<mta::game_de::CPedClearWeaponDE, mta::game_de::CPedClothesDE>);
static_assert(
    std::is_same_v<decltype(&mta::game_de::CPedClothesDE::SetClothesTextureAndModel), void (mta::game_de::CPedClothesDE::*)(const char*, const char*, int)>);

int main()
{
    try
    {
        ped_clothes_fixture::Game         game;
        ped_clothes_fixture::TaskScenario tasks;
        ped_clothes_fixture::WeaponLookup lookup;
        OtherOwners                       other;
        Grant                             grant;
        Clear                             clear;
        Clothes                           first, second;
        NativePedReferenceToken           caller{0xfedcba9876543210ull, 0x8000000000000001ull, 0xfffffffeu};
        const auto                        expected = caller;
        const NativePedReferenceToken     otherToken{17, 29, 3};
        auto                              ped = MakePed(game, other, grant, clear, first, caller, tasks, lookup);
        auto                              otherPed = MakePed(game, other, grant, clear, second, otherToken, tasks, lookup);
        CPed&                             sdk = ped;
        first.forbiddenBaseAddress = ped.FixtureTokenAddress();
        first.forbiddenCallerAddress = &caller;
        second.forbiddenBaseAddress = otherPed.FixtureTokenAddress();
        Check(!first.calls && !second.calls && !tasks.eventCount && !lookup.count,
              "constructing the actual clothes chain only borrows its owners; no native data initialization, rebuild or weapon lookup runs");

        const char texture[] = "hoodyablue";
        const char model[] = "hoodya";
        const char empty[] = "";
        // The SDK forwards pointer intent; only the genuine native admission
        // layer owns ASCII/name-cache qualification and valid-slot refusal.
        for (int raw = 0; raw != 256; ++raw)
        {
            const auto before = first.calls;
            sdk.SetClothesTextureAndModel(texture, model, raw);
            Check(first.calls == before + 1 && first.part == static_cast<std::uint8_t>(raw) && first.texture == texture && first.model == model &&
                      SameToken(first.received, expected) && !first.aliased && !second.calls,
                  "actual CPed dispatch forwards each narrowed byte, exact name pointers and complete copied token once without clamping or rebuilding");
        }
        for (const int raw : {-1, -256, -257, -511, -512, 256, 257, 511, 512, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        {
            const auto before = first.calls;
            sdk.SetClothesTextureAndModel(texture, model, raw);
            Check(first.calls == before + 1 && first.part == static_cast<std::uint8_t>(raw) && SameToken(first.received, expected),
                  "negative and full-width int boundaries retain classic's defined unsigned-byte conversion before owner admission");
        }
        const std::array<std::array<const char*, 2>, 6> names{
            {{{nullptr, nullptr}}, {{empty, nullptr}}, {{nullptr, empty}}, {{empty, empty}}, {{texture, model}}, {{model, texture}}}};
        for (const auto& pair : names)
        {
            const auto before = first.calls;
            sdk.SetClothesTextureAndModel(pair[0], pair[1], 4);
            Check(first.calls == before + 1 && first.texture == pair[0] && first.model == pair[1] && first.part == 4 && SameToken(first.received, expected),
                  "null and nonnull-empty texture/model intents remain distinct and both names keep their actual argument order");
        }

        int             callerPart = -239;
        const char*     callerTexture = texture;
        const char*     callerModel = model;
        CallbackContext context{&caller, &callerPart, &callerTexture, &callerModel, &otherPed, 0};
        first.callback = MutateAndDispatchOther;
        first.context = &context;
        const auto before = first.calls;
        sdk.SetClothesTextureAndModel(callerTexture, callerModel, callerPart);
        Check(first.calls == before + 1 && first.part == 17 && first.texture == texture && first.model == model && SameToken(first.received, expected) &&
                  first.stableAcrossCallback && !first.aliased,
              "caller variable mutation and nested other-ped dispatch cannot change the selected copied token, names or narrowed part");
        Check(context.calls == 1 && caller.version == 0 && callerPart == 0 && !callerTexture && !callerModel && second.calls == 1 && second.part == 17 &&
                  !second.texture && !second.model && SameToken(second.received, otherToken) && !second.aliased,
              "nested calls retain independent owner/token identities and the wrapper adds no compensation or rebuild");
        first.callback = nullptr;
        first.context = nullptr;

        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = status;
            const auto count = first.calls;
            Check(Refused(sdk, status) && first.calls == count + 1 && first.part == 3 && !first.texture && !first.model &&
                      SameToken(first.received, expected) && second.calls == 1 && !tasks.eventCount && !lookup.count,
                  "every owner refusal names the actual SDK method/status after one attempt with no fallback, retry, rebuild or unrelated SDK operation");
        }
        first.result = ClockResult::Ok;
        UnavailableClothes unavailable;
        auto               missing = MakePed(game, other, grant, clear, unavailable, expected, tasks, lookup);
        Check(Refused(missing, ClockResult::NotInitialized), "default access refuses instead of fabricating a successful clothing edit");
        const auto secondBefore = second.calls;
        {
            auto temporary = MakePed(game, other, grant, clear, second, otherToken, tasks, lookup);
            static_cast<CPed&>(temporary).SetClothesTextureAndModel(nullptr, nullptr, 0);
        }
        Check(second.calls == secondBefore + 1 && SameToken(second.received, otherToken) && !tasks.eventCount && !lookup.count,
              "borrowed wrapper destruction adds no native edit, clothes/player-data release or rebuild to the single requested call");
        std::puts("Actual ped clothes SDK fixture completed");
        std::printf("Passed %u actual ped clothes SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
