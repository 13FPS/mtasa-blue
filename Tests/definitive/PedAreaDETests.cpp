/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual raw area BYTE SDK dispatch on the immutable ped token chain
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedAreaDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
namespace ped_area_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified area fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedAreaFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned areaChecks = 0;
    void     CheckArea(bool ok, const char* why)
    {
        ++areaChecks;
        if (!ok)
            throw std::runtime_error(why);
    }
    struct AreaAccesses : NativePedVitalsAccess,
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
                          NativePedAreaAccess
    {
    };
    struct AreaOwner final : AreaAccesses
    {
        NativePedReferenceToken received;
        unsigned                reads = 0, writes = 0;
        std::uint8_t            nativeByte = 0, targetByte = 0;
        ClockResult             result = ClockResult::Ok;
        ClockResult             ReadPedAreaCode(const NativePedReferenceToken& token, std::uint8_t& output) noexcept override
        {
            ++reads;
            received = token;
            output = nativeByte;
            return result;
        }
        ClockResult SetPedAreaCode(const NativePedReferenceToken& token, std::uint8_t target) noexcept override
        {
            ++writes;
            received = token;
            targetByte = target;
            if (result == ClockResult::Ok)
                nativeByte = target;
            return result;
        }
    };
    bool SameToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    template <class Operation>
    void RefusedArea(AreaOwner& owner, Operation operation, const char* name)
    {
        const auto before = owner.reads + owner.writes;
        bool       threw = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            threw = std::string(error.what()).find(name) != std::string::npos;
        }
        CheckArea(threw && owner.reads + owner.writes == before + 1, "actual area SDK names and propagates owner refusal once, without retry/cache/repair");
    }
}
int main()
{
    try
    {
        ped_area_fixture::Game           game;
        AreaOwner                        owner;
        NativePedReferenceToken          token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto                       expected = token;
        ped_area_fixture::PedAreaFixture ped(game, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                                             owner, owner, owner, token, 0x12345678);
        CEntity&                         entity = ped;
        token = {};
        CheckArea(!owner.reads && !owner.writes && ped.GetType() == 0x12345678 && !ped.GetCustomMoveAnim(),
                  "area constructor only copies borrowed immutable token, preserving actual wrapper metadata without native/game I/O");
        for (unsigned value = 0; value != 256; ++value)
        {
            owner.nativeByte = static_cast<std::uint8_t>(value);
            auto before = owner.reads;
            CheckArea(entity.GetAreaCode() == value && owner.reads == before + 1 && SameToken(owner.received, expected),
                      "actual inherited CEntity getter dispatch preserves all256 unsigned BYTE values and copied token");
            const auto target = static_cast<BYTE>(value ^ 0xffu);
            before = owner.writes;
            entity.SetAreaCode(target);
            CheckArea(owner.writes == before + 1 && owner.targetByte == target && SameToken(owner.received, expected) && !ped.repairCalls,
                      "actual setter forwards exactBYTE once without interior streaming/player cleanup, clamping or physics repair");
        }
        for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                                  ClockResult::WriteFailure, ClockResult::Expired, ClockResult::OwnerConflict})
        {
            owner.result = status;
            RefusedArea(owner, [&] { entity.GetAreaCode(); }, "GetAreaCode");
            RefusedArea(owner, [&] { entity.SetAreaCode(255); }, "SetAreaCode");
        }
        AreaAccesses                     unavailable;
        ped_area_fixture::PedAreaFixture missing(game, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                 unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                 unavailable, unavailable, expected, 0);
        bool                             refused = false;
        try
        {
            missing.GetAreaCode();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        CheckArea(refused, "default access refuses unavailable area storage instead of fabricating exterior area0");
        std::puts("Actual ped area BYTE SDK fixture completed");
        std::printf("Passed %u ped area SDK checks (64-bit; no native engine code executed)\n", areaChecks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", areaChecks, error.what());
        return 1;
    }
}
