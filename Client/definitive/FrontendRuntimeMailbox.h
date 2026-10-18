/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Script-owned commands cannot destroy an owner-thread GUI lifetime
 *****************************************************************************/
#pragma once
#include "FrontendRuntimeReport.h"
#include <atomic>

namespace deonline
{
    class FrontendRuntimeMailbox
    {
    public:
        using Command = FrontendRuntimeReport::Command;
        struct Request
        {
            std::uint32_t sequence = 0;
            Command       command = Command::None;
        };
        std::uint32_t Begin(std::uint32_t thread) noexcept
        {
            if (!thread || (owner && owner != thread) || generation == UINT32_MAX)
                return 0;
            owner = thread;
            word.store(0);
            superseded.store(0);
            return ++generation;
        }
        bool Submit(std::uint32_t thread, std::uint32_t token, std::uint32_t sequence, Command command) noexcept
        {
            if (!thread || thread != owner || token != generation || !token || !sequence || command < Command::Show || command > Command::Stop ||
                (sequence == UINT32_MAX && command != Command::Stop))
                return false;
            auto previous = word.load();
            for (;;)
            {
                const auto old = Decode(previous);
                if (sequence <= old.sequence || old.command == Command::Stop || (command != Command::Stop && previous && !(previous & 1)))
                    return false;
                const auto next = Encode(sequence, command);
                if (word.compare_exchange_weak(previous, next))
                {
                    if (command == Command::Stop && previous && !(previous & 1))
                        superseded.store(previous);
                    return true;
                }
            }
        }
        Request Accepted() const noexcept { return Decode(word.load()); }
        Request Pending() const noexcept
        {
            const auto value = word.load();
            return value & 1 ? Request{} : Decode(value);
        }
        bool Complete(const Request& request) noexcept
        {
            auto expected = Encode(request.sequence, request.command);
            // Stop can replace a pending Show/Hide atomically. A late callback
            // cannot acknowledge visibility after the dominant stop won.
            return request.sequence && word.compare_exchange_strong(expected, expected | 1);
        }
        bool          Stopping() const noexcept { return Accepted().command == Command::Stop; }
        Request       TakeSuperseded() noexcept { return Decode(superseded.exchange(0)); }
        std::uint32_t Generation() const noexcept { return generation; }

    private:
        static std::uint64_t Encode(std::uint32_t sequence, Command command) noexcept
        {
            return (static_cast<std::uint64_t>(sequence) << 4) | (static_cast<std::uint64_t>(command) << 2);
        }
        static Request Decode(std::uint64_t value) noexcept { return {static_cast<std::uint32_t>(value >> 4), static_cast<Command>((value >> 2) & 3)}; }
        std::uint32_t  owner = 0, generation = 0;
        std::atomic<std::uint64_t> word{0};
        std::atomic<std::uint64_t> superseded{0};
    };
}
