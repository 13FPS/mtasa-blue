/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: MTA-owned lifecycle and observation contract for another game backend
 *****************************************************************************/

#pragma once

#include <CControllerState.h>
#include <CControlStateSynthesis.h>
#include <array>
#include <cstddef>
#include <cstdint>

class CCoreInterface;
class CGame;

namespace mta::game_de
{
    // ABI6 adds build-pinned eligibility for Core's configured movement trial. Earlier consumers
    // must not receive this changed vtable even though their methods remain.
    inline constexpr std::uint32_t AdapterAbiVersion = 6;
    inline constexpr std::uint32_t AdapterControllerMask = 0xfffffu;

    enum class AdapterCapability : std::uint64_t
    {
        ObservationIngest = 1,
        GameInterface = 2,
        EngineInputRead = 4,
        EngineInputWrite = 8,
        RemotePlayerContext = 16,
        MtaInputSynthesis = 32,
        NativePadCandidateRead = 64,
        NativeDeviceStateCandidateRead = 128,
        NativeMovementWriteCandidate = 256
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

    inline constexpr std::size_t NativeDeviceStateCount = 127;

    // These source values belong to a build-pinned native table. No physical
    // device, held-action or previous-frame interpretation is established here.
    struct NativeDeviceState
    {
        std::array<std::int32_t, NativeDeviceStateCount> sources{};
    };

    // Trial failures are diagnostic outcomes. None does not establish complete
    // engine input or remote-player support, which retain separate capabilities.
    enum class NativeMovementTrialFailure : std::uint32_t
    {
        None = 0,
        InvalidState = 1,
        WrongThread = 2,
        UnsupportedBuild = 3,
        BindingChanged = 4,
        NestedCallback = 5,
        Vehicle = 6,
        Expired = 7,
        StaleInput = 8,
        ReadFailure = 9,
        WriteFailure = 10,
        RestoreFailure = 11,
        HookInstall = 12,
        HookRemove = 13,
        ContextChanged = 14,
        ScopeActive = 15,
        OwnerConflict = 16,
        OriginalException = 17,
        IneligibleContext = 18
    };

    struct NativeMovementTrialState
    {
        bool          bound = false;
        bool          active = false;
        std::uint64_t callbackCount = 0;
        std::uint64_t scopedSteps = 0;
        std::uint64_t routedQueries = 0;
        std::uint64_t neutralQueries = 0;
        std::uint64_t restoredSteps = 0;
        std::uint64_t restoreFailures = 0;
        std::uint64_t foreignThreadCallbacks = 0;
        std::uint64_t nestedCallbacks = 0;
        std::uint32_t failureReason = 0;
        // This is the observed native field after an original control callback;
        // it remains a raw value rather than a validated locomotion mapping.
        std::uint32_t lastMoveState = 0;
        // Pinning succeeds before hook installation can fail. Report its real
        // lifetime independently of active/bound so Core can qualify cleanup.
        bool modulePinned = false;
    };

    enum class NativeMovementEligibilityFlag : std::uint32_t
    {
        LocalIdentity = 1,
        OnFoot = 2,
        MovementState = 4,
        ControlsEnabled = 8
    };
    inline constexpr std::uint32_t NativeMovementEligibilityMask = 0xf;

    // Known fields describe the exact local-player identity, native InVehicle
    // flag, PED_IDLE state and controls-disable word. This deliberately admits
    // only the existing six movement actions; weapon/task context remains
    // unknown and must not be inferred from the other Context defaults.
    struct NativeMovementEligibility
    {
        std::uint32_t     knownMask = 0;
        std::uint32_t     flags = 0;
        std::uint32_t     rawPedState = 0;
        std::uint32_t     rawDisabledControls = 0;
        bool              eligible = false;
        MtaInput::Context context;
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
        virtual Status        ReadNativeDeviceState(NativeDeviceState& state) noexcept = 0;
        virtual Status        BeginNativeMovementTrial() noexcept = 0;
        virtual Status        GetNativeMovementTrialState(NativeMovementTrialState& state) noexcept = 0;
        virtual Status        EndNativeMovementTrial() noexcept = 0;
        virtual Status        GetNativeMovementEligibility(NativeMovementEligibility& state) noexcept = 0;

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
