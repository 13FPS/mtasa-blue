/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Stable SDK wrappers for all independently qualified classic garage IDs
 ****************************************************************************/
#pragma once
#include "CGaragesDE.h"
#include "CGarageNamedDE.h"
#include "NativeNumericGarageAccess.h"
#include "NativeNumericGarageProfile.h"
#include <game/CGarageInitializationPolicy.h>
#include <array>
#include <cstdint>
#include <type_traits>

namespace mta::game_de
{
    namespace NumericGarageSdkDetail
    {
        // Reuse the existing eight SDK property implementations. This adapter
        // supplies a fixed numeric owner route, never the native named lookup.
        class BoundAccess final : public NativeNamedGarageAccess
        {
        public:
            BoundAccess(NativeNumericGarageAccess&, const NativeNumericGarageDescriptor&) noexcept;
            ClockResult ReadNamedGarageGeometry(const NativeGarageName&, NativeGarageGeometry&) noexcept override;
            ClockResult ReadNamedGarageType(const NativeGarageName&, std::uint8_t&) noexcept override;
            ClockResult SetNamedGarageType(const NativeGarageName&, std::uint8_t) noexcept override;
            ClockResult ReadNamedGarageOpen(const NativeGarageName&, bool&) noexcept override;
            ClockResult SetNamedGarageOpen(const NativeGarageName&, bool) noexcept override;

        private:
            bool                       Accepts(const NativeGarageName&) const noexcept;
            NativeNumericGarageAccess& native;
            const std::uint32_t        sdkId;
            const NativeGarageName     name;
        };

        class AdapterStorage
        {
        protected:
            AdapterStorage(NativeNumericGarageAccess&, const NativeNumericGarageDescriptor&) noexcept;
            BoundAccess adapter;
        };

        // Base order is part of the lifetime contract: the adapter exists
        // before the property base borrows it, and outlives that base on exit.
        class Garage final : private AdapterStorage, public CGarageNamedDE
        {
        public:
            Garage(NativeNumericGarageAccess&, const NativeNumericGarageDescriptor&) noexcept;
            Garage(const Garage&) = delete;
            Garage& operator=(const Garage&) = delete;
            Garage(Garage&&) = delete;
            Garage& operator=(Garage&&) = delete;
        };
    }

    // The complete SDK garage range shares the existing current native owner.
    // Own concrete derived objects because the legacy SDK bases have no
    // virtual destructor. All returned SDK pointers are borrowed and stable.
    class CGaragesStartupDE final : public CGaragesDE
    {
    public:
        explicit CGaragesStartupDE(NativeNumericGarageAccess&) noexcept;
        CGarage*    GetGarage(DWORD) override;
        ClockResult RequireReady() noexcept;
        CGaragesStartupDE(const CGaragesStartupDE&) = delete;
        CGaragesStartupDE& operator=(const CGaragesStartupDE&) = delete;
        CGaragesStartupDE(CGaragesStartupDE&&) = delete;
        CGaragesStartupDE& operator=(CGaragesStartupDE&&) = delete;

    private:
        NativeNumericGarageAccess&                                                                 native;
        std::array<NumericGarageSdkDetail::Garage, NativeNumericGarageProfile::Descriptors.size()> garages;
    };
    static_assert(!std::is_abstract_v<CGaragesStartupDE> && std::is_base_of_v<CGarages, CGaragesStartupDE>);
}
