/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse genuine model classifier semantics with current owner bookends
 ****************************************************************************/
#include "NativeModelClassificationCalls.h"
#include "NativeStreamingModelContext.h"
#include "NativeImageAccess.h"
#include "NativeModelBankProfile.h"
#include "NativeStartGameCalls.h"
#include "NativeModelTypeLeaf.h"
#include "NativeGameStateMemory.h"
#include <windows.h>
#include <array>
#include <algorithm>
#include <limits>
#include <cstring>
namespace mta::game_de::NativeModelClassificationCalls
{
    namespace ClassificationDetail
    {
        namespace P = NativeModelClassificationProfile;
        bool Profile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return NativeStreamingModelContext::QualifyProfile(image);
        }
        bool VehicleField(std::uintptr_t model, std::uint32_t& output) noexcept
        {
            // Classifier DWORD+84 is the only additional receiver field. The
            // bounded prefix proves readable data pages in one allocation;
            // it neither adopts that allocation nor follows the raw value.
            constexpr std::size_t offset = 0x84, bytes = offset + sizeof(output);
            if (!model || model % alignof(std::uintptr_t) || bytes > (std::numeric_limits<std::uintptr_t>::max)() - model)
                return false;
            const auto     end = model + bytes;
            std::uintptr_t address = model, allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED && page.Type != MEM_IMAGE) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto access = page.Protect & 0xff;
                if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY)
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress), current = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
                if (!current || (allocation && allocation != current) || begin > address ||
                    page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                allocation = current;
                address = (std::min)(end, begin + page.RegionSize);
            }
            SIZE_T        copied = 0;
            std::uint32_t next = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(model + offset), &next, sizeof(next), &copied) || copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
