/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse genuine model classifier semantics with current owner bookends
 ****************************************************************************/
#include "NativeModelClassificationCalls.h"
#include "NativeModelVehicleNamedProfile.h"
#include "NativeModelTimeProfile.h"
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
        bool NamedProfile(std::uintptr_t image) noexcept
        {
            namespace Named = NativeModelVehicleNamedProfile;
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!NativeImageAccess::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !NativeImageAccess::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
                nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                bytes > NativePedPoolProfile::ImageSize - at || !NativeImageAccess::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            for (const auto& pin : Named::Pins)
            {
                const bool code = pin.rva == Named::ProducerRva;
                bool       contained = false;
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    if (NativeImageAccess::SectionContains(sections[index], pin.rva, pin.size, IMAGE_SCN_MEM_READ | (code ? IMAGE_SCN_MEM_EXECUTE : 0),
                                                           IMAGE_SCN_MEM_WRITE | (code ? 0 : IMAGE_SCN_MEM_EXECUTE)))
                        contained = true;
                std::array<unsigned char, Named::MaximumPinBytes> actual{};
                if (!contained || !pin.size || pin.size > actual.size() ||
                    !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, code) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        ClockResult ReadNamed(std::uintptr_t image, std::uint32_t modelIndex, std::uint32_t expectedSubtype, CheckOwner owner, bool& output,
                              Receipt& receipt) noexcept
        {
            receipt.policyPort = true;
            if (!owner)
                return ClockResult::NotInitialized;
            NativeModelBankReader        bank({image}, owner);
            NativeModelBankReader::Value model;
            auto                         result = bank.Read(modelIndex, model);
            if (result != ClockResult::Ok)
                return result;
            if (!NamedProfile(image))
                return ClockResult::UnsupportedBuild;
            receipt.modelInfo = model.nativeModelInfo;
            if (!model.nativeModelInfo)
            {
                // Actual classic predicates return false for an absent row.
                // Close the null bank word after all fallible owner callbacks.
                if ((result = bank.Recheck(model)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok)
                    return result;
                std::uintptr_t current = 1;
                if (!NativeModelBankReader::Revalidate({image}) || !NamedProfile(image) ||
                    !NativeImageAccess::ReadImage(image, model.slotAddress, &current, sizeof(current), true) || current)
                    return ClockResult::BindingChanged;
                std::uint32_t state = 0;
                if (!NativeGameStateMemory::Read(image, state))
                    return ClockResult::BindingChanged;
                if (state != 9)
                    return ClockResult::NotInitialized;
                if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                    return result;
                output = false;
                return ClockResult::Ok;
            }
            NativeStreamingModelContext::Value context;
            if ((result = NativeStreamingModelContext::Capture(image, model, context)) != ClockResult::Ok)
                return result;
            receipt.root = context.root;
            std::array<unsigned char, 6> before{}, after{};
            std::uint8_t                 firstType = 0, secondType = 0;
            if (!NativeModelTypeLeaf::Read(image, context.typeMethod, before, firstType))
                return ClockResult::UnsupportedBuild;
            std::uint32_t first = 0, second = 0;
            // Type AL6 is the source prerequisite for touching DWORD+84.
            // No archive lookup, loaded check or low-byte conversion belongs
            // to these named predicate policies.
            if (firstType == 6 && !VehicleField(model.nativeModelInfo, first))
                return ClockResult::ReadFailure;
            if ((result = bank.Recheck(model)) != ClockResult::Ok ||
                (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok ||
                (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
                return result;
            // Finish every callback before these final byte/data bookends.
            // The two observations stay coupled to the very same bank row,
            // metadata vtable/provider/membership and pure native type leaf.
            if (!NativeModelBankReader::Revalidate({image}) || !NamedProfile(image) ||
                !NativeModelTypeLeaf::Read(image, context.typeMethod, after, secondType) || before != after || secondType != firstType)
                return ClockResult::BindingChanged;
            if (firstType == 6 && !VehicleField(model.nativeModelInfo, second))
                return ClockResult::ReadFailure;
            if (first != second)
                return ClockResult::BindingChanged;
            if ((result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
                return result;
            std::uintptr_t current = 0;
            if (!NativeImageAccess::ReadImage(image, model.slotAddress, &current, sizeof(current), true) || current != model.nativeModelInfo ||
                !NativeStreamingModelContext::MembershipUnchanged(image, context) || !NamedProfile(image))
                return ClockResult::BindingChanged;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(image, state))
                return ClockResult::BindingChanged;
            if (state != 9)
                return ClockResult::NotInitialized;
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return result;
            receipt.nativeResult = firstType == 6 && first == expectedSubtype ? 1 : 0;
            output = receipt.nativeResult != 0;
            return ClockResult::Ok;
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

    namespace TimeDetail
    {
        namespace T = NativeModelTimeProfile;
        bool Profile(std::uintptr_t image) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!NativeImageAccess::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !NativeImageAccess::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
                nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                bytes > NativePedPoolProfile::ImageSize - at || !NativeImageAccess::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                    if (NativeImageAccess::SectionContains(sections[i], rva, size, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(T::PoolRva, T::PoolCapacity * T::RowBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(T::PoolCountRva, sizeof(std::int32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(T::VtableRva, sizeof(T::VtableMethods), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : T::Pins)
            {
                std::array<unsigned char, T::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() ||
                    !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                    !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            for (std::size_t i = 0; i != sizeof(T::VtableMethods) / sizeof(T::VtableMethods[0]); ++i)
            {
                std::uintptr_t pointer = 0;
                if (!NativeImageAccess::ReadImage(image, image + T::VtableRva + i * sizeof(pointer), &pointer, sizeof(pointer)) ||
                    pointer != image + T::VtableMethods[i])
                    return false;
            }
            return NativeStreamingModelContext::QualifyProfile(image);
        }
        bool Tuple(std::uintptr_t image, std::uintptr_t model, std::int32_t& count, std::array<unsigned char, 4>& tuple) noexcept
        {
            const auto base = image + T::PoolRva;
            if (model < base || model - base >= T::PoolCapacity * T::RowBytes || (model - base) % T::RowBytes ||
                !NativeImageAccess::ReadImage(image, image + T::PoolCountRva, &count, sizeof(count), true) || count <= 0 ||
                count > static_cast<std::int32_t>(T::PoolCapacity) || (model - base) / T::RowBytes >= static_cast<std::size_t>(count))
                return false;
            std::uintptr_t vtable = 0, method = 0;
            if (!NativeImageAccess::ReadImage(image, model, &vtable, sizeof(vtable), true) || vtable != image + T::VtableRva ||
                !NativeImageAccess::ReadImage(image, vtable + T::TimeSlot, &method, sizeof(method)) || method != image + T::TimeMethodRva)
                return false;
            return NativeImageAccess::ReadImage(image, model + T::TimeOffset, tuple.data(), tuple.size(), true);
        }
    }
    ClockResult ReadTime(std::uintptr_t image, std::uint32_t modelIndex, CheckOwner owner, NativeModelTimeObservation& output) noexcept
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
        if (!TimeDetail::Profile(image))
            return ClockResult::UnsupportedBuild;
        NativeModelTimeObservation         next;
        NativeStreamingModelContext::Value context;
        std::array<unsigned char, 6>       before{}, after{};
        unsigned char                      type = 0, confirmed = 0;
        std::array<unsigned char, 4>       tuple{}, tupleAfter{};
        std::int32_t                       count = 0, countAfter = 0;
        if (model.nativeModelInfo)
        {
            if ((result = NativeStreamingModelContext::Capture(image, model, context)) != ClockResult::Ok)
                return result;
            if (!NativeModelTypeLeaf::Read(image, context.typeMethod, before, type))
                return ClockResult::UnsupportedBuild;
            if (type == 3)
            {
                // A second type3 table has a different accessor. Admit only
                // the separately named current169-row producer and pure slot38;
                // neither type alone nor an arbitrary metadata pointer suffices.
                if (context.modelVtable != image + NativeModelTimeProfile::VtableRva || context.typeMethod != image + NativeModelTimeProfile::TypeMethodRva ||
                    !TimeDetail::Tuple(image, model.nativeModelInfo, count, tuple))
                    return ClockResult::UnsupportedBuild;
                next = {true, tuple[0], tuple[1]};
            }
        }
        if ((result = bank.Recheck(model)) != ClockResult::Ok ||
            (model.nativeModelInfo && (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok) ||
            (result = owner()) != ClockResult::Ok ||
            (model.nativeModelInfo && (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok))
            return result;
        // Finish all owner callbacks before closing code/count/tuple/bank facts.
        // Publish no partial tuple when a current identity or paired byte moved.
        if (!NativeModelBankReader::Revalidate({image}) || !TimeDetail::Profile(image))
            return ClockResult::BindingChanged;
        if (model.nativeModelInfo && (!NativeModelTypeLeaf::Read(image, context.typeMethod, after, confirmed) || before != after || type != confirmed))
            return ClockResult::BindingChanged;
        if (next.hasTime && (!TimeDetail::Tuple(image, model.nativeModelInfo, countAfter, tupleAfter) || count != countAfter || tuple != tupleAfter))
            return ClockResult::BindingChanged;
        if (model.nativeModelInfo && (result = NativeStreamingModelContext::Recheck(image, model, context)) != ClockResult::Ok)
            return result;
        std::uintptr_t current = 0;
        if (!NativeImageAccess::ReadImage(image, model.slotAddress, &current, sizeof(current), true) || current != model.nativeModelInfo ||
            (model.nativeModelInfo && !NativeStreamingModelContext::MembershipUnchanged(image, context)) || !TimeDetail::Profile(image))
            return ClockResult::BindingChanged;
        std::uint32_t state = 0;
        if (!NativeGameStateMemory::Read(image, state))
            return ClockResult::BindingChanged;
        if (state != 9)
            return ClockResult::NotInitialized;
        if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            return result;
        output = next;
        return ClockResult::Ok;
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
        // Named policies use a separate independently qualified producer; this
        // raw observation itself introduces no enum interpretation.
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
        std::uint32_t namedSubtype = 0;
        if (NativeModelVehicleNamedProfile::Find(predicate, namedSubtype))
            return finish(ClassificationDetail::ReadNamed(image, modelIndex, namedSubtype, owner, output, receipt));
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
