/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the current proved DE acceleration operand without a fake setter
 *****************************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>

namespace mta::game_de
{
    class NativeGravityReadAccess final
    {
    public:
        // The established timing binding owns exact-image identity and thread
        // admission. This borrowed callback must check the current image pages
        // and complete OS read; it cannot grant native call/write authority.
        using CheckedRead = bool (*)(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool executable) noexcept;
        NativeGravityReadAccess(std::uintptr_t imageAddress, CheckedRead readMemory) noexcept : image(imageAddress), read(readMemory) {}
        bool Read(float& output) const noexcept;

    private:
        bool                 Qualify() const noexcept;
        const std::uintptr_t image;
        const CheckedRead    read;
    };
}
