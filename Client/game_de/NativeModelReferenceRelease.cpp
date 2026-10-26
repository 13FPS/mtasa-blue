/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve accepted final-reference effects through native removal
 ****************************************************************************/
#include "NativeModelReferenceRelease.h"
#include "NativeModelReferenceMap.h"
#include "NativeStreamingRemoveAdmission.h"
namespace mta::game_de::NativeModelReferenceRelease
{
    namespace M = NativeModelReferenceMap;
    namespace C = NativeStreamingModelContext;
    namespace A = NativeStreamingRemoveAdmission;
    namespace RC = NativeStreamingRemoveCalls;
    namespace P = NativeModelReferenceProfile;
    namespace
    {
        struct Current
        {
            std::uintptr_t                         image;
            NativeModelReferenceMemory::CheckOwner owner;
            NativeModelBankReader                  bank;
            NativeModelLoadedReader                loaded;
            NativeModelBankReader::Value           model{};
            NativeModelLoadedReader::Value         state{};
            C::Value                               context{};
            M::Snapshot                            first{};
            A::Value                               removal{};
            Current(std::uintptr_t image, NativeModelReferenceMemory::CheckOwner owner)
                : image(image), owner(owner), bank({image}, owner), loaded({image}, owner)
            {
            }
            ClockResult Recheck(bool accepted)
            {
                auto result = bank.Recheck(model);
                if (result != ClockResult::Ok || (result = loaded.Recheck(state)) != ClockResult::Ok ||
                    (result = C::Recheck(image, model, context)) != ClockResult::Ok)
                    return result;
                M::Snapshot after;
                if ((result = M::Capture(context, model, after)) != ClockResult::Ok)
                    return result;
                if (!M::Same(first, after, accepted, 0) || !M::QualifyProfile(image))
                    return ClockResult::BindingChanged;
                return owner();
            }
        };
        ClockResult BeforeRemoval(void* opaque) noexcept
        {
            // This state is stack owned by the single synchronous operation.
            // No row address or context is retained across callbacks.
            try
            {
                auto& current = *static_cast<Current*>(opaque);
                auto  result = current.Recheck(true);
                if (result != ClockResult::Ok || (result = A::Recheck(current.image, current.owner, current.removal)) != ClockResult::Ok)
                    return result;
                return current.Recheck(true);
            }
            catch (...)
            {
                return ClockResult::ReadFailure;
            }
        }
#if !defined(MTA_DE_MODEL_REFERENCE_RELEASE_EXCHANGE)
        ClockResult NativeExchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed,
                                   std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(LONG) == sizeof(expected));
            LONG e = 0, t = 0;
            std::memcpy(&e, &expected, sizeof(e));
            std::memcpy(&t, &target, sizeof(t));
            __try
            {
                const auto previous = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), t, e);
                std::memcpy(&observed, &previous, sizeof(observed));
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
    ClockResult ReleaseFinal(std::uintptr_t image, std::uint32_t modelIndex, ZeroPolicy policy, NativeModelReferenceMemory::CheckOwner owner, Receipt* output,
                             NativeModelReferenceMemory::Exchange exchange, RC::Invoke invoke,
                             const NativeModelReferenceMemory::RequiredIdentity* inputIdentity) noexcept
    {
        const bool                                         requireIdentity = inputIdentity != nullptr;
        const NativeModelReferenceMemory::RequiredIdentity identity = inputIdentity ? *inputIdentity : NativeModelReferenceMemory::RequiredIdentity{};
        Receipt                                            receipt;
        receipt.reference.modelIndex = modelIndex;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount || (policy != ZeroPolicy::KeepResident && policy != ZeroPolicy::RemoveAtZero))
            return finish(ClockResult::InvalidArgument);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        try
        {
            Current current(image, owner);
            auto    result = current.bank.Read(modelIndex, current.model);
            if (result != ClockResult::Ok || (result = current.loaded.Read(modelIndex, current.state)) != ClockResult::Ok)
                return finish(result);
            if (!current.model.nativeModelInfo || !current.state.streamedIn)
                return finish(ClockResult::NotInitialized);
            if (!M::QualifyProfile(image))
                return finish(ClockResult::UnsupportedBuild);
            if ((result = C::Capture(image, current.model, current.context)) != ClockResult::Ok ||
                (result = M::Capture(current.context, current.model, current.first)) != ClockResult::Ok)
                return finish(result);
            // An issued capability belongs to the original embedded map.
            // Existing rechecks fence this origin through CAS and removal;
            // current readability alone cannot adopt a replacement application.
            if (requireIdentity && (!identity.root || !identity.map || current.context.root != identity.root || current.first.map != identity.map ||
                                    !NativeUObjectMembershipAccess::MatchesWeak(current.context.rootMembership, identity.rootIndex, identity.rootSerial)))
                return finish(ClockResult::BindingChanged);
            auto& ref = receipt.reference;
            ref.root = current.context.root;
            ref.map = current.first.map;
            ref.origin = {current.context.root, current.first.map, current.context.rootMembership.index, current.context.rootMembership.serial};
            ref.row = current.first.rowsAddress + static_cast<std::uintptr_t>(current.first.selected) * P::RowBytes;
            ref.before = current.first.rows[static_cast<std::size_t>(current.first.selected)].count;
            if (ref.before != 1)
                return finish(ClockResult::InvalidArgument);
            if (policy == ZeroPolicy::RemoveAtZero)
            {
                if ((result = A::Capture(image, modelIndex, owner, current.removal)) != ClockResult::Ok)
                    return finish(result);
                if (current.removal.state.rawLoadState != 1 || current.removal.model.nativeModelInfo != current.model.nativeModelInfo ||
                    current.removal.native.context.root != current.context.root || current.removal.native.context.provider != current.context.provider)
                    return finish(ClockResult::BindingChanged);
                receipt.removalPrepared = true;
            }
            if ((result = current.Recheck(false)) != ClockResult::Ok)
                return finish(result);
            if (receipt.removalPrepared && (result = A::Recheck(image, owner, current.removal)) != ClockResult::Ok)
                return finish(result);
            if ((result = current.Recheck(false)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_MODEL_REFERENCE_RELEASE_EXCHANGE)
            const auto entry = exchange ? exchange : &MTA_DE_MODEL_REFERENCE_RELEASE_EXCHANGE;
#else
            const auto entry = exchange ? exchange : &NativeExchange;
#endif
            ref.entered = true;
            result = entry(ref.row + 4, ref.before, 0, ref.observed, ref.exceptionCode);
            ref.accepted = result == ClockResult::Ok && ref.observed == ref.before;
            if (result != ClockResult::Ok)
                return finish(result);
            if (!ref.accepted)
                return finish(ClockResult::Busy);
            if ((result = current.Recheck(true)) != ClockResult::Ok)
                return finish(result);
            if (policy == ZeroPolicy::KeepResident)
                return finish(ClockResult::Ok);
            result = RC::RemoveGuarded(image, modelIndex, owner, &BeforeRemoval, &current, &receipt.removal, invoke);
            if (result != ClockResult::Ok)
                return finish(result);
            // Native streaming removal changes its own stream/asset state,
            // not this keyed DWORD map. Reacquire it without requiring the
            // old loaded state and never free or restore the accepted row.
            if ((result = current.bank.Recheck(current.model)) != ClockResult::Ok ||
                (result = C::Recheck(image, current.model, current.context)) != ClockResult::Ok)
                return finish(result);
            M::Snapshot after;
            if ((result = M::Capture(current.context, current.model, after)) != ClockResult::Ok)
                return finish(result);
            if (!M::Same(current.first, after, true, 0) || !M::QualifyProfile(image))
                return finish(ClockResult::BindingChanged);
            return finish(owner());
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
