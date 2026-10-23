/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse current native world admission for the real LOS-clear query
 ****************************************************************************/
#include "NativeWorldLineCalls.h"
#include "NativeWorldLineProfile.h"
#include "NativeWorldGroundContext.h"
#include "NativeImageAccess.h"
#include <SharedUtil.IntTypes.h>
#include <CVector2D.h>
// The shared world/entity ABI contains legacy enum forwards and anonymous
// surface fields. Preserve it while keeping strict warnings for this code.
#pragma warning(push)
#pragma warning(disable : 4471 4201)
#include <game/CWorld.h>
#pragma warning(pop)
#include <cstring>
namespace mta::game_de::NativeWorldLineCalls
{
    namespace
    {
        bool Profile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : NativeWorldLineProfile::Pins)
            {
                std::array<unsigned char, NativeWorldLineProfile::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return NativeWorldGroundContext::Profile(image);
        }
#if !defined(MTA_DE_WORLD_LINE_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, const CVector& start, const CVector& end, const Flags& flags, bool& clear,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                clear = function(&start, &end, flags[0], flags[1], flags[2], flags[3], flags[4], flags[5], flags[6]);
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
    ClockResult Clear(std::uintptr_t image, const CVector& sourceStart, const CVector& sourceEnd, const SLineOfSightFlags& sourceFlags, bool& output,
                      CheckOwner owner, Receipt* receiptOutput, Invoke invoke) noexcept
    {
        namespace D = NativeStreamingCompletionContext;
        namespace G = NativeWorldGroundContext;
        // Capture both endpoints and all consumed flags before any supplied
        // owner/read callback. Preserve order and raw XYZ; the actual SDK
        // neither scales, clamps nor rejects nonfinite endpoint values.
        const CVector start = sourceStart, end = sourceEnd;
        const Flags   flags{sourceFlags.bCheckBuildings,
                          sourceFlags.bCheckVehicles,
                          sourceFlags.bCheckPeds,
                          sourceFlags.bCheckObjects,
                          sourceFlags.bCheckDummies,
                          sourceFlags.bSeeThroughStuff,
                          sourceFlags.bIgnoreSomeObjectsForCamera};
        Receipt       receipt;
        receipt.entryRva = NativeWorldLineProfile::EntryRva;
        receipt.start = {start.fX, start.fY, start.fZ};
        receipt.end = {end.fX, end.fY, end.fZ};
        receipt.flags = flags;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        auto     result = D::Owner(image, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        G::Value first, confirmed;
        if ((result = G::Capture(image, first)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok ||
            (result = G::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!G::SameBefore(first, confirmed) || !D::Fence(image, confirmed.graph) || !Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.root = first.graph.root;
        receipt.gameMode = first.graph.gameMode;
        receipt.level = first.graph.level;
        receipt.world = first.graph.world;
        receipt.scanBefore = first.scan;
        bool nativeClear = false;
#if defined(MTA_DE_WORLD_LINE_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_WORLD_LINE_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + receipt.entryRva), start, end, flags, nativeClear, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        receipt.nativeClear = nativeClear;
        // Native code owns sector traversal, scan wrap and lazy matrices.
        // New coherent list topology is accepted; old scan/head/tail values
        // are neither compared across the entry nor restored on failure.
        if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = G::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        receipt.scanAfter = confirmed.scan;
        if (!G::SameWorld(first, confirmed) || !D::Fence(image, confirmed.graph) || !Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        output = nativeClear;
        return finish(ClockResult::Ok);
    }
}
