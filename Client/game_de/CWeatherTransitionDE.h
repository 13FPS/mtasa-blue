/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the actual MTA weather transition interface over its owner
 *****************************************************************************/
#pragma once
#include <game/CWeather.h>
#include <game/CGameClockService.h>
#include <type_traits>

namespace mta::game_de
{
    class NativeWeatherTransitionAccess
    {
    public:
        // Timing-only owners explicitly refuse weather operations until their
        // native callback has admitted this additional field group.
        virtual ClockResult ReadForced(std::uint16_t&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult SetTypes(std::uint8_t, std::uint8_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ResyncWithClock() noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReleaseForced() noexcept { return ClockResult::NotInitialized; }

    protected:
        ~NativeWeatherTransitionAccess() = default;
    };
    // Scalar weather overrides still need their native writer ownership. They
    // remain pure, so this partial SDK object cannot be published as a complete
    // CWeather provider or silently substitute cached rain/fog observations.
    class CWeatherTransitionDE : public CWeather
    {
    public:
        unsigned char Get() override;
        void          Set(unsigned char primary, unsigned char secondary) override;
        void          ResyncInterpolationWithGameClock(unsigned char primary, unsigned char secondary) override;
        void          Release() override;

    protected:
        explicit CWeatherTransitionDE(NativeWeatherTransitionAccess& owner) noexcept : access(owner) {}

    private:
        NativeWeatherTransitionAccess& access;
    };
    static_assert(std::is_abstract_v<CWeatherTransitionDE> && std::is_base_of_v<CWeather, CWeatherTransitionDE>);
}
