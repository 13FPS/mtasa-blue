/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Invoke the real read-only four-corner producer under exact borrows
 ****************************************************************************/
#include "NativeWorldBoundRectCalls.h"
#include "NativeWorldBoundRectProfile.h"
#include <windows.h>

#ifndef MTA_DE_WORLD_BOUND_RECT_NATIVE_INVOKE
    #define MTA_DE_WORLD_BOUND_RECT_NATIVE_INVOKE &Original
#endif
namespace mta::game_de::NativeWorldBoundRectCalls
{
    namespace
    {
        ClockResult Original(Entry entry, std::uintptr_t entity, Rect& output, Rect*& returned, std::uint32_t& exception) noexcept
        {
            __try
            {
                returned = entry(reinterpret_cast<void*>(entity), &output);
                return ClockResult::Ok;
            }
            __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
    }
    ClockResult Read(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, Rect& output, CheckOwner owner,
                     CheckOwner afterApplication, Receipt* suppliedReceipt, Invoker invoke)
    {
        Receipt    receipt;
        const auto finish = [&](ClockResult result)
        {
            receipt.result = result;
            receipt.uncertain = receipt.entered && result != ClockResult::Ok;
            if (suppliedReceipt)
                *suppliedReceipt = receipt;
            return result;
        };
        NativeWorldBoundRectMemory::Value value;
        auto                              result = NativeWorldBoundRectMemory::Capture(image, generation, kind, index, owner, afterApplication, value);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.entity = value.entity.address;
        receipt.metadata = value.model.model.nativeModelInfo;
        Rect  local{};
        Rect* returned = nullptr;
        // Type1 never enters the model-type10 resolver. The qualified matrix
        // path reads only its current link; exact inline +0 takes no-call math.
        // The genuine whole function writes only this owned caller output.
        receipt.entered = true;
        result = (invoke ? invoke : MTA_DE_WORLD_BOUND_RECT_NATIVE_INVOKE)(reinterpret_cast<Entry>(image + NativeWorldBoundRectProfile::EntryRva),
                                                                           value.entity.address, local, returned, receipt.exception);
        receipt.returned = result == ClockResult::Ok;
        if (result != ClockResult::Ok)
            return finish(result);
        if (returned != &local || !NativeWorldIplPlan::Valid(local))
            return finish(ClockResult::BindingChanged);
        if ((result = owner()) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return finish(result);
        if (!NativeWorldBoundRectMemory::Seal(image, value))
            return finish(ClockResult::BindingChanged);
        output = local;
        return finish(ClockResult::Ok);
    }
}
