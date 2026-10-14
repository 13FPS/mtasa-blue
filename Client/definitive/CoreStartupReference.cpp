/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#include "CoreStartupReference.h"
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

    bool CoreStartupReference::Start(const void* moduleAddress, bool configuredInput, bool configuredMovement, bool configuredRender,
                                     std::uint32_t configuredRenderRoute, std::uint32_t configuredRenderApi) noexcept
    {
        DWORD      expected = 0;
        const auto current = GetCurrentThreadId();
        if ((configuredMovement && configuredRender) || (!ownerThread.compare_exchange_strong(expected, current) && expected != current) || !moduleAddress ||
            IsPending() || thread)
            return false;
        address = moduleAddress;
        inputRequested = configuredInput;
        movementRequested = configuredMovement;
        renderRequested = configuredRender;
        renderRoute = configuredRenderRoute;
        renderApi = configuredRenderApi;
        AcquireSRWLockExclusive(&inputLock);
        inputAvailable = false;
        publishedInput = {};
        movementAvailable = false;
        publishedMovement = {};
        renderAvailable = false;
        publishedRender = {};
        renderV3Available = false;
        publishedRenderV3 = {};
        renderV2Available = false;
        publishedRenderV2 = {};
        ReleaseSRWLockExclusive(&inputLock);
        capturedInput = {};
        capturedMovement = {};
        capturedRender = {};
        capturedRenderV2 = {};
        capturedRenderV3 = {};
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

    bool CoreStartupReference::InputCancelled()
    {
        return !inputOwner || inputOwner->cancellation.load(std::memory_order_acquire);
    }

    void CoreStartupReference::PublishInput(const std::uint32_t* snapshot, std::uint32_t count)
    {
        if (!inputOwner || !snapshot ||
            count != (inputOwner->renderRequested     ? (inputOwner->renderApi == 3 ? DefinitiveRenderReference::FieldCountV3
                                                         : inputOwner->renderRoute  ? DefinitiveRenderReference::FieldCountV2
                                                                                    : DefinitiveRenderReference::FieldCount)
                      : inputOwner->movementRequested ? DefinitiveMovementReference::FieldCount
                                                      : DefinitiveInputReference::FieldCount))
            return;
        AcquireSRWLockExclusive(&inputOwner->inputLock);
        if (inputOwner->renderRequested && inputOwner->renderApi == 3)
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
                    const auto inputVersion =
                        reinterpret_cast<Version>(GetProcAddress(core, renderRequested && renderApi == 3 ? "MTA_DERenderReferenceApiVersionV3"
                                                                       : renderRoute                     ? "MTA_DERenderReferenceApiVersionV2"
                                                                       : renderRequested                 ? "MTA_DERenderReferenceApiVersion"
                                                                       : movementRequested               ? "MTA_DEMovementReferenceApiVersion"
                                                                                                         : "MTA_DEInputReferenceApiVersion"));
                    const auto configure = reinterpret_cast<Configure>(GetProcAddress(core, renderRequested     ? "MTA_ConfigureDERenderReference"
                                                                                            : movementRequested ? "MTA_ConfigureDEMovementReference"
                                                                                                                : "MTA_ConfigureDEInputReference"));
                    const auto configureV2 = reinterpret_cast<ConfigureRenderV2>(
                        GetProcAddress(core, renderApi == 3 ? "MTA_ConfigureDERenderReferenceV3" : "MTA_ConfigureDERenderReferenceV2"));
                    const auto expectedVersion = renderRequested && renderApi == 3 ? DefinitiveRenderReference::ApiVersionV3
                                                 : renderRoute                     ? DefinitiveRenderReference::ApiVersionV2
                                                 : renderRequested                 ? DefinitiveRenderReference::ApiVersion
                                                                                   : DefinitiveInputReference::ApiVersion;
                    if (!inputVersion || (renderRoute ? !configureV2 : !configure) || inputVersion() != expectedVersion)
                    {
                        values[16] = 13;
                        return;
                    }
                    inputOwner = this;
                    const auto duration = renderRequested     ? DefinitiveRenderReference::MaximumDurationMs
                                          : movementRequested ? DefinitiveMovementReference::MaximumDurationMs
                                                              : DefinitiveInputReference::MaximumDurationMs;
                    if ((renderRoute ? configureV2(duration, renderRoute, InputCancelled, PublishInput) : configure(duration, InputCancelled, PublishInput)) !=
                        1)
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
                        renderApi == 3 ? explicitRouteEnded(publishedRenderV3, renderV3Available, DefinitiveRenderReference::ApiVersionV3)
                        : renderRoute  ? renderV2Available && renderEnded(publishedRenderV2, DefinitiveRenderReference::ApiVersionV2) &&
                                            publishedRenderV2[DefinitiveRenderReference::RequestedRoute] == renderRoute &&
                                            (publishedRenderV2[DefinitiveRenderReference::ObservedRoute] == 0 ||
                                             publishedRenderV2[DefinitiveRenderReference::ObservedRoute] == renderRoute) &&
                                            publishedRenderV2[DefinitiveRenderReference::PresenceKnownMask] ==
                                                (publishedRenderV2[DefinitiveRenderReference::ObservedRoute] ? (renderRoute == 2 ? 19u : 15u) : 0u)
                                      : renderAvailable && renderEnded(publishedRender, DefinitiveRenderReference::ApiVersion);
                    const bool inputComplete = renderRequested     ? renderComplete
                                               : movementRequested ? movementAvailable && complete(publishedMovement) &&
                                                                         !publishedMovement[DefinitiveMovementReference::CleanupPending]
                                                                   : inputAvailable && complete(publishedInput);
                    ReleaseSRWLockShared(&inputLock);
                    if (!inputComplete)
                    {
                        values[16] = 15;
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
        if (core && !FreeLibrary(core))
            values[16] = 9;
        values[12] = LegacyRendererModules();
        values[13] = GetModuleHandleW(L"core.dll") ? 0u : 1u;
        values[15] = GetModuleHandleW(L"game_de.dll") ? 1u : 0u;
        if ((values[12] & ~values[11]) && !values[16])
            values[16] = 10;
        const auto expectedBackend = renderRequested && renderApi == 3 && renderV3Available
                                         ? publishedRenderV3[DefinitiveRenderReference::BackendPinnedExpected]
                                     : renderRequested && renderRoute && renderV2Available ? publishedRenderV2[DefinitiveRenderReference::BackendPinnedExpected]
                                     : renderRequested && renderAvailable                  ? publishedRender[DefinitiveRenderReference::BackendPinnedExpected]
                                     : movementRequested && movementAvailable ? publishedMovement[DefinitiveMovementReference::BackendPinnedExpected]
                                                                              : 0u;
        if ((!values[13] || values[15] != expectedBackend) && !values[16])
            values[16] = 9;
    }
}
