/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact read-only jetpack profile shared by reader and Runtime fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldJetpackProfile.h"
#include <windows.h>
namespace NativeWorldJetpackFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        namespace P = mta::game_de::NativeWorldJetpackProfile;
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& group : P::Groups)
        {
            const auto entry = image + group.entryRva;
            const auto branch = static_cast<std::uint32_t>(group.branchRva);
            writer(host, group.slotRva, &entry, sizeof(entry), PAGE_READONLY);
            writer(host, group.tableSlotRva, &branch, sizeof(branch), PAGE_READONLY);
        }
        for (const auto& slot : P::Slots)
        {
            const auto target = image + slot.targetRva;
            writer(host, P::VtableRva + slot.offset, &target, sizeof(target), PAGE_READONLY);
        }
    }
}
