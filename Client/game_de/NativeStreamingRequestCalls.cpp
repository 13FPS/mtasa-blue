/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native asynchronous request effects on the existing owner
 ****************************************************************************/
#include "NativeStreamingRequestCalls.h"
#include "NativeStreamingRequestProfile.h"
#include "NativeModelBankProfile.h"
#include "NativeStreamingModelContext.h"
#include <windows.h>
#include <game/CModelRequestPolicy.h>

namespace mta::game_de::NativeStreamingRequestCalls
{
    namespace
    {
        namespace P = NativeStreamingRequestProfile;
        namespace Context = NativeStreamingModelContext;
#if !defined(MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE)
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
    ClockResult Request(std::uintptr_t image, std::uint32_t modelIndex, std::uint32_t flags, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        Receipt receipt;
        receipt.entryRva = P::RequestRva;
        receipt.modelIndex = modelIndex;
        receipt.flags = flags;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount)
            return finish(ClockResult::InvalidArgument);
        // MTA deliberately dispatches these IDs through RequestVehicleUpgrade.
        // Ordinary RequestModel cannot substitute for the paired DE operation.
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(modelIndex))
            return finish(ClockResult::UnsupportedBuild);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        NativeModelBankReader          bank({image}, owner);
        NativeModelLoadedReader        loaded({image}, owner);
        NativeModelBankReader::Value   model;
        NativeModelLoadedReader::Value before, after;
        auto                           result = bank.Read(modelIndex, model);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!model.nativeModelInfo)
            return finish(ClockResult::NotInitialized);
        if ((result = loaded.Read(modelIndex, before)) != ClockResult::Ok)
            return finish(result);
        receipt.rawFlagsBefore = before.rawFlags;
        receipt.rawStateBefore = before.rawLoadState;
        if (before.rawLoadState > 4)
            return finish(ClockResult::BindingChanged);
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::UnsupportedBuild);
        Context::Value first, confirmed;
        if ((result = Context::Capture(image, model, first)) != ClockResult::Ok)
            return finish(result);
        if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = loaded.Recheck(before)) != ClockResult::Ok)
            return finish(result);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        if ((result = Context::Capture(image, model, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!Context::Same(first, confirmed) || !Context::QualifyProfile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = owner()) != ClockResult::Ok)
            return finish(result);
        receipt.root = first.root;
        receipt.provider = first.provider;
        receipt.modelInfo = model.nativeModelInfo;
        const auto function = reinterpret_cast<NativeFunction>(image + P::RequestRva);
#if defined(MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_STREAMING_REQUEST_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        receipt.entered = true;
        result = entry(function, modelIndex, flags, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if ((result = loaded.Read(modelIndex, after)) != ClockResult::Ok)
            return finish(result);
        receipt.rawFlagsAfter = after.rawFlags;
        receipt.rawStateAfter = after.rawLoadState;
        if ((result = bank.Recheck(model)) != ClockResult::Ok)
            return finish(result);
        if ((result = Context::Recheck(image, model, first)) != ClockResult::Ok)
            return finish(result);
        if (!Context::QualifyProfile(image))
            return finish(ClockResult::BindingChanged);
        // Async completion may advance or intentionally reject the request.
        // Acknowledgement means the real entry returned on the same admitted
        // owner; it neither fabricates loaded state nor rewinds native jobs.
        return finish(owner());
    }
}
