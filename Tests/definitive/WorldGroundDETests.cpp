/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CWorld ground-height dispatch over the qualified DE owner
 ****************************************************************************/
#include "../../Client/game_de/CWorldGroundDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace world_ground_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified ground query fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldGroundFixture.generated.h"

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
    float FromBits(std::uint32_t bits) noexcept
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    std::array<std::uint32_t, 3> VectorBits(const CVector& value) noexcept
    {
        return {Bits(value.fX), Bits(value.fY), Bits(value.fZ)};
    }
    struct GroundOwner final : NativeWorldGroundAccess
    {
        unsigned                     calls = 0;
        ClockResult                  result = ClockResult::Ok;
        std::uint32_t                heightBits = 0;
        CVector*                     caller = nullptr;
        bool                         changeCaller = false;
        bool                         receivedCallerAlias = false;
        bool                         outputAliasesCaller = false;
        bool                         ownedPositionSurvived = false;
        std::array<std::uint32_t, 3> received{};
        ClockResult                  FindWorldGroundZ(const CVector& position, float& output) noexcept override
        {
            ++calls;
            received = VectorBits(position);
            receivedCallerAlias = &position == caller;
            const auto outputAddress = reinterpret_cast<std::uintptr_t>(&output);
            const auto callerAddress = reinterpret_cast<std::uintptr_t>(caller);
            outputAliasesCaller = caller && outputAddress >= callerAddress && outputAddress < callerAddress + sizeof(*caller);
            // A callback can change the caller's next intent. The current
            // query must already own XYZ, independently of that live storage.
            if (changeCaller && caller)
                *caller = CVector(-901.5f, 802.25f, -703.125f);
            ownedPositionSurvived = VectorBits(position) == received;
            // Deliberately write even on refusal. The SDK must not publish a
            // native partial output as a successful height or retry the call.
            output = FromBits(heightBits);
            return result;
        }
    };
    struct UnavailableOwner final : NativeWorldGroundAccess
    {
    };
}

static_assert(std::is_abstract_v<mta::game_de::CWorldGroundDE>);
static_assert(std::is_base_of_v<CWorld, mta::game_de::CWorldGroundDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CWorldGroundDE::FindGroundZFor3DPosition), float (mta::game_de::CWorldGroundDE::*)(CVector*)>);

int main()
{
    try
    {
        GroundOwner                              owner;
        world_ground_fixture::WorldGroundFixture world(owner);
        CWorld&                                  sdk = world;
        Check(owner.calls == 0, "constructing the abstract world slice has no native effects");

        CVector position(128.25f, -256.5f, 4096.125f);
        owner.caller = &position;
        const auto original = VectorBits(position);
        // The native function owns its no-hit policy. In particular +0 must
        // stay +0 rather than become the supplied altitude or a cached hit.
        for (const std::uint32_t bits :
             {0x43579a00u, 0xc39a4000u, 0x00000000u, 0x80000000u, 0x00000001u, 0x007fffffu, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u})
        {
            owner.heightBits = bits;
            const auto  before = owner.calls;
            const float actual = sdk.FindGroundZFor3DPosition(&position);
            Check(Bits(actual) == bits && owner.calls == before + 1, "actual CWorld dispatch preserves the successful raw native float exactly once");
            Check(owner.received == original && !owner.receivedCallerAlias && !owner.outputAliasesCaller && owner.ownedPositionSurvived,
                  "ground dispatch owns exact XYZ and separate output without passing caller storage to the owner");
            Check(VectorBits(position) == original, "query results never overwrite the caller's input position");
        }

        owner.changeCaller = true;
        owner.heightBits = 0x41260000u;
        const auto beforeMutation = owner.calls;
        Check(Bits(sdk.FindGroundZFor3DPosition(&position)) == owner.heightBits && owner.calls == beforeMutation + 1,
              "callback mutation does not retry or replace the height query");
        Check(owner.received == original && owner.ownedPositionSurvived && !owner.receivedCallerAlias && VectorBits(position) != original,
              "copied XYZ survives a callback changing the original caller vector");
        owner.changeCaller = false;
        const auto changed = VectorBits(position);
        sdk.FindGroundZFor3DPosition(&position);
        Check(owner.received == changed, "a later query observes new caller intent instead of retaining a prior input copy");

        const auto beforeNull = owner.calls;
        bool       nullRefused = false;
        try
        {
            sdk.FindGroundZFor3DPosition(nullptr);
        }
        catch (const std::invalid_argument& error)
        {
            nullRefused = std::string(error.what()).find("FindGroundZFor3DPosition") != std::string::npos;
        }
        Check(nullRefused && owner.calls == beforeNull, "null position is named and rejected before reaching the native owner");

        for (const auto result :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.result = result;
            owner.heightBits = 0x44650000u;
            float      output = FromBits(0xc2f6e979u);
            const auto before = owner.calls;
            bool       refused = false;
            try
            {
                output = sdk.FindGroundZFor3DPosition(&position);
            }
            catch (const std::runtime_error& error)
            {
                const std::string text(error.what());
                refused =
                    text.find("FindGroundZFor3DPosition") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(result))) != std::string::npos;
            }
            Check(refused && owner.calls == before + 1 && Bits(output) == 0xc2f6e979u,
                  "every explicit refusal names its result and leaves the caller output unchanged without retry or fallback");
            Check(owner.received == changed && !owner.receivedCallerAlias && VectorBits(position) == changed,
                  "refused queries keep copied XYZ and do not alter caller input");
        }

        owner.result = ClockResult::Ok;
        owner.heightBits = 0;
        Check(Bits(sdk.FindGroundZFor3DPosition(&position)) == 0, "a fresh successful native no-hit remains +0 after earlier hits and refusals");
        GroundOwner secondOwner;
        secondOwner.heightBits = 0xc24a0000u;
        secondOwner.caller = &position;
        world_ground_fixture::WorldGroundFixture secondWorld(secondOwner);
        const auto                               firstCalls = owner.calls;
        Check(Bits(static_cast<CWorld&>(secondWorld).FindGroundZFor3DPosition(&position)) == secondOwner.heightBits && secondOwner.calls == 1 &&
                  owner.calls == firstCalls,
              "a world wrapper uses its own borrowed owner without a global or cached query receiver");

        UnavailableOwner                         unavailable;
        world_ground_fixture::WorldGroundFixture missing(unavailable);
        bool                                     unavailableRefused = false;
        try
        {
            static_cast<CWorld&>(missing).FindGroundZFor3DPosition(&position);
        }
        catch (const std::runtime_error& error)
        {
            unavailableRefused = std::string(error.what()).find("FindGroundZFor3DPosition") != std::string::npos;
        }
        Check(unavailableRefused, "an unimplemented native access refuses instead of fabricating a usable world height");
        std::puts("Actual world ground SDK fixture completed");
        std::printf("Passed %u actual world ground SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "World ground SDK check failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
