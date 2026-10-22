/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve genuine resource decode/retry and non-replayed effects
 ****************************************************************************/
#include "NativeStreamingResourceCalls.h"
#include "NativeStreamingResourceContext.h"
#include "NativeStreamingResourceProfile.h"
#include <windows.h>
namespace mta::game_de::NativeStreamingResourceCalls
{
    namespace ResourceCallDetail
    {
#if !defined(MTA_DE_STREAMING_RESOURCE_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uint32_t model, std::uint32_t flags, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(static_cast<std::int32_t>(model), flags, 0);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    ClockResult Request(std::uintptr_t image, std::uint32_t model, std::uint32_t flags, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        namespace P = NativeStreamingResourceProfile;
        namespace Context = NativeStreamingResourceContext;
        Receipt receipt;
        receipt.entryRva = NativeStreamingRequestProfile::RequestRva;
        receipt.modelIndex = model;
        receipt.flags = flags;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (model < P::ResourceFirst || model >= P::ResourceEnd)
            return finish(ClockResult::InvalidArgument);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        NativeModelLoadedReader        reader({image}, owner);
        NativeModelLoadedReader::Value before, after;
        auto                           result = reader.Read(model, before);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.rawFlagsBefore = before.rawFlags;
        receipt.rawStateBefore = before.rawLoadState;
        if (before.rawLoadState > 4)
            return finish(ClockResult::BindingChanged);
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::UnsupportedBuild);
        Context::Value context;
        if ((result = Context::Capture(image, before, context)) != ClockResult::Ok)
            return finish(result);
        if ((result = reader.Recheck(before)) != ClockResult::Ok || (result = Context::Recheck(image, context)) != ClockResult::Ok)
            return finish(result);
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        receipt.root = context.root;
        receipt.datProvider = context.datProvider;
        receipt.imgFileCount = context.imgFileCount;
        const auto function = reinterpret_cast<NativeFunction>(image + receipt.entryRva);
#if defined(MTA_DE_STREAMING_RESOURCE_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_STREAMING_RESOURCE_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &ResourceCallDetail::NativeCall;
#endif
        receipt.entered = true;
        result = entry(function, model, flags, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if ((result = reader.Read(model, after)) != ClockResult::Ok)
            return finish(result);
        receipt.rawFlagsAfter = after.rawFlags;
        receipt.rawStateAfter = after.rawLoadState;
        if ((result = Context::Recheck(image, context, true)) != ClockResult::Ok || (result = Context::CheckCurrentLists(image, model)) != ClockResult::Ok)
            return finish(result);
        if ((result = reader.Recheck(after)) != ClockResult::Ok)
            return finish(result);
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::BindingChanged);
        // The genuine entry may already have decoded, queued, removed/retried
        // or rebuilt a native map. Its receipt acknowledges that call only;
        // it grants no loaded result and no inverse mutation or replay.
        return finish(owner());
    }
}
