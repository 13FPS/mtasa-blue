/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: New grant profile and actual weapon/model-reader setup for owned fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativePedWeaponGrantProfile.h"
#include "../../Client/game_de/NativePedWeaponProfile.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include "../../Client/game_de/NativeWeaponInfoProfile.h"
#include "../../Client/game_de/NativeWeaponInfoReader.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#include <windows.h>
namespace NativePedWeaponGrantFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Info(Writer writer, void* host, std::uint32_t index, const mta::game_de::NativeWeaponInfoValues& value)
    {
        namespace P = mta::game_de::NativeWeaponInfoProfile;
        writer(host, P::BankRva + index * P::RowBytes, &value, sizeof(value), PAGE_READWRITE);
    }
    inline void Model(Writer writer, void* host, std::uint32_t index, std::uintptr_t model, bool loaded)
    {
        namespace B = mta::game_de::NativeModelBankProfile;
        namespace L = mta::game_de::NativeModelLoadedProfile;
        writer(host, B::BankRva + index * B::SlotBytes, &model, sizeof(model), PAGE_READWRITE);
        const unsigned char state = loaded ? 1 : 0;
        writer(host, L::ArrayRva + index * L::RowStride + L::StateOffset, &state, sizeof(state), PAGE_READWRITE);
    }
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        namespace P = mta::game_de::NativePedWeaponGrantProfile;
        namespace I = mta::game_de::NativeWeaponInfoProfile;
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePedWeaponProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePedMetadataProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : I::Code)
            writer(host, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : I::Literals)
            writer(host, pin.rva, pin.bytes.data(), pin.size, PAGE_READONLY);
        for (const auto& pin : mta::game_de::NativeModelBankProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeModelLoadedProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto group = image + P::GroupRva;
        const auto branch = static_cast<std::uint32_t>(P::BranchRva);
        writer(host, P::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
        // Native remap/table bytes share .text page10d8000 with the pool
        // handle consumer10d86da. Keep that page executable so installing
        // fixture constants cannot revoke the real pool reader's code.
        writer(host, P::RemapRva, &P::Selector, sizeof(P::Selector), PAGE_EXECUTE_READ);
        writer(host, P::TableSlotRva, &branch, sizeof(branch), PAGE_EXECUTE_READ);
        const std::uint16_t empty = 0;
        for (const auto& storage : P::ImmediateStorage)
            writer(host, storage.rva, &empty, storage.bytes, PAGE_READONLY);
        static const std::array<unsigned char, mta::game_de::NativeModelBankProfile::BankBytes> bank{};
        writer(host, mta::game_de::NativeModelBankProfile::BankRva, bank.data(), bank.size(), PAGE_READWRITE);
        // Rows are real complete native-shaped records consumed by the actual
        // bound reader. -1 is the native no-model sentinel, not modelzero.
        for (std::uint32_t index = 0; index < I::RowCount; ++index)
        {
            mta::game_de::NativeWeaponInfoValues value;
            value.model = value.model2 = -1;
            value.slot = static_cast<std::int32_t>(index % 13);
            value.clipAmmo = 17;
            if (index == 22 || index == 23)
                value.slot = 2;
            Info(writer, host, index, value);
        }
    }
}
