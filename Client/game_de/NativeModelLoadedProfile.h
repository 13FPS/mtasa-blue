/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native resource-row streaming state predicate and exact array evidence
 ****************************************************************************/
#pragma once
#include "NativeModelBankProfile.h"
#include "NativeStreamingFileIdsProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace mta::game_de::NativeModelLoadedProfile
{
    inline constexpr std::uintptr_t ArrayRva = NativeStreamingFileIdsProfile::nativeArrayRva;
    // The initializer's byte limit, rather than the DFF metadata-bank limit,
    // bounds every physical resource row, including its four list sentinels.
    // Requesting a DFF still uses NativeModelBankProfile's separate20000 limit.
    inline constexpr std::uint32_t ArrayExtent =
        std::uint32_t{NativeStreamingFileIdsProfile::initialize[0x43]} | (std::uint32_t{NativeStreamingFileIdsProfile::initialize[0x44]} << 8) |
        (std::uint32_t{NativeStreamingFileIdsProfile::initialize[0x45]} << 16) | (std::uint32_t{NativeStreamingFileIdsProfile::initialize[0x46]} << 24);
    inline constexpr std::uint32_t ModelCount = ArrayExtent / 16;
    inline constexpr std::size_t   RowStride = 16, FlagsOffset = 12, StateOffset = 13, ArrayBytes = ModelCount * RowStride;
    inline constexpr unsigned char LoadedState = 1;
    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
    };
    // Native request and entity-list consumers independently compare state1.
    // This is the ordinary stream state predicate, not a model/render lifetime
    // lease, the SDK's compound IsLoaded check or its upgrade-model special case.
    inline constexpr std::array<unsigned char, 15> NativeLoadedListConsumer = {0x41, 0x80, 0xbc, 0xc6, 0x9d, 0x7f, 0x29, 0x05,
                                                                               0x01, 0x0f, 0x84, 0x97, 0x00, 0x00, 0x00};
    inline constexpr std::array<unsigned char, 13> NativeLoadedRequestConsumer = {0x41, 0x0f, 0xb6, 0x0e, 0x80, 0xf9, 0x01, 0x0f, 0x85, 0xef, 0x00, 0x00, 0x00};
    inline constexpr std::array<unsigned char, 21> NativeRemoveClearsLoadedState = {0x4d, 0x8d, 0xa7, 0x90, 0x7f, 0x29, 0x05, 0x4c, 0x03, 0xe5, 0xbd,
                                                                                    0xff, 0xff, 0x00, 0x00, 0x41, 0xc6, 0x44, 0x24, 0x0d, 0x00};
    inline constexpr std::array<unsigned char, 21> NativeScriptLoadedPrimary = {0x48, 0x63, 0xc3, 0x40, 0x32, 0xf6, 0x4c, 0x8b, 0xf0, 0x4d, 0x03,
                                                                                0xf6, 0x43, 0x80, 0xbc, 0xf7, 0x9d, 0x7f, 0x29, 0x05, 0x01};
    inline constexpr std::array<Pin, 5>            Code = {{
        {NativeStreamingFileIdsProfile::initializeRva, NativeStreamingFileIdsProfile::initialize.data(), NativeStreamingFileIdsProfile::initialize.size()},
        {0x10d9650, NativeScriptLoadedPrimary.data(), NativeScriptLoadedPrimary.size()},
        {0x1288417, NativeLoadedListConsumer.data(), NativeLoadedListConsumer.size()},
        {0x1288630, NativeLoadedRequestConsumer.data(), NativeLoadedRequestConsumer.size()},
        {0x12895b1, NativeRemoveClearsLoadedState.data(), NativeRemoveClearsLoadedState.size()},
    }};
    static_assert(NativeStreamingFileIdsProfile::initialize[0x40] == RowStride && ArrayBytes == ArrayExtent && ArrayExtent % RowStride == 0);
    inline constexpr std::size_t MaximumPinBytes = NativeStreamingFileIdsProfile::initialize.size();
}
