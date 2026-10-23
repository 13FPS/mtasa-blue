/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CWorld jetpack-height getter over one borrowed native owner
 ****************************************************************************/
#include "../../Client/game_de/CWorldJetpackHeightDE.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace world_jetpack_height_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified jetpack-height fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldJetpackHeightFixture.generated.h"

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
        float output;
        std::memcpy(&output, &bits, sizeof(output));
        return output;
    }
    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    struct OtherOwners final : NativeWorldGroundAccess, NativeWorldLineAccess, NativeWorldAreaAccess
    {
        unsigned    calls = 0;
        ClockResult FindWorldGroundZ(const CVector&, float&) noexcept override
        {
            ++calls;
            return ClockResult::NotInitialized;
        }
        ClockResult IsWorldLineOfSightClear(const CVector&, const CVector&, const SLineOfSightFlags&, bool&) noexcept override
        {
            ++calls;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadWorldCurrentArea(std::uint32_t&) noexcept override
        {
            ++calls;
            return ClockResult::NotInitialized;
        }
    };
    struct HeightOwner final : NativeWorldJetpackHeightAccess
    {
        unsigned      calls = 0;
        std::uint32_t valueBits = 0;
        ClockResult   result = ClockResult::Ok;
        void (*afterRead)(HeightOwner&) noexcept = nullptr;
        ClockResult ReadJetpackMaxHeight(float& output) noexcept override
        {
            ++calls;
            // Deliberately stage data before a possible bookend refusal. The
            // SDK must never publish that partial scalar or replace it by100.
            output = FromBits(valueBits);
            if (afterRead)
                afterRead(*this);
            return result;
        }
    };
    struct UnavailableOwner final : NativeWorldJetpackHeightAccess
    {
    };
}

static_assert(std::is_abstract_v<mta::game_de::CWorldJetpackHeightDE>);
static_assert(std::is_base_of_v<mta::game_de::CWorldAreaDE, mta::game_de::CWorldJetpackHeightDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CWorldJetpackHeightDE::GetJetpackMaxHeight), float (mta::game_de::CWorldJetpackHeightDE::*)()>);

int main()
{
    try
    {
        OtherOwners                                             other;
        HeightOwner                                             first, second;
        NativeWorldJetpackHeightAccess*                         selected = &first;
        world_jetpack_height_fixture::WorldJetpackHeightFixture world(other, other, other, *selected);
        CWorld&                                                 sdk = world;
        Check(!first.calls && !second.calls && !other.calls, "construction borrows owners without executing any query or applying a jetpack limit");
        selected = &second;
        for (const std::uint32_t bits : {0u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x42c80000u, 0x43ba2000u, 1u, 0x007fffffu, 0x00800000u, 0x7f7fffffu,
                                         0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc23456u, 0x7f812345u, 0xff823456u})
        {
            first.valueBits = bits;
            const auto before = first.calls;
            Check(Bits(sdk.GetJetpackMaxHeight()) == bits && first.calls == before + 1,
                  "actual CWorld getter returns accepted raw float bits once without clamping, normalizing or substituting a known constant");
            Check(!second.calls && !other.calls, "getter retains its borrowed owner and does not use global selection or unrelated inherited world queries");
        }
        for (const auto result :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = result;
            first.valueBits = 0x42c80000u;
            float      output = FromBits(0xc2f6e979u);
            const auto before = first.calls;
            bool       refused = false;
            try
            {
                output = sdk.GetJetpackMaxHeight();
            }
            catch (const std::runtime_error& error)
            {
                const std::string text(error.what());
                refused =
                    text.find("GetJetpackMaxHeight") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(result))) != std::string::npos;
            }
            Check(refused && Bits(output) == 0xc2f6e979u && first.calls == before + 1 && !second.calls && !other.calls,
                  "every explicit owner refusal keeps caller output unchanged and names its result without retry or fabricated default height");
        }
        first.result = ClockResult::Ok;
        first.valueBits = 0x41280000u;
        first.afterRead = [](HeightOwner& owner) noexcept
        {
            owner.valueBits = 0xc3148000u;
            owner.result = ClockResult::BindingChanged;
        };
        const auto beforeCallback = first.calls;
        float      output = FromBits(0xc0000000u);
        bool       refused = false;
        try
        {
            output = sdk.GetJetpackMaxHeight();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find("GetJetpackMaxHeight") != std::string::npos;
        }
        Check(refused && Bits(output) == 0xc0000000u && first.calls == beforeCallback + 1,
              "an owner refusing after staging cannot leak partial data or trigger another read");
        first.afterRead = nullptr;
        first.result = ClockResult::Ok;
        const auto beforeFresh = first.calls;
        Check(Bits(sdk.GetJetpackMaxHeight()) == first.valueBits && first.calls == beforeFresh + 1,
              "a later successful getter reads fresh owner state instead of caching an accepted or refused value");
        first.valueBits = 0;
        Check(Bits(sdk.GetJetpackMaxHeight()) == 0, "an accepted zero remains a real value rather than a refusal sentinel or default-height request");
        second.valueBits = 0x44164000u;
        world_jetpack_height_fixture::WorldJetpackHeightFixture secondWorld(other, other, other, *selected);
        const auto                                              beforeSecond = first.calls;
        Check(Bits(static_cast<CWorld&>(secondWorld).GetJetpackMaxHeight()) == second.valueBits && second.calls == 1 && first.calls == beforeSecond &&
                  !other.calls,
              "another world uses its own borrowed height owner without a process-global scalar cache");
        UnavailableOwner                                        unavailable;
        world_jetpack_height_fixture::WorldJetpackHeightFixture missing(other, other, other, unavailable);
        output = FromBits(0x447a0000u);
        bool missingRefused = false;
        try
        {
            output = static_cast<CWorld&>(missing).GetJetpackMaxHeight();
        }
        catch (const std::runtime_error& error)
        {
            missingRefused = std::string(error.what()).find("GetJetpackMaxHeight") != std::string::npos;
        }
        Check(missingRefused && Bits(output) == 0x447a0000u && !other.calls,
              "default unavailable access refuses without returning100, zero or a fabricated functioning jetpack service");
        std::puts("Actual world jetpack height SDK fixture completed");
        std::printf("Passed %u actual world jetpack height SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "World jetpack height SDK check failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
