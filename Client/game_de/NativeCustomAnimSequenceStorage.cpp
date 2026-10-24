/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exactly-once native array allocation and storage-only retirement
 ****************************************************************************/
#include "NativeCustomAnimSequenceStorage.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeAnimationSequenceRowCalls.h"
#include "NativeAnimationSequenceRowProfile.h"
#include <intrin.h>
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace CustomAnimSequenceStorageDetail
    {
        namespace Profile = NativeCustomAnimSequenceStorageProfile;
        namespace Heap = NativeModelReferenceHeap;
        namespace Context = NativeStreamingCompletionContext;
        namespace Image = NativeImageAccess;
        using State = NativeCustomAnimSequenceStorage::State;
        using Receipt = NativeCustomAnimSequenceStorageCalls::Receipt;
        using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
        constexpr std::uint32_t AllocEntered = 1, AllocReturned = 2, Initialized = 4, FreeEntered = 8, FreeReturned = 16;
        bool                    ProfileMatches(std::uintptr_t image) noexcept
        {
            if (!Heap::QualifyProfile(image))
                return false;
            for (const auto& pin : Profile::Code)
            {
                std::array<unsigned char, Profile::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool Slots(std::uintptr_t image, std::uintptr_t table, std::uintptr_t allocate, std::uintptr_t release) noexcept
        {
            std::uintptr_t a = 0, b = 0;
            return Heap::ReadOnlyImage(image, table, 0x40) && Image::ReadImage(image, table + 0x10, &a, sizeof(a)) &&
                   Image::ReadImage(image, table + 0x30, &b, sizeof(b)) && a == image + allocate && b == image + release;
        }
        ClockResult Capture(std::uintptr_t image, CheckOwner owner, Heap::Value& output) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = Context::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!ProfileMatches(image))
                return ClockResult::BindingChanged;
            Heap::Value next;
            if ((result = Heap::Capture(image, next)) != ClockResult::Ok)
                return result;
            if (!Slots(image, next.authorityVtable, Profile::AuthorityMallocRva, Profile::AuthorityFreeRva) ||
                (next.current != next.authority && !Slots(image, next.currentVtable, Profile::ProxyMallocRva, Profile::ProxyFreeRva)))
                return ClockResult::BindingChanged;
            if ((result = Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool RowProfileMatches(std::uintptr_t image) noexcept
        {
            namespace RowProfile = NativeAnimationSequenceRowProfile;
            for (const auto& pin : RowProfile::Code)
            {
                std::array<unsigned char, RowProfile::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool SameRow(const NativeAnimationSequenceRowCalls::Value& a, const NativeAnimationSequenceRowCalls::Value& b) noexcept
        {
            return a.row == b.row && a.keyFrames == b.keyFrames && a.boneTag == b.boneTag && a.flags == b.flags && a.frameCount == b.frameCount;
        }
        // Keep the real guarded CPU writer available even when a test chooses
        // an injection hook. It operates only on the already admitted row.
        ClockResult WriteRow(std::uintptr_t row, NativeAnimationSequenceRowCalls::Operation operation, const NativeAnimationSequenceRowCalls::Value& target,
                             std::uint32_t& exception) noexcept
        {
            using Operation = NativeAnimationSequenceRowCalls::Operation;
            __try
            {
                if (operation == Operation::Initialize)
                {
                    const std::uint16_t  unknown = 0xffff, zero = 0;
                    const std::uintptr_t null = 0;
                    std::memcpy(reinterpret_cast<void*>(row + Profile::BoneOffset), &unknown, sizeof(unknown));
                    std::memcpy(reinterpret_cast<void*>(row + Profile::FlagsOffset), &zero, sizeof(zero));
                    std::memcpy(reinterpret_cast<void*>(row + Profile::FrameCountOffset), &zero, sizeof(zero));
                    std::memcpy(reinterpret_cast<void*>(row + Profile::FramePointerOffset), &null, sizeof(null));
                    return ClockResult::Ok;
                }
                if (operation == Operation::SetBoneTag)
                {
                    // The saved native operation is OR, not replacement of a
                    // flag snapshot. Preserve every other actual flag bit.
                    InterlockedOr16(reinterpret_cast<volatile SHORT*>(row + Profile::FlagsOffset), 0x10);
                    std::memcpy(reinterpret_cast<void*>(row + Profile::BoneOffset), &target.boneTag, sizeof(target.boneTag));
                    return ClockResult::Ok;
                }
                return ClockResult::InvalidArgument;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult InitializeArray(std::uintptr_t base, std::size_t capacity, std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(std::size_t) == sizeof(std::uint64_t));
            const auto           count = static_cast<std::uint64_t>(capacity);
            const std::uint16_t  bone = 0xffff, zero = 0;
            const std::uintptr_t pointer = 0;
            __try
            {
                std::memcpy(reinterpret_cast<void*>(base), &count, sizeof(count));
                for (std::size_t index = 0; index != capacity; ++index)
                {
                    const auto row = base + Profile::CookieBytes + index * Profile::RowBytes;
                    // Preserve the source/native initialization itinerary:
                    // name64 and unrelated padding are not a fake hash field.
                    std::memcpy(reinterpret_cast<void*>(row + Profile::BoneOffset), &bone, sizeof(bone));
                    std::memcpy(reinterpret_cast<void*>(row + Profile::FlagsOffset), &zero, sizeof(zero));
                    std::memcpy(reinterpret_cast<void*>(row + Profile::FrameCountOffset), &zero, sizeof(zero));
                    std::memcpy(reinterpret_cast<void*>(row + Profile::FramePointerOffset), &pointer, sizeof(pointer));
                }
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#if !defined(MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE)
        ClockResult NativeMalloc(NativeCustomAnimSequenceStorageCalls::MallocFunction function, std::uintptr_t heap, std::size_t bytes, std::uint32_t alignment,
                                 std::uintptr_t& output, std::uint32_t& exception) noexcept
        {
            __try
            {
                output = function(heap, bytes, alignment);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
#if !defined(MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE)
        ClockResult NativeFree(NativeCustomAnimSequenceStorageCalls::FreeFunction function, std::uintptr_t heap, std::uintptr_t base,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                function(heap, base);
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
    bool NativeCustomAnimSequenceStorage::CanDiscard() const noexcept
    {
        const auto current = GetState();
        return current == State::Empty || current == State::Released;
    }
    bool NativeCustomAnimSequenceStorage::CanFreeStorage() const noexcept
    {
        namespace Detail = CustomAnimSequenceStorageDetail;
        const auto current = GetState();
        return (current == State::LiveUnpublished || current == State::FaultDebt) && !hierarchyAffinity && base && tag && (effects & Detail::AllocReturned) &&
               !(effects & Detail::FreeEntered);
    }
    struct NativeCustomAnimSequenceStorageAccess
    {
        using Storage = NativeCustomAnimSequenceStorage;
        using State = Storage::State;
        using Kind = Storage::Kind;
        using Receipt = NativeCustomAnimSequenceStorageCalls::Receipt;
        using CheckOwner = NativeCustomAnimSequenceStorageCalls::CheckOwner;
        static ClockResult Allocate(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::size_t amount, CheckOwner owner, Receipt* output,
                                    NativeCustomAnimSequenceStorageCalls::InvokeMalloc invoke, Kind kind) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            const auto capacity = kind == Kind::SequenceArray ? amount : 0;
            Receipt    receipt;
            receipt.generation = generation;
            receipt.capacity = capacity;
            bool       reserved = false;
            const auto finish = [&](ClockResult result)
            {
                if (reserved && !receipt.entered)
                    storage.state.store(State::Empty, std::memory_order_release);
                receipt.debt = storage.HasOutstandingResource();
                if (output)
                    *output = receipt;
                return result;
            };
            if (!amount)
            {
                receipt.verified = true;
                return finish(ClockResult::Ok);
            }
            if (!generation || (kind == Kind::SequenceArray &&
                                capacity > ((std::numeric_limits<std::size_t>::max)() - Detail::Profile::CookieBytes) / Detail::Profile::RowBytes))
                return finish(ClockResult::InvalidArgument);
            State expected = State::Empty;
            if (!storage.state.compare_exchange_strong(expected, State::Preparing, std::memory_order_acq_rel))
                return finish(ClockResult::OwnerConflict);
            reserved = true;
            if (!Detail::Context::TryEnter())
                return finish(ClockResult::Busy);
            Detail::Context::Scope scope;
            auto                   result = Detail::Capture(image, owner, storage.origin);
            if (result != ClockResult::Ok)
                return finish(result);
            storage.kind = kind;
            storage.capacity = capacity;
            storage.bytes = kind == Kind::SequenceArray ? Detail::Profile::CookieBytes + capacity * Detail::Profile::RowBytes : amount;
            storage.generation = generation;
            receipt.bytes = storage.bytes;
            receipt.heap = storage.origin.current;
            receipt.authority = storage.origin.authority;
            receipt.entryRva = storage.origin.current == storage.origin.authority ? Detail::Profile::AuthorityMallocRva : Detail::Profile::ProxyMallocRva;
#if defined(MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_CUSTOM_ANIM_SEQUENCE_MALLOC_INVOKE;
#else
            const auto entry = invoke ? invoke : &Detail::NativeMalloc;
#endif
            const auto     function = reinterpret_cast<NativeCustomAnimSequenceStorageCalls::MallocFunction>(image + receipt.entryRva);
            std::uintptr_t base = 0;
            storage.effects = Detail::AllocEntered;
            storage.state.store(State::AllocateEntered, std::memory_order_release);
            receipt.entered = true;
            result = entry(function, storage.origin.current, storage.bytes, 0, base, receipt.exceptionCode);
            if (result != ClockResult::Ok)
            {
                storage.state.store(State::FaultDebt, std::memory_order_release);
                return finish(result);
            }
            storage.effects |= Detail::AllocReturned;
            receipt.returned = true;
            if (!base)
            {
                storage.state.store(State::Released, std::memory_order_release);
                if (kind == Kind::SequenceArray)
                    return finish(ClockResult::WriteFailure);
                // MTA's frame allocator is nullable. A real null return owns
                // no storage, but still needs the same actual owner/heap
                // acknowledgement before it can become a successful null.
                Detail::Heap::Value after;
                if ((result = Detail::Capture(image, owner, after)) != ClockResult::Ok)
                    return finish(result);
                if (!Detail::Heap::Same(storage.origin, after))
                    return finish(ClockResult::BindingChanged);
                receipt.verified = true;
                return finish(ClockResult::Ok);
            }
            // Retain a known returned allocation before any fallible post-call
            // observation. A lost acknowledgement cannot erase its authority.
            storage.base = base;
            receipt.base = base;
            receipt.allocated = true;
            storage.state.store(State::FaultDebt, std::memory_order_release);
            if (base % alignof(std::uintptr_t) || storage.bytes > (std::numeric_limits<std::uintptr_t>::max)() - base ||
                !Detail::Heap::M::PrivateWritable(base, storage.bytes))
                return finish(ClockResult::WriteFailure);
            storage.tag = kind == Kind::SequenceArray ? base + Detail::Profile::CookieBytes : base;
            if (kind == Kind::SequenceArray)
            {
                if ((result = Detail::InitializeArray(base, capacity, receipt.exceptionCode)) != ClockResult::Ok)
                    return finish(result);
                storage.effects |= Detail::Initialized;
                receipt.initialized = true;
            }
            // Frame buffers return the original native allocation directly.
            // Their payload has no sequence cookie or initialization promise.
            Detail::Heap::Value after;
            if ((result = Detail::Capture(image, owner, after)) != ClockResult::Ok)
                return finish(result);
            if (!Detail::Heap::Same(storage.origin, after))
                return finish(ClockResult::BindingChanged);
            if (kind == Kind::SequenceArray)
            {
                std::uint64_t cookie = 0;
                if (!Detail::Heap::M::Copy(base, &cookie, sizeof(cookie)) || cookie != capacity)
                    return finish(ClockResult::BindingChanged);
            }
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            storage.state.store(State::LiveUnpublished, std::memory_order_release);
            receipt.readyTag = storage.tag;
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult FreeStorage(std::uintptr_t image, Storage& storage, std::uint64_t generation, CheckOwner owner, Receipt* output,
                                       NativeCustomAnimSequenceStorageCalls::InvokeFree invoke, Kind kind) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            Receipt receipt;
            receipt.generation = generation;
            const auto prior = storage.GetState();
            bool       reserved = false;
            const auto finish = [&](ClockResult result)
            {
                if (reserved && !receipt.entered)
                    storage.state.store(prior, std::memory_order_release);
                receipt.debt = storage.HasOutstandingResource();
                if (output)
                    *output = receipt;
                return result;
            };
            if (!generation || generation != storage.generation || kind != storage.kind)
                return finish(ClockResult::OwnerConflict);
            if (!storage.CanFreeStorage())
                return finish(ClockResult::OwnerConflict);
            State expected = prior;
            if (!storage.state.compare_exchange_strong(expected, State::Preparing, std::memory_order_acq_rel))
                return finish(ClockResult::Busy);
            reserved = true;
            if (!Detail::Context::TryEnter())
                return finish(ClockResult::Busy);
            Detail::Context::Scope scope;
            Detail::Heap::Value    current;
            auto                   result = Detail::Capture(image, owner, current);
            if (result != ClockResult::Ok)
                return finish(result);
            // A current validated proxy may differ between callbacks while
            // routing to the same real allocation authority. Never bypass it.
            if (current.authority != storage.origin.authority || current.authorityVtable != storage.origin.authorityVtable ||
                !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes))
                return finish(ClockResult::BindingChanged);
            if (kind == Kind::SequenceArray && (storage.effects & Detail::Initialized))
            {
                std::uint64_t cookie = 0;
                if (!Detail::Heap::M::Copy(storage.base, &cookie, sizeof(cookie)) || cookie != storage.capacity)
                    return finish(ClockResult::BindingChanged);
            }
            // Recheck the existing owner/GT/GC fence after the last storage
            // observation. Frames have no cookie or payload read here.
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            // Partial unpublished initialization can still be retired through
            // its private known base: storage-free never walks a cookie/frame.
            receipt.base = storage.base;
            receipt.capacity = storage.capacity;
            receipt.bytes = storage.bytes;
            receipt.heap = current.current;
            receipt.authority = current.authority;
            receipt.entryRva = current.current == current.authority ? Detail::Profile::AuthorityFreeRva : Detail::Profile::ProxyFreeRva;
#if defined(MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_CUSTOM_ANIM_SEQUENCE_FREE_INVOKE;
#else
            const auto entry = invoke ? invoke : &Detail::NativeFree;
#endif
            const auto function = reinterpret_cast<NativeCustomAnimSequenceStorageCalls::FreeFunction>(image + receipt.entryRva);
            storage.effects |= Detail::FreeEntered;
            storage.state.store(State::FreeEntered, std::memory_order_release);
            receipt.entered = true;
            result = entry(function, current.current, storage.base, receipt.exceptionCode);
            if (result != ClockResult::Ok)
            {
                storage.state.store(State::FaultDebt, std::memory_order_release);
                return finish(result);
            }
            storage.effects |= Detail::FreeReturned;
            storage.state.store(State::Released, std::memory_order_release);
            receipt.returned = receipt.released = true;
            // Do not inspect freed storage, even if the later owner/heap fence
            // fails. Known consumption must precede acknowledgement checks.
            Detail::Heap::Value after;
            if ((result = Detail::Capture(image, owner, after)) != ClockResult::Ok)
                return finish(result);
            if (!Detail::Heap::Same(current, after))
                return finish(ClockResult::BindingChanged);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult RowResourceShape(const Storage& storage, std::uint64_t generation, Kind kind) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!generation || generation != storage.generation)
                return ClockResult::OwnerConflict;
            if (storage.kind != kind || storage.GetState() != State::LiveUnpublished || storage.hierarchyAffinity || !storage.base || !storage.tag ||
                !storage.bytes || !(storage.effects & Detail::AllocReturned))
                return ClockResult::BindingChanged;
            if (storage.base % alignof(std::uintptr_t) || storage.bytes > (std::numeric_limits<std::uintptr_t>::max)() - storage.base)
                return ClockResult::BindingChanged;
            if (kind == Kind::SequenceArray)
            {
                if (!(storage.effects & Detail::Initialized) || !storage.capacity ||
                    storage.capacity > ((std::numeric_limits<std::size_t>::max)() - Detail::Profile::CookieBytes) / Detail::Profile::RowBytes ||
                    storage.bytes != Detail::Profile::CookieBytes + storage.capacity * Detail::Profile::RowBytes ||
                    storage.tag != storage.base + Detail::Profile::CookieBytes)
                    return ClockResult::BindingChanged;
            }
            else if (storage.tag != storage.base)
                return ClockResult::BindingChanged;
            return ClockResult::Ok;
        }
        static ClockResult CaptureRow(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::size_t index, CheckOwner owner,
                                      NativeAnimationSequenceRowCalls::Value& output) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = RowResourceShape(storage, generation, Kind::SequenceArray);
            if (result != ClockResult::Ok)
                return result;
            if (index >= storage.capacity)
                return ClockResult::InvalidArgument;
            Detail::Heap::Value heap, after;
            if ((result = Detail::Capture(image, owner, heap)) != ClockResult::Ok)
                return result;
            if (heap.authority != storage.origin.authority || heap.authorityVtable != storage.origin.authorityVtable ||
                !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes) || !Detail::RowProfileMatches(image))
                return ClockResult::BindingChanged;
            std::uint64_t cookie = 0, cookieAfter = 0;
            if (!Detail::Heap::M::Copy(storage.base, &cookie, sizeof(cookie)))
                return ClockResult::ReadFailure;
            if (cookie != storage.capacity)
                return ClockResult::BindingChanged;
            NativeAnimationSequenceRowCalls::Value next;
            next.row = storage.tag + index * Detail::Profile::RowBytes;
            static_assert(sizeof(std::uintptr_t) == 8);
            // Read only bone/flags/count/padding/frame-pointer tail. Name64 is
            // unrelated physical storage, never a classic logical hash union.
            std::array<unsigned char, 16> tail{};
            if (!Detail::Heap::M::Copy(next.row + Detail::Profile::BoneOffset, tail.data(), tail.size()) ||
                !Detail::Heap::M::Copy(storage.base, &cookieAfter, sizeof(cookieAfter)))
                return ClockResult::ReadFailure;
            if (cookieAfter != cookie || (result = RowResourceShape(storage, generation, Kind::SequenceArray)) != ClockResult::Ok)
                return cookieAfter != cookie ? ClockResult::BindingChanged : result;
            std::memcpy(&next.boneTag, tail.data(), sizeof(next.boneTag));
            std::memcpy(&next.flags, tail.data() + Detail::Profile::FlagsOffset - Detail::Profile::BoneOffset, sizeof(next.flags));
            std::memcpy(&next.frameCount, tail.data() + Detail::Profile::FrameCountOffset - Detail::Profile::BoneOffset, sizeof(next.frameCount));
            std::memcpy(&next.keyFrames, tail.data() + Detail::Profile::FramePointerOffset - Detail::Profile::BoneOffset, sizeof(next.keyFrames));
            if ((result = Detail::Capture(image, owner, after)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(heap, after) || !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes) || !Detail::RowProfileMatches(image))
                return ClockResult::BindingChanged;
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok ||
                (result = RowResourceShape(storage, generation, Kind::SequenceArray)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        static ClockResult ReadRow(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::size_t index, CheckOwner owner,
                                   NativeAnimationSequenceRowCalls::Value& output) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope                 scope;
            NativeAnimationSequenceRowCalls::Value first, second;
            auto                                   result = CaptureRow(image, storage, generation, index, owner, first);
            if (result != ClockResult::Ok || (result = CaptureRow(image, storage, generation, index, owner, second)) != ClockResult::Ok)
                return result;
            if (!Detail::SameRow(first, second))
                return ClockResult::OwnerConflict;
            output = second;
            return ClockResult::Ok;
        }
        static ClockResult MutateRow(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::size_t index,
                                     NativeAnimationSequenceRowCalls::Operation operation, std::int32_t boneTag, CheckOwner owner,
                                     NativeAnimationSequenceRowCalls::Receipt* output, NativeAnimationSequenceRowCalls::Write write) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            namespace Row = NativeAnimationSequenceRowCalls;
            const auto   requestedBone = boneTag;
            Row::Receipt receipt;
            const auto   finish = [&](ClockResult result)
            {
                // Metadata uncertainty never erases the underlying allocation
                // journal. Storage-only retirement remains available, while
                // the same row cannot replay after an entered failure.
                if (receipt.entered && result != ClockResult::Ok)
                    storage.state.store(State::FaultDebt, std::memory_order_release);
                if (output)
                    *output = receipt;
                return result;
            };
            if (!Detail::Context::TryEnter())
                return finish(ClockResult::Busy);
            Detail::Context::Scope scope;
            Row::Value             first;
            auto                   result = CaptureRow(image, storage, generation, index, owner, first);
            if (result != ClockResult::Ok || (result = CaptureRow(image, storage, generation, index, owner, receipt.before)) != ClockResult::Ok)
                return finish(result);
            if (!Detail::SameRow(first, receipt.before))
                return finish(ClockResult::OwnerConflict);
            if (operation == Row::Operation::SetBoneTag && requestedBone == -1)
            {
                receipt.after = receipt.before;
                receipt.accepted = receipt.verified = true;
                return finish(ClockResult::Ok);
            }
            auto target = receipt.before;
            if (operation == Row::Operation::Initialize)
            {
                target.boneTag = 0xffff;
                target.flags = target.frameCount = 0;
                target.keyFrames = 0;
            }
            else
            {
                target.boneTag = static_cast<std::uint16_t>(requestedBone);
                target.flags = static_cast<std::uint16_t>(target.flags | 0x10u);
            }
            if (!Detail::Heap::M::PrivateWritable(receipt.before.row, Detail::Profile::RowBytes))
                return finish(ClockResult::WriteFailure);
#if defined(MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE)
            const auto entry = write ? write : &MTA_DE_ANIMATION_SEQUENCE_ROW_WRITE;
#else
            const auto entry = write ? write : &Detail::WriteRow;
#endif
            receipt.entered = true;
            result = entry(receipt.before.row, operation, target, receipt.exceptionCode);
            receipt.accepted = result == ClockResult::Ok;
            if (result != ClockResult::Ok)
                return finish(result);
            if ((result = CaptureRow(image, storage, generation, index, owner, receipt.after)) != ClockResult::Ok)
                return finish(result);
            if (operation == Row::Operation::Initialize)
            {
                if (!Detail::SameRow(receipt.after, target))
                    return finish(ClockResult::BindingChanged);
            }
            else if (receipt.after.boneTag != target.boneTag || !(receipt.after.flags & 0x10u))
                return finish(ClockResult::BindingChanged);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult CheckFramePointer(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::uintptr_t pointer,
                                             CheckOwner owner) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            auto result = RowResourceShape(storage, generation, Kind::KeyFrames);
            if (result != ClockResult::Ok)
                return result;
            if (pointer < storage.base || pointer - storage.base >= storage.bytes)
                return ClockResult::InvalidArgument;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope scope;
            Detail::Heap::Value    first, second;
            if ((result = Detail::Capture(image, owner, first)) != ClockResult::Ok)
                return result;
            if (first.authority != storage.origin.authority || first.authorityVtable != storage.origin.authorityVtable ||
                !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes))
                return ClockResult::BindingChanged;
            // No payload byte is read. A range-shaped pointer alone does not
            // replace this record's actual allocation/heap/current owner.
            if ((result = Detail::Capture(image, owner, second)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(first, second) || !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes))
                return ClockResult::BindingChanged;
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            return RowResourceShape(storage, generation, Kind::KeyFrames);
        }
        static ClockResult PinPublication(Storage& storage, std::uint64_t generation, std::uintptr_t affinity) noexcept
        {
            if (!affinity || !generation || generation != storage.generation)
                return ClockResult::OwnerConflict;
            State expected = State::LiveUnpublished;
            if (!storage.state.compare_exchange_strong(expected, State::Preparing, std::memory_order_acq_rel))
                return ClockResult::OwnerConflict;
            storage.hierarchyAffinity = affinity;
            storage.state.store(State::PublicationPinned, std::memory_order_release);
            return ClockResult::Ok;
        }
    };
    namespace NativeCustomAnimSequenceStorageCalls
    {
        ClockResult Allocate(std::uintptr_t image, NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, std::size_t capacity, CheckOwner owner,
                             Receipt* output, InvokeMalloc invoke) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::Allocate(image, storage, generation, capacity, owner, output, invoke,
                                                                   NativeCustomAnimSequenceStorage::Kind::SequenceArray);
        }
        ClockResult FreeStorage(std::uintptr_t image, NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, CheckOwner owner, Receipt* output,
                                InvokeFree invoke) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::FreeStorage(image, storage, generation, owner, output, invoke,
                                                                      NativeCustomAnimSequenceStorage::Kind::SequenceArray);
        }
        ClockResult PinPublication(NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, std::uintptr_t affinity) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::PinPublication(storage, generation, affinity);
        }
    }
    namespace NativeAnimationKeyFrameStorageCalls
    {
        ClockResult Allocate(std::uintptr_t image, NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, std::uint32_t bytes, CheckOwner owner,
                             Receipt* output, InvokeMalloc invoke) noexcept
        {
            // The SDK's full uint32 request widens exactly to native size_t;
            // no multiplication, header or narrowing can overflow it here.
            static_assert(sizeof(std::size_t) >= sizeof(std::uint32_t));
            return NativeCustomAnimSequenceStorageAccess::Allocate(image, storage, generation, static_cast<std::size_t>(bytes), owner, output, invoke,
                                                                   NativeCustomAnimSequenceStorage::Kind::KeyFrames);
        }
        ClockResult FreeStorage(std::uintptr_t image, NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, CheckOwner owner, Receipt* output,
                                InvokeFree invoke) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::FreeStorage(image, storage, generation, owner, output, invoke,
                                                                      NativeCustomAnimSequenceStorage::Kind::KeyFrames);
        }
    }
    namespace NativeAnimationSequenceRowCalls
    {
        ClockResult Read(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::size_t index, CheckOwner owner, Value& output) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::ReadRow(image, storage, generation, index, owner, output);
        }
        ClockResult Initialize(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::size_t index, CheckOwner owner, Receipt* output,
                               Write write) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::MutateRow(image, storage, generation, index, Operation::Initialize, 0, owner, output, write);
        }
        ClockResult SetBoneTag(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::size_t index, std::int32_t boneTag, CheckOwner owner,
                               Receipt* output, Write write) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::MutateRow(image, storage, generation, index, Operation::SetBoneTag, boneTag, owner, output, write);
        }
        ClockResult CheckFramePointer(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::uintptr_t pointer, CheckOwner owner) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::CheckFramePointer(image, storage, generation, pointer, owner);
        }
    }
}
