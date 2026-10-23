/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native jetpack height bits with current owner/world bookends
 ****************************************************************************/
#include "NativeWorldJetpackMemory.h"
#include "NativeWorldJetpackProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <cstring>
namespace mta::game_de::NativeWorldJetpackMemory
{
    namespace
    {
        bool Profile(std::uintptr_t image) noexcept
        {
            namespace P = NativeWorldJetpackProfile;
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            for (const auto& group : P::Groups)
            {
                std::uintptr_t entry = 0;
                std::uint32_t  branch = 0;
                if (!NativeImageAccess::ReadImage(image, image + group.slotRva, &entry, sizeof(entry)) || entry != image + group.entryRva ||
                    !NativeImageAccess::ReadImage(image, image + group.tableSlotRva, &branch, sizeof(branch)) || branch != group.branchRva)
                    return false;
            }
            for (const auto& slot : P::Slots)
            {
                std::uintptr_t target = 0;
                if (!NativeImageAccess::ReadImage(image, image + P::VtableRva + slot.offset, &target, sizeof(target)) || target != image + slot.targetRva)
                    return false;
            }
            return NativeStreamingCompletionContext::Profile(image);
        }
    }
    ClockResult Read(std::uintptr_t image, float& output, CheckOwner owner, Receipt* receiptOutput) noexcept
    {
        namespace D = NativeStreamingCompletionContext;
        Receipt receipt;
        receipt.limitRva = NativeWorldJetpackProfile::LimitRva;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        auto result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        D::Value firstWorld, confirmedWorld;
        if ((result = D::Capture(image, firstWorld)) != ClockResult::Ok)
            return finish(result);
        std::uint32_t first = 0, second = 0;
        static_assert(sizeof(float) == sizeof(first));
        // The qualified consumer reads .rdata. Requiring writable pages here
        // would reject the actual supported native value; no store is admitted.
        if (!NativeImageAccess::ReadImage(image, image + receipt.limitRva, &first, sizeof(first)))
            return finish(ClockResult::ReadFailure);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = D::Capture(image, confirmedWorld)) != ClockResult::Ok)
            return finish(result);
        if (!NativeImageAccess::ReadImage(image, image + receipt.limitRva, &second, sizeof(second)))
            return finish(ClockResult::ReadFailure);
        // The common SDK returns a raw float. Compare copied bits so stable
        // NaNs and signed zero do not acquire a new clamp/equality policy.
        if (first != second || !D::Same(firstWorld, confirmedWorld, true) || !D::Fence(image, confirmedWorld) || !Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.root = confirmedWorld.root;
        receipt.gameMode = confirmedWorld.gameMode;
        receipt.level = confirmedWorld.level;
        receipt.world = confirmedWorld.world;
        std::memcpy(&receipt.value, &second, sizeof(second));
        receipt.read = true;
        std::memcpy(&output, &second, sizeof(second));
        return finish(ClockResult::Ok);
    }
}