#if !defined(MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uint32_t model, unsigned char& result, std::uint32_t& exception) noexcept
        {
            __try
            {
                result = function(static_cast<std::int32_t>(model));
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
    ClockResult ReadType(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner owner, NativeModelTypeObservation& output) noexcept
    {
        if (modelIndex >= NativeModelBankProfile::ModelCount)
            return ClockResult::InvalidArgument;
        if (!owner)
            return ClockResult::NotInitialized;
        NativeModelBankReader        bank({image}, owner);
        NativeModelBankReader::Value model;
        auto                         result = bank.Read(modelIndex, model);
        if (result != ClockResult::Ok)
            return result;
        if (!model.nativeModelInfo)
        {
            // Classic GetModelType has a real absent-interface result. Publish
            // absence only after qualifying the current null bank word; owner
            // failure is never reinterpreted as UNKNOWN or native AL9.
            if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok)
                return result;
            std::uintptr_t current = 1;
            if (!NativeImageAccess::ReadImage(image, model.slotAddress, &current, sizeof(current), true) || current)
                return ClockResult::BindingChanged;
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return result;
            output = {false, 0};
            return ClockResult::Ok;
        }
        if (!NativeStreamingModelContext::QualifyProfile(image))
            return ClockResult::UnsupportedBuild;
        NativeStreamingModelContext::Value context;
        if ((result = NativeStreamingModelContext::Capture(image, model, context)) != ClockResult::Ok)
            return result;
        std::array<unsigned char, 6> before{}, after{};
        unsigned char                type = 0, confirmed = 0;
        if (!NativeModelTypeLeaf::Read(image, context.typeMethod, before, type))
            return ClockResult::UnsupportedBuild;
        if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok ||
            (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
            return result;
        std::uintptr_t currentModel = 0;
        if (!NativeModelTypeLeaf::Read(image, context.typeMethod, after, confirmed) || before != after || type != confirmed ||
            !NativeImageAccess::ReadImage(image, model.slotAddress, &currentModel, sizeof(currentModel), true) || currentModel != model.nativeModelInfo ||
            !NativeStreamingModelContext::QualifyProfile(image))
            return ClockResult::BindingChanged;
        if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            return result;
        // This is the actual AL value from one proved self-contained body,
        // not a vtable identity guess or a native call through an arbitrary slot.
        output = {true, type};
        return ClockResult::Ok;
    }
    ClockResult ReadVehicleSubtype(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner owner, std::uint32_t& output) noexcept
    {
        if (modelIndex >= NativeModelBankProfile::ModelCount)
            return ClockResult::InvalidArgument;
        if (!owner)
            return ClockResult::NotInitialized;
        NativeModelBankReader        bank({image}, owner);
        NativeModelBankReader::Value model;
        auto                         result = bank.Read(modelIndex, model);
        if (result != ClockResult::Ok)
            return result;
        if (!model.nativeModelInfo)
            return ClockResult::NotInitialized;
        if (!ClassificationDetail::Profile(image))
            return ClockResult::UnsupportedBuild;
        NativeStreamingModelContext::Value context;
        if ((result = NativeStreamingModelContext::Capture(image, model, context)) != ClockResult::Ok)
            return result;
        std::array<unsigned char, 6> before{}, after{};
        std::uint8_t                 firstType = 0, secondType = 0;
        if (!NativeModelTypeLeaf::Read(image, context.typeMethod, before, firstType))
            return ClockResult::UnsupportedBuild;
        if (firstType != 6)
            return ClockResult::NotInitialized;
        std::uint32_t first = 0, second = 0;
        if (!ClassificationDetail::VehicleField(model.nativeModelInfo, first))
            return ClockResult::ReadFailure;
        if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok ||
            (result = owner()) != ClockResult::Ok)
            return result;
        // Finish all owner callbacks before the final type/data samples.
        // Re-admit the same provider/root/membership and exact profile, rather
        // than treating the SDK's earlier IsVehicle query as a lasting lease.
        if ((result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
            return result;
        if (!NativeModelBankReader::Revalidate({image}) || !ClassificationDetail::Profile(image))
            return ClockResult::BindingChanged;
        if (!NativeModelTypeLeaf::Read(image, context.typeMethod, after, secondType) || before != after || secondType != firstType)
            return ClockResult::BindingChanged;
        if (!ClassificationDetail::VehicleField(model.nativeModelInfo, second))
            return ClockResult::ReadFailure;
        if (first != second)
            return ClockResult::BindingChanged;
        if ((result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
            return result;
        std::uintptr_t current = 0;
        if (!NativeImageAccess::ReadImage(image, model.slotAddress, &current, sizeof(current), true) || current != model.nativeModelInfo ||
            !NativeStreamingModelContext::MembershipUnchanged(image, context) || !ClassificationDetail::Profile(image))
            return ClockResult::BindingChanged;
        std::uint32_t state = 0;
        if (!NativeGameStateMemory::Read(image, state))
            return ClockResult::BindingChanged;
        if (state != 9)
            return ClockResult::NotInitialized;
        if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            return result;
        // Unknown/high subtype bits are still a successful raw observation.
        // This adds no names for the seven unqualified vehicle predicates.
        output = second;
        return ClockResult::Ok;
    }
    ClockResult Read(std::uintptr_t image, std::uint32_t modelIndex, NativeModelVehiclePredicate predicate, CheckOwner owner, bool& output,
                     Receipt* receiptOutput, Invoke invoke) noexcept
    {
        Receipt receipt;
        receipt.modelIndex = modelIndex;
        receipt.predicate = predicate;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (modelIndex >= NativeModelBankProfile::ModelCount)
            return finish(ClockResult::InvalidArgument);
        for (const auto& entry : NativeModelClassificationProfile::Entries)
            if (entry.predicate == predicate)
                receipt.entryRva = entry.rva;
        if (!receipt.entryRva)
            return finish(ClockResult::InvalidArgument);
        if (!owner)
            return finish(ClockResult::NotInitialized);
        NativeModelBankReader        bank({image}, owner);
        NativeModelBankReader::Value model;
        auto                         result = bank.Read(modelIndex, model);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!model.nativeModelInfo)
            return finish(ClockResult::NotInitialized);
        if (!ClassificationDetail::Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        NativeStreamingModelContext::Value context, confirmed;
        if ((result = NativeStreamingModelContext::Capture(image, model, context)) != ClockResult::Ok || (result = bank.Recheck(model)) != ClockResult::Ok ||
            (result = owner()) != ClockResult::Ok || (result = NativeStreamingModelContext::Capture(image, model, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!NativeStreamingModelContext::Same(context, confirmed) || !NativeStreamingModelContext::MembershipUnchanged(image, context) ||
            !ClassificationDetail::Profile(image))
            return finish(ClockResult::BindingChanged);
        // The classifier itself indexes the current bank. Close that bank
        // word after owner callbacks, rather than qualifying only an earlier
        // metadata address that the native entry would no longer consume.
        std::uintptr_t currentModel = 0;
        if (!NativeImageAccess::ReadImage(image, model.slotAddress, &currentModel, sizeof(currentModel), true) || currentModel != model.nativeModelInfo)
            return finish(ClockResult::BindingChanged);
        if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            return finish(result);
        receipt.root = context.root;
        receipt.modelInfo = model.nativeModelInfo;
#if defined(MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_MODEL_CLASSIFICATION_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &ClassificationDetail::NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + receipt.entryRva), modelIndex, receipt.nativeResult, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if (receipt.nativeResult > 1)
            return finish(ClockResult::BindingChanged);
        if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
            return finish(result);
        if ((result = owner()) != ClockResult::Ok || (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
            return finish(result);
        if (!ClassificationDetail::Profile(image) || !NativeImageAccess::ReadImage(image, model.slotAddress, &currentModel, sizeof(currentModel), true) ||
            currentModel != model.nativeModelInfo)
            return finish(ClockResult::BindingChanged);
        if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            return finish(result);
        output = receipt.nativeResult != 0;
        return finish(ClockResult::Ok);
    }
}
