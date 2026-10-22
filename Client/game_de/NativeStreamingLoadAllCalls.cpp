/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve genuine blocking-all effects within the shared completion owner
 ****************************************************************************/
#include "NativeStreamingLoadAllCalls.h"
#include "NativeStreamingLoadAllProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeStartGameCalls.h"
#include <windows.h>
namespace mta::game_de::NativeStreamingLoadAllCalls
{
    namespace LoadAllDetail
    {
#if !defined(MTA_DE_STREAMING_LOADALL_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uint32_t& exception) noexcept
        {
            __try
            {
                function();
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
    ClockResult Load(std::uintptr_t image, bool onlyPriority, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        namespace D = NativeStreamingCompletionContext;
        Receipt receipt;
        receipt.entryRva = NativeStreamingLoadAllProfile::EntryRva;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (onlyPriority)
            return finish(ClockResult::UnsupportedBuild);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        auto     result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        D::Value first, confirmed;
        if ((result = D::Capture(image, first)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        if ((result = D::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!D::Same(first, confirmed, true) || !D::Fence(image, first) || !D::Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            return finish(result);
        receipt.root = first.root;
        receipt.gameMode = first.gameMode;
        receipt.level = first.level;
        receipt.world = first.world;
        receipt.modeBefore = first.mode;
#if defined(MTA_DE_STREAMING_LOADALL_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_STREAMING_LOADALL_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &LoadAllDetail::NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + receipt.entryRva), receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        // Loading may retire requests, run callbacks, and change mode2 to1.
        // Preserve those effects. Only the same live world/root and borrowed
        // owner can acknowledge return; neither the old queue nor mode is reset.
        if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = D::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        receipt.modeAfter = confirmed.mode;
        if (!D::Same(first, confirmed, false) || !D::Fence(image, first) || !D::Profile(image))
            return finish(ClockResult::BindingChanged);
        return finish(NativeStartGameCalls::CheckThread(image));
    }
}
