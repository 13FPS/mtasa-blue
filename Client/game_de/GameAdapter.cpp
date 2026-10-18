/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core-owned DE adapter lifetime and checked local-observation storage
 *****************************************************************************/

#include <game/CGameAdapter.h>
#include <game/CGameRenderObservation.h>
#include <game/CGameRenderEndpointDiagnostic.h>
#include <game/CGameRenderSetupDiagnostic.h>
#include <game/CGameRenderDeviceLease.h>
#include <game/CGameRenderDeviceWindowLease.h>
#include <game/CGameFrontendInputLease.h>
#include <game/CGameLocalInputLease.h>
#include <game/CGameClockService.h>
#include "FrontendInputLeasePolicy.h"
#include "LocalInputLeasePolicy.h"
#include "ControllerSnapshot.h"
#include "RemoteDataStorage.h"
#include "NativePadReader.h"
#include "NativeMovementInput.h"
#include "NativeClockService.h"
#include "NativeRenderObservation.h"

#include <atomic>
#include <cmath>
#include <new>
#include <windows.h>

namespace mta::game_de
{
    namespace
    {
        class GameAdapter;
        SRWLOCK      frontendAdapterLock = SRWLOCK_INIT;
        GameAdapter* frontendAdapters = nullptr;

        bool Finite(const AdapterVector3& value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        class GameAdapter final : public IGameAdapter
        {
        public:
            GameAdapter() noexcept
            {
                // Optional exports validate a retained factory object against
                // this registry before dereferencing a caller-supplied pointer.
                AcquireSRWLockExclusive(&frontendAdapterLock);
                nextFrontendAdapter = frontendAdapters;
                frontendAdapters = this;
                ReleaseSRWLockExclusive(&frontendAdapterLock);
            }
            ~GameAdapter() override
            {
                AcquireSRWLockExclusive(&frontendAdapterLock);
                auto** entry = &frontendAdapters;
                while (*entry && *entry != this)
                    entry = &(*entry)->nextFrontendAdapter;
                if (*entry)
                    *entry = nextFrontendAdapter;
                ReleaseSRWLockExclusive(&frontendAdapterLock);
            }

            enum class FrontendLeaseAction
            {
                Begin,
                Renew,
                Read,
                End
            };
            enum class ClockAction
            {
                Begin,
                Submit,
                Renew,
                Read,
                End
            };
            enum class LocalLeaseAction
            {
                Begin,
                Renew,
                Read,
                End
            };
            static Status LocalLease(IGameAdapter* adapter, LocalLeaseAction action, const LocalInputLeaseRequestV1* request, std::uint64_t generation,
                                     const MtaInputState* movement, LocalInputLeaseStateV1* output) noexcept
            {
                // The optional API shares the retained-adapter registry. It
                // must not dereference an unknown object or wait on a registry
                // lock while the producer needs native hook quiescence.
                if (!TryAcquireSRWLockShared(&frontendAdapterLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = frontendAdapters; current; current = current->nextFrontendAdapter)
                {
                    if (static_cast<IGameAdapter*>(current) != adapter)
                        continue;
                    result = current->Access();
                    if (result != Status::Ok)
                        break;
                    if (action == LocalLeaseAction::Begin)
                    {
                        if (current->nativeClock.IsRetained())
                            result = Status::Unsupported;
                        else if (!NativeMovementInput::ValidateLocalLeaseRequest(*request))
                            result = Status::InvalidArgument;
                        else if (!current->nativePad.IsBound() || !current->hasInputState || !LocalInputLeasePolicy::Neutral(current->inputStorage.ReadInput()))
                            result = Status::NotInitialized;
                        else
                        {
                            const bool             started = current->nativeMovement.BeginLocalLease(*request);
                            LocalInputLeaseStateV1 state;
                            // A valid native attempt can retain partial hooks
                            // after refusal. Return that real token for End.
                            if (current->nativeMovement.ReadLocalLease(state))
                                *output = state;
                            result = started ? Status::Ok : Status::Unsupported;
                        }
                    }
                    else if (action == LocalLeaseAction::Renew)
                        result = current->nativeMovement.RenewLocalLease(generation, *movement) ? Status::Ok : Status::Unsupported;
                    else if (action == LocalLeaseAction::End)
                        result = current->nativeMovement.EndLocalLease(generation) ? Status::Ok : Status::Unsupported;
                    else
                    {
                        LocalInputLeaseStateV1 state;
                        if (current->nativeMovement.ReadLocalLease(state))
                        {
                            *output = state;
                            result = Status::Ok;
                        }
                        else
                            result = Status::Unsupported;
                    }
                    break;
                }
                ReleaseSRWLockShared(&frontendAdapterLock);
                return result;
            }
            static Status ClockService(IGameAdapter* adapter, ClockAction action, const ClockServiceRequestV1* request, const ClockCommandV1* command,
                                       std::uint64_t generation, ClockServiceStateV1* output) noexcept
            {
                // The same retained-adapter registry prevents optional service
                // calls from borrowing an arbitrary or already released object.
                if (!TryAcquireSRWLockShared(&frontendAdapterLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = frontendAdapters; current; current = current->nextFrontendAdapter)
                {
                    if (static_cast<IGameAdapter*>(current) != adapter)
                        continue;
                    result = current->Access();
                    if (result != Status::Ok)
                        break;
                    bool completed = false;
                    switch (action)
                    {
                        case ClockAction::Begin:
                            if (current->nativeMovement.IsBound() || current->nativeClock.IsRetained())
                                break;
                            completed = current->nativeClock.Begin(*request, *output);
                            break;
                        case ClockAction::Submit:
                            completed = current->nativeClock.Submit(*command);
                            break;
                        case ClockAction::Renew:
                            completed = current->nativeClock.Renew(generation);
                            break;
                        case ClockAction::Read:
                            completed = current->nativeClock.Read(*output);
                            break;
                        case ClockAction::End:
                            completed = current->nativeClock.End(generation);
                            break;
                    }
                    result = completed ? Status::Ok : Status::Unsupported;
                    break;
                }
                ReleaseSRWLockShared(&frontendAdapterLock);
                return result;
            }
            static Status FrontendLease(IGameAdapter* adapter, FrontendLeaseAction action, const FrontendInputLeaseRequestV1* request, std::uint64_t generation,
                                        const MtaInputState* neutral, FrontendInputLeaseStateV1* output) noexcept
            {
                // Nonblocking registry access cannot strand a simulation thread
                // while the producer installs/detaches existing native hooks.
                if (!TryAcquireSRWLockShared(&frontendAdapterLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = frontendAdapters; current; current = current->nextFrontendAdapter)
                {
                    if (static_cast<IGameAdapter*>(current) != adapter)
                        continue;
                    result = current->Access();
                    if (result != Status::Ok)
                        break;
                    if (action == FrontendLeaseAction::Begin)
                    {
                        if (current->nativeClock.IsRetained())
                            result = Status::Unsupported;
                        else if (!NativeMovementInput::ValidateFrontendLeaseRequest(*request))
                            result = Status::InvalidArgument;
                        else if (!current->nativePad.IsBound() || !current->hasInputState ||
                                 !FrontendInputLeasePolicy::Neutral(current->inputStorage.ReadInput()))
                            result = Status::NotInitialized;
                        else
                        {
                            const bool                started = current->nativeMovement.BeginFrontendLease(*request);
                            FrontendInputLeaseStateV1 state;
                            // A valid native attempt may own partial hooks even
                            // on refusal. Publish its token for explicit End.
                            if (current->nativeMovement.ReadFrontendLease(state))
                                *output = state;
                            result = started ? Status::Ok : Status::Unsupported;
                        }
                    }
                    else if (action == FrontendLeaseAction::Renew)
                        result = current->nativeMovement.RenewFrontendLease(generation, *neutral) ? Status::Ok : Status::Unsupported;
                    else if (action == FrontendLeaseAction::End)
                        result = current->nativeMovement.EndFrontendLease(generation) ? Status::Ok : Status::Unsupported;
                    else
                    {
                        FrontendInputLeaseStateV1 state;
                        if (current->nativeMovement.ReadFrontendLease(state))
                        {
                            *output = state;
                            result = Status::Ok;
                        }
                        else
                            result = Status::Unsupported;
                    }
                    break;
                }
                ReleaseSRWLockShared(&frontendAdapterLock);
                return result;
            }

            Status Initialize(CCoreInterface* newOwner) noexcept override
            {
                if (!newOwner)
                    return Status::InvalidArgument;
                const auto currentThread = GetCurrentThreadId();
                DWORD      unbound = 0;
                if (!ownerThread.compare_exchange_strong(unbound, currentThread) && unbound != currentThread)
                    return Status::WrongThread;
                if (owner && owner != newOwner)
                    return Status::InvalidArgument;
                owner = newOwner;
                initialized = true;
                return Status::Ok;
            }

            Status Pulse() noexcept override { return Access(); }

            Status Reset() noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (!nativeClock.End() || !EndMovement())
                    return Status::Unsupported;
                Clear();
                return Status::Ok;
            }

            Status Shutdown() noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (!nativeClock.End() || !EndMovement())
                    return Status::Unsupported;
                Clear();
                initialized = false;
                return Status::Ok;
            }

            void Release() noexcept override
            {
                // A live detour still references this module's state. A caller
                // cannot turn failed cleanup into a dangling callback by Release.
                if ((ownerThread.load() && ownerThread.load() != GetCurrentThreadId()) || !nativeClock.End() || !EndMovement())
                    return;
                delete this;
            }

            std::uint64_t GetCapabilities() const noexcept override
            {
                // A successful address read remains a diagnostic capability;
                // the live trial did not establish an engine controller layout.
                return capabilities.load();
            }

            CGame* GetGameInterface() const noexcept override { return nullptr; }

            Status PublishLocalObservation(const LocalObservation& observation) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (!observation.localPedHandle || !Finite(observation.transform.position) || !std::isfinite(observation.transform.heading) ||
                    !Finite(observation.rawVelocity) || (observation.controllerKnownMask != 0 && observation.controllerKnownMask != AdapterControllerMask) ||
                    observation.controller.m_bChatIndicated || observation.controller.m_bPedWalk || observation.controller.m_bVehicleMouseLook ||
                    observation.controller.m_bRadioTrackSkip)
                    return Status::InvalidArgument;

                // Stage validation before replacing either pose or controller
                // history. No coverage means zero storage is explicitly unknown.
                auto nextController = controller;
                if (!nextController.Publish(observation.controller))
                    return Status::InvalidArgument;
                if (!observation.controllerKnownMask)
                {
                    for (std::uint32_t index = 0; index < deonline::ControllerSnapshot::NativeFieldCount; ++index)
                    {
                        if (nextController.Read(0, index) != 0)
                            return Status::InvalidArgument;
                    }
                    nextController.Reset();
                }
                controller = nextController;
                latest = observation;
                latest.controller = controller.Current();
                latest.controllerKnownMask = controller.CurrentMask();
                hasObservation = true;
                return Status::Ok;
            }

            Status GetLocalObservation(LocalObservation& observation) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (!hasObservation)
                    return Status::NotInitialized;
                observation = latest;
                return Status::Ok;
            }

            Status SubmitControlActions(const ControlActionFrame& frame) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if ((frame.heldActions & ~MtaInput::KnownActionMask) || (nativeMovement.IsActive() && !TrialFrame(frame)))
                {
                    if (!EndMovement())
                        return Status::Unsupported;
                    return Status::InvalidArgument;
                }

                // Keep logical MTA inputs separate from native evidence, whose
                // twenty-field mask cannot describe MTA's walk/radio fields.
                auto          nextHistory = directionHistory;
                MtaInputState next;
                next.previous = *inputStorage.CurrentControllerState();
                next.current = MtaInput::BuildControllerState(frame.heldActions, frame.context, nextHistory);
                if (nativeMovement.IsActive() && nativeMovement.IsFrontendLease() && !FrontendInputLeasePolicy::Neutral(next))
                {
                    EndMovement();
                    return Status::InvalidArgument;
                }
                // Publishing MTA's canonical neutral history remains useful,
                // but only the explicit lease token API may renew native input.
                if (nativeMovement.IsActive() && !nativeMovement.IsFrontendLease() && !nativeMovement.IsLocalLease() && !nativeMovement.Update(next))
                {
                    EndMovement();
                    return Status::Unsupported;
                }
                directionHistory = nextHistory;
                inputStorage.WriteInput(next);
                inputFrame = frame;
                hasInputState = true;
                return Status::Ok;
            }

