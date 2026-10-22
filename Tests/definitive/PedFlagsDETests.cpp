/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute genuine ped flag SDK forwards without a complete factory
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedFlagsDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>
namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::abort();
    }
}
#include "PedFlagsFixture.generated.h"
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
    bool SameToken(const NativePedReferenceToken& a, const NativePedReferenceToken& b)
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    struct Owner final : NativePedVitalsAccess,
                         NativePedMetadataAccess,
                         NativePedMutationAccess,
                         NativePedObservationAccess,
                         NativePedRotationAccess,
                         NativePedOxygenAccess,
                         NativePedDuckingAccess,
                         NativePedBikeFlagsAccess,
                         NativePedShotFlagsAccess
    {
        unsigned                bikeReads = 0, bikeWrites = 0, shotReads = 0, shotWrites = 0;
        std::uint32_t           bikeValue = 0;
        bool                    shotValue = false;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken token;
        ClockResult             ReadPedCantBeKnockedOffBike(const NativePedReferenceToken& selected, std::uint32_t& value) noexcept override
        {
            ++bikeReads;
            token = selected;
            value = bikeValue;
            return result;
        }
        ClockResult SetPedCantBeKnockedOffBike(const NativePedReferenceToken& selected, std::uint32_t value) noexcept override
        {
            ++bikeWrites;
            token = selected;
            bikeValue = value;
            return result;
        }
        ClockResult ReadPedCanBeShotInVehicle(const NativePedReferenceToken& selected, bool& value) noexcept override
        {
            ++shotReads;
            token = selected;
            value = shotValue;
            return result;
        }
        ClockResult SetPedCanBeShotInVehicle(const NativePedReferenceToken& selected, bool value) noexcept override
        {
            ++shotWrites;
            token = selected;
            shotValue = value;
            return result;
        }
    };
    struct Absent final : NativePedVitalsAccess,
                          NativePedMetadataAccess,
                          NativePedMutationAccess,
                          NativePedObservationAccess,
                          NativePedRotationAccess,
                          NativePedOxygenAccess,
                          NativePedDuckingAccess,
                          NativePedBikeFlagsAccess,
                          NativePedShotFlagsAccess
    {
    };
    template <class Action>
    void Refuses(Action action, const char* operation)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find(operation) != std::string::npos;
        }
        Check(refused, "every owner refusal remains an explicit operation-specific SDK failure");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedFlagsDE> && std::is_base_of_v<CPedStateDE, CPedFlagsDE>);
        Owner                    owner;
        NativePedReferenceToken  token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        fixture::PedFlagsFixture ped(owner, owner, owner, owner, owner, owner, owner, owner, owner, token, CIVILIAN_PED);
        CPed*                    sdk = &ped;
        const CPed*              constant = &ped;
        for (const auto input : std::array<std::uint32_t, 9>{0, 1, 2, 3, 4, 5, 0x10000, 0x80000002, 0xffffffff})
        {
            const auto writes = owner.bikeWrites;
            sdk->SetCantBeKnockedOffBike(input);
            Check(owner.bikeWrites == writes + 1 && owner.bikeValue == input && SameToken(owner.token, token),
                  "actual SDK forwards full DWORD bike intent once; native two-bit storage owns truncation rather than a gameplay clamp");
        }
        for (std::uint32_t value = 0; value != 4; ++value)
        {
            owner.bikeValue = value;
            const auto reads = owner.bikeReads;
            Check(constant->GetCantBeKnockedOffBike() == value && owner.bikeReads == reads + 1 && SameToken(owner.token, token),
                  "actual const getter preserves all four raw two-bit values on the exact registered token");
        }
        for (const bool value : {false, true})
        {
            owner.shotValue = value;
            const auto reads = owner.shotReads, writes = owner.shotWrites;
            Check(constant->GetCanBeShotInVehicle() == value && owner.shotReads == reads + 1 && SameToken(owner.token, token),
                  "actual const shot getter preserves native true and false without inferring vehicle/task presence");
            sdk->SetCanBeShotInVehicle(value);
            Check(owner.shotWrites == writes + 1 && owner.shotValue == value && SameToken(owner.token, token),
                  "actual bool shot setter forwards only independently qualified intent once");
        }
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.result = static_cast<ClockResult>(status);
            const auto br = owner.bikeReads, bw = owner.bikeWrites, sr = owner.shotReads, sw = owner.shotWrites;
            Refuses([&] { (void)constant->GetCantBeKnockedOffBike(); }, "GetCantBeKnockedOffBike");
            Refuses([&] { sdk->SetCantBeKnockedOffBike(0xffffffff); }, "SetCantBeKnockedOffBike");
            Refuses([&] { (void)constant->GetCanBeShotInVehicle(); }, "GetCanBeShotInVehicle");
            Refuses([&] { sdk->SetCanBeShotInVehicle(true); }, "SetCanBeShotInVehicle");
            Check(owner.bikeReads == br + 1 && owner.bikeWrites == bw + 1 && owner.shotReads == sr + 1 && owner.shotWrites == sw + 1 &&
                      owner.bikeValue == 0xffffffff && owner.shotValue && constant->GetType() == CIVILIAN_PED,
                  "uncertain native flag effects never trigger SDK replay/rollback or change independent CPU wrapper identity");
        }
        Absent                   absent;
        fixture::PedFlagsFixture unbound(absent, absent, absent, absent, absent, absent, absent, absent, absent, token, PLAYER_PED);
        Refuses([&] { (void)unbound.GetCantBeKnockedOffBike(); }, "GetCantBeKnockedOffBike");
        Refuses([&] { unbound.SetCantBeKnockedOffBike(1); }, "SetCantBeKnockedOffBike");
        Refuses([&] { (void)unbound.GetCanBeShotInVehicle(); }, "GetCanBeShotInVehicle");
        Refuses([&] { unbound.SetCanBeShotInVehicle(false); }, "SetCanBeShotInVehicle");
        std::puts("Production MTA ped bike and shot flags SDK fixture completed");
        std::printf("Passed %u actual CPed flag SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped flags SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
