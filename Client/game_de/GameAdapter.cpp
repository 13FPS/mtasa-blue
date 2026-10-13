/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core-owned DE adapter lifetime and checked local-observation storage
 *****************************************************************************/

#include <game/CGameAdapter.h>
#include "ControllerSnapshot.h"
#include "NativePadReader.h"
#include "NativeMovementInput.h"

#include <atomic>
#include <cmath>
#include <new>
#include <windows.h>

namespace mta::game_de
{
    namespace
    {
        bool Finite(const AdapterVector3& value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        class GameAdapter final : public IGameAdapter
        {
        public:
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
                if (!EndMovement())
                    return Status::Unsupported;
                Clear();
                return Status::Ok;
            }

            Status Shutdown() noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                if (!EndMovement())
                    return Status::Unsupported;
                Clear();
                initialized = false;
                return Status::Ok;
            }

            void Release() noexcept override
            {
                // A live detour still references this module's state. A caller
                // cannot turn failed cleanup into a dangling callback by Release.
                if ((ownerThread.load() && ownerThread.load() != GetCurrentThreadId()) || !EndMovement())
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
                next.previous = inputState.current;
                next.current = MtaInput::BuildControllerState(frame.heldActions, frame.context, nextHistory);
                if (nativeMovement.IsActive() && !nativeMovement.Update(next))
                {
                    EndMovement();
                    return Status::Unsupported;
                }
                directionHistory = nextHistory;
                inputState = next;
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
                state = inputState;
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
                if (!hasInputState)
                    return Status::NotInitialized;
                if (!TrialFrame(inputFrame))
                    return Status::InvalidArgument;
                const bool started = nativeMovement.Begin(inputState);
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
                return nativeMovement.IsBound() ? static_cast<std::uint64_t>(AdapterCapability::NativeMovementWriteCandidate) : 0;
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
                inputState = MtaInputState();
                inputFrame = ControlActionFrame();
                hasInputState = false;
                nativePad.Reset();
                capabilities.store(BaseCapabilities);
            }

            // Native access will require a real backend binding. For this first
            // module, the Core pointer is identity only and is never dereferenced.
            CCoreInterface*                owner = nullptr;
            std::atomic<DWORD>             ownerThread{0};
            bool                           initialized = false;
            bool                           hasObservation = false;
            LocalObservation               latest;
            deonline::ControllerSnapshot   controller;
            MtaInput::DirectionHistory     directionHistory;
            MtaInputState                  inputState;
            ControlActionFrame             inputFrame;
            bool                           hasInputState = false;
            static constexpr std::uint64_t BaseCapabilities =
                static_cast<std::uint64_t>(AdapterCapability::ObservationIngest) | static_cast<std::uint64_t>(AdapterCapability::MtaInputSynthesis);
            std::atomic<std::uint64_t> capabilities{BaseCapabilities};
            NativePadReader            nativePad;
            NativeMovementInput        nativeMovement;
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
}
