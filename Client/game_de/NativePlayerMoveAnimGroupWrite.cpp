/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guard the one proven movement DWORD without replay or rollback
 ****************************************************************************/
#include <windows.h>
#include "NativePlayerMoveAnimGroupWrite.h"
#include "NativePedMoveAnimProfile.h"
#include "NativePedMetadataMemory.h"
#include "NativePlayerInfoProfile.h"
#include "NativeModelReferenceHeap.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>

namespace mta::game_de::NativePlayerMoveAnimGroupWrite
{
    namespace Detail
    {
        namespace I = NativeImageAccess;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativePedMoveAnimProfile;
        struct Guard
        {
            std::atomic_flag& flag;
            bool              held;
            explicit Guard(std::atomic_flag& value) noexcept : flag(value), held(!flag.test_and_set()) {}
            ~Guard()
            {
                if (held)
                    flag.clear();
            }
        };
        struct Snapshot
        {
            std::int32_t   selector = -1, group = 0;
            std::uintptr_t playerData = 0;
            D::Value       world;
        };
        bool Pages(std::uintptr_t at) noexcept
        {
            if (!at || at % alignof(std::int32_t) || at > (std::numeric_limits<std::uintptr_t>::max)() - sizeof(std::int32_t))
                return false;
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
                (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (page.Protect & 0xff) != PAGE_READWRITE)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            return at >= begin && page.RegionSize >= sizeof(std::int32_t) && at - begin <= page.RegionSize - sizeof(std::int32_t);
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            // The existing named0245 and Process producer already qualify the
            // exact +6e0 DWORD. This adds no guessed structure or native call.
            if (!NativePedMoveAnimReader::Revalidate({image}))
                return false;
            std::array<unsigned char, 160> bytes{};
            for (std::size_t i = 0; i != 3; ++i)
            {
                const auto& pin = NativePlayerInfoProfile::Code[i];
                if (!pin.size || pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            std::uintptr_t control = 0;
            return NativeModelReferenceHeap::ReadOnlyImage(image, image + NativePlayerInfoProfile::PlayerVtableRva, 0x58) &&
                   I::ReadImage(image, image + NativePlayerInfoProfile::PlayerVtableRva + 0x50, &control, sizeof(control)) &&
                   control == image + NativePlayerInfoProfile::PlayerControlRva;
        }
        ClockResult Capture(std::uintptr_t image, const Ped& ped, CheckOwner owner, CheckOwner afterApplication, Snapshot& out) noexcept
        {
            auto result = owner();
            if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            NativePedMetadata metadata;
            if ((result = NativePedMetadataMemory({image}, owner).Read(ped, metadata)) != ClockResult::Ok)
                return result;
            if (metadata.entityType != 3 || ped.vtable != image + NativePlayerInfoProfile::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            Snapshot value;
            if (!D::At(ped.nativePed, 0x7e0, value.selector) || !D::At(ped.nativePed, 0x5f0, value.playerData))
                return ClockResult::ReadFailure;
            if (value.selector != 0 && value.selector != 1)
                return ClockResult::UnsupportedBuild;
            const auto     row = image + NativePlayerInfoProfile::BankRva + static_cast<std::uintptr_t>(value.selector) * NativePlayerInfoProfile::RowBytes;
            std::uintptr_t published = 0;
            if (!I::ReadImage(image, row, &published, sizeof(published), true) || published != ped.nativePed || value.playerData != row + 8)
                return ClockResult::BindingChanged;
            if ((result = NativePedMoveAnimReader({image}, owner).Read(ped, value.group)) != ClockResult::Ok ||
                (result = D::Capture(image, value.world)) != ClockResult::Ok ||
                (result = NativePedPoolReader({image}, owner).Recheck(ped)) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
                return result;
            out = value;
            return ClockResult::Ok;
        }
        bool Anchors(std::uintptr_t image, const Ped& ped, const Snapshot& value, std::int32_t group) noexcept
        {
            std::uintptr_t pool = 0, objects = 0, flags = 0, vtable = 0, method = 0, data = 0, published = 0;
            std::int32_t   capacity = 0, cursor = 0, selector = -1, current = 0;
            unsigned char  owns = 0, flag = 0;
            if (!I::ReadImage(image, image + NativePedPoolProfile::RootRva, &pool, sizeof(pool), true) || pool != ped.pool || !D::At(pool, 0, objects) ||
                objects != ped.objects || !D::At(pool, 8, flags) || flags != ped.flags || !D::At(pool, 0x10, capacity) || capacity != ped.capacity ||
                !D::At(pool, 0x14, cursor) || cursor != ped.cursor || !D::At(pool, 0x18, owns) || owns != ped.ownsAllocations ||
                !D::Copy(ped.flags + ped.slot, &flag, sizeof(flag)) || (flag & NativePedPoolProfile::EmptyBit) ||
                (flag & NativePedPoolProfile::GenerationMask) != ped.generation || !D::At(ped.nativePed, 0, vtable) || vtable != ped.vtable ||
                !I::ReadImage(image, vtable, &method, sizeof(method)) || method != ped.firstMethod || !D::At(ped.nativePed, 0x7e0, selector) ||
                selector != value.selector || !D::At(ped.nativePed, 0x5f0, data) || data != value.playerData ||
                !D::At(ped.nativePed, P::GroupOffset, current) || current != group)
                return false;
            const auto row = image + NativePlayerInfoProfile::BankRva + static_cast<std::uintptr_t>(selector) * NativePlayerInfoProfile::RowBytes;
            return I::ReadImage(image, row, &published, sizeof(published), true) && published == ped.nativePed && D::Fence(image, value.world);
        }
        bool ExchangeField(std::uintptr_t address, std::int32_t expected, std::int32_t target, std::int32_t& observed, std::uint32_t& exception) noexcept
        {
            static_assert(sizeof(LONG) == sizeof(std::int32_t));
            __try
            {
#if defined(MTA_DE_PLAYER_MOVEANIM_GROUP_EXCHANGE)
                const LONG value =
                    MTA_DE_PLAYER_MOVEANIM_GROUP_EXCHANGE(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(target), static_cast<LONG>(expected));
#else
                const LONG value =
                    InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(target), static_cast<LONG>(expected));
#endif
                std::memcpy(&observed, &value, sizeof(observed));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
        }
    }
    ClockResult Controller::Write(std::uintptr_t image, const Ped& supplied, std::int32_t desired, CheckOwner owner, CheckOwner afterApplication,
                                  CheckGroup qualifyGroup, Receipt* output, Exchange exchange) noexcept
    {
        Receipt receipt;
        receipt.image = image;
        receipt.nativePed = supplied.nativePed;
        receipt.handle = supplied.handle;
        receipt.desired = desired;
        const auto finish = [&](ClockResult result)
        {
            receipt.result = result;
            if (receipt.unknown && !retained.load())
            {
                firstUnknown = receipt;
                retained.store(true);
            }
            if (output)
                *output = receipt;
            return result;
        };
        Detail::Guard guard(operation);
        if (!guard.held)
            return finish(ClockResult::Busy);
        if (retained.load())
            return finish(ClockResult::RestoreFailure);
        if (!image || !owner || !afterApplication || !qualifyGroup || desired <= 0)
            return finish(ClockResult::InvalidArgument);
        const auto       ped = supplied;
        Detail::Snapshot before;
        auto             result = Detail::Capture(image, ped, owner, afterApplication, before);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.selector = before.selector;
        receipt.playerData = before.playerData;
        receipt.world = before.world.world;
        receipt.before = before.group;
        if (ped.nativePed > (std::numeric_limits<std::uintptr_t>::max)() - NativePedMoveAnimProfile::GroupOffset)
            return finish(ClockResult::ReadFailure);
        receipt.address = ped.nativePed + NativePedMoveAnimProfile::GroupOffset;
        // Group qualification is the last callback. Raw current player/pool/
        // world/field anchors follow it, so a provider cannot replace the row
        // while returning a successful final owner status before the CAS.
        if ((result = owner()) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok || (result = qualifyGroup(desired)) != ClockResult::Ok)
            return finish(result);
        if (!Detail::Profile(image) || !Detail::Pages(receipt.address) || !Detail::Anchors(image, ped, before, before.group))
            return finish(ClockResult::BindingChanged);
        receipt.phase = Phase::Admitted;
        if (!exchange)
            exchange = &Detail::ExchangeField;
        receipt.phase = Phase::Entered;
        receipt.entered = true;
        const bool returned = exchange(receipt.address, before.group, desired, receipt.observed, receipt.exceptionCode);
        receipt.returned = returned;
        if (!returned)
        {
            receipt.phase = Phase::Unknown;
            receipt.unknown = true;
            return finish(ClockResult::OriginalException);
        }
        receipt.phase = Phase::Returned;
        receipt.accepted = receipt.observed == before.group;
        if (!receipt.accepted)
            return finish(ClockResult::OwnerConflict);
        Detail::Snapshot after;
        result = Detail::Capture(image, ped, owner, afterApplication, after);
        receipt.after = after.group;
        if (result == ClockResult::Ok && (after.selector != before.selector || after.playerData != before.playerData ||
                                          !NativeStreamingCompletionContext::Same(before.world, after.world, true) || after.group != desired))
            result = ClockResult::BindingChanged;
        if (result == ClockResult::Ok)
            result = qualifyGroup(desired);
        if (result == ClockResult::Ok && (!Detail::Profile(image) || !Detail::Anchors(image, ped, after, desired)))
            result = ClockResult::BindingChanged;
        if (result != ClockResult::Ok)
        {
            receipt.phase = Phase::Unknown;
            receipt.unknown = true;
            return finish(result);
        }
        receipt.phase = Phase::Acknowledged;
        receipt.acknowledged = true;
        return finish(ClockResult::Ok);
    }
    void Controller::RetainAfterEntry(const Receipt& input, ClockResult result) noexcept
    {
        // Like the existing whole-Reapply controller, this outer completion is
        // serialized by the actual Runtime's admitted engine callback. It runs
        // after Write releases its operation; never drop an entered prefix
        // merely because a diagnostic read temporarily owns that CPU flag.
        if (retained.load() || !input.entered || (!input.accepted && input.returned))
            return;
        firstUnknown = input;
        firstUnknown.phase = Phase::Unknown;
        firstUnknown.result = result;
        firstUnknown.acknowledged = false;
        firstUnknown.unknown = true;
        retained.store(true);
    }
    bool Controller::Read(Receipt& output) const noexcept
    {
        Detail::Guard guard(operation);
        if (!guard.held || !retained.load())
            return false;
        output = firstUnknown;
        return true;
    }
}
