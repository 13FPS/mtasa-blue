/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Copy one runtime generation without waiting on a native-held peer
 *****************************************************************************/
#pragma once
#include "FrontendRuntimeReport.h"
#include <atomic>

namespace deonline
{
    // Both frontend and local-input publications use the same bounded,
    // nonblocking copy. A script suspended by native admission must never
    // hold a lock needed by the Core producer's next publication.
    template <typename Snapshot>
    class FrontendValueCache
    {
    public:
        void Reset() noexcept
        {
            available.store(false);
            sequence.store(0);
        }
        void Publish(const Snapshot& value) noexcept
        {
            sequence.fetch_add(1);
            for (std::size_t field = 0; field < value.size(); ++field)
                words[field].store(value[field]);
            available.store(true);
            sequence.fetch_add(1);
        }
        bool Capture(Snapshot& value) const noexcept
        {
            if (!available.load())
                return false;
            for (unsigned attempt = 0; attempt < 2; ++attempt)
            {
                const auto before = sequence.load();
                if (before & 1)
                    continue;
                Snapshot next{};
                for (std::size_t field = 0; field < next.size(); ++field)
                    next[field] = words[field].load();
                if (sequence.load() == before)
                {
                    value = next;
                    return true;
                }
            }
            return false;
        }

    private:
        std::array<std::atomic<std::uint32_t>, std::tuple_size<Snapshot>::value> words{};
        std::atomic<std::uint64_t>                                               sequence{0};
        std::atomic<bool>                                                        available{false};
    };
    using FrontendRuntimeCache = FrontendValueCache<FrontendRuntimeReport::Snapshot>;
}
