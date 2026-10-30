/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK GetTime transport, raw chars and exact output refusal
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoDE.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "ModelTimeFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&ModelTimeObserver::GetTime), decltype(&CModelInfoDE::GetTime)>);
static_assert(std::is_abstract_v<CModelInfoDE>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    unsigned char Bits(char value)
    {
        unsigned char bits = 0;
        std::memcpy(&bits, &value, 1);
        return bits;
    }
    struct Access final : NativeModelClassificationAccess
    {
        std::uint32_t              key = 0;
        unsigned                   calls = 0, unrelated = 0;
        NativeModelTimeObservation value{};
        ClockResult                status = ClockResult::Ok;
        ClockResult                ReadModelTime(std::uint32_t index, NativeModelTimeObservation& output) noexcept override
        {
            ++calls;
            key = index;
            output = value;
            return status;
        }
        ClockResult ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelVehiclePredicate(std::uint32_t, NativeModelVehiclePredicate, bool&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelVehicleSubtype(std::uint32_t, std::uint32_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelInterface(std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelRender(std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelAnimFileIndex(std::uint32_t, std::uint32_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult CreateAndPublishPedModelMetadata(std::uint32_t) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
    };
    template <class F>
    void Refuses(F&& operation, ClockResult status)
    {
        bool refused = false;
        try
        {
            operation();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text = error.what(), tail = ": " + std::to_string(static_cast<unsigned>(status));
            refused = text.find("DE model owner refused GetTime") != std::string::npos && text.size() >= tail.size() &&
                      text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
        Check(refused, "GetTime propagates exact owner status rather than poisoned tuple or false");
    }
    void Cases()
    {
        Access                                 access;
        constexpr std::array<std::uint32_t, 4> keys{0, 19999, 20000, 0xffffffffu};
        constexpr std::array<unsigned char, 8> bytes{0, 1, 6, 20, 24, 0x7f, 0x80, 0xff};
        for (const auto key : keys)
        {
            ModelTimeObserver observer(key, access);
            CModelInfo&       sdk = observer;
            for (const auto on : bytes)
                for (const auto off : bytes)
                {
                    access.value = {true, on, off};
                    char       a = 37, b = 71;
                    const auto calls = access.calls;
                    Check(sdk.GetTime(a, b) && Bits(a) == on && Bits(b) == off && access.calls == calls + 1 && access.key == key,
                          "actual SDK forwards fullDWORD once and preserves every rawchar bit without clamp");
                    char alias = 99;
                    Check(sdk.GetTime(alias, alias) && Bits(alias) == off, "source On then Off order leaves Off in aliased ordinary outputs");
                }
            access.value = {false, 0xff, 0x80};
            char       a = 37, b = 71;
            const auto calls = access.calls;
            Check(!sdk.GetTime(a, b) && a == 37 && b == 71 && access.calls == calls + 1 && access.key == key,
                  "admitted null/nonTIME false preserves both outputs despite poisoned absent tuple");
        }
        for (const auto key : keys)
        {
            ModelTimeObserver observer(key, access);
            for (const auto status :
                 {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                  ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                  ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
            {
                access.status = status;
                access.value = {true, 0xff, 0x80};
                char       a = 37, b = 71;
                const auto calls = access.calls;
                Refuses([&] { observer.GetTime(a, b); }, status);
                Check(a == 37 && b == 71 && access.calls == calls + 1 && access.key == key,
                      "all failed statuses preserve outputs and current fullDWORD dispatch");
            }
        }
        NativeModelClassificationAccess missing;
        ModelTimeObserver               observer(3, missing);
        char                            a = 37, b = 71;
        Refuses([&] { observer.GetTime(a, b); }, ClockResult::NotInitialized);
        Check(a == 37 && b == 71, "default-refusing old owner preserves both outputs");
        Check(!access.unrelated, "GetTime uses no archive/type factory/render or vehicle SDK shortcut");
        std::puts("Actual model info time getter SDK fixture completed");
        std::puts("Actual model time raw chars output alias and refusal transport fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u model time getter SDK checks (%u-bit; transport only)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
