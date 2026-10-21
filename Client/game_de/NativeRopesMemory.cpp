/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve DE rope actions and their native reference lifetime protocol
 *****************************************************************************/
#include "NativeRopesMemory.h"
#include "NativeRopesProfile.h"
#include "NativeImageAccess.h"
// CVector4D consumes MTA's shared epsilon before CVector is declared; keep
// this translation unit independent of the classic game's precompiled header.
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <intrin.h>

namespace mta::game_de
{
    namespace RopesDetail
    {
        namespace P = NativeRopesProfile;
        namespace Image = NativeImageAccess;
        struct Row
        {
            std::uintptr_t identity = 0, holder = 0, hook = 0, picked = 0;
            unsigned char  type = 0;
            bool           Same(const Row& other) const noexcept
            {
                return identity == other.identity && holder == other.holder && hook == other.hook && picked == other.picked && type == other.type;
            }
        };
        struct Snapshot
        {
            std::array<Row, P::RowCount> rows{};
            std::uint32_t                counter = 0;
            bool                         Same(const Snapshot& other) const noexcept
            {
                if (counter != other.counter)
                    return false;
                for (std::size_t i = 0; i < rows.size(); ++i)
                    if (!rows[i].Same(other.rows[i]))
                        return false;
                return true;
            }
        };
        bool Pages(std::uintptr_t address, std::size_t bytes, bool writable) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + bytes;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = info.Protect & 0xff;
                const bool canWrite = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
                if ((!canWrite && protection != PAGE_READONLY) || (writable && !canWrite))
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool Object(std::uintptr_t address, T& output) noexcept
        {
            SIZE_T copied = 0;
            return Pages(address, sizeof(output), false) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &output, sizeof(output), &copied) && copied == sizeof(output);
        }
        template <class T>
        bool Global(std::uintptr_t image, std::uintptr_t rva, T& output) noexcept
        {
            return Image::ReadImage(image, image + rva, &output, sizeof(output), true);
        }
        bool Binding(std::uintptr_t image) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageSize)
                return false;
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::uintptr_t>(dos.e_lfanew) > P::ImageSize - sizeof(nt) || !Image::ReadImage(image, image + dos.e_lfanew, &nt, sizeof(nt)) ||
                nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || !nt.FileHeader.NumberOfSections ||
                nt.FileHeader.NumberOfSections > 96 || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != P::ImageSize)
                return false;
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, 2304> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            std::uintptr_t destroy = 0, remove = 0;
            return Image::ReadImage(image, image + P::HookVtableRva, &destroy, sizeof(destroy)) && destroy == image + P::HookDestructorRva &&
                   Image::ReadImage(image, image + P::HookVtableRva + 0x20, &remove, sizeof(remove)) && remove == image + P::HookRemoveRva &&
                   Image::ImagePages(image, image + P::TableRva, P::RowCount * P::RowBytes, true) && Image::ImagePages(image, image + P::CounterRva, 4, true) &&
                   Image::ImagePages(image, image + P::ReferencesRva, P::ReferenceCount * P::ReferenceBytes, true);
        }
        bool Reference(std::uintptr_t image, std::uintptr_t entity, std::uintptr_t slot, bool hook) noexcept
        {
            if (!entity)
                return true;
            std::uintptr_t vtable = 0, node = 0;
            unsigned char  type = 0;
            if (entity % alignof(std::uintptr_t) || !Pages(entity, hook ? 0x248 : 0x80, true) || !Object(entity, vtable) || !Object(entity + 0x6a, type) ||
                !Object(entity + 0x40, node) || !node)
                return false;
            type &= 7;
            if (type < 2 || type > 4 || (hook && (type != 4 || vtable != image + P::HookVtableRva)) || !Image::ImagePages(image, vtable, 0x28))
                return false;
            // Native registration's three pool walks independently identify
            // physical ped/vehicle/object strides and occupied-byte semantics.
            // The hook deleting destructor also derives its free slot from the
            // object pool. A registered reference alone must not authorize an
            // arbitrary readable object outside its still-occupied allocation.
            const std::uintptr_t rootRva = type == 2 ? 0x572b598 : type == 3 ? 0x572b5a0 : 0x572b588;
            const std::size_t    stride = type == 2 ? 0xdb8 : type == 3 ? 0xb10 : 0x248;
            std::uintptr_t       pool = 0, objects = 0, flags = 0;
            std::int32_t         capacity = 0;
            unsigned char        generation = 0;
            if (!Global(image, rootRva, pool) || !pool || pool % alignof(std::uintptr_t) || !Object(pool, objects) || !Object(pool + 8, flags) ||
                !Object(pool + 0x10, capacity) || capacity <= 0 || !objects || !flags || entity < objects || (entity - objects) % stride)
                return false;
            const auto index = (entity - objects) / stride;
            if (index >= static_cast<std::uint32_t>(capacity) || index > (std::numeric_limits<std::uintptr_t>::max)() - flags ||
                !Object(flags + index, generation) || (generation & 0x80))
                return false;
            bool found = false;
            // The actual destructor traverses this complete list before it
            // frees the entity. Check genuine image-owned nodes and all slots,
            // rather than treating readable entity pages as a lifetime lease.
            for (std::size_t count = 0; node && count < P::ReferenceCount; ++count)
            {
                const auto begin = image + P::ReferencesRva;
                if (node < begin || node - begin >= P::ReferenceCount * P::ReferenceBytes || (node - begin) % P::ReferenceBytes)
                    return false;
                std::array<std::uintptr_t, 2> entry{};
                if (!Image::ReadImage(image, node, entry.data(), sizeof(entry), true) || !entry[1] || entry[1] % alignof(std::uintptr_t) ||
                    !Pages(entry[1], sizeof(std::uintptr_t), true))
                    return false;
                std::uintptr_t target = 0;
                if (!Object(entry[1], target))
                    return false;
                if (entry[1] == slot && target == entity)
                    found = true;
                node = entry[0];
            }
            std::uintptr_t currentPool = 0, currentObjects = 0, currentFlags = 0;
            std::int32_t   currentCapacity = 0;
            unsigned char  currentGeneration = 0;
            return !node && found && Global(image, rootRva, currentPool) && currentPool == pool && Object(pool, currentObjects) && currentObjects == objects &&
                   Object(pool + 8, currentFlags) && currentFlags == flags && Object(pool + 0x10, currentCapacity) && currentCapacity == capacity &&
                   Object(flags + index, currentGeneration) && currentGeneration == generation;
        }
        ClockResult Capture(std::uintptr_t image, Snapshot& output) noexcept
        {
            Snapshot next;
            if (!Global(image, P::CounterRva, next.counter))
                return ClockResult::ReadFailure;
            if (next.counter > 255)
                return ClockResult::BindingChanged;
            for (std::size_t i = 0; i < next.rows.size(); ++i)
            {
                auto&      row = next.rows[i];
                const auto address = image + P::TableRva + i * P::RowBytes;
                if (!Global(image, P::TableRva + i * P::RowBytes + P::IdentityOffset, row.identity) || !Object(address + P::HolderOffset, row.holder) ||
                    !Object(address + P::HookOffset, row.hook) || !Object(address + P::PickedUpOffset, row.picked) ||
                    !Object(address + P::TypeOffset, row.type))
                    return ClockResult::ReadFailure;
                if (row.type > 8 || (row.picked && !row.hook) || !Reference(image, row.hook, address + P::HookOffset, true) ||
                    !Reference(image, row.picked, address + P::PickedUpOffset, false))
                    return ClockResult::BindingChanged;
            }
            output = next;
            return ClockResult::Ok;
        }
        ClockResult Owner(NativeRopesMemory::CheckOwner owner) noexcept
        {
            return owner ? owner() : ClockResult::NotInitialized;
        }
        bool Commit(std::uintptr_t address, std::uint32_t next, std::uint32_t expected, std::uint32_t& observed) noexcept
        {
            __try
            {
                observed = static_cast<std::uint32_t>(
                    InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(next), static_cast<LONG>(expected)));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
#if !defined(MTA_DE_ROPES_CREATE_INVOKE)
        ClockResult NativeCreate(NativeRopesMemory::CreateFunction function, std::uintptr_t identity, const NativeRopesMemory::Vector12& position,
                                 std::uint32_t duration, std::int32_t& result, std::uint32_t& exception) noexcept
        {
            __try
            {
                result = function(identity, 8, &position, 1, 0, 0, 0, duration);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
#if !defined(MTA_DE_ROPES_REMOVE_INVOKE)
        ClockResult NativeRemove(NativeRopesMemory::RemoveFunction function, void* row, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(row);
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
    ClockResult NativeRopesMemory::Bind() noexcept
    {
        if (ownerThread.load() && ownerThread.load() != GetCurrentThreadId())
            return ClockResult::WrongThread;
        Reset();
        if (!NativeImageAccess::CheckExecutableBuild())
            return ClockResult::UnsupportedBuild;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!RopesDetail::Binding(image))
            return ClockResult::BindingChanged;
        imageAddress.store(image);
        ownerThread.store(GetCurrentThreadId());
        return ClockResult::Ok;
    }
    void NativeRopesMemory::Reset() noexcept
    {
        imageAddress.store(0);
        ownerThread.store(0);
    }
    bool NativeRopesMemory::IsBound() const noexcept
    {
        return imageAddress.load() != 0;
    }
    ClockResult NativeRopesMemory::Current(std::uintptr_t& image) const noexcept
    {
        image = imageAddress.load();
        if (!image)
            return ClockResult::NotInitialized;
        if (ownerThread.load() != GetCurrentThreadId())
            return ClockResult::WrongThread;
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) || !RopesDetail::Binding(image))
            return ClockResult::BindingChanged;
        return ClockResult::Ok;
    }
    ClockResult NativeRopesMemory::Qualify(CheckOwner owner) const noexcept
    {
        auto result = RopesDetail::Owner(owner);
        if (result != ClockResult::Ok)
            return result;
        std::uintptr_t image = 0;
        result = Current(image);
        if (result != ClockResult::Ok)
            return result;
        RopesDetail::Snapshot first, second;
        result = RopesDetail::Capture(image, first);
        if (result != ClockResult::Ok)
            return result;
        result = RopesDetail::Owner(owner);
        if (result != ClockResult::Ok)
            return result;
        result = RopesDetail::Capture(image, second);
        if (result != ClockResult::Ok)
            return result;
        if (!first.Same(second))
            return ClockResult::BindingChanged;
        result = Current(image);
        return result == ClockResult::Ok ? RopesDetail::Owner(owner) : result;
    }
    ClockResult NativeRopesMemory::Create(const CVector& position, std::uint32_t duration, std::int32_t& output, CheckOwner owner, Receipt* receiptOutput,
                                          CreateInvoke invoke) const noexcept
    {
        static_assert(sizeof(Vector12) == 12);
        const Vector12 vector{position.fX, position.fY, position.fZ};
        Receipt        receipt;
        const auto     finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (!std::isfinite(vector.x) || !std::isfinite(vector.y) || !std::isfinite(vector.z))
            return finish(ClockResult::InvalidArgument);
        auto result = Qualify(owner);
        if (result != ClockResult::Ok)
            return finish(result);
        std::uintptr_t image = 0;
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        RopesDetail::Snapshot before, confirmed;
        result = RopesDetail::Capture(image, before);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.identity = before.counter + 100;
        for (const auto& row : before.rows)
            if (row.type && row.identity == receipt.identity && row.type != 8)
                return finish(ClockResult::BindingChanged);
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        result = RopesDetail::Capture(image, confirmed);
        if (result != ClockResult::Ok || !before.Same(confirmed))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        result = RopesDetail::Owner(owner);
        if (result != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_ROPES_CREATE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_ROPES_CREATE_INVOKE;
#else
        const auto entry = invoke ? invoke : &RopesDetail::NativeCreate;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<CreateFunction>(image + NativeRopesProfile::CreateRva), receipt.identity, vector, duration, receipt.nativeResult,
                       receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        result = RopesDetail::Owner(owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (receipt.nativeResult < 0)
        {
            output = -1;
            return finish(ClockResult::Ok);
        }
        std::uint32_t observed = 0;
        if (!RopesDetail::Global(image, NativeRopesProfile::CounterRva, observed))
            return finish(ClockResult::ReadFailure);
        if (observed != before.counter)
            return finish(ClockResult::OwnerConflict);
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        result = RopesDetail::Owner(owner);
        if (result != ClockResult::Ok)
            return finish(result);
        const auto next = (before.counter + 1) & 255u;
        if (!RopesDetail::Commit(image + NativeRopesProfile::CounterRva, next, before.counter, observed))
            return finish(ClockResult::WriteFailure);
        if (observed != before.counter)
            return finish(ClockResult::OwnerConflict);
        receipt.counterCommitted = true;
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!RopesDetail::Global(image, NativeRopesProfile::CounterRva, observed))
            return finish(ClockResult::ReadFailure);
        if (observed != next)
            return finish(ClockResult::OwnerConflict);
        result = RopesDetail::Owner(owner);
        if (result != ClockResult::Ok)
            return finish(result);
        // This preserves the observed DE script branch exactly: native0/full
        // is nonnegative and still advances the ID counter. Returning an ID is
        // not a claim that Register created a row in a full native table.
        output = static_cast<std::int32_t>(receipt.identity);
        return finish(ClockResult::Ok);
    }
    ClockResult NativeRopesMemory::Remove(std::uintptr_t key, CheckOwner owner, Receipt* receiptOutput, RemoveInvoke invoke) const noexcept
    {
        Receipt receipt;
        receipt.identity = key;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        auto result = Qualify(owner);
        if (result != ClockResult::Ok)
            return finish(result);
        std::uintptr_t image = 0;
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        RopesDetail::Snapshot before, confirmed;
        result = RopesDetail::Capture(image, before);
        if (result != ClockResult::Ok)
            return finish(result);
        std::size_t selected = before.rows.size();
        // Classic RemoveEntityRope compares all8 identities, including an
        // inactive first match. It never reads memory at the supplied key.
        for (std::size_t i = 0; i < before.rows.size(); ++i)
            if (before.rows[i].identity == key)
            {
                selected = i;
                break;
            }
        if (selected == before.rows.size())
            return finish(RopesDetail::Owner(owner));
        receipt.row = image + NativeRopesProfile::TableRva + selected * NativeRopesProfile::RowBytes;
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        result = RopesDetail::Capture(image, confirmed);
        if (result != ClockResult::Ok || !before.Same(confirmed))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        result = RopesDetail::Owner(owner);
        if (result != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_ROPES_REMOVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_ROPES_REMOVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &RopesDetail::NativeRemove;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<RemoveFunction>(image + NativeRopesProfile::RemoveRva), reinterpret_cast<void*>(receipt.row), receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        result = Current(image);
        if (result != ClockResult::Ok)
            return finish(result);
        unsigned char  type = 255;
        std::uintptr_t hook = 1, picked = 1;
        if (!RopesDetail::Object(receipt.row + NativeRopesProfile::TypeOffset, type) ||
            !RopesDetail::Object(receipt.row + NativeRopesProfile::HookOffset, hook) ||
            !RopesDetail::Object(receipt.row + NativeRopesProfile::PickedUpOffset, picked))
            return finish(ClockResult::ReadFailure);
        if (type || hook || picked)
            return finish(ClockResult::OwnerConflict);
        return finish(RopesDetail::Owner(owner));
    }
}
