/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exactly-once native array allocation and storage-only retirement
 ****************************************************************************/
#include "NativeCustomAnimSequenceStorage.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeAnimationAssociationOuterStorageCalls.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeAnimationSequenceRowCalls.h"
#include "NativeAnimationSequenceRowProfile.h"
#include "NativeAnimationSequenceCopyCalls.h"
#include "NativeAnimationSequenceFrameViewCalls.h"
#include "NativeAnimationSequenceDependencyCalls.h"
#include "NativeAnimationSequenceFactoryCalls.h"
#include "NativeAnimationSequenceCleanupCalls.h"
#include "NativeAnimationSequenceOperationCalls.h"
#include "NativeAnimationSequencePayloadCalls.h"
#include "NativeAnimationSequenceNameCalls.h"
#include "NativeAnimationSequenceNameProfile.h"
#include "NativeAnimationSequenceFrameBindCalls.h"
#include "NativeAnimationSequenceFrameBindProfile.h"
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
        // The real CPU copy stays available to injection hooks. Captured
        // source bytes never alias this admitted destination row.
        ClockResult CopyRow(std::uintptr_t target, const NativeAnimationSequenceCopyCalls::Record& source, std::uint32_t& exception) noexcept
        {
            static_assert(NativeAnimationSequenceCopyCalls::Record{}.bytes.size() == Profile::RowBytes);
            __try
            {
                std::memcpy(reinterpret_cast<void*>(target), source.bytes.data(), source.bytes.size());
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        bool NameProfileMatches(std::uintptr_t image) noexcept
        {
            namespace NameProfile = NativeAnimationSequenceNameProfile;
            for (const auto& pin : NameProfile::Code)
            {
                std::array<unsigned char, NameProfile::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        // The actual safe owned-buffer writer remains available to test
        // hooks. Only the bounded terminated spelling and selected bone are
        // stores; existing padding, flag bits and frame aliases stay intact.
        ClockResult WriteName(std::uintptr_t row, const NativeAnimationSequenceName& name, std::size_t terminatedBytes, bool projectBone, std::uint16_t boneTag,
                              std::uint32_t& exception) noexcept
        {
            if (!terminatedBytes || terminatedBytes > name.size())
                return ClockResult::InvalidArgument;
            __try
            {
                std::memcpy(reinterpret_cast<void*>(row), name.data(), terminatedBytes);
                if (projectBone)
                    std::memcpy(reinterpret_cast<void*>(row + Profile::BoneOffset), &boneTag, sizeof(boneTag));
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        bool FrameBindProfileMatches(std::uintptr_t image) noexcept
        {
            namespace BindProfile = NativeAnimationSequenceFrameBindProfile;
            for (const auto& pin : BindProfile::Code)
            {
                std::array<unsigned char, BindProfile::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        // Retain the existing writer name/test seam while sharing exactly
        // these metadata stores between explicit fresh encoded itineraries.
        // No payload conversion or general SDK SetKeyFrames policy is implied.
        ClockResult WriteCompressedFrames(std::uintptr_t row, const NativeAnimationSequenceRowCalls::Value& target, std::uint32_t& exception) noexcept
        {
            __try
            {
                std::memcpy(reinterpret_cast<void*>(row + Profile::FlagsOffset), &target.flags, sizeof(target.flags));
                std::memcpy(reinterpret_cast<void*>(row + Profile::FrameCountOffset), &target.frameCount, sizeof(target.frameCount));
                std::memcpy(reinterpret_cast<void*>(row + Profile::FramePointerOffset), &target.keyFrames, sizeof(target.keyFrames));
                return ClockResult::Ok;
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
                if (kind != Kind::KeyFrames)
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
        static ClockResult RowResourceShape(const Storage& storage, std::uint64_t generation, Kind kind, bool acceptedPrivateDebt = false) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!generation || generation != storage.generation)
                return ClockResult::OwnerConflict;
            if (storage.kind != kind ||
                (storage.GetState() != State::LiveUnpublished &&
                 !(acceptedPrivateDebt && storage.GetState() == State::FaultDebt && storage.CanFreeStorage())) ||
                storage.hierarchyAffinity || !storage.base || !storage.tag || !storage.bytes || !(storage.effects & Detail::AllocReturned))
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
        static ClockResult CaptureDynamicOuter(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner,
                                               NativeModelReferenceHeap::Value& output) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            auto result = RowResourceShape(storage, generation, Kind::DynamicAnimation, true);
            if (result != ClockResult::Ok)
                return result;
            if (storage.bytes != 104 || storage.tag != storage.base)
                return ClockResult::BindingChanged;
            Detail::Heap::Value current;
            if ((result = Detail::Capture(image, owner, current)) != ClockResult::Ok)
                return result;
            if (current.authority != storage.origin.authority || current.authorityVtable != storage.origin.authorityVtable ||
                !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes))
                return ClockResult::BindingChanged;
            output = current;
            return ClockResult::Ok;
        }
        static ClockResult ReadArrayCapacity(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner,
                                             std::size_t& output) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = RowResourceShape(storage, generation, Kind::SequenceArray);
            if (result != ClockResult::Ok)
                return result;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope held;
            Detail::Heap::Value    first, after;
            if ((result = Detail::Capture(image, owner, first)) != ClockResult::Ok)
                return result;
            if (first.authority != storage.origin.authority || first.authorityVtable != storage.origin.authorityVtable ||
                !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes))
                return ClockResult::BindingChanged;
            std::uint64_t cookie = 0, confirmed = 0;
            // Source At reads only the original allocation's capacity cookie.
            // In particular a legitimate OOB lookup never inspects row0's
            // flags/frame/count or turns their unrelated state into absence.
            if (!Detail::Heap::M::Copy(storage.base, &cookie, sizeof(cookie)))
                return ClockResult::ReadFailure;
            if (cookie != storage.capacity)
                return ClockResult::BindingChanged;
            if ((result = Detail::Capture(image, owner, after)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(first, after) || !Detail::Heap::M::PrivateWritable(storage.base, storage.bytes))
                return ClockResult::BindingChanged;
            if (!Detail::Heap::M::Copy(storage.base, &confirmed, sizeof(confirmed)))
                return ClockResult::ReadFailure;
            if (confirmed != cookie || (result = RowResourceShape(storage, generation, Kind::SequenceArray)) != ClockResult::Ok)
                return confirmed != cookie ? ClockResult::BindingChanged : result;
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = storage.capacity;
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
        static ClockResult CaptureWholeRow(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::size_t index, CheckOwner owner,
                                           NativeAnimationSequenceCopyCalls::Record& output, std::uintptr_t& row) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            NativeAnimationSequenceRowCalls::Value   first, second, final;
            NativeAnimationSequenceCopyCalls::Record bytes, after;
            auto                                     result = CaptureRow(image, storage, generation, index, owner, first);
            if (result != ClockResult::Ok)
                return result;
            if (!Detail::Heap::M::Copy(first.row, bytes.bytes.data(), bytes.bytes.size()))
                return ClockResult::ReadFailure;
            if ((result = CaptureRow(image, storage, generation, index, owner, second)) != ClockResult::Ok)
                return result;
            if (!Detail::SameRow(first, second))
                return ClockResult::OwnerConflict;
            if (!Detail::Heap::M::Copy(second.row, after.bytes.data(), after.bytes.size()))
                return ClockResult::ReadFailure;
            if (bytes.bytes != after.bytes)
                return ClockResult::OwnerConflict;
            if ((result = CaptureRow(image, storage, generation, index, owner, final)) != ClockResult::Ok)
                return result;
            if (!Detail::SameRow(second, final))
                return ClockResult::OwnerConflict;
            output = after;
            row = final.row;
            return ClockResult::Ok;
        }
        static ClockResult CopySequence(std::uintptr_t image, Storage& target, std::size_t targetIndex, const Storage& source, std::size_t sourceIndex,
                                        std::uint64_t generation, CheckOwner owner, NativeAnimationSequenceCopyCalls::Receipt* output,
                                        NativeAnimationSequenceCopyCalls::Write write) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            namespace Copy = NativeAnimationSequenceCopyCalls;
            Copy::Receipt receipt;
            const auto    finish = [&](ClockResult result)
            {
                if (receipt.entered && result != ClockResult::Ok)
                    target.state.store(State::FaultDebt, std::memory_order_release);
                if (output)
                    *output = receipt;
                return result;
            };
            if (!Detail::Context::TryEnter())
                return finish(ClockResult::Busy);
            Detail::Context::Scope scope;
            // One current native heap must qualify both actual private arrays.
            // Matching readable addresses never substitute for these records.
            if (target.origin.authority != source.origin.authority || target.origin.authorityVtable != source.origin.authorityVtable)
                return finish(ClockResult::BindingChanged);
            Copy::Record intent, before;
            auto         result = CaptureWholeRow(image, source, generation, sourceIndex, owner, intent, receipt.sourceRow);
            if (result != ClockResult::Ok ||
                (result = CaptureWholeRow(image, target, generation, targetIndex, owner, before, receipt.targetRow)) != ClockResult::Ok)
                return finish(result);
            receipt.sourceValue.row = receipt.sourceRow;
            std::memcpy(&receipt.sourceValue.keyFrames, intent.bytes.data() + Detail::Profile::FramePointerOffset, sizeof(receipt.sourceValue.keyFrames));
            std::memcpy(&receipt.sourceValue.boneTag, intent.bytes.data() + Detail::Profile::BoneOffset, sizeof(receipt.sourceValue.boneTag));
            std::memcpy(&receipt.sourceValue.flags, intent.bytes.data() + Detail::Profile::FlagsOffset, sizeof(receipt.sourceValue.flags));
            std::memcpy(&receipt.sourceValue.frameCount, intent.bytes.data() + Detail::Profile::FrameCountOffset, sizeof(receipt.sourceValue.frameCount));
            if (receipt.sourceRow == receipt.targetRow)
            {
                receipt.selfCopy = receipt.verified = true;
                return finish(ClockResult::Ok);
            }
            if (!Detail::Heap::M::PrivateWritable(receipt.targetRow, Detail::Profile::RowBytes))
                return finish(ClockResult::WriteFailure);
#if defined(MTA_DE_ANIMATION_SEQUENCE_COPY_WRITE)
            const auto entry = write ? write : &MTA_DE_ANIMATION_SEQUENCE_COPY_WRITE;
#else
            const auto entry = write ? write : &Detail::CopyRow;
#endif
            receipt.copiedRecord = intent;
            receipt.entered = true;
            result = entry(receipt.targetRow, intent, receipt.exceptionCode);
            // This flag acknowledges the complete raw80 copy, independent of
            // subsequent owner/read refusal. The controller commits copied
            // logical metadata on this flag; it never frees any alias here.
            receipt.rawCopied = result == ClockResult::Ok;
            if (result != ClockResult::Ok)
                return finish(result);
            Copy::Record   after;
            std::uintptr_t row = 0;
            if ((result = CaptureWholeRow(image, target, generation, targetIndex, owner, after, row)) != ClockResult::Ok)
                return finish(result);
            if (row != receipt.targetRow || after.bytes != intent.bytes)
                return finish(ClockResult::BindingChanged);
            if ((result = RowResourceShape(source, generation, Kind::SequenceArray)) != ClockResult::Ok)
                return finish(result);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult CaptureNamedRow(std::uintptr_t image, const Storage& storage, std::uint64_t generation, std::size_t index, CheckOwner owner,
                                           NativeAnimationSequenceNameCalls::Record& output, std::uintptr_t& row) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!Detail::NameProfileMatches(image))
                return ClockResult::BindingChanged;
            NativeAnimationSequenceNameCalls::Record next;
            std::uintptr_t                           nextRow = 0;
            auto                                     result = CaptureWholeRow(image, storage, generation, index, owner, next, nextRow);
            if (result != ClockResult::Ok)
                return result;
            if (!Detail::NameProfileMatches(image))
                return ClockResult::BindingChanged;
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = next;
            row = nextRow;
            return ClockResult::Ok;
        }
        static ClockResult SetSequenceName(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::size_t index,
                                           const NativeAnimationSequenceName& name, std::uint32_t logicalHash, CheckOwner owner,
                                           NativeAnimationSequenceNameCalls::Receipt* output, NativeAnimationSequenceNameCalls::Write write) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            namespace NameCalls = NativeAnimationSequenceNameCalls;
            // These copied CPU inputs are the entire intent. Owner callbacks
            // cannot change the spelling/hash subsequently applied to the row.
            const auto         intent = name;
            const auto         hash = logicalHash;
            NameCalls::Receipt receipt;
            const auto         finish = [&](ClockResult result)
            {
                if (receipt.entered && result != ClockResult::Ok)
                    storage.state.store(State::FaultDebt, std::memory_order_release);
                if (output)
                    *output = receipt;
                return result;
            };
            std::size_t length = 0;
            if (!NativeAnimationSequenceNamePolicy::Length(intent, length))
                return finish(ClockResult::InvalidArgument);
            if (!owner)
                return finish(ClockResult::NotInitialized);
            if (!Detail::Context::TryEnter())
                return finish(ClockResult::Busy);
            Detail::Context::Scope scope;
            NameCalls::Record      first;
            std::uintptr_t         firstRow = 0;
            auto                   result = CaptureNamedRow(image, storage, generation, index, owner, first, firstRow);
            if (result != ClockResult::Ok ||
                (result = CaptureNamedRow(image, storage, generation, index, owner, receipt.before, receipt.row)) != ClockResult::Ok)
                return finish(result);
            if (firstRow != receipt.row || first.bytes != receipt.before.bytes)
                return finish(ClockResult::OwnerConflict);
            std::uint16_t flags = 0;
            std::memcpy(&flags, receipt.before.bytes.data() + Detail::Profile::FlagsOffset, sizeof(flags));
            const bool projectBone = (flags & 0x10u) != 0;
            const auto bone = static_cast<std::uint16_t>(hash);
            const auto terminatedBytes = length + 1;
            auto       expected = receipt.before;
            std::memcpy(expected.bytes.data(), intent.data(), terminatedBytes);
            if (projectBone)
                std::memcpy(expected.bytes.data() + Detail::Profile::BoneOffset, &bone, sizeof(bone));
            if (!Detail::Heap::M::PrivateWritable(receipt.row, Detail::Profile::RowBytes))
                return finish(ClockResult::WriteFailure);
#if defined(MTA_DE_ANIMATION_SEQUENCE_NAME_WRITE)
            const auto entry = write ? write : &MTA_DE_ANIMATION_SEQUENCE_NAME_WRITE;
#else
            const auto entry = write ? write : &Detail::WriteName;
#endif
            receipt.entered = true;
            result = entry(receipt.row, intent, terminatedBytes, projectBone, bone, receipt.exceptionCode);
            // This acknowledgement survives later owner/read failure. The
            // controller must commit its classic logical union on acceptance;
            // no frame/allocation journal is removed and no write is replayed.
            receipt.rawWritten = receipt.accepted = result == ClockResult::Ok;
            if (result != ClockResult::Ok)
                return finish(result);
            std::uintptr_t afterRow = 0;
            if ((result = CaptureNamedRow(image, storage, generation, index, owner, receipt.after, afterRow)) != ClockResult::Ok)
                return finish(result);
            if (afterRow != receipt.row || receipt.after.bytes != expected.bytes)
                return finish(ClockResult::BindingChanged);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult CaptureFrameBuffer(std::uintptr_t image, const Storage& frames, std::uint64_t generation, CheckOwner owner,
                                              CustomAnimSequenceStorageDetail::Heap::Value& output) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            auto result = RowResourceShape(frames, generation, Kind::KeyFrames);
            if (result != ClockResult::Ok)
                return result;
            Detail::Heap::Value first, second;
            if ((result = Detail::Capture(image, owner, first)) != ClockResult::Ok)
                return result;
            if (first.authority != frames.origin.authority || first.authorityVtable != frames.origin.authorityVtable ||
                !Detail::Heap::M::PrivateWritable(frames.base, frames.bytes))
                return ClockResult::BindingChanged;
            // The issued frame's exact original authority and private extent
            // are requalified without reading or initializing any payload.
            if ((result = Detail::Capture(image, owner, second)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(first, second) || !Detail::Heap::M::PrivateWritable(frames.base, frames.bytes))
                return ClockResult::BindingChanged;
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok ||
                (result = RowResourceShape(frames, generation, Kind::KeyFrames)) != ClockResult::Ok)
                return result;
            output = second;
            return ClockResult::Ok;
        }
        static ClockResult AttachFreshFrames(std::uintptr_t image, Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames,
                                             std::size_t count, bool root, bool compressed, CheckOwner owner,
                                             NativeAnimationSequenceFrameBindCalls::Receipt* output,
                                             NativeAnimationSequenceFrameBindCalls::Write    write) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            namespace Bind = NativeAnimationSequenceFrameBindCalls;
            namespace BindProfile = NativeAnimationSequenceFrameBindProfile;
            const auto    requestedCount = count;
            const auto    requestedRoot = root;
            const auto    requestedCompressed = compressed;
            Bind::Receipt receipt;
            receipt.count = requestedCount;
            const auto finish = [&](ClockResult result)
            {
                // A failed entered metadata write never discards either
                // allocation record. Only known storage-only cleanup remains;
                // the row cannot replay after this uncertain transaction.
                if (receipt.entered && result != ClockResult::Ok)
                    array.state.store(State::FaultDebt, std::memory_order_release);
                if (output)
                    *output = receipt;
                return result;
            };
            const auto stride = requestedCompressed ? (requestedRoot ? BindProfile::RootStride : BindProfile::ChildStride)
                                                    : (requestedRoot ? BindProfile::UncompressedRootStride : BindProfile::UncompressedChildStride);
            if (!requestedCount || requestedCount > (std::numeric_limits<std::size_t>::max)() / stride)
                return finish(ClockResult::InvalidArgument);
            receipt.bytes = requestedCount * stride;
            if (receipt.bytes > (std::numeric_limits<std::uint32_t>::max)())
                return finish(ClockResult::InvalidArgument);
            if (!owner)
                return finish(ClockResult::NotInitialized);
            auto result = RowResourceShape(array, generation, Kind::SequenceArray);
            if (result != ClockResult::Ok || (result = RowResourceShape(frames, generation, Kind::KeyFrames)) != ClockResult::Ok)
                return finish(result);
            if (array.origin.authority != frames.origin.authority || array.origin.authorityVtable != frames.origin.authorityVtable)
                return finish(ClockResult::BindingChanged);
            if (receipt.bytes > frames.bytes)
                return finish(ClockResult::InvalidArgument);
            if (!Detail::Context::TryEnter())
                return finish(ClockResult::Busy);
            Detail::Context::Scope scope;
            if (!Detail::FrameBindProfileMatches(image))
                return finish(ClockResult::BindingChanged);
            Detail::Heap::Value firstHeap, secondHeap;
            if ((result = CaptureFrameBuffer(image, frames, generation, owner, firstHeap)) != ClockResult::Ok)
                return finish(result);
            Bind::Record   first;
            std::uintptr_t firstRow = 0;
            if ((result = CaptureWholeRow(image, array, generation, rowIndex, owner, first, firstRow)) != ClockResult::Ok ||
                (result = CaptureFrameBuffer(image, frames, generation, owner, secondHeap)) != ClockResult::Ok ||
                (result = CaptureWholeRow(image, array, generation, rowIndex, owner, receipt.before, receipt.row)) != ClockResult::Ok)
                return finish(result);
            if (!Detail::Heap::Same(firstHeap, secondHeap) || firstRow != receipt.row || first.bytes != receipt.before.bytes)
                return finish(ClockResult::OwnerConflict);
            NativeAnimationSequenceRowCalls::Value target;
            target.row = receipt.row;
            std::memcpy(&target.boneTag, receipt.before.bytes.data() + Detail::Profile::BoneOffset, sizeof(target.boneTag));
            std::memcpy(&target.flags, receipt.before.bytes.data() + Detail::Profile::FlagsOffset, sizeof(target.flags));
            std::memcpy(&target.frameCount, receipt.before.bytes.data() + Detail::Profile::FrameCountOffset, sizeof(target.frameCount));
            std::memcpy(&target.keyFrames, receipt.before.bytes.data() + Detail::Profile::FramePointerOffset, sizeof(target.keyFrames));
            // Only Initialize -> Name -> optional Bone is admitted. This
            // avoids inventing original false/replacement/compression rules.
            if (target.keyFrames || target.frameCount || (target.flags & ~std::uint16_t{0x10}))
                return finish(ClockResult::InvalidArgument);
            if ((result = RowResourceShape(frames, generation, Kind::KeyFrames)) != ClockResult::Ok)
                return finish(result);
            if (!Detail::Heap::M::PrivateWritable(frames.base, frames.bytes) || !Detail::FrameBindProfileMatches(image))
                return finish(ClockResult::BindingChanged);
            receipt.frames = frames.base;
            // Each fresh producer begins without a frame or format flags.
            // The explicit uncompressed path adds no04 and never converts
            // existing compressed bytes or infers shared08 from a pointer.
            target.flags = static_cast<std::uint16_t>(target.flags | 0x01u | (requestedRoot ? 0x02u : 0u) | (requestedCompressed ? 0x04u : 0u));
            // Full source count already qualified the allocation byte span;
            // only this actual native field deliberately narrows to WORD.
            target.frameCount = static_cast<std::uint16_t>(requestedCount);
            target.keyFrames = receipt.frames;
            auto expected = receipt.before;
            std::memcpy(expected.bytes.data() + Detail::Profile::FlagsOffset, &target.flags, sizeof(target.flags));
            std::memcpy(expected.bytes.data() + Detail::Profile::FrameCountOffset, &target.frameCount, sizeof(target.frameCount));
            std::memcpy(expected.bytes.data() + Detail::Profile::FramePointerOffset, &target.keyFrames, sizeof(target.keyFrames));
            if (!Detail::Heap::M::PrivateWritable(receipt.row, Detail::Profile::RowBytes))
                return finish(ClockResult::WriteFailure);
#if defined(MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE)
            const auto entry = write ? write : &MTA_DE_ANIMATION_SEQUENCE_FRAME_BIND_WRITE;
#else
            const auto entry = write ? write : &Detail::WriteCompressedFrames;
#endif
            receipt.intended = expected;
            receipt.entered = true;
            result = entry(receipt.row, target, receipt.exceptionCode);
            receipt.accepted = result == ClockResult::Ok;
            if (result != ClockResult::Ok)
                return finish(result);
            Detail::Heap::Value afterHeap;
            if ((result = CaptureFrameBuffer(image, frames, generation, owner, afterHeap)) != ClockResult::Ok)
                return finish(result);
            std::uintptr_t afterRow = 0;
            if ((result = CaptureWholeRow(image, array, generation, rowIndex, owner, receipt.after, afterRow)) != ClockResult::Ok)
                return finish(result);
            if (!Detail::Heap::Same(firstHeap, afterHeap) || afterRow != receipt.row || receipt.after.bytes != expected.bytes ||
                !Detail::FrameBindProfileMatches(image))
                return finish(ClockResult::BindingChanged);
            if (!Detail::Heap::M::PrivateWritable(frames.base, frames.bytes))
                return finish(ClockResult::BindingChanged);
            if ((result = RowResourceShape(frames, generation, Kind::KeyFrames)) != ClockResult::Ok ||
                (result = Detail::Context::Owner(image, owner)) != ClockResult::Ok)
                return finish(result);
            receipt.verified = true;
            return finish(ClockResult::Ok);
        }
        static ClockResult AcknowledgePrivateRow(std::uintptr_t image, Storage& array, std::uint64_t generation, std::size_t index,
                                                 const NativeAnimationSequenceCopyCalls::Record& intent, bool frameBinding, CheckOwner owner) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            auto result = RowResourceShape(array, generation, Kind::SequenceArray, true);
            if (result != ClockResult::Ok || index >= array.capacity)
                return result != ClockResult::Ok ? result : ClockResult::InvalidArgument;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope gate;
            Detail::Heap::Value    before, after;
            if ((result = Detail::Capture(image, owner, before)) != ClockResult::Ok)
                return result;
            if (before.authority != array.origin.authority || before.authorityVtable != array.origin.authorityVtable || !Detail::RowProfileMatches(image) ||
                (frameBinding && !Detail::FrameBindProfileMatches(image)))
                return ClockResult::BindingChanged;
            const auto observe = [&]() noexcept
            {
                if (!Detail::Heap::M::PrivateWritable(array.base, array.bytes))
                    return ClockResult::BindingChanged;
                std::uint64_t                            cookie = 0;
                NativeAnimationSequenceCopyCalls::Record current;
                if (!Detail::Heap::M::Copy(array.base, &cookie, sizeof(cookie)) ||
                    !Detail::Heap::M::Copy(array.tag + index * Detail::Profile::RowBytes, current.bytes.data(), current.bytes.size()))
                    return ClockResult::ReadFailure;
                return cookie == array.capacity && current.bytes == intent.bytes ? ClockResult::Ok : ClockResult::BindingChanged;
            };
            if ((result = observe()) != ClockResult::Ok || (result = Detail::Capture(image, owner, after)) != ClockResult::Ok ||
                (result = observe()) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(before, after) || !Detail::RowProfileMatches(image) || (frameBinding && !Detail::FrameBindProfileMatches(image)))
                return ClockResult::BindingChanged;
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok)
                return result;
            // Only the controller's actual accepted writer receipt admits this
            // exact raw intent. Unknown/partial or publication states cannot
            // reach this read-only ACK, and no field store is ever replayed.
            auto expected = State::FaultDebt;
            if (!array.state.compare_exchange_strong(expected, State::LiveUnpublished, std::memory_order_acq_rel) && expected != State::LiveUnpublished)
                return ClockResult::OwnerConflict;
            return ClockResult::Ok;
        }
        static ClockResult QualifyStandaloneRetirement(std::uintptr_t image, const Storage& array, std::uint64_t generation, CheckOwner owner) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            auto result = RowResourceShape(array, generation, Kind::SequenceArray, true);
            if (result != ClockResult::Ok)
                return result;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope gate;
            Detail::Heap::Value    before, after;
            if ((result = Detail::Capture(image, owner, before)) != ClockResult::Ok)
                return result;
            if (before.authority != array.origin.authority || before.authorityVtable != array.origin.authorityVtable ||
                !Detail::Heap::M::PrivateWritable(array.base, array.bytes))
                return ClockResult::BindingChanged;
            std::uint64_t first = 0, last = 0;
            if (!Detail::Heap::M::Copy(array.base, &first, sizeof(first)))
                return ClockResult::ReadFailure;
            if ((result = Detail::Capture(image, owner, after)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::M::Copy(array.base, &last, sizeof(last)))
                return ClockResult::ReadFailure;
            if (first != array.capacity || last != first || !Detail::Heap::Same(before, after) || !Detail::Heap::M::PrivateWritable(array.base, array.bytes))
                return ClockResult::BindingChanged;
            return Detail::Context::Owner(image, owner);
        }
        static ClockResult WithCleanupArray(std::uintptr_t image, Storage& array, std::uint64_t generation, CheckOwner owner,
                                            NativeAnimationSequenceDependencyCalls::ResolveFrame resolve, void* resolveContext,
                                            NativeAnimationSequenceDependencyCalls::Action action, void* actionContext) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!owner || !resolve || !action)
                return ClockResult::NotInitialized;
            auto result = RowResourceShape(array, generation, Kind::SequenceArray);
            if (result != ClockResult::Ok)
                return result;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope gate;
            Detail::Heap::Value    before, after;
            if ((result = Detail::Capture(image, owner, before)) != ClockResult::Ok)
                return result;
            if (before.authority != array.origin.authority || before.authorityVtable != array.origin.authorityVtable || !Detail::RowProfileMatches(image))
                return ClockResult::BindingChanged;
            const auto snapshot = [&]() noexcept
            {
                if (!Detail::Heap::M::PrivateWritable(array.base, array.bytes))
                    return ClockResult::BindingChanged;
                std::uint64_t cookie = 0, confirmed = 0;
                if (!Detail::Heap::M::Copy(array.base, &cookie, sizeof(cookie)))
                    return ClockResult::ReadFailure;
                if (cookie != array.capacity)
                    return ClockResult::BindingChanged;
                for (std::size_t index = 0; index != array.capacity; ++index)
                {
                    NativeAnimationSequenceRowCalls::Value row;
                    row.row = array.tag + index * Detail::Profile::RowBytes;
                    std::array<unsigned char, 16> tail{};
                    if (!Detail::Heap::M::Copy(row.row + Detail::Profile::BoneOffset, tail.data(), tail.size()))
                        return ClockResult::ReadFailure;
                    std::memcpy(&row.boneTag, tail.data(), sizeof(row.boneTag));
                    std::memcpy(&row.flags, tail.data() + Detail::Profile::FlagsOffset - Detail::Profile::BoneOffset, sizeof(row.flags));
                    std::memcpy(&row.frameCount, tail.data() + Detail::Profile::FrameCountOffset - Detail::Profile::BoneOffset, sizeof(row.frameCount));
                    std::memcpy(&row.keyFrames, tail.data() + Detail::Profile::FramePointerOffset - Detail::Profile::BoneOffset, sizeof(row.keyFrames));
                    const Storage* frames = nullptr;
                    auto           checked = resolve(index, row, frames, resolveContext);
                    if (checked != ClockResult::Ok)
                        return checked;
                    if (row.flags & 0x08u)
                        return ClockResult::InvalidArgument;
                    if (!row.keyFrames)
                    {
                        if (frames || row.frameCount)
                            return ClockResult::BindingChanged;
                        continue;
                    }
                    if (!frames || frames->tag != row.keyFrames)
                        return ClockResult::BindingChanged;
                    if ((checked = RowResourceShape(*frames, generation, Kind::KeyFrames)) != ClockResult::Ok)
                        return checked;
                    if (frames->origin.authority != before.authority || frames->origin.authorityVtable != before.authorityVtable ||
                        !Detail::Heap::M::PrivateWritable(frames->base, frames->bytes))
                        return ClockResult::BindingChanged;
                    const std::size_t stride = (row.flags & 0x04u) ? ((row.flags & 0x02u) ? 16u : 10u) : ((row.flags & 0x02u) ? 32u : 20u);
                    if (static_cast<std::size_t>(row.frameCount) > frames->bytes / stride)
                        return ClockResult::InvalidArgument;
                }
                if (!Detail::Heap::M::Copy(array.base, &confirmed, sizeof(confirmed)))
                    return ClockResult::ReadFailure;
                if (confirmed != cookie)
                    return ClockResult::BindingChanged;
                return Detail::Context::Owner(image, owner);
            };
            // Reuse one admitted heap/profile rather than restarting the full
            // graph for every initialized capacity row. All tails/origins and
            // the cookie are still observed before/after the borrowed action.
            if ((result = snapshot()) != ClockResult::Ok || (result = snapshot()) != ClockResult::Ok)
                return result;
            NativeAnimationSequenceDependencyCalls::Epoch epoch(image, generation, array.tag, array.capacity);
            if ((result = action(array, epoch, actionContext)) != ClockResult::Ok || (result = snapshot()) != ClockResult::Ok ||
                (result = Detail::Capture(image, owner, after)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(before, after) || !Detail::RowProfileMatches(image))
                return ClockResult::BindingChanged;
            if ((result = RowResourceShape(array, generation, Kind::SequenceArray)) != ClockResult::Ok)
                return result;
            return Detail::Context::Owner(image, owner);
        }
        static ClockResult WithDependency(std::uintptr_t image, Storage& array, std::uint64_t generation, CheckOwner owner,
                                          NativeAnimationSequenceDependencyCalls::ResolveFrame resolve, void* resolveContext,
                                          NativeAnimationSequenceDependencyCalls::Action action, void* actionContext) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            namespace Dependency = NativeAnimationSequenceDependencyCalls;
            if (!owner)
                return ClockResult::NotInitialized;
            if (!action)
                return ClockResult::InvalidArgument;
            auto result = RowResourceShape(array, generation, Kind::SequenceArray);
            if (result != ClockResult::Ok)
                return result;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope held;
            Detail::Heap::Value    firstHeap, afterHeap;
            if ((result = Detail::Capture(image, owner, firstHeap)) != ClockResult::Ok)
                return result;
            if (firstHeap.authority != array.origin.authority || firstHeap.authorityVtable != array.origin.authorityVtable)
                return ClockResult::BindingChanged;
            const auto validate = [&]() noexcept
            {
                const auto count = resolve ? array.capacity : std::size_t{1};
                for (std::size_t index = 0; index != count; ++index)
                {
                    NativeAnimationSequenceRowCalls::Value row;
                    auto                                   checked = CaptureRow(image, array, generation, index, owner, row);
                    if (checked != ClockResult::Ok)
                        return checked;
                    if (!resolve)
                        continue;
                    // This first prepared-only slice owns ordinary original
                    // frame bases, never a shared-chunk/interior adoption.
                    const Storage* frame = nullptr;
                    if ((checked = resolve(index, row, frame, resolveContext)) != ClockResult::Ok)
                        return checked;
                    if (row.flags & 0x08u)
                        return ClockResult::InvalidArgument;
                    if (!row.keyFrames)
                    {
                        if (frame || row.frameCount)
                            return ClockResult::BindingChanged;
                        continue;
                    }
                    if (!frame || frame->tag != row.keyFrames || frame->origin.authority != array.origin.authority ||
                        frame->origin.authorityVtable != array.origin.authorityVtable)
                        return ClockResult::BindingChanged;
                    Detail::Heap::Value frameHeap;
                    if ((checked = CaptureFrameBuffer(image, *frame, generation, owner, frameHeap)) != ClockResult::Ok)
                        return checked;
                    if (!Detail::Heap::Same(firstHeap, frameHeap))
                        return ClockResult::BindingChanged;
                    // The parent may consume every logical frame. Qualifying
                    // the original allocation alone does not prove the row's
                    // count/format still fits that owned buffer. The saved
                    // loader and whole postprocessors establish these four
                    // strides; zero count remains a valid empty span.
                    const std::size_t stride = (row.flags & 0x04u) ? ((row.flags & 0x02u) ? 16u : 10u) : ((row.flags & 0x02u) ? 32u : 20u);
                    if (static_cast<std::size_t>(row.frameCount) > frame->bytes / stride)
                        return ClockResult::InvalidArgument;
                }
                return ClockResult::Ok;
            };
            if ((result = validate()) != ClockResult::Ok || (result = validate()) != ClockResult::Ok)
                return result;
            Dependency::Epoch epoch(image, generation, array.tag, array.capacity);
            result = action(array, epoch, actionContext);
            if (result != ClockResult::Ok)
                return result;
            if ((result = validate()) != ClockResult::Ok || (result = Detail::Capture(image, owner, afterHeap)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(firstHeap, afterHeap))
                return ClockResult::BindingChanged;
            if ((result = RowResourceShape(array, generation, Kind::SequenceArray)) != ClockResult::Ok)
                return result;
            return Detail::Context::Owner(image, owner);
        }
        static ClockResult QualifyPayloadHeld(const NativeAnimationSequencePayloadCalls::Epoch& epoch, const Storage& array, std::uint16_t count,
                                              NativeAnimationSequencePayloadCalls::Operation operation, CheckOwner owner,
                                              NativeAnimationSequencePayloadCalls::ResolveFrame resolve, void* context) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            namespace Payload = NativeAnimationSequencePayloadCalls;
            const auto logicalCount = count;
            const auto requestedOperation = operation;
            if (!owner || !resolve)
                return ClockResult::NotInitialized;
            if (requestedOperation != Payload::Operation::RemoveQuaternionFlips && requestedOperation != Payload::Operation::CalculateTotalTime)
                return ClockResult::InvalidArgument;
            if (epoch.ArrayTag() != array.tag || epoch.Capacity() != array.capacity)
                return ClockResult::BindingChanged;
            if (logicalCount > 0x7fffu || static_cast<std::size_t>(logicalCount) > array.capacity)
                return ClockResult::InvalidArgument;
            auto result = RowResourceShape(array, epoch.Generation(), Kind::SequenceArray);
            if (result != ClockResult::Ok)
                return result;
            Detail::Heap::Value firstHeap, afterHeap;
            if ((result = Detail::Capture(epoch.Image(), owner, firstHeap)) != ClockResult::Ok)
                return result;
            if (firstHeap.authority != array.origin.authority || firstHeap.authorityVtable != array.origin.authorityVtable)
                return ClockResult::BindingChanged;
            const auto validate = [&]() noexcept
            {
                for (std::size_t index = 0; index != logicalCount; ++index)
                {
                    NativeAnimationSequenceRowCalls::Value row;
                    auto                                   checked = CaptureRow(epoch.Image(), array, epoch.Generation(), index, owner, row);
                    if (checked != ClockResult::Ok)
                        return checked;
                    if (row.frameCount > 0x7fffu)
                        return ClockResult::InvalidArgument;
                    if (requestedOperation == Payload::Operation::RemoveQuaternionFlips && row.frameCount < 2)
                        continue;
                    // Time reads frameCount-1 before its loop guard. Count0
                    // cannot be laundered into an empty safe timestamp call.
                    if (!row.frameCount)
                        return ClockResult::InvalidArgument;
                    const Storage* frames = nullptr;
                    if ((checked = resolve(index, row, frames, context)) != ClockResult::Ok)
                        return checked;
                    if (!frames || !row.keyFrames || frames->tag != row.keyFrames || (row.flags & 0x0cu) ||
                        frames->origin.authority != array.origin.authority || frames->origin.authorityVtable != array.origin.authorityVtable)
                        return ClockResult::BindingChanged;
                    const std::size_t stride = (row.flags & 0x02u) ? 32u : 20u;
                    if (static_cast<std::size_t>(row.frameCount) > frames->bytes / stride)
                        return ClockResult::InvalidArgument;
                    Detail::Heap::Value frameHeap;
                    if ((checked = CaptureFrameBuffer(epoch.Image(), *frames, epoch.Generation(), owner, frameHeap)) != ClockResult::Ok)
                        return checked;
                    if (!Detail::Heap::Same(firstHeap, frameHeap))
                        return ClockResult::BindingChanged;
                }
                return ClockResult::Ok;
            };
            // Borrow the existing gate; all pointer-changing controller paths
            // remain frozen by the active parent reservation and Seq flag.
            if ((result = validate()) != ClockResult::Ok || (result = validate()) != ClockResult::Ok ||
                (result = Detail::Capture(epoch.Image(), owner, afterHeap)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::Same(firstHeap, afterHeap))
                return ClockResult::BindingChanged;
            return Detail::Context::Owner(epoch.Image(), owner);
        }
        static ClockResult ReadSequenceFrame(std::uintptr_t image, const Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames,
                                             std::size_t frameIndex, std::uint32_t stride, CheckOwner owner, std::uintptr_t& output) noexcept
        {
            namespace Detail = CustomAnimSequenceStorageDetail;
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = RowResourceShape(array, generation, Kind::SequenceArray);
            if (result != ClockResult::Ok || (result = RowResourceShape(frames, generation, Kind::KeyFrames)) != ClockResult::Ok)
                return result;
            if (array.origin.authority != frames.origin.authority || array.origin.authorityVtable != frames.origin.authorityVtable)
                return ClockResult::BindingChanged;
            if (!Detail::Context::TryEnter())
                return ClockResult::Busy;
            Detail::Context::Scope                 scope;
            NativeAnimationSequenceRowCalls::Value first, after;
            if ((result = CaptureRow(image, array, generation, rowIndex, owner, first)) != ClockResult::Ok)
                return result;
            if (!first.keyFrames)
                return ClockResult::NotInitialized;
            if (!Detail::Heap::M::PrivateWritable(frames.base, frames.bytes))
                return ClockResult::BindingChanged;
            if (first.keyFrames < frames.base || first.keyFrames - frames.base >= frames.bytes)
                return ClockResult::InvalidArgument;
            // Source arithmetic is full size_t times uint32, with no logical
            // WORD frame-count clamp or native frame-format interpretation.
            if (stride && frameIndex > (std::numeric_limits<std::size_t>::max)() / stride)
                return ClockResult::InvalidArgument;
            const auto offset = frameIndex * static_cast<std::size_t>(stride);
            if (offset > (std::numeric_limits<std::uintptr_t>::max)() - first.keyFrames)
                return ClockResult::InvalidArgument;
            const auto selected = first.keyFrames + offset;
            if (selected < frames.base || selected - frames.base >= frames.bytes)
                return ClockResult::InvalidArgument;
            if (stride && static_cast<std::size_t>(stride) > frames.bytes - (selected - frames.base))
                return ClockResult::InvalidArgument;
            if ((result = CaptureRow(image, array, generation, rowIndex, owner, after)) != ClockResult::Ok)
                return result;
            if (after.row != first.row || after.keyFrames != first.keyFrames)
                return ClockResult::OwnerConflict;
            // CaptureRow requalified the actual current heap. Equal original
            // authorities plus this frame's live/private extent prove the
            // coupled view without reading a single keyframe payload byte.
            if ((result = RowResourceShape(frames, generation, Kind::KeyFrames)) != ClockResult::Ok)
                return result;
            if (!Detail::Heap::M::PrivateWritable(frames.base, frames.bytes))
                return ClockResult::BindingChanged;
            if ((result = Detail::Context::Owner(image, owner)) != ClockResult::Ok ||
                (result = RowResourceShape(array, generation, Kind::SequenceArray)) != ClockResult::Ok ||
                (result = RowResourceShape(frames, generation, Kind::KeyFrames)) != ClockResult::Ok)
                return result;
            output = selected;
            return ClockResult::Ok;
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
    namespace NativeAnimationAssociationOuterStorageCalls
    {
        ClockResult Allocate(std::uintptr_t image, NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, CheckOwner owner, Receipt* output,
                             InvokeMalloc invoke) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::Allocate(image, storage, generation, 104, owner, output, invoke,
                                                                   NativeCustomAnimSequenceStorage::Kind::DynamicAnimation);
        }
        ClockResult FreeUnconstructed(std::uintptr_t image, NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, CheckOwner owner,
                                      Receipt* output, InvokeFree invoke) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::FreeStorage(image, storage, generation, owner, output, invoke,
                                                                      NativeCustomAnimSequenceStorage::Kind::DynamicAnimation);
        }
        ClockResult Qualify(std::uintptr_t image, const NativeCustomAnimSequenceStorage& storage, std::uint64_t generation, CheckOwner owner,
                            NativeModelReferenceHeap::Value& output) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::CaptureDynamicOuter(image, storage, generation, owner, output);
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
    namespace NativeAnimationSequenceCopyCalls
    {
        ClockResult Copy(std::uintptr_t image, Storage& target, std::size_t targetIndex, const Storage& source, std::size_t sourceIndex,
                         std::uint64_t generation, CheckOwner owner, Receipt* output, Write write) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::CopySequence(image, target, targetIndex, source, sourceIndex, generation, owner, output, write);
        }
    }
    namespace NativeAnimationSequenceNameCalls
    {
        ClockResult Set(std::uintptr_t image, Storage& storage, std::uint64_t generation, std::size_t index, const Name& name, std::uint32_t logicalHash,
                        CheckOwner owner, Receipt* output, Write write) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::SetSequenceName(image, storage, generation, index, name, logicalHash, owner, output, write);
        }
    }
    namespace NativeAnimationSequenceFrameBindCalls
    {
        ClockResult AttachCompressed(std::uintptr_t image, Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames,
                                     std::size_t count, bool root, CheckOwner owner, Receipt* output, Write write) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::AttachFreshFrames(image, array, generation, rowIndex, frames, count, root, true, owner, output,
                                                                            write);
        }
    }
    namespace NativeAnimationSequenceFrameBindCalls
    {
        ClockResult AttachUncompressed(std::uintptr_t image, Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames,
                                       std::size_t count, bool root, CheckOwner owner, Receipt* output, Write write) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::AttachFreshFrames(image, array, generation, rowIndex, frames, count, root, false, owner, output,
                                                                            write);
        }
    }
    namespace NativeAnimationSequenceOperationCalls
    {
        ClockResult AcknowledgeAcceptedRow(std::uintptr_t image, NativeCustomAnimSequenceStorage& array, std::uint64_t generation, std::size_t index,
                                           const NativeAnimationSequenceCopyCalls::Record& intent, bool frameBinding,
                                           NativeCustomAnimSequenceStorageCalls::CheckOwner owner) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::AcknowledgePrivateRow(image, array, generation, index, intent, frameBinding, owner);
        }
        ClockResult QualifyStandaloneRetirement(std::uintptr_t image, const NativeCustomAnimSequenceStorage& array, std::uint64_t generation,
                                                NativeCustomAnimSequenceStorageCalls::CheckOwner owner) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::QualifyStandaloneRetirement(image, array, generation, owner);
        }
    }
    namespace NativeAnimationSequenceCleanupCalls
    {
        ClockResult WithArray(std::uintptr_t image, NativeCustomAnimSequenceStorage& array, std::uint64_t generation,
                              NativeAnimationSequenceDependencyCalls::CheckOwner owner, NativeAnimationSequenceDependencyCalls::ResolveFrame resolve,
                              void* resolver, NativeAnimationSequenceDependencyCalls::Action action, void* context) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::WithCleanupArray(image, array, generation, owner, resolve, resolver, action, context);
        }
    }
    namespace NativeAnimationSequencePayloadCalls
    {
        ClockResult QualifyHeld(const Epoch& epoch, const Storage& array, std::uint16_t count, Operation operation, CheckOwner owner, ResolveFrame resolve,
                                void* context) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::QualifyPayloadHeld(epoch, array, count, operation, owner, resolve, context);
        }
    }
    namespace NativeAnimationSequenceFactoryCalls
    {
        ClockResult ReadCapacity(std::uintptr_t image, const Storage& storage, std::uint64_t generation, CheckOwner owner, std::size_t& output) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::ReadArrayCapacity(image, storage, generation, owner, output);
        }
    }
    namespace NativeAnimationSequenceDependencyCalls
    {
        ClockResult WithPrepared(std::uintptr_t image, Storage& array, std::uint64_t generation, CheckOwner owner, ResolveFrame resolve, void* resolveContext,
                                 Action action, void* actionContext) noexcept
        {
            if (!resolve)
                return ClockResult::InvalidArgument;
            return NativeCustomAnimSequenceStorageAccess::WithDependency(image, array, generation, owner, resolve, resolveContext, action, actionContext);
        }
        ClockResult WithArray(std::uintptr_t image, Storage& array, std::uint64_t generation, CheckOwner owner, Action action, void* actionContext) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::WithDependency(image, array, generation, owner, nullptr, nullptr, action, actionContext);
        }
    }
    namespace NativeAnimationSequenceFrameViewCalls
    {
        ClockResult Read(std::uintptr_t image, const Storage& array, std::uint64_t generation, std::size_t rowIndex, const Storage& frames,
                         std::size_t frameIndex, std::uint32_t stride, CheckOwner owner, std::uintptr_t& output) noexcept
        {
            return NativeCustomAnimSequenceStorageAccess::ReadSequenceFrame(image, array, generation, rowIndex, frames, frameIndex, stride, owner, output);
        }
    }
}
