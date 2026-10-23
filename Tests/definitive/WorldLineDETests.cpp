/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CWorld line-clear boolean dispatch with owned query intent
 ****************************************************************************/
#include "../../Client/game_de/CWorldLineDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace world_line_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified line-clear fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "WorldLineFixture.generated.h"

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
    unsigned FlagBits(const SLineOfSightFlags& flags) noexcept
    {
        return (flags.bCheckBuildings ? 1u : 0u) | (flags.bCheckVehicles ? 2u : 0u) | (flags.bCheckPeds ? 4u : 0u) | (flags.bCheckObjects ? 8u : 0u) |
               (flags.bCheckDummies ? 16u : 0u) | (flags.bSeeThroughStuff ? 32u : 0u) | (flags.bIgnoreSomeObjectsForCamera ? 64u : 0u) |
               (flags.bShootThroughStuff ? 128u : 0u) | (flags.bCheckCarTires ? 256u : 0u);
    }
    SLineOfSightFlags MakeFlags(unsigned bits) noexcept
    {
        SLineOfSightFlags flags;
        flags.bCheckBuildings = (bits & 1u) != 0;
        flags.bCheckVehicles = (bits & 2u) != 0;
        flags.bCheckPeds = (bits & 4u) != 0;
        flags.bCheckObjects = (bits & 8u) != 0;
        flags.bCheckDummies = (bits & 16u) != 0;
        flags.bSeeThroughStuff = (bits & 32u) != 0;
        flags.bIgnoreSomeObjectsForCamera = (bits & 64u) != 0;
        flags.bShootThroughStuff = (bits & 128u) != 0;
        flags.bCheckCarTires = (bits & 256u) != 0;
        return flags;
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
        unsigned                     calls = 0;
        ClockResult                  result = ClockResult::Ok;
        bool                         clear = false;
        CVector*                     callerStart = nullptr;
        CVector*                     callerEnd = nullptr;
        SLineOfSightFlags*           callerFlags = nullptr;
        bool                         mutateCaller = false;
        bool                         inputAliasesCaller = false;
        bool                         outputAliasesFlags = false;
        bool                         snapshotsSurvived = false;
        std::array<std::uint32_t, 3> startBits{}, endBits{};
        unsigned                     flagsBits = 0;
        ClockResult IsWorldLineOfSightClear(const CVector& start, const CVector& end, const SLineOfSightFlags& flags, bool& output) noexcept override
        {
            ++calls;
            startBits = VectorBits(start);
            endBits = VectorBits(end);
            flagsBits = FlagBits(flags);
            inputAliasesCaller = &start == callerStart || &start == callerEnd || &end == callerStart || &end == callerEnd || &flags == callerFlags;
            const auto outputAddress = reinterpret_cast<std::uintptr_t>(&output);
            const auto flagsAddress = reinterpret_cast<std::uintptr_t>(callerFlags);
            outputAliasesFlags = callerFlags && outputAddress >= flagsAddress && outputAddress < flagsAddress + sizeof(*callerFlags);
            // Both vectors and the by-value SDK flags must be snapshots before
            // the owner can change caller storage for a future query.
            if (mutateCaller)
            {
                if (callerStart)
                    *callerStart = CVector(-501.25f, 602.5f, -703.75f);
                if (callerEnd)
                    *callerEnd = CVector(804.125f, -905.25f, 1006.5f);
                if (callerFlags)
                    *callerFlags = MakeFlags(flagsBits ^ 511u);
            }
            snapshotsSurvived = VectorBits(start) == startBits && VectorBits(end) == endBits && FlagBits(flags) == flagsBits;
            // Partial output on refusal is intentional: the real SDK must
            // throw rather than turn it into a clear/blocked answer or retry.
            output = clear;
            return result;
        }
    };
    struct UnavailableOwner final : NativeWorldLineAccess
    {
    };
}

static_assert(std::is_abstract_v<mta::game_de::CWorldLineDE>);
static_assert(std::is_base_of_v<mta::game_de::CWorldGroundDE, mta::game_de::CWorldLineDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CWorldLineDE::IsLineOfSightClear),
                             bool (mta::game_de::CWorldLineDE::*)(const CVector*, const CVector*, SLineOfSightFlags)>);

