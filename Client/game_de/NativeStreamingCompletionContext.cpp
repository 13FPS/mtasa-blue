/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the actual blocking-all owner graph for selective completion
 ****************************************************************************/
#include "NativeStreamingCompletionContext.h"
#include "NativeStreamingLoadAllProfile.h"
#include "NativeStartupOwnerAccess.h"
#include "NativeStartupWorldAccess.h"
#include "NativeUObjectMembershipProfile.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include "NativeImageAccess.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
namespace mta::game_de::NativeStreamingCompletionContext
{
    namespace P = NativeStreamingLoadAllProfile;
    namespace Image = NativeImageAccess;
    namespace
    {
        std::atomic_flag active = ATOMIC_FLAG_INIT;
    }
    bool TryEnter() noexcept
    {
        return !active.test_and_set(std::memory_order_acquire);
    }
    void Leave() noexcept
    {
        active.clear(std::memory_order_release);
    }
    bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
    {
        if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
            return false;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto end = address + bytes;
        auto       at = address;
        while (at < end)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (VirtualQuery(reinterpret_cast<void*>(at), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT || !page.RegionSize ||
                (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                return false;
            const auto access = page.Protect & 0xff;
            if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY)
                return false;
            if (page.Type == MEM_IMAGE)
            {
                if (reinterpret_cast<std::uintptr_t>(page.AllocationBase) != image)
                    return false;
            }
            else if (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED)
                return false;
            const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
            if (begin > at || page.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - begin || begin + page.RegionSize <= at)
                return false;
            at = (std::min)(end, begin + page.RegionSize);
        }
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), output, bytes, &copied) && copied == bytes;
    }
    bool Writable(std::uintptr_t address, std::size_t bytes) noexcept
    {
        MEMORY_BASIC_INFORMATION page{};
        if (!address || address % alignof(std::uintptr_t) || !bytes || VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) ||
            page.State != MEM_COMMIT || (page.Type != MEM_PRIVATE && page.Type != MEM_MAPPED) ||
            (page.Protect != PAGE_READWRITE && page.Protect != PAGE_WRITECOPY))
            return false;
        const auto begin = reinterpret_cast<std::uintptr_t>(page.BaseAddress);
        return begin <= address && page.RegionSize <= (std::numeric_limits<std::uintptr_t>::max)() - begin && address <= begin + page.RegionSize &&
               bytes <= begin + page.RegionSize - address;
    }
    template <class Pins>
    bool PinsMatch(std::uintptr_t image, const Pins& pins) noexcept
    {
        for (const auto& pin : pins)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        if (!PinsMatch(image, P::Code) || !PinsMatch(image, NativeUObjectMembershipProfile::pins))
            return false;
        const auto pins = [&](const auto& values)
        {
            for (const auto& pin : values)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        };
        return pins(NativeStartupOwnerProfile::pins) && pins(NativeStartupWorldProfile::pins);
    }
    ClockResult Owner(std::uintptr_t image, CheckOwner check) noexcept
    {
        auto result = check();
        if (result != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            return result;
        std::uint32_t state = 0;
        if (!NativeGameStateMemory::Read(image, state))
            return ClockResult::BindingChanged;
        return state == 9 ? ClockResult::Ok : ClockResult::NotInitialized;
    }
    bool Fence(std::uintptr_t image, const Value& graph) noexcept
    {
        NativeUObjectMembershipAccess membership(image, &Copy);
        for (std::size_t i = 0; i != graph.count; ++i)
            if (!membership.Recheck(graph.members[i]))
                return false;
        return true;
    }
    bool Add(std::uintptr_t image, std::uintptr_t object, Value& graph) noexcept
    {
        if (!object)
            return false;
        for (std::size_t i = 0; i != graph.count; ++i)
            if (graph.members[i].objectAddress == object)
                return true;
        if (graph.count == graph.members.size() || !NativeUObjectMembershipAccess(image, &Copy).Capture(object, graph.members[graph.count]))
            return false;
        ++graph.count;
        return true;
    }
    ClockResult Capture(std::uintptr_t image, Value& output) noexcept
    {
        Value next;
        if (!Image::ReadImage(image, image + P::RootRva, &next.root, sizeof(next.root), true))
            return ClockResult::ReadFailure;
        if (!next.root)
            return ClockResult::NotInitialized;
        std::uintptr_t rootWorld = 0;
        if (!Writable(next.root, 0x180) || !Add(image, next.root, next) || next.members[0].vtableAddress != image + P::RootVtableRva ||
            !Image::ReadImage(image, image + P::RootWorldSlotRva, &rootWorld, sizeof(rootWorld)) || rootWorld != image + P::RootWorldRva)
            return ClockResult::BindingChanged;
        NativeStartupOwnerAccess::Value owner;
        NativeStartupWorldAccess::Value world;
        if (!NativeStartupOwnerAccess(image, &Copy).Read(next.root, owner) || !NativeStartupWorldAccess(image, &Copy).Read(owner.gameModeAddress, world))
            return ClockResult::BindingChanged;
        next.gameMode = owner.gameModeAddress;
        next.level = world.levelAddress;
        next.world = world.worldAddress;
        if (!At(image, NativeStartupOwnerProfile::gameModeClassCacheRva, next.gameClass) ||
            !At(image, NativeStartupWorldProfile::levelClassCacheRva, next.levelClass) || !Writable(next.gameMode, 0x2c8) ||
            !Writable(next.world, P::WorldModeOffset + 1) || !At(next.world, P::WorldModeOffset, next.mode) || next.mode > 2 ||
            !Add(image, next.gameMode, next) || !Add(image, world.outerAddress, next) || !Add(image, next.level, next) || !Add(image, next.world, next) ||
            !Add(image, next.gameClass, next) || !Add(image, next.levelClass, next))
            return ClockResult::BindingChanged;
        // Read-only UClass dependencies are current raw members too. Close
        // this bounded graph over each observed class; it grants no GC pin.
        for (std::size_t i = 0; i != next.count; ++i)
            if (!Add(image, next.members[i].classAddress, next))
                return ClockResult::BindingChanged;
        if (!Fence(image, next))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }
    bool Same(const Value& a, const Value& b, bool sameMode) noexcept
    {
        if (a.root != b.root || a.gameMode != b.gameMode || a.level != b.level || a.world != b.world || a.gameClass != b.gameClass ||
            a.levelClass != b.levelClass || a.count != b.count || (sameMode && a.mode != b.mode))
            return false;
        for (std::size_t i = 0; i != a.count; ++i)
        {
            const auto& x = a.members[i];
            const auto& y = b.members[i];
            if (x.objectAddress != y.objectAddress || x.vtableAddress != y.vtableAddress || x.classAddress != y.classAddress ||
                x.outerAddress != y.outerAddress || x.itemAddress != y.itemAddress || x.index != y.index || x.serial != y.serial)
                return false;
        }
        return true;
    }
}
