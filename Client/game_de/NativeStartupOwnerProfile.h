/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native startup-owner metadata for bounded read-only observations
 *****************************************************************************/
// The native typed-outer loop accepts declared GameMode subclasses; vptr equality
// cannot replace that relation. Static proof: mta-startup-owner-profile-20261019,
// startup-owner-read-profile SHA256 d02607e7b4b8d3b08d4c70d3803239ccd7249d13b549afb4864e3516bfaee10b.
// Missing caches stay missing: these pins never authorize native initialization.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mta::game_de::NativeStartupOwnerProfile
{
    inline constexpr std::uintptr_t rootRva = 0x5725750;
    inline constexpr std::uintptr_t engineRva = 0x57d7f70;
    inline constexpr std::uintptr_t gameModeClassCacheRva = 0x5727430;
    inline constexpr std::uintptr_t playerControllerClassCacheRva = 0x5727ca8;
    inline constexpr std::uintptr_t viewportClassCacheRva = 0x5728948;
    inline constexpr std::uintptr_t gameModeVtableRva = 0x42039c8;
    inline constexpr std::uintptr_t gameModeGetWorldSlotRva = 0x4203b20;
    inline constexpr std::uintptr_t gameModeGetWorldRva = 0x30027f0;
    inline constexpr std::size_t    objectClassOffset = 0x10;
    inline constexpr std::size_t    objectOuterOffset = 0x20;
    inline constexpr std::size_t    gameModeInterfaceOffset = 0x2c0;
    inline constexpr std::size_t    engineViewportOffset = 0x780;
    inline constexpr std::size_t    viewportManagerOffset = 0x360;
    inline constexpr std::size_t    classChainOffset = 0x30;
    inline constexpr std::size_t    classChainIndexOffset = 0x38;
    // These cache slots identify declared UClass metadata, including subclasses.
    // Do not invoke a getter to initialize a missing cache or substitute vptr equality.
    // Typed outer starts at root->Outer, and owner+2c0 correlation is separate.
    inline constexpr std::size_t                   maximumPinBytes = 93;
    inline constexpr std::uintptr_t                gameModeCacheReadRva = 0xaf461a;
    inline constexpr std::array<unsigned char, 16> gameModeCacheRead = {
        0x48, 0x8b, 0x05, 0x0f, 0x2e, 0xc3, 0x04, 0x48, 0x85, 0xc0, 0x0f, 0x85, 0x8a, 0x00, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t               gameModeRegistrationCacheRva = 0xaf4647;
    inline constexpr std::array<unsigned char, 7> gameModeRegistrationCache = {
        0x4c, 0x8d, 0x05, 0xe2, 0x2d, 0xc3, 0x04,
    };
    inline constexpr std::uintptr_t               gameModeNameArgumentRva = 0xaf4659;
    inline constexpr std::array<unsigned char, 7> gameModeNameArgument = {
        0x48, 0x8d, 0x15, 0x0a, 0x1d, 0x76, 0x03,
    };
    inline constexpr std::uintptr_t               gameModePackageArgumentRva = 0xaf466b;
    inline constexpr std::array<unsigned char, 7> gameModePackageArgument = {
        0x48, 0x8d, 0x0d, 0x5e, 0xcb, 0x73, 0x03,
    };
    inline constexpr std::uintptr_t                gameModeRegistrationReturnRva = 0xaf46a8;
    inline constexpr std::array<unsigned char, 20> gameModeRegistrationReturn = {
        0xe8, 0x93, 0x5b, 0x02, 0x01, 0x48, 0x8b, 0x05, 0x7c, 0x2d, 0xc3, 0x04, 0x48, 0x81, 0xc4, 0x88, 0x00, 0x00, 0x00, 0xc3,
    };
    inline constexpr std::uintptr_t                gameModeNameRva = 0x425636a;
    inline constexpr std::array<unsigned char, 24> gameModeName = {
        0x47, 0x00, 0x54, 0x00, 0x41, 0x00, 0x47, 0x00, 0x61, 0x00, 0x6d, 0x00, 0x65, 0x00, 0x4d, 0x00, 0x6f, 0x00, 0x64, 0x00, 0x65, 0x00, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t                playerControllerCacheReadRva = 0xaf477a;
    inline constexpr std::array<unsigned char, 16> playerControllerCacheRead = {
        0x48, 0x8b, 0x05, 0x27, 0x35, 0xc3, 0x04, 0x48, 0x85, 0xc0, 0x0f, 0x85, 0x8a, 0x00, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t               playerControllerRegistrationCacheRva = 0xaf47a7;
    inline constexpr std::array<unsigned char, 7> playerControllerRegistrationCache = {
        0x4c, 0x8d, 0x05, 0xfa, 0x34, 0xc3, 0x04,
    };
    inline constexpr std::uintptr_t               playerControllerNameArgumentRva = 0xaf47b9;
    inline constexpr std::array<unsigned char, 7> playerControllerNameArgument = {
        0x48, 0x8d, 0x15, 0x4a, 0x33, 0x77, 0x03,
    };
    inline constexpr std::uintptr_t               playerControllerPackageArgumentRva = 0xaf47cb;
    inline constexpr std::array<unsigned char, 7> playerControllerPackageArgument = {
        0x48, 0x8d, 0x0d, 0xfe, 0xc9, 0x73, 0x03,
    };
    inline constexpr std::uintptr_t                playerControllerRegistrationReturnRva = 0xaf4808;
    inline constexpr std::array<unsigned char, 20> playerControllerRegistrationReturn = {
        0xe8, 0x33, 0x5a, 0x02, 0x01, 0x48, 0x8b, 0x05, 0x94, 0x34, 0xc3, 0x04, 0x48, 0x81, 0xc4, 0x88, 0x00, 0x00, 0x00, 0xc3,
    };
    inline constexpr std::uintptr_t                playerControllerNameRva = 0x4267b0a;
    inline constexpr std::array<unsigned char, 40> playerControllerName = {
        0x47, 0x00, 0x54, 0x00, 0x41, 0x00, 0x50, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x79, 0x00, 0x65, 0x00, 0x72, 0x00, 0x43, 0x00,
        0x6f, 0x00, 0x6e, 0x00, 0x74, 0x00, 0x72, 0x00, 0x6f, 0x00, 0x6c, 0x00, 0x6c, 0x00, 0x65, 0x00, 0x72, 0x00, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t                viewportCacheReadRva = 0xbd34aa;
    inline constexpr std::array<unsigned char, 16> viewportCacheRead = {
        0x48, 0x8b, 0x05, 0x97, 0x54, 0xb5, 0x04, 0x48, 0x85, 0xc0, 0x0f, 0x85, 0x8a, 0x00, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t               viewportRegistrationCacheRva = 0xbd34d7;
    inline constexpr std::array<unsigned char, 7> viewportRegistrationCache = {
        0x4c, 0x8d, 0x05, 0x6a, 0x54, 0xb5, 0x04,
    };
    inline constexpr std::uintptr_t               viewportNameArgumentRva = 0xbd34e9;
    inline constexpr std::array<unsigned char, 7> viewportNameArgument = {
        0x48, 0x8d, 0x15, 0x22, 0x4a, 0x6b, 0x03,
    };
    inline constexpr std::uintptr_t               viewportPackageArgumentRva = 0xbd34fb;
    inline constexpr std::array<unsigned char, 7> viewportPackageArgument = {
        0x48, 0x8d, 0x0d, 0xce, 0xdc, 0x65, 0x03,
    };
    inline constexpr std::uintptr_t                viewportRegistrationReturnRva = 0xbd3538;
    inline constexpr std::array<unsigned char, 20> viewportRegistrationReturn = {
        0xe8, 0x03, 0x6d, 0xf4, 0x00, 0x48, 0x8b, 0x05, 0x04, 0x54, 0xb5, 0x04, 0x48, 0x81, 0xc4, 0x88, 0x00, 0x00, 0x00, 0xc3,
    };
    inline constexpr std::uintptr_t                viewportNameRva = 0x4287f12;
    inline constexpr std::array<unsigned char, 36> viewportName = {
        0x47, 0x00, 0x54, 0x00, 0x41, 0x00, 0x56, 0x00, 0x69, 0x00, 0x65, 0x00, 0x77, 0x00, 0x70, 0x00, 0x6f, 0x00,
        0x72, 0x00, 0x74, 0x00, 0x43, 0x00, 0x6c, 0x00, 0x69, 0x00, 0x65, 0x00, 0x6e, 0x00, 0x74, 0x00, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t                gameBasePackageRva = 0x42311d0;
    inline constexpr std::array<unsigned char, 32> gameBasePackage = {
        0x2f, 0x00, 0x53, 0x00, 0x63, 0x00, 0x72, 0x00, 0x69, 0x00, 0x70, 0x00, 0x74, 0x00, 0x2f, 0x00,
        0x47, 0x00, 0x54, 0x00, 0x41, 0x00, 0x42, 0x00, 0x61, 0x00, 0x73, 0x00, 0x65, 0x00, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t                ownerOuterSearchRva = 0xafebff;
    inline constexpr std::array<unsigned char, 75> ownerOuterSearch = {
        0xe8, 0x0c, 0x5a, 0xff, 0xff, 0x48, 0x8b, 0x53, 0x20, 0x4c, 0x8b, 0xc8, 0x45, 0x33, 0xe4, 0x66, 0x90, 0x49, 0x8b, 0xcc, 0x48, 0x85, 0xd2, 0x74, 0x24,
        0x48, 0x8b, 0x42, 0x10, 0x4d, 0x8d, 0x41, 0x30, 0x49, 0x63, 0x48, 0x08, 0x3b, 0x48, 0x38, 0x7f, 0x0a, 0x48, 0x8b, 0x40, 0x30, 0x4c, 0x39, 0x04, 0xc8,
        0x74, 0x06, 0x48, 0x8b, 0x52, 0x20, 0xeb, 0xd7, 0x48, 0x8b, 0xca, 0x48, 0x8b, 0x01, 0x4c, 0x89, 0x74, 0x24, 0x50, 0xff, 0x90, 0x58, 0x01, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t                playerControllerTypeRva = 0xafec67;
    inline constexpr std::array<unsigned char, 38> playerControllerType = {
        0xe8, 0x04, 0x5b, 0xff, 0xff, 0x49, 0x8b, 0x56, 0x10, 0x4c, 0x8d, 0x40, 0x30, 0x48, 0x63, 0x40, 0x38, 0x3b, 0x42,
        0x38, 0x7f, 0x0d, 0x48, 0x8b, 0xc8, 0x48, 0x8b, 0x42, 0x30, 0x4c, 0x39, 0x04, 0xc8, 0x74, 0x03, 0x4d, 0x8b, 0xf4,
    };
    inline constexpr std::uintptr_t                viewportManagerLookupRva = 0xbd36f0;
    inline constexpr std::array<unsigned char, 93> viewportManagerLookup = {
        0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0x05, 0x73, 0x48, 0xc0, 0x04, 0x48, 0x8b, 0x98, 0x80, 0x07, 0x00, 0x00, 0x48, 0x85, 0xdb, 0x74,
        0x3c, 0xe8, 0x92, 0xfd, 0xff, 0xff, 0x48, 0x8b, 0x53, 0x10, 0x4c, 0x8d, 0x40, 0x30, 0x48, 0x63, 0x40, 0x38, 0x3b, 0x42, 0x38, 0x7f, 0x26, 0x48,
        0x8b, 0xc8, 0x48, 0x8b, 0x42, 0x30, 0x4c, 0x39, 0x04, 0xc8, 0x75, 0x19, 0x48, 0x8b, 0x83, 0x60, 0x03, 0x00, 0x00, 0x48, 0x85, 0xc0, 0x75, 0x0f,
        0x48, 0x8b, 0xcb, 0x48, 0x83, 0xc4, 0x20, 0x5b, 0xe9, 0x0b, 0x00, 0x00, 0x00, 0x33, 0xc0, 0x48, 0x83, 0xc4, 0x20, 0x5b, 0xc3,
    };
    inline constexpr std::uintptr_t                gameModeConstructorVptrRva = 0xc5bb9d;
    inline constexpr std::array<unsigned char, 10> gameModeConstructorVptr = {
        0x48, 0x8d, 0x05, 0x24, 0x7e, 0x5a, 0x03, 0x48, 0x89, 0x03,
    };
    inline constexpr std::uintptr_t                ownerRootPublicationRva = 0xb4c6e5;
    inline constexpr std::array<unsigned char, 17> ownerRootPublication = {
        0x48, 0x89, 0x86, 0xc0, 0x02, 0x00, 0x00, 0x48, 0x8b, 0xc8, 0x48, 0x89, 0x05, 0x5a, 0x90, 0xbd, 0x04,
    };
    inline constexpr std::uintptr_t               ownerNormalFrameBackreferenceRva = 0xb4cb66;
    inline constexpr std::array<unsigned char, 7> ownerNormalFrameBackreference = {
        0x48, 0x8b, 0x8b, 0xc0, 0x02, 0x00, 0x00,
    };
    inline constexpr std::uintptr_t               ownerPostFrameBackreferenceRva = 0xb4cb8a;
    inline constexpr std::array<unsigned char, 7> ownerPostFrameBackreference = {
        0x48, 0x8b, 0x8b, 0xc0, 0x02, 0x00, 0x00,
    };
    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
        bool                 code;
    };
    inline constexpr std::array<Pin, 26> pins = {{
        {gameModeCacheReadRva, gameModeCacheRead.data(), gameModeCacheRead.size(), true},
        {gameModeRegistrationCacheRva, gameModeRegistrationCache.data(), gameModeRegistrationCache.size(), true},
        {gameModeNameArgumentRva, gameModeNameArgument.data(), gameModeNameArgument.size(), true},
        {gameModePackageArgumentRva, gameModePackageArgument.data(), gameModePackageArgument.size(), true},
        {gameModeRegistrationReturnRva, gameModeRegistrationReturn.data(), gameModeRegistrationReturn.size(), true},
        {gameModeNameRva, gameModeName.data(), gameModeName.size(), false},
        {playerControllerCacheReadRva, playerControllerCacheRead.data(), playerControllerCacheRead.size(), true},
        {playerControllerRegistrationCacheRva, playerControllerRegistrationCache.data(), playerControllerRegistrationCache.size(), true},
        {playerControllerNameArgumentRva, playerControllerNameArgument.data(), playerControllerNameArgument.size(), true},
        {playerControllerPackageArgumentRva, playerControllerPackageArgument.data(), playerControllerPackageArgument.size(), true},
        {playerControllerRegistrationReturnRva, playerControllerRegistrationReturn.data(), playerControllerRegistrationReturn.size(), true},
        {playerControllerNameRva, playerControllerName.data(), playerControllerName.size(), false},
        {viewportCacheReadRva, viewportCacheRead.data(), viewportCacheRead.size(), true},
        {viewportRegistrationCacheRva, viewportRegistrationCache.data(), viewportRegistrationCache.size(), true},
        {viewportNameArgumentRva, viewportNameArgument.data(), viewportNameArgument.size(), true},
        {viewportPackageArgumentRva, viewportPackageArgument.data(), viewportPackageArgument.size(), true},
        {viewportRegistrationReturnRva, viewportRegistrationReturn.data(), viewportRegistrationReturn.size(), true},
        {viewportNameRva, viewportName.data(), viewportName.size(), false},
        {gameBasePackageRva, gameBasePackage.data(), gameBasePackage.size(), false},
        {ownerOuterSearchRva, ownerOuterSearch.data(), ownerOuterSearch.size(), true},
        {playerControllerTypeRva, playerControllerType.data(), playerControllerType.size(), true},
        {viewportManagerLookupRva, viewportManagerLookup.data(), viewportManagerLookup.size(), true},
        {gameModeConstructorVptrRva, gameModeConstructorVptr.data(), gameModeConstructorVptr.size(), true},
        {ownerRootPublicationRva, ownerRootPublication.data(), ownerRootPublication.size(), true},
        {ownerNormalFrameBackreferenceRva, ownerNormalFrameBackreference.data(), ownerNormalFrameBackreference.size(), true},
        {ownerPostFrameBackreferenceRva, ownerPostFrameBackreference.data(), ownerPostFrameBackreference.size(), true},
    }};
}
