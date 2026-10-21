/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact native x64 atomic weather patch windows and capability refusal
 *****************************************************************************/
#include "NativeWeatherOverrideAtomic.h"

namespace mta::game_de::NativeWeatherOverrideAtomicProfile
{
    // Qualified from the supported image, with hashes and every37 operand's
    // alignment recorded in proof/qualified-atomic-windows.json. Four complete
    // groups cross aligned16 boundaries; a partial-copy fallback is unsafe for
    // the current engine continuation even while every other thread is parked.
    const std::array<Window, 6> Windows = {
        {{Field::Sandstorm, 0x1146926, 0x1146928, 7, 8, {0x25, 0xf7, 0xd4, 0x1c, 0x04, 0x44, 0x89, 0x25}},
         {Field::Sandstorm, 0x1149974, 0x1149970, 6, 16, {0x64, 0x18, 0x1d, 0x04, 0x89, 0x2d, 0xaa, 0xa4, 0x1c, 0x04, 0x89, 0x2d, 0x5c, 0x25, 0x5e, 0x04}},
         {Field::Sandstorm, 0x1291d12, 0x1291d10, 8, 16, {0x5c, 0xca, 0xf3, 0x0f, 0x11, 0x0d, 0x0a, 0x21, 0x08, 0x04, 0x41, 0xb9, 0x6f, 0x6c, 0x06, 0x00}},
         {Field::Fog, 0x1291db5, 0x1291db8, 8, 8, {0x05, 0x63, 0x20, 0x08, 0x04, 0x66, 0x83, 0xfa}},
         {Field::Fog, 0x1291dc7, 0x1291dc8, 8, 8, {0x0f, 0x11, 0x05, 0x51, 0x20, 0x08, 0x04, 0xb9}},
         {Field::Rainbow, 0x1291e7e, 0x1291e80, 8, 8, {0x11, 0x0d, 0x7e, 0xf1, 0x06, 0x04, 0x66, 0x41}}}};
    bool Supports(Field field, bool compareExchange128) noexcept
    {
        return field == Field::Fog || field == Field::Rainbow || (field == Field::Sandstorm && compareExchange128);
    }
    const Window* Find(std::uintptr_t writerRva) noexcept
    {
        for (const auto& window : Windows)
            if (window.writerRva == writerRva)
                return &window;
        return nullptr;
    }
    bool Build(const Window& window, const Entry& entry, std::array<unsigned char, 16>& output) noexcept
    {
        const auto& writer = entry.writer;
        if (window.field != writer.field || window.writerRva != writer.rva || window.writerSize != writer.size ||
            (window.windowSize != 8 && window.windowSize != 16) || window.windowRva % window.windowSize || writer.rva + writer.displacement < window.windowRva)
            return false;
        const auto offset = writer.rva + writer.displacement - window.windowRva;
        if (offset + sizeof(std::int32_t) > window.windowSize)
            return false;
        for (std::size_t index = 0; index < writer.size; ++index)
        {
            const auto rva = writer.rva + index;
            if (rva >= window.windowRva && rva < window.windowRva + window.windowSize && window.stock[rva - window.windowRva] != writer.original[index])
                return false;
            if ((index < writer.displacement || index >= writer.displacement + 4) && entry.redirected[index] != writer.original[index])
                return false;
        }
        auto changed = window.stock;
        std::memcpy(changed.data() + offset, entry.redirected.data() + writer.displacement, 4);
        output = changed;
        return true;
    }
}
