/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Invoke genuine empty branches while preserving every typed lifetime byte
 ****************************************************************************/
#include "NativeWorldBackupCleanupCalls.h"
#include <windows.h>
#ifndef MTA_DE_WORLD_BACKUP_CLEANUP_NATIVE_INVOKE
    #define MTA_DE_WORLD_BACKUP_CLEANUP_NATIVE_INVOKE &Original
#endif
namespace mta::game_de::NativeWorldBackupCleanupCalls
{
    namespace
    {
        ClockResult Original(Operation, Entry entry, std::uintptr_t entity, std::uint32_t& exception) noexcept
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
    ClockResult RunEmptyPrefix(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner, CheckOwner after,
                               Receipt* supplied, Invoker invoke) noexcept
    {
        Receipt    receipt;
        const auto finish = [&](ClockResult result)
        {
            receipt.result = result;
            for (const auto& stage : receipt.stages)
                receipt.uncertain = receipt.uncertain || (stage.entered && !stage.acknowledged);
            if (supplied)
                *supplied = receipt;
            return result;
        };
        if (!NativeStreamingCompletionContext::TryEnter())
            return finish(ClockResult::Busy);
        NativeStreamingCompletionContext::Scope scope;
        try
        {
            NativeWorldBackupCleanupMemory::Value value;
            auto                                  result = NativeWorldBackupCleanupMemory::CaptureEmpty(image, generation, kind, index, owner, after, value);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.entity = value.inputs.entity.address;
            const std::uintptr_t entries[] = {NativeWorldBackupCleanupProfile::DeleteRwRva, NativeWorldBackupCleanupProfile::ResolveReferencesRva};
            for (unsigned i = 0; i < receipt.stages.size(); ++i)
            {
                if ((result = owner()) != ClockResult::Ok || (result = after()) != ClockResult::Ok)
                    return finish(result);
                if (!NativeWorldBackupCleanupMemory::SealEmpty(image, value))
                    return finish(ClockResult::BindingChanged);
                auto& stage = receipt.stages[i];
                stage.entered = true;
                result = (invoke ? invoke : MTA_DE_WORLD_BACKUP_CLEANUP_NATIVE_INVOKE)(
                    i ? Operation::ResolveReferences : Operation::DeleteRwObject, reinterpret_cast<Entry>(image + entries[i]), receipt.entity, stage.exception);
                stage.result = result;
                stage.returned = result == ClockResult::Ok;
                if (result != ClockResult::Ok)
                    return finish(result);
                if ((result = owner()) != ClockResult::Ok || (result = after()) != ClockResult::Ok)
                    return finish(result);
                if (!NativeWorldBackupCleanupMemory::SealEmpty(image, value))
                    return finish(ClockResult::BindingChanged);
                stage.acknowledged = true;
                ++receipt.completed;
            }
            return finish(ClockResult::Ok);
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
