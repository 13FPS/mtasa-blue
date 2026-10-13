/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <windows.h>
#include "../core/DefinitiveInputReference.h"
#include "../core/DefinitiveMovementReference.h"
#include "../core/DefinitiveRenderReference.h"

namespace deonline
{
    // The pinned bridge keeps this storage alive through worker completion.
    // Loading a DLL may exceed CLEO's watchdog or hold the loader lock, so the
    // script can only request work and poll value-only completion here.
    class CoreStartupReference
    {
    public:
        bool          Begin(const void* moduleAddress) noexcept;
        bool          BeginInputReference(const void* moduleAddress) noexcept;
        bool          BeginMovementReference(const void* moduleAddress) noexcept;
        bool          BeginRenderReference(const void* moduleAddress) noexcept;
        int           Poll() noexcept;
        std::uint32_t Read(std::uint32_t field) const noexcept;
        bool          CaptureInput() noexcept;
        std::uint32_t ReadInput(std::uint32_t field) const noexcept;
        bool          CaptureMovement() noexcept;
        std::uint32_t ReadMovement(std::uint32_t field) const noexcept;
        bool          CaptureRender() noexcept;
        std::uint32_t ReadRender(std::uint32_t field) const noexcept;
        bool          Cancel() noexcept;
        bool          IsPending() const noexcept;

    private:
        static unsigned __stdcall ThreadMain(void* context) noexcept;
        void                      Run() noexcept;
        bool                      CheckCancellation() noexcept;
        bool        Start(const void* moduleAddress, bool configuredInput, bool configuredMovement = false, bool configuredRender = false) noexcept;
        static bool InputCancelled();
        static void PublishInput(const std::uint32_t* values, std::uint32_t count);

        // Only Begin/Poll's owning thread touches the handle or input address.
        // State2 publishes the worker's immutable values; State3 also proves
        // the thread has exited, allowing another attempt without a live job.
        std::atomic<DWORD>                    ownerThread{0};
        std::atomic<unsigned>                 state{0};
        std::atomic<bool>                     cancellation{false};
        HANDLE                                thread = nullptr;
        const void*                           address = nullptr;
        std::array<std::uint32_t, 19>         values{};
        bool                                  inputRequested = false;
        bool                                  movementRequested = false;
        bool                                  renderRequested = false;
        SRWLOCK                               inputLock = SRWLOCK_INIT;
        bool                                  inputAvailable = false;
        DefinitiveInputReference::Snapshot    publishedInput{};
        DefinitiveInputReference::Snapshot    capturedInput{};
        bool                                  movementAvailable = false;
        DefinitiveMovementReference::Snapshot publishedMovement{};
        DefinitiveMovementReference::Snapshot capturedMovement{};
        bool                                  renderAvailable = false;
        DefinitiveRenderReference::Snapshot   publishedRender{};
        DefinitiveRenderReference::Snapshot   capturedRender{};
    };
}
