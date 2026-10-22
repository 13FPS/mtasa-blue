/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real SDK raw oxygen, ducking and independent CPU wrapper identity
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedStateDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
#include "PedStateFixture.generated.h"
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
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        return bits;
    }
    float Float(std::uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
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
                         NativePedDuckingAccess
    {
        unsigned                reads = 0, writes = 0, duckReads = 0;
        std::uint32_t           bits = 0;
        bool                    duck = false;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken token;
        ClockResult             ReadPedOxygenLevel(const NativePedReferenceToken& selected, float& value) noexcept override
        {
            ++reads;
            token = selected;
            std::memcpy(&value, &bits, 4);
            return result;
        }
        ClockResult SetPedOxygenLevel(const NativePedReferenceToken& selected, float value) noexcept override
        {
            ++writes;
            token = selected;
            std::memcpy(&bits, &value, 4);
            return result;
        }
        ClockResult ReadPedDucking(const NativePedReferenceToken& selected, bool& value) noexcept override
        {
            ++duckReads;
            token = selected;
            value = duck;
            return result;
        }
    };
    struct Absent final : NativePedVitalsAccess,
                          NativePedMetadataAccess,
                          NativePedMutationAccess,
                          NativePedObservationAccess,
                          NativePedRotationAccess,
                          NativePedOxygenAccess,
                          NativePedDuckingAccess
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
        Check(refused, "native refusal must be an explicit SDK operation error instead of false/zero/cached state");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedStateDE> && std::is_base_of_v<CPedRotationDE, CPedStateDE>);
        Owner                    owner;
        NativePedReferenceToken  token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        fixture::PedStateFixture ped(owner, owner, owner, owner, owner, owner, owner, token, PLAYER_PED);
        CPed*                    sdk = &ped;
        const CPed*              constant = &ped;
        static_assert(noexcept(constant->GetCustomMoveAnim()) && std::is_same_v<decltype(constant->GetCustomMoveAnim()), int>);
        Check(constant->GetCustomMoveAnim() == 0 && !owner.reads && !owner.writes && !owner.duckReads,
              "classic custom-move bookkeeping starts at zero without native token I/O or animation effects");
        for (const auto type : std::array<std::uint32_t, 5>{PLAYER_PED, CIVILIAN_PED, 0x10000, 0x80000000, 0xffffffff})
        {
            owner.result = ClockResult::Expired;
            const auto reads = owner.reads, writes = owner.writes, duckReads = owner.duckReads;
            ped.SetType(type);
            Check(constant->GetType() == type && owner.reads == reads && owner.writes == writes && owner.duckReads == duckReads,
                  "actual full DWORD wrapper type is CPU bookkeeping independent of native category, token and callback status");
        }

        for (const int value : {(std::numeric_limits<int>::min)(), -1, 0, 55, 138, (std::numeric_limits<int>::max)()})
        {
            owner.result = ClockResult::Expired;
            const auto reads = owner.reads, writes = owner.writes, duckReads = owner.duckReads;
            const auto wrapperType = constant->GetType();
            ped.RecordCustomMoveAnimForFixture(value);
            Check(constant->GetCustomMoveAnim() == value && constant->GetType() == wrapperType && owner.reads == reads && owner.writes == writes &&
                      owner.duckReads == duckReads,
                  "protected signed bookkeeping preserves all int values and CPU identity while native access refuses");
        }
        fixture::PedStateFixture independent(owner, owner, owner, owner, owner, owner, owner, token, PLAYER_PED);
        Check(independent.GetCustomMoveAnim() == 0 && constant->GetCustomMoveAnim() == (std::numeric_limits<int>::max)(),
              "custom movement bookkeeping belongs to each wrapper rather than a shared current-player cache");
        owner.result = ClockResult::Ok;
        for (const auto bits : std::array<std::uint32_t, 9>{0, 0x80000000, 0x3f800001, 0xc1200000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0x7fc12345, 0xffc07654})
        {
            owner.bits = bits;
            const auto reads = owner.reads, writes = owner.writes;
            Check(Bits(constant->GetOxygenLevel()) == bits && owner.reads == reads + 1 && SameToken(owner.token, token),
                  "const actual oxygen getter preserves selected registered token and raw breath float bits");
            sdk->SetOxygenLevel(Float(bits));
            Check(owner.writes == writes + 1 && owner.bits == bits && SameToken(owner.token, token),
                  "actual oxygen setter dispatches raw intent once without clamp, stamina/current-player substitution or replay");
        }
        for (const bool value : {false, true})
        {
            owner.duck = value;
            const auto before = owner.duckReads;
            Check(constant->IsDucking() == value && owner.duckReads == before + 1 && SameToken(owner.token, token),
                  "actual const IsDucking returns the genuine selected token's true or false query once");
        }
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.result = static_cast<ClockResult>(status);
            const auto reads = owner.reads, writes = owner.writes, duckReads = owner.duckReads;
            Refuses([&] { (void)constant->GetOxygenLevel(); }, "GetOxygenLevel");
            Refuses([&] { sdk->SetOxygenLevel(Float(0x80000000)); }, "SetOxygenLevel");
            Refuses([&] { (void)constant->IsDucking(); }, "IsDucking");
            Check(owner.reads == reads + 1 && owner.writes == writes + 1 && owner.duckReads == duckReads + 1 && owner.bits == 0x80000000,
                  "all refused/uncertain effects stay explicit without wrapper retry, rollback or fabricated query output");
        }
        Absent                   absent;
        fixture::PedStateFixture unbound(absent, absent, absent, absent, absent, absent, absent, token, CIVILIAN_PED);
        Refuses([&] { (void)unbound.GetOxygenLevel(); }, "GetOxygenLevel");
        Refuses([&] { unbound.SetOxygenLevel(2); }, "SetOxygenLevel");
        Refuses([&] { (void)unbound.IsDucking(); }, "IsDucking");
        Check(unbound.GetType() == CIVILIAN_PED, "absent native services do not change explicit CPU wrapper identity");
        Check(unbound.GetCustomMoveAnim() == 0, "absent native services preserve the actual CPU custom-move default");
        unbound.RecordCustomMoveAnimForFixture((std::numeric_limits<int>::min)());
        Check(unbound.GetCustomMoveAnim() == (std::numeric_limits<int>::min)() && unbound.GetType() == CIVILIAN_PED,
              "protected CPU bookkeeping requires no native service and leaves wrapper classification unchanged");
        std::puts("Production MTA ped custom-move bookkeeping SDK fixture completed");
        std::puts("Production MTA oxygen and ducking ped SDK fixture completed");
        std::puts("Production MTA ped wrapper type SDK fixture completed");
        std::printf("Passed %u actual CPed oxygen/state SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Oxygen/state SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
