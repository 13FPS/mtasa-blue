/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned IPL/tree allocations for actual memory and Runtime fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldIplMemory.h"
#include "../../Client/game_de/NativeWorldIplProfile.h"
#include "../../Client/game_de/NativeWorldIplPoolProfile.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#include "../../Client/game_de/NativeStreamingRemoveProfile.h"
#include <windows.h>
#include <array>
#include <cstring>

namespace NativeWorldIplFixture
{
    namespace P = mta::game_de::NativeWorldIplProfile;
    namespace Q = mta::game_de::NativeWorldIplPoolProfile;
    namespace H = mta::game_de::NativeModelReferenceTransactionProfile;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    template <class T>
    void Put(std::uintptr_t address, std::size_t offset, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(address + offset), &value, sizeof(value));
    }
    struct Pool
    {
        alignas(std::uintptr_t) std::array<unsigned char, 32> header{};
        unsigned char *objects = nullptr, *flags = nullptr;
        std::int32_t   capacity = 0;
        std::size_t    stride = 0;
        Pool(std::int32_t count, std::size_t bytes) : capacity(count), stride(bytes)
        {
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, count * bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(VirtualAlloc(nullptr, count, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Reset();
        }
        ~Pool()
        {
            if (objects)
                VirtualFree(objects, 0, MEM_RELEASE);
            if (flags)
                VirtualFree(flags, 0, MEM_RELEASE);
        }
        Pool(const Pool&) = delete;
        Pool&          operator=(const Pool&) = delete;
        bool           Ready() const noexcept { return objects && flags; }
        std::uintptr_t Address() const noexcept { return reinterpret_cast<std::uintptr_t>(header.data()); }
        std::uintptr_t Row(std::size_t index) const noexcept { return reinterpret_cast<std::uintptr_t>(objects) + index * stride; }
        void           Reset()
        {
            header.fill(0);
            if (!Ready())
                return;
            std::memset(objects, 0, static_cast<std::size_t>(capacity) * stride);
            std::memset(flags, 0x80, capacity);
            Put(Address(), 0, reinterpret_cast<std::uintptr_t>(objects));
            Put(Address(), 8, reinterpret_cast<std::uintptr_t>(flags));
            Put(Address(), 0x10, capacity);
            Put(Address(), 0x14, std::int32_t{-1});
            header[0x18] = 1;
        }
    };
    struct State
    {
        Pool ipl{256, P::IplBytes}, quad{400, P::QuadBytes}, links{75000, P::LinkBytes};
        alignas(std::uintptr_t) std::array<unsigned char, H::HeapBytes> heap{};
        std::uintptr_t image = 0;
        Writer         writer = nullptr;
        void*          host = nullptr;
        State(std::uintptr_t selectedImage, Writer selectedWriter, void* selectedHost) : image(selectedImage), writer(selectedWriter), host(selectedHost)
        {
            Reset();
        }
        bool Ready() const noexcept { return ipl.Ready() && quad.Ready() && links.Ready(); }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        void Stream(std::uint32_t index, unsigned char value)
        {
            Global(mta::game_de::NativeModelLoadedProfile::ArrayRva + (P::IplFileBase + index) * 16 + 13, value);
        }
        void Reset()
        {
            if (!Ready())
                return;
            const auto code = [&](const auto& pins)
            {
                for (const auto& pin : pins)
                    writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            };
            code(P::Code);
            code(Q::Code);
            code(H::Code);
            code(mta::game_de::NativeModelReferenceProfile::Code);
            code(mta::game_de::NativeStreamingRequestProfile::Code);
            code(mta::game_de::NativePedPoolProfile::Code);
            code(mta::game_de::NativeModelBankProfile::Code);
            code(mta::game_de::NativeModelLoadedProfile::Code);
            writer(host, mta::game_de::NativeStreamingRemoveProfile::RemoveRva, mta::game_de::NativeStreamingRemoveProfile::Remove.data(),
                   mta::game_de::NativeStreamingRemoveProfile::Remove.size(), PAGE_EXECUTE_READ);
            Global(Q::HalfRva, Q::HalfBits, PAGE_READONLY);
            writer(host, H::ClassSizesRva, H::ClassSizes.data(), sizeof(H::ClassSizes), PAGE_READONLY);
            heap.fill(0);
            Put(reinterpret_cast<std::uintptr_t>(heap.data()), 0, image + H::HeapVtableRva);
            Global(H::HeapRva, reinterpret_cast<std::uintptr_t>(heap.data()));
            Global(H::HeapAuthorityRva, reinterpret_cast<std::uintptr_t>(heap.data()));
            Global(H::HeapVtableRva + 0x20, image + H::HeapReallocateRva, PAGE_READONLY);
            Global(H::HeapVtableRva + 0x38, image + H::HeapQuantizeRva, PAGE_READONLY);
            std::array<unsigned char, H::LookupBytes> lookup{};
            std::array<std::uint16_t, H::ClassCount>  reverse{};
            std::size_t                               category = 0;
            for (std::size_t index = 0; index != lookup.size(); ++index)
            {
                while (category < H::ClassSizes.size() && H::ClassSizes[category] < index * 16)
                    ++category;
                lookup[index] = static_cast<unsigned char>(category);
            }
            for (std::size_t index = 0; index != reverse.size(); ++index)
                reverse[index] = H::ClassSizes[H::ClassSizes.size() - index - 1];
            writer(host, H::LookupRva, lookup.data(), lookup.size(), PAGE_READWRITE);
            writer(host, H::ReverseSizesRva, reverse.data(), sizeof(reverse), PAGE_READWRITE);
            ipl.Reset();
            quad.Reset();
            links.Reset();
            Global(P::IplPoolRva, ipl.Address());
            Global(P::QuadPoolRva, quad.Address());
            Global(P::LinkPoolRva, links.Address());
            std::array<std::array<unsigned char, 16>, 256> stream{};
            writer(host, mta::game_de::NativeModelLoadedProfile::ArrayRva + P::IplFileBase * 16, stream.data(), sizeof(stream), PAGE_READWRITE);
            // Generic is deliberately occupied and must never be disabled or
            // offered to the filter. The other name has no null terminator.
            const mta::game_de::NativeWorldIplPlan::Rect rect{-20, 20, 20, -20};
            for (std::size_t index = 0; index != 3; ++index)
            {
                ipl.flags[index] = 1;
                Put(ipl.Row(index), 0, rect);
                const std::array<char, 8> name =
                    index == 1 ? std::array<char, 8>{'t', 'e', 's', 't', 'a', 'b', 'c', 'd'} : std::array<char, 8>{'b', 'a', 'r', 'r', 'i', 'e', 'r', 's'};
                Put(ipl.Row(index), P::IplNameOffset, name);
                Put(ipl.Row(index), P::DisabledOffset, static_cast<unsigned char>(0));
                Put(ipl.Row(index), P::DisabledOffset + 1, static_cast<unsigned char>(0x5a));
            }
            quad.flags[0] = 1;
            Put(quad.Row(0), 0, rect);
            Put(quad.Row(0), P::QuadLevelOffset, std::uint32_t{0});
            Global(P::TreeRva, quad.Row(0));
        }
        void List(std::size_t count)
        {
            links.Reset();
            Put(quad.Row(0), P::QuadListOffset, count ? links.Row(0) : std::uintptr_t{0});
            for (std::size_t at = 0; at != count; ++at)
            {
                links.flags[at] = 1;
                Put(links.Row(at), 0, ipl.Row(at % 2 + 1));
                Put(links.Row(at), 8, at + 1 == count ? std::uintptr_t{0} : links.Row(at + 1));
            }
        }
    };
}
