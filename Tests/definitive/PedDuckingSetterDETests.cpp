/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CPed raw ducking setter forwards one immutable owner intent
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedStateDE.h"
#include <game/CPedIK.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
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
    void     Check(bool ok, const char* reason)
    {
        ++checks;
        if (!ok)
            throw std::runtime_error(reason);
    }
    struct Owner final : NativePedVitalsAccess,
                         NativePedMetadataAccess,
                         NativePedMutationAccess,
                         NativePedObservationAccess,
                         NativePedRotationAccess,
                         NativePedOxygenAccess,
                         NativePedDuckingAccess
    {
        unsigned                writes = 0, reads = 0;
        bool                    value = false;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken received;
        ClockResult             SetPedDucking(const NativePedReferenceToken& token, bool ducking) noexcept override
        {
            ++writes;
            received = token;
            value = ducking;
            return result;
        }
        ClockResult ReadPedDucking(const NativePedReferenceToken& token, bool& output) noexcept override
        {
            ++reads;
            received = token;
            output = value;
            return result;
        }
    };
    struct Unavailable final : NativePedVitalsAccess,
                               NativePedMetadataAccess,
                               NativePedMutationAccess,
                               NativePedObservationAccess,
                               NativePedRotationAccess,
                               NativePedOxygenAccess,
                               NativePedDuckingAccess
    {
    };
}
int main()
{
    try
    {
        Owner                    owner;
        NativePedReferenceToken  token{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto               expected = token;
        fixture::PedStateFixture ped(owner, owner, owner, owner, owner, owner, owner, token, PLAYER_PED);
        CPed&                    sdk = ped;
        token = {};
        Check(!owner.writes && !owner.reads, "construction neither writes ducking nor consults native state");
        for (bool value : {false, true, true, false})
        {
            const auto writes = owner.writes;
            sdk.SetDucking(value);
            Check(owner.writes == writes + 1 && owner.value == value && owner.received.version == expected.version &&
                      owner.received.ownerGeneration == expected.ownerGeneration && owner.received.slot == expected.slot,
                  "actual CPed virtual setter forwards bool and copied immutable token exactly once");
            Check(static_cast<const CPed&>(ped).IsDucking() == value && owner.writes == writes + 1,
                  "existing actual IsDucking query observes owner state without setter replay");
        }
        for (unsigned raw = 1; raw <= static_cast<unsigned>(ClockResult::OriginalException); ++raw)
        {
            owner.result = static_cast<ClockResult>(raw);
            const auto writes = owner.writes;
            bool       refused = false;
            try
            {
                sdk.SetDucking(true);
            }
            catch (const std::runtime_error& error)
            {
                refused = std::string(error.what()).find("SetDucking") != std::string::npos;
            }
            Check(refused && owner.writes == writes + 1 && owner.value,
                  "owner refusal is an explicit operation error with no retry, task effect or rollback of possibly accepted intent");
        }
        Unavailable              missing;
        fixture::PedStateFixture absent(missing, missing, missing, missing, missing, missing, missing, expected, PLAYER_PED);
        bool                     refused = false;
        try
        {
            static_cast<CPed&>(absent).SetDucking(false);
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, "read-only or older native access defaults to refusing the new setter");
        std::puts("Actual ped ducking setter SDK fixture completed");
        std::printf("Passed %u actual CPed ducking-setter SDK checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", checks, error.what());
        return 1;
    }
}
