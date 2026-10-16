/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include "CoreStartupReference.h"
#include "../core/DefinitiveFrontend.h"
#include <algorithm>
#include <filesystem>
#include <process.h>

namespace deonline
{
    namespace
    {
        thread_local CoreStartupReference* inputOwner = nullptr;
        std::uint32_t                      LegacyRendererModules()
        {
            return (GetModuleHandleW(L"d3dx9_43.dll") ? 1u : 0u) | (GetModuleHandleW(L"d3d9.dll") ? 2u : 0u);
        }
    }

    bool CoreStartupReference::Begin(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, false);
    }

    bool CoreStartupReference::BeginInputReference(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, true);
    }

    bool CoreStartupReference::BeginMovementReference(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, true, true);
    }

    bool CoreStartupReference::BeginRenderReference(const void* moduleAddress) noexcept
    {
        return Start(moduleAddress, true, false, true);
    }

    bool CoreStartupReference::BeginRenderReferenceV2(const void* moduleAddress, std::uint32_t route) noexcept
    {
        return (route == 1 || route == 2) && Start(moduleAddress, true, false, true, route, 2);
    }

    bool CoreStartupReference::BeginRenderReferenceV3(const void* moduleAddress, std::uint32_t route) noexcept
    {
        return route == 2 && Start(moduleAddress, true, false, true, route, 3);
    }

    bool CoreStartupReference::BeginRenderReferenceV4(const void* moduleAddress, std::uint32_t route) noexcept
    {
        // The classification is additive: requesting it must not change the
        // old ownership snapshots or silently select another native route.
        return route == 2 && Start(moduleAddress, true, false, true, route, 4);
    }

    bool CoreStartupReference::BeginRenderReferenceV5(const void* moduleAddress, std::uint32_t route) noexcept
    {
        // A scoped Present association needs an independent lease and cache;
        // earlier protocols must never expose or silently request its hooks.
        return route == 2 && Start(moduleAddress, true, false, true, route, 5);
    }

    bool CoreStartupReference::BeginRenderReferenceV6(const void* moduleAddress, std::uint32_t route) noexcept
    {
        // Native dispatch is independently selected and cached; its caller hook
        // must never be enabled or exposed by historical Present protocols.
        return route == 2 && Start(moduleAddress, true, false, true, route, 6);
    }

    bool CoreStartupReference::BeginGuiStartupReference(const void* moduleAddress) noexcept
    {
        // GUI startup owns a device reference and its own workload. It must not
        // silently enable or reuse the historical read-only render protocol.
        return Start(moduleAddress, true, false, false, 0, 1, true);
    }

    bool CoreStartupReference::Start(const void* moduleAddress, bool configuredInput, bool configuredMovement, bool configuredRender,
                                     std::uint32_t configuredRenderRoute, std::uint32_t configuredRenderApi, bool configuredGuiStartup,
                                     bool configuredGuiSurface, bool configuredGuiFrontend) noexcept
    {
        DWORD      expected = 0;
        const auto current = GetCurrentThreadId();
        if ((static_cast<unsigned>(configuredMovement) + static_cast<unsigned>(configuredRender) + static_cast<unsigned>(configuredGuiStartup) +
                 static_cast<unsigned>(configuredGuiSurface) + static_cast<unsigned>(configuredGuiFrontend) >
             1) ||
            (!ownerThread.compare_exchange_strong(expected, current) && expected != current) || !moduleAddress || IsPending() || thread ||
            frontendQuarantined || !frontendControl.PrepareAttempt(current))
            return false;
        address = moduleAddress;
        inputRequested = configuredInput;
        movementRequested = configuredMovement;
        renderRequested = configuredRender;
        guiRequested = configuredGuiStartup;
        guiSurfaceRequested = configuredGuiSurface;
        guiFrontendRequested = configuredGuiFrontend;
        renderRoute = configuredRenderRoute;
        renderApi = configuredRenderApi;
        AcquireSRWLockExclusive(&inputLock);
        guiAvailable = false;
        publishedGui = {};
        guiSurfaceAvailable = false;
        publishedGuiSurface = {};
        guiFrontendAvailable = false;
        publishedGuiFrontend = {};
        inputAvailable = false;
        publishedInput = {};
        movementAvailable = false;
        publishedMovement = {};
        renderAvailable = false;
        publishedRender = {};
        renderV3Available = false;
        publishedRenderV3 = {};
        renderV4Available = false;
        publishedRenderV4 = {};
        renderV5Available = false;
        publishedRenderV5 = {};
        renderV6Available = false;
        publishedRenderV6 = {};
        renderV2Available = false;
        publishedRenderV2 = {};
        ReleaseSRWLockExclusive(&inputLock);
        capturedInput = {};
        capturedMovement = {};
        capturedRender = {};
        capturedRenderV2 = {};
        capturedRenderV3 = {};
        capturedRenderV4 = {};
        capturedRenderV5 = {};
        capturedRenderV6 = {};
        capturedGui = {};
        capturedGuiSurface = {};
        capturedGuiFrontend = {};
        capturedFrontendControl = {};
        cancellation.store(false);
        state.store(1, std::memory_order_release);
        // Use the existing CRT worker convention instead of a joinable global
        // std::thread whose destructor could run under the Windows loader lock.
        const auto handle = _beginthreadex(nullptr, 0, ThreadMain, this, 0, nullptr);
        if (!handle)
        {
            state.store(0, std::memory_order_release);
            return false;
        }
        thread = reinterpret_cast<HANDLE>(handle);
        return true;
    }

    int CoreStartupReference::Poll() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId())
            return -1;
        const auto phase = state.load(std::memory_order_acquire);
        if (phase == 3)
            return 1;
        if (phase == 0)
            return -1;
        if (phase != 2 || WaitForSingleObject(thread, 0) != WAIT_OBJECT_0)
            return 0;
        // No loader API or blocking join is allowed on the script's polling
        // path, even when the worker is inside a slow DLL initializer.
        CloseHandle(thread);
        thread = nullptr;
        state.store(3, std::memory_order_release);
        return 1;
    }

    std::uint32_t CoreStartupReference::Read(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && state.load(std::memory_order_acquire) == 3 && field < values.size() ? values[field] : 0;
    }

    bool CoreStartupReference::CaptureInput() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = inputAvailable;
        if (available)
            capturedInput = publishedInput;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadInput(std::uint32_t field) const noexcept
    {
        // Reads use the one captured owner-thread copy, never independently
        // sampled atomics or a Core export that the worker may be unloading.
        return ownerThread.load() == GetCurrentThreadId() && field < capturedInput.size() ? capturedInput[field] : 0;
    }

    bool CoreStartupReference::CaptureMovement() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = movementAvailable;
        if (available)
            capturedMovement = publishedMovement;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadMovement(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && field < capturedMovement.size() ? capturedMovement[field] : 0;
    }

    bool CoreStartupReference::CaptureRender() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderAvailable;
        if (available)
            capturedRender = publishedRender;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRender(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && !renderRoute && field < capturedRender.size() ? capturedRender[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV2() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 2 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV2Available;
        if (available)
            capturedRenderV2 = publishedRenderV2;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV2(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 2 && renderRoute && field < capturedRenderV2.size() ? capturedRenderV2[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV3() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 3 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV3Available;
        if (available)
            capturedRenderV3 = publishedRenderV3;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV3(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 3 && renderRoute && field < capturedRenderV3.size() ? capturedRenderV3[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV4() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 4 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV4Available;
        if (available)
            capturedRenderV4 = publishedRenderV4;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV4(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 4 && renderRoute && field < capturedRenderV4.size() ? capturedRenderV4[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV5() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 5 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV5Available;
        if (available)
            capturedRenderV5 = publishedRenderV5;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV5(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 5 && renderRoute && field < capturedRenderV5.size() ? capturedRenderV5[field] : 0;
    }

    bool CoreStartupReference::CaptureRenderV6() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || renderApi != 6 || !renderRoute || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = renderV6Available;
        if (available)
            capturedRenderV6 = publishedRenderV6;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadRenderV6(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && renderApi == 6 && renderRoute && field < capturedRenderV6.size() ? capturedRenderV6[field] : 0;
    }

    bool CoreStartupReference::CaptureGuiStartup() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !guiRequested || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = guiAvailable;
        if (available)
            capturedGui = publishedGui;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadGuiStartup(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiRequested && field < capturedGui.size() ? capturedGui[field] : 0;
    }
    bool CoreStartupReference::BeginGuiSurfaceReference(const void* moduleAddress) noexcept
    {
        // A child surface owns a separate queue/window and cache. It neither
        // replaces offscreen startup evidence nor enables native-frame writes.
        return Start(moduleAddress, true, false, false, 0, 1, false, true);
    }
    bool CoreStartupReference::CaptureGuiSurface() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !guiSurfaceRequested || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = guiSurfaceAvailable;
        if (available)
            capturedGuiSurface = publishedGuiSurface;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }
    std::uint32_t CoreStartupReference::ReadGuiSurface(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiSurfaceRequested && field < capturedGuiSurface.size() ? capturedGuiSurface[field] : 0;
    }

    bool CoreStartupReference::BeginGuiFrontendReference(const void* moduleAddress) noexcept
    {
        // This owns the actual MTA menu and a script-confirmed engine control
        // lease. Its cache and protocol remain separate from passive Surface96.
        return Start(moduleAddress, true, false, false, 0, 1, false, false, true);
    }

    bool CoreStartupReference::CaptureGuiFrontend() noexcept
    {
        if (ownerThread.load() != GetCurrentThreadId() || !guiFrontendRequested || !TryAcquireSRWLockShared(&inputLock))
            return false;
        const bool available = guiFrontendAvailable;
        if (available)
            capturedGuiFrontend = publishedGuiFrontend;
        ReleaseSRWLockShared(&inputLock);
        return available;
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontend(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested && field < capturedGuiFrontend.size() ? capturedGuiFrontend[field] : 0;
    }

    bool CoreStartupReference::CaptureGuiFrontendControl() noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Capture(GetCurrentThreadId(), GetTickCount64(), capturedFrontendControl);
    }

    std::uint32_t CoreStartupReference::ReadGuiFrontendControl(std::uint32_t field) const noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested && field < capturedFrontendControl.size() ? capturedFrontendControl[field]
                                                                                                                            : 0;
    }

    bool CoreStartupReference::AcknowledgeGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Acknowledge, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::HeartbeatGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Heartbeat, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::RevokeGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Revoke, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::RestoreGuiFrontendControl(std::uint32_t generation) noexcept
    {
        return ownerThread.load() == GetCurrentThreadId() && guiFrontendRequested &&
               frontendControl.Script(FrontendControlMailbox::ScriptAction::Restore, GetCurrentThreadId(), generation, GetTickCount64());
    }

    bool CoreStartupReference::ControlInput(std::uint32_t action, std::uint32_t* snapshot, std::uint32_t count)
    {
        return inputOwner && inputOwner->guiFrontendRequested &&
               inputOwner->frontendControl.Worker(action, GetCurrentThreadId(), GetTickCount64(), snapshot, count);
    }

    bool CoreStartupReference::InputCancelled()
    {
        return !inputOwner || inputOwner->cancellation.load(std::memory_order_acquire);
    }

    void CoreStartupReference::PublishInput(const std::uint32_t* snapshot, std::uint32_t count)
    {
        if (!inputOwner || !snapshot ||
            count != (inputOwner->guiFrontendRequested  ? DefinitiveGuiFrontendReference::FieldCount
                      : inputOwner->guiSurfaceRequested ? DefinitiveGuiSurfaceReference::FieldCount
                      : inputOwner->guiRequested        ? DefinitiveGuiStartupReference::FieldCount
                      : inputOwner->renderRequested     ? (inputOwner->renderApi == 6   ? DefinitiveRenderReference::FieldCountV6
                                                           : inputOwner->renderApi == 5 ? DefinitiveRenderReference::FieldCountV5
                                                           : inputOwner->renderApi == 4 ? DefinitiveRenderReference::FieldCountV4
                                                           : inputOwner->renderApi == 3 ? DefinitiveRenderReference::FieldCountV3
                                                           : inputOwner->renderRoute    ? DefinitiveRenderReference::FieldCountV2
                                                                                        : DefinitiveRenderReference::FieldCount)
                      : inputOwner->movementRequested   ? DefinitiveMovementReference::FieldCount
                                                        : DefinitiveInputReference::FieldCount))
            return;
        const bool pendingDispatchCleanup = inputOwner->renderRequested && inputOwner->renderApi == 6 && count == DefinitiveRenderReference::FieldCountV6 &&
                                            snapshot[DefinitiveRenderReference::Phase] == 3 && snapshot[DefinitiveRenderReference::CleanupPending] == 1;
        const bool pendingGuiCleanup = inputOwner->guiRequested && count == DefinitiveGuiStartupReference::FieldCount &&
                                       snapshot[DefinitiveGuiStartupReference::Phase] == 3 && snapshot[DefinitiveGuiStartupReference::CleanupPending] == 1;
        const bool pendingSurfaceCleanup = inputOwner->guiSurfaceRequested && count == DefinitiveGuiSurfaceReference::FieldCount &&
                                           snapshot[DefinitiveGuiStartupReference::Phase] == 3 && snapshot[DefinitiveGuiStartupReference::CleanupPending] == 1;
        const bool pendingFrontendCleanup = inputOwner->guiFrontendRequested && count == DefinitiveGuiFrontendReference::FieldCount &&
                                            snapshot[DefinitiveGuiStartupReference::Phase] == 3 && snapshot[DefinitiveGuiStartupReference::CleanupPending] == 1;
        // A failed resume may retain the script peer inside a shared capture.
        // Drop only intermediate API6/GUI cleanup reports instead of blocking the
        // worker that must retry that resume; final publication remains reliable.
        if (pendingDispatchCleanup || pendingGuiCleanup || pendingSurfaceCleanup || pendingFrontendCleanup)
        {
            if (!TryAcquireSRWLockExclusive(&inputOwner->inputLock))
                return;
        }
        else
            AcquireSRWLockExclusive(&inputOwner->inputLock);
        if (inputOwner->guiFrontendRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedGuiFrontend.begin());
            inputOwner->guiFrontendAvailable = true;
        }
        else if (inputOwner->guiSurfaceRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedGuiSurface.begin());
            inputOwner->guiSurfaceAvailable = true;
        }
        else if (inputOwner->guiRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedGui.begin());
            inputOwner->guiAvailable = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 6)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV6.begin());
            inputOwner->renderV6Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 5)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV5.begin());
            inputOwner->renderV5Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 4)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV4.begin());
            inputOwner->renderV4Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderApi == 3)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV3.begin());
            inputOwner->renderV3Available = true;
        }
        else if (inputOwner->renderRequested && inputOwner->renderRoute)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRenderV2.begin());
            inputOwner->renderV2Available = true;
        }
        else if (inputOwner->renderRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedRender.begin());
            inputOwner->renderAvailable = true;
        }
        else if (inputOwner->movementRequested)
        {
            std::copy_n(snapshot, count, inputOwner->publishedMovement.begin());
            inputOwner->movementAvailable = true;
        }
        else
        {
            std::copy_n(snapshot, count, inputOwner->publishedInput.begin());
            inputOwner->inputAvailable = true;
        }
        ReleaseSRWLockExclusive(&inputOwner->inputLock);
    }

    bool CoreStartupReference::Cancel() noexcept
    {
        if (state.load(std::memory_order_acquire) == 0)
            return false;
        cancellation.store(true, std::memory_order_release);
        return true;
    }

    bool CoreStartupReference::IsPending() const noexcept
    {
        const auto phase = state.load(std::memory_order_acquire);
        return phase == 1 || phase == 2;
    }

    unsigned __stdcall CoreStartupReference::ThreadMain(void* context) noexcept
    {
        const auto self = static_cast<CoreStartupReference*>(context);
        self->Run();
        self->state.store(2, std::memory_order_release);
        return 0;
    }

    bool CoreStartupReference::CheckCancellation() noexcept
    {
        if (!cancellation.load(std::memory_order_acquire))
            return false;
        if (!values[16])
            values[16] = 12;
        return true;
    }

    void CoreStartupReference::Run() noexcept
    {
        values = {};
        values[1] = values[8] = values[9] = values[10] = 0xffffffffu;
        values[7] = 1;  // No adapter has been started by this attempt yet.
        values[11] = LegacyRendererModules();
        values[14] = GetModuleHandleW(L"game_de.dll") ? 1u : 0u;
        HMODULE core = nullptr;
        try
        {
            const auto run = [&]()
            {
                if (CheckCancellation())
                    return;
                if (GetModuleHandleW(L"core.dll") || values[14])
                {
                    values[16] = 2;
                    return;
                }
                HMODULE                    self = nullptr;
                std::array<wchar_t, 32768> ownPath{}, gamePath{};
                if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                        reinterpret_cast<LPCWSTR>(address), &self))
                {
                    values[16] = 3;
                    return;
                }
                const auto ownLength = GetModuleFileNameW(self, ownPath.data(), static_cast<DWORD>(ownPath.size()));
                const auto gameLength = GetModuleFileNameW(nullptr, gamePath.data(), static_cast<DWORD>(gamePath.size()));
                if (!ownLength || ownLength >= ownPath.size() || !gameLength || gameLength >= gamePath.size())
                {
                    values[16] = 3;
                    return;
                }
                const auto mtaDirectory = std::filesystem::path(std::wstring(ownPath.data(), ownLength)).parent_path();
                const auto gtaDirectory = std::filesystem::path(std::wstring(gamePath.data(), gameLength)).parent_path();
                if (CheckCancellation())
                    return;
                core = LoadLibraryExW((mtaDirectory / L"core.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (!core)
                {
                    // Preserve the loader error immediately; cleanup APIs may
                    // replace it before the script receives this snapshot.
                    values[18] = GetLastError();
                    values[16] = 4;
                    return;
                }
                if (CheckCancellation())
                    return;
                using Version = std::uint32_t (*)();
                using Read = std::uint32_t (*)(std::uint32_t);
                using SetDirectory = void (*)(const wchar_t*, std::size_t);
                using Initialize = int (*)();
                const auto version = reinterpret_cast<Version>(GetProcAddress(core, "MTA_DEStartupApiVersion"));
                const auto read = reinterpret_cast<Read>(GetProcAddress(core, "MTA_ReadDEStartupState"));
                const auto setMta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetMTADirectory"));
                const auto setGta = reinterpret_cast<SetDirectory>(GetProcAddress(core, "SetGTADirectory"));
                const auto initialize = reinterpret_cast<Initialize>(GetProcAddress(core, "InitializeCore"));
                if (!version || !read || !setMta || !setGta || !initialize)
                {
                    values[16] = 5;
                    return;
                }
                if (version() != 1)
                {
                    values[16] = 6;
                    return;
                }
                values[8] = static_cast<std::uint32_t>(initialize());
                const auto mta = mtaDirectory.wstring(), gta = gtaDirectory.wstring();
                if (CheckCancellation())
                    return;
                setMta(mta.c_str(), mta.size());
                values[9] = static_cast<std::uint32_t>(initialize());
                if (CheckCancellation())
                    return;
                setGta(gta.c_str(), gta.size());
                if (inputRequested)
                {
                    using Configure = std::uint32_t (*)(std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);
                    using ConfigureRenderV2 =
                        std::uint32_t (*)(std::uint32_t, std::uint32_t, DefinitiveInputReference::CancelCallback, DefinitiveInputReference::PublishCallback);
                    using ConfigureFrontend = std::uint32_t (*)(std::uint32_t, std::uint32_t, DefinitiveInputReference::CancelCallback,
                                                                DefinitiveInputReference::PublishCallback, DefinitiveGuiFrontendReference::ControlCallback);
                    const auto inputVersion =
                        reinterpret_cast<Version>(GetProcAddress(core, guiFrontendRequested                ? "MTA_DEGuiFrontendReferenceApiVersion"
                                                                       : guiSurfaceRequested               ? "MTA_DEGuiSurfaceReferenceApiVersion"
                                                                       : guiRequested                      ? "MTA_DEGuiStartupReferenceApiVersion"
                                                                       : renderRequested && renderApi == 6 ? "MTA_DERenderReferenceApiVersionV6"
                                                                       : renderRequested && renderApi == 5 ? "MTA_DERenderReferenceApiVersionV5"
                                                                       : renderRequested && renderApi == 4 ? "MTA_DERenderReferenceApiVersionV4"
                                                                       : renderRequested && renderApi == 3 ? "MTA_DERenderReferenceApiVersionV3"
                                                                       : renderRoute                       ? "MTA_DERenderReferenceApiVersionV2"
                                                                       : renderRequested                   ? "MTA_DERenderReferenceApiVersion"
                                                                       : movementRequested                 ? "MTA_DEMovementReferenceApiVersion"
                                                                                                           : "MTA_DEInputReferenceApiVersion"));
                    const auto configure = reinterpret_cast<Configure>(GetProcAddress(core, guiSurfaceRequested ? "MTA_ConfigureDEGuiSurfaceReference"
                                                                                            : guiRequested      ? "MTA_ConfigureDEGuiStartupReference"
                                                                                            : renderRequested   ? "MTA_ConfigureDERenderReference"
                                                                                            : movementRequested ? "MTA_ConfigureDEMovementReference"
                                                                                                                : "MTA_ConfigureDEInputReference"));
                    const auto configureV2 = reinterpret_cast<ConfigureRenderV2>(GetProcAddress(core, renderApi == 6   ? "MTA_ConfigureDERenderReferenceV6"
                                                                                                      : renderApi == 5 ? "MTA_ConfigureDERenderReferenceV5"
                                                                                                      : renderApi == 4 ? "MTA_ConfigureDERenderReferenceV4"
                                                                                                      : renderApi == 3 ? "MTA_ConfigureDERenderReferenceV3"
                                                                                                                       : "MTA_ConfigureDERenderReferenceV2"));
                    const auto configureFrontend = reinterpret_cast<ConfigureFrontend>(GetProcAddress(core, "MTA_ConfigureDEGuiFrontendReference"));
                    const auto expectedVersion = guiFrontendRequested                ? DefinitiveGuiFrontendReference::ApiVersion
                                                 : guiSurfaceRequested               ? DefinitiveGuiSurfaceReference::ApiVersion
                                                 : guiRequested                      ? DefinitiveGuiStartupReference::ApiVersion
                                                 : renderRequested && renderApi == 6 ? DefinitiveRenderReference::ApiVersionV6
                                                 : renderRequested && renderApi == 5 ? DefinitiveRenderReference::ApiVersionV5
                                                 : renderRequested && renderApi == 4 ? DefinitiveRenderReference::ApiVersionV4
                                                 : renderRequested && renderApi == 3 ? DefinitiveRenderReference::ApiVersionV3
                                                 : renderRoute                       ? DefinitiveRenderReference::ApiVersionV2
                                                 : renderRequested                   ? DefinitiveRenderReference::ApiVersion
                                                                                     : DefinitiveInputReference::ApiVersion;
                    if (!inputVersion ||
                        (guiFrontendRequested ? !configureFrontend
                         : renderRoute        ? !configureV2
                                              : !configure) ||
                        inputVersion() != expectedVersion)
                    {
                        values[16] = 13;
                        return;
                    }
                    inputOwner = this;
                    const auto duration = guiFrontendRequested  ? DefinitiveGuiFrontendReference::MaximumDurationMs
                                          : guiSurfaceRequested ? DefinitiveGuiSurfaceReference::MaximumDurationMs
                                          : guiRequested        ? DefinitiveGuiStartupReference::MaximumDurationMs
                                          : renderRequested     ? DefinitiveRenderReference::MaximumDurationMs
                                          : movementRequested   ? DefinitiveMovementReference::MaximumDurationMs
                                                                : DefinitiveInputReference::MaximumDurationMs;
                    if ((guiFrontendRequested
                             ? configureFrontend(DefinitiveGuiFrontendReference::ApiVersion, duration, InputCancelled, PublishInput, ControlInput)
                         : renderRoute ? configureV2(duration, renderRoute, InputCancelled, PublishInput)
                                       : configure(duration, InputCancelled, PublishInput)) != 1)
                    {
                        values[16] = 14;
                        return;
                    }
                }
                values[10] = static_cast<std::uint32_t>(initialize());
                // The real diagnostic export rejects foreign threads. All
                // Core calls, including readback and unload, stay on this worker.
                for (std::uint32_t field = 0; field < 8; ++field)
                    values[field] = read(field);
                if (inputRequested)
                {
                    // Native peer holds and GUI callbacks have ended. Wait for
                    // a final independent control read before taking the cache
                    // lock, so script capture contention cannot force quarantine.
                    DefinitiveGuiFrontendReference::ControlSnapshot finalControl{};
                    const bool                                      finalControlAvailable =
                        !guiFrontendRequested || frontendControl.CaptureFinalOnWorker(GetCurrentThreadId(), GetTickCount64(), finalControl);
                    AcquireSRWLockShared(&inputLock);
                    const auto complete = [](const auto& sample)
                    {
                        return sample[DefinitiveInputReference::Version] == DefinitiveInputReference::ApiVersion &&
                               sample[DefinitiveInputReference::Phase] == 2 && sample[DefinitiveInputReference::InputCleanupComplete] == 1;
                    };
                    const auto renderEnded = [](const auto& sample, std::uint32_t expected)
                    {
                        using namespace DefinitiveRenderReference;
                        return sample[DefinitiveRenderReference::Version] == expected && sample[Phase] == 2 && sample[CleanupComplete] == 1 &&
                               !sample[CleanupPending] && !sample[NativeCleanupPending] && !sample[CleanupDeadlineExceeded] &&
                               (sample[Reason] == static_cast<std::uint32_t>(StopReason::TimeBudget) ||
                                sample[Reason] == static_cast<std::uint32_t>(StopReason::Cancelled));
                    };
                    // Completing a diagnostic lease is separate from observing
                    // a callback. Even a zero-callback run must end and unload
                    // safely, with the requested protocol/route still intact.
                    const auto explicitRouteEnded = [&](const auto& sample, bool available, std::uint32_t version)
                    {
                        using namespace DefinitiveRenderReference;
                        return available && renderEnded(sample, version) && sample[RequestedRoute] == renderRoute &&
                               (!sample[ObservedRoute] || sample[ObservedRoute] == renderRoute) &&
                               sample[PresenceKnownMask] == (sample[ObservedRoute] ? (renderRoute == 2 ? 19u : 15u) : 0u);
                    };
                    const bool renderComplete =
                        renderApi == 6   ? explicitRouteEnded(publishedRenderV6, renderV6Available, DefinitiveRenderReference::ApiVersionV6)
                        : renderApi == 5 ? explicitRouteEnded(publishedRenderV5, renderV5Available, DefinitiveRenderReference::ApiVersionV5)
                        : renderApi == 4 ? explicitRouteEnded(publishedRenderV4, renderV4Available, DefinitiveRenderReference::ApiVersionV4)
                        : renderApi == 3 ? explicitRouteEnded(publishedRenderV3, renderV3Available, DefinitiveRenderReference::ApiVersionV3)
                        : renderRoute    ? renderV2Available && renderEnded(publishedRenderV2, DefinitiveRenderReference::ApiVersionV2) &&
                                            publishedRenderV2[DefinitiveRenderReference::RequestedRoute] == renderRoute &&
                                            (publishedRenderV2[DefinitiveRenderReference::ObservedRoute] == 0 ||
                                             publishedRenderV2[DefinitiveRenderReference::ObservedRoute] == renderRoute) &&
                                            publishedRenderV2[DefinitiveRenderReference::PresenceKnownMask] ==
                                                (publishedRenderV2[DefinitiveRenderReference::ObservedRoute] ? (renderRoute == 2 ? 19u : 15u) : 0u)
                                      : renderAvailable && renderEnded(publishedRender, DefinitiveRenderReference::ApiVersion);
                    const bool guiComplete =
                        guiAvailable && publishedGui[DefinitiveGuiStartupReference::Version] == DefinitiveGuiStartupReference::ApiVersion &&
                        publishedGui[DefinitiveGuiStartupReference::Phase] == 2 && publishedGui[DefinitiveGuiStartupReference::CleanupComplete] == 1 &&
                        !publishedGui[DefinitiveGuiStartupReference::CleanupPending] && !publishedGui[DefinitiveGuiStartupReference::CleanupDeadlineExceeded] &&
                        !publishedGui[DefinitiveGuiStartupReference::ObserverPendingCleanup] && !publishedGui[DefinitiveGuiStartupReference::ModuleRetained] &&
                        !publishedGui[DefinitiveGuiStartupReference::SessionRetained] && !publishedGui[DefinitiveGuiStartupReference::DeviceRetained] &&
                        !publishedGui[DefinitiveGuiStartupReference::DeviceReleaseFault];
                    // Core cannot unload while a child class, composition tree
                    // or pending resize still belongs to its surface owner.
                    const bool surfaceComplete = guiSurfaceAvailable &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::Version] == DefinitiveGuiSurfaceReference::ApiVersion &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::Phase] == 2 &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::CleanupComplete] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::ObserverEnded] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiStartupReference::GuiCleanupComplete] == 1 &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::GuiShutdownStatus] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::CleanupPending] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::CleanupDeadlineExceeded] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::ObserverPendingCleanup] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::ModuleRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::SessionRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::DeviceRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiStartupReference::DeviceReleaseFault] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::ChildRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::QueueRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::SurfaceRetained] &&
                                                 !publishedGuiSurface[DefinitiveGuiSurfaceReference::ResizePending] &&
                                                 publishedGuiSurface[DefinitiveGuiSurfaceReference::WindowCleanupComplete] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] == 1 &&
                                                 publishedGuiSurface[DefinitiveGuiSurfaceReference::ClassUnregistered] == 1;
                    const auto frontendComplete = [&]
                    {
                        using namespace DefinitiveGuiFrontendReference;
                        using namespace DefinitiveGuiStartupReference;
                        const auto& sample = publishedGuiFrontend;
                        const auto& control = finalControl;
                        // Independently read the mailbox: Core cannot turn an
                        // unrestored script obligation into an unload claim.
                        if (!guiFrontendAvailable || !finalControlAvailable)
                            return false;
                        const bool controlComplete = control[ControlField::Requested]
                                                         ? control[ControlField::Revoked] == 1 && control[ControlField::Detached] == 1 &&
                                                               control[ControlField::Restored] == 1 && sample[ControlRequested] == 1 &&
                                                               sample[ControlGeneration] == control[ControlField::Generation] && sample[ControlRestored] == 1 &&
                                                               sample[ControlInputDetached] == 1 && sample[ControlRevoked] == 1 && !sample[ControlFresh] &&
                                                               sample[ControlAcknowledged] == control[ControlField::Acknowledged] &&
                                                               sample[ControlFault] == control[ControlField::Fault]
                                                         : std::all_of(sample.begin() + ControlGeneration, sample.begin() + ControlFault + 1,
                                                                       [](std::uint32_t value) { return value == 0; });
                        const bool frontendRetired =
                            sample[Frontend + DefinitiveFrontend::Version] == 0
                                ? !sample[FrontendStarted] && !sample[FrontendStopped] &&
                                      std::all_of(sample.begin() + Frontend, sample.begin() + Frontend + DefinitiveFrontend::FieldCount,
                                                  [](std::uint32_t value) { return value == 0; })
                                : sample[Frontend + DefinitiveFrontend::Version] == DefinitiveFrontend::ApiVersion &&
                                      sample[Frontend + DefinitiveFrontend::Phase] == 4 && !sample[Frontend + DefinitiveFrontend::InputRequested] &&
                                      !sample[Frontend + DefinitiveFrontend::ReleaseFault] && sample[Frontend + DefinitiveFrontend::CleanupComplete] == 1 &&
                                      !sample[Frontend + DefinitiveFrontend::CleanupPending] && !sample[Frontend + DefinitiveFrontend::ServicesRetained] &&
                                      !sample[Frontend + DefinitiveFrontend::GuiBorrowed] && sample[FrontendStopped] == 1;
                        const bool inputRetired =
                            sample[InputDetached] == 1 || (!sample[DefinitiveGuiSurfaceReference::ChildCreated] && !sample[FrontendStarted] &&
                                                           !sample[InputEventsLow] && !sample[InputEventsHigh]);
                        // A missed cleanup deadline remains workload evidence;
                        // eventual proven retirement must still release Core.
                        return sample[DefinitiveGuiStartupReference::Version] == DefinitiveGuiFrontendReference::ApiVersion && sample[Phase] == 2 &&
                               sample[CleanupComplete] == 1 && sample[ObserverEnded] == 1 && sample[GuiCleanupComplete] == 1 && !sample[GuiShutdownStatus] &&
                               !sample[CleanupPending] && !sample[ObserverPendingCleanup] && !sample[ModuleRetained] && !sample[SessionRetained] &&
                               !sample[DeviceRetained] && !sample[DeviceReleaseFault] && !sample[DefinitiveGuiSurfaceReference::ChildRetained] &&
                               !sample[DefinitiveGuiSurfaceReference::QueueRetained] && !sample[DefinitiveGuiSurfaceReference::SurfaceRetained] &&
                               !sample[DefinitiveGuiSurfaceReference::ResizePending] && sample[DefinitiveGuiSurfaceReference::WindowCleanupComplete] == 1 &&
                               sample[DefinitiveGuiSurfaceReference::CompositionCleanupComplete] == 1 &&
                               sample[DefinitiveGuiSurfaceReference::ClassUnregistered] == 1 && frontendRetired && inputRetired && !sample[PressedKeys] &&
                               !sample[PressedButtons] && !sample[Frontend + DefinitiveFrontend::NetworkStarted] &&
                               !sample[Frontend + DefinitiveFrontend::GameplayReady] && !sample[Frontend + DefinitiveFrontend::FullMtaReady] &&
                               !sample[CameraCaptureVerified] && !sample[FullEngineInputReady] && controlComplete;
                    };
                    // Workload failure is separate from completing ownership
                    // cleanup. The GUI snapshot retains its own outcome facts.
                    const bool inputComplete = guiFrontendRequested  ? frontendComplete()
                                               : guiSurfaceRequested ? surfaceComplete
                                               : guiRequested        ? guiComplete
                                               : renderRequested     ? renderComplete
                                               : movementRequested   ? movementAvailable && complete(publishedMovement) &&
                                                                         !publishedMovement[DefinitiveMovementReference::CleanupPending]
                                                                   : inputAvailable && complete(publishedInput);
                    ReleaseSRWLockShared(&inputLock);
                    if (!inputComplete)
                    {
                        values[16] = 15;
                        frontendQuarantined = guiFrontendRequested;
                        return;
                    }
                }
                if (CheckCancellation())
                    return;
                if (values[8] != 3 || values[9] != 4 || values[10] != 5)
                {
                    values[16] = 7;
                    return;
                }
                if (values[0] != 1 || values[1] != 5 || values[2] != 6 || values[3] != 97 || values[4] || values[5] || values[6] || values[7] != 1)
                {
                    values[16] = 8;
                    return;
                }
                values[17] = 1;
            };
            run();
        }
        catch (...)
        {
            values[16] = 11;
        }
        inputOwner = nullptr;
        // Cancellation never interrupts the loader or unloads Core from a
        // second thread. Completion is published only after this owned cleanup.
        if (guiFrontendRequested && core && values[16] == 11)
            frontendQuarantined = true;
        if (core && !frontendQuarantined && !FreeLibrary(core))
            values[16] = 9;
        values[12] = LegacyRendererModules();
        values[13] = GetModuleHandleW(L"core.dll") ? 0u : 1u;
        values[15] = GetModuleHandleW(L"game_de.dll") ? 1u : 0u;
        if ((values[12] & ~values[11]) && !values[16])
            values[16] = 10;
        const auto expectedBackend =
            guiFrontendRequested && guiFrontendAvailable             ? publishedGuiFrontend[DefinitiveGuiStartupReference::BackendPinnedExpected]
            : guiSurfaceRequested && guiSurfaceAvailable             ? publishedGuiSurface[DefinitiveGuiStartupReference::BackendPinnedExpected]
            : guiRequested && guiAvailable                           ? publishedGui[DefinitiveGuiStartupReference::BackendPinnedExpected]
            : renderRequested && renderApi == 6 && renderV6Available ? publishedRenderV6[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderApi == 5 && renderV5Available ? publishedRenderV5[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderApi == 4 && renderV4Available ? publishedRenderV4[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderApi == 3 && renderV3Available ? publishedRenderV3[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderRoute && renderV2Available    ? publishedRenderV2[DefinitiveRenderReference::BackendPinnedExpected]
            : renderRequested && renderAvailable                     ? publishedRender[DefinitiveRenderReference::BackendPinnedExpected]
            : movementRequested && movementAvailable                 ? publishedMovement[DefinitiveMovementReference::BackendPinnedExpected]
                                                                     : 0u;
        if ((!values[13] || values[15] != expectedBackend) && !values[16])
            values[16] = 9;
    }
}
