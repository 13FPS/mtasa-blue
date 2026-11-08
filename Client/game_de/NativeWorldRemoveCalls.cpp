/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Call the complete World Remove without destructor substitution
 ****************************************************************************/
#include "NativeWorldRemoveCalls.h"
#include "NativeWorldPoolMatrixProfile.h"
#include <windows.h>
#ifndef MTA_DE_WORLD_REMOVE_NATIVE_INVOKE
    #define MTA_DE_WORLD_REMOVE_NATIVE_INVOKE &Original
#endif
namespace mta::game_de::NativeWorldRemoveCalls
{
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
    }
    ClockResult Invoke(std::uintptr_t image, const NativeWorldRemoveMemory::Value& value, Receipt& receipt, Invoker invoke) noexcept
    {
        receipt = {};
        if (!NativeWorldRemoveMemory::Seal(image, value))
            return receipt.result = ClockResult::BindingChanged;
        const auto entry = reinterpret_cast<Entry>(image + NativeWorldPoolMatrixProfile::WorldRemoveRva);
        // No callback can invalidate the final raw fence before entry. Pending
        // ownership was established by the caller, not by observing this value.
        receipt.entered = true;
        receipt.result = (invoke ? invoke : MTA_DE_WORLD_REMOVE_NATIVE_INVOKE)(entry, reinterpret_cast<void*>(value.bound.entity.address), receipt.exception);
        receipt.returned = receipt.result == ClockResult::Ok;
        return receipt.result;
    }
}
