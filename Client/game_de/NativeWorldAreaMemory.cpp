/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep all current-area bits with current owner/world bookends
 ****************************************************************************/
#include "NativeWorldAreaMemory.h"
#include "NativeWorldAreaProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <cstring>
namespace mta::game_de::NativeWorldAreaMemory
{
    namespace
    {
        bool Profile(std::uintptr_t image) noexcept
        {
            namespace P = NativeWorldAreaProfile;
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
                unsigned char  selector = 0;
                if (!NativeImageAccess::ReadImage(image, image + group.slotRva, &entry, sizeof(entry)) || entry != image + group.entryRva ||
                    !NativeImageAccess::ReadImage(image, image + group.tableSlotRva, &branch, sizeof(branch)) || branch != group.branchRva ||
                    (group.remapRva &&
                     (!NativeImageAccess::ReadImage(image, image + group.remapRva, &selector, sizeof(selector)) || selector != group.selector)))
                    return false;
            }
            return NativeStreamingCompletionContext::Profile(image);
        }
    }
    ClockResult Read(std::uintptr_t image, std::uint32_t& output, CheckOwner owner, Receipt* receiptOutput) noexcept
    {
        namespace D = NativeStreamingCompletionContext;
        Receipt receipt;
        receipt.areaRva = NativeWorldAreaProfile::AreaRva;
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
        if (!NativeImageAccess::ReadImage(image, image + receipt.areaRva, &first, sizeof(first), true))
            return finish(ClockResult::ReadFailure);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = D::Capture(image, confirmedWorld)) != ClockResult::Ok)
            return finish(result);
        if (!NativeImageAccess::ReadImage(image, image + receipt.areaRva, &second, sizeof(second), true))
            return finish(ClockResult::ReadFailure);
        // Native077E and04BB establish a full DWORD. Do not truncate to the
        // low byte used by one streaming consumer or substitute a ped's area.
        // The value is an owned snapshot, with no lifetime lease or setter.
        if (first != second || !D::Same(firstWorld, confirmedWorld, true) || !D::Fence(image, confirmedWorld) || !Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.root = confirmedWorld.root;
        receipt.gameMode = confirmedWorld.gameMode;
        receipt.level = confirmedWorld.level;
        receipt.world = confirmedWorld.world;
        receipt.value = second;
        receipt.read = true;
        output = second;
        return finish(ClockResult::Ok);
    }
}
