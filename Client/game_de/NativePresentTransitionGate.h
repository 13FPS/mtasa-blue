/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Coordinate short presentation-policy reads with native peer holds
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <atomic>
#include <cstdint>

namespace mta::game_de::NativePresentTransitionGate
{
    class Registration;
    class Read;
    class Transition;
    ClockResult RegisterOwner(std::uint64_t ownerGeneration, Registration&) noexcept;
    ClockResult UnregisterOwner(const Registration&) noexcept;
    ClockResult TryRead(const Registration&, Read&) noexcept;
    ClockResult TryAcquireTransition(std::uint64_t clockGeneration, Transition&) noexcept;
    bool        RegisteredModuleRetained() noexcept;

    // This CPU cooperation identity is not a native presentation provider or
    // SDK capability. A future actual owner must keep its own state alive until
    // successful unregister, and publish immutable token copies to its readers.
    // Its actual hook/loader module reference must outlive RegisterOwner and
    // UnregisterOwner return; the gate owns only an additional scoped ref.
    class Registration
    {
    public:
        Registration() noexcept = default;
        bool          IsSet() const noexcept { return serial != 0; }
        std::uint64_t OwnerGeneration() const noexcept { return generation; }

    private:
        friend ClockResult RegisterOwner(std::uint64_t, Registration&) noexcept;
        friend ClockResult UnregisterOwner(const Registration&) noexcept;
        friend ClockResult TryRead(const Registration&, Read&) noexcept;
        std::uint64_t      serial = 0, generation = 0;
    };

    // Only the bounded allocation-free policy decision belongs here. Release
    // this read before invoking original Present, COM, GUI, or arbitrary code;
    // the original call's duration must never hold a Clock transition hostage.
    class Read
    {
    public:
        Read() noexcept = default;
        ~Read();
        Read(const Read&) = delete;
        Read&       operator=(const Read&) = delete;
        bool        Held() const noexcept { return serial != 0; }
        ClockResult Release() noexcept;

    private:
        friend ClockResult TryRead(const Registration&, Read&) noexcept;
        std::uint64_t      serial = 0;
    };

    // No automatic release: a stopped peer or uncertain native patch can
    // outlive a local holder. Transfer the same token to retained recovery;
    // only its explicit safe completion may reopen readers/registration.
    // Mutating methods require one owning caller; Held is safe for observers.
    class Transition
    {
    public:
        Transition() noexcept = default;
        Transition(const Transition&) = delete;
        Transition&   operator=(const Transition&) = delete;
        bool          Held() const noexcept { return serial.load() != 0; }
        std::uint64_t OwnerGeneration() const noexcept { return generation.load(); }
        ClockResult   Release() noexcept;
        ClockResult   TransferTo(Transition&) noexcept;

    private:
        friend ClockResult         TryAcquireTransition(std::uint64_t, Transition&) noexcept;
        std::atomic<std::uint64_t> serial{0}, generation{0};
    };
}
