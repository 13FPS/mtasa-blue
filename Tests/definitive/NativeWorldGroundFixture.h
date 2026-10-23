/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Small owned scan/matrix roots shared by primitive and Runtime checks
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldGroundProfile.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <vector>
namespace NativeWorldGroundFixture
{
    struct Roots
    {
        using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
        alignas(std::uintptr_t) std::array<std::array<unsigned char, mta::game_de::NativeWorldGroundProfile::NodeBytes>, 2> free{};
        alignas(std::uintptr_t) std::array<unsigned char, mta::game_de::NativeWorldGroundProfile::NodeBytes> active{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x20> owner{};
        std::uintptr_t image = 0;
        Writer         writer = nullptr;
        void*          host = nullptr;
        Roots(std::uintptr_t selectedImage, Writer selectedWriter, void* selectedHost) : image(selectedImage), writer(selectedWriter), host(selectedHost) {}
        static std::uintptr_t Address(const void* value) { return reinterpret_cast<std::uintptr_t>(value); }
        template <class T>
        static void Put(void* address, std::size_t offset, const T& value)
        {
            std::memcpy(static_cast<unsigned char*>(address) + offset, &value, sizeof(value));
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        void InitializePins()
        {
            namespace P = mta::game_de::NativeWorldGroundProfile;
            for (const auto& pin : P::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            // Empty sectors are intentional: readiness comes from the actual
            // owner and real image storage, never fabricated world entities.
            const std::vector<unsigned char> sectors(P::SectorBytes, 0);
            writer(host, P::SectorsRva, sectors.data(), sectors.size(), PAGE_READWRITE);
        }
        void Reset(bool emptyFree = false)
        {
            namespace P = mta::game_de::NativeWorldGroundProfile;
            for (auto& node : free)
                node.fill(0);
            active.fill(0);
            owner.fill(0);
            const std::array<unsigned char, 2 * P::NodeBytes> sentinels{};
            writer(host, P::ActiveStartRva, sentinels.data(), sentinels.size(), PAGE_READWRITE);
            writer(host, P::FreeStartRva, sentinels.data(), sentinels.size(), PAGE_READWRITE);
            const auto freeStart = image + P::FreeStartRva, freeEnd = image + P::FreeEndRva;
            const auto activeStart = image + P::ActiveStartRva, activeEnd = image + P::ActiveEndRva;
            Global(P::FreeStartRva + P::NextOffset, emptyFree ? freeEnd : Address(free[0].data()));
            Global(P::FreeEndRva + P::PrevOffset, emptyFree ? freeStart : Address(free[1].data()));
            Put(free[0].data(), P::PrevOffset, freeStart);
            Put(free[0].data(), P::NextOffset, Address(free[1].data()));
            Put(free[1].data(), P::PrevOffset, Address(free[0].data()));
            Put(free[1].data(), P::NextOffset, freeEnd);
            Global(P::ActiveStartRva + P::NextOffset, Address(active.data()));
            Global(P::ActiveEndRva + P::PrevOffset, Address(active.data()));
            Put(active.data(), P::PrevOffset, activeStart);
            Put(active.data(), P::NextOffset, activeEnd);
            Put(active.data(), P::OwnerOffset, Address(owner.data()));
            Put(owner.data(), 0x18, Address(active.data()));
            Global(P::ScanRva, std::uint16_t{19});
            Global(P::HitScratchRva, std::uint32_t{0});
            Global(P::IgnoredEntityRva, std::uintptr_t{0});
        }
    };
}
