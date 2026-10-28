/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Nonblocking policy-reader exclusion with retained transition identity
 ****************************************************************************/
#include "NativePresentTransitionGate.h"
#include <windows.h>
#include <limits>

namespace mta::game_de::NativePresentTransitionGate
{
    // A parked producer can own library locks. These paths must remain actual
    // lock-free atomics on every admitted build, rather than a library fallback.
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    namespace
    {
        constexpr std::uint64_t    registered = std::uint64_t{1} << 63;
        constexpr std::uint64_t    transition = std::uint64_t{1} << 62;
        constexpr std::uint64_t    registrationChange = std::uint64_t{1} << 61;
        constexpr std::uint64_t    readerMask = 0xffffffffu;
        std::atomic<std::uint64_t> state{0}, nextSerial{0}, ownerSerial{0}, ownerGeneration{0}, transitionSerial{0}, transitionGeneration{0};
        std::atomic<HMODULE>       moduleReference{nullptr};

        // Both control operations hold mutually exclusive state bits. Never
        // reuse an identity or wrap it into a still-valid stale owner token.
        std::uint64_t NewSerial() noexcept
        {
            const auto previous = nextSerial.load();
            if (previous == (std::numeric_limits<std::uint64_t>::max)())
                return 0;
            nextSerial.store(previous + 1);
            return previous + 1;
        }
        void ModuleAnchor() noexcept
        {
        }
        bool RetainModule() noexcept
        {
            if (moduleReference.load())
                return false;
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&ModuleAnchor), &module) || !module)
                return false;
            moduleReference.store(module);
            return true;
        }
    }

    bool RegisteredModuleRetained() noexcept
    {
        // This is only the registration's additional owned module reference,
        // never a native-provider capability or another caller's hook reference.
        return moduleReference.load() != nullptr;
    }

    ClockResult RegisterOwner(std::uint64_t generation, Registration& output) noexcept
    {
        if (!generation || output.IsSet())
            return ClockResult::InvalidArgument;
        std::uint64_t expected = 0;
        if (!state.compare_exchange_strong(expected, registrationChange))
            return ClockResult::Busy;
        // Retain only after excluding Clock transitions. A registration must not
        // enter a loader operation while that producer can already be parked.
        const auto serial = NewSerial();
        if (!serial || !RetainModule())
        {
            state.store(0);
            return serial ? ClockResult::NotInitialized : ClockResult::OwnerConflict;
        }
        ownerSerial.store(serial);
        ownerGeneration.store(generation);
        output.serial = serial;
        output.generation = generation;
        state.store(registered);
        return ClockResult::Ok;
    }

    ClockResult UnregisterOwner(const Registration& owner) noexcept
    {
        if (!owner.serial || ownerSerial.load() != owner.serial || ownerGeneration.load() != owner.generation)
            return ClockResult::OwnerConflict;
        std::uint64_t expected = registered;
        if (!state.compare_exchange_strong(expected, registrationChange))
            return ClockResult::Busy;
        // Recheck after control acquisition: another complete unregister/new
        // registration cycle must not turn a stale token into current authority.
        if (ownerSerial.load() != owner.serial || ownerGeneration.load() != owner.generation)
        {
            state.store(registered);
            return ClockResult::OwnerConflict;
        }
        // The caller still owns its actual hook/loader reference through this
        // return. Retire only our acquired ref while registrationChange keeps
        // Clock from parking this thread inside the loader's release path.
        const auto module = moduleReference.load();
        if (!module || !FreeLibrary(module))
        {
            state.store(registered);
            return ClockResult::RestoreFailure;
        }
        moduleReference.store(nullptr);
        ownerSerial.store(0);
        ownerGeneration.store(0);
        state.store(0);
        return ClockResult::Ok;
    }

    ClockResult TryRead(const Registration& owner, Read& output) noexcept
    {
        if (!owner.serial || output.Held())
            return ClockResult::InvalidArgument;
        for (unsigned attempt = 0; attempt != 16; ++attempt)
        {
            auto observed = state.load();
            if (!(observed & registered))
                return ClockResult::NotInitialized;
            if (observed & (transition | registrationChange))
                return ClockResult::Busy;
            if ((observed & readerMask) == readerMask)
                return ClockResult::Busy;
            if (!state.compare_exchange_weak(observed, observed + 1))
                continue;
            // This count prevents both unregister and transition acquisition.
            // No caller state can be borrowed until the identity bookend passes.
            if (ownerSerial.load() != owner.serial || ownerGeneration.load() != owner.generation)
            {
                state.fetch_sub(1);
                return ClockResult::OwnerConflict;
            }
            output.serial = owner.serial;
            return ClockResult::Ok;
        }
        return ClockResult::Busy;
    }

    Read::~Read()
    {
        Release();
    }
    ClockResult Read::Release() noexcept
    {
        if (!serial)
            return ClockResult::Ok;
        const auto observed = state.load();
        if (ownerSerial.load() != serial || !(observed & registered) || !(observed & readerMask) || (observed & (transition | registrationChange)))
            return ClockResult::OwnerConflict;
        state.fetch_sub(1);
        serial = 0;
        return ClockResult::Ok;
    }

    ClockResult TryAcquireTransition(std::uint64_t generation, Transition& output) noexcept
    {
        if (output.Held())
            return ClockResult::InvalidArgument;
        auto observed = state.load();
        if (observed & (transition | registrationChange | readerMask))
            return ClockResult::Busy;
        if (!state.compare_exchange_strong(observed, observed | transition))
            return ClockResult::Busy;
        const auto serial = NewSerial();
        if (!serial)
        {
            state.store(observed);
            return ClockResult::OwnerConflict;
        }
        // Even genuine absence owns this closed state. Otherwise a participant
        // could register after the absence check while Clock parks its thread.
        transitionSerial.store(serial);
        transitionGeneration.store(generation);
        output.generation.store(generation);
        output.serial.store(serial);
        return ClockResult::Ok;
    }

    ClockResult Transition::Release() noexcept
    {
        const auto owned = serial.load();
        if (!owned)
            return ClockResult::Ok;
        const auto observed = state.load();
        if (!(observed & transition) || (observed & (registrationChange | readerMask)) || transitionSerial.load() != owned ||
            transitionGeneration.load() != generation.load())
            return ClockResult::OwnerConflict;
        transitionSerial.store(0);
        transitionGeneration.store(0);
        state.store(observed & ~transition);
        serial.store(0);
        generation.store(0);
        return ClockResult::Ok;
    }

    ClockResult Transition::TransferTo(Transition& output) noexcept
    {
        const auto owned = serial.load();
        if (!owned || output.Held())
            return ClockResult::OwnerConflict;
        // Owning Clock callers serialize this handoff with their existing peer
        // gate. Publish retained ownership before dropping the local identity;
        // the global transition bit never opens between the two holders. Move
        // even a refused release identity: recovery must retain its record
        // rather than discard it merely because the global fence changed.
        output.generation.store(generation.load());
        output.serial.store(owned);
        serial.store(0);
        generation.store(0);
        return ClockResult::Ok;
    }
}
