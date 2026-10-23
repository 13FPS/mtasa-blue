/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Serialize one owned pointer effect without retaining a native entity
 ****************************************************************************/
#include "NativeWorldIgnoredEntityMemory.h"
#include "NativeWorldIgnoredEntityProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeStartGameCalls.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
namespace mta::game_de::NativeWorldIgnoredEntityMemory
{
    namespace
    {
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeWorldIgnoredEntityProfile;
        bool Profile(std::uintptr_t image) noexcept
        {
            if (!NativePedPoolReader::Revalidate({image}) || !D::Profile(image))
                return false;
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
            const auto                           sectionBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                sectionBytes > NativePedPoolProfile::ImageSize - at ||
                !NativeImageAccess::ReadImage(image, image + at, sections.data(), nt.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER)))
                return false;
            bool data = false;
            for (std::size_t i = 0; i < nt.FileHeader.NumberOfSections; ++i)
                if (NativeImageAccess::SectionContains(sections[i], P::GlobalRva, P::Bytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE))
                    data = true;
            if (!data || !NativeImageAccess::ImagePages(image, image + P::GlobalRva, P::Bytes, true))
                return false;
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        ClockResult RecoveryOwner(std::uintptr_t image, CheckOwner owner) noexcept
        {
            const auto result = owner();
            return result == ClockResult::Ok ? NativeStartGameCalls::CheckThread(image) : result;
        }
        struct Context
        {
            D::Value          graph;
            NativePedMetadata metadata;
        };
        ClockResult Capture(std::uintptr_t image, const Ped* ped, CheckOwner owner, bool recovery, Context& value) noexcept
        {
            auto result = recovery ? RecoveryOwner(image, owner) : D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            if (recovery)
                return RecoveryOwner(image, owner);
            if ((result = D::Capture(image, value.graph)) != ClockResult::Ok)
                return result;
            if (ped)
            {
                if ((result = NativePedMetadataMemory({image}, owner).Read(*ped, value.metadata)) != ClockResult::Ok ||
                    (result = NativePedPoolReader({image}, owner).Recheck(*ped)) != ClockResult::Ok)
                    return result;
                if (value.metadata.entityType != 3)
                    return ClockResult::BindingChanged;
            }
            if (!D::Fence(image, value.graph) || !Profile(image))
                return ClockResult::BindingChanged;
            return D::Owner(image, owner);
        }
        bool Same(const Context& a, const Context& b, bool ped, bool mode) noexcept
        {
            return D::Same(a.graph, b.graph, mode) &&
                   (!ped || (a.metadata.modelIndex == b.metadata.modelIndex && a.metadata.entityType == b.metadata.entityType));
        }
        ClockResult NativeExchange(std::uintptr_t address, std::uintptr_t expected, std::uintptr_t desired, std::uintptr_t& observed,
                                   std::uint32_t& exception) noexcept
        {
            __try
            {
                observed = reinterpret_cast<std::uintptr_t>(InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile*>(address), reinterpret_cast<void*>(desired), reinterpret_cast<void*>(expected)));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult Apply(std::uintptr_t image, std::uintptr_t expected, std::uintptr_t alternate, const Ped* source, CheckOwner owner, bool recovery,
                          Receipt* receiptOutput, CompareExchange exchange) noexcept
        {
            const bool hasPed = source != nullptr;
            const Ped  ped = source ? *source : Ped{};
            Receipt    receipt;
            receipt.address = image + P::GlobalRva;
            receipt.expected = expected;
            receipt.desired = hasPed ? ped.nativePed : 0;
            const auto finish = [&](ClockResult result)
            {
                if (receiptOutput)
                    *receiptOutput = receipt;
                return result;
            };
            if (!owner)
                return finish(ClockResult::NotInitialized);
            if ((hasPed && !ped.nativePed) || (recovery && ((!expected && !alternate) || hasPed)))
                return finish(ClockResult::InvalidArgument);
            if (!D::TryEnter())
                return finish(ClockResult::Busy);
            D::Scope scope;
            Context  first, current;
            auto     result = Capture(image, hasPed ? &ped : nullptr, owner, recovery, first);
            if (result != ClockResult::Ok)
                return finish(result);
            if (!NativeImageAccess::ReadImage(image, receipt.address, &receipt.before, sizeof(receipt.before), true))
                return finish(ClockResult::ReadFailure);
            if ((result = Capture(image, hasPed ? &ped : nullptr, owner, recovery, current)) != ClockResult::Ok)
                return finish(result);
            if (!recovery && !Same(first, current, hasPed, true))
                return finish(ClockResult::BindingChanged);
            if (!NativeImageAccess::ReadImage(image, receipt.address, &receipt.after, sizeof(receipt.after), true))
                return finish(ClockResult::ReadFailure);
            if (receipt.after != receipt.before)
                return finish(ClockResult::OwnerConflict);
            // Never adopt a foreign nonzero baseline. The journal may find its
            // own value already cleared; that needs acknowledgement, not replay.
            const bool alreadyClear = !receipt.desired && !receipt.before;
            if (recovery && receipt.before && receipt.before == alternate)
                receipt.expected = receipt.before;
            const auto comparand = receipt.expected;
            const bool foreignAbsent = recovery && receipt.before && receipt.before != comparand;
            if (!alreadyClear && !foreignAbsent && receipt.before != comparand)
                return finish(ClockResult::OwnerConflict);
            if (!alreadyClear && !foreignAbsent)
            {
                receipt.attempted = true;
                result = (exchange ? exchange : &NativeExchange)(receipt.address, comparand, receipt.desired, receipt.observed, receipt.exceptionCode);
                if (result != ClockResult::Ok)
                    return finish(result);
                receipt.applied = receipt.observed == comparand && receipt.desired != comparand;
                if (receipt.observed != comparand && (receipt.desired || receipt.observed))
                    return finish(ClockResult::OwnerConflict);
            }
            if ((result = Capture(image, hasPed ? &ped : nullptr, owner, recovery, current)) != ClockResult::Ok)
                return finish(result);
            if (!recovery && !Same(first, current, hasPed, false))
                return finish(ClockResult::BindingChanged);
            if (!NativeImageAccess::ReadImage(image, receipt.address, &receipt.after, sizeof(receipt.after), true))
                return finish(ClockResult::ReadFailure);
            // A stable foreign replacement is positive evidence that neither
            // journal candidate occupies this one global. Retire our debt
            // without writing, while moving/unreadable state still refuses.
            if (receipt.after != (foreignAbsent ? receipt.before : receipt.desired))
                return finish(ClockResult::OwnerConflict);
            result = recovery ? RecoveryOwner(image, owner) : D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.foreignReplacement = foreignAbsent;
            receipt.acknowledged = true;
            return finish(ClockResult::Ok);
        }
    }
    ClockResult Exchange(std::uintptr_t image, std::uintptr_t expected, const Ped* ped, CheckOwner owner, Receipt* receipt, CompareExchange exchange) noexcept
    {
        return Apply(image, expected, 0, ped, owner, false, receipt, exchange);
    }
    ClockResult ClearOwned(std::uintptr_t image, std::uintptr_t committed, std::uintptr_t pending, CheckOwner recovery, Receipt* receipt,
                           CompareExchange exchange) noexcept
    {
        return Apply(image, committed, pending, nullptr, recovery, true, receipt, exchange);
    }
}
