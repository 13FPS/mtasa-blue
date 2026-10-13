/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <windows.h>

namespace deonline
{
    // The pinned bridge keeps this storage alive through worker completion.
    // Loading a DLL may exceed CLEO's watchdog or hold the loader lock, so the
    // script can only request work and poll value-only completion here.
    class CoreStartupReference
    {
    public:
        bool          Begin(const void* moduleAddress) noexcept;
        int           Poll() noexcept;
        std::uint32_t Read(std::uint32_t field) const noexcept;
        bool          Cancel() noexcept;
        bool          IsPending() const noexcept;

    private:
        static unsigned __stdcall ThreadMain(void* context) noexcept;
        void                      Run() noexcept;
        bool                      CheckCancellation() noexcept;

        // Only Begin/Poll's owning thread touches the handle or input address.
        // State2 publishes the worker's immutable values; State3 also proves
        // the thread has exited, allowing another attempt without a live job.
        std::atomic<DWORD>            ownerThread{0};
        std::atomic<unsigned>         state{0};
        std::atomic<bool>             cancellation{false};
        HANDLE                        thread = nullptr;
        const void*                   address = nullptr;
        std::array<std::uint32_t, 19> values{};
    };
}
