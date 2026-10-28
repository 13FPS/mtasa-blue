/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bind actual CGame presentation intent to the installed native owner
 *****************************************************************************/
#include "NativePresentationPolicyOwner.h"
#include "NativePresentOwner.h"
#include <atomic>
#include <limits>
#include <windows.h>

namespace mta::game_de
{
    namespace PresentationPolicyDetail
    {
        // Callback-visible storage is process/module owned. A CGame identity is
        // an opaque stamp only: Present never reads the object or its Core slot.
        struct Runtime
        {
            std::atomic<std::uint64_t>  generation{0}, clientSerial{0}, clientGeneration{0}, clientIdentity{0};
            std::atomic<std::uintptr_t> adapter{0}, core{0};
            std::atomic<std::uint32_t>  producer{0};
            std::atomic<bool>           clientActive{false}, requested{false};
        };
        Runtime runtime;
        static_assert(std::atomic<std::uint64_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);

        bool Intent(std::uint64_t generation) noexcept
        {
            if (!generation || runtime.generation.load() != generation || !runtime.clientActive.load())
                return false;
            const auto client = runtime.clientGeneration.load();
            return client && runtime.requested.load() && runtime.clientActive.load() && runtime.clientGeneration.load() == client &&
                   runtime.generation.load() == generation;
        }
        bool Current(std::uint64_t generation, const void* adapter, CCoreInterface* core, std::uint32_t producer) noexcept
        {
            return generation && runtime.generation.load() == generation && runtime.adapter.load() == reinterpret_cast<std::uintptr_t>(adapter) &&
                   runtime.core.load() == reinterpret_cast<std::uintptr_t>(core) && runtime.producer.load() == producer;
        }
        bool Client(const PresentationPolicyClientV1& client, std::uint64_t generation, const void* adapter, CCoreInterface* core,
                    std::uint32_t producer) noexcept
        {
            return client.size == sizeof(client) && client.version == PresentationPolicyVersionV1 && !client.reserved &&
                   Current(generation, adapter, core, producer) && client.providerGeneration == generation && client.producerThread == producer &&
                   client.clientGeneration && client.clientGeneration == runtime.clientGeneration.load() && client.clientIdentity &&
                   client.clientIdentity == runtime.clientIdentity.load();
        }
        void FillState(PresentationPolicyStateV1& output, std::uint64_t generation) noexcept
        {
            output = {};
            output.generation = generation;
            if (!generation || runtime.generation.load() != generation)
                return;
            const auto state = NativePresentOwner::ReadPolicyState();
            if (state.generation != generation)
            {
                output.failure = ClockResult::BindingChanged;
                return;
            }
            output.producerThread = runtime.producer.load();
            output.hooked = state.hooked;
            output.active = state.active;
            output.retiring = state.retiring;
            output.requested = runtime.requested.load();
            output.clientActive = runtime.clientActive.load();
            output.poisoned = state.poisoned;
            output.pendingCleanup = state.pendingCleanup;
            output.modulePinned = state.modulePinned;
            output.failure = state.poisoned ? ClockResult::BindingChanged : state.failure;
            output.callbacks = state.callbacks;
            output.applied = state.applied;
            output.rejected = state.rejected;
        }
    }