int main()
{
    try
    {
        GroundOwner                          ground;
        LineOwner                            line;
        world_line_fixture::WorldLineFixture world(ground, line);
        CWorld&                              sdk = world;
        Check(!ground.calls && !line.calls, "constructing the borrowed world slice performs no native or inherited ground calls");
        CVector           start(16.25f, -32.5f, 64.125f), end(-128.5f, 256.25f, -512.75f);
        SLineOfSightFlags flags;
        line.callerStart = &start;
        line.callerEnd = &end;
        line.callerFlags = &flags;
        const auto expectedStart = VectorBits(start), expectedEnd = VectorBits(end);

        line.clear = true;
        Check(sdk.IsLineOfSightClear(&start, &end) && line.flagsBits == 31u && line.calls == 1,
              "actual common SDK defaults reach the owner without a duplicate backend default policy");
        // The SDK preserves the common input object, including both unused
        // fields. The separate native call fixture proves that only seven
        // classic flags become native arguments; no SDK-side inversion or
        // interpretation may turn either ignored field into a result policy.
        for (unsigned bits = 0; bits != 512; ++bits)
        {
            flags = MakeFlags(bits);
            for (const bool clear : {false, true})
            {
                line.clear = clear;
                const auto before = line.calls;
                Check(sdk.IsLineOfSightClear(&start, &end, flags) == clear && line.calls == before + 1,
                      "every common flag combination preserves the native boolean exactly once without inversion");
                Check(line.startBits == expectedStart && line.endBits == expectedEnd && line.flagsBits == bits,
                      "copied start/end and all common flag bits retain their exact direction and input meaning");
                Check(!line.inputAliasesCaller && !line.outputAliasesFlags && line.snapshotsSurvived && VectorBits(start) == expectedStart &&
                          VectorBits(end) == expectedEnd && FlagBits(flags) == bits && !ground.calls,
                      "line queries do not alias or alter caller intent, consult ground height or invoke unrelated world operations");
            }
        }

        flags = MakeFlags(0x155u);
        line.mutateCaller = true;
        line.clear = false;
        const auto beforeMutation = line.calls;
        Check(!sdk.IsLineOfSightClear(&start, &end, flags) && line.calls == beforeMutation + 1,
              "callbacks changing caller vectors and flags do not retry or fabricate a clear answer");
        Check(line.startBits == expectedStart && line.endBits == expectedEnd && line.flagsBits == 0x155u && line.snapshotsSurvived &&
                  !line.inputAliasesCaller && VectorBits(start) != expectedStart && VectorBits(end) != expectedEnd && FlagBits(flags) == (0x155u ^ 511u),
              "both endpoint copies and the flags snapshot survive callback changes to original storage");
        line.mutateCaller = false;
        const auto changedStart = VectorBits(start), changedEnd = VectorBits(end);
        const auto changedFlags = FlagBits(flags);
        sdk.IsLineOfSightClear(&start, &end, flags);
        Check(line.startBits == changedStart && line.endBits == changedEnd && line.flagsBits == changedFlags,
              "a subsequent query takes fresh caller intent rather than retaining earlier endpoints or flags");

        for (const auto nullCase : {0u, 1u, 2u})
        {
            const auto before = line.calls;
            bool       refused = false;
            try
            {
                sdk.IsLineOfSightClear(nullCase != 1u ? nullptr : &start, nullCase != 0u ? nullptr : &end, flags);
            }
            catch (const std::invalid_argument& error)
            {
                refused = std::string(error.what()).find("IsLineOfSightClear") != std::string::npos;
            }
            Check(refused && line.calls == before && !ground.calls, "either or both null endpoints are named and refused before the owner");
        }
        for (const auto result :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            line.result = result;
            line.clear = false;
            bool       output = true, refused = false;
            const auto before = line.calls;
            try
            {
                output = sdk.IsLineOfSightClear(&start, &end, flags);
            }
            catch (const std::runtime_error& error)
            {
                const std::string text(error.what());
                refused = text.find("IsLineOfSightClear") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(result))) != std::string::npos;
            }
            Check(refused && output && line.calls == before + 1 && !ground.calls,
                  "all explicit owner refusals leave caller output untouched and name the failure without retry or fallback");
            Check(line.startBits == changedStart && line.endBits == changedEnd && line.flagsBits == changedFlags && !line.inputAliasesCaller,
                  "refused native partial output cannot change the owned query intent");
        }
        line.result = ClockResult::Ok;
        line.clear = false;
        Check(!sdk.IsLineOfSightClear(&start, &end, flags), "a new blocked result remains false after earlier clear results and refusals");

        LineOwner second;
        second.clear = true;
        world_line_fixture::WorldLineFixture secondWorld(ground, second);
        const auto                           beforeSecond = line.calls;
        Check(static_cast<CWorld&>(secondWorld).IsLineOfSightClear(&start, &end, flags) && second.calls == 1 && line.calls == beforeSecond && !ground.calls,
              "another world uses its own borrowed line owner without a process-global or inherited ground fallback");
        UnavailableOwner                     unavailable;
        world_line_fixture::WorldLineFixture missing(ground, unavailable);
        bool                                 missingRefused = false;
        try
        {
            static_cast<CWorld&>(missing).IsLineOfSightClear(&start, &end, flags);
        }
        catch (const std::runtime_error& error)
        {
            missingRefused = std::string(error.what()).find("IsLineOfSightClear") != std::string::npos;
        }
        Check(missingRefused && !ground.calls, "default unavailable access refuses instead of fabricating a clear or blocked world query");
        std::puts("Actual world line SDK fixture completed");
        std::printf("Passed %u actual world line SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "World line SDK check failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
