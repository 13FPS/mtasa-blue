/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Controlled DLL provider for the real Core consumer, not a game hook
 *****************************************************************************/
#include <windows.h>
#include "PresentationPolicyHandoffFixture.h"
#include <new>

// Separate .def lists export the controlled full/partial ABI without changing
// SDK declaration linkage or requiring the deliberately absent partial End.

namespace
{
    using namespace mta::game_de;
    using namespace PresentationHandoffFixture;
    Snapshot      snapshot;
    Control       control = Normal;
    std::uint64_t generation = 40, clientGeneration = 0;

    class Adapter;
    Adapter* current = nullptr;
    class Adapter final : public IGameAdapter
    {
    public:
        CCoreInterface* core = nullptr;
        DWORD           producer = 0;
        bool            initialized = false;
        Status          Initialize(CCoreInterface* owner) noexcept override
        {
            if (!owner)
                return Status::InvalidArgument;
            core = owner;
            producer = GetCurrentThreadId();
            initialized = true;
            return Status::Ok;
        }
        Status Pulse() noexcept override { return Access(); }
        Status Reset() noexcept override
        {
            ++snapshot.resets;
            snapshot.policy = {};
            snapshot.client = {};
            return Access();
        }
        Status Shutdown() noexcept override
        {
            if (Access() != Status::Ok)
                return Access();
            ++snapshot.shutdowns;
            snapshot.shutdownOrder = ++snapshot.order;
            if (snapshot.client.clientGeneration)
                return Status::Unsupported;
            snapshot.policy = {};
            initialized = false;
            return Status::Ok;
        }
        void Release() noexcept override
        {
            ++snapshot.releases;
            current = nullptr;
            delete this;
        }
        std::uint64_t GetCapabilities() const noexcept override { return 0; }
        CGame*        GetGameInterface() const noexcept override { return nullptr; }
        Status        PublishLocalObservation(const LocalObservation&) noexcept override { return Status::Unsupported; }
        Status        GetLocalObservation(LocalObservation&) noexcept override { return Status::Unsupported; }
        Status        SubmitControlActions(const ControlActionFrame&) noexcept override { return Status::Unsupported; }
        Status        GetMtaInputState(MtaInputState&) noexcept override { return Status::Unsupported; }
        Status        BindNativeInput() noexcept override { return Status::Unsupported; }
        Status        ReadNativeControllerState(std::uint32_t, NativeControllerState&) noexcept override { return Status::Unsupported; }
        Status        ReadNativeDeviceState(NativeDeviceState&) noexcept override { return Status::Unsupported; }
        Status        BeginNativeMovementTrial() noexcept override { return Status::Unsupported; }
        Status        GetNativeMovementTrialState(NativeMovementTrialState&) noexcept override { return Status::Unsupported; }
        Status        EndNativeMovementTrial() noexcept override { return Status::Ok; }
        Status        GetNativeMovementEligibility(NativeMovementEligibility&) noexcept override { return Status::Unsupported; }
        Status        Access() const noexcept
        {
            if (producer != GetCurrentThreadId())
                return Status::WrongThread;
            return initialized ? Status::Ok : Status::NotInitialized;
        }
    };
    Status Access(std::uint32_t version, IGameAdapter* adapter, CCoreInterface* core) noexcept
    {
        if (version != PresentationPolicyVersionV1 || !current || adapter != current || core != current->core)
            return Status::InvalidArgument;
        return current->Access();
    }
}

