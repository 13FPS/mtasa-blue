/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded owner-thread startup of the existing GUI on an owned workload
 *****************************************************************************/
#pragma once
#include <cstdint>

struct ID3D12Device;

namespace DefinitiveGuiStartup
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
        DeviceAttached,
        Resources,
        Session,
        Submitted,
        Readback,
        Stopping,
        Complete
    };

    // This private Core result has no native address or frame permission. The
    // workload uses its own queue and resources, independently of the renderer
    // that supplied the single transferred device reference.
    struct Result
    {
        std::uint32_t version = 1, stage = 0, failureStage = 0, hresult = 0, guiStatus = 0;
        std::uint32_t deviceLuidLow = 0, deviceLuidHigh = 0;
        std::uint32_t nodeCount = 0, queueCreated = 0, targetCreated = 0;
        std::uint32_t width = 640, height = 480, targetFormat = 87;
        std::uint32_t modulePrepared = 0, deviceAttached = 0, sessionCreated = 0, widgetsCreated = 0;
        // Only the helper's clear/copy ExecuteCommandLists calls are counted
        // here. The existing GUI owns its separate frame submission internally.
        std::uint32_t ownQueueSubmissions = 0, guiFrames = 0, signalAttempts = 0;
        std::uint64_t fenceValue = 0, completedFenceValue = 0;
        std::uint32_t readbackComplete = 0, changedPixels = 0, backgroundVerified = 0, textureVerified = 0, opaqueAlphaVerified = 0;
        std::uint32_t workloadSucceeded = 0, shutdownRequested = 0, cleanupComplete = 0, deviceRemoved = 0;
        std::uint32_t moduleRetained = 0, sessionRetained = 0, deviceRetained = 0;
        std::uint32_t deviceReleaseFault = 0;
    };

    class Session
    {
    public:
        Session() noexcept = default;
        ~Session() noexcept;
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        // Preparation loads/configures the actual cgui DLL before observation
        // hooks can hold peers. Both paths are absolute and null terminated.
        Status Prepare(const wchar_t* absoluteGuiDll, const wchar_t* absoluteAssetRoot) noexcept;
        // Ok consumes exactly one caller-owned reference using pointer storage
        // only. Failure leaves it owned by the caller; there is no AddRef here.
        Status AttachDevice(ID3D12Device* transferredReference) noexcept;
        // The caller must finish observer End/Release, including all retained
        // suspensions and callback quiescence, before these three operations.
        Status Start() noexcept;
        Status Poll() noexcept;
        Status Shutdown() noexcept;
        // Owner-thread scalar copy only, including while End is still pending.
        Result Snapshot() const noexcept;

    private:
        struct Impl;
        Impl* impl = nullptr;
    };
}
