/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Source-owned empty hierarchy initialization and plain CPU retirement
 ****************************************************************************/
#include "NativePrivateAnimationHierarchyStorage.h"
#include "NativeAnimationHierarchyReadProfile.h"
#include "NativeAnimationManagerMemory.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeModelReferenceMap.h"
#include <cstring>
namespace mta::game_de
{
    namespace PrivateAnimationHierarchyDetail
    {
        using Storage = NativePrivateAnimationHierarchyStorage;
        using State = Storage::State;
        using CheckOwner = NativePrivateAnimationHierarchyCalls::CheckOwner;
        namespace Context = NativeStreamingCompletionContext;
        namespace Image = NativeImageAccess;
        namespace Profile = NativeAnimationHierarchyReadProfile;
        using Bytes = std::array<unsigned char, 40>;
        // Saved FEC67A..FEC6CE establishes the cache QWORD20 separately from
        // the scalar/count/sequence producers. No cache operation is called.
        inline constexpr unsigned char CacheProof[] = {0x4c, 0x8b, 0x41, 0x20, 0x4d, 0x85, 0xc0, 0x74, 0x4b, 0x49, 0x8b, 0x48, 0x10, 0x49, 0x8b, 0x40, 0x08,
                                                       0x48, 0x89, 0x41, 0x08, 0x49, 0x8b, 0x40, 0x10, 0x49, 0x8b, 0x48, 0x08, 0x48, 0x89, 0x41, 0x10, 0x48,
                                                       0x8b, 0x05, 0x3e, 0x42, 0x3d, 0x04, 0x49, 0x89, 0x40, 0x10, 0x48, 0x8b, 0x05, 0x33, 0x42, 0x3d, 0x04,
                                                       0x4c, 0x89, 0x40, 0x08, 0x48, 0x8d, 0x05, 0x18, 0x42, 0x3d, 0x04, 0x49, 0x89, 0x40, 0x08, 0x4c, 0x89,
                                                       0x05, 0x1d, 0x42, 0x3d, 0x04, 0x4d, 0x89, 0x66, 0x20, 0x4d, 0x89, 0x20, 0x49, 0x8b, 0x56, 0x08};
        bool                           ProfileMatches(std::uintptr_t image) noexcept
        {
            if (!NativeAnimationManagerMemory::Revalidate({image}))
                return false;
            for (const auto& pin : Profile::Code)
            {
                std::array<unsigned char, Profile::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            std::array<unsigned char, sizeof(CacheProof)> cache{};
            return Image::ReadImage(image, image + 0xfec67a, cache.data(), cache.size(), false, true) && !std::memcmp(cache.data(), CacheProof, cache.size());
        }
        template <class T>
        T Field(const Bytes& bytes, std::size_t at) noexcept
        {
            T value;
            std::memcpy(&value, bytes.data() + at, sizeof(value));
            return value;
        }
        // Snapshots contain intentionally undefined hash/alignment bytes. Use
        // object-representation comparison below instead of evaluating those
        // bytes as semantic scalar or array-element values.
        bool Matches(const Bytes& bytes, std::uintptr_t sequences, std::uint16_t count) noexcept
        {
            return Field<std::uintptr_t>(bytes, 8) == sequences && Field<std::uint16_t>(bytes, 0x10) == count && !Field<std::uintptr_t>(bytes, 0x20);
        }
        void InitializeImage(Bytes& bytes) noexcept
        {
            const std::uintptr_t pointer = 0;
            const std::uint16_t  count = 0;
            const std::uint8_t   zero = 0;
            const std::int32_t   custom = -1;
            const std::uint32_t  time = 0;
            std::memcpy(bytes.data() + 8, &pointer, sizeof(pointer));
            std::memcpy(bytes.data() + 0x10, &count, sizeof(count));
            std::memcpy(bytes.data() + 0x12, &zero, sizeof(zero));
            std::memcpy(bytes.data() + 0x13, &zero, sizeof(zero));
            std::memcpy(bytes.data() + 0x14, &custom, sizeof(custom));
            std::memcpy(bytes.data() + 0x18, &time, sizeof(time));
            std::memcpy(bytes.data() + 0x20, &pointer, sizeof(pointer));
        }
        ClockResult WriteScalar(std::uintptr_t address, NativePrivateAnimationHierarchyCalls::ScalarOperation operation, std::uint32_t bits,
                                std::uint32_t& exception) noexcept
        {
            using Operation = NativePrivateAnimationHierarchyCalls::ScalarOperation;
            __try
            {
                if (operation == Operation::RunningCompressed)
                {
                    const auto value = static_cast<std::uint8_t>(bits);
                    std::memcpy(reinterpret_cast<void*>(address + 0x12), &value, sizeof(value));
                }
                else if (operation == Operation::AnimationBlockID)
                    std::memcpy(reinterpret_cast<void*>(address + 0x14), &bits, sizeof(bits));
                else if (operation == Operation::NameHash)
                    std::memcpy(reinterpret_cast<void*>(address), &bits, sizeof(bits));
                else if (operation == Operation::SequenceCount)
                {
                    const auto count = static_cast<std::uint16_t>(bits);
                    std::memcpy(reinterpret_cast<void*>(address + 0x10), &count, sizeof(count));
                }
                else
                    return ClockResult::InvalidArgument;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult WriteSequences(std::uintptr_t address, std::uintptr_t sequences, std::uint32_t& exception) noexcept
        {
            __try
            {
                // The source setter assigns only the pointer. Count and every
                // allocation journal survive both attachment and detachment.
                std::memcpy(reinterpret_cast<void*>(address + 8), &sequences, sizeof(sequences));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult InitializeFields(std::uintptr_t address, std::uint32_t& exception) noexcept
        {
            // Source Initialize deliberately preserves the hash and alignment
            // bytes0..7. This is not memset40 and owns no native children.
            const std::uintptr_t pointer = 0;
            const std::uint16_t  count = 0;
            const std::uint8_t   zero = 0;
            const std::int32_t   custom = -1;
            const std::uint32_t  time = 0;
            __try
            {
                std::memcpy(reinterpret_cast<void*>(address + 8), &pointer, sizeof(pointer));
                std::memcpy(reinterpret_cast<void*>(address + 0x10), &count, sizeof(count));
                std::memcpy(reinterpret_cast<void*>(address + 0x12), &zero, sizeof(zero));
                std::memcpy(reinterpret_cast<void*>(address + 0x13), &zero, sizeof(zero));
                std::memcpy(reinterpret_cast<void*>(address + 0x14), &custom, sizeof(custom));
                std::memcpy(reinterpret_cast<void*>(address + 0x18), &time, sizeof(time));
                std::memcpy(reinterpret_cast<void*>(address + 0x20), &pointer, sizeof(pointer));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
    }
    namespace PrivateHierarchyStorageDetail = PrivateAnimationHierarchyDetail;
    struct NativePrivateAnimationHierarchyStorageAccess
    {
        using Storage = NativePrivateAnimationHierarchyStorage;
        using State = Storage::State;
        using Receipt = NativePrivateAnimationHierarchyCalls::Receipt;
        using CheckOwner = NativePrivateAnimationHierarchyCalls::CheckOwner;
        using Write = NativePrivateAnimationHierarchyCalls::Write;
        static bool        Unlinked(const Storage& storage) noexcept { return !storage.expectedSequences && !storage.pointerAckPending; }
        static bool        NoChild(const Storage& storage) noexcept { return !storage.expectedSequences; }
        static bool        PointerPending(const Storage& storage) noexcept { return storage.pointerAckPending; }
        static ClockResult Check(const Storage& storage, std::uint64_t generation, bool allowEntered = false) noexcept
        {
            if (!generation || storage.generation != generation)
                return ClockResult::BindingChanged;
            const auto state = storage.GetState();
            return state == State::Ready || (allowEntered && state == State::Entered) ? ClockResult::Ok : ClockResult::NotInitialized;
        }
        static void RetainForeign(const Storage& storage) noexcept
        {
            // Observed foreign children/cache carry lifetime uncertainty.
            // Latch it before returning; raw field clearing cannot unpin it.
            auto current = storage.GetState();
            while (current == State::Ready || current == State::Entered)
                if (storage.state.compare_exchange_weak(current, State::PublicationPinned, std::memory_order_acq_rel))
                    return;
        }
        static ClockResult Snapshot(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner,
                                    PrivateHierarchyStorageDetail::Bytes& output, bool allowEntered = false) noexcept
        {
            auto result = Check(storage, generation, allowEntered);
            if (result != ClockResult::Ok || (result = PrivateHierarchyStorageDetail::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!PrivateHierarchyStorageDetail::ProfileMatches(image))
                return ClockResult::BindingChanged;
            PrivateHierarchyStorageDetail::Bytes first, second;
            if (!NativeModelReferenceMap::Copy(storage.Address(), first.data(), first.size()))
                return ClockResult::ReadFailure;
            if (!PrivateHierarchyStorageDetail::Matches(first, storage.expectedSequences, storage.expectedCount))
            {
                RetainForeign(storage);
                return ClockResult::BindingChanged;
            }
            if ((result = PrivateHierarchyStorageDetail::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!NativeModelReferenceMap::Copy(storage.Address(), second.data(), second.size()))
                return ClockResult::ReadFailure;
            if (!PrivateHierarchyStorageDetail::Matches(second, storage.expectedSequences, storage.expectedCount))
            {
                RetainForeign(storage);
                return ClockResult::BindingChanged;
            }
            if (std::memcmp(first.data(), second.data(), first.size()) || !PrivateHierarchyStorageDetail::ProfileMatches(image))
                return ClockResult::BindingChanged;
            if ((result = PrivateHierarchyStorageDetail::Context::Owner(image, owner)) != ClockResult::Ok ||
                (result = Check(storage, generation, allowEntered)) != ClockResult::Ok)
                return result;
            output = second;
            return ClockResult::Ok;
        }
        static ClockResult Initialize(std::uintptr_t image, Storage& storage, std::uint64_t generation, CheckOwner owner, Receipt* output, Write write) noexcept
        {
            Receipt    receipt;
            const auto state = storage.GetState();
            const auto finish = [&](ClockResult result)
            {
                if (receipt.entered && result != ClockResult::Ok)
                {
                    // Never overwrite an irreversible publication marker.
                    // A never-accepted fresh write has no exposed/child state;
                    // every other failed write requires an empty recheck.
                    auto       current = storage.GetState();
                    const auto failed = state == State::Fresh && !receipt.accepted ? State::Failed : State::Entered;
                    while (current != State::PublicationPinned && !storage.state.compare_exchange_weak(current, failed, std::memory_order_acq_rel))
                    {
                    }
                }
                if (output)
                    *output = receipt;
                return result;
            };
            if (!generation || storage.generation != generation)
                return finish(ClockResult::BindingChanged);
            if (!owner)
                return finish(ClockResult::NotInitialized);
            if (state != State::Fresh && state != State::Ready)
                return finish(ClockResult::NotInitialized);
            if (storage.expectedSequences || storage.pointerAckPending)
                return finish(ClockResult::OwnerConflict);
            if (!PrivateHierarchyStorageDetail::Context::TryEnter())
                return finish(ClockResult::Busy);
            PrivateHierarchyStorageDetail::Context::Scope scope;
            PrivateHierarchyStorageDetail::Bytes          before;
            auto                                          result = PrivateHierarchyStorageDetail::Context::Owner(image, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            if (!PrivateHierarchyStorageDetail::ProfileMatches(image))
                return finish(ClockResult::BindingChanged);
            if (state == State::Ready)
            {
                if ((result = Snapshot(image, storage, generation, owner, before)) != ClockResult::Ok)
                    return finish(result);
            }
            else if (!NativeModelReferenceMap::Copy(storage.Address(), before.data(), before.size()))
                return finish(ClockResult::ReadFailure);
            auto expected = before;
            PrivateHierarchyStorageDetail::InitializeImage(expected);
#if defined(MTA_DE_PRIVATE_ANIMATION_HIERARCHY_INITIALIZE)
            const auto entry = write ? write : &MTA_DE_PRIVATE_ANIMATION_HIERARCHY_INITIALIZE;
#else
            const auto entry = write ? write : &PrivateHierarchyStorageDetail::InitializeFields;
#endif
            storage.state.store(State::Entered, std::memory_order_release);
            receipt.entered = true;
            receipt.hierarchy = storage.Address();
            result = entry(storage.Address(), receipt.exceptionCode);
            receipt.accepted = result == ClockResult::Ok;
            if (receipt.accepted)
            {
                storage.expectedSequences = 0;
                storage.expectedCount = 0;
            }
            if (result != ClockResult::Ok)
                return finish(result);
            storage.state.store(State::Ready, std::memory_order_release);
            PrivateHierarchyStorageDetail::Bytes after;
            if ((result = Snapshot(image, storage, generation, owner, after)) != ClockResult::Ok)
                return finish(result);
            if (std::memcmp(after.data(), expected.data(), after.size()))
                return finish(ClockResult::BindingChanged);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult SetScalar(std::uintptr_t image, Storage& storage, std::uint64_t generation,
                                     NativePrivateAnimationHierarchyCalls::ScalarOperation operation, std::uint32_t bits, CheckOwner owner, Receipt* receipt,
                                     NativePrivateAnimationHierarchyCalls::ScalarWrite write) noexcept
        {
            if (receipt)
                *receipt = {};
            if (storage.expectedSequences || storage.pointerAckPending)
                return ClockResult::OwnerConflict;
            if (!PrivateHierarchyStorageDetail::Context::TryEnter())
                return ClockResult::Busy;
            PrivateHierarchyStorageDetail::Context::Scope scope;
            return SetScalarWithin(image, storage, generation, operation, bits, owner, receipt, write);
        }
        static ClockResult SetScalarWithin(std::uintptr_t image, Storage& storage, std::uint64_t generation,
                                           NativePrivateAnimationHierarchyCalls::ScalarOperation operation, std::uint32_t bits, CheckOwner owner,
                                           Receipt* output, NativePrivateAnimationHierarchyCalls::ScalarWrite write) noexcept
        {
            using Operation = NativePrivateAnimationHierarchyCalls::ScalarOperation;
            const auto requestedOperation = operation;
            const auto requestedBits = bits;
            Receipt    receipt;
            const auto finish = [&](ClockResult result)
            {
                if (receipt.entered && result != ClockResult::Ok)
                {
                    auto current = storage.GetState();
                    while ((current == State::Ready || current == State::Entered) &&
                           !storage.state.compare_exchange_weak(current, State::Entered, std::memory_order_acq_rel))
                    {
                    }
                }
                if (output)
                    *output = receipt;
                return result;
            };
            if (!owner)
                return finish(ClockResult::NotInitialized);
            PrivateHierarchyStorageDetail::Bytes before;
            auto                                 result = Snapshot(image, storage, generation, owner, before);
            if (result != ClockResult::Ok)
                return finish(result);
            auto expected = before;
            if (requestedOperation == Operation::RunningCompressed)
                expected[0x12] = static_cast<unsigned char>(requestedBits);
            else if (requestedOperation == Operation::AnimationBlockID)
                std::memcpy(expected.data() + 0x14, &requestedBits, sizeof(requestedBits));
            else if (requestedOperation == Operation::NameHash)
                std::memcpy(expected.data(), &requestedBits, sizeof(requestedBits));
            else if (requestedOperation == Operation::SequenceCount)
            {
                const auto count = static_cast<std::uint16_t>(requestedBits);
                std::memcpy(expected.data() + 0x10, &count, sizeof(count));
            }
            else
                return finish(ClockResult::InvalidArgument);
#if defined(MTA_DE_PRIVATE_ANIMATION_HIERARCHY_SCALAR_WRITE)
            const auto entry = write ? write : &MTA_DE_PRIVATE_ANIMATION_HIERARCHY_SCALAR_WRITE;
#else
            const auto entry = write ? write : &PrivateHierarchyStorageDetail::WriteScalar;
#endif
            storage.state.store(State::Entered, std::memory_order_release);
            receipt.hierarchy = storage.Address();
            receipt.entered = true;
            result = entry(storage.Address(), requestedOperation, requestedBits, receipt.exceptionCode);
            receipt.accepted = result == ClockResult::Ok;
            // This source-defined value exists as soon as its complete DWORD
            // write is acknowledged. A later lost owner/read acknowledgement
            // must not roll its definedness back. Initialize preserves it.
            if (receipt.accepted && requestedOperation == Operation::NameHash)
                storage.hashDefined = true;
            if (receipt.accepted && requestedOperation == Operation::SequenceCount)
                storage.expectedCount = static_cast<std::uint16_t>(requestedBits);
            if (result != ClockResult::Ok)
                return finish(result);
            // Only this acknowledged CPU store can restore its entered state.
            // A concurrently latched publication marker is never overwritten.
            auto entered = State::Entered;
            if (!storage.state.compare_exchange_strong(entered, State::Ready, std::memory_order_acq_rel))
                return finish(ClockResult::OwnerConflict);
            PrivateHierarchyStorageDetail::Bytes after;
            if ((result = Snapshot(image, storage, generation, owner, after)) != ClockResult::Ok)
                return finish(result);
            if (std::memcmp(after.data(), expected.data(), after.size()))
                return finish(ClockResult::BindingChanged);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult Read(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner,
                                NativeAnimationHierarchyValues& output) noexcept
        {
            if (storage.expectedSequences || storage.pointerAckPending)
                return ClockResult::OwnerConflict;
            if (!PrivateHierarchyStorageDetail::Context::TryEnter())
                return ClockResult::Busy;
            PrivateHierarchyStorageDetail::Context::Scope scope;
            return ReadWithin(image, storage, generation, owner, output);
        }
        static ClockResult ReadWithin(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner,
                                      NativeAnimationHierarchyValues& output) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            PrivateHierarchyStorageDetail::Bytes bytes;
            auto                                 result = Snapshot(image, storage, generation, owner, bytes);
            if (result != ClockResult::Ok)
                return result;
            NativeAnimationHierarchyValues next;
            next.interfaceAddress = storage.Address();
            next.sequenceCount = PrivateHierarchyStorageDetail::Field<std::uint16_t>(bytes, 0x10);
            next.runningCompressed = PrivateHierarchyStorageDetail::Field<std::uint8_t>(bytes, 0x12);
            next.blockId = PrivateHierarchyStorageDetail::Field<std::int32_t>(bytes, 0x14);
            next.totalTimeBits = PrivateHierarchyStorageDetail::Field<std::uint32_t>(bytes, 0x18);
            next.hashDefined = storage.hashDefined;
            // No semantic hash read occurs before a qualified defining write.
            // hash0 with hashDefined=false is transport, never a native value.
            if (next.hashDefined)
                next.hash = PrivateHierarchyStorageDetail::Field<std::uint32_t>(bytes, 0);
            output = next;
            return ClockResult::Ok;
        }
        static ClockResult SetSequencesWithin(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::uintptr_t requested, CheckOwner owner,
                                              Receipt* output, NativePrivateAnimationHierarchyCalls::SequenceWrite write) noexcept
        {
            const auto target = requested;
            Receipt    receipt;
            const auto finish = [&](ClockResult result)
            {
                if (receipt.entered && result != ClockResult::Ok)
                {
                    auto       current = storage.GetState();
                    const auto failed = receipt.accepted ? State::Entered : State::PublicationPinned;
                    while ((current == State::Ready || current == State::Entered) &&
                           !storage.state.compare_exchange_weak(current, failed, std::memory_order_acq_rel))
                    {
                    }
                }
                if (output)
                    *output = receipt;
                return result;
            };
            if (generation != storage.generation || storage.pointerAckPending)
                return finish(ClockResult::OwnerConflict);
            PrivateHierarchyStorageDetail::Bytes before;
            auto                                 result = Snapshot(image, storage, generation, owner, before);
            if (result != ClockResult::Ok)
                return finish(result);
            auto expected = before;
            std::memcpy(expected.data() + 8, &target, sizeof(target));
#if defined(MTA_DE_PRIVATE_ANIMATION_HIERARCHY_SEQUENCES_WRITE)
            const auto entry = write ? write : &MTA_DE_PRIVATE_ANIMATION_HIERARCHY_SEQUENCES_WRITE;
#else
            const auto entry = write ? write : &PrivateHierarchyStorageDetail::WriteSequences;
#endif
            storage.state.store(State::Entered, std::memory_order_release);
            receipt.hierarchy = storage.Address();
            receipt.entered = true;
            result = entry(storage.Address(), target, receipt.exceptionCode);
            receipt.accepted = result == ClockResult::Ok;
            if (receipt.accepted)
            {
                // This is the durable actual-store receipt, before postflight.
                // Only its same-command read-only ACK can finish the operation.
                storage.expectedSequences = target;
                storage.pointerAckPending = true;
            }
            if (result != ClockResult::Ok)
                return finish(result);
            PrivateHierarchyStorageDetail::Bytes after;
            result = Snapshot(image, storage, generation, owner, after, true);
            if (result != ClockResult::Ok)
                return finish(result);
            if (std::memcmp(expected.data(), after.data(), after.size()))
                return finish(ClockResult::BindingChanged);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult AcknowledgeSequencesWithin(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::uintptr_t expected,
                                                      CheckOwner owner, Receipt* output) noexcept
        {
            Receipt receipt;
            receipt.hierarchy = storage.Address();
            const auto finish = [&](ClockResult result)
            {
                if (output)
                    *output = receipt;
                return result;
            };
            if (generation != storage.generation || storage.expectedSequences != expected)
                return finish(ClockResult::BindingChanged);
            PrivateHierarchyStorageDetail::Bytes bytes;
            auto                                 result = Snapshot(image, storage, generation, owner, bytes, true);
            if (result != ClockResult::Ok)
                return finish(result);
            auto current = storage.GetState();
            if (current == State::Entered && !storage.state.compare_exchange_strong(current, State::Ready, std::memory_order_acq_rel))
                return finish(ClockResult::OwnerConflict);
            storage.pointerAckPending = false;
            receipt.accepted = receipt.verified = receipt.acknowledged = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult Retire(std::uintptr_t image, Storage& storage, std::uint64_t generation, CheckOwner owner, Receipt* output) noexcept
        {
            Receipt    receipt;
            const auto finish = [&](ClockResult result)
            {
                if (output)
                    *output = receipt;
                return result;
            };
            if (!generation || storage.generation != generation)
                return finish(ClockResult::BindingChanged);
            if (!owner)
                return finish(ClockResult::NotInitialized);
            if (storage.expectedSequences || storage.pointerAckPending)
                return finish(ClockResult::OwnerConflict);
            const auto state = storage.GetState();
            if (state == State::PublicationPinned || state == State::Retired)
                return finish(ClockResult::OwnerConflict);
            if (!PrivateHierarchyStorageDetail::Context::TryEnter())
                return finish(ClockResult::Busy);
            PrivateHierarchyStorageDetail::Context::Scope scope;
            auto                                          result = PrivateHierarchyStorageDetail::Context::Owner(image, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            if (state == State::Ready)
            {
                PrivateHierarchyStorageDetail::Bytes bytes;
                if ((result = Snapshot(image, storage, generation, owner, bytes)) != ClockResult::Ok)
                    return finish(result);
            }
            else if (state == State::Entered)
            {
                // A failed CPU write may have lost its acknowledgement. It
                // cannot replay, and retirement must observe actual empty
                // fields twice; a nonnull child/cache remains retained.
                PrivateHierarchyStorageDetail::Bytes first, second;
                if (!PrivateHierarchyStorageDetail::ProfileMatches(image))
                    return finish(ClockResult::BindingChanged);
                if (!NativeModelReferenceMap::Copy(storage.Address(), first.data(), first.size()))
                    return finish(ClockResult::ReadFailure);
                if (!PrivateHierarchyStorageDetail::Matches(first, storage.expectedSequences, storage.expectedCount))
                {
                    RetainForeign(storage);
                    return finish(ClockResult::BindingChanged);
                }
                if ((result = PrivateHierarchyStorageDetail::Context::Owner(image, owner)) != ClockResult::Ok)
                    return finish(result);
                if (!NativeModelReferenceMap::Copy(storage.Address(), second.data(), second.size()))
                    return finish(ClockResult::ReadFailure);
                if (!PrivateHierarchyStorageDetail::Matches(second, storage.expectedSequences, storage.expectedCount))
                {
                    RetainForeign(storage);
                    return finish(ClockResult::BindingChanged);
                }
                if (std::memcmp(first.data(), second.data(), first.size()) || storage.GetState() != state)
                    return finish(ClockResult::BindingChanged);
            }
            // Fresh/failed records have never had a child attachment or native
            // entry in this restricted provider. Their raw uninitialized bits
            // are not child/cache authority and need no native cleanup.
            receipt.hierarchy = storage.Address();
            storage.state.store(State::Retired, std::memory_order_release);
            receipt.accepted = receipt.retired = true;
            if ((result = PrivateHierarchyStorageDetail::Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult Pin(Storage& storage, std::uint64_t generation) noexcept
        {
            if (!generation || storage.OwnerGeneration() != generation)
                return ClockResult::BindingChanged;
            auto expected = State::Ready;
            return storage.state.compare_exchange_strong(expected, State::PublicationPinned, std::memory_order_acq_rel) ? ClockResult::Ok
                                                                                                                        : ClockResult::OwnerConflict;
        }
    };
    namespace NativePrivateAnimationHierarchyCalls
    {
        ClockResult Initialize(std::uintptr_t image, Storage& storage, std::uint64_t generation, CheckOwner owner, Receipt* receipt, Write write) noexcept
        {
            return NativePrivateAnimationHierarchyStorageAccess::Initialize(image, storage, generation, owner, receipt, write);
        }
        ClockResult SetRunningCompressed(std::uintptr_t image, Storage& storage, std::uint64_t generation, bool compressed, CheckOwner owner, Receipt* receipt,
                                         ScalarWrite write) noexcept
        {
            const auto value = compressed;
            return NativePrivateAnimationHierarchyStorageAccess::SetScalar(image, storage, generation, ScalarOperation::RunningCompressed, value ? 1u : 0u,
                                                                           owner, receipt, write);
        }
        ClockResult SetAnimationBlockID(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::int32_t block, CheckOwner owner,
                                        Receipt* receipt, ScalarWrite write) noexcept
        {
            const auto    value = block;
            std::uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return NativePrivateAnimationHierarchyStorageAccess::SetScalar(image, storage, generation, ScalarOperation::AnimationBlockID, bits, owner, receipt,
                                                                           write);
        }
        ClockResult SetNameHash(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::uint32_t hash, CheckOwner owner, Receipt* receipt,
                                ScalarWrite write) noexcept
        {
            const auto requested = hash;
            return NativePrivateAnimationHierarchyStorageAccess::SetScalar(image, storage, generation, ScalarOperation::NameHash, requested, owner, receipt,
                                                                           write);
        }
        ClockResult SetSequenceCount(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::uint16_t count, CheckOwner owner, Receipt* receipt,
                                     ScalarWrite write) noexcept
        {
            return NativePrivateAnimationHierarchyStorageAccess::SetScalar(image, storage, generation, ScalarOperation::SequenceCount, count, owner, receipt,
                                                                           write);
        }
        ClockResult SetScalarHeld(const Epoch& epoch, Storage& storage, ScalarOperation operation, std::uint32_t bits, CheckOwner owner, Receipt* receipt,
                                  ScalarWrite write) noexcept
        {
            if (epoch.Generation() != storage.OwnerGeneration())
                return ClockResult::BindingChanged;
            return NativePrivateAnimationHierarchyStorageAccess::SetScalarWithin(epoch.Image(), storage, epoch.Generation(), operation, bits, owner, receipt,
                                                                                 write);
        }
        ClockResult SetEmptySequences(std::uintptr_t image, Storage& storage, std::uint64_t generation, CheckOwner owner, Receipt* output,
                                      SequenceWrite write) noexcept
        {
            if (output)
                *output = {};
            if (!NativePrivateAnimationHierarchyStorageAccess::NoChild(storage))
                return ClockResult::OwnerConflict;
            if (!PrivateHierarchyStorageDetail::Context::TryEnter())
                return ClockResult::Busy;
            PrivateHierarchyStorageDetail::Context::Scope held;
            Receipt                                       receipt, ack;
            auto                                          result = NativePrivateAnimationHierarchyStorageAccess::PointerPending(storage)
                                                                       ? NativePrivateAnimationHierarchyStorageAccess::AcknowledgeSequencesWithin(image, storage, generation, 0, owner, &receipt)
                                                                       : NativePrivateAnimationHierarchyStorageAccess::SetSequencesWithin(image, storage, generation, 0, owner, &receipt, write);
            if (result == ClockResult::Ok)
            {
                result = NativePrivateAnimationHierarchyStorageAccess::AcknowledgeSequencesWithin(image, storage, generation, 0, owner, &ack);
                receipt.acknowledged = ack.acknowledged;
                receipt.verified = result == ClockResult::Ok;
            }
            if (output)
                *output = receipt;
            return result;
        }
        ClockResult SetSequencesHeld(const Epoch& epoch, Storage& storage, std::uintptr_t target, CheckOwner owner, Receipt* receipt,
                                     SequenceWrite write) noexcept
        {
            if (target && target != epoch.ArrayTag())
            {
                if (receipt)
                    *receipt = {};
                return ClockResult::BindingChanged;
            }
            return NativePrivateAnimationHierarchyStorageAccess::SetSequencesWithin(epoch.Image(), storage, epoch.Generation(), target, owner, receipt, write);
        }
        ClockResult AcknowledgeSequencesHeld(const Epoch& epoch, Storage& storage, std::uintptr_t expected, CheckOwner owner, Receipt* receipt) noexcept
        {
            if (expected && expected != epoch.ArrayTag())
            {
                if (receipt)
                    *receipt = {};
                return ClockResult::BindingChanged;
            }
            return NativePrivateAnimationHierarchyStorageAccess::AcknowledgeSequencesWithin(epoch.Image(), storage, epoch.Generation(), expected, owner,
                                                                                            receipt);
        }
        ClockResult ReadHeld(const Epoch& epoch, const Storage& storage, CheckOwner owner, NativeAnimationHierarchyValues& output) noexcept
        {
            if (epoch.Generation() != storage.OwnerGeneration())
                return ClockResult::BindingChanged;
            return NativePrivateAnimationHierarchyStorageAccess::ReadWithin(epoch.Image(), storage, epoch.Generation(), owner, output);
        }
        ClockResult Read(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner,
                         NativeAnimationHierarchyValues& output) noexcept
        {
            return NativePrivateAnimationHierarchyStorageAccess::Read(image, storage, generation, owner, output);
        }
        ClockResult Retire(std::uintptr_t image, Storage& storage, std::uint64_t generation, CheckOwner owner, Receipt* receipt) noexcept
        {
            return NativePrivateAnimationHierarchyStorageAccess::Retire(image, storage, generation, owner, receipt);
        }
        ClockResult PinPublication(Storage& storage, std::uint64_t generation) noexcept
        {
            return NativePrivateAnimationHierarchyStorageAccess::Pin(storage, generation);
        }
    }
}
