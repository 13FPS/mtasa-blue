/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate typed resource inputs for the genuine native request
 ****************************************************************************/
#pragma once
#include "NativeModelLoadedReader.h"
#include "NativeUObjectMembershipAccess.h"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace mta::game_de::NativeStreamingResourceContext
{
    struct Value
    {
        std::uintptr_t root = 0, datProvider = 0;
        std::int32_t   imgFileCount = 0;
        // Exact copied inputs stay on this synchronous callback's stack-owned
        // C++ value. No engine object, file buffer or cache allocation is owned.
        struct Token
        {
            std::uintptr_t             address = 0;
            std::vector<unsigned char> bytes;
            bool                       stableAfterEntry = true;
        };
        struct Buffer
        {
            std::uintptr_t address = 0;
            std::size_t    bytes = 0;
        };
        struct FileCheck
        {
            std::uintptr_t object = 0;
            std::uint32_t  hash = 0;
        };
        std::vector<Token>                                   tokens;
        std::vector<Buffer>                                  buffers;
        std::vector<FileCheck>                               fileChecks;
        std::vector<NativeUObjectMembershipAccess::Snapshot> memberships;
    };
    bool        QualifyProfile(std::uintptr_t) noexcept;
    ClockResult Capture(std::uintptr_t, const NativeModelLoadedReader::Value&, Value&) noexcept;
    ClockResult Recheck(std::uintptr_t, const Value&, bool afterEntry = false) noexcept;
    ClockResult CheckCurrentLists(std::uintptr_t, std::uint32_t) noexcept;
}
