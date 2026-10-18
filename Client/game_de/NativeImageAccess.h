/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared exact-image and checked-page access for native DE services
 *****************************************************************************/
#pragma once

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace mta::game_de::NativeImageAccess
{
    // Native services share the established executable identity and page checks
    // while retaining their own instruction signatures and object-layout guards.
    bool CheckExecutableBuild() noexcept;
    bool ImagePages(std::uintptr_t image, std::uintptr_t address, std::size_t bytes, bool writable = false, bool executable = false) noexcept;
    bool ReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false, bool executable = false) noexcept;
    bool SectionContains(const IMAGE_SECTION_HEADER& section, std::size_t rva, std::size_t bytes, DWORD required, DWORD forbidden) noexcept;
}
