/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited ClearWeapon dispatch, identity and explicit refusal
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedClearWeaponDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <game/CWeapon.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ped_clear_weapon_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified ClearWeapon fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedClearWeaponFixture.generated.h"

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
        unsigned    calls = 0;
        ClockResult GrantPedWeapon(const NativePedReferenceToken&, eWeaponType, std::uint32_t, std::uint32_t&) noexcept override
        {
            ++calls;
            return ClockResult::NotInitialized;
        }
    };
    struct Clear final : NativePedWeaponClearAccess
    {
        unsigned                       calls = 0;
        ClockResult                    result = ClockResult::Ok;
        NativePedReferenceToken        received{};
        std::uint32_t                  type = 0;
        const NativePedReferenceToken* forbiddenBaseAddress = nullptr;
        const NativePedReferenceToken* forbiddenCallerAddress = nullptr;
        bool                           aliased = false;
        bool                           stableAcrossCallback = true;
        void (*callback)(void*) noexcept = nullptr;
        void* context = nullptr;

        ClockResult ClearPedWeapon(const NativePedReferenceToken& token, eWeaponType selected) noexcept override
        {
            ++calls;
            received = token;
            type = static_cast<std::uint32_t>(selected);
            aliased = aliased || &token == forbiddenBaseAddress || &token == forbiddenCallerAddress;
            const auto before = token;
            if (callback)
                callback(context);
            stableAcrossCallback = stableAcrossCallback && SameToken(before, token);
            return result;
        }
    };
    struct UnavailableClear final : NativePedWeaponClearAccess
    {
    };
    struct CallbackContext
    {
        NativePedReferenceToken* callerToken = nullptr;
        eWeaponType*             callerType = nullptr;
        CPed*                    nestedPed = nullptr;
        unsigned                 calls = 0;
    };
    void MutateAndDispatchOther(void* opaque) noexcept
    {
        auto& context = *static_cast<CallbackContext*>(opaque);
        ++context.calls;
        if (context.callerToken)
            *context.callerToken = {};
        if (context.callerType)
            *context.callerType = WEAPONTYPE_INVALID;
        if (context.nestedPed)
            context.nestedPed->ClearWeapon(WEAPONTYPE_CAMERA);
    }
    ped_clear_weapon_fixture::PedClearWeaponFixture MakePed(CGame& game, OtherOwners& other, NativePedWeaponGrantAccess& grant,
                                                            NativePedWeaponClearAccess& clear, const NativePedReferenceToken& token,
                                                            ped_clear_weapon_fixture::TaskScenario& tasks, ped_clear_weapon_fixture::WeaponLookup& lookup)
    {
        return ped_clear_weapon_fixture::PedClearWeaponFixture(game, other, other, other, other, other, other, other, other, other, other, other, other, other,
                                                               other, other, other, other, other, other, other, other, other, other, grant, clear, token, 7u,
                                                               tasks, lookup);
    }
    bool Refused(CPed& ped, eWeaponType type, ClockResult expected)
    {
        try
        {
            ped.ClearWeapon(type);
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            return text.find("ClearWeapon") != std::string::npos && text.find(": " + std::to_string(static_cast<unsigned>(expected))) != std::string::npos;
        }
        return false;
    }
}

static_assert(std::is_abstract_v<mta::game_de::CPedClearWeaponDE>);
static_assert(std::is_base_of_v<mta::game_de::CPedGiveWeaponDE, mta::game_de::CPedClearWeaponDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CPedClearWeaponDE::ClearWeapon), void (mta::game_de::CPedClearWeaponDE::*)(eWeaponType)>);
static_assert(sizeof(eWeaponType) == sizeof(std::uint32_t));

