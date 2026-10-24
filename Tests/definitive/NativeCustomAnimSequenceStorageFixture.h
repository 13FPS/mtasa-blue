/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reusable owned allocator seam over existing real heap/world guards
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeCustomAnimSequenceStorage.h"
#include "../../Client/game_de/NativeCustomAnimSequenceStorageProfile.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <utility>
namespace NativeCustomAnimSequenceStorageFixture
{
    namespace Calls = mta::game_de::NativeCustomAnimSequenceStorageCalls;
    namespace Profile = mta::game_de::NativeCustomAnimSequenceStorageProfile;
    namespace HeapProfile = mta::game_de::NativeModelReferenceTransactionProfile;
    using ClockResult = mta::game_de::ClockResult;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        for (const auto& pin : Profile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& table : {std::pair<std::uintptr_t, std::uintptr_t>{HeapProfile::HeapVtableRva, Profile::AuthorityMallocRva},
                                  {HeapProfile::ProxyVtableRva, Profile::ProxyMallocRva}})
        {
            const auto address = image + table.second;
            writer(host, table.first + 0x10, &address, sizeof(address), PAGE_READONLY);
        }
        for (const auto& table : {std::pair<std::uintptr_t, std::uintptr_t>{HeapProfile::HeapVtableRva, Profile::AuthorityFreeRva},
                                  {HeapProfile::ProxyVtableRva, Profile::ProxyFreeRva}})
        {
            const auto address = image + table.second;
            writer(host, table.first + 0x30, &address, sizeof(address), PAGE_READONLY);
        }
    }
    struct State
    {
        // Host cleanup owns only test allocations. It is not a production
        // record destructor and must not be mistaken for native debt recovery.
        std::array<std::uintptr_t, 32> allocations{};
        std::uintptr_t                 image = 0, heap = 0, base = 0;
        std::size_t                    bytes = 0;
        std::uint32_t                  alignment = ~std::uint32_t{0};
        unsigned                       mallocCalls = 0, freeCalls = 0, allocationCount = 0;
        Calls::MallocFunction          mallocFunction = nullptr;
        Calls::FreeFunction            freeFunction = nullptr;
        ClockResult                    mallocResult = ClockResult::Ok, freeResult = ClockResult::Ok;
        std::uint32_t                  exceptionCode = 0xc0000005;
        bool                           nullMalloc = false, consumeFailedFree = false;
        void (*afterMalloc)(State&) noexcept = nullptr;
        void (*afterFree)(State&) noexcept = nullptr;
        void* context = nullptr;
        State() = default;
        State(const State&) = delete;
        State& operator=(const State&) = delete;
        ~State() { Clear(); }
        void Clear() noexcept
        {
            for (auto& allocation : allocations)
            {
                if (allocation)
                    VirtualFree(reinterpret_cast<void*>(allocation), 0, MEM_RELEASE);
                allocation = 0;
            }
            allocationCount = 0;
        }
        void Reset(std::uintptr_t value, Writer writer, void* host)
        {
            Clear();
            image = value;
            heap = base = 0;
            bytes = 0;
            alignment = ~std::uint32_t{0};
            mallocCalls = freeCalls = 0;
            mallocFunction = nullptr;
            freeFunction = nullptr;
            mallocResult = freeResult = ClockResult::Ok;
            exceptionCode = 0xc0000005;
            nullMalloc = consumeFailedFree = false;
            afterMalloc = afterFree = nullptr;
            context = nullptr;
            Install(image, writer, host);
        }
        bool Contains(std::uintptr_t address) const noexcept
        {
            for (const auto allocation : allocations)
                if (allocation == address && address)
                    return true;
            return false;
        }
        bool Release(std::uintptr_t address) noexcept
        {
            for (auto& allocation : allocations)
                if (allocation == address && address)
                {
                    if (!VirtualFree(reinterpret_cast<void*>(address), 0, MEM_RELEASE))
                        return false;
                    allocation = 0;
                    return true;
                }
            return false;
        }
        ClockResult Malloc(Calls::MallocFunction function, std::uintptr_t receiver, std::size_t size, std::uint32_t align, std::uintptr_t& result,
                           std::uint32_t& exception) noexcept
        {
            ++mallocCalls;
            mallocFunction = function;
            heap = receiver;
            bytes = size;
            alignment = align;
            result = base = 0;
            if (!nullMalloc)
            {
                if (allocationCount == allocations.size())
                    return ClockResult::WriteFailure;
                const auto allocation = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
                if (!allocation)
                    return ClockResult::WriteFailure;
                allocations[allocationCount++] = allocation;
                result = base = allocation;
                // Native construction deliberately leaves name64/padding
                // untouched. A nonzero pattern makes fake memset visible.
                std::memset(reinterpret_cast<void*>(allocation), 0xa5, size);
            }
            if (afterMalloc)
                afterMalloc(*this);
            if (mallocResult != ClockResult::Ok)
                exception = exceptionCode;
            return mallocResult;
        }
        ClockResult Free(Calls::FreeFunction function, std::uintptr_t receiver, std::uintptr_t originalBase, std::uint32_t& exception) noexcept
        {
            ++freeCalls;
            freeFunction = function;
            heap = receiver;
            base = originalBase;
            if (!Contains(originalBase))
                return ClockResult::WriteFailure;
            if ((freeResult == ClockResult::Ok || consumeFailedFree) && !Release(originalBase))
                return ClockResult::WriteFailure;
            if (afterFree)
                afterFree(*this);
            if (freeResult != ClockResult::Ok)
                exception = exceptionCode;
            return freeResult;
        }
    };
    inline State*      active = nullptr;
    inline ClockResult Malloc(Calls::MallocFunction function, std::uintptr_t heap, std::size_t bytes, std::uint32_t alignment, std::uintptr_t& output,
                              std::uint32_t& exception) noexcept
    {
        return active ? active->Malloc(function, heap, bytes, alignment, output, exception) : ClockResult::NotInitialized;
    }
    inline ClockResult Free(Calls::FreeFunction function, std::uintptr_t heap, std::uintptr_t base, std::uint32_t& exception) noexcept
    {
        return active ? active->Free(function, heap, base, exception) : ClockResult::NotInitialized;
    }
}
