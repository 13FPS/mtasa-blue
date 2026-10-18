/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <windows.h>
#include "../core/DefinitiveInputReference.h"
#include "../core/DefinitiveMovementReference.h"
#include "../core/DefinitiveClockReference.h"
#include "../core/DefinitiveClockSetReference.h"
#include "../core/DefinitiveRenderReference.h"
#include "../core/DefinitiveGuiStartupReference.h"
#include "../core/DefinitiveGuiSurfaceReference.h"
#include "../core/DefinitiveGuiFrontendInputReference.h"
#include "FrontendControlMailbox.h"
#include "FrontendStartupReport.h"
#include "PersistentFrontendHostLease.h"
#include "../core/DefinitivePersistentFrontend.h"
#include "../core/DefinitiveFrontendClockReference.h"
#include "../core/DefinitiveFrontendRuntime.h"
#include "FrontendRuntimeMailbox.h"
#include "FrontendRuntimeCache.h"

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
        bool          BeginClockReference(const void* moduleAddress) noexcept;
        bool          CaptureClock() noexcept;
        std::uint32_t ReadClock(std::uint32_t field) const noexcept;
        bool          BeginClockSetReference(const void* moduleAddress) noexcept;
        bool          CaptureClockSet() noexcept;
        std::uint32_t ReadClockSet(std::uint32_t field) const noexcept;
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
        bool          BeginRenderReferenceV6(const void* moduleAddress, std::uint32_t route) noexcept;
        bool          CaptureRenderV6() noexcept;
        std::uint32_t ReadRenderV6(std::uint32_t field) const noexcept;
        bool          BeginGuiStartupReference(const void* moduleAddress) noexcept;
        bool          CaptureGuiStartup() noexcept;
        std::uint32_t ReadGuiStartup(std::uint32_t field) const noexcept;
        bool          BeginGuiSurfaceReference(const void* moduleAddress) noexcept;
        bool          CaptureGuiSurface() noexcept;
        std::uint32_t ReadGuiSurface(std::uint32_t field) const noexcept;
        bool          BeginGuiFrontendReference(const void* moduleAddress) noexcept;
        bool          CaptureGuiFrontend() noexcept;
        std::uint32_t ReadGuiFrontend(std::uint32_t field) const noexcept;
        bool          BeginGuiFrontendInputReference(const void* moduleAddress) noexcept;
        bool          CaptureGuiFrontendInput() noexcept;
        std::uint32_t ReadGuiFrontendInput(std::uint32_t field) const noexcept;
        bool          BeginGuiFrontendStartupReference(const void* moduleAddress) noexcept;
        bool          BeginGuiFrontendStartupReferenceV2(const void* moduleAddress) noexcept;
        bool          CaptureGuiFrontendStartupClockV2() noexcept;
        std::uint32_t ReadGuiFrontendStartupClockV2(std::uint32_t field) const noexcept;
        bool          CaptureGuiFrontendStartupClockOwnerV2() noexcept;
        std::uint32_t ReadGuiFrontendStartupClockOwnerV2(std::uint32_t field) const noexcept;
        bool          CaptureGuiFrontendStartup() noexcept;
        std::uint32_t ReadGuiFrontendStartup(std::uint32_t field) const noexcept;
        std::uint32_t BeginPersistentFrontend(const void* moduleAddress) noexcept;
        bool          HeartbeatPersistentFrontend(std::uint32_t generation) noexcept;
        bool          CapturePersistentFrontend() noexcept;
        std::uint32_t ReadPersistentFrontend(std::uint32_t field) const noexcept;
        bool          CapturePersistentFrontendLifecycle() noexcept;
        std::uint32_t ReadPersistentFrontendLifecycle(std::uint32_t field) const noexcept;
        std::uint32_t BeginFrontendRuntime(const void* moduleAddress) noexcept;
        bool          CommandFrontendRuntime(std::uint32_t generation, std::uint32_t sequence, std::uint32_t command) noexcept;
        bool          HeartbeatFrontendRuntime(std::uint32_t generation) noexcept;
        int           PollFrontendRuntime() noexcept;
        bool          CaptureFrontendRuntime() noexcept;
        std::uint32_t ReadFrontendRuntime(std::uint32_t field) const noexcept;
        bool          CaptureGuiFrontendControl() noexcept;
        std::uint32_t ReadGuiFrontendControl(std::uint32_t field) const noexcept;
        bool          AcknowledgeGuiFrontendControl(std::uint32_t generation) noexcept;
        bool          HeartbeatGuiFrontendControl(std::uint32_t generation) noexcept;
        bool          RevokeGuiFrontendControl(std::uint32_t generation) noexcept;
        bool          RestoreGuiFrontendControl(std::uint32_t generation) noexcept;
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
#ifdef MTA_TEST_CORE_STARTUP_ACCESS
        // The owned worker fixture must contend on this exact cache lock while
        // invoking the production callback, without a runtime test bypass.
        friend struct CoreStartupReferenceTestAccess;
