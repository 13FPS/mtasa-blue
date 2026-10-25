/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One borrowed native epoch for prepared-array owner transactions
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceRowCalls.h"
namespace mta::game_de::NativeAnimationSequenceDependencyCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using CheckOwner = NativeAnimationSequenceRowCalls::CheckOwner;
    using Row = NativeAnimationSequenceRowCalls::Value;
    class Epoch final
    {
    public:
        Epoch(const Epoch&) = delete;
        Epoch&         operator=(const Epoch&) = delete;
        std::uintptr_t Image() const noexcept { return image; }
        std::uint64_t  Generation() const noexcept { return generation; }
        std::uintptr_t ArrayTag() const noexcept { return arrayTag; }
        std::size_t    Capacity() const noexcept { return capacity; }

    private:
        friend struct ::mta::game_de::NativeCustomAnimSequenceStorageAccess;
        Epoch(std::uintptr_t image, std::uint64_t generation, std::uintptr_t arrayTag, std::size_t capacity) noexcept
            : image(image), generation(generation), arrayTag(arrayTag), capacity(capacity)
        {
        }
        std::uintptr_t image;
        std::uint64_t  generation;
        std::uintptr_t arrayTag;
        std::size_t    capacity;
    };
    using ResolveFrame = ClockResult (*)(std::size_t, const Row&, const Storage*&, void*) noexcept;
    using Action = ClockResult (*)(Storage&, const Epoch&, void*) noexcept;
    // ResolveFrame supplies actual existing issued frame records. The native
    // helper independently qualifies their private origin/heap, never payload.
    // Action runs inside this one shared gate and borrows Epoch only until
    // return. Ok alone grants no parent attach/detach or cleanup permission.
    ClockResult WithPrepared(std::uintptr_t image, Storage&, std::uint64_t generation, CheckOwner, ResolveFrame, void* resolveContext, Action,
                             void* actionContext) noexcept;
    // A pre-entry cancellation can qualify the original array without walking
    // malformed row children. Only the matching parent command may request it.
    ClockResult WithArray(std::uintptr_t image, Storage&, std::uint64_t generation, CheckOwner, Action, void* actionContext) noexcept;
}
