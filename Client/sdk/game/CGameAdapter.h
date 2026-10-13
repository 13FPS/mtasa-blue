/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: MTA-owned lifecycle and observation contract for another game backend
 *****************************************************************************/

#pragma once

#include <CControllerState.h>
#include <CControlStateSynthesis.h>
#include <cstdint>

class CCoreInterface;
class CGame;

namespace mta::game_de
{
    // ABI3 adds explicit native input binding/read virtuals. Earlier consumers
    // must not receive this changed vtable even though their methods remain.
    inline constexpr std::uint32_t AdapterAbiVersion = 3;
    inline constexpr std::uint32_t AdapterControllerMask = 0xfffffu;

    enum class AdapterCapability : std::uint64_t
    {
        ObservationIngest = 1,
        GameInterface = 2,
        EngineInputRead = 4,
        EngineInputWrite = 8,
        RemotePlayerContext = 16,
        MtaInputSynthesis = 32,
        NativePadCandidateRead = 64
    };

    // The first live trial contradicted the SDK controller layout: movement
    // axes/sprint stayed zero while unrelated-looking fields changed. Reading
    // this address is diagnostic evidence, not a verified engine input binding.
    inline constexpr std::uint64_t NativePadBindingCapabilities = static_cast<std::uint64_t>(AdapterCapability::NativePadCandidateRead);

    enum class Status : std::uint32_t
    {
        Ok,
        InvalidArgument,
        WrongThread,
        NotInitialized,
        Unsupported
    };

    struct AdapterVector3
    {
        float x = 0;
        float y = 0;
        float z = 0;
    };

    struct AdapterTransform
    {
        AdapterVector3 position;
        float          heading = 0;
    };

    // These are caller observations, not engine object layouts or native input
    // guarantees. A zero coverage mask explicitly means no controller evidence;
    // the current storage accepts either no controls or all twenty typed fields.
    // Velocity retains the source's raw units, and sampleAtMs is its own clock.
    struct LocalObservation
    {
        std::uint64_t    sampleAtMs = 0;
        std::uint32_t    localPedHandle = 0;
        AdapterTransform transform;
        AdapterVector3   rawVelocity;
        CControllerState controller;
        std::uint32_t    controllerKnownMask = 0;
    };

    // These actions come from MTA's configured bind state, after its context
    // transitions. They are not raw OS keys or observations of an engine pad.
    // Joystick overrides remain a separate Core policy after digital synthesis.
    struct ControlActionFrame
    {
        std::uint64_t     heldActions = 0;
        MtaInput::Context context;
    };

    struct MtaInputState
    {
        CControllerState current;
        CControllerState previous;
    };

    // Raw candidate bytes copied at the public SDK's proposed state offsets.
    // The live trial did not validate that layout. Retain this representation
    // for diagnosis without treating it as engine controls or applying it back.
    struct NativeControllerState
    {
        CControllerState current;
        CControllerState previous;
    };

    class IGameAdapter
    {
    public:
        // Core owns this lifetime. Initialize records its calling thread and
        // retains only the opaque owner identity; it does not call classic APIs.
        virtual Status Initialize(CCoreInterface* owner) noexcept = 0;
        virtual Status Pulse() noexcept = 0;
        virtual Status Reset() noexcept = 0;
        virtual Status Shutdown() noexcept = 0;
        virtual void   Release() noexcept = 0;

        virtual std::uint64_t GetCapabilities() const noexcept = 0;
        virtual CGame*        GetGameInterface() const noexcept = 0;
        virtual Status        PublishLocalObservation(const LocalObservation& observation) noexcept = 0;
        virtual Status        GetLocalObservation(LocalObservation& observation) noexcept = 0;
        virtual Status        SubmitControlActions(const ControlActionFrame& frame) noexcept = 0;
        virtual Status        GetMtaInputState(MtaInputState& state) noexcept = 0;
        virtual Status        BindNativeInput() noexcept = 0;
        virtual Status        ReadNativeControllerState(std::uint32_t padIndex, NativeControllerState& state) noexcept = 0;

    protected:
        // Release keeps allocation/deallocation in the factory's module. The
        // caller must finish all calls before releasing this unique ownership.
        virtual ~IGameAdapter() = default;
    };

#if defined(_WIN32) && defined(MTA_GAME_DE_EXPORTS)
    extern "C" __declspec(dllexport) IGameAdapter* CreateGameAdapter(std::uint32_t abiVersion) noexcept;
#else
    extern "C" IGameAdapter* CreateGameAdapter(std::uint32_t abiVersion) noexcept;
#endif
}
