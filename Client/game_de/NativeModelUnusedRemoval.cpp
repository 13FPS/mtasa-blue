/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep UnloadUnused distinct from reference release and count mutation
 ****************************************************************************/
#include "NativeModelUnusedRemoval.h"
#include "NativeModelBankProfile.h"
#include "NativeModelReferenceMap.h"
#include "NativeModelLoadedReader.h"

namespace mta::game_de::NativeModelUnusedRemoval
{
    namespace UnusedRemovalDetail
    {
        struct Current
        {
            std::uintptr_t                         image;
            NativeModelReferenceMemory::CheckOwner owner;
            NativeModelBankReader                  bank;
            NativeModelLoadedReader                loaded;
            NativeModelBankReader::Value           model{};
            NativeModelLoadedReader::Value         state{};
            NativeStreamingModelContext::Value     context{};
            NativeModelReferenceMap::Snapshot      first{};
            Current(std::uintptr_t image, NativeModelReferenceMemory::CheckOwner owner)
                : image(image), owner(owner), bank({image}, owner), loaded({image}, owner)
            {
            }

            ClockResult Recheck(bool afterRemoval)
            {
                auto result = bank.Recheck(model);
                if (result != ClockResult::Ok || (!afterRemoval && (result = loaded.Recheck(state)) != ClockResult::Ok) ||
                    (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
                    return result;
                NativeModelReferenceMap::Snapshot after;
                if ((result = NativeModelReferenceMap::Capture(context, model, after)) != ClockResult::Ok)
                    return result;
                // This operation never changes a reference DWORD. Retain the
                // complete qualified map through both entry and return checks.
                if (!NativeModelReferenceMap::Same(first, after, false, 0) || !NativeModelReferenceMap::QualifyProfile(image))
                    return ClockResult::BindingChanged;
                return owner();
            }
        };
        ClockResult BeforeRemoval(void* opaque) noexcept
        {
            try
            {
                return static_cast<Current*>(opaque)->Recheck(false);
            }
            catch (...)
            {
                return ClockResult::ReadFailure;
            }
        }
    }

    ClockResult RemoveIfZero(std::uintptr_t image, std::uint32_t modelIndex, NativeModelReferenceMemory::CheckOwner owner, bool& removed, Receipt* output,
                             NativeStreamingRemoveCalls::Invoke invoke, const NativeModelReferenceOrigin* inputOrigin) noexcept
    {
        const bool                       requireOrigin = inputOrigin != nullptr;
        const NativeModelReferenceOrigin expected = inputOrigin ? *inputOrigin : NativeModelReferenceOrigin{};
        Receipt                          receipt;
        receipt.modelIndex = modelIndex;
        const auto finish = [&](ClockResult result) noexcept
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount)
            return finish(ClockResult::InvalidArgument);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        try
        {
            UnusedRemovalDetail::Current current(image, owner);
            auto                         result = current.bank.Read(modelIndex, current.model);
            if (result != ClockResult::Ok || (result = current.loaded.Read(modelIndex, current.state)) != ClockResult::Ok)
                return finish(result);
            if (!current.model.nativeModelInfo || !current.state.streamedIn)
                return finish(ClockResult::NotInitialized);
            if (!NativeModelReferenceMap::QualifyProfile(image))
                return finish(ClockResult::UnsupportedBuild);
            if ((result = NativeStreamingModelContext::Capture(image, current.model, current.context)) != ClockResult::Ok ||
                (result = NativeModelReferenceMap::Capture(current.context, current.model, current.first)) != ClockResult::Ok)
                return finish(result);
            receipt.origin = {current.context.root, current.first.map, current.context.rootMembership.index, current.context.rootMembership.serial};
            if (receipt.origin.rootIndex < 0 || !receipt.origin.rootSerial ||
                (requireOrigin && (!expected.root || !expected.map || receipt.origin.root != expected.root || receipt.origin.map != expected.map ||
                                   !NativeUObjectMembershipAccess::MatchesWeak(current.context.rootMembership, expected.rootIndex, expected.rootSerial))))
                return finish(ClockResult::BindingChanged);
            receipt.count = current.first.rows[static_cast<std::size_t>(current.first.selected)].count;
            if ((result = current.Recheck(false)) != ClockResult::Ok)
                return finish(result);
            receipt.countObserved = true;
            if (receipt.count != 0)
            {
                receipt.verified = true;
                removed = false;
                return finish(ClockResult::Ok);
            }
            result =
                NativeStreamingRemoveCalls::RemoveGuarded(image, modelIndex, owner, &UnusedRemovalDetail::BeforeRemoval, &current, &receipt.removal, invoke);
            if (result != ClockResult::Ok)
                return finish(result);
            // Removal itself verifies unloaded stream state. Requiring the old
            // loaded state here would reject the acknowledged intended effect.
            if ((result = current.Recheck(true)) != ClockResult::Ok)
                return finish(result);
            receipt.verified = true;
            removed = true;
            return finish(ClockResult::Ok);
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