int main()
{
    try
    {
        ped_clear_weapon_fixture::Game         game;
        ped_clear_weapon_fixture::TaskScenario tasks;
        ped_clear_weapon_fixture::WeaponLookup lookup;
        OtherOwners                            other;
        Grant                                  grant;
        Clear                                  first, second;
        NativePedReferenceToken                caller{0xfedcba9876543210ull, 0x8000000000000001ull, 0xfffffffeu};
        const auto                             expected = caller;
        const NativePedReferenceToken          otherToken{17, 29, 3};
        auto                                   ped = MakePed(game, other, grant, first, caller, tasks, lookup);
        auto                                   otherPed = MakePed(game, other, grant, second, otherToken, tasks, lookup);
        CPed&                                  sdk = ped;
        first.forbiddenBaseAddress = ped.FixtureTokenAddress();
        first.forbiddenCallerAddress = &caller;
        second.forbiddenBaseAddress = otherPed.FixtureTokenAddress();
        Check(!first.calls && !second.calls && !grant.calls && !tasks.eventCount && !lookup.count,
              "constructing the actual ClearWeapon chain borrows owners without native initialization, model preparation or a wrapper lookup");

        // eWeaponType is an actual unfixed SDK enum with maximum enumerator
        // 255. Exercise its entire defined language range without constructing
        // invalid high-bit enum objects; native raw-DWORD tests are separate.
        for (std::uint32_t raw = 0; raw != 256; ++raw)
        {
            const auto before = first.calls;
            sdk.ClearWeapon(static_cast<eWeaponType>(raw));
            Check(first.calls == before + 1 && first.type == raw && SameToken(first.received, expected) && !first.aliased && !second.calls,
                  "actual CPed void dispatch passes each defined enum value and complete copied token to its selected owner once without slot remapping");
        }

        eWeaponType     callerType = WEAPONTYPE_REMOTE_SATCHEL_CHARGE;
        CallbackContext context{&caller, &callerType, &otherPed, 0};
        first.callback = MutateAndDispatchOther;
        first.context = &context;
        const auto before = first.calls;
        sdk.ClearWeapon(callerType);
        Check(first.calls == before + 1 && first.type == static_cast<std::uint32_t>(WEAPONTYPE_REMOTE_SATCHEL_CHARGE) && SameToken(first.received, expected) &&
                  first.stableAcrossCallback && !first.aliased,
              "caller storage mutation and a nested different receiver cannot change the current copied token or selected weapon");
        Check(context.calls == 1 && caller.version == 0 && callerType == WEAPONTYPE_INVALID && second.calls == 1 &&
                  second.type == static_cast<std::uint32_t>(WEAPONTYPE_CAMERA) && SameToken(second.received, otherToken) && !second.aliased,
              "nested calls retain separate borrowed owner/token identities and callback effects are not silently undone");
        first.callback = nullptr;
        first.context = nullptr;

        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = status;
            const auto count = first.calls;
            Check(Refused(sdk, WEAPONTYPE_INFRARED, status) && first.calls == count + 1 && first.type == static_cast<std::uint32_t>(WEAPONTYPE_INFRARED) &&
                      SameToken(first.received, expected) && second.calls == 1 && !grant.calls && !tasks.eventCount && !lookup.count,
                  "every owner refusal names the actual SDK method/status after one attempt, with no grant, lookup, equip, cleanup or retry callback");
        }
        first.result = ClockResult::Ok;
        static_cast<CPedClearWeaponDE&>(ped).ClearWeapon(WEAPONTYPE_UNARMED);
        Check(first.type == static_cast<std::uint32_t>(WEAPONTYPE_UNARMED) && SameToken(first.received, expected) && first.stableAcrossCallback &&
                  !first.aliased && !grant.calls && !tasks.eventCount && !lookup.count,
              "a later direct inherited call uses fresh owner state without substituting unarmed/slot defaults or requiring a native weapon bank");

        UnavailableClear unavailable;
        auto             missing = MakePed(game, other, grant, unavailable, expected, tasks, lookup);
        Check(Refused(missing, WEAPONTYPE_PISTOL, ClockResult::NotInitialized),
              "default access explicitly refuses rather than fabricating a successful void operation");

        const auto secondBefore = second.calls;
        {
            auto temporary = MakePed(game, other, grant, second, otherToken, tasks, lookup);
            static_cast<CPed&>(temporary).ClearWeapon(WEAPONTYPE_NIGHTVISION);
        }
        Check(second.calls == secondBefore + 1 && SameToken(second.received, otherToken) && !grant.calls && !tasks.eventCount && !lookup.count,
              "the wrapper's destruction adds no native clear/release or unrelated SDK operation to its borrowed lifetime");
        std::puts("Actual ped ClearWeapon SDK fixture completed");
        std::printf("Passed %u actual ped ClearWeapon SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
