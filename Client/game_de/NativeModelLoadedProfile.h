/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native ordinary-model streaming state predicate and row evidence
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
    inline constexpr std::uint32_t  ModelCount = NativeModelBankProfile::ModelCount;
    inline constexpr std::size_t    RowStride = 16, FlagsOffset = 12, StateOffset = 13, ArrayBytes = ModelCount * RowStride;
    inline constexpr unsigned char  LoadedState = 1;
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
    inline constexpr std::array<Pin, 4>            Code = {{
        {NativeStreamingFileIdsProfile::initializeRva, NativeStreamingFileIdsProfile::initialize.data(), NativeStreamingFileIdsProfile::initialize.size()},
        {0x1288417, NativeLoadedListConsumer.data(), NativeLoadedListConsumer.size()},
        {0x1288630, NativeLoadedRequestConsumer.data(), NativeLoadedRequestConsumer.size()},
        {0x12895b1, NativeRemoveClearsLoadedState.data(), NativeRemoveClearsLoadedState.size()},
    }};
    inline constexpr std::size_t                   MaximumPinBytes = NativeStreamingFileIdsProfile::initialize.size();
}
