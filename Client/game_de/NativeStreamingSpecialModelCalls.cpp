/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit native removal/name-cache/requeue dependencies without emulating them
 ****************************************************************************/
#include "NativeStreamingSpecialModelCalls.h"
#include "NativeStreamingSpecialModelProfile.h"
#include "NativeNameConstructionContext.h"
#include "NativeStreamingCompletionContext.h"
// Removal admission consumes the loaded-row type. Include it explicitly so
// the normal backend does not depend on a combined fixture's include order.
#include "NativeModelLoadedReader.h"
#include "NativeStreamingRemoveAdmission.h"
#include "NativeModelReferenceHeap.h"
#include "NativeStartGameCalls.h"
#include <game/CModelRequestPolicy.h>
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

namespace mta::game_de::NativeStreamingSpecialModelCalls
{
    namespace SpecialModelDetail
    {
        namespace P = NativeStreamingSpecialModelProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace M = NativeModelReferenceMap;
        namespace H = NativeModelReferenceHeap;
        namespace C = NativeStreamingModelContext;
        // Both native operations observe the same existing name/heap state.
        // The real entry still owns construction, locking, insertion and effects.
        using NativeNameConstructionContext::CallerByte;
        using NativeNameConstructionContext::Names;
        using NativeNameConstructionContext::Profile;
        using NativeNameConstructionContext::ReadNames;
        using NativeNameConstructionContext::Same;
        ClockResult ReferenceZero(std::uintptr_t metadata) noexcept
        {
            std::int16_t refs = 0;
            if (!M::Copy(metadata + 0x22, &refs, sizeof(refs)))
                return ClockResult::ReadFailure;
            // Native positive refs scan/destroy peds and objects. Negative raw
            // counts are not evidence that those unqualified effects are safe.
            return refs == 0 ? ClockResult::Ok : ClockResult::UnsupportedBuild;
        }
#if !defined(MTA_DE_STREAMING_SPECIAL_MODEL_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uint32_t model, const char* name, std::uint32_t channel, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(static_cast<std::int32_t>(model), name, channel);
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
    ClockResult CheckReady(std::uintptr_t image, CheckOwner owner) noexcept
    {
        namespace S = SpecialModelDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace C = NativeStreamingModelContext;
        namespace R = NativeStreamingRemoveMemory;
        if (!owner)
            return ClockResult::NotInitialized;
        if (!D::TryEnter())
            return ClockResult::Busy;
        D::Scope scope;
        try
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            // Reuse the real name/heap/world profiles and independent removal
            // and bank qualification. No selected row may be required before
            // the startup itinerary has created its replacement ped metadata.
            if (!S::Profile(image) || !R::QualifyProfile(image))
                return ClockResult::UnsupportedBuild;
            if (!NativeModelBankReader::Revalidate({image}))
                return ClockResult::BindingChanged;
            C::Value provider, confirmedProvider;
            D::Value world, confirmedWorld;
            S::Names names, confirmedNames;
            if ((result = C::CaptureProvider(image, provider)) != ClockResult::Ok || (result = D::Capture(image, world)) != ClockResult::Ok ||
                (result = S::ReadNames(image, names)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok ||
                (result = C::CaptureProvider(image, confirmedProvider)) != ClockResult::Ok || (result = D::Capture(image, confirmedWorld)) != ClockResult::Ok ||
                (result = S::ReadNames(image, confirmedNames)) != ClockResult::Ok)
                return result;
            if (!C::Same(provider, confirmedProvider) || !C::MembershipUnchanged(image, provider) || !C::MembershipUnchanged(image, confirmedProvider) ||
                !D::Same(world, confirmedWorld, true) || !D::Fence(image, world) || !D::Fence(image, confirmedWorld) || !S::Same(names, confirmedNames) ||
                !S::Profile(image) || !R::QualifyProfile(image) || !NativeModelBankReader::Revalidate({image}))
                return ClockResult::BindingChanged;
            return D::Owner(image, owner);
        }
        catch (const std::bad_alloc&)
        {
            // Existing name-cache readers use temporary CPU copy buffers;
            // their failure is no native allocator entry or engine effect.
            return ClockResult::WriteFailure;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    ClockResult Request(std::uintptr_t image, std::uint32_t modelIndex, const char* name, std::uint32_t channel, CheckOwner owner, Receipt* output,
                        Invoke invoke) noexcept
    {
        namespace S = SpecialModelDetail;
        namespace P = NativeStreamingSpecialModelProfile;
        namespace D = NativeStreamingCompletionContext;
        Receipt receipt;
        receipt.entryRva = P::EntryRva;
        receipt.modelIndex = modelIndex;
        receipt.channel = channel;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount || !name)
            return finish(ClockResult::InvalidArgument);
        if (reinterpret_cast<std::uintptr_t>(name) > (std::numeric_limits<std::uintptr_t>::max)() - receipt.copiedName.size())
            return finish(ClockResult::InvalidArgument);
        if (MtaGame::CModelRequestPolicy::IsUpgradeModel(modelIndex))
            return finish(ClockResult::UnsupportedBuild);
        std::size_t nameBytes = 0;
        for (; nameBytes != receipt.copiedName.size(); ++nameBytes)
        {
            unsigned char byte = 0;
            if (!S::CallerByte(reinterpret_cast<std::uintptr_t>(name) + nameBytes, byte))
                return finish(ClockResult::ReadFailure);
            if (byte >= 0x80)
                return finish(ClockResult::UnsupportedBuild);
            receipt.copiedName[nameBytes] = static_cast<char>(byte);
            if (!byte)
                break;
        }
        if (nameBytes == receipt.copiedName.size())
            return finish(ClockResult::UnsupportedBuild);
        ++nameBytes;
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        try
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            if (!S::Profile(image))
                return finish(ClockResult::UnsupportedBuild);
            NativeModelBankReader        bank({image}, owner);
            NativeModelBankReader::Value model;
            if ((result = bank.Read(modelIndex, model)) != ClockResult::Ok)
                return finish(result);
            if (!model.nativeModelInfo)
                return finish(ClockResult::NotInitialized);
            NativeStreamingModelContext::Value    context;
            D::Value                              world, confirmedWorld;
            NativeStreamingRemoveAdmission::Value removal;
            S::Names                              names, confirmedNames;
            if ((result = NativeStreamingModelContext::Capture(image, model, context)) != ClockResult::Ok ||
                (result = D::Capture(image, world)) != ClockResult::Ok || (result = S::ReferenceZero(model.nativeModelInfo)) != ClockResult::Ok ||
                (result = NativeStreamingRemoveAdmission::Capture(image, modelIndex, owner, removal)) != ClockResult::Ok ||
                (result = S::ReadNames(image, names)) != ClockResult::Ok)
                return finish(result);
            receipt.root = context.root;
            receipt.provider = context.provider;
            receipt.modelInfo = model.nativeModelInfo;
            receipt.rawFlagsBefore = removal.state.rawFlags;
            receipt.rawStateBefore = removal.state.rawLoadState;
            if ((result = bank.Recheck(model)) != ClockResult::Ok ||
                (result = NativeStreamingRemoveAdmission::Recheck(image, owner, removal)) != ClockResult::Ok ||
                (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok ||
                (result = D::Owner(image, owner)) != ClockResult::Ok || (result = D::Capture(image, confirmedWorld)) != ClockResult::Ok ||
                (result = S::ReadNames(image, confirmedNames)) != ClockResult::Ok || (result = S::ReferenceZero(model.nativeModelInfo)) != ClockResult::Ok)
                return finish(result);
            if (!D::Same(world, confirmedWorld, true) || !D::Fence(image, world) || !S::Same(names, confirmedNames) || !S::Profile(image))
                return finish(ClockResult::BindingChanged);
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_STREAMING_SPECIAL_MODEL_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_STREAMING_SPECIAL_MODEL_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &S::NativeCall;
#endif
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(image + receipt.entryRva), modelIndex, receipt.copiedName.data(), channel, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            // The real entry copies the name into metadata and cache-owned
            // FName storage before returning. Its asynchronous tail receives
            // only model/flags/extra0; it never borrows our stack name later.
            NativeModelLoadedReader        loaded({image}, owner);
            NativeModelLoadedReader::Value after;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = bank.Recheck(model)) != ClockResult::Ok ||
                (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok ||
                (result = loaded.Read(modelIndex, after)) != ClockResult::Ok || (result = D::Capture(image, confirmedWorld)) != ClockResult::Ok ||
                (result = S::ReadNames(image, confirmedNames)) != ClockResult::Ok)
                return finish(result);
            receipt.rawFlagsAfter = after.rawFlags;
            receipt.rawStateAfter = after.rawLoadState;
            std::array<char, P::MaximumNameBytes> actual{};
            if (!S::M::Copy(model.nativeModelInfo + 8, &receipt.keyAfter, 4) || !S::M::Copy(model.nativeModelInfo + 0xc, actual.data(), nameBytes))
                return finish(ClockResult::ReadFailure);
            if (std::memcmp(actual.data(), receipt.copiedName.data(), nameBytes) || after.rawLoadState > 4 || !D::Same(world, confirmedWorld, false) ||
                !D::Fence(image, world) || !S::Profile(image))
                return finish(ClockResult::BindingChanged);
            // Cache growth, removal and requeueing persist. Returning does not
            // claim loaded success or restore the former names/queue/list state.
            return finish(NativeStartGameCalls::CheckThread(image));
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
