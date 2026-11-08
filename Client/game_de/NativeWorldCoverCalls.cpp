/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Invoke real per-owner point cleanup and DoubleLink list retirement
 ****************************************************************************/
#include "NativeWorldCoverCalls.h"
#include "NativeWorldCoverProfile.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <array>
#include <cstring>

#ifndef MTA_DE_WORLD_COVER_NATIVE_INVOKE
    #define MTA_DE_WORLD_COVER_NATIVE_INVOKE &Original
#endif

namespace mta::game_de::NativeWorldCoverCalls
{
    namespace CoverP = NativeWorldCoverProfile;
    namespace
    {
        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, CoverP::MaximumPinBytes> bytes{};
            for (const auto& pin : CoverP::Pins)
            {
                if (!image || !pin.size || pin.size > bytes.size() ||
                    !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            // These are real image-owned write targets of the admitted whole
            // originals. Their contents/receiver graphs are the controller's
            // sealed borrows; an arbitrary writable external list is not one.
            return NativeImageAccess::ImagePages(image, image + CoverP::ListRootRva, sizeof(std::uintptr_t), true) &&
                   NativeImageAccess::ImagePages(image, image + CoverP::CountRva, CoverP::CountBytes, true) &&
                   NativeImageAccess::ImagePages(image, image + CoverP::FreeRefRootRva, sizeof(std::uintptr_t), true) &&
                   NativeImageAccess::ImagePages(image, image + CoverP::TableRva, CoverP::PointCapacity * CoverP::PointBytes, true) &&
                   NativeImageAccess::ImagePages(image, image + CoverP::RefBankRva, CoverP::RefCapacity * CoverP::RefBytes, true);
        }
        ClockResult Original(Operation operation, std::uintptr_t image, std::uintptr_t entity, Receipt& receipt) noexcept
        {
            using Entry = void (*)(void*);
            __try
            {
                if (operation == Operation::RemoveForEntity)
                    reinterpret_cast<Entry>(image + CoverP::RemoveRva)(reinterpret_cast<void*>(entity));
                else
                    reinterpret_cast<Entry>(image + CoverP::ClearRva)(reinterpret_cast<void*>(image + CoverP::ListRootRva));
                return ClockResult::Ok;
            }
            __except (receipt.exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
    }
    ClockResult Invoke(Operation operation, std::uintptr_t image, std::uintptr_t entity, Receipt& receipt, Invoker invoke) noexcept
    {
        receipt = {};
        if ((operation != Operation::RemoveForEntity && operation != Operation::ClearList) || (operation == Operation::RemoveForEntity && !entity))
        {
            receipt.result = ClockResult::InvalidArgument;
            return receipt.result;
        }
        if (!Profile(image))
        {
            receipt.result = ClockResult::UnsupportedBuild;
            return receipt.result;
        }
        // Keep acceptance explicit even if the original raises SEH. The test
        // seam replaces only this final body; native ownership/ACKs stay real.
        receipt.entered = true;
        receipt.result = (invoke ? invoke : MTA_DE_WORLD_COVER_NATIVE_INVOKE)(operation, image, entity, receipt);
        receipt.returned = receipt.result == ClockResult::Ok;
        return receipt.result;
    }
}
