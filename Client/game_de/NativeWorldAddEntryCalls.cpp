/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the whole native WorldAdd acceptance and exact ACK boundary
 ****************************************************************************/
#include "NativeWorldAddEntryCalls.h"
#include <windows.h>
#ifndef MTA_DE_WORLD_ADD_NATIVE_INVOKE
    #define MTA_DE_WORLD_ADD_NATIVE_INVOKE &Original
#endif
namespace mta::game_de::NativeWorldAddEntryCalls
{
    namespace
    {
        ClockResult Original(Entry entry, std::uintptr_t entity, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(reinterpret_cast<void*>(entity));
                return ClockResult::Ok;
            }
            __except (exception = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
            {
                return ClockResult::OriginalException;
            }
        }
    }
    ClockResult Add(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner, CheckOwner after, Receipt* supplied,
                    Invoker invoke, NativeWorldBoundRectCalls::Invoker rectInvoke) noexcept
    {
        Receipt    receipt;
        const auto finish = [&](ClockResult result)
        {
            receipt.result = result;
            receipt.uncertain = receipt.entered && !receipt.acknowledged;
            if (supplied)
                *supplied = receipt;
            return result;
        };
        if (!NativeStreamingCompletionContext::TryEnter())
            return finish(ClockResult::Busy);
        NativeStreamingCompletionContext::Scope scope;
        try
        {
            NativeWorldAddEntryMemory::Value before, expected;
            auto result = NativeWorldAddEntryMemory::Capture(image, generation, kind, index, owner, after, before, rectInvoke, &receipt.rectangle);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.entity = before.budget.entity.address;
            receipt.nodes = before.budget.plan.steps.size();
            if (!NativeWorldAddEntryMemory::Expected(before, expected))
                return finish(ClockResult::BindingChanged);
            if ((result = owner()) != ClockResult::Ok || (result = after()) != ClockResult::Ok)
                return finish(result);
            if (!NativeWorldAddEntryMemory::Seal(image, before))
                return finish(ClockResult::BindingChanged);
            // Type1/5 skip the physical moving-list tail. Actor20 null returns
            // UEUpdate at its cookie epilogue, before any UE or transform write.
            // Entity Add then calls the already sealed BoundRect/AddRect slots.
            receipt.entered = true;
            result = (invoke ? invoke : MTA_DE_WORLD_ADD_NATIVE_INVOKE)(reinterpret_cast<Entry>(image + NativeWorldAddProfile::WorldAddRva), receipt.entity,
                                                                        receipt.exception);
            receipt.returned = result == ClockResult::Ok;
            if (result != ClockResult::Ok)
                return finish(result);
            if ((result = owner()) != ClockResult::Ok || (result = after()) != ClockResult::Ok)
                return finish(result);
            if (!NativeWorldAddEntryMemory::Seal(image, expected))
                return finish(ClockResult::BindingChanged);
            receipt.acknowledged = true;
            return finish(ClockResult::Ok);
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
