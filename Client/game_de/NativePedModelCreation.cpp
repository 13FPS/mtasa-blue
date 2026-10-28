/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Create manual ped defaults and journal one exact bank publication
 ****************************************************************************/
#include "NativePedModelCreation.h"
#include "NativePedModelDefaultCollision.h"
#include "NativePedModelMetadataProfile.h"
#include "NativeModelBankProfile.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <intrin.h>

namespace mta::game_de::NativePedModelCreation
{
    namespace PedCreationDetail
    {
        namespace Layout = NativePedModelMetadataProfile;
        namespace Heap = NativeModelReferenceHeap;
        namespace HeapLayout = NativeModelReferenceTransactionProfile;
        namespace AllocLayout = NativeCustomAnimSequenceStorageProfile;
        namespace Context = NativeStreamingCompletionContext;
        namespace Default = NativePedModelDefaultCollision;
        using Bytes = std::array<unsigned char, Layout::Size>;

        bool Vtable(std::uintptr_t image) noexcept
        {
            // These exact four observed cells bind the newly installed vptr.
            // Neither a familiar getter body nor a full-table guess suffices.
            constexpr std::array<std::uintptr_t, 4> offsets{0, 0x20, 0x40, 0x78};
            constexpr std::array<std::uintptr_t, 4> targets{0x11f5540, 0x11f5530, 0x120c530, 0x11f5520};
            for (std::size_t i = 0; i != offsets.size(); ++i)
            {
                const auto     cell = image + Layout::VtableRva + offsets[i];
                std::uintptr_t value = 0;
                if (!Heap::ReadOnlyImage(image, cell, sizeof(value)) || !NativeImageAccess::ReadImage(image, cell, &value, sizeof(value)) ||
                    value != image + targets[i] || !NativeImageAccess::ImagePages(image, value, 1, false, true))
                    return false;
            }
            const std::array<unsigned char, 3> expected{0xb0, 0x07, 0xc3};
            std::array<unsigned char, 3>       actual{};
            return NativeImageAccess::ReadImage(image, image + targets[1], actual.data(), actual.size(), false, true) && actual == expected;
        }
        bool Slots(std::uintptr_t image, const Heap::Value& heap) noexcept
        {
            const auto check = [&](std::uintptr_t table, std::uintptr_t allocate, std::uintptr_t release)
            {
                std::uintptr_t a = 0, b = 0;
                return Heap::ReadOnlyImage(image, table, 0x40) && NativeImageAccess::ReadImage(image, table + 0x10, &a, sizeof(a)) &&
                       NativeImageAccess::ReadImage(image, table + 0x30, &b, sizeof(b)) && a == image + allocate && b == image + release;
            };
            return check(heap.authorityVtable, AllocLayout::AuthorityMallocRva, AllocLayout::AuthorityFreeRva) &&
                   (heap.current == heap.authority || check(heap.currentVtable, AllocLayout::ProxyMallocRva, AllocLayout::ProxyFreeRva));
        }
        ClockResult CaptureHeap(std::uintptr_t image, CheckOwner owner, std::uintptr_t authority, Heap::Value& output) noexcept
        {
            auto result = Context::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Heap::QualifyProfile(image) || !Vtable(image))
                return ClockResult::BindingChanged;
            Heap::Value next;
            if ((result = Heap::Capture(image, next)) != ClockResult::Ok)
                return result;
            if (!Slots(image, next) || next.authority != authority)
                return ClockResult::BindingChanged;
            output = next;
            return Context::Owner(image, owner);
        }
        bool Disjoint(std::uintptr_t image, std::uintptr_t native, const Heap::Value& heap, const Default::Value& value) noexcept
        {
            return Heap::M::PrivateWritable(native, Layout::Size) && Heap::M::Disjoint(native, Layout::Size, image, Layout::ImageSize) &&
                   Heap::M::Disjoint(native, Layout::Size, heap.authority, HeapLayout::HeapBytes) &&
                   (heap.current == heap.authority || Heap::M::Disjoint(native, Layout::Size, heap.current, HeapLayout::ProxyBytes)) &&
                   Heap::M::Disjoint(native, Layout::Size, value.canonical, value.canonicalBytes.size()) &&
                   Heap::M::Disjoint(native, Layout::Size, value.data, value.dataBytes.size()) &&
                   Heap::M::Disjoint(native, Layout::Size, value.spheres, value.sphereBytes.size()) &&
                   Heap::M::Disjoint(native, Layout::Size, value.segments, value.segmentBytes.size());
        }
        Bytes Expected(std::uintptr_t image, std::uintptr_t canonical) noexcept
        {
            Bytes               value{};
            const auto          vtable = image + Layout::VtableRva;
            const std::uint64_t none = Layout::AnimNone;
            std::memcpy(value.data(), &vtable, sizeof(vtable));
            std::memcpy(value.data() + Layout::CollisionOffset, &canonical, sizeof(canonical));
            std::memcpy(value.data() + Layout::AnimOffset, &none, sizeof(none));
            return value;
        }
        bool Matches(std::uintptr_t native, const Bytes& expected) noexcept
        {
            Bytes actual{};
            return Heap::M::PrivateWritable(native, actual.size()) && Heap::M::Copy(native, actual.data(), actual.size()) && actual == expected;
        }
        ClockResult World(std::uintptr_t image, CheckOwner owner, const Context::Value* previous, Context::Value& output) noexcept
        {
            auto result = Context::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Context::Profile(image))
                return ClockResult::BindingChanged;
            if ((result = Context::Capture(image, output)) != ClockResult::Ok)
                return result;
            if (previous && (!Context::Fence(image, *previous) || !Context::Same(*previous, output, true)))
                return ClockResult::BindingChanged;
            return Context::Owner(image, owner);
        }
        struct RetirementGuard
        {
            std::uintptr_t image = 0, native = 0, slot = 0;
            CheckOwner     owner = nullptr;
            bool           publishedObserved = false;
        };
        thread_local RetirementGuard* retirement = nullptr;
        ClockResult                   RetirementOwner() noexcept
        {
            if (!retirement || !retirement->owner)
                return ClockResult::NotInitialized;
            auto result = retirement->owner();
            if (result != ClockResult::Ok)
                return result;
            if (!NativeModelBankReader::Revalidate({retirement->image}))
                return ClockResult::BindingChanged;
            std::uintptr_t current = 0;
            if (!NativeImageAccess::ReadImage(retirement->image, retirement->slot, &current, sizeof(current), true))
                return ClockResult::ReadFailure;
            if (current == retirement->native)
            {
                retirement->publishedObserved = true;
                return ClockResult::BindingChanged;
            }
            return ClockResult::Ok;
        }
        struct RetirementScope
        {
            RetirementGuard* previous;
            explicit RetirementScope(RetirementGuard& guard) noexcept : previous(retirement) { retirement = &guard; }
            ~RetirementScope() { retirement = previous; }
        };
    }

    bool Record::CanDiscard() const noexcept
    {
        const auto current = GetStage();
        return (current == Stage::Empty || current == Stage::Released) && storage.CanDiscard();
    }
    bool Record::CanRetireUnpublished() const noexcept
    {
        const auto current = GetStage();
        return (current == Stage::Unpublished || current == Stage::InitializedUnpublished) && journal.unpublishedProved &&
               (!journal.publishEntered || (journal.publication.returned && !journal.publication.accepted && journal.publication.observed != journal.native)) &&
               storage.GetKind() == NativeCustomAnimSequenceStorage::Kind::PedModelMetadata && storage.CanFreeStorage();
    }
    ClockResult WriteDefaults(std::uintptr_t object, std::uintptr_t pedVtable, std::uintptr_t canonical, std::uint32_t& exception) noexcept
    {
        __try
        {
            // MTA manually initializes fresh metadata. No global initializer,
            // native Init40 defaults or borrowed-object reset occurs here.
            std::memset(reinterpret_cast<void*>(object), 0, NativePedModelMetadataProfile::Size);
            std::memcpy(reinterpret_cast<void*>(object), &pedVtable, sizeof(pedVtable));
            std::memcpy(reinterpret_cast<void*>(object + NativePedModelMetadataProfile::CollisionOffset), &canonical, sizeof(canonical));
            const std::uint64_t none = NativePedModelMetadataProfile::AnimNone;
            std::memcpy(reinterpret_cast<void*>(object + NativePedModelMetadataProfile::AnimOffset), &none, sizeof(none));
            return ClockResult::Ok;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exception = GetExceptionCode();
            return ClockResult::OriginalException;
        }
    }
    ClockResult CompareExchange(std::uintptr_t slot, std::uintptr_t expected, std::uintptr_t target, CompareExchangeReceipt& receipt) noexcept
    {
        __try
        {
            receipt.observed = reinterpret_cast<std::uintptr_t>(
                InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(slot), reinterpret_cast<void*>(target), reinterpret_cast<void*>(expected)));
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
        static ClockResult Finish(Record& record, ClockResult result, Receipt* output) noexcept
        {
            auto& journal = record.journal;
            journal.entered = journal.allocation.entered || journal.initEntered || journal.publishEntered || journal.retirement.entered;
            journal.returned = journal.publication.returned;
            journal.debt = record.HasOutstandingResource();
            if (output)
                *output = journal;
            return result;
        }
        static bool Shape(const Record& record) noexcept
        {
            return record.storage.GetKind() == NativeCustomAnimSequenceStorage::Kind::PedModelMetadata &&
                   record.storage.OwnerGeneration() == record.generation && record.storage.ByteSize() == NativePedModelMetadataProfile::Size &&
                   !record.storage.Capacity() && !record.storage.PublicationAffinity() && record.storage.CanFreeStorage() &&
                   record.journal.allocation.allocated && record.storage.IssuedTag() == record.journal.allocation.base &&
                   record.storage.IssuedTag() == record.journal.native;
        }
        static ClockResult Create(std::uintptr_t image, Record& record, std::uint64_t generation, std::uint32_t model, CheckOwner owner, Receipt* output,
                                  InvokeWriteDefaults write, InvokeCompareExchange exchange, InvokeMalloc allocate) noexcept
        {
            namespace D = PedCreationDetail;
            if (!generation || model >= NativeModelBankProfile::ModelCount)
                return Finish(record, ClockResult::InvalidArgument, output);
            if (!owner)
                return Finish(record, ClockResult::NotInitialized, output);
            Stage expected = Stage::Empty;
            if (!record.stage.compare_exchange_strong(expected, Stage::Allocating, std::memory_order_acq_rel))
                return Finish(record, ClockResult::OwnerConflict, output);
            record.generation = generation;
            record.modelIndex = model;
            record.journal = {};
            auto& journal = record.journal;
            journal.generation = generation;
            journal.modelIndex = model;
            journal.unpublishedProved = true;
            auto result = NativePedModelMetadataStorageCalls::Allocate(image, record.storage, generation, owner, &journal.allocation, allocate);
            journal.native = journal.allocation.base;
            if (record.storage.CanDiscard())
                record.stage.store(journal.allocation.entered ? Stage::Released : Stage::Empty, std::memory_order_release);
            else
                record.stage.store(record.storage.CanFreeStorage() ? Stage::Unpublished : Stage::FaultDebt, std::memory_order_release);
            if (result != ClockResult::Ok)
                return Finish(record, result, output);
            if (!Shape(record))
                return Finish(record, ClockResult::BindingChanged, output);
            // Allocate owns the same gate internally. Re-enter only after its
            // durable original-base acknowledgement has already been saved.
            if (!D::Context::TryEnter())
                return Finish(record, ClockResult::Busy, output);
            D::Context::Scope            scope;
            D::Context::Value            world;
            D::Heap::Value               heap;
            D::Default::Value            canonical;
            NativeModelBankReader        reader({image}, owner);
            NativeModelBankReader::Value bank;
            if ((result = D::World(image, owner, nullptr, world)) != ClockResult::Ok ||
                (result = D::CaptureHeap(image, owner, journal.allocation.authority, heap)) != ClockResult::Ok ||
                (result = D::Default::Read(image, owner, canonical)) != ClockResult::Ok || (result = reader.Read(model, bank)) != ClockResult::Ok)
                return Finish(record, result, output);
            journal.slotAddress = bank.slotAddress;
            journal.previousBorrowedModel = bank.nativeModelInfo;
            if (!Shape(record) || bank.slotAddress % alignof(std::uintptr_t) ||
                !NativeImageAccess::ImagePages(image, bank.slotAddress, sizeof(std::uintptr_t), true))
                return Finish(record, ClockResult::BindingChanged, output);
            if (bank.nativeModelInfo == journal.native || !D::Disjoint(image, journal.native, heap, canonical))
            {
                // A claimed allocation overlapping a live authority/default
                // contradicts exclusive ownership. Neither zero nor free it.
                journal.unpublishedProved = false;
                record.stage.store(Stage::FaultDebt, std::memory_order_release);
                return Finish(record, ClockResult::BindingChanged, output);
            }
            if ((result = D::Context::Owner(image, owner)) != ClockResult::Ok)
                return Finish(record, result, output);
            record.stage.store(Stage::Initializing, std::memory_order_release);
            journal.initEntered = true;
            result = (write ? write : &WriteDefaults)(journal.native, image + D::Layout::VtableRva, canonical.canonical, journal.initExceptionCode);
            journal.initReturned = result == ClockResult::Ok;
            record.stage.store(Stage::Unpublished, std::memory_order_release);
            if (result != ClockResult::Ok)
                return Finish(record, result, output);
            const auto bytes = D::Expected(image, canonical.canonical);
            if (!D::Matches(journal.native, bytes))
                return Finish(record, ClockResult::BindingChanged, output);
            journal.initVerified = true;
            record.stage.store(Stage::InitializedUnpublished, std::memory_order_release);
            D::Context::Value nextWorld;
            if ((result = D::World(image, owner, &world, nextWorld)) != ClockResult::Ok ||
                (result = D::Default::Recheck(image, owner, canonical)) != ClockResult::Ok || (result = D::Heap::Recheck(image, heap)) != ClockResult::Ok ||
                (result = reader.Recheck(bank)) != ClockResult::Ok)
                return Finish(record, result, output);
            if (!D::Vtable(image) || !Shape(record) || !D::Matches(journal.native, bytes))
                return Finish(record, ClockResult::BindingChanged, output);
            if (!D::Disjoint(image, journal.native, heap, canonical))
            {
                journal.unpublishedProved = false;
                record.stage.store(Stage::FaultDebt, std::memory_order_release);
                return Finish(record, ClockResult::BindingChanged, output);
            }
            if ((result = D::Context::Owner(image, owner)) != ClockResult::Ok)
                return Finish(record, result, output);
            record.stage.store(Stage::Publishing, std::memory_order_release);
            journal.publishEntered = true;
            journal.unpublishedProved = false;
            CompareExchangeReceipt publication;
            result = (exchange ? exchange : &CompareExchange)(bank.slotAddress, bank.nativeModelInfo, journal.native, publication);
            // Save the physical CAS acknowledgement before any owner/profile
            // check. Known exposure and unknown entry can never become freeable.
            journal.publication = publication;
            const bool coherent = !publication.returned || publication.accepted == (publication.observed == bank.nativeModelInfo);
            if (!coherent)
                record.stage.store(Stage::FaultDebt, std::memory_order_release);
            else if (publication.returned && publication.accepted)
                record.stage.store(Stage::Published, std::memory_order_release);
            else if (publication.returned && !publication.accepted && publication.observed != journal.native)
            {
                journal.unpublishedProved = true;
                record.stage.store(Stage::InitializedUnpublished, std::memory_order_release);
            }
            else
                record.stage.store(Stage::FaultDebt, std::memory_order_release);
            if (result != ClockResult::Ok)
                return Finish(record, result, output);
            if (!coherent)
                return Finish(record, ClockResult::BindingChanged, output);
            if (!publication.returned || !publication.accepted)
                return Finish(record, ClockResult::BindingChanged, output);
            bank.nativeModelInfo = journal.native;
            if ((result = D::World(image, owner, &world, nextWorld)) != ClockResult::Ok ||
                (result = D::Default::Recheck(image, owner, canonical)) != ClockResult::Ok || (result = D::Heap::Recheck(image, heap)) != ClockResult::Ok ||
                (result = reader.Recheck(bank)) != ClockResult::Ok)
                return Finish(record, result, output);
            if (!D::Vtable(image) || !D::Matches(journal.native, bytes))
                return Finish(record, ClockResult::BindingChanged, output);
            if ((result = D::Context::Owner(image, owner)) != ClockResult::Ok)
                return Finish(record, result, output);
            journal.publishVerified = journal.verified = true;
            return Finish(record, ClockResult::Ok, output);
        }
        static ClockResult Retire(std::uintptr_t image, Record& record, std::uint64_t generation, CheckOwner owner, Receipt* output,
                                  InvokeFree release) noexcept
        {
            namespace D = PedCreationDetail;
            if (!generation || generation != record.generation || !record.CanRetireUnpublished())
                return Finish(record, ClockResult::OwnerConflict, output);
            if (!owner)
                return Finish(record, ClockResult::NotInitialized, output);
            if (D::retirement)
                return Finish(record, ClockResult::Busy, output);
            const auto prior = record.GetStage();
            auto       expected = prior;
            if (!record.stage.compare_exchange_strong(expected, Stage::Freeing, std::memory_order_acq_rel))
                return Finish(record, ClockResult::Busy, output);
            D::RetirementGuard guard{
                image, record.journal.native,
                image + NativeModelBankProfile::BankRva + static_cast<std::uintptr_t>(record.modelIndex) * NativeModelBankProfile::SlotBytes, owner, false};
            D::RetirementScope guarded(guard);
            auto               result =
                NativePedModelMetadataStorageCalls::FreeStorage(image, record.storage, generation, &D::RetirementOwner, &record.journal.retirement, release);
            if (guard.publishedObserved)
            {
                record.journal.unpublishedProved = false;
                record.stage.store(Stage::FaultDebt, std::memory_order_release);
            }
            else if (record.storage.CanDiscard())
                record.stage.store(Stage::Released, std::memory_order_release);
            else
                record.stage.store(record.journal.retirement.entered ? Stage::FaultDebt : prior, std::memory_order_release);
            return Finish(record, result, output);
        }
    };
    ClockResult CreateAndPublish(std::uintptr_t image, Record& record, std::uint64_t generation, std::uint32_t model, CheckOwner owner, Receipt* output,
                                 InvokeWriteDefaults write, InvokeCompareExchange exchange, InvokeMalloc allocate) noexcept
    {
        return Access::Create(image, record, generation, model, owner, output, write, exchange, allocate);
    }
    ClockResult RetireUnpublished(std::uintptr_t image, Record& record, std::uint64_t generation, CheckOwner owner, Receipt* output,
                                  InvokeFree release) noexcept
    {
        return Access::Retire(image, record, generation, owner, output, release);
    }
}
