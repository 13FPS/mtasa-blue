// Run the ordinary cache-budget pulse with a recording streaming budget provider.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>

using uint = unsigned int;
using std::size_t;
#define NUMELMS(array) (sizeof(array) / sizeof((array)[0]))
#include "Interpolation.generated.inl"

struct Limits
{
    int bytes{}, queries{};
    int GetStreamingMemory()
    {
        ++queries;
        return bytes;
    }
};

struct Multiplayer
{
    Limits* limits{};
    Limits* GetLimits() { return limits; }
};

struct CCore
{
    Multiplayer*  multiplayer{};
    int           queries{};
    static CCore& GetSingleton()
    {
        static CCore core;
        return core;
    }
    Multiplayer* GetMultiplayer()
    {
        ++queries;
        return multiplayer;
    }
};

struct CTickCount
{
    static int Now() { return 1; }
};

struct CModelCacheManagerImpl
{
    int  m_TickCountNow{}, preloads{};
    uint m_uiMaxCachedPedModels{}, m_uiMaxCachedVehicleModels{};
    bool m_IsUsingCustomPedCacheLimit{}, m_IsUsingCustomVehicleCacheLimit{};
    void PreLoad() { ++preloads; }
    void DoPulse();
    void SetCustomLimits(std::optional<size_t> vehicles, std::optional<size_t> peds);
};

// Only the historical address-based reader is replaced in a baseline build.
// Supply its integer from the same test budget without touching process memory.
[[maybe_unused]] static int legacyBudgetBytes;
#include "CacheBudget.generated.inl"

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::string_view name = argv[1];
    CModelCacheManagerImpl cache;
    Limits                 limits;
    Multiplayer            multiplayer{&limits};
    auto&                  core = CCore::GetSingleton();
    core.multiplayer = &multiplayer;
    auto pulse = [&](int bytes, uint peds, uint vehicles)
    {
        legacyBudgetBytes = limits.bytes = bytes;
        cache.DoPulse();
        if (cache.m_uiMaxCachedPedModels == peds && cache.m_uiMaxCachedVehicleModels == vehicles)
            return true;
        std::cerr << bytes << " bytes gave " << cache.m_uiMaxCachedPedModels << "/" << cache.m_uiMaxCachedVehicleModels << " ped/vehicle limits, expected "
                  << peds << "/" << vehicles << "\n";
        return false;
    };
    constexpr int mib = 1024 * 1024;
    bool          ok = false;
    if (name == "samples")
        ok = pulse(64 * mib, 9, 7) && pulse(96 * mib, 18, 28) && pulse(128 * mib, 36, 56) && pulse(256 * mib, 72, 56) && pulse(80 * mib, 13, 17) &&
             pulse(112 * mib, 27, 42) && limits.queries == 6;
    else if (name == "endpoints")
        ok = pulse(-1, 9, 7) && pulse(0, 9, 7) && pulse(1, 9, 7) && pulse(std::numeric_limits<int>::max(), 72, 56) && limits.queries == 4;
    else if (name == "availability")
    {
        core.multiplayer = nullptr;
        ok = pulse(128 * mib, 9, 7);
        core.multiplayer = &multiplayer;
        multiplayer.limits = nullptr;
        ok = pulse(128 * mib, 9, 7) && ok;
        multiplayer.limits = &limits;
        ok = pulse(128 * mib, 36, 56) && ok;
        core.multiplayer = nullptr;
        ok = pulse(128 * mib, 9, 7) && limits.queries == 1 && ok;
    }
    else if (name == "overrides")
    {
        cache.SetCustomLimits(0, 19);
        ok = pulse(80 * mib, 19, 0) && core.queries == 0 && cache.preloads == 1;
        cache.SetCustomLimits(17, {});
        ok = pulse(128 * mib, 36, 17) && ok;
        cache.SetCustomLimits({}, 5);
        ok = pulse(96 * mib, 5, 28) && ok;
        cache.SetCustomLimits({}, {});
        ok = pulse(64 * mib, 9, 7) && limits.queries == 3 && ok;
    }
    else if (name == "large-overrides")
    {
        const size_t largest = std::numeric_limits<uint>::max();
        const size_t above = sizeof(size_t) > sizeof(uint) ? largest + 1 : largest;
        cache.SetCustomLimits(above, above);
        ok = pulse(0, static_cast<uint>(largest), static_cast<uint>(largest));
    }
    return ok ? 0 : 1;
}
