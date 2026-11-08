/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve real native side effects and exact entered/returned edges
 ****************************************************************************/
#include "NativeWorldPoolEntryCalls.h"
#include "NativeWorldPoolMatrixProfile.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <array>
#include <cstring>
#ifndef MTA_DE_WORLD_POOL_ENTRY_NATIVE_INVOKE
    #define MTA_DE_WORLD_POOL_ENTRY_NATIVE_INVOKE &Original
#endif
namespace mta::game_de::NativeWorldPoolEntryCalls
{
    namespace P = NativeWorldPoolMatrixProfile;
    namespace
    {
        ClockResult Original(Entry entry, void* entity, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(entity);
                return ClockResult::Ok;
            }
            __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
        bool Select(Operation operation, P::Pin& pin) noexcept
        {
            switch (operation)
            {
                case Operation::RemoveMatrix:
                    pin = {P::RemoveMatrixRva, P::RemoveMatrixBytes.data(), P::RemoveMatrixBytes.size(), true};
                    return true;
                case Operation::AllocateStaticMatrix:
                    pin = {P::AllocateStaticRva, P::AllocateStaticBytes.data(), P::AllocateStaticBytes.size(), true};
                    return true;
            }
            return false;
        }
    }
    ClockResult Invoke(Operation operation, std::uintptr_t image, std::uintptr_t entity, Receipt& receipt, Invoker invoke) noexcept
    {
        receipt = {};
        P::Pin pin{};
        if (!entity || !Select(operation, pin))
        {
            receipt.result = ClockResult::InvalidArgument;
            return receipt.result;
        }
        std::array<unsigned char, P::MaximumPinBytes> bytes{};
        if (!image || !pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
            std::memcmp(bytes.data(), pin.bytes, pin.size))
        {
            receipt.result = ClockResult::UnsupportedBuild;
            return receipt.result;
        }
        // All engine mutation belongs to the genuine whole function. Keep the
        // acceptance boundary visible even when SEH catches an original fault.
        receipt.entered = true;
        receipt.result = (invoke ? invoke : MTA_DE_WORLD_POOL_ENTRY_NATIVE_INVOKE)(reinterpret_cast<Entry>(image + pin.rva), reinterpret_cast<void*>(entity),
                                                                                   receipt.exception);
        receipt.returned = receipt.result == ClockResult::Ok;
        return receipt.result;
    }
}