#endif
        static unsigned __stdcall ThreadMain(void* context) noexcept;
        void                      Run() noexcept;
        bool                      CheckCancellation() noexcept;
        void                      RunFrontendStartup(HMODULE core, const std::wstring& mtaDirectory, const std::wstring& gtaDirectory) noexcept;
        void                      PublishFrontendStartup(const FrontendStartupReport::Snapshot& report) noexcept;
        void                      RunFrontendRuntime(HMODULE core, const std::wstring& mtaDirectory, const std::wstring& gtaDirectory) noexcept;
        void                      PublishRuntime() noexcept;
        void                      FinishRuntime(HMODULE core) noexcept;
        static bool               RuntimeViewCancelled();
        static bool               RuntimeHostFresh();
        static void               PublishRuntimeView(const std::uint32_t* values, std::uint32_t count);
        bool                      Start(const void* moduleAddress, bool configuredInput, bool configuredMovement = false, bool configuredRender = false,
                                        std::uint32_t configuredRenderRoute = 0, std::uint32_t configuredRenderApi = 1, bool configuredGuiStartup = false,
                                        bool configuredGuiSurface = false, bool configuredGuiFrontend = false, bool configuredGuiFrontendInput = false,
                                        bool configuredFrontendStartup = false, bool configuredPersistentFrontend = false, bool configuredClock = false,
                                        bool configuredClockSet = false, bool configuredClockBeforeFrontend = false, bool configuredFrontendRuntime = false) noexcept;
        bool                      CopyClockSnapshot(DefinitiveClockReference::Snapshot& output) const noexcept;
        bool                      CopyClockSetSnapshot(DefinitiveClockSetReference::Snapshot& output) const noexcept;
        void                      WaitForClockRetirement() noexcept;
        void                      PublishClockOwner(const DefinitiveFrontendStartup::Snapshot& report) noexcept;
        bool                      CaptureFrontendLifecycle() noexcept;
        static bool               InputCancelled();
        static bool               PersistentHostFresh();
        static void               PublishPersistentFrontend(const std::uint32_t* values, std::uint32_t count);
        static void               PublishInput(const std::uint32_t* values, std::uint32_t count);
        static void               PublishClock(const std::uint32_t* values, std::uint32_t count);
        static void               PublishClockSet(const std::uint32_t* values, std::uint32_t count);
        static bool               ControlInput(std::uint32_t action, std::uint32_t* values, std::uint32_t count);

        // Only Begin/Poll's owning thread touches the handle or input address.
        // State2 publishes the worker's immutable values; State3 also proves
        // the thread has exited, allowing another attempt without a live job.
        std::atomic<DWORD>            ownerThread{0};
        std::atomic<unsigned>         state{0};
        std::atomic<bool>             cancellation{false};
        HANDLE                        thread = nullptr;
        const void*                   address = nullptr;
        std::array<std::uint32_t, 19> values{};
        bool                          inputRequested = false;
        bool                          movementRequested = false;
        bool                          clockRequested = false;
        bool                          clockSetRequested = false;
        bool                          clockBeforeFrontendRequested = false;
        bool                          clockInitializationInvoked = false, clockQuarantined = false;
        // Clock cleanup can retain a script peer inside the historical cache
        // lock. Its independent publication/capture path never takes that lock.
        std::array<std::atomic<std::uint32_t>, DefinitiveClockReference::FieldCount> publishedClock{};
        std::atomic<std::uint32_t>                                                   clockSequence{0};
        std::atomic<bool>                                                            clockAvailable{false};
        DefinitiveClockReference::Snapshot                                           capturedClock{};
        // The completed owner is copied only after its independent clock
        // retirement proof, before admitting the existing GUI view.
        std::array<std::atomic<std::uint32_t>, DefinitiveFrontendStartup::FieldCount> publishedClockOwner{};
        std::atomic<std::uint32_t>                                                    clockOwnerSequence{0};
        std::atomic<bool>                                                             clockOwnerAvailable{false};
        DefinitiveFrontendStartup::Snapshot                                           capturedClockOwner{};
        // Set has a distinct report so its bounded write proof cannot be
        // interpreted as the historical API1 read-only observation.
        std::array<std::atomic<std::uint32_t>, DefinitiveClockSetReference::FieldCount> publishedClockSet{};
        std::atomic<std::uint32_t>                                                      clockSetSequence{0};
        std::atomic<bool>                                                               clockSetAvailable{false};
        DefinitiveClockSetReference::Snapshot                                           capturedClockSet{};
        bool                                                                            renderRequested = false;
        bool                                                                            guiRequested = false;
        bool                                                                            guiAvailable = false;
        DefinitiveGuiStartupReference::Snapshot                                         publishedGui{}, capturedGui{};
        bool                                                                            guiSurfaceRequested = false, guiSurfaceAvailable = false;
        DefinitiveGuiSurfaceReference::Snapshot                                         publishedGuiSurface{}, capturedGuiSurface{};
        bool                                                                            guiFrontendRequested = false, guiFrontendAvailable = false;
        DefinitiveGuiFrontendReference::Snapshot                                        publishedGuiFrontend{}, capturedGuiFrontend{};
        bool                                                                            guiFrontendInputRequested = false, guiFrontendInputAvailable = false;
        DefinitiveGuiFrontendInputReference::Snapshot                                   publishedGuiFrontendInput{}, capturedGuiFrontendInput{};
        bool                                                                            frontendStartupRequested = false;
        bool                                                                            persistentFrontendRequested = false;
        PersistentFrontendHostLease                                                     persistentHostLease;
        bool                                                                            frontendRuntimeRequested = false;
        FrontendRuntimeMailbox                                                          runtimeMailbox;
        PersistentFrontendHostLease                                                     runtimeHostLease;
        FrontendRuntimeCache                                                            runtimeCache;
        FrontendRuntimeReport::Snapshot                                                 runtimeReport{}, capturedRuntime{};
        FrontendRuntimeMailbox::Request                                                 runtimeViewRequest{};
        std::atomic<bool>                                                               runtimePublicationFault{false};
        std::uint64_t                                                                   runtimeStartedAt = 0, runtimePublicationSequence = 0;
        bool                                                                            runtimeViewAvailable = false;
        // Persistent reports never enter the historical 256-field cache.
        std::array<std::atomic<std::uint32_t>, DefinitivePersistentFrontend::FieldCount> publishedPersistentFrontend{};
        std::atomic<std::uint32_t>                                                       persistentFrontendSequence{0};
        std::atomic<bool>                                                                persistentFrontendAvailable{false};
        DefinitivePersistentFrontend::Snapshot                                           capturedPersistentFrontend{};
        // Atomic scalar publication cannot block a worker behind a script peer
        // retained inside the other protocol's capture lock during cleanup.
        std::array<std::atomic<std::uint32_t>, FrontendStartupReport::FieldCount> publishedFrontendStartup{};
        std::atomic<std::uint32_t>                                                frontendStartupSequence{0};
        std::atomic<bool>                                                         frontendStartupAvailable{false};
        FrontendStartupReport::Snapshot                                           capturedFrontendStartup{};
        FrontendControlMailbox                                                    frontendControl;
        FrontendControlMailbox::Snapshot                                          capturedFrontendControl{};
        // Retain a faulty new frontend's code rather than unloading callbacks
        // or allowing a second attempt while ownership remains unresolved.
        bool                                  frontendQuarantined = false;
        std::uint32_t                         renderRoute = 0;
        std::uint32_t                         renderApi = 1;
        bool                                  renderV3Available = false;
        DefinitiveRenderReference::SnapshotV3 publishedRenderV3{}, capturedRenderV3{};
        bool                                  renderV4Available = false;
        DefinitiveRenderReference::SnapshotV4 publishedRenderV4{}, capturedRenderV4{};
        bool                                  renderV5Available = false;
        DefinitiveRenderReference::SnapshotV5 publishedRenderV5{}, capturedRenderV5{};
        bool                                  renderV6Available = false;
        DefinitiveRenderReference::SnapshotV6 publishedRenderV6{}, capturedRenderV6{};
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
