/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CWorld current-area DWORD dispatch without native writes
 ****************************************************************************/
#include "../../Client/game_de/CWorldAreaDE.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace world_area_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified current-area fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldAreaFixture.generated.h"

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
    struct GroundOwner final : NativeWorldGroundAccess
    {
        unsigned    calls = 0;
        ClockResult FindWorldGroundZ(const CVector&, float&) noexcept override
        {
            ++calls;
            return ClockResult::NotInitialized;
        }
    };
    struct LineOwner final : NativeWorldLineAccess
    {
        unsigned    calls = 0;
        ClockResult IsWorldLineOfSightClear(const CVector&, const CVector&, const SLineOfSightFlags&, bool&) noexcept override
        {
            ++calls;
            return ClockResult::NotInitialized;
        }
    };
    struct AreaOwner final : NativeWorldAreaAccess
    {
        unsigned      calls = 0;
        std::uint32_t value = 0;
        ClockResult   result = ClockResult::Ok;
        void (*afterRead)(AreaOwner&) noexcept = nullptr;
        ClockResult ReadWorldCurrentArea(std::uint32_t& output) noexcept override
        {
            ++calls;
            // A bookend can refuse after native data was staged. Publishing
            // that partial value as zero or a previous area would be unsafe.
            output = value;
            if (afterRead)
                afterRead(*this);
            return result;
        }
    };
    struct UnavailableOwner final : NativeWorldAreaAccess
    {
    };
}

static_assert(sizeof(DWORD) == sizeof(std::uint32_t));
static_assert(std::is_abstract_v<mta::game_de::CWorldAreaDE>);
static_assert(std::is_base_of_v<mta::game_de::CWorldLineDE, mta::game_de::CWorldAreaDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CWorldAreaDE::GetCurrentArea), DWORD (mta::game_de::CWorldAreaDE::*)()>);

int main()
{
    try
    {
        GroundOwner                          ground;
        LineOwner                            line;
        AreaOwner                            first, second;
        NativeWorldAreaAccess*               selected = &first;
        world_area_fixture::WorldAreaFixture world(ground, line, *selected);
        CWorld&                              sdk = world;
        Check(!first.calls && !second.calls && !ground.calls && !line.calls,
              "constructing the borrowed current-area slice performs no native or inherited query calls");

        selected = &second;
        second.value = 0x12345678u;
        for (const std::uint32_t expected : {0u, 1u, 255u, 256u, 65535u, 65536u, 0x7fffffffu, 0x80000000u, 0xffffffffu, 0xaaaaaaaau, 0x55555555u, 0x01020304u})
        {
            first.value = expected;
            const auto before = first.calls;
            Check(static_cast<std::uint32_t>(sdk.GetCurrentArea()) == expected && first.calls == before + 1,
                  "actual CWorld dispatch returns the accepted full DWORD exactly once without byte narrowing or signed normalization");
            Check(!second.calls && !ground.calls && !line.calls,
                  "a wrapper retains its original borrowed area owner and never consults global selection, Ground or Line");
        }
        // Isolate every bit and its complement. These checks catch width,
        // signedness and accidental masks without inventing valid area IDs.
        for (unsigned bit = 0; bit != 32; ++bit)
        {
            for (const std::uint32_t expected : {std::uint32_t{1} << bit, ~(std::uint32_t{1} << bit)})
            {
                first.value = expected;
                const auto before = first.calls;
                Check(static_cast<std::uint32_t>(sdk.GetCurrentArea()) == expected && first.calls == before + 1,
                      "every raw current-area bit and complement survives the common DWORD return contract");
            }
        }

        for (const auto result :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = result;
            first.value = 0;
            DWORD      output = 0xfedcba98u;
            const auto before = first.calls;
            bool       refused = false;
            try
            {
                output = sdk.GetCurrentArea();
            }
            catch (const std::runtime_error& error)
            {
                const std::string text(error.what());
                refused = text.find("GetCurrentArea") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(result))) != std::string::npos;
            }
            Check(refused && output == 0xfedcba98u && first.calls == before + 1 && !second.calls && !ground.calls && !line.calls,
                  "all explicit owner refusals preserve caller output and name the failure without a successful zero, retry or fallback");
        }

        first.result = ClockResult::Ok;
        first.value = 0x11223344u;
        first.afterRead = [](AreaOwner& owner) noexcept
        {
            owner.value = 0x88776655u;
            owner.result = ClockResult::BindingChanged;
        };
        DWORD      output = 0xdecafbad;
        const auto beforeCallback = first.calls;
        bool       callbackRefused = false;
        try
        {
            output = sdk.GetCurrentArea();
        }
        catch (const std::runtime_error& error)
        {
            callbackRefused = std::string(error.what()).find("GetCurrentArea") != std::string::npos;
        }
        Check(callbackRefused && output == 0xdecafbadu && first.calls == beforeCallback + 1,
              "a callback owner refusing after staging cannot publish its partial area or trigger an SDK retry");
        first.afterRead = nullptr;
        first.result = ClockResult::Ok;
        const auto beforeFresh = first.calls;
        Check(static_cast<std::uint32_t>(sdk.GetCurrentArea()) == 0x88776655u && first.calls == beforeFresh + 1,
              "the next successful call observes fresh owner state instead of caching an earlier accepted or refused area");
        first.value = 0;
        Check(sdk.GetCurrentArea() == 0, "a genuine accepted exterior zero remains distinguishable from owner refusal");

        world_area_fixture::WorldAreaFixture secondWorld(ground, line, *selected);
        const auto                           beforeSecond = first.calls;
        Check(static_cast<std::uint32_t>(static_cast<CWorld&>(secondWorld).GetCurrentArea()) == second.value && second.calls == 1 &&
                  first.calls == beforeSecond && !ground.calls && !line.calls,
              "a separate world uses its own borrowed area provider with no process-global owner or inherited-query fallback");
        UnavailableOwner                     unavailable;
        world_area_fixture::WorldAreaFixture missing(ground, line, unavailable);
        bool                                 missingRefused = false;
        try
        {
            static_cast<CWorld&>(missing).GetCurrentArea();
        }
        catch (const std::runtime_error& error)
        {
            missingRefused = std::string(error.what()).find("GetCurrentArea") != std::string::npos;
        }
        Check(missingRefused && !ground.calls && !line.calls,
              "default unavailable area access refuses without publishing a fabricated exterior or complete world service");
        std::puts("Actual world area SDK fixture completed");
        std::printf("Passed %u actual world area SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "World area SDK check failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