namespace mta::game_de
{
    extern "C" IGameAdapter* CreateGameAdapter(std::uint32_t version) noexcept
    {
        if (version != AdapterAbiVersion || current)
            return nullptr;
        snapshot = {};
        control = Normal;
        current = new (std::nothrow) Adapter;
        return current;
    }
    extern "C" Status BeginGamePresentationPolicyV1(std::uint32_t version, IGameAdapter* adapter, CCoreInterface* core,
                                                    PresentationPolicyStateV1* output) noexcept
    {
        const auto status = Access(version, adapter, core);
        if (status != Status::Ok)
            return status;
        ++snapshot.begins;
        snapshot.policy = {};
        snapshot.policy.generation = ++generation;
        snapshot.policy.producerThread = GetCurrentThreadId();
        snapshot.policy.hooked = snapshot.policy.active = 1;
        snapshot.policy.modulePinned = 1;
        snapshot.policy.failure = ClockResult::Ok;
        if (control == FailBegin)
        {
            snapshot.policy.active = 0;
            snapshot.policy.pendingCleanup = 1;
            snapshot.policy.failure = ClockResult::PatchFailure;
        }
        *output = snapshot.policy;
        return control == FailBegin ? Status::Unsupported : Status::Ok;
    }
    extern "C" Status BindGamePresentationClientV1(std::uint32_t version, IGameAdapter* adapter, CCoreInterface* core, std::uint64_t provider,
                                                   const void* identity, PresentationPolicyClientV1* output) noexcept
    {
        const auto status = Access(version, adapter, core);
        if (status != Status::Ok)
            return status;
        if (provider != snapshot.policy.generation || snapshot.client.clientGeneration || !identity)
            return Status::InvalidArgument;
        ++snapshot.binds;
        snapshot.client = {};
        snapshot.client.providerGeneration = provider;
        snapshot.client.clientGeneration = ++clientGeneration;
        snapshot.client.clientIdentity = reinterpret_cast<std::uintptr_t>(identity);
        snapshot.client.producerThread = GetCurrentThreadId();
        snapshot.policy.clientActive = 1;
        *output = snapshot.client;
        return control == BindReturnsStampThenFails ? Status::Unsupported : Status::Ok;
    }
    extern "C" Status DisableGamePresentationVSyncV1(std::uint32_t version, IGameAdapter* adapter, CCoreInterface* core,
                                                     const PresentationPolicyClientV1* client) noexcept
    {
        const auto status = Access(version, adapter, core);
        if (status != Status::Ok)
            return status;
        if (!client || !client->clientGeneration || !SamePresentationClient(*client, snapshot.client) || snapshot.policy.poisoned)
            return Status::InvalidArgument;
        ++snapshot.disables;
        snapshot.policy.requested = 1;
        return Status::Ok;
    }
    extern "C" Status RetireGamePresentationClientV1(std::uint32_t version, IGameAdapter* adapter, CCoreInterface* core,
                                                     const PresentationPolicyClientV1* client) noexcept
    {
        const auto status = Access(version, adapter, core);
        if (status != Status::Ok)
            return status;
        ++snapshot.retires;
        if (control == FailRetire)
            return Status::Unsupported;
        if (!client || !client->clientGeneration || !SamePresentationClient(*client, snapshot.client))
            return Status::InvalidArgument;
        snapshot.retireOrder = ++snapshot.order;
        snapshot.client = {};
        snapshot.policy.clientActive = snapshot.policy.requested = 0;
        return Status::Ok;
    }
    extern "C" Status ReadGamePresentationPolicyV1(std::uint32_t version, IGameAdapter* adapter, CCoreInterface* core,
                                                   PresentationPolicyStateV1* output) noexcept
    {
        const auto status = Access(version, adapter, core);
        if (status != Status::Ok)
            return status;
        *output = snapshot.policy;
        if (control == PoisonRead)
            output->poisoned = 1;
        if (control == UnpinnedRead)
            output->modulePinned = 0;
        if (control == MalformedPinRead)
            output->modulePinned = 2;
        return Status::Ok;
    }
#if !defined(MTA_PRESENTATION_FIXTURE_PARTIAL)
    extern "C" Status EndGamePresentationPolicyV1(std::uint32_t version, IGameAdapter* adapter, CCoreInterface* core, std::uint64_t provider) noexcept
    {
        const auto status = Access(version, adapter, core);
        if (status != Status::Ok)
            return status;
        ++snapshot.ends;
        if (control == FailEnd || (provider && provider != snapshot.policy.generation) || snapshot.client.clientGeneration)
            return Status::Unsupported;
        snapshot.policy = {};
        return Status::Ok;
    }
#endif
}
extern "C" void PresentationHandoffFixtureControlV1(PresentationHandoffFixture::Control value) noexcept
{
    control = value;
}
extern "C" void ReadPresentationHandoffFixtureV1(PresentationHandoffFixture::Snapshot* output) noexcept
{
    if (output)
        *output = snapshot;
}
extern "C" bool CreatePresentationHandoffStorageV1(mta::game_de::PresentationPolicyBorrowLifetime* output) noexcept
{
    if (!output)
        return false;
    *output = mta::game_de::PresentationPolicyBorrowLifetime::Create();
    return static_cast<bool>(*output);
}
