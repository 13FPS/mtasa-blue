/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One native ground query inside the existing loaded-world owner
 ****************************************************************************/
#include "NativeWorldGroundCalls.h"
#include "NativeWorldGroundContext.h"
#include "NativeWorldGroundProfile.h"
#include "NativeStartGameCalls.h"
namespace mta::game_de::NativeWorldGroundCalls
{
    namespace
    {
#if !defined(MTA_DE_WORLD_GROUND_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, const CVector& position, bool* found, std::uintptr_t* entity, CVector* normal, float& value,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                value = function(position.fX, position.fY, position.fZ, found, entity, normal);
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
    ClockResult Find(std::uintptr_t image, const CVector& source, float& output, CheckOwner owner, Receipt* receiptOutput, Invoke invoke) noexcept
    {
        namespace D = NativeStreamingCompletionContext;
        namespace G = NativeWorldGroundContext;
        // Snapshot value arguments before any supplied owner/read callback can
        // mutate the caller. Do not import scene scaling, coordinate clamps or
        // a made-up height fallback into MTA's existing float XYZ semantics.
        const CVector position = source;
        Receipt       receipt;
        receipt.entryRva = NativeWorldGroundProfile::EntryRva;
        receipt.position = {position.fX, position.fY, position.fZ};
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
        if (!G::Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        G::Value first, confirmed;
        if ((result = G::Capture(image, first)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok ||
            (result = G::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!G::SameBefore(first, confirmed) || !D::Fence(image, confirmed.graph) || !G::Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.root = first.graph.root;
        receipt.gameMode = first.graph.gameMode;
        receipt.level = first.graph.level;
        receipt.world = first.graph.world;
        receipt.scanBefore = first.scan;
        float nativeZ = 0;
        bool  found = false;
#if defined(MTA_DE_WORLD_GROUND_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_WORLD_GROUND_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + receipt.entryRva), position, &found, nullptr, nullptr, nativeZ, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        receipt.found = found;
        receipt.nativeZ = nativeZ;
        // The real entry may advance/wrap the global scan or lazily allocate
        // and evict matrices. Observe new current readiness without requiring
        // old list topology or restoring any of those legitimate native effects.
        if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = G::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        receipt.scanAfter = confirmed.scan;
        if (!G::SameWorld(first, confirmed) || !D::Fence(image, confirmed.graph) || !G::Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        output = nativeZ;
        return finish(ClockResult::Ok);
    }
}
