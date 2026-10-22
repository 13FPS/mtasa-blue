/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute actual bleeding/stay SDK methods without a ped factory
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedBehaviorDE.h"
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
#include "PedBehaviorFixture.generated.h"
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
                         NativePedShotFlagsAccess,
                         NativePedBleedingAccess,
                         NativePedStayAccess
    {
        unsigned                bleedReads = 0, bleedWrites = 0, stayReads = 0, stayWrites = 0;
        bool                    bleedValue = false, stayValue = false;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken token;
        ClockResult             ReadPedBleeding(const NativePedReferenceToken& selected, bool& value) noexcept override
        {
            ++bleedReads;
            token = selected;
            value = bleedValue;
            return result;
        }
        ClockResult SetPedBleeding(const NativePedReferenceToken& selected, bool value) noexcept override
        {
            ++bleedWrites;
            token = selected;
            bleedValue = value;
            return result;
        }
        ClockResult ReadPedStayInSamePlace(const NativePedReferenceToken& selected, bool& value) noexcept override
        {
            ++stayReads;
            token = selected;
            value = stayValue;
            return result;
        }
        ClockResult SetPedStayInSamePlace(const NativePedReferenceToken& selected, bool value) noexcept override
        {
            ++stayWrites;
            token = selected;
            stayValue = value;
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
                          NativePedShotFlagsAccess,
                          NativePedBleedingAccess,
                          NativePedStayAccess
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
        Check(refused, "refused native bool properties must remain explicit SDK failures instead of successful false");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedBehaviorDE> && std::is_base_of_v<CPedFlagsDE, CPedBehaviorDE>);
        Owner                       owner;
        NativePedReferenceToken     token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        fixture::PedBehaviorFixture ped(owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, owner, token, CIVILIAN_PED);
        CPed*                       sdk = &ped;
        const CPed*                 constant = &ped;
        for (const bool value : {false, true})
        {
            owner.bleedValue = value;
            owner.stayValue = !value;
            const auto br = owner.bleedReads, bw = owner.bleedWrites, sr = owner.stayReads, sw = owner.stayWrites;
            Check(constant->IsBleeding() == value && owner.bleedReads == br + 1 && SameToken(owner.token, token),
                  "actual const IsBleeding returns the registered owner's true or false observation once");
            Check(constant->GetStayInSamePlace() == !value && owner.stayReads == sr + 1 && SameToken(owner.token, token),
                  "actual const stay getter remains independent of bleeding and task presence");
            sdk->SetBleeding(!value);
            Check(owner.bleedWrites == bw + 1 && owner.bleedValue == !value && owner.stayValue == !value && SameToken(owner.token, token),
                  "actual bleeding setter forwards exactly one bool intent without replacing another property");
            sdk->SetStayInSamePlace(value);
            Check(owner.stayWrites == sw + 1 && owner.stayValue == value && owner.bleedValue == !value && SameToken(owner.token, token),
                  "actual stay setter forwards only independently qualified storage intent once");
        }
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.result = static_cast<ClockResult>(status);
            const auto br = owner.bleedReads, bw = owner.bleedWrites, sr = owner.stayReads, sw = owner.stayWrites;
            Refuses([&] { (void)constant->IsBleeding(); }, "IsBleeding");
            Refuses([&] { sdk->SetBleeding(true); }, "SetBleeding");
            Refuses([&] { (void)constant->GetStayInSamePlace(); }, "GetStayInSamePlace");
            Refuses([&] { sdk->SetStayInSamePlace(false); }, "SetStayInSamePlace");
            Check(owner.bleedReads == br + 1 && owner.bleedWrites == bw + 1 && owner.stayReads == sr + 1 && owner.stayWrites == sw + 1 && owner.bleedValue &&
                      !owner.stayValue && constant->GetType() == CIVILIAN_PED,
                  "uncertain bool effects remain persistent without SDK replay/rollback or changing CPU wrapper identity");
        }
        Absent                      absent;
        fixture::PedBehaviorFixture unbound(absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, absent, token, PLAYER_PED);
        Refuses([&] { (void)unbound.IsBleeding(); }, "IsBleeding");
        Refuses([&] { unbound.SetBleeding(false); }, "SetBleeding");
        Refuses([&] { (void)unbound.GetStayInSamePlace(); }, "GetStayInSamePlace");
        Refuses([&] { unbound.SetStayInSamePlace(true); }, "SetStayInSamePlace");
        std::puts("Production MTA ped bleeding and stay SDK fixture completed");
        std::printf("Passed %u actual CPed behavior SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped behavior SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
