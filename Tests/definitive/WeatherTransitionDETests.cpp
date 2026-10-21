/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the real MTA weather SDK transition methods and refusals
 *****************************************************************************/
#include "../../Client/game_de/CWeatherTransitionDE.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated weather SDK method reached transition fixture\n", stderr);
        std::abort();
    }
}

// Only the remaining abstract SDK operations are supplied by the generated
// fixture. The four methods under test are the actual compiled implementation.
#include "WeatherTransitionFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }

    template <typename Operation>
    void Refuses(Operation operation, const char* description)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, description);
            return;
        }
        catch (...)
        {
        }
        Check(false, description);
    }

    class DefaultAccess final : public NativeWeatherTransitionAccess
    {
    };

    class Owner final : public NativeWeatherTransitionAccess
    {
    public:
        ClockResult ReadForced(std::uint16_t& output) noexcept override
        {
            ++reads;
            if (result == ClockResult::Ok)
                output = forced;
            return result;
        }
        ClockResult SetTypes(std::uint8_t first, std::uint8_t second) noexcept override
        {
            ++sets;
            primary = first;
            secondary = second;
            return result;
        }
        ClockResult ResyncWithClock() noexcept override
        {
            ++resyncs;
            return result;
        }
        ClockResult ReleaseForced() noexcept override
        {
            ++releases;
            return result;
        }
        ClockResult   result = ClockResult::Ok;
        std::uint16_t forced = 0;
        std::uint8_t  primary = 0, secondary = 0;
        unsigned      reads = 0, sets = 0, resyncs = 0, releases = 0;
    };

    class Weather final : public fixture::WeatherFixtureBase
    {
    public:
        explicit Weather(NativeWeatherTransitionAccess& owner) noexcept : WeatherFixtureBase(owner) {}
    };
}

int main()
{
    static_assert(std::is_abstract_v<CWeatherTransitionDE> && !std::is_abstract_v<Weather>);
    DefaultAccess unavailable;
    Weather       unopened(unavailable);
    CWeather&     missing = unopened;
    Refuses([&] { missing.Get(); }, "a timing-only owner cannot publish a guessed weather value");
    Refuses([&] { missing.Set(1, 2); }, "a timing-only owner cannot acknowledge a weather write");
    Refuses([&] { missing.ResyncInterpolationWithGameClock(1, 2); }, "resync requires its admitted native owner");
    Refuses([&] { missing.Release(); }, "release cannot silently succeed without native access");

    Owner     first, second;
    Weather   one(first), two(second);
    CWeather& weather = one;
    CWeather& other = two;
    for (std::uint16_t type = 0; type < 23; ++type)
    {
        first.forced = type;
        Check(weather.Get() == type, "actual SDK Get preserves each native weather identifier");
    }
    first.forced = 0xffff;
    Check(weather.Get() == 0xff, "native signed WORD release preserves the SDK byte sentinel");
    second.forced = 7;
    Check(other.Get() == 7 && second.reads == 1, "SDK instances borrow their own weather owner");
    weather.Set(0, 22);
    Check(first.sets == 1 && first.primary == 0 && first.secondary == 22 && second.sets == 0, "SDK Set forwards both endpoints to the same native owner");
    weather.Set(22, 0);
    Check(first.sets == 2 && first.primary == 22 && first.secondary == 0, "SDK forwards reversed weather types without changing their meaning");
    const auto oldSets = first.sets;
    weather.ResyncInterpolationWithGameClock(1, 2);
    weather.ResyncInterpolationWithGameClock(22, 0);
    Check(first.resyncs == 2 && first.sets == oldSets && first.primary == 22 && first.secondary == 0,
          "resync delegates the clock bookend operation and never changes requested weather types");
    weather.Release();
    Check(first.releases == 1 && second.releases == 0 && first.sets == oldSets,
          "SDK release is one owner operation, not a copied byte write or broader native reset");

    for (unsigned status = static_cast<unsigned>(ClockResult::InvalidArgument); status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
    {
        first.result = static_cast<ClockResult>(status);
        const auto reads = first.reads, sets = first.sets, resyncs = first.resyncs, releases = first.releases;
        Refuses([&] { weather.Get(); }, "every refused native read remains an explicit SDK failure");
        Refuses([&] { weather.Set(3, 4); }, "every refused native type transaction remains an explicit SDK failure");
        Refuses([&] { weather.ResyncInterpolationWithGameClock(3, 4); }, "every refused native clock resync remains an explicit SDK failure");
        Refuses([&] { weather.Release(); }, "every refused native release remains an explicit SDK failure");
        Check(first.reads == reads + 1 && first.sets == sets + 1 && first.resyncs == resyncs + 1 && first.releases == releases + 1,
              "SDK never retries over an uncertain or competing native transaction");
    }
    std::printf("Passed %u actual weather transition SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
