/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual current rotation SDK forwarding preserves registered intent
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CPedRotationDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
// All unrelated methods reject only in this test subclass. Production keeps
// target rotation and the genuine native repair hook unimplemented/pure.
#include "PedRotationFixture.generated.h"
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
    bool SameToken(const NativePedReferenceToken& first, const NativePedReferenceToken& second)
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    struct Owner final : NativePedVitalsAccess, NativePedMetadataAccess, NativePedMutationAccess, NativePedObservationAccess, NativePedRotationAccess
    {
        unsigned                reads = 0, writes = 0;
        std::uint32_t           bits = 0;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken token;
        ClockResult             ReadPedCurrentRotation(const NativePedReferenceToken& selected, float& value) noexcept override
        {
            ++reads;
            token = selected;
            // Deliberately fill even on refusal: no unsuccessful SDK getter
            // may publish this local value as an acknowledged observation.
            std::memcpy(&value, &bits, 4);
            return result;
        }
        ClockResult SetPedCurrentRotation(const NativePedReferenceToken& selected, float value) noexcept override
        {
            ++writes;
            token = selected;
            std::memcpy(&bits, &value, 4);
            return result;
        }
    };
    struct Absent final : NativePedVitalsAccess, NativePedMetadataAccess, NativePedMutationAccess, NativePedObservationAccess, NativePedRotationAccess
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
        Check(refused, "each native refusal must remain an explicit operation-specific SDK error");
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPedRotationDE> && std::is_base_of_v<CPedPhysicsDE, CPedRotationDE>);
        Owner                       owner;
        NativePedReferenceToken     token{0xfedcba9876543210ULL, 0x123456789abcdef0ULL, 17};
        fixture::PedRotationFixture ped(owner, owner, owner, owner, owner, token);
        CPed*                       sdk = &ped;
        const CPed*                 constSdk = &ped;
        // Include values outside gameplay's usual domain: the actual classic
        // SDK stores raw bits, it does not normalize degrees/radians here.
        for (const auto bits : std::array<std::uint32_t, 9>{0, 0x80000000, 0x3f800001, 0xc1200000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0x7fc12345, 0xffc07654})
        {
            const auto reads = owner.reads, writes = owner.writes;
            owner.bits = bits;
            Check(Bits(constSdk->GetCurrentRotation()) == bits && owner.reads == reads + 1 && SameToken(owner.token, token),
                  "const actual CPed current getter preserves raw float bits and exact immutable CPU token");
            sdk->SetCurrentRotation(Float(bits));
            Check(owner.writes == writes + 1 && owner.bits == bits && SameToken(owner.token, token),
                  "actual CPed setter forwards exactly one raw current-field intent without conversion or replay");
        }
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.result = static_cast<ClockResult>(status);
            const auto reads = owner.reads, writes = owner.writes;
            Refuses([&] { (void)constSdk->GetCurrentRotation(); }, "GetCurrentRotation");
            Refuses([&] { sdk->SetCurrentRotation(Float(0x80000000)); }, "SetCurrentRotation");
            Check(owner.reads == reads + 1 && owner.writes == writes + 1 && owner.bits == 0x80000000,
                  "refusal never retries, rolls back accepted native bits or invokes unrelated target/placement methods");
        }
        Absent                      absent;
        fixture::PedRotationFixture unbound(absent, absent, absent, absent, absent, token);
        Refuses([&] { (void)unbound.GetCurrentRotation(); }, "GetCurrentRotation");
        Refuses([&] { unbound.SetCurrentRotation(1); }, "SetCurrentRotation");
        std::puts("Production MTA current ped rotation SDK fixture completed");
        std::printf("Passed %u actual CPed current rotation SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Current rotation SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
