/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Proved pause-safe timer continuation for the existing native owner
 *****************************************************************************/
#pragma once

#include <array>
#include <cstdint>

namespace mta::game_de::NativeGamePauseProfile
{
    // Disk-only evidence: mta-pause-owner-20261019-094017/pause-owner-evidence.json.
    // Unlike the weather continuation, both admitted gameplay timer origins
    // keep running while user pause is set. Other callers retain native behavior
    // but cannot adopt an engine owner. Detours relocates the RIP-relative entry.
    inline constexpr std::uintptr_t                userPauseRva = 0x522bcf3, entryRva = 0x1186920;
    inline constexpr std::array<unsigned char, 11> entry = {0x48, 0x83, 0xec, 0x58, 0x48, 0x8b, 0x0d, 0x9d, 0xb2, 0x0b, 0x04};
    inline constexpr std::uintptr_t                tailRva = 0x118692b;
    inline constexpr std::array<unsigned char, 9>  tail = {0x48, 0x85, 0xc9, 0x0f, 0x84, 0x83, 0x02, 0x00, 0x00};
    inline constexpr std::uintptr_t                pauseReadsRva = 0x1186a91;
    inline constexpr std::array<unsigned char, 14> pauseReads = {0x0f, 0xb6, 0x15, 0x5b, 0x52, 0x0a, 0x04, 0x0f, 0xb6, 0x0d, 0x4c, 0x8d, 0x09, 0x04};
    inline constexpr std::uintptr_t                pauseBranchRva = 0x1186afa;
    inline constexpr std::array<unsigned char, 11> pauseBranch = {0x84, 0xd2, 0x75, 0x04, 0x84, 0xc9, 0x74, 0x03, 0x0f, 0x57, 0xf6};
    inline constexpr std::uintptr_t                frameRva = 0x1186b99;
    inline constexpr std::array<unsigned char, 6>  frame = {0xff, 0x05, 0xfd, 0x37, 0x0b, 0x04};
    inline constexpr std::uintptr_t                returnRva = 0x1186bb7;
    inline constexpr std::array<unsigned char, 5>  returnCode = {0x48, 0x83, 0xc4, 0x58, 0xc3};
    inline constexpr std::uintptr_t                normalCallRva = 0x1152082, normalReturnRva = 0x1152087;
    inline constexpr std::array<unsigned char, 5>  normalCall = {0xe8, 0x99, 0x48, 0x03, 0x00};
    inline constexpr std::uintptr_t                alternateCallRva = 0x1296ed5, alternateReturnRva = 0x1296eda;
    inline constexpr std::array<unsigned char, 10> alternateCall = {0xe8, 0x36, 0x2f, 0xf7, 0xff, 0xe9, 0x4b, 0x01, 0x00, 0x00};
    inline constexpr std::uintptr_t                gameplayTailRva = 0x120a33f;
    inline constexpr std::array<unsigned char, 19> gameplayTail = {0x48, 0x8b, 0x5c, 0x24, 0x40, 0x48, 0x8b, 0x7c, 0x24, 0x30,
                                                                   0x48, 0x83, 0xc4, 0x38, 0xe9, 0xce, 0xc5, 0xf7, 0xff};
}
