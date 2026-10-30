/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve matched MTA RemoveAnimBlock order in its proven empty domain
 ****************************************************************************/
#include "NativeAnimationBlockRemoval.h"
#include "NativeAnimationManagerMemory.h"
#include "NativeAnimationManagerProfile.h"
#include "NativeAnimationBlockProfile.h"
#include "NativeAnimationBlockRefProfile.h"
#include "NativeAnimationGroupArrayProfile.h"
#include "NativeAnimationStaticAssociationProfile.h"
#include "NativeStreamingRemoveProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include "NativeImageAccess.h"
#include "NativeModelReferenceMap.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace mta::game_de
{
    namespace AnimationBlockRemovalDetail
    {
        namespace Block = NativeAnimationBlockProfile;
        namespace Group = NativeAnimationGroupArrayProfile;
        namespace Image = NativeImageAccess;
        namespace Memory = NativeModelReferenceMap;
        namespace Context = NativeStreamingCompletionContext;
        using CheckOwner = NativeAnimationBlockRemovalCalls::CheckOwner;
        using Store = NativeAnimationBlockRemovalCalls::Store;
        using Receipt = NativeAnimationBlockRemovalCalls::Receipt;
        using Layout = NativeAnimationManagerMemory::AssociationGroupReplacementLayout;
        using GroupRow = std::array<unsigned char, Group::RowBytes>;
        // An observation budget, never an assertion of native group capacity.
        constexpr std::int32_t   MaximumGroups = 65536;
        constexpr std::uintptr_t RetireRva = 0xfeb580, ArmRva = 0x12891c6;
        constexpr std::size_t    ArmOffset = ArmRva - NativeStreamingRemoveProfile::RemoveRva, ArmBytes = 0x1289287 - ArmRva;
        static_assert(Block::RowBytes == 32 && Group::RowBytes == 32);
        static_assert(ArmOffset + ArmBytes <= NativeStreamingRemoveProfile::Remove.size());

        template <class T, class Bytes>
        T Field(const Bytes& bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        ClockResult Owner(std::uintptr_t image, CheckOwner owner, CheckOwner postFrameOwner) noexcept
        {
            if (!owner || !postFrameOwner)
                return ClockResult::NotInitialized;
            auto result = postFrameOwner();
            if (result != ClockResult::Ok || (result = owner()) != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(image, state))
                return ClockResult::BindingChanged;
            return state == 9 ? postFrameOwner() : ClockResult::NotInitialized;
        }
        bool Sections(std::uintptr_t image, std::size_t bankBytes) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !Image::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                bytes > NativePedPoolProfile::ImageSize - at || !Image::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            constexpr DWORD rw = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, rx = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            if (!bankBytes || !contains(Block::BankRva, bankBytes, rw, IMAGE_SCN_MEM_EXECUTE) || !contains(Block::CountRva, 4, rw, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(Group::CountRva, 4, rw, IMAGE_SCN_MEM_EXECUTE) || !contains(Group::GlobalRva, 8, rw, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(ArmRva, ArmBytes, rx, IMAGE_SCN_MEM_WRITE) ||
                !contains(RetireRva, NativeAnimationStaticAssociationProfile::Retire.size(), rx, IMAGE_SCN_MEM_WRITE))
                return false;
            for (const auto& pin : Group::Code)
                if (!contains(pin.rva, pin.size, rx, IMAGE_SCN_MEM_WRITE))
                    return false;
            return true;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            if (!NativeAnimationManagerMemory::Revalidate({image}))
                return false;
            for (const auto& pin : Group::Code)
            {
                std::array<unsigned char, Group::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            std::array<unsigned char, NativeAnimationStaticAssociationProfile::Retire.size()> retire{};
            std::array<unsigned char, ArmBytes>                                               arm{};
            return Image::ReadImage(image, image + RetireRva, retire.data(), retire.size(), false, true) &&
                   retire == NativeAnimationStaticAssociationProfile::Retire && Image::ReadImage(image, image + ArmRva, arm.data(), arm.size(), false, true) &&
                   !std::memcmp(arm.data(), NativeStreamingRemoveProfile::Remove.data() + ArmOffset, arm.size());
        }
        bool PrivateSpan(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!Memory::PrivateWritable(address, bytes))
                return false;
            const auto end = address + bytes;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) || page.Type != MEM_PRIVATE || !page.RegionSize)
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
                if (begin > address || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= address)
                    return false;
                address = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        ClockResult InvokeOriginal(NativeAnimationBlockRemovalCalls::GroupFunction entry, std::uintptr_t group, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(group);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult WriteStores(const Store* stores, std::size_t count, std::uint32_t& exception) noexcept
        {
            if (!stores || !count || count > 2 || stores[0].bytes != 1 || (count == 2 && stores[1].bytes != 2))
                return ClockResult::InvalidArgument;
            __try
            {
                // Preserve source widths and order. A lost acknowledgement of
                // either store retains the journal instead of replaying it.
                *reinterpret_cast<volatile std::uint8_t*>(stores[0].address) = 0;
                if (count == 2)
                    *reinterpret_cast<volatile std::uint16_t*>(stores[1].address) = 0;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        struct BusyScope
        {
            std::atomic<bool>& busy;
            ~BusyScope() { busy.store(false, std::memory_order_release); }
        };
    }
    struct NativeAnimationBlockRemoval::Data
    {
        NativeAnimationBlockIdentity                                                 identity{};
        NativeAnimationManagerCounts                                                 counts{};
        NativeAnimationBlockRemovalCalls::Receipt                                    receipt{};
        NativeAnimationManagerMemory::AssociationGroupReplacementLayout              anchor{};
        std::uintptr_t                                                               groups = 0;
        std::uint64_t                                                                groupCapacity = 0;
        std::int32_t                                                                 groupCount = 0;
        std::vector<unsigned char>                                                   before, expected, observed;
        std::vector<AnimationBlockRemovalDetail::GroupRow>                           groupRows, observedGroups;
        std::vector<NativeAnimationManagerMemory::AssociationGroupReplacementLayout> matches;
        std::array<NativeAnimationBlockRemovalCalls::Store, 2>                       stores{};
    };
    namespace AnimationBlockRemovalDetail
    {
        template <class Plan>
        ClockResult Raw(std::uintptr_t image, Plan& plan, bool after) noexcept
        {
            if (!Sections(image, plan.before.size()) || !Profile(image))
                return ClockResult::BindingChanged;
            std::int32_t   blocks = 0, animations = 0, count = 0;
            std::uintptr_t groups = 0;
            if (!Image::ReadImage(image, image + Block::CountRva, &blocks, 4, true) ||
                !Image::ReadImage(image, image + NativeAnimationManagerProfile::AnimationCountRva, &animations, 4, true) ||
                !Image::ReadImage(image, image + Group::CountRva, &count, 4, true) || !Image::ReadImage(image, image + Group::GlobalRva, &groups, 8, true))
                return ClockResult::ReadFailure;
            if (blocks != plan.counts.blocks || animations != plan.counts.animations || count != plan.groupCount || groups != plan.groups)
                return ClockResult::BindingChanged;
            if (!Image::ReadImage(image, image + Block::BankRva, plan.observed.data(), plan.observed.size(), true))
                return ClockResult::ReadFailure;
            if (plan.observed != (after ? plan.expected : plan.before))
                return ClockResult::BindingChanged;
            if (plan.groupCount)
            {
                const auto    bytes = Group::CookieBytes + static_cast<std::size_t>(plan.groupCapacity) * Group::RowBytes;
                std::uint64_t cookie = 0;
                if (!PrivateSpan(groups - Group::CookieBytes, bytes) || !Memory::Copy(groups - Group::CookieBytes, &cookie, sizeof(cookie)) ||
                    !Memory::Copy(groups, plan.observedGroups.data(), plan.observedGroups.size() * Group::RowBytes))
                    return ClockResult::ReadFailure;
                if (cookie != plan.groupCapacity || plan.observedGroups != plan.groupRows)
                    return ClockResult::BindingChanged;
            }
            return ClockResult::Ok;
        }
        template <class Plan>
        ClockResult Verify(std::uintptr_t image, CheckOwner owner, CheckOwner postFrameOwner, Plan& plan, bool after) noexcept
        {
            auto result = Owner(image, owner, postFrameOwner);
            if (result != ClockResult::Ok || (result = Raw(image, plan, after)) != ClockResult::Ok)
                return result;
            NativeAnimationManagerMemory memory({image}, owner);
            if (plan.groupCount && (result = memory.RecheckAssociationGroupReplacementLayout(plan.anchor)) != ClockResult::Ok)
                return result;
            for (const auto& match : plan.matches)
                if ((result = memory.RecheckAssociationGroupReplacementLayout(match)) != ClockResult::Ok)
                    return result;
            if ((result = Owner(image, owner, postFrameOwner)) != ClockResult::Ok)
                return result;
            // Owner callbacks precede this final callback-free byte bookend;
            // they cannot redirect a later native call to unchecked children.
            return Raw(image, plan, after);
        }
        template <class Plan>
        ClockResult Prepare(std::uintptr_t image, CheckOwner owner, CheckOwner postFrameOwner, Plan& plan, Receipt& receipt)
        {
            auto result = Owner(image, owner, postFrameOwner);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationManagerMemory memory({image}, owner);
            if ((result = memory.ReadCounts(plan.counts)) != ClockResult::Ok)
                return result;
            if (plan.counts.blocks <= 0 || static_cast<std::uint32_t>(plan.counts.blocks) > Block::ObservationLimit ||
                plan.identity.index >= static_cast<std::uint32_t>(plan.counts.blocks))
                return ClockResult::OwnerConflict;
            const auto bytes = static_cast<std::size_t>(plan.counts.blocks) * Block::RowBytes;
            if (!Sections(image, bytes) || !Profile(image))
                return ClockResult::BindingChanged;
            plan.before.resize(bytes);
            plan.observed.resize(bytes);
            if (!Image::ReadImage(image, image + Block::BankRva, plan.before.data(), plan.before.size(), true))
                return ClockResult::ReadFailure;
            const auto at = static_cast<std::size_t>(plan.identity.index) * Block::RowBytes;
            if (std::memcmp(plan.before.data() + at, plan.identity.name.data(), plan.identity.name.size()))
                return ClockResult::BindingChanged;
            // Zero is exact, not a signed nonpositive shortcut. No hierarchy
            // destructor is omitted for a block with actual animation rows.
            if (Field<std::uint32_t>(plan.before, at + Block::AnimationCountOffset))
                return ClockResult::OwnerConflict;
            plan.expected = plan.before;
            plan.expected[at + Block::LoadedOffset] = 0;
            plan.expected[at + NativeAnimationBlockRefProfile::RefOffset] = 0;
            plan.expected[at + NativeAnimationBlockRefProfile::RefOffset + 1] = 0;
            if (!Image::ReadImage(image, image + Group::CountRva, &plan.groupCount, 4, true) ||
                !Image::ReadImage(image, image + Group::GlobalRva, &plan.groups, 8, true))
                return ClockResult::ReadFailure;
            if (plan.groupCount < 0 || plan.groupCount > MaximumGroups || (!plan.groupCount && plan.groups))
                return ClockResult::OwnerConflict;
            if (plan.groupCount)
            {
                NativeAnimationAssociationGroupReferenceToken token{plan.identity.ownerGeneration, 0, plan.groups};
                if ((result = memory.ReadAssociationGroupReplacementLayout(token, plan.anchor)) != ClockResult::Ok)
                    return result;
                if (plan.anchor.groups != plan.groups || plan.anchor.groupCount != plan.groupCount ||
                    static_cast<std::uint64_t>(plan.groupCount) > plan.anchor.groupCapacity)
                    return ClockResult::BindingChanged;
                plan.groupCapacity = plan.anchor.groupCapacity;
                plan.groupRows.resize(static_cast<std::size_t>(plan.groupCount));
                plan.observedGroups.resize(plan.groupRows.size());
                plan.matches.reserve(plan.groupRows.size());
                if (!Memory::Copy(plan.groups, plan.groupRows.data(), plan.groupRows.size() * Group::RowBytes))
                    return ClockResult::ReadFailure;
                const auto block = image + Block::BankRva + at;
                for (std::uint32_t index = 0; index != static_cast<std::uint32_t>(plan.groupCount); ++index)
                {
                    const auto& row = plan.groupRows[index];
                    if (Field<std::uintptr_t>(row, 0) != block)
                        continue;
                    // FEB580's null-child branch returns without even the
                    // pointer/count clears. Require both already zero so no
                    // nonempty native retirement can be silently skipped.
                    if (Field<std::uintptr_t>(row, 8) || Field<std::uint32_t>(row, Group::CreatedOffset))
                        return ClockResult::OwnerConflict;
                    token.index = index;
                    token.locator = plan.groups + static_cast<std::uintptr_t>(index) * Group::RowBytes;
                    Layout match;
                    if ((result = memory.ReadAssociationGroupReplacementLayout(token, match)) != ClockResult::Ok)
                        return result;
                    if (match.parent != row || match.children || match.groupCapacity != plan.groupCapacity || match.groups != plan.groups ||
                        match.groupCount != plan.groupCount)
                        return ClockResult::BindingChanged;
                    plan.matches.push_back(match);
                }
            }
            const auto row = image + Block::BankRva + at;
            plan.stores = {{{row + Block::LoadedOffset, 1}, {row + NativeAnimationBlockRefProfile::RefOffset, 2}}};
            receipt.index = static_cast<std::int32_t>(plan.identity.index);
            receipt.blockCount = plan.counts.blocks;
            receipt.groupCount = plan.groupCount;
            receipt.matchingGroups = static_cast<std::uint32_t>(plan.matches.size());
            receipt.beforeLoaded = plan.before[at + Block::LoadedOffset];
            receipt.beforeRefs = Field<std::uint16_t>(plan.before, at + NativeAnimationBlockRefProfile::RefOffset);
            receipt.stores = static_cast<std::uint32_t>(plan.stores.size());
            return Verify(image, owner, postFrameOwner, plan, false);
        }
    }
    NativeAnimationBlockRemoval::NativeAnimationBlockRemoval() noexcept = default;
    NativeAnimationBlockRemoval::~NativeAnimationBlockRemoval() = default;
    ClockResult NativeAnimationBlockRemoval::RemoveEmpty(std::uintptr_t image, std::uint64_t generation, const NativeAnimationBlockIdentity& requested,
                                                         CheckOwner owner, CheckOwner postFrameOwner, Receipt* output, InvokeGroup invoke, Write write) noexcept
    {
        namespace Detail = AnimationBlockRemovalDetail;
        const auto request = requested;
        const auto unchanged = [&]
        { return requested.ownerGeneration == request.ownerGeneration && requested.index == request.index && requested.name == request.name; };
        Receipt receipt;
        receipt.generation = generation;
        const auto report = [&](ClockResult result)
        {
            receipt.debt = debt.load(std::memory_order_acquire);
            receipt.result = result;
            if (output)
                *output = receipt;
            return result;
        };
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - NativePedPoolProfile::ImageSize || !generation ||
            request.ownerGeneration != generation || request.index >= NativeAnimationBlockProfile::ObservationLimit || !owner || !postFrameOwner)
            return report(ClockResult::InvalidArgument);
        bool expected = false;
        if (!busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return report(ClockResult::Busy);
        Detail::BusyScope operation{busy};
        if (debt.load(std::memory_order_acquire))
        {
            if (data)
                receipt = data->receipt;
            return report(ClockResult::OwnerConflict);
        }
        const auto finish = [&](ClockResult result)
        {
            if (receipt.entered && result != ClockResult::Ok)
                debt.store(true, std::memory_order_release);
            receipt.debt = debt.load(std::memory_order_acquire);
            receipt.result = result;
            if (data)
                data->receipt = receipt;
            if (!receipt.debt)
                data.reset();
            return report(result);
        };
        if (!Detail::Context::TryEnter())
            return finish(ClockResult::Busy);
        Detail::Context::Scope gate;
        ClockResult            result;
        try
        {
            data = std::make_unique<Data>();
            data->identity = request;
            result = Detail::Prepare(image, owner, postFrameOwner, *data, receipt);
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        if (result != ClockResult::Ok)
            return finish(result);
        if (!unchanged())
            return finish(ClockResult::BindingChanged);
#if defined(MTA_DE_ANIMATION_BLOCK_REMOVAL_GROUP)
        const auto groupEntry = invoke ? invoke : &MTA_DE_ANIMATION_BLOCK_REMOVAL_GROUP;
#else
        const auto groupEntry = invoke ? invoke : &Detail::InvokeOriginal;
#endif
#if defined(MTA_DE_ANIMATION_BLOCK_REMOVAL_WRITE)
        const auto storeEntry = write ? write : &MTA_DE_ANIMATION_BLOCK_REMOVAL_WRITE;
#else
        const auto storeEntry = write ? write : &Detail::WriteStores;
#endif
        // All snapshots and output storage exist before native entry. Each
        // original call follows source group order and retains uncertainty.
        for (const auto& match : data->matches)
        {
            if ((result = Detail::Verify(image, owner, postFrameOwner, *data, false)) != ClockResult::Ok)
                return finish(result);
            if (!unchanged())
                return finish(ClockResult::BindingChanged);
            receipt.entered = true;
            ++receipt.groupCalls;
            result = groupEntry(reinterpret_cast<NativeAnimationBlockRemovalCalls::GroupFunction>(image + Detail::RetireRva), match.group.locator,
                                receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            ++receipt.groupReturns;
            if ((result = Detail::Verify(image, owner, postFrameOwner, *data, false)) != ClockResult::Ok)
                return finish(result);
        }
        if ((result = Detail::Verify(image, owner, postFrameOwner, *data, false)) != ClockResult::Ok)
            return finish(result);
        if (!unchanged())
            return finish(ClockResult::BindingChanged);
        // Exact numAnims0 means the source hierarchy loop has no iterations.
        // This is a policy port, not an independently discovered DE entry.
        receipt.entered = receipt.writeEntered = true;
        result = storeEntry(data->stores.data(), data->stores.size(), receipt.exceptionCode);
        receipt.returned = result == ClockResult::Ok;
        if (result != ClockResult::Ok || (result = Detail::Verify(image, owner, postFrameOwner, *data, true)) != ClockResult::Ok ||
            (result = Detail::Verify(image, owner, postFrameOwner, *data, true)) != ClockResult::Ok)
            return finish(result);
        if (!unchanged())
            return finish(ClockResult::BindingChanged);
        receipt.effectsVerified = true;
        return finish(ClockResult::Ok);
    }
}
