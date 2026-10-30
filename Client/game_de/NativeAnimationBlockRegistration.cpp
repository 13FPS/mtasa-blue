/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA registration effects without inventing an entry
 ****************************************************************************/
#include "NativeAnimationBlockRegistration.h"
#include "NativeAnimationManagerMemory.h"
#include "NativeAnimationManagerProfile.h"
#include "NativeAnimationAssocDefinitionProfile.h"
#include "NativeAnimationBlockProfile.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace mta::game_de
{
    namespace AnimationBlockRegistrationDetail
    {
        namespace Block = NativeAnimationBlockProfile;
        namespace Definition = NativeAnimationAssocDefinitionProfile;
        namespace Image = NativeImageAccess;
        namespace Context = NativeStreamingCompletionContext;
        using CheckOwner = NativeAnimationBlockRegistrationCalls::CheckOwner;
        using Store = NativeAnimationBlockRegistrationCalls::Store;
        using Receipt = NativeAnimationBlockRegistrationCalls::Receipt;
        constexpr std::size_t DefinitionPrefixBytes = Definition::RowBytes * Definition::BlockNamePrefixCount;
        constexpr std::size_t StoreCount = 7;
        static_assert(Block::RowBytes == 32 && NativeAnimationBlockName{}.size() == 16);

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
        bool Sections(std::uintptr_t image, std::size_t bankBytes, bool definitions) noexcept
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
            constexpr auto rw = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            if (!bankBytes || !contains(Block::BankRva, bankBytes, rw, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(Block::CountRva, sizeof(std::int32_t), rw, IMAGE_SCN_MEM_EXECUTE))
                return false;
            return !definitions || (contains(Definition::BankRva, DefinitionPrefixBytes, rw, IMAGE_SCN_MEM_EXECUTE) &&
                                    contains(0xff1070, Definition::FirstGroupCode.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE));
        }
        bool FirstGroupSource(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, Definition::FirstGroupCode.size()> bytes{};
            return Image::ReadImage(image, image + 0xff1070, bytes.data(), bytes.size(), false, true) && bytes == Definition::FirstGroupCode;
        }
        ClockResult WriteStores(const Store* stores, std::size_t count, std::uint32_t& exception) noexcept
        {
            // The controller always supplies all seven stores. Keep the
            // primitive bounded while permitting an actual executed prefix
            // when testing lost acknowledgement after partial publication.
            if (!stores || !count || count > StoreCount)
                return ClockResult::InvalidArgument;
            __try
            {
                // This is the explicitly ported classic caller policy, not
                // a claim that a standalone DE Register entry was executed.
                // Count publishes first; unknown partial stores stay retained.
                for (std::size_t index = 0; index != count; ++index)
                    *reinterpret_cast<volatile std::uint32_t*>(stores[index].address) = stores[index].value;
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
    struct NativeAnimationBlockRegistration::Data
    {
        NativeAnimationBlockName                                                                          name{};
        std::string                                                                                       query;
        NativeAnimationManagerCounts                                                                      counts{};
        std::int32_t                                                                                      index = -1;
        std::uint32_t                                                                                     group = 0;
        bool                                                                                              append = false;
        std::vector<unsigned char>                                                                        before, expected, observed;
        std::array<unsigned char, AnimationBlockRegistrationDetail::DefinitionPrefixBytes>                definitions{}, observedDefinitions{};
        std::array<AnimationBlockRegistrationDetail::Store, AnimationBlockRegistrationDetail::StoreCount> stores{};
    };
    namespace AnimationBlockRegistrationDetail
    {
        template <class Plan>
        ClockResult Verify(std::uintptr_t image, CheckOwner owner, CheckOwner postFrameOwner, Plan& plan, bool after) noexcept
        {
            auto result = Owner(image, owner, postFrameOwner);
            if (result != ClockResult::Ok)
                return result;
            if (!Sections(image, plan.before.size(), plan.append) || !NativeAnimationManagerMemory::Revalidate({image}) ||
                (plan.append && !FirstGroupSource(image)))
                return ClockResult::BindingChanged;
            NativeAnimationManagerMemory memory({image}, owner);
            NativeAnimationManagerCounts counts;
            if ((result = memory.ReadCounts(counts)) != ClockResult::Ok)
                return result;
            if (counts.blocks != plan.counts.blocks + (after && plan.append ? 1 : 0) || counts.animations != plan.counts.animations)
                return ClockResult::BindingChanged;
            if (!Image::ReadImage(image, image + Block::BankRva, plan.observed.data(), plan.observed.size(), true))
                return ClockResult::ReadFailure;
            if (plan.observed != (after ? plan.expected : plan.before))
                return ClockResult::BindingChanged;
            if (plan.append)
            {
                if (!Image::ReadImage(image, image + Definition::BankRva, plan.observedDefinitions.data(), plan.observedDefinitions.size(), true))
                    return ClockResult::ReadFailure;
                if (plan.observedDefinitions != plan.definitions)
                    return ClockResult::BindingChanged;
            }
            // The full bank and source fold/code bookends retain the exact
            // first-match lookup result. The full fixed definition prefix
            // retains the selected FF1070/default118 result. Neither helper
            // is replayed after entry: both allocate CPU observation buffers.
            NativeAnimationManagerCounts final;
            if ((result = memory.ReadCounts(final)) != ClockResult::Ok)
                return result;
            if (final.blocks != counts.blocks || final.animations != counts.animations || !NativeAnimationManagerMemory::Revalidate({image}) ||
                (plan.append && !FirstGroupSource(image)))
                return ClockResult::BindingChanged;
            return Owner(image, owner, postFrameOwner);
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
            // This is the preexisting observation interval, not native bank
            // capacity. The command refuses to append beyond that proven span.
            if (plan.counts.blocks < 0 || static_cast<std::uint32_t>(plan.counts.blocks) > Block::ObservationLimit)
                return ClockResult::OwnerConflict;
            receipt.beforeCount = receipt.afterCount = plan.counts.blocks;
            const auto prefixBytes = static_cast<std::size_t>(plan.counts.blocks) * Block::RowBytes;
            plan.before.resize(prefixBytes);
            if (prefixBytes &&
                (!Sections(image, prefixBytes, false) || !Image::ReadImage(image, image + Block::BankRva, plan.before.data(), prefixBytes, true)))
                return ClockResult::ReadFailure;
            if ((result = memory.FindBlockIndex(plan.name, plan.index)) != ClockResult::Ok)
                return result;
            plan.append = plan.index < 0;
            if (plan.append && static_cast<std::uint32_t>(plan.counts.blocks) == Block::ObservationLimit)
                return ClockResult::OwnerConflict;
            if (!plan.append && plan.index >= plan.counts.blocks)
                return ClockResult::BindingChanged;
            const auto rows = static_cast<std::size_t>(plan.counts.blocks) + (plan.append ? 1u : 0u);
            const auto bytes = rows * Block::RowBytes;
            if (!Sections(image, bytes, plan.append))
                return ClockResult::BindingChanged;
            plan.observed.resize(bytes);
            if (!Image::ReadImage(image, image + Block::BankRva, plan.observed.data(), bytes, true))
                return ClockResult::ReadFailure;
            if (prefixBytes && std::memcmp(plan.before.data(), plan.observed.data(), prefixBytes))
                return ClockResult::BindingChanged;
            plan.before = plan.observed;
            plan.expected = plan.before;
            if (!plan.append)
            {
                std::memcpy(&plan.group, plan.before.data() + static_cast<std::size_t>(plan.index) * Block::RowBytes + 0x1c, sizeof(plan.group));
                receipt.group = plan.group;
                receipt.noWrite = true;
                return Verify(image, owner, postFrameOwner, plan, false);
            }
            plan.index = plan.counts.blocks;
            const auto at = static_cast<std::size_t>(plan.index) * Block::RowBytes;
            for (std::size_t byte = 0; byte != Block::RowBytes; ++byte)
                if (plan.before[at + byte])
                    return ClockResult::OwnerConflict;
            if (!FirstGroupSource(image) || !Image::ReadImage(image, image + Definition::BankRva, plan.definitions.data(), plan.definitions.size(), true))
                return ClockResult::BindingChanged;
            // FF1070 consumes only the fixed definition prefix and name, not
            // the new block row/count. Prepare its result before publication;
            // post-entry bookends preserve those exact deterministic inputs.
            if ((result = memory.FindFirstGroupByBlockName(plan.query, plan.group)) != ClockResult::Ok)
                return result;
            if (plan.group > Definition::BlockNamePrefixCount)
                return ClockResult::BindingChanged;
            const auto row = image + Block::BankRva + at;
            plan.stores[0] = {image + Block::CountRva, static_cast<std::uint32_t>(plan.counts.blocks + 1)};
            for (std::size_t word = 0; word != 4; ++word)
            {
                std::uint32_t value = 0;
                std::memcpy(&value, plan.name.data() + word * sizeof(value), sizeof(value));
                plan.stores[word + 1] = {row + word * sizeof(value), value};
            }
            // Classic registration writes NumAnims0 and the first matching
            // group; it does not initialize loaded/ref/start fields. The
            // explicit fresh-zero-row domain preserves those untouched bytes.
            plan.stores[5] = {row + 0x18, 0};
            plan.stores[6] = {row + 0x1c, plan.group};
            std::memcpy(plan.expected.data() + at, plan.name.data(), plan.name.size());
            std::memcpy(plan.expected.data() + at + 0x1c, &plan.group, sizeof(plan.group));
            receipt.afterCount = plan.counts.blocks + 1;
            receipt.group = plan.group;
            receipt.stores = static_cast<std::uint32_t>(plan.stores.size());
            return Verify(image, owner, postFrameOwner, plan, false);
        }
    }
    NativeAnimationBlockRegistration::NativeAnimationBlockRegistration() noexcept = default;
    NativeAnimationBlockRegistration::~NativeAnimationBlockRegistration() = default;
    ClockResult NativeAnimationBlockRegistration::Register(std::uintptr_t image, std::uint64_t generation, const NativeAnimationBlockName& requested,
                                                           CheckOwner owner, CheckOwner postFrameOwner, Receipt* output, Write write) noexcept
    {
        namespace Detail = AnimationBlockRegistrationDetail;
        const auto               sourceName = requested;
        NativeAnimationBlockName name{};
        std::size_t              length = 0;
        while (length != sourceName.size() && sourceName[length])
        {
            if (sourceName[length] >= 0x80)
                break;
            name[length] = sourceName[length];
            ++length;
        }
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
        if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - NativePedPoolProfile::ImageSize || !generation || !owner || !postFrameOwner ||
            !length || length == sourceName.size() || sourceName[length])
            return report(ClockResult::InvalidArgument);
        bool expected = false;
        if (!busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return report(ClockResult::Busy);
        Detail::BusyScope operation{busy};
        const auto        finish = [&](ClockResult result)
        {
            if (receipt.entered && result != ClockResult::Ok)
                debt.store(true, std::memory_order_release);
            if (!debt.load(std::memory_order_acquire))
                data.reset();
            return report(result);
        };
        if (debt.load(std::memory_order_acquire))
            return finish(ClockResult::OwnerConflict);
        if (!Detail::Context::TryEnter())
            return finish(ClockResult::Busy);
        Detail::Context::Scope gate;
        ClockResult            result;
        try
        {
            data = std::make_unique<Data>();
            data->name = name;
            data->query.assign(reinterpret_cast<const char*>(name.data()), length);
            result = Detail::Prepare(image, owner, postFrameOwner, *data, receipt);
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        if (result != ClockResult::Ok || (result = Detail::Verify(image, owner, postFrameOwner, *data, false)) != ClockResult::Ok)
            return finish(result);
        if (receipt.noWrite)
        {
            receipt.index = data->index;
            receipt.effectsVerified = true;
            return finish(ClockResult::Ok);
        }
#if defined(MTA_DE_ANIMATION_BLOCK_REGISTRATION_WRITE)
        const auto entry = write ? write : &MTA_DE_ANIMATION_BLOCK_REGISTRATION_WRITE;
#else
        const auto entry = write ? write : &Detail::WriteStores;
#endif
        if ((result = Detail::Owner(image, owner, postFrameOwner)) != ClockResult::Ok)
            return finish(result);
        // Every expected bank/definition byte and all seven ordered DWORDs
        // exist before this boundary. Entered uncertainty never publishes an
        // index or schedules replay/repair of the partially visible block.
        receipt.entered = true;
        result = entry(data->stores.data(), data->stores.size(), receipt.exceptionCode);
        receipt.returned = result == ClockResult::Ok;
        if (result != ClockResult::Ok || (result = Detail::Verify(image, owner, postFrameOwner, *data, true)) != ClockResult::Ok ||
            (result = Detail::Verify(image, owner, postFrameOwner, *data, true)) != ClockResult::Ok)
            return finish(result);
        receipt.index = data->index;
        receipt.effectsVerified = true;
        return finish(ClockResult::Ok);
    }
}
