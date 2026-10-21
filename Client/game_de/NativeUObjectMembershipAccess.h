/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate borrowed UObject identity observations, never a GC lease
 *****************************************************************************/
#pragma once
#include "NativeClassChainAccess.h"
#include "NativeStartupProvidersProfile.h"
#include "NativeStartupWorldProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    // The caller must already own the exact native synchronous GameThread
    // scope and qualify its GC, image and object-type protocols. Registry
    // membership cannot keep an object alive or authorize a deferred call.
    class NativeUObjectMembershipAccess final
    {
    public:
        using ReadMemory = NativeClassChainAccess::ReadMemory;
        struct Snapshot
        {
            std::uintptr_t objectAddress = 0, vtableAddress = 0, classAddress = 0, outerAddress = 0, itemAddress = 0;
            std::int32_t   index = -1;
            std::uint32_t  serial = 0;

        private:
            friend class NativeUObjectMembershipAccess;
            struct Token
            {
                std::uintptr_t               address = 0;
                std::size_t                  size = 0;
                std::array<unsigned char, 8> bytes{};
            };
            std::array<Token, 11> tokens{};
            std::size_t           count = 0;
        };

        NativeUObjectMembershipAccess(std::uintptr_t imageBase, ReadMemory guardedRead) noexcept : image(imageBase), read(guardedRead) {}

        // Native raw->weak conversion allocates a serial only when needed.
        // Consequently a zero item serial is a legitimate raw observation;
        // this reader never calls the allocator or manufactures a weak handle.
        // Every refusal leaves the previous output unchanged.
        bool Capture(std::uintptr_t object, Snapshot& output) const noexcept
        {
            Snapshot first{}, second{};
            if (!Gather(object, first) || !Fence(first) || !Gather(object, second) || !Fence(second) || !Equal(first, second))
                return false;
            output = second;
            return true;
        }

        // Snapshots belong on the stack inside the same borrowed callback.
        // Rechecking pointer, header, registry route, item flags and serial
        // catches reuse without claiming to pin any of those dependencies.
        bool Recheck(const Snapshot& previous) const noexcept
        {
            Snapshot current{};
            return previous.count == previous.tokens.size() && Capture(previous.objectAddress, current) && Equal(previous, current);
        }

        // Apply only to an existing native weak token. Serial zero is rejected
        // by the actual weak resolver even though raw membership can observe it.
        static bool MatchesWeak(const Snapshot& value, std::int32_t index, std::uint32_t serial) noexcept
        {
            return value.count == value.tokens.size() && index >= 0 && serial != 0 && value.index == index && value.serial == serial;
        }

    private:
        static_assert(sizeof(std::uintptr_t) == 8);
        static_assert(NativeStartupWorldProfile::objectIndexOffset == 0xc);
        static_assert(NativeStartupProvidersProfile::chunkIndexShift == 16 && NativeStartupProvidersProfile::objectItemBytes == 24);
        static_assert(NativeStartupProvidersProfile::itemSerialOffset == 0x10 && NativeStartupProvidersProfile::itemRejectMask == 0x30000000);

        static bool Add(std::uintptr_t base, std::size_t offset, std::uintptr_t& output) noexcept
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return false;
            output = base + offset;
            return true;
        }
        bool Copy(std::uintptr_t at, void* output, std::size_t size) const noexcept
        {
            return read && at && size && size <= (std::numeric_limits<std::uintptr_t>::max)() - at && read(at, output, size);
        }
        template <typename T>
        bool Record(Snapshot& sample, std::uintptr_t base, std::size_t offset, T& output) const noexcept
        {
            static_assert(sizeof(T) <= 8);
            Snapshot::Token token{};
            if (sample.count == sample.tokens.size() || !Add(base, offset, token.address))
                return false;
            token.size = sizeof(T);
            if (!Copy(token.address, token.bytes.data(), token.size))
                return false;
            std::memcpy(&output, token.bytes.data(), sizeof(T));
            sample.tokens[sample.count++] = token;
            return true;
        }
        bool Gather(std::uintptr_t object, Snapshot& output) const noexcept
        {
            namespace P = NativeStartupProvidersProfile;
            namespace W = NativeStartupWorldProfile;
            Snapshot next{};
            next.objectAddress = object;
            std::uint32_t  objectFlags = 0, itemFlags = 0, itemSerial = 0;
            std::int32_t   count = 0;
            std::uintptr_t chunks = 0, chunk = 0, itemObject = 0;
            if (!Record(next, object, 0, next.vtableAddress) || !next.vtableAddress || !Record(next, object, W::objectFlagsOffset, objectFlags) ||
                !Record(next, object, W::objectIndexOffset, next.index) || next.index < 0 || !Record(next, object, W::objectClassOffset, next.classAddress) ||
                !next.classAddress || !Record(next, object, W::objectOuterOffset, next.outerAddress) || !Record(next, image, P::objectCountRva, count) ||
                count <= 0 || next.index >= count || !Record(next, image, P::objectChunksRva, chunks) || !chunks)
                return false;
            const auto index = static_cast<std::uint32_t>(next.index);
            // Nonnegative signed index and the native 16-bit split bound both
            // products; checked addition still rejects a corrupt chunk pointer.
            if (!Record(next, chunks, static_cast<std::size_t>(index >> P::chunkIndexShift) * P::chunkEntryBytes, chunk) || !chunk ||
                !Add(chunk, static_cast<std::size_t>(index & 0xffffu) * P::objectItemBytes, next.itemAddress) ||
                !Record(next, next.itemAddress, 0, itemObject) || itemObject != object || !Record(next, next.itemAddress, P::itemFlagsOffset, itemFlags) ||
                (itemFlags & P::itemRejectMask) || !Record(next, next.itemAddress, P::itemSerialOffset, itemSerial))
                return false;
            next.serial = itemSerial;
            // RF_ClassDefaultObject (0x10) is not a generic rejection: UClass
            // metadata and UObject kinds have their own separately proved type
            // policy. Header flags are bookended rather than reinterpreted here.
            output = next;
            return true;
        }
        bool Fence(const Snapshot& sample) const noexcept
        {
            for (std::size_t i = 0; i < sample.count; ++i)
            {
                const auto&                  token = sample.tokens[i];
                std::array<unsigned char, 8> after{};
                if (!Copy(token.address, after.data(), token.size) || after != token.bytes)
                    return false;
            }
            return true;
        }
        static bool Equal(const Snapshot& a, const Snapshot& b) noexcept
        {
            if (a.objectAddress != b.objectAddress || a.vtableAddress != b.vtableAddress || a.classAddress != b.classAddress ||
                a.outerAddress != b.outerAddress || a.itemAddress != b.itemAddress || a.index != b.index || a.serial != b.serial || a.count != b.count)
                return false;
            for (std::size_t i = 0; i < a.count; ++i)
                if (a.tokens[i].address != b.tokens[i].address || a.tokens[i].size != b.tokens[i].size || a.tokens[i].bytes != b.tokens[i].bytes)
                    return false;
            return true;
        }
        const std::uintptr_t image;
        const ReadMemory     read;
    };
}
