/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve real native removal effects and typed dependency bookends
 ****************************************************************************/
#include "NativeStreamingRemoveCalls.h"
#include "NativeModelLoadedProfile.h"
#include "NativeStreamingRemoveAdmission.h"
namespace mta::game_de::NativeStreamingRemoveCalls
{
    namespace R = NativeStreamingRemoveMemory;
    namespace P = NativeStreamingRemoveProfile;
#if !defined(MTA_DE_STREAMING_REMOVE_NATIVE_INVOKE)
    namespace
    {
        ClockResult NativeCall(NativeFunction function, std::uint32_t model, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(static_cast<std::int32_t>(model));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
    }
#endif
    namespace
    {
        ClockResult Run(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner owner, CheckBeforeEntry guard, void* guardContext, Receipt* output,
                        Invoke invoke) noexcept
        {
            Receipt receipt;
            receipt.entryRva = P::RemoveRva;
            receipt.modelIndex = modelIndex;
            const auto finish = [&](ClockResult result)
            {
                if (output)
                    *output = receipt;
                return result;
            };
            if (modelIndex >= NativeModelLoadedProfile::ModelCount)
                return finish(ClockResult::InvalidArgument);
            if (!owner)
                return finish(ClockResult::NotInitialized);
            try
            {
                NativeStreamingRemoveAdmission::Value admission;
                NativeModelLoadedReader::Value        observed;
                auto                                  result = NativeStreamingRemoveAdmission::Capture(image, modelIndex, owner, admission, &observed);
                receipt.rawFlagsBefore = observed.rawFlags;
                receipt.rawStateBefore = observed.rawLoadState;
                if (result != ClockResult::Ok)
                    return finish(result);
                NativeModelLoadedReader        loaded({image}, owner);
                NativeModelLoadedReader::Value after{};
                NativeModelBankReader          bank({image}, owner);
                const auto&                    before = admission.state;
                const auto&                    model = admission.model;
                const auto&                    first = admission.native;
                const auto&                    idleBefore = admission.idle;
                receipt.rawFlagsBefore = before.rawFlags;
                receipt.rawStateBefore = before.rawLoadState;
                if (before.rawLoadState == 1)
                {
                    receipt.root = first.context.root;
                    receipt.provider = first.context.provider;
                    receipt.modelInfo = first.model;
                    receipt.bundle = first.bundle;
                    receipt.assetMap = first.assets.map;
                }
                if (guard && (result = guard(guardContext)) != ClockResult::Ok)
                    return finish(result);
                if ((result = owner()) != ClockResult::Ok)
                    return finish(result);
                const auto function = reinterpret_cast<NativeFunction>(image + P::RemoveRva);
#if defined(MTA_DE_STREAMING_REMOVE_NATIVE_INVOKE)
                const auto entry = invoke ? invoke : &MTA_DE_STREAMING_REMOVE_NATIVE_INVOKE;
#else
                const auto entry = invoke ? invoke : &NativeCall;
#endif
                receipt.entered = true;
                result = entry(function, modelIndex, receipt.exceptionCode);
                if (result != ClockResult::Ok)
                    return finish(result);
                receipt.returned = true;
                if ((result = loaded.Read(modelIndex, after)) != ClockResult::Ok)
                    return finish(result);
                receipt.rawFlagsAfter = after.rawFlags;
                receipt.rawStateAfter = after.rawLoadState;
                if (after.rawLoadState != 0 || after.rawFlags != before.rawFlags)
                    return finish(ClockResult::BindingChanged);
                if (before.rawLoadState == 1)
                {
                    if ((result = bank.Recheck(model)) != ClockResult::Ok)
                        return finish(result);
                    if ((result = R::RecheckReturned(image, model, first)) != ClockResult::Ok)
                        return finish(result);
                    if (!R::H::QualifyProfile(image))
                        return finish(ClockResult::BindingChanged);
                }
                else
                {
                    R::StreamRow idleAfter{};
                    if (!NativeImageAccess::ReadImage(image, after.rowAddress, idleAfter.data(), idleAfter.size(), true) || idleBefore != idleAfter)
                        return finish(ClockResult::BindingChanged);
                }
                if (!R::QualifyProfile(image))
                    return finish(ClockResult::BindingChanged);
                return finish(owner());
            }
            catch (...)
            {
                // Host observation allocation failure after entry is still an
                // entered failure. No accepted native effects are undone/replayed.
                return finish(ClockResult::ReadFailure);
            }
        }
    }
    ClockResult Remove(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        return Run(image, modelIndex, owner, nullptr, nullptr, output, invoke);
    }
    ClockResult RemoveGuarded(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner owner, CheckBeforeEntry guard, void* context, Receipt* output,
                              Invoke invoke) noexcept
    {
        if (!guard)
        {
            if (output)
                *output = {};
            return ClockResult::InvalidArgument;
        }
        return Run(image, modelIndex, owner, guard, context, output, invoke);
    }
}
