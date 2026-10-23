/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact new query profile and immediate roots shared by native/owner fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativePedOnScreenProfile.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include <windows.h>
namespace NativePedOnScreenFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        namespace P = mta::game_de::NativePedOnScreenProfile;
        namespace B = mta::game_de::NativeModelBankProfile;
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : mta::game_de::NativePedMetadataProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : B::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto group = image + P::GroupRva;
        const auto branch = static_cast<std::uint32_t>(P::BranchRva);
        writer(host, P::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
        writer(host, P::RemapRva, &P::Selector, sizeof(P::Selector), PAGE_READONLY);
        writer(host, P::TableSlotRva, &branch, sizeof(branch), PAGE_READONLY);
        static const std::array<unsigned char, P::CameraBytes> camera{};
        static const std::array<unsigned char, 8>              depth{};
        static const std::array<unsigned char, 0x28>           fallback{};
        static const std::array<unsigned char, B::BankBytes>   bank{};
        writer(host, P::CameraRva, camera.data(), camera.size(), PAGE_READWRITE);
        writer(host, P::DepthRva, depth.data(), depth.size(), PAGE_READWRITE);
        writer(host, P::FallbackCollisionRva, fallback.data(), fallback.size(), PAGE_READWRITE);
        // Null model slots are a real bound-helper fallback, not fake model
        // objects. The enclosing pool fixture sets its actual ped model WORD.
        writer(host, B::BankRva, bank.data(), bank.size(), PAGE_READWRITE);
    }
}
