/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual static-Atomic receiver geometry for owned Runtime fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeModelObjectGroupProfile.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <vector>
namespace NativeModelObjectGroupFixture
{
    namespace P = mta::game_de::NativeModelObjectGroupProfile;
    namespace B = mta::game_de::NativeModelBankProfile;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    template <class T>
    void Put(unsigned char* bytes, std::size_t offset, const T& value)
    {
        std::memcpy(bytes + offset, &value, sizeof(value));
    }
    inline void Install(std::uintptr_t image, Writer writer, void* host, std::uint32_t used = 2, std::uint32_t firstModel = 17)
    {
        // The allocated prefix represents the genuine constructor + AddAtomic
        // + vslot40 itinerary. No host native method or engine code executes.
        // Full pool pages are owned image storage, not a heap pointer with a
        // forged type getter; unused rows remain outside the counted prefix.
        if (used > P::PoolCapacity || used > B::ModelCount || firstModel > B::ModelCount - used)
            return;
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        std::array<std::uintptr_t, P::VtableMethods.size()> methods{};
        for (std::size_t index = 0; index != methods.size(); ++index)
            methods[index] = image + P::VtableMethods[index];
        writer(host, P::VtableRva, methods.data(), sizeof(methods), PAGE_READONLY);
        std::vector<unsigned char> pool(P::PoolBytes);
        for (std::uint32_t index = 0; index != used; ++index)
        {
            auto* row = pool.data() + static_cast<std::size_t>(index) * P::PoolStride;
            Put(row, 0, image + P::VtableRva);
            Put(row, 0x22, std::uint16_t{0});
            Put(row, 0x26, std::uint32_t{0xffffffff});
            Put(row, 0x2a, std::uint16_t{0xc0});
            Put(row, 0x38, std::uint32_t{0x44fa0000});
        }
        writer(host, P::PoolRva, pool.data(), pool.size(), PAGE_READWRITE);
        const auto count = static_cast<std::int32_t>(used);
        writer(host, P::PoolCountRva, &count, sizeof(count), PAGE_READWRITE);
        for (std::uint32_t index = 0; index != used; ++index)
        {
            const auto address = image + P::PoolRva + static_cast<std::size_t>(index) * P::PoolStride;
            writer(host, B::BankRva + static_cast<std::size_t>(firstModel + index) * B::SlotBytes, &address, sizeof(address), PAGE_READWRITE);
        }
    }
    inline std::uintptr_t Row(std::uintptr_t image, std::uint32_t index = 0) noexcept
    {
        return image + P::PoolRva + static_cast<std::size_t>(index) * P::PoolStride;
    }
    inline std::uint16_t Group(std::uintptr_t image, std::uint32_t index = 0) noexcept
    {
        std::uint16_t value = 0;
        std::memcpy(&value, reinterpret_cast<void*>(Row(image, index) + P::FieldOffset), sizeof(value));
        return value;
    }
}
