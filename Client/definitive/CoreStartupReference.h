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
        bool          BeginRenderReferenceV2(const void* moduleAddress, std::uint32_t route) noexcept;
        bool          BeginRenderReferenceV3(const void* moduleAddress, std::uint32_t route) noexcept;
        bool          CaptureRenderV3() noexcept;
        std::uint32_t ReadRenderV3(std::uint32_t field) const noexcept;
        bool          BeginRenderReferenceV4(const void* moduleAddress, std::uint32_t route) noexcept;
        bool          CaptureRenderV4() noexcept;
        std::uint32_t ReadRenderV4(std::uint32_t field) const noexcept;
        bool          BeginRenderReferenceV5(const void* moduleAddress, std::uint32_t route) noexcept;
        bool          CaptureRenderV5() noexcept;
        std::uint32_t ReadRenderV5(std::uint32_t field) const noexcept;
        int           Poll() noexcept;
        std::uint32_t Read(std::uint32_t field) const noexcept;
        bool          CaptureInput() noexcept;
        std::uint32_t ReadInput(std::uint32_t field) const noexcept;
        bool          CaptureMovement() noexcept;
        std::uint32_t ReadMovement(std::uint32_t field) const noexcept;
        bool          CaptureRender() noexcept;
        std::uint32_t ReadRender(std::uint32_t field) const noexcept;
        bool          CaptureRenderV2() noexcept;
        std::uint32_t ReadRenderV2(std::uint32_t field) const noexcept;
        bool          Cancel() noexcept;
        bool          IsPending() const noexcept;

    private:
        static unsigned __stdcall ThreadMain(void* context) noexcept;
        void                      Run() noexcept;
        bool                      CheckCancellation() noexcept;
        bool                      Start(const void* moduleAddress, bool configuredInput, bool configuredMovement = false, bool configuredRender = false,
                                        std::uint32_t configuredRenderRoute = 0, std::uint32_t configuredRenderApi = 1) noexcept;
        static bool               InputCancelled();
        static void               PublishInput(const std::uint32_t* values, std::uint32_t count);

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
        std::uint32_t                         renderRoute = 0;
        std::uint32_t                         renderApi = 1;
        bool                                  renderV3Available = false;
        DefinitiveRenderReference::SnapshotV3 publishedRenderV3{}, capturedRenderV3{};
        bool                                  renderV4Available = false;
        DefinitiveRenderReference::SnapshotV4 publishedRenderV4{}, capturedRenderV4{};
        bool                                  renderV5Available = false;
        DefinitiveRenderReference::SnapshotV5 publishedRenderV5{}, capturedRenderV5{};
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
        bool                                  renderV2Available = false;
        DefinitiveRenderReference::SnapshotV2 publishedRenderV2{};
        DefinitiveRenderReference::SnapshotV2 capturedRenderV2{};
    };
}
