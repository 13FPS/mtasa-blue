/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own a transparent MTA GUI composition surface inside a borrowed HWND
 *****************************************************************************/
#pragma once
#include <cstdint>

struct ID3D12Device;
struct HWND__;

namespace DefinitiveGuiSurface
{
    enum class Status : std::uint32_t
    {
        Ok,
        Busy,
        InvalidArgument,
        WrongThread,
        Failed
    };
    enum class Stage : std::uint32_t
    {
        None,
        Prepared,
        Attached,
        Resources,
        Composed,
        Session,
        Running,
        Resizing,
        Stopping,
        Complete
    };

    // This surface owns its queue, swapchain, child HWND and all GPU resources.
    // Neither its pixels nor its fence grants permission to touch a native frame.
    struct Result
    {
        std::uint32_t version = 1, stage = 0, failureStage = 0, hresult = 0, guiStatus = 0;
        std::uint32_t deviceLuidLow = 0, deviceLuidHigh = 0, nodeCount = 0;
        std::uint32_t modulePrepared = 0, deviceAttached = 0, parentValidated = 0, parentWindowed = 0, parentAlive = 0;
        std::uint32_t childCreated = 0, childVisible = 0, compositionCreated = 0, swapchainCreated = 0, queueCreated = 0;
        std::uint32_t width = 0, height = 0, targetFormat = 87, alphaMode = 1;
        std::uint32_t sessionCreated = 0, widgetsCreated = 0, guiFrames = 0, presentCalls = 0, presentHresult = 0, ownQueueSubmissions = 0;
        std::uint32_t signalAttempts = 0, readbackComplete = 0, readbackRedVerified = 0, transparentPixelVerified = 0, premultipliedPixelsVerified = 0;
        std::uint32_t resizeCount = 0, resizePending = 0, parentDestroyed = 0, stopRequested = 0, cleanupComplete = 0, deviceRemoved = 0, releaseFault = 0;
        std::uint32_t moduleRetained = 0, sessionRetained = 0, deviceRetained = 0, childRetained = 0, queueRetained = 0, surfaceRetained = 0;
        std::uint32_t readbackPixels = 0, presentSucceeded = 0, childShown = 0, ownerThreadId = 0, parentThreadId = 0;
        std::uint32_t windowCleanupComplete = 0, compositionCleanupComplete = 0, classUnregistered = 0;
        std::uint32_t presentBusy = 0;
        std::uint64_t fenceValue = 0, completedFenceValue = 0;
    };

    // Input validation has a separate contract so the successful passive
    // surface trial keeps its original layout and remains non-interactive.
    // This is GUI ownership only, not permission to suppress native game input.
    struct InteractionResult
    {
        std::uint32_t version = 1, enabled = 0, focused = 0, focusAcquired = 0, focusLost = 0;
        std::uint64_t events = 0, syntheticReleases = 0;
        std::uint32_t pressedKeys = 0, pressedButtons = 0;
        std::uint32_t buttonClicks = 0, textChanges = 0, textAccepted = 0, textBytes = 0;
        std::uint32_t closeRequested = 0, inputFailed = 0, inputDetached = 0;
    };

    class Session
    {
    public:
        Session() noexcept = default;
        ~Session() noexcept;
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        Status Prepare(const wchar_t* absoluteGuiDll, const wchar_t* absoluteAssetRoot) noexcept;
        // Explicit opt-in before Start. Until native gameplay input capture is
        // available, only the disposable owned-window host enables this mode.
        Status EnableInteraction() noexcept;
        // Ok consumes one existing device reference by storage only. The HWND
        // remains borrowed; Start validates it after native End has fully drained.
        Status AttachDeviceWindow(ID3D12Device* transferredReference, HWND__* borrowedParent, bool windowed) noexcept;
        Status Start() noexcept;
        // Pumps only this worker's owned child and polls fences without waits.
        // The Core owner supplies the bounded display duration and stop request.
        Status            Poll() noexcept;
        Status            Shutdown() noexcept;
        Result            Snapshot() const noexcept;
        InteractionResult InteractionSnapshot() const noexcept;

    private:
        struct Impl;
        Impl* impl = nullptr;
    };
}
