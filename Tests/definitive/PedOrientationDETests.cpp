/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA threefloat orientation forwarding without native execution
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedOrientationDE.h"
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <type_traits>
namespace ped_orientation_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified orientation fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedOrientationFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned orientationChecks = 0;
    void     CheckOrientation(bool value, const char* why)
    {
        ++orientationChecks;
        if (!value)
            throw std::runtime_error(why);
    }
    struct OrientationAccesses : NativePedVitalsAccess,
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
                                 NativePedOrientationAccess
    {
    };
    struct OrientationOwner final : OrientationAccesses
    {
        NativePedReferenceToken observedToken;
        CVector                 observedAngles;
        unsigned                calls = 0;
        ClockResult             result = ClockResult::Ok;
        ClockResult             SetPedOrientation(const NativePedReferenceToken& token, const CVector& targetRadians) noexcept override
        {
            ++calls;
            observedToken = token;
            std::memcpy(&observedAngles, &targetRadians, sizeof(observedAngles));
            return result;
        }
    };
    float RawFloat(std::uint32_t bits)
    {
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedOrientationDE> && std::is_base_of_v<CPedWaterDE, CPedOrientationDE>);
        ped_orientation_fixture::Game                  game;
        OrientationOwner                               owner;
        const NativePedReferenceToken                  token{0x123456789abcdef0ULL, 0xfedcba9876543210ULL, 17};
        ped_orientation_fixture::PedOrientationFixture ped(game, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner,
                                                           owner, owner, owner, owner, token, 0x12345678);
        CEntity&                                       entity = ped;
        CheckOrientation(!owner.calls && ped.GetType() == 0x12345678 && !ped.GetCustomMoveAnim(),
                         "orientation constructor performs no native or borrowed-game I/O");
        for (const CVector requested : {CVector(0, RawFloat(0x80000000), 0), CVector(1.25f, -2.5f, 3.75f), CVector(1000.5f, -999.25f, 17),
                                        CVector(RawFloat(0x7fc01234), RawFloat(0x7f800000), RawFloat(0xff800000))})
        {
            const auto before = owner.calls;
            entity.SetOrientation(requested.fX, requested.fY, requested.fZ);
            CheckOrientation(owner.calls == before + 1 && !std::memcmp(&owner.observedAngles, &requested, sizeof(requested)) &&
                                 owner.observedToken.version == token.version && owner.observedToken.ownerGeneration == token.ownerGeneration &&
                                 owner.observedToken.slot == token.slot && !ped.repairCalls,
                             "actual CEntity virtual preserves every supplied radians bit through one same-token dispatch, without conversion/clamp/heading "
                             "cache or physics repair");
        }
        for (const auto refused :
             {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::Expired, ClockResult::UnsupportedBuild, ClockResult::BindingChanged,
              ClockResult::OwnerConflict, ClockResult::ReadFailure, ClockResult::OriginalException, ClockResult::Stopped, ClockResult::Busy})
        {
            owner.result = refused;
            const auto before = owner.calls;
            bool       caught = false;
            try
            {
                entity.SetOrientation(1, 2, 3);
            }
            catch (const std::runtime_error&)
            {
                caught = true;
            }
            CheckOrientation(caught && owner.calls == before + 1 && !ped.repairCalls,
                             "native refusal or entered failure is explicit; SDK never replays phases or fabricates repair");
        }
        OrientationAccesses                            absent;
        ped_orientation_fixture::PedOrientationFixture unavailable(game, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent,
                                                                   absent, absent, absent, absent, absent, absent, token, 2);
        bool                                           caught = false;
        try
        {
            unavailable.SetOrientation(1, 2, 3);
        }
        catch (const std::runtime_error&)
        {
            caught = true;
        }
        CheckOrientation(caught, "default owner grants no orientation effect or complete ped factory");
        std::puts("Production MTA ped native orientation SDK fixture completed");
        std::printf("Passed %u actual CPed orientation SDK checks (%zu-bit)\n", orientationChecks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Orientation SDK failed after %u checks: %s\n", orientationChecks, error.what());
        return 1;
    }
}
