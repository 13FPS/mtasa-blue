/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh typed wheel tuple reads and one guarded native metadata CAS
 *****************************************************************************/
#include "NativeModelWheelMemory.h"
#include "NativeModelWheelProfile.h"
#include "NativeModelTypeLeaf.h"
#include "NativeImageAccess.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeModelWheelMemory
{
    namespace Detail
    {
        namespace P = NativeModelWheelProfile;
        namespace I = NativeImageAccess;
        bool TuplePages(std::uintptr_t image, std::uintptr_t at) noexcept
        {
            if (!at || at % alignof(std::uint64_t) || at > (std::numeric_limits<std::uintptr_t>::max)() - P::TupleBytes)
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || (page.Protect & 0xff) != PAGE_READWRITE ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED && page.Type != MEM_IMAGE) ||
                (page.Type == MEM_IMAGE && reinterpret_cast<std::uintptr_t>(page.AllocationBase) != image))
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            return begin <= at && page.RegionSize >= P::TupleBytes && at - begin <= page.RegionSize - P::TupleBytes;
        }
        bool Tuple(std::uintptr_t image, std::uintptr_t model, std::array<std::uint32_t, 2>& output) noexcept
        {
            if (!model || model > (std::numeric_limits<std::uintptr_t>::max)() - P::FrontOffset || !TuplePages(image, model + P::FrontOffset))
                return false;
            std::array<std::uint32_t, 2> next{};
            SIZE_T                       copied = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(model + P::FrontOffset), next.data(), sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!I::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !I::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                sizeof(IMAGE_SECTION_HEADER) * nt.FileHeader.NumberOfSections > NativePedPoolProfile::ImageSize - at ||
                !I::ReadImage(image, image + at, sections.data(), sizeof(IMAGE_SECTION_HEADER) * nt.FileHeader.NumberOfSections))
                return false;
            const auto pin = [&](const P::Pin& item, bool code) noexcept
            {
                bool contained = false;
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (I::SectionContains(sections[index], item.rva, item.size, IMAGE_SCN_MEM_READ | (code ? IMAGE_SCN_MEM_EXECUTE : 0),
                                           IMAGE_SCN_MEM_WRITE | (code ? 0 : IMAGE_SCN_MEM_EXECUTE)))
                        contained = true;
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                return contained && item.size && item.size <= actual.size() && I::ReadImage(image, image + item.rva, actual.data(), item.size, false, code) &&
                       !std::memcmp(actual.data(), item.bytes, item.size);
            };
            for (const auto& item : P::Pins)
                if (!pin(item, item.code))
                    return false;
            bool vtableContained = false;
            for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                if (I::SectionContains(sections[index], P::VtableRva, P::VtableBytes, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                    vtableContained = true;
            std::array<std::uintptr_t, P::VtableMethods.size()> methods{};
            static_assert(sizeof(std::uintptr_t) == sizeof(std::uint64_t));
            if (!vtableContained || !I::ReadImage(image, image + P::VtableRva, methods.data(), sizeof(methods)))
                return false;
            for (std::size_t index = 0; index < methods.size(); ++index)
                if (methods[index] != image + P::VtableMethods[index])
                    return false;
            return true;
        }
        bool Pool(std::uintptr_t image, const Value& value, std::int32_t& count) noexcept
        {
            if (!I::ImagePages(image, image + P::PoolRva, P::PoolBytes, true) || !I::ReadImage(image, image + P::PoolCountRva, &count, sizeof(count), true) ||
                count <= 0 || count > static_cast<std::int32_t>(P::PoolCapacity) || value.model.nativeModelInfo < image + P::PoolRva)
                return false;
            const auto delta = value.model.nativeModelInfo - (image + P::PoolRva);
            return delta % P::PoolStride == 0 && delta / P::PoolStride < static_cast<std::uint32_t>(count) &&
                   value.context.modelVtable == image + P::VtableRva && value.context.typeMethod == image + P::TypeGetterRva &&
                   value.context.animationMethod == image + P::VtableMethods[15];
        }
        bool Anchors(std::uintptr_t image, const Value& value, const std::array<std::uint32_t, 2>& expected) noexcept
        {
            // Finish every caller callback before these current raw anchors.
            // No captured row is promoted to a lifetime lease across frames.
            std::uintptr_t current = 0;
            if (!NativeModelBankReader::Revalidate({image}) || !I::ReadImage(image, value.model.slotAddress, &current, sizeof(current), true) ||
                current != value.model.nativeModelInfo)
                return false;
            if (current)
            {
                if (NativeStreamingModelContext::Recheck(image, value.model, value.context) != ClockResult::Ok)
                    return false;
                std::array<unsigned char, 6> bytes{};
                std::uint8_t                 type = 0;
                if (!NativeModelTypeLeaf::Read(image, value.context.typeMethod, bytes, type) || bytes != value.typeBytes || (type == 6) != value.vehicle)
                    return false;
                if (value.vehicle)
                {
                    std::array<std::uint32_t, 2> observed{};
                    std::int32_t                 count = 0;
                    if (!Profile(image) || !Pool(image, value, count) || count != value.poolCount || !Tuple(image, current, observed) || observed != expected)
                        return false;
                }
            }
            std::uint32_t state = 0;
            return NativeGameStateMemory::Read(image, state) && state == 9 && NativeStartGameCalls::CheckThread(image) == ClockResult::Ok;
        }
        bool ExchangeTuple(std::uintptr_t at, std::uint64_t expected, std::uint64_t desired, std::uint64_t& observed, std::uint32_t& exception) noexcept
        {
            __try
            {
                LONG64 from = 0, to = 0;
                std::memcpy(&from, &expected, sizeof(from));
                std::memcpy(&to, &desired, sizeof(to));
                const LONG64 result = InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(at), to, from);
                std::memcpy(&observed, &result, sizeof(observed));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
        }
    }
    bool SameIdentity(const Value& left, const Value& right) noexcept
    {
        return left.generation == right.generation && left.model.modelIndex == right.model.modelIndex && left.model.slotAddress == right.model.slotAddress &&
               left.model.nativeModelInfo == right.model.nativeModelInfo && left.vehicle == right.vehicle && left.poolCount == right.poolCount &&
               left.typeBytes == right.typeBytes && NativeStreamingModelContext::Same(left.context, right.context) &&
               left.context.rootMembership.index == right.context.rootMembership.index &&
               left.context.rootMembership.serial == right.context.rootMembership.serial &&
               left.context.providerMembership.index == right.context.providerMembership.index &&
               left.context.providerMembership.serial == right.context.providerMembership.serial;
    }
    Origin Identity(const Value& value) noexcept
    {
        Origin origin;
        origin.generation = value.generation;
        origin.model = value.model.modelIndex;
        origin.slot = value.model.slotAddress;
        origin.metadata = value.model.nativeModelInfo;
        origin.vtable = value.context.modelVtable;
        origin.typeMethod = value.context.typeMethod;
        origin.root = value.context.root;
        origin.provider = value.context.provider;
        origin.rows = value.context.rows;
        origin.count = value.context.count;
        origin.rootIndex = value.context.rootMembership.index;
        origin.rootSerial = value.context.rootMembership.serial;
        origin.providerIndex = value.context.providerMembership.index;
        origin.providerSerial = value.context.providerMembership.serial;
        origin.typeBytes = value.typeBytes;
        return origin;
    }
    bool SameIdentity(const Origin& origin, const Value& current) noexcept
    {
        const auto next = Identity(current);
        return current.vehicle && origin.generation == next.generation && origin.model == next.model && origin.slot == next.slot &&
               origin.metadata == next.metadata && origin.vtable == next.vtable && origin.typeMethod == next.typeMethod && origin.root == next.root &&
               origin.provider == next.provider && origin.rows == next.rows && origin.count == next.count && origin.rootIndex == next.rootIndex &&
               origin.rootSerial == next.rootSerial && origin.providerIndex == next.providerIndex && origin.providerSerial == next.providerSerial &&
               origin.typeBytes == next.typeBytes;
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner, CheckOwner afterApplication,
                        Value& output) noexcept
    {
        if (!image || !generation || !owner || !afterApplication)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        Value next;
        next.generation = generation;
        NativeModelBankReader bank({image}, owner);
        if ((result = bank.Read(model, next.model)) != ClockResult::Ok)
            return result;
        if (next.model.nativeModelInfo)
        {
            if ((result = NativeStreamingModelContext::Capture(image, next.model, next.context)) != ClockResult::Ok)
                return result;
            std::uint8_t type = 0;
            if (!NativeModelTypeLeaf::Read(image, next.context.typeMethod, next.typeBytes, type))
                return ClockResult::UnsupportedBuild;
            next.vehicle = type == 6;
            if (next.vehicle)
            {
                if (!Detail::Profile(image))
                    return ClockResult::UnsupportedBuild;
                if (!Detail::Pool(image, next, next.poolCount))
                    return ClockResult::BindingChanged;
                if (!Detail::Tuple(image, next.model.nativeModelInfo, next.bits))
                    return ClockResult::ReadFailure;
            }
        }
        if ((result = bank.Recheck(next.model)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        if (!Detail::Anchors(image, next, next.bits))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult Write(std::uintptr_t image, const Value& supplied, const std::array<std::uint32_t, 2>& desired, CheckOwner owner, CheckOwner afterApplication,
                      Receipt& output, Exchange exchange) noexcept
    {
        Receipt receipt;
        receipt.before = supplied;
        receipt.desired = desired;
        const auto finish = [&](ClockResult result) noexcept
        {
            receipt.result = result;
            output = receipt;
            return result;
        };
        if (!supplied.vehicle)
            return finish(ClockResult::InvalidArgument);
        Value current;
        auto  result = Capture(image, supplied.generation, supplied.model.modelIndex, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!SameIdentity(supplied, current) || supplied.bits != current.bits)
            return finish(ClockResult::BindingChanged);
        std::uint64_t before = 0, target = 0, observed = 0;
        static_assert(sizeof(before) == sizeof(current.bits));
        std::memcpy(&before, current.bits.data(), sizeof(before));
        std::memcpy(&target, desired.data(), sizeof(target));
        receipt.entered = true;
        const bool returned = (exchange ? exchange : Detail::ExchangeTuple)(current.model.nativeModelInfo + NativeModelWheelProfile::FrontOffset, before,
                                                                            target, observed, receipt.exceptionCode);
        std::memcpy(receipt.observed.data(), &observed, sizeof(observed));
        if (!returned)
        {
            receipt.uncertain = true;
            return finish(ClockResult::OriginalException);
        }
        receipt.accepted = observed == before;
        if (!receipt.accepted)
            return finish(ClockResult::OwnerConflict);
        Value after;
        result = Capture(image, supplied.generation, supplied.model.modelIndex, owner, afterApplication, after);
        if (result == ClockResult::Ok && (!SameIdentity(current, after) || after.bits != desired))
            result = ClockResult::BindingChanged;
        if (result != ClockResult::Ok)
        {
            receipt.uncertain = true;
            return finish(result);
        }
        return finish(ClockResult::Ok);
    }
}
