/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared current owner/world admission for both genuine completion paths
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include "NativeUObjectMembershipAccess.h"
#include <array>
#include <limits>
namespace mta::game_de::NativeStreamingCompletionContext
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Value
    {
        std::uintptr_t                                          root = 0, gameMode = 0, level = 0, world = 0, gameClass = 0, levelClass = 0;
        unsigned char                                           mode = 0;
        std::array<NativeUObjectMembershipAccess::Snapshot, 64> members{};
        std::size_t                                             count = 0;
    };
    // One shared scope rejects reentry between full and selective loading;
    // neither callback is allowed to borrow the other's live admission.
    bool TryEnter() noexcept;
    void Leave() noexcept;
    struct Scope
    {
        ~Scope() { Leave(); }
    };
    bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept;
    bool Writable(std::uintptr_t address, std::size_t bytes) noexcept;
    template <class T>
    bool At(std::uintptr_t address, std::size_t offset, T& value) noexcept
    {
        return address && offset <= (std::numeric_limits<std::uintptr_t>::max)() - address && Copy(address + offset, &value, sizeof(value));
    }
    bool        Profile(std::uintptr_t image) noexcept;
    ClockResult Owner(std::uintptr_t image, CheckOwner) noexcept;
    ClockResult Capture(std::uintptr_t image, Value&) noexcept;
    // Extend the same stack-only graph for a genuine completion entry's
    // independently qualified immediate dependencies; no extra owner is made.
    bool Add(std::uintptr_t image, std::uintptr_t object, Value&) noexcept;
    bool Fence(std::uintptr_t image, const Value&) noexcept;
    bool Same(const Value&, const Value&, bool sameMode) noexcept;
}
