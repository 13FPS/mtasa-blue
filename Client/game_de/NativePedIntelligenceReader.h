/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the real occupied intelligence/embedded manager identity
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"
#include <array>

namespace mta::game_de
{
    class NativePedIntelligenceReader final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        using PedValue = NativePedPoolReader::Value;
        struct Value
        {
            PedValue                      ped;
            std::uintptr_t                pool = 0, objects = 0, flags = 0, nativeIntelligence = 0, taskManager = 0;
            std::array<std::uintptr_t, 5> primaryTasks{};
            std::array<std::uintptr_t, 6> secondaryTasks{};
            std::uint32_t                 slot = 0;
            std::int32_t                  capacity = 0, cursor = -1;
            unsigned char                 generation = 0, ownsAllocations = 0, rawType = 0;
        };
        NativePedIntelligenceReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(const PedValue&, Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;

        // Values are immediate observations under the caller's existing native
        // callback, not retained pointer leases or fabricated SDK services.
        // Task addresses are opaque: this reader never dereferences or calls
        // them. Native deletion can keep a task in its manager cell until the
        // destructor returns; even an unchanged cell proves no task lifetime.
        // No native tasks/AI/registries are created, reset, freed or mutated.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
