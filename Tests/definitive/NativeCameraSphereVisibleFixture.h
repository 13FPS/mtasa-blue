/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Install only genuine camera predicate code and its immediate native storage
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeCameraSphereVisibleProfile.h"
#include <windows.h>
namespace NativeCameraSphereVisibleFixture
{
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline void Install(std::uintptr_t, Writer writer, void* host)
    {
        namespace P = mta::game_de::NativeCameraSphereVisibleProfile;
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        static const std::array<unsigned char, P::CameraBytes> camera{};
        static const std::array<unsigned char, 8>              depth{};
        writer(host, P::CameraRva, camera.data(), camera.size(), PAGE_READWRITE);
        writer(host, P::DepthRva, depth.data(), depth.size(), PAGE_READWRITE);
    }
}