    ClockResult NativePresentationPolicyOwner::BindOwner(CCoreInterface* owner, std::uint32_t producerThread, const void* adapterIdentity) noexcept
    {
        if (!owner || !adapterIdentity || !producerThread)
            return ClockResult::InvalidArgument;
        if (GetCurrentThreadId() != producerThread)
            return ClockResult::WrongThread;
        if (generation || (core && (core != owner || adapter != adapterIdentity || producer != producerThread)))
            return ClockResult::OwnerConflict;
        core = owner;
        adapter = adapterIdentity;
        producer = producerThread;
        return ClockResult::Ok;
    }
    ClockResult NativePresentationPolicyOwner::Access() const noexcept
    {
        if (!core || !adapter || !producer)
            return ClockResult::NotInitialized;
        return GetCurrentThreadId() == producer ? ClockResult::Ok : ClockResult::WrongThread;
    }
    ClockResult NativePresentationPolicyOwner::Begin(PresentationPolicyStateV1& output) noexcept
    {
        output = {};
        const auto access = Access();
        if (access != ClockResult::Ok)
            return access;
        NativePresentOwner::Operation operation;
        if (!operation.acquired)
            return ClockResult::Busy;
        if (generation)
        {
            if (!PresentationPolicyDetail::Current(generation, adapter, core, producer))
                return ClockResult::OwnerConflict;
            PresentationPolicyDetail::FillState(output, generation);
            return output.active && !output.retiring && !output.pendingCleanup ? ClockResult::Ok : ClockResult::Busy;
        }
        if (PresentationPolicyDetail::runtime.generation.load())
            return ClockResult::Busy;
        std::uint64_t admitted = 0;
        const auto    result = NativePresentOwner::BeginPolicy(operation, &PresentationPolicyDetail::Intent, admitted);
        if (admitted)
        {
            auto& runtime = PresentationPolicyDetail::runtime;
            runtime.clientActive.store(false);
            runtime.requested.store(false);
            runtime.clientGeneration.store(0);
            runtime.clientIdentity.store(0);
            runtime.adapter.store(reinterpret_cast<std::uintptr_t>(adapter));
            runtime.core.store(reinterpret_cast<std::uintptr_t>(core));
            runtime.producer.store(producer);
            generation = admitted;
            // Publish only after the exact owner tuple is installed. An early
            // original callback sees no intent, including failed Begin debt.
            runtime.generation.store(admitted);
        }
        PresentationPolicyDetail::FillState(output, generation);
        return result;
    }
    ClockResult NativePresentationPolicyOwner::BindClient(std::uint64_t providerGeneration, const void* clientIdentity,
                                                          PresentationPolicyClientV1& output) noexcept
    {
        output = {};
        const auto access = Access();
        if (access != ClockResult::Ok)
            return access;
        NativePresentOwner::Operation operation;
        if (!operation.acquired)
            return ClockResult::Busy;
        if (!clientIdentity || providerGeneration != generation || !PresentationPolicyDetail::Current(generation, adapter, core, producer))
            return ClockResult::OwnerConflict;
        const auto state = NativePresentOwner::ReadPolicyState();
        auto&      runtime = PresentationPolicyDetail::runtime;
        if (state.generation != generation || !state.active || state.retiring || state.poisoned || !state.hooked || !state.modulePinned ||
            runtime.clientActive.load())
            return ClockResult::Busy;
        const auto serial = runtime.clientSerial.load();
        if (serial == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;
        runtime.clientSerial.store(serial + 1);
        runtime.requested.store(false);
        runtime.clientIdentity.store(reinterpret_cast<std::uintptr_t>(clientIdentity));
        runtime.clientGeneration.store(serial + 1);
        runtime.clientActive.store(true);
        output.providerGeneration = generation;
        output.clientGeneration = serial + 1;
        output.clientIdentity = reinterpret_cast<std::uintptr_t>(clientIdentity);
        output.producerThread = producer;
        return ClockResult::Ok;
    }
    ClockResult NativePresentationPolicyOwner::DisableVSync(const PresentationPolicyClientV1& client) noexcept
    {
        const auto access = Access();
        if (access != ClockResult::Ok)
            return access;
        if (!PresentationPolicyDetail::Client(client, generation, adapter, core, producer) || !PresentationPolicyDetail::runtime.clientActive.load())
            return ClockResult::OwnerConflict;
        const auto state = NativePresentOwner::ReadPolicyState();
        if (state.generation != generation || !state.active || state.retiring || state.poisoned || !state.hooked || !state.modulePinned)
            return ClockResult::BindingChanged;
        // The SDK operation is one-way intent. Clock can defer its native
        // application, so this path never attaches/detaches or waits on Present.
        PresentationPolicyDetail::runtime.requested.store(true);
        return ClockResult::Ok;
    }
    ClockResult NativePresentationPolicyOwner::RetireClient(const PresentationPolicyClientV1& client) noexcept
    {
        const auto access = Access();
        if (access != ClockResult::Ok)
            return access;
        if (!PresentationPolicyDetail::Client(client, generation, adapter, core, producer))
            return ClockResult::OwnerConflict;
        PresentationPolicyDetail::runtime.requested.store(false);
        PresentationPolicyDetail::runtime.clientActive.store(false);
        return ClockResult::Ok;
    }
    ClockResult NativePresentationPolicyOwner::RetireAll() noexcept
    {
        if (!generation)
            return ClockResult::Ok;
        const auto access = Access();
        if (access != ClockResult::Ok)
            return access;
        if (!PresentationPolicyDetail::Current(generation, adapter, core, producer))
            return ClockResult::OwnerConflict;
        PresentationPolicyDetail::runtime.requested.store(false);
        PresentationPolicyDetail::runtime.clientActive.store(false);
        return NativePresentOwner::RetirePolicy(generation);
    }
    ClockResult NativePresentationPolicyOwner::End(std::uint64_t providerGeneration) noexcept
    {
        if (!generation)
            return ClockResult::Ok;
        if (providerGeneration && providerGeneration != generation)
            return ClockResult::OwnerConflict;
        const auto retired = RetireAll();
        if (retired != ClockResult::Ok)
            return retired;
        NativePresentOwner::Operation operation;
        if (!operation.acquired)
            return ClockResult::Busy;
        const auto result = NativePresentOwner::EndPolicy(operation, generation);
        if (result != ClockResult::Ok)
            return result;
        auto& runtime = PresentationPolicyDetail::runtime;
        runtime.generation.store(0);
        runtime.adapter.store(0);
        runtime.core.store(0);
        runtime.producer.store(0);
        generation = 0;
        return ClockResult::Ok;
    }
    ClockResult NativePresentationPolicyOwner::ReadState(PresentationPolicyStateV1& output) const noexcept
    {
        const auto access = Access();
        if (access != ClockResult::Ok)
        {
            output = {};
            return access;
        }
        if (generation && !PresentationPolicyDetail::Current(generation, adapter, core, producer))
            return ClockResult::OwnerConflict;
        PresentationPolicyDetail::FillState(output, generation);
        return ClockResult::Ok;
    }
    bool NativePresentationPolicyOwner::Pending() const noexcept
    {
        return generation != 0;
    }
}
