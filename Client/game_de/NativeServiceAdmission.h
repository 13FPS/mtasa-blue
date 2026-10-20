/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Process-wide admission for native services with suspension ownership
 *****************************************************************************/
#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace mta::game_de::NativeServiceAdmission
{
    enum class Category : std::uint32_t
    {
        None = 0,
        Input = 1,
        Render = 2,
        Clock = 4,
        PadUpdate = 8
    };

    struct Token
    {
        Category      category = Category::None;
        std::uint64_t serial = 0;
        explicit      operator bool() const noexcept { return serial != 0 && category != Category::None; }
    };

    namespace Detail
    {
        inline std::atomic<std::uint32_t> mask{0};
        inline std::atomic<std::uint64_t> nextSerial{0};
        inline std::atomic<std::uint64_t> owners[4] = {0, 0, 0, 0};

        inline unsigned Index(Category category) noexcept
        {
            return category == Category::Input       ? 0
                   : category == Category::Render    ? 1
                   : category == Category::Clock     ? 2
                   : category == Category::PadUpdate ? 3
                                                     : 4;
        }
    }

    // Input and rendering intentionally coexist for MTA's frontend. Clock
    // transactions can park those producers, so their whole setup/cleanup
    // lifetime must exclude clock admission even across separate adapters.
    inline bool TryAcquire(Category category, Token& output) noexcept
    {
        const auto index = Detail::Index(category);
        if (index == 4)
            return false;
        auto serial = Detail::nextSerial.load();
        bool numbered = false;
        for (unsigned attempt = 0; attempt != 16; ++attempt)
        {
            if (serial == (std::numeric_limits<std::uint64_t>::max)())
                return false;
            if (Detail::nextSerial.compare_exchange_strong(serial, serial + 1))
            {
                ++serial;
                numbered = true;
                break;
            }
        }
        if (!numbered)
            return false;
        const auto bit = static_cast<std::uint32_t>(category);
        // Global pad production and local scoped input/rendering coexist.
        // Clock's raw transactions must exclude every suspension owner.
        const auto conflicts = category == Category::Clock ? 15u : bit | static_cast<std::uint32_t>(Category::Clock);
        auto       observed = Detail::mask.load();
        for (unsigned attempt = 0; attempt != 16; ++attempt)
        {
            if (observed & conflicts)
                return false;
            if (Detail::mask.compare_exchange_strong(observed, observed | bit))
            {
                Detail::owners[index].store(serial);
                output = {category, serial};
                return true;
            }
        }
        return false;
    }

    inline bool Release(Token token) noexcept
    {
        const auto index = Detail::Index(token.category);
        if (!token.serial || index == 4)
            return false;
        auto expected = token.serial;
        if (!Detail::owners[index].compare_exchange_strong(expected, 0))
            return false;
        Detail::mask.fetch_and(~static_cast<std::uint32_t>(token.category));
        return true;
    }

    inline bool IsClaimed(Category category) noexcept
    {
        return Detail::Index(category) != 4 && (Detail::mask.load() & static_cast<std::uint32_t>(category)) != 0;
    }
}
