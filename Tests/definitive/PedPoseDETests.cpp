/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CEntity placement overloads and prefix commit in the DE ped
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedPoseDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
namespace ped_pose_fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::abort();
    }
}
#include "PedPoseFixture.generated.h"
namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool ok, const char* why)
    {
        ++checks;
        if (!ok)
            throw std::runtime_error(why);
    }
    bool SameVector(const CVector& a, const CVector& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }
    bool SameToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    struct Accesses : NativePedVitalsAccess,
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
                      NativePedPlacementAccess
    {
    };
    struct Owner final : Accesses
    {
        std::string             events;
        CVector                 position{1, 2, 3}, requested;
        bool                    matrix = true;
        ClockResult             positionResult = ClockResult::Ok, observationResult = ClockResult::Ok, completionResult = ClockResult::Ok;
        NativePedReferenceToken token;
        ClockResult             SetReferencedPedPosition(const NativePedReferenceToken& selected, const CVector& value) noexcept override
        {
            events += 'P';
            token = selected;
            requested = value;
            if (positionResult == ClockResult::Ok)
                position = value;
            return positionResult;
        }
        ClockResult CompleteReferencedPedTeleport(const NativePedReferenceToken& selected, const CVector& value) noexcept override
        {
            events += 'T';
            token = selected;
            requested = value;
            return completionResult;
        }
        ClockResult ReadReferencedPedTransform(const NativePedReferenceToken& selected, NativePedTransformReader::Value& value) noexcept override
        {
            events += 'M';
            token = selected;
            value = {};
            value.matrix = matrix ? 1 : 0;
            return observationResult;
        }
        ClockResult BorrowReferencedPedPosition(const NativePedReferenceToken& selected, CVector*& value) noexcept override
        {
            events += 'G';
            token = selected;
            value = &position;
            return ClockResult::Ok;
        }
    };
    template <class Action>
    void Refuses(Action action)
    {
        bool threw = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            threw = std::string(error.what()).find("DE ped pose owner refused") != std::string::npos;
        }
        Check(threw, "pose refusal remains explicit without fake placement, fallback or repeated effects");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedPoseDE> && std::is_base_of_v<CPedWeaponSelectionDE, CPedPoseDE>);
        Owner                            owner;
        const NativePedReferenceToken    token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        ped_pose_fixture::PedPoseFixture ped(owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, token,
                                             CIVILIAN_PED);
        CEntity&                         sdk = ped;
        Check(owner.events.empty() && ped.GetType() == CIVILIAN_PED && !ped.GetCustomMoveAnim(),
              "new pose layer preserves inherited CPU bookkeeping and performs no constructor native entry");
        const CVector first(-0.0f, 245.5f, -12.25f);
        sdk.SetPosition(first.fX, first.fY, first.fZ);
        Check(owner.events == "P" && SameVector(owner.position, first) && SameToken(owner.token, token),
              "actual scalar SDK setter forwards raw XYZ and exact immutable token once");
        ped.repairCallback = [&](const CMatrix& matrix) { owner.position = matrix.vPos; };
        owner.position.fX = std::numeric_limits<float>::quiet_NaN();
        Check(sdk.GetPosition() == &owner.position && SameVector(ped.repairRequested.vPos, first) && SameVector(owner.position, first) && ped.repairCalls == 1,
              "successful scalar placement commits the same last-good CPU position used by existing actual MTA getter repair policy");
        CVector second(11, -22, 33);
        owner.events.clear();
        sdk.SetPosition(&second);
        Check(owner.events == "P" && SameVector(owner.position, second) && SameToken(owner.token, token),
              "actual pointer overload dispatches through the qualified scalar implementation");
        sdk.SetPosition(static_cast<CVector*>(nullptr));
        Check(owner.events == "P", "null position pointer preserves the classic no-op without even borrowing an owner");
        for (const auto bad : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            for (unsigned component = 0; component != 3; ++component)
            {
                CVector invalid(1, 2, 3);
                if (component == 0)
                    invalid.fX = bad;
                if (component == 1)
                    invalid.fY = bad;
                if (component == 2)
                    invalid.fZ = bad;
                owner.events.clear();
                sdk.SetPosition(invalid.fX, invalid.fY, invalid.fZ);
                sdk.SetPosition(&invalid);
                sdk.Teleport(invalid.fX, invalid.fY, invalid.fZ);
                Check(owner.events.empty() && SameVector(owner.position, second),
                      "both SetPosition overloads and Teleport reject nonfinite input before matrix lookup or native calls");
            }
        for (const bool matrix : {false, true})
        {
            owner.events.clear();
            owner.matrix = matrix;
            const CVector target(140.5f, -210.25f, 72);
            sdk.Teleport(target.fX, target.fY, target.fZ);
            Check(
                owner.events == (matrix ? "MPT" : "MP") && SameVector(owner.position, target) && SameVector(owner.requested, target) &&
                    SameToken(owner.token, token),
                "actual Teleport observes its raw matrix branch once, commits placement first, and completes native action only for the matrix-backed branch");
            owner.position.fZ = std::numeric_limits<float>::quiet_NaN();
            sdk.GetPosition();
            Check(SameVector(ped.repairRequested.vPos, target), "both Teleport branches commit last-good placement before any subsequent repair");
        }
        for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::WriteFailure,
                                  ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.matrix = true;
            owner.events.clear();
            owner.positionResult = status;
            Refuses([&] { sdk.Teleport(51, 52, 53); });
            Check(owner.events == "MP" && SameToken(owner.token, token), "position refusal never advances to native completion or retries the write");
            owner.positionResult = ClockResult::Ok;
            owner.events.clear();
            owner.observationResult = status;
            Refuses([&] { sdk.Teleport(61, 62, 63); });
            Check(owner.events == "M", "unqualified matrix observation cannot be treated as inline absence or advance to placement");
            owner.observationResult = ClockResult::Ok;
            owner.events.clear();
            owner.completionResult = status;
            const CVector prefix(71, 72, 73);
            Refuses([&] { sdk.Teleport(prefix.fX, prefix.fY, prefix.fZ); });
            Check(owner.events == "MPT" && SameVector(owner.position, prefix),
                  "native completion refusal preserves its already-accepted placement prefix without rollback/replay");
            owner.position.fX = std::numeric_limits<float>::quiet_NaN();
            sdk.GetPosition();
            Check(SameVector(ped.repairRequested.vPos, prefix), "last-good CPU state remains committed when the later teleport stage refuses");
            owner.completionResult = ClockResult::Ok;
        }
        Accesses                         unavailable;
        ped_pose_fixture::PedPoseFixture absent(unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, unavailable,
                                                unavailable, unavailable, unavailable, unavailable, unavailable, unavailable, token, PLAYER_PED);
        Refuses([&] { absent.SetPosition(1, 2, 3); });
        Refuses([&] { absent.Teleport(1, 2, 3); });
        std::puts("Production MTA ped pose SDK fixture completed");
        std::printf("Passed %u actual CPed pose SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped pose SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
