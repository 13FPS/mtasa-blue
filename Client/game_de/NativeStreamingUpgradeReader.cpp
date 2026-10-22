/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep the real reverse linked-upgrade lookup and stream bookends
 ****************************************************************************/
#include "NativeStreamingUpgradeReader.h"
#include "NativeImageAccess.h"
#include "NativeModelBankProfile.h"
#include <game/CModelRequestPolicy.h>
#include <array>
#include <cstring>
namespace mta::game_de
{
    namespace UpgradeReaderDetail
    {
        namespace P = NativeStreamingUpgradeProfile;
        using Reader = NativeStreamingUpgradeReader;
        bool Same(const Reader::Links& a, const Reader::Links& b) noexcept
        {
            return a.modelIndex == b.modelIndex && a.count == b.count && a.companionIndex == b.companionIndex && a.matchingRow == b.matchingRow &&
                   a.first == b.first && a.second == b.second;
        }
        ClockResult Capture(std::uintptr_t image, std::uint32_t model, Reader::Links& output) noexcept
        {
            Reader::Links next;
            next.modelIndex = model;
            if (!NativeImageAccess::ReadImage(image, image + P::LinkCountRva, &next.count, sizeof(next.count), true))
                return ClockResult::ReadFailure;
            // Only the physically disjoint consumed arrays are admitted. A
            // larger or negative count must not turn the native reverse loop
            // into authority over unrelated globals or a guessed allocation.
            if (next.count < 0 || next.count > static_cast<std::int32_t>(P::LinkBudget))
                return ClockResult::BindingChanged;
            const auto bytes = static_cast<std::size_t>(next.count) * sizeof(std::uint16_t);
            if (bytes && (!NativeImageAccess::ReadImage(image, image + P::FirstLinksRva, next.first.data(), bytes, true) ||
                          !NativeImageAccess::ReadImage(image, image + P::SecondLinksRva, next.second.data(), bytes, true)))
                return ClockResult::ReadFailure;
            std::int32_t after = 0;
            if (!NativeImageAccess::ReadImage(image, image + P::LinkCountRva, &after, sizeof(after), true))
                return ClockResult::ReadFailure;
            if (after != next.count)
                return ClockResult::BindingChanged;
            for (auto index = next.count; index > 0;)
            {
                --index;
                const auto row = static_cast<std::size_t>(index);
                if (next.first[row] == model || next.second[row] == model)
                {
                    // The native loop checks first before second at each
                    // reverse row and sign-extends the companion WORD. FFFF
                    // means no companion; other negative keys are refused.
                    const auto   word = next.first[row] == model ? next.second[row] : next.first[row];
                    std::int16_t companion = 0;
                    std::memcpy(&companion, &word, sizeof(companion));
                    next.companionIndex = companion;
                    next.matchingRow = index;
                    if (next.companionIndex < -1 || next.companionIndex >= static_cast<std::int32_t>(NativeModelBankProfile::ModelCount))
                        return ClockResult::BindingChanged;
                    break;
                }
            }
            output = next;
            return ClockResult::Ok;
        }
    }
    bool NativeStreamingUpgradeReader::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativeStreamingUpgradeProfile;
        if (!NativeModelLoadedReader::Revalidate(binding))
            return false;
        std::array<unsigned char, P::MaximumPinBytes> actual{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > actual.size() ||
                !NativeImageAccess::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        return true;
    }
    ClockResult NativeStreamingUpgradeReader::ReadLinks(std::uint32_t model, Links& output) const noexcept
    {
        if (!MtaGame::CModelRequestPolicy::IsUpgradeModel(model))
            return ClockResult::InvalidArgument;
        NativeModelLoadedReader        loaded(binding, owner);
        NativeModelLoadedReader::Value guard;
        auto                           result = loaded.Read(model, guard);
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::UnsupportedBuild;
        Links first, second;
        if ((result = UpgradeReaderDetail::Capture(binding.image, model, first)) != ClockResult::Ok)
            return result;
        if ((result = loaded.Recheck(guard)) != ClockResult::Ok)
            return result;
        if ((result = UpgradeReaderDetail::Capture(binding.image, model, second)) != ClockResult::Ok)
            return result;
        if (!UpgradeReaderDetail::Same(first, second) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = owner()) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativeStreamingUpgradeReader::RecheckLinks(const Links& previous) const noexcept
    {
        Links      next;
        const auto result = ReadLinks(previous.modelIndex, next);
        return result != ClockResult::Ok ? result : UpgradeReaderDetail::Same(previous, next) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
    ClockResult NativeStreamingUpgradeReader::Read(std::uint32_t model, Value& output) const noexcept
    {
        if (!MtaGame::CModelRequestPolicy::IsUpgradeModel(model))
            return ClockResult::InvalidArgument;
        NativeModelLoadedReader loaded(binding, owner);
        Value                   next;
        auto                    result = loaded.Read(model, next.selected);
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::UnsupportedBuild;
        next.streamedIn = next.selected.streamedIn;
        // Match the genuine pure leaf: a selected non-state1 row returns
        // false without reading the companion table or manufacturing absence.
        if (next.streamedIn)
        {
            if ((result = ReadLinks(model, next.links)) != ClockResult::Ok)
                return result;
            next.linksObserved = true;
            if (next.links.companionIndex >= 0)
            {
                if ((result = loaded.Read(static_cast<std::uint32_t>(next.links.companionIndex), next.companion)) != ClockResult::Ok)
                    return result;
                next.streamedIn = next.companion.streamedIn;
                if ((result = loaded.Recheck(next.companion)) != ClockResult::Ok)
                    return result;
            }
            if ((result = RecheckLinks(next.links)) != ClockResult::Ok)
                return result;
        }
        if ((result = loaded.Recheck(next.selected)) != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = owner()) != ClockResult::Ok)
            return result;
        output = next;
        return ClockResult::Ok;
    }
}
