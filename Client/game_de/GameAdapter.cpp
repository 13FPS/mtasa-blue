/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Core-owned DE adapter lifetime and checked local-observation storage
 *****************************************************************************/

#include <game/CGameAdapter.h>
#include "ControllerSnapshot.h"
#include "NativePadReader.h"

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
                Clear();
                return Status::Ok;
            }

            Status Shutdown() noexcept override
            {
                const auto status = Access();
                if (status != Status::Ok)
                    return status;
                Clear();
                initialized = false;
                return Status::Ok;
            }

            void Release() noexcept override { delete this; }

            std::uint64_t GetCapabilities() const noexcept override
            {
                // Native read availability is added only after an explicit
                // build/image binding; synthesis alone cannot establish it.
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
                if (frame.heldActions & ~MtaInput::KnownActionMask)
                    return Status::InvalidArgument;

                // Keep logical MTA inputs separate from native evidence, whose
                // twenty-field mask cannot describe MTA's walk/radio fields.
                const auto current = MtaInput::BuildControllerState(frame.heldActions, frame.context, directionHistory);
                inputState.previous = inputState.current;
                inputState.current = current;
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
                capabilities.store(BaseCapabilities | (bound ? static_cast<std::uint64_t>(AdapterCapability::EngineInputRead) : 0));
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
                    capabilities.store(BaseCapabilities);
                    return Status::Unsupported;
                }
                state = next;
                return Status::Ok;
            }

        private:
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
            bool                           hasInputState = false;
            static constexpr std::uint64_t BaseCapabilities =
                static_cast<std::uint64_t>(AdapterCapability::ObservationIngest) | static_cast<std::uint64_t>(AdapterCapability::MtaInputSynthesis);
            std::atomic<std::uint64_t> capabilities{BaseCapabilities};
            NativePadReader            nativePad;
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
