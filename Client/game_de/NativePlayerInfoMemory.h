/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked native player identity and exact-width persistent mutations
 *****************************************************************************/
#pragma once
#include "NativePlayerInfoAccess.h"
#include <atomic>
#include <cstddef>

namespace mta::game_de
{
    class NativePlayerInfoMemory final
    {
    public:
        using CheckOwner = ClockResult (*)() noexcept;
        struct Identity
        {
            std::uintptr_t image = 0, ped = 0;
        };
        ClockResult Bind() noexcept;
        void        Reset() noexcept;
        bool        IsBound() const noexcept;
        // A snapshot records current identity, not a retained native pointer
        // lease. Dependent services must recheck before and after their work.
        ClockResult ReadIdentity(Identity& output, CheckOwner owner) const noexcept;
        ClockResult ReadMoney(std::int32_t& output, CheckOwner owner) const noexcept;
        ClockResult SetMoney(std::int32_t value, bool instant, CheckOwner owner) const noexcept;
        ClockResult SetLastTimeEaten(std::int16_t value, CheckOwner owner) const noexcept;
        ClockResult SetDoesNotGetTired(bool value, CheckOwner owner) const noexcept;
        ClockResult ReadFloat(NativePlayerInfoFloatField field, float& output, CheckOwner owner) const noexcept;
        ClockResult ReadDword(NativePlayerInfoDwordField field, std::uint32_t& output, CheckOwner owner) const noexcept;
        ClockResult ReadByte(NativePlayerInfoByteField field, std::uint8_t& output, CheckOwner owner) const noexcept;
        ClockResult SetCamDrunkLevel(std::uint8_t level, CheckOwner owner) const noexcept;

    private:
        ClockResult                 Current(std::uintptr_t& image) const noexcept;
        ClockResult                 Write(std::size_t offset, std::size_t width, std::uint64_t value, bool instant, CheckOwner owner) const noexcept;
        ClockResult                 ReadScalar(std::size_t offset, std::size_t width, std::uint64_t& output, CheckOwner owner) const noexcept;
        std::atomic<std::uintptr_t> imageAddress{0};
        std::atomic<std::uint32_t>  ownerThread{0};
    };
}
