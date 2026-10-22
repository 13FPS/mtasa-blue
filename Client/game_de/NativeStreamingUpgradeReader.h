/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe the actual paired vehicle-upgrade loaded predicate
 ****************************************************************************/
#pragma once
#include "NativeModelLoadedReader.h"
#include "NativeStreamingUpgradeProfile.h"
#include <array>
namespace mta::game_de
{
    class NativeStreamingUpgradeReader final
    {
    public:
        using Binding = NativeModelLoadedReader::Binding;
        using CheckOwner = NativeModelLoadedReader::CheckOwner;
        struct Links
        {
            std::uint32_t                                                        modelIndex = 0;
            std::int32_t                                                         count = 0, companionIndex = -1, matchingRow = -1;
            std::array<std::uint16_t, NativeStreamingUpgradeProfile::LinkBudget> first{}, second{};
        };
        struct Value
        {
            NativeModelLoadedReader::Value selected, companion;
            Links                          links;
            bool                           linksObserved = false, streamedIn = false;
        };
        NativeStreamingUpgradeReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uint32_t, Value&) const noexcept;
        ClockResult ReadLinks(std::uint32_t, Links&) const noexcept;
        ClockResult RecheckLinks(const Links&) const noexcept;
        // Only current-callback raw facts. No model object/refcount/loaded
        // render object is created, and no companion pointer is dereferenced.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
