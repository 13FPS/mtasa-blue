/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Copy nullable borrowed collision identity without adopting its owner
 ****************************************************************************/
#include "NativeStartupModelCollision.h"
#include "NativeModelObjectProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>

namespace mta::game_de::NativeStartupModelCollision
{
    namespace Detail
    {
        namespace P = NativeModelObjectProfile;
        namespace C = NativeStreamingCompletionContext;
        bool Profile(std::uintptr_t image) noexcept
        {
            // The existing common consumer proves field30 for model metadata.
            // Copying an arbitrary borrowed tag needs no ped identity/geometry.
            std::array<unsigned char, P::CollisionConsumer.size()> bytes{};
            return NativeModelBankReader::Revalidate({image}) &&
                   NativeImageAccess::ReadImage(image, image + P::CollisionConsumerRva, bytes.data(), bytes.size(), false, true) &&
                   bytes == P::CollisionConsumer;
        }
        bool Prefix(std::uintptr_t receiver) noexcept
        {
            if (!receiver || receiver % alignof(std::uintptr_t) || P::CollisionPrefixBytes > (std::numeric_limits<std::uintptr_t>::max)() - receiver)
                return false;
            const auto     end = receiver + P::CollisionPrefixBytes;
            std::uintptr_t allocation = 0;
            while (receiver < end)
            {
                MEMORY_BASIC_INFORMATION page{};
                if (VirtualQuery(reinterpret_cast<void*>(receiver), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                    (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED && page.Type != MEM_IMAGE) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = page.Protect & 0xff;
                if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                    return false;
                const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress), current = reinterpret_cast<std::uintptr_t>(page.AllocationBase);
                if (!current || (allocation && current != allocation) || begin > receiver ||
                    page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= receiver)
                    return false;
                allocation = current;
                receiver = (std::min)(end, begin + page.RegionSize);
            }
            return true;
        }
        bool Field(std::uintptr_t receiver, std::uintptr_t& output) noexcept
        {
            SIZE_T         copied = 0;
            std::uintptr_t next = 0;
            if (!Prefix(receiver) ||
                !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(receiver + P::CollisionOffset), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        bool Writable(std::uintptr_t image, std::uintptr_t field) noexcept
        {
            MEMORY_BASIC_INFORMATION page{};
            if (!field || field % alignof(std::uintptr_t) || VirtualQuery(reinterpret_cast<void*>(field), &page, sizeof(page)) != sizeof(page) ||
                page.State != MEM_COMMIT || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                return false;
            if (page.Type == MEM_IMAGE)
                return NativeImageAccess::ImagePages(image, field, sizeof(std::uintptr_t), true);
            if ((page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || ((page.Protect & 0xff) != PAGE_READWRITE && (page.Protect & 0xff) != PAGE_WRITECOPY))
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            return begin <= field && page.RegionSize <= (std::numeric_limits<std::uintptr_t>::max)() - begin && field <= begin + page.RegionSize &&
                   sizeof(std::uintptr_t) <= begin + page.RegionSize - field;
        }
        ClockResult World(std::uintptr_t image, CheckOwner owner, const C::Value* previous, C::Value& output) noexcept
        {
            auto result = C::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!C::Profile(image) || !Profile(image))
                return ClockResult::BindingChanged;
            if ((result = C::Capture(image, output)) != ClockResult::Ok)
                return result;
            if (previous && (!C::Fence(image, *previous) || !C::Same(*previous, output, true)))
                return ClockResult::BindingChanged;
            return C::Owner(image, owner);
        }
    }
    ClockResult CheckReady(std::uintptr_t image, CheckOwner owner) noexcept
    {
        if (!owner)
            return ClockResult::NotInitialized;
        auto result = Detail::C::Owner(image, owner);
        if (result != ClockResult::Ok)
            return result;
        // This is the established temporary completion exclusion and GC-zero
        // protocol, not a model-field CAS, entered receipt or reserved lease.
        if (!Detail::C::TryEnter())
            return ClockResult::Busy;
        Detail::C::Scope scope;
        Detail::C::Value first, next;
        result = Detail::World(image, owner, nullptr, first);
        if (result != ClockResult::Ok)
            return result;
        result = Detail::World(image, owner, &first, next);
        if (result != ClockResult::Ok)
            return result;
        if (!Detail::C::Fence(image, next) || !Detail::Profile(image))
            return ClockResult::BindingChanged;
        return Detail::C::Owner(image, owner);
    }

    void Record::MarkUnacknowledged() noexcept
    {
        if (journal.entered)
        {
            journal.debt = true;
            stage.store(Stage::FaultDebt, std::memory_order_release);
        }
    }
    ClockResult CompareExchange(std::uintptr_t field, std::uintptr_t expected, std::uintptr_t replacement, CompareExchangeReceipt& receipt) noexcept
    {
        __try
        {
            receipt.observed = reinterpret_cast<std::uintptr_t>(InterlockedCompareExchangePointer(
                reinterpret_cast<void* volatile*>(field), reinterpret_cast<void*>(replacement), reinterpret_cast<void*>(expected)));
            receipt.returned = true;
            receipt.accepted = receipt.observed == expected;
            return ClockResult::Ok;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            receipt.exceptionCode = GetExceptionCode();
            return ClockResult::OriginalException;
        }
    }
    struct Access
    {
        static ClockResult Finish(Record& record, ClockResult status, Receipt* output) noexcept
        {
            auto& journal = record.journal;
            if (journal.entered && (!journal.returned || (journal.accepted && !journal.verified)))
                record.stage.store(Stage::FaultDebt, std::memory_order_release);
            else
                record.stage.store(Stage::Complete, std::memory_order_release);
            journal.debt = record.HasDebt();
            if (output)
                *output = journal;
            return status;
        }
        static ClockResult Run(std::uintptr_t image, Record& record, std::uint64_t generation, std::uint32_t targetKey, std::uint32_t sourceKey,
                               CheckOwner owner, Receipt* output, Exchange exchange) noexcept
        {
            namespace D = Detail;
            Stage expected = Stage::Empty;
            if (!record.stage.compare_exchange_strong(expected, Stage::Preparing, std::memory_order_acq_rel))
            {
                if (output)
                    *output = record.journal;
                return ClockResult::OwnerConflict;
            }
            auto& journal = record.journal;
            journal.generation = generation;
            journal.targetModel = targetKey;
            journal.sourceModel = sourceKey;
            if (!generation)
                return Finish(record, ClockResult::InvalidArgument, output);
            if (!owner)
                return Finish(record, ClockResult::NotInitialized, output);
            NativeModelBankReader        bank({image}, owner);
            NativeModelBankReader::Value target, source;
            auto                         result = bank.Read(targetKey, target);
            if (result != ClockResult::Ok)
                return Finish(record, result, output);
            journal.targetInterface = target.nativeModelInfo;
            const auto skip = [&]() noexcept
            {
                journal.skipped = true;
                journal.verified = true;
                return Finish(record, ClockResult::Ok, output);
            };
            if (!target.nativeModelInfo)
            {
                if ((result = bank.Recheck(target)) != ClockResult::Ok)
                    return Finish(record, result, output);
                return skip();
            }
            if (!D::Profile(image))
                return Finish(record, ClockResult::BindingChanged, output);
            std::uintptr_t targetCollision = 0, sourceCollision = 0, current = 0;
            if (!D::Field(target.nativeModelInfo, targetCollision))
                return Finish(record, ClockResult::ReadFailure, output);
            if ((result = bank.Recheck(target)) != ClockResult::Ok)
                return Finish(record, result, output);
            if (!D::Field(target.nativeModelInfo, current) || current != targetCollision || !D::Profile(image))
                return Finish(record, ClockResult::BindingChanged, output);
            if (targetCollision)
            {
                if ((result = D::C::Owner(image, owner)) != ClockResult::Ok)
                    return Finish(record, result, output);
                return skip();
            }
            // Source lookup remains conditional, exactly as FixModelCol. Even
            // an invalid source key is irrelevant to an already repaired target.
            if ((result = bank.Read(sourceKey, source)) != ClockResult::Ok)
                return Finish(record, result, output);
            journal.sourceInterface = source.nativeModelInfo;
            if (!source.nativeModelInfo)
            {
                if ((result = bank.Recheck(target)) != ClockResult::Ok || (result = bank.Recheck(source)) != ClockResult::Ok)
                    return Finish(record, result, output);
                if (!D::Field(target.nativeModelInfo, current) || current != 0 || !D::Profile(image))
                    return Finish(record, ClockResult::BindingChanged, output);
                if ((result = D::C::Owner(image, owner)) != ClockResult::Ok)
                    return Finish(record, result, output);
                return skip();
            }
            if (!D::Field(source.nativeModelInfo, sourceCollision))
                return Finish(record, ClockResult::ReadFailure, output);
            journal.targetField = target.nativeModelInfo + D::P::CollisionOffset;
            journal.sourceCollision = sourceCollision;
            if (!D::C::TryEnter())
                return Finish(record, ClockResult::Busy, output);
            D::C::Scope scope;
            D::C::Value world, nextWorld;
            if ((result = D::World(image, owner, nullptr, world)) != ClockResult::Ok || (result = bank.Recheck(target)) != ClockResult::Ok ||
                (result = bank.Recheck(source)) != ClockResult::Ok)
                return Finish(record, result, output);
            if ((result = D::World(image, owner, &world, nextWorld)) != ClockResult::Ok)
                return Finish(record, result, output);
            if (!D::Field(source.nativeModelInfo, current) || current != sourceCollision || !D::Field(target.nativeModelInfo, current) || current != 0 ||
                !D::Writable(image, journal.targetField))
                return Finish(record, ClockResult::BindingChanged, output);
            if ((result = D::C::Owner(image, owner)) != ClockResult::Ok || !D::Profile(image))
                return Finish(record, result == ClockResult::Ok ? ClockResult::BindingChanged : result, output);
            record.stage.store(Stage::Entering, std::memory_order_release);
            journal.entered = true;
            CompareExchangeReceipt receipt;
            result = (exchange ? exchange : &CompareExchange)(journal.targetField, 0, sourceCollision, receipt);
            journal.returned = receipt.returned;
            journal.accepted = receipt.accepted;
            journal.observed = receipt.observed;
            journal.exceptionCode = receipt.exceptionCode;
            // A contradictory seam/entry acknowledgement is uncertain. Keep
            // the entered journal instead of converting it into retry rights.
            if (!receipt.returned || receipt.accepted != (receipt.observed == 0))
            {
                journal.returned = false;
                return Finish(record, result == ClockResult::Ok ? ClockResult::BindingChanged : result, output);
            }
            if (result != ClockResult::Ok)
                return Finish(record, result, output);
            if (!receipt.accepted)
                return Finish(record, ClockResult::Busy, output);
            if ((result = bank.Recheck(target)) != ClockResult::Ok || (result = bank.Recheck(source)) != ClockResult::Ok ||
                (result = D::World(image, owner, &world, nextWorld)) != ClockResult::Ok)
                return Finish(record, result, output);
            if (!D::Field(source.nativeModelInfo, current) || current != sourceCollision || !D::Field(target.nativeModelInfo, current) ||
                current != sourceCollision || !D::Writable(image, journal.targetField))
                return Finish(record, ClockResult::BindingChanged, output);
            if ((result = D::C::Owner(image, owner)) != ClockResult::Ok || !D::Profile(image))
                return Finish(record, result == ClockResult::Ok ? ClockResult::BindingChanged : result, output);
            journal.verified = true;
            return Finish(record, ClockResult::Ok, output);
        }
    };
    ClockResult Copy(std::uintptr_t image, Record& record, std::uint64_t generation, std::uint32_t target, std::uint32_t source, CheckOwner owner,
                     Receipt* output, Exchange exchange) noexcept
    {
        return Access::Run(image, record, generation, target, source, owner, output, exchange);
    }
}