            Status GetMtaInputState(MtaInputState& state) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (!hasInputState)
                    return Status::NotInitialized;
                state = inputStorage.ReadInput();
                return Status::Ok;
            }

            Status BindNativeInput() noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                // Ordinary module startup does not hash the executable or
                // touch engine memory before its owning Core requests binding.
                const bool bound = nativePad.Bind();
                capabilities.store(BaseCapabilities | (bound ? NativePadBindingCapabilities : 0) | MovementCapability());
                return bound ? Status::Ok : Status::Unsupported;
            }

            Status ReadNativeControllerState(std::uint32_t padIndex, NativeControllerState& state) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (padIndex >= 2)
                    return Status::InvalidArgument;
                if (!nativePad.IsBound())
                    return Status::Unsupported;
                NativeControllerState next;
                if (!nativePad.Read(padIndex, next.current, next.previous))
                {
                    nativePad.Reset();
                    capabilities.store(BaseCapabilities | MovementCapability());
                    return Status::Unsupported;
                }
                state = next;
                return Status::Ok;
            }

            Status ReadNativeDeviceState(NativeDeviceState& state) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                NativeDeviceState next;
                if (!nativePad.ReadDeviceState(next))
                {
                    nativePad.Reset();
                    capabilities.store(BaseCapabilities | MovementCapability());
                    return Status::Unsupported;
                }
                state = next;
                capabilities.fetch_or(static_cast<std::uint64_t>(AdapterCapability::NativeDeviceStateCandidateRead));
                return Status::Ok;
            }

            Status BeginNativeMovementTrial() noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (nativeClock.IsRetained())
                    return Status::Unsupported;
                if (!hasInputState)
                    return Status::NotInitialized;
                if (!TrialFrame(inputFrame))
                    return Status::InvalidArgument;
                const bool started = nativeMovement.Begin(inputStorage.ReadInput());
                RefreshMovementCapability();
                return started ? Status::Ok : Status::Unsupported;
            }

            Status GetNativeMovementTrialState(NativeMovementTrialState& state) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                nativeMovement.ReadState(state);
                RefreshMovementCapability();
                return Status::Ok;
            }

            Status EndNativeMovementTrial() noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                return EndMovement() ? Status::Ok : Status::Unsupported;
            }

            Status GetNativeMovementEligibility(NativeMovementEligibility& state) noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                // Reuse the adapter's exact-image binding. Unknown or changed
                // identity cannot authorize a Core producer or an engine write.
                if (!nativePad.IsBound())
                    return Status::Unsupported;
                NativeMovementEligibility next;
                if (!nativeMovement.ReadEligibility(next))
                    return Status::Unsupported;
                state = next;
                return Status::Ok;
            }

        private:
            static bool TrialFrame(const ControlActionFrame& frame) noexcept
            {
                constexpr auto allowed = MtaInput::ActionBit(FORWARDS) | MtaInput::ActionBit(BACKWARDS) | MtaInput::ActionBit(LEFT) |
                                         MtaInput::ActionBit(RIGHT) | MtaInput::ActionBit(SPRINT) | MtaInput::ActionBit(WALK);
                return !(frame.heldActions & ~allowed) && !frame.context.inVehicle && !frame.context.hasDetonator && !frame.context.isDead &&
                       !frame.context.aimingWeapon && !frame.context.enteringVehicle;
            }

            std::uint64_t MovementCapability() const noexcept
            {
                return nativeMovement.IsBound() && !nativeMovement.IsFrontendLease() && !nativeMovement.IsLocalLease()
                           ? static_cast<std::uint64_t>(AdapterCapability::NativeMovementWriteCandidate)
                           : 0;
            }

            void RefreshMovementCapability() noexcept
            {
                // Binding this bounded candidate must not erase independent read
                // evidence or advertise full engine/remote-player input support.
                constexpr auto bit = static_cast<std::uint64_t>(AdapterCapability::NativeMovementWriteCandidate);
                capabilities.store((capabilities.load() & ~bit) | MovementCapability());
            }

            bool EndMovement() noexcept
            {
                const bool ended = nativeMovement.End();
                RefreshMovementCapability();
                return ended;
            }

            Status Access() const noexcept
            {
                const auto thread = ownerThread.load();
                if (!thread)
                    return Status::NotInitialized;
                if (thread != GetCurrentThreadId())
                    return Status::WrongThread;
                return initialized ? Status::Ok : Status::NotInitialized;
            }

            void Clear() noexcept
            {
                controller.Reset();
                latest = LocalObservation();
                hasObservation = false;
                directionHistory = MtaInput::DirectionHistory();
                inputStorage.WriteInput(MtaInputState{});
                inputFrame = ControlActionFrame();
                hasInputState = false;
                nativePad.Reset();
                capabilities.store(BaseCapabilities);
            }

            // Native access will require a real backend binding. For this first
            // module, the Core pointer is identity only and is never dereferenced.
            CCoreInterface*              owner = nullptr;
            std::atomic<DWORD>           ownerThread{0};
            bool                         initialized = false;
            bool                         hasObservation = false;
            LocalObservation             latest;
            deonline::ControllerSnapshot controller;
            MtaInput::DirectionHistory   directionHistory;
            // Typed ownership keeps cached SDK state addresses stable and avoids
            // deleting through CRemoteDataStorage, which has no virtual destructor.
            RemoteDataStorage              inputStorage;
            ControlActionFrame             inputFrame;
            bool                           hasInputState = false;
            static constexpr std::uint64_t BaseCapabilities =
                static_cast<std::uint64_t>(AdapterCapability::ObservationIngest) | static_cast<std::uint64_t>(AdapterCapability::MtaInputSynthesis);
            std::atomic<std::uint64_t> capabilities{BaseCapabilities};
            NativePadReader            nativePad;
            NativeMovementInput        nativeMovement;
            NativeClockService         nativeClock;
            GameAdapter*               nextFrontendAdapter = nullptr;
        };

        RenderObservationState CopyRenderObservation(const NativeRenderObservationState& native) noexcept
        {
            RenderObservationState next;
            next.bound = native.bound;
            next.active = native.active;
            next.modulePinned = native.modulePinned;
            next.pendingCleanup = native.pendingCleanup;
            next.generation = native.generation;
            next.nativePresentCallbacks = native.nativePresentCallbacks;
            next.acceptedSnapshots = native.acceptedSnapshots;
            next.rejectedSnapshots = native.rejectedSnapshots;
            next.nestedCallbacks = native.nestedCallbacks;
            next.foreignThreadCallbacks = native.foreignThreadCallbacks;
            next.publicationMisses = native.publicationMisses;
            next.callbackThreadId = native.callbackThreadId;
            next.width = native.width;
            next.height = native.height;
            next.syncInterval = native.syncInterval;
            next.failureReason = native.failureReason;
            next.windowPresent = native.windowPresent;
            next.swapchainPresent = native.swapchainPresent;
            next.devicePresent = native.devicePresent;
            next.contextPresent = native.contextPresent;
            next.lastOriginalReturned = native.lastOriginalReturned;
            next.lastOriginalResult = native.lastOriginalResult;
            return next;
        }

        class GameRenderObservation final : public IGameRenderObservation
        {
        public:
            Status Begin() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                // API1 always requests its original D3D11 path; it cannot gain
                // a different route when the newer optional contract is used.
                return observation.Begin(RenderObservationRoute::D3D11) ? Status::Ok : Status::Unsupported;
            }

            Status ReadState(RenderObservationState& state) noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                NativeRenderObservationState native;
                observation.ReadState(native);
                state = CopyRenderObservation(native);
                return Status::Ok;
            }

            Status End() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.End() ? Status::Ok : Status::Unsupported;
            }

            void Release() noexcept override
            {
                // A failed detach still owns callbacks in this module. Keep the
                // lease alive until its creating thread can finish cleanup.
                if (ownerThread != GetCurrentThreadId() || !observation.End())
                    return;
                delete this;
            }

        private:
            const DWORD             ownerThread = GetCurrentThreadId();
            NativeRenderObservation observation;
        };

        class GameRenderObservationV2 final : public IGameRenderObservationV2
        {
        public:
            explicit GameRenderObservationV2(RenderObservationRoute requested) noexcept : route(requested) {}

            Status Begin() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.Begin(route) ? Status::Ok : Status::Unsupported;
            }

            Status ReadState(RenderObservationStateV2& state) noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                NativeRenderObservationState native;
                observation.ReadState(native);
                RenderObservationStateV2 next;
                next.base = CopyRenderObservation(native);
                next.requestedRoute = native.requestedRoute;
                next.observedRoute = native.observedRoute;
                next.presenceKnownMask = native.presenceKnownMask;
                next.queuePresent = native.queuePresent;
                state = next;
                return Status::Ok;
            }

            Status End() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.End() ? Status::Ok : Status::Unsupported;
            }

            void Release() noexcept override
            {
                if (ownerThread != GetCurrentThreadId() || !observation.End())
                    return;
                delete this;
            }

        private:
            const DWORD                  ownerThread = GetCurrentThreadId();
            const RenderObservationRoute route;
            NativeRenderObservation      observation;
        };
        class GameRenderObservationV3 final : public IGameRenderObservationV3
        {
        public:
            explicit GameRenderObservationV3(RenderObservationRoute requested) noexcept : route(requested) {}

            Status Begin() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.Begin(route, true) ? Status::Ok : Status::Unsupported;
            }

            Status ReadState(RenderObservationStateV3& state) noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                NativeRenderObservationState native;
                observation.ReadState(native);
                RenderObservationStateV3 next;
                next.base.base = CopyRenderObservation(native);
                next.base.requestedRoute = native.requestedRoute;
                next.base.observedRoute = native.observedRoute;
                next.base.presenceKnownMask = native.presenceKnownMask;
                next.base.queuePresent = native.queuePresent;
                next.gpu = native.gpu;
                state = next;
                return Status::Ok;
            }

            Status End() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.End() ? Status::Ok : Status::Unsupported;
            }

            void Release() noexcept override
            {
                if (ownerThread != GetCurrentThreadId() || !observation.End())
                    return;
                delete this;
            }

        private:
            const DWORD                  ownerThread = GetCurrentThreadId();
            const RenderObservationRoute route;
            NativeRenderObservation      observation;
        };
        class GameRenderObservationV4 final : public IGameRenderObservationV4
        {
        public:
            explicit GameRenderObservationV4(RenderObservationRoute requested) noexcept : route(requested) {}

            Status Begin() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.Begin(route, true, true) ? Status::Ok : Status::Unsupported;
            }

            Status ReadState(RenderObservationStateV4& state) noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                NativeRenderObservationState native;
                observation.ReadState(native);
                RenderObservationStateV4 next;
                next.base.base.base = CopyRenderObservation(native);
                next.base.base.requestedRoute = native.requestedRoute;
                next.base.base.observedRoute = native.observedRoute;
                next.base.base.presenceKnownMask = native.presenceKnownMask;
                next.base.base.queuePresent = native.queuePresent;
                next.base.gpu = native.gpu;
                next.custom = native.custom;
                state = next;
                return Status::Ok;
            }

            Status End() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.End() ? Status::Ok : Status::Unsupported;
            }

            void Release() noexcept override
            {
                if (ownerThread != GetCurrentThreadId() || !observation.End())
                    return;
                delete this;
            }

        private:
            const DWORD                  ownerThread = GetCurrentThreadId();
            const RenderObservationRoute route;
            NativeRenderObservation      observation;
        };
        class GameRenderObservationV5;
        class GameRenderObservationV6;
        SRWLOCK                  diagnosticLeaseLock = SRWLOCK_INIT;
        GameRenderObservationV5* diagnosticLeases = nullptr;
        GameRenderObservationV6* diagnosticLeasesV6 = nullptr;

        class GameRenderObservationV5 final : public IGameRenderObservationV5
        {
        public:
            explicit GameRenderObservationV5(RenderObservationRoute requested) noexcept : route(requested)
            {
                // The separate export receives an opaque lease value, so only
                // our live-object registry may authorize touching that object.
                AcquireSRWLockExclusive(&diagnosticLeaseLock);
                nextDiagnosticLease = diagnosticLeases;
                diagnosticLeases = this;
                ReleaseSRWLockExclusive(&diagnosticLeaseLock);
            }

            static Status ReadEndpointDiagnostic(IGameRenderObservationV5* lease, RenderEndpointDiagnosticV1& output) noexcept
            {
                Status result = Status::InvalidArgument;
                AcquireSRWLockShared(&diagnosticLeaseLock);
                for (auto* current = diagnosticLeases; current; current = current->nextDiagnosticLease)
                {
                    if (static_cast<IGameRenderObservationV5*>(current) != lease)
                        continue;
                    if (current->ownerThread != GetCurrentThreadId())
                        result = Status::WrongThread;
                    else
                        result = current->observation.ReadEndpointDiagnostic(output) ? Status::Ok : Status::Unsupported;
                    break;
                }
                ReleaseSRWLockShared(&diagnosticLeaseLock);
                return result;
            }

            Status Begin() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.Begin(route, true, true, true) ? Status::Ok : Status::Unsupported;
            }
            Status ReadState(RenderObservationStateV5& state) noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                NativeRenderObservationState native;
                observation.ReadState(native);
                RenderObservationStateV5 next;
                next.base.base.base.base = CopyRenderObservation(native);
                next.base.base.base.requestedRoute = native.requestedRoute;
                next.base.base.base.observedRoute = native.observedRoute;
                next.base.base.base.presenceKnownMask = native.presenceKnownMask;
                next.base.base.base.queuePresent = native.queuePresent;
                next.base.base.gpu = native.gpu;
                next.base.custom = native.custom;
                next.boundary = native.boundary;
                state = next;
                return Status::Ok;
            }
            Status End() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.End() ? Status::Ok : Status::Unsupported;
            }
            void Release() noexcept override
            {
                if (ownerThread != GetCurrentThreadId() || !observation.End())
                    return;
                // Failed cleanup retains both the native lease and registry
                // entry. Successful End alone keeps its final cache readable;
                // only Release excludes readers before deleting the object.
                AcquireSRWLockExclusive(&diagnosticLeaseLock);
                auto** current = &diagnosticLeases;
                while (*current && *current != this)
                    current = &(*current)->nextDiagnosticLease;
                if (*current)
                    *current = nextDiagnosticLease;
                ReleaseSRWLockExclusive(&diagnosticLeaseLock);
                delete this;
            }

        private:
            const DWORD                  ownerThread = GetCurrentThreadId();
            const RenderObservationRoute route;
            NativeRenderObservation      observation;
            GameRenderObservationV5*     nextDiagnosticLease = nullptr;
        };

        class GameRenderObservationV6 final : public IGameRenderObservationV6
        {
        public:
            static Status RequestDeviceLease(IGameRenderObservationV6* lease, bool window = false) noexcept
            {
                if (!TryAcquireSRWLockShared(&diagnosticLeaseLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = diagnosticLeasesV6; current; current = current->nextDiagnosticLease)
                {
                    if (static_cast<IGameRenderObservationV6*>(current) != lease)
                        continue;
                    if (current->ownerThread != GetCurrentThreadId())
                        result = Status::WrongThread;
                    else if (current->beginAttempted || current->deviceRequested || current->deviceWindowRequested)
                        result = Status::Unsupported;
                    else
                    {
                        if (window)
                            current->deviceWindowRequested = true;
                        else
                            current->deviceRequested = true;
                        result = Status::Ok;
                    }
                    break;
                }
                ReleaseSRWLockShared(&diagnosticLeaseLock);
                return result;
            }
            static Status TakeDeviceLease(IGameRenderObservationV6* lease, RenderDeviceLeaseV1& output) noexcept
            {
                if (!TryAcquireSRWLockShared(&diagnosticLeaseLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = diagnosticLeasesV6; current; current = current->nextDiagnosticLease)
                {
                    if (static_cast<IGameRenderObservationV6*>(current) != lease)
                        continue;
                    result = current->ownerThread != GetCurrentThreadId() ? Status::WrongThread : current->observation.TakeDeviceLease(output);
                    break;
                }
                ReleaseSRWLockShared(&diagnosticLeaseLock);
                return result;
            }
            static Status TakeDeviceWindowLease(IGameRenderObservationV6* lease, RenderDeviceWindowLeaseV2& output) noexcept
            {
                if (!TryAcquireSRWLockShared(&diagnosticLeaseLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = diagnosticLeasesV6; current; current = current->nextDiagnosticLease)
                {
                    if (static_cast<IGameRenderObservationV6*>(current) != lease)
                        continue;
                    result = current->ownerThread != GetCurrentThreadId() ? Status::WrongThread : current->observation.TakeDeviceWindowLease(output);
                    break;
                }
                ReleaseSRWLockShared(&diagnosticLeaseLock);
                return result;
            }
            explicit GameRenderObservationV6(RenderObservationRoute requested) noexcept : route(requested)
            {
                // A separate registry keeps exact lease validation from
                // granting either diagnostic export the other API's object.
                AcquireSRWLockExclusive(&diagnosticLeaseLock);
                nextDiagnosticLease = diagnosticLeasesV6;
                diagnosticLeasesV6 = this;
                ReleaseSRWLockExclusive(&diagnosticLeaseLock);
            }

            static Status ReadEndpointDiagnostic(IGameRenderObservationV6* lease, RenderEndpointDiagnosticV1& output) noexcept
            {
                Status result = Status::InvalidArgument;
                // A failed API6 cleanup can retain a peer stopped inside lease
                // registration. Optional diagnostics must not wait for that
                // peer's lock and prevent the owner from retrying cleanup.
                if (!TryAcquireSRWLockShared(&diagnosticLeaseLock))
                    return Status::Unsupported;
                for (auto* current = diagnosticLeasesV6; current; current = current->nextDiagnosticLease)
                {
                    if (static_cast<IGameRenderObservationV6*>(current) != lease)
                        continue;
                    if (current->ownerThread != GetCurrentThreadId())
                        result = Status::WrongThread;
                    else
                        result = current->observation.ReadEndpointDiagnostic(output) ? Status::Ok : Status::Unsupported;
                    break;
                }
                ReleaseSRWLockShared(&diagnosticLeaseLock);
                return result;
            }

            static Status ReadSetupDiagnostic(IGameRenderObservationV6* lease, RenderSetupDiagnosticV1& output) noexcept
            {
                // The same exact registry identity protects setup evidence from
                // arbitrary pointers and Release, without waiting on a stopped
                // peer that may hold registration's exclusive lock.
                if (!TryAcquireSRWLockShared(&diagnosticLeaseLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = diagnosticLeasesV6; current; current = current->nextDiagnosticLease)
                {
                    if (static_cast<IGameRenderObservationV6*>(current) != lease)
                        continue;
                    result = current->ownerThread != GetCurrentThreadId()       ? Status::WrongThread
                             : current->observation.ReadSetupDiagnostic(output) ? Status::Ok
                                                                                : Status::Unsupported;
                    break;
                }
                ReleaseSRWLockShared(&diagnosticLeaseLock);
                return result;
            }

            static Status ReadRootsDiagnostic(IGameRenderObservationV6* lease, RenderRootsDiagnosticV1& output) noexcept
            {
                // The same exact registry identity protects roots evidence from
                // arbitrary pointers and Release, without waiting on a stopped
                // peer that may hold registration's exclusive lock.
                if (!TryAcquireSRWLockShared(&diagnosticLeaseLock))
                    return Status::Unsupported;
                Status result = Status::InvalidArgument;
                for (auto* current = diagnosticLeasesV6; current; current = current->nextDiagnosticLease)
                {
                    if (static_cast<IGameRenderObservationV6*>(current) != lease)
                        continue;
                    result = current->ownerThread != GetCurrentThreadId()       ? Status::WrongThread
                             : current->observation.ReadRootsDiagnostic(output) ? Status::Ok
                                                                                : Status::Unsupported;
                    break;
                }
                ReleaseSRWLockShared(&diagnosticLeaseLock);
                return result;
            }

            Status Begin() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                const bool captureDevice = deviceRequested;
                const bool captureDeviceWindow = deviceWindowRequested;
                deviceRequested = false;
                deviceWindowRequested = false;
                beginAttempted = true;
                return observation.Begin(route, true, true, true, true, captureDevice, captureDeviceWindow) ? Status::Ok : Status::Unsupported;
            }
            Status ReadState(RenderObservationStateV6& state) noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                NativeRenderObservationState native;
                observation.ReadState(native);
                RenderObservationStateV6 next;
                next.base.base.base.base.base = CopyRenderObservation(native);
                next.base.base.base.base.requestedRoute = native.requestedRoute;
                next.base.base.base.base.observedRoute = native.observedRoute;
                next.base.base.base.base.presenceKnownMask = native.presenceKnownMask;
                next.base.base.base.base.queuePresent = native.queuePresent;
                next.base.base.base.gpu = native.gpu;
                next.base.base.custom = native.custom;
                next.base.boundary = native.boundary;
                next.nativeDispatch = native.nativeDispatch;
                state = next;
                return Status::Ok;
            }
            Status End() noexcept override
            {
                if (ownerThread != GetCurrentThreadId())
                    return Status::WrongThread;
                return observation.End() ? Status::Ok : Status::Unsupported;
            }
            void Release() noexcept override
            {
                if (ownerThread != GetCurrentThreadId() || (beginAttempted && !observation.End()))
                    return;
                // The callsite may still own a live relay after failed End.
                // Retain it and exclude diagnostic readers only on safe release.
                // A fresh allocation has no runtime generation. Its disposal
                // must not depend on a different observer's global hook state.
                AcquireSRWLockExclusive(&diagnosticLeaseLock);
                auto** current = &diagnosticLeasesV6;
                while (*current && *current != this)
                    current = &(*current)->nextDiagnosticLease;
                if (*current)
                    *current = nextDiagnosticLease;
                ReleaseSRWLockExclusive(&diagnosticLeaseLock);
                delete this;
            }

        private:
            const DWORD                  ownerThread = GetCurrentThreadId();
            const RenderObservationRoute route;
            NativeRenderObservation      observation;
            GameRenderObservationV6*     nextDiagnosticLease = nullptr;
            bool                         deviceRequested = false, deviceWindowRequested = false, beginAttempted = false;
        };
    }

    extern "C" IGameAdapter* CreateGameAdapter(std::uint32_t abiVersion) noexcept
    {
        if (abiVersion != AdapterAbiVersion)
            return nullptr;
        try
        {
            return new (std::nothrow) GameAdapter;
        }
        catch (...)
        {
            // Allocation/constructor failures cannot escape the module ABI.
            return nullptr;
        }
    }

    extern "C" Status BeginGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, const LocalInputLeaseRequestV1* request,
                                                 LocalInputLeaseStateV1* output) noexcept
    {
        if (version != LocalInputLeaseVersionV1 || !adapter || !request || !output)
            return Status::InvalidArgument;
        return GameAdapter::LocalLease(adapter, GameAdapter::LocalLeaseAction::Begin, request, 0, nullptr, output);
    }

    extern "C" Status RenewGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation, const MtaInputState* movement) noexcept
    {
        if (version != LocalInputLeaseVersionV1 || !adapter || !generation || !movement)
            return Status::InvalidArgument;
        return GameAdapter::LocalLease(adapter, GameAdapter::LocalLeaseAction::Renew, nullptr, generation, movement, nullptr);
    }

    extern "C" Status ReadGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, LocalInputLeaseStateV1* output) noexcept
    {
        if (version != LocalInputLeaseVersionV1 || !adapter || !output)
            return Status::InvalidArgument;
        return GameAdapter::LocalLease(adapter, GameAdapter::LocalLeaseAction::Read, nullptr, 0, nullptr, output);
    }

    extern "C" Status EndGameLocalInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (version != LocalInputLeaseVersionV1 || !adapter || !generation)
            return Status::InvalidArgument;
        return GameAdapter::LocalLease(adapter, GameAdapter::LocalLeaseAction::End, nullptr, generation, nullptr, nullptr);
    }

    extern "C" Status BeginGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, const FrontendInputLeaseRequestV1* request,
                                                    FrontendInputLeaseStateV1* output) noexcept
    {
        if (version != FrontendInputLeaseVersionV1 || !adapter || !request || !output)
            return Status::InvalidArgument;
        return GameAdapter::FrontendLease(adapter, GameAdapter::FrontendLeaseAction::Begin, request, 0, nullptr, output);
    }

    extern "C" Status RenewGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation,
                                                    const MtaInputState* neutral) noexcept
    {
        if (version != FrontendInputLeaseVersionV1 || !adapter || !generation || !neutral)
            return Status::InvalidArgument;
        return GameAdapter::FrontendLease(adapter, GameAdapter::FrontendLeaseAction::Renew, nullptr, generation, neutral, nullptr);
    }

    extern "C" Status ReadGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, FrontendInputLeaseStateV1* output) noexcept
    {
        if (version != FrontendInputLeaseVersionV1 || !adapter || !output)
            return Status::InvalidArgument;
        return GameAdapter::FrontendLease(adapter, GameAdapter::FrontendLeaseAction::Read, nullptr, 0, nullptr, output);
    }

    extern "C" Status EndGameFrontendInputLeaseV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (version != FrontendInputLeaseVersionV1 || !adapter || !generation)
            return Status::InvalidArgument;
        return GameAdapter::FrontendLease(adapter, GameAdapter::FrontendLeaseAction::End, nullptr, generation, nullptr, nullptr);
    }

    extern "C" Status BeginGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, const ClockServiceRequestV1* request,
                                              ClockServiceStateV1* output) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !request || !output || request->size != sizeof(*request) ||
            request->version != ClockServiceVersionV1 || !request->maximumDurationMs || request->maximumDurationMs > ClockServiceMaximumDurationMsV1 ||
            request->reserved[0] || request->reserved[1] || request->reserved[2])
            return Status::InvalidArgument;
        return GameAdapter::ClockService(adapter, GameAdapter::ClockAction::Begin, request, nullptr, 0, output);
    }

    extern "C" Status SubmitGameClockCommandV1(std::uint32_t version, IGameAdapter* adapter, const ClockCommandV1* command) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !command || command->size != sizeof(*command) || command->version != ClockServiceVersionV1 ||
            !command->generation || !command->sequence || command->reserved ||
            static_cast<std::uint32_t>(command->operation) > static_cast<std::uint32_t>(ClockOperation::ResetTimeFrozen) ||
            (command->operation == ClockOperation::Set ? command->hour > 23 || command->minute > 60 : command->hour || command->minute))
            return Status::InvalidArgument;
        return GameAdapter::ClockService(adapter, GameAdapter::ClockAction::Submit, nullptr, command, 0, nullptr);
    }

    extern "C" Status RenewGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !generation)
            return Status::InvalidArgument;
        return GameAdapter::ClockService(adapter, GameAdapter::ClockAction::Renew, nullptr, nullptr, generation, nullptr);
    }

    extern "C" Status ReadGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, ClockServiceStateV1* output) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !output)
            return Status::InvalidArgument;
        return GameAdapter::ClockService(adapter, GameAdapter::ClockAction::Read, nullptr, nullptr, 0, output);
    }

    extern "C" Status EndGameClockServiceV1(std::uint32_t version, IGameAdapter* adapter, std::uint64_t generation) noexcept
    {
        if (version != ClockServiceVersionV1 || !adapter || !generation)
            return Status::InvalidArgument;
        return GameAdapter::ClockService(adapter, GameAdapter::ClockAction::End, nullptr, nullptr, generation, nullptr);
    }

    extern "C" IGameRenderObservation* CreateGameRenderObservation(std::uint32_t apiVersion, CCoreInterface* owner) noexcept
    {
        // The opaque identity authorizes a Core-owned lease without calling
        // host APIs. Allocation alone performs no bind, hook or module pin.
        if (apiVersion != RenderObservationApiVersion || !owner)
            return nullptr;
        try
        {
            return new (std::nothrow) GameRenderObservation;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    extern "C" IGameRenderObservationV2* CreateGameRenderObservationV2(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        // Explicit selection precedes allocation, binding and pinning. A
        // missing callback never authorizes an automatic switch to another RHI.
        if (apiVersion != RenderObservationApiVersionV2 || !owner || (route != RenderObservationRoute::D3D11 && route != RenderObservationRoute::D3D12))
            return nullptr;
        try
        {
            return new (std::nothrow) GameRenderObservationV2(route);
        }
        catch (...)
        {
            return nullptr;
        }
    }
    extern "C" IGameRenderObservationV3* CreateGameRenderObservationV3(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        // Explicit selection precedes allocation, binding and pinning. A
        // missing callback never authorizes an automatic switch to another RHI.
        if (apiVersion != RenderObservationApiVersionV3 || !owner || route != RenderObservationRoute::D3D12)
            return nullptr;
        try
        {
            return new (std::nothrow) GameRenderObservationV3(route);
        }
        catch (...)
        {
            return nullptr;
        }
    }
    extern "C" IGameRenderObservationV4* CreateGameRenderObservationV4(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        // API4 adds static policy diagnostics through its own lease; it does
        // not change the older factories or authorize a custom rendering path.
        if (apiVersion != RenderObservationApiVersionV4 || !owner || route != RenderObservationRoute::D3D12)
            return nullptr;
        try
        {
            return new (std::nothrow) GameRenderObservationV4(route);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    extern "C" IGameRenderObservationV5* CreateGameRenderObservationV5(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        // A separate lease keeps all four historical ABI layouts unchanged.
        if (apiVersion != RenderObservationApiVersionV5 || !owner || route != RenderObservationRoute::D3D12)
            return nullptr;
        try
        {
            return new (std::nothrow) GameRenderObservationV5(route);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    extern "C" Status ReadGameRenderEndpointDiagnosticV1(std::uint32_t version, IGameRenderObservationV5* retainedLease,
                                                         RenderEndpointDiagnosticV1* output) noexcept
    {
        if (version != RenderEndpointDiagnosticVersionV1 || !retainedLease || !output)
            return Status::InvalidArgument;
        RenderEndpointDiagnosticV1 next;
        const auto                 result = GameRenderObservationV5::ReadEndpointDiagnostic(retainedLease, next);
        if (result == Status::Ok)
            *output = next;
        return result;
    }

    extern "C" IGameRenderObservationV6* CreateGameRenderObservationV6(std::uint32_t apiVersion, CCoreInterface* owner, RenderObservationRoute route) noexcept
    {
        // Natural callsite dispatch is a new observation, not a relaxation of
        // the existing API5 endpoint gate or its ABI layout.
        if (apiVersion != RenderObservationApiVersionV6 || !owner || route != RenderObservationRoute::D3D12)
            return nullptr;
        try
        {
            return new (std::nothrow) GameRenderObservationV6(route);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    extern "C" Status ReadGameRenderEndpointDiagnosticV2(std::uint32_t version, IGameRenderObservationV6* retainedLease,
                                                         RenderEndpointDiagnosticV1* output) noexcept
    {
        if (version != RenderEndpointDiagnosticVersionV1 || !retainedLease || !output)
            return Status::InvalidArgument;
        RenderEndpointDiagnosticV1 next;
        const auto                 result = GameRenderObservationV6::ReadEndpointDiagnostic(retainedLease, next);
        if (result == Status::Ok)
            *output = next;
        return result;
    }

    extern "C" Status ReadGameRenderSetupDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* retainedLease, RenderSetupDiagnosticV1* output) noexcept
    {
        if (version != RenderSetupDiagnosticVersionV1 || !retainedLease || !output)
            return Status::InvalidArgument;
        RenderSetupDiagnosticV1 next;
        const auto              result = GameRenderObservationV6::ReadSetupDiagnostic(retainedLease, next);
        if (result == Status::Ok)
            *output = next;
        return result;
    }

    extern "C" Status ReadGameRenderRootsDiagnosticV1(std::uint32_t version, IGameRenderObservationV6* retainedLease, RenderRootsDiagnosticV1* output) noexcept
    {
        if (version != RenderRootsDiagnosticVersionV1 || !retainedLease || !output)
            return Status::InvalidArgument;
        RenderRootsDiagnosticV1 next;
        const auto              result = GameRenderObservationV6::ReadRootsDiagnostic(retainedLease, next);
        if (result == Status::Ok)
            *output = next;
        return result;
    }
}

extern "C" mta::game_de::Status RequestGameRenderDeviceLeaseV1(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease) noexcept
{
    if (version != mta::game_de::RenderDeviceLeaseVersionV1 || !retainedLease)
        return mta::game_de::Status::InvalidArgument;
    return mta::game_de::GameRenderObservationV6::RequestDeviceLease(retainedLease);
}
extern "C" mta::game_de::Status TakeGameRenderDeviceLeaseV1(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease,
                                                            mta::game_de::RenderDeviceLeaseV1* output) noexcept
{
    if (version != mta::game_de::RenderDeviceLeaseVersionV1 || !retainedLease || !output)
        return mta::game_de::Status::InvalidArgument;
    // The record is output-only. Ordinary refusal never reads or overwrites a
    // caller sentinel; ownership moves only after exact lease/thread validation.
    mta::game_de::RenderDeviceLeaseV1 next;
    const auto                        result = mta::game_de::GameRenderObservationV6::TakeDeviceLease(retainedLease, next);
    if (result == mta::game_de::Status::Ok)
        *output = next;
    return result;
}
extern "C" mta::game_de::Status RequestGameRenderDeviceWindowLeaseV2(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease) noexcept
{
    if (version != mta::game_de::RenderDeviceWindowLeaseVersionV2 || !retainedLease)
        return mta::game_de::Status::InvalidArgument;
    return mta::game_de::GameRenderObservationV6::RequestDeviceLease(retainedLease, true);
}
extern "C" mta::game_de::Status TakeGameRenderDeviceWindowLeaseV2(std::uint32_t version, mta::game_de::IGameRenderObservationV6* retainedLease,
                                                                  mta::game_de::RenderDeviceWindowLeaseV2* output) noexcept
{
    if (version != mta::game_de::RenderDeviceWindowLeaseVersionV2 || !retainedLease || !output)
        return mta::game_de::Status::InvalidArgument;
    mta::game_de::RenderDeviceWindowLeaseV2 next;
    const auto                              result = mta::game_de::GameRenderObservationV6::TakeDeviceWindowLease(retainedLease, next);
    if (result == mta::game_de::Status::Ok)
        *output = next;
    return result;
}
