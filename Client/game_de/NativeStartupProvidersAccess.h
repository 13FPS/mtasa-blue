/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guarded observations of controller/viewport startup provider links
 *****************************************************************************/
#pragma once
#include "NativeClassChainAccess.h"
#include "NativeStartupProvidersProfile.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    // Caller qualifies the native frame, image/profile and world. This reader
    // does not acquire UObject/GC lifetime or permission to create a provider.
    class NativeStartupProvidersAccess final
    {
    public:
        using ReadMemory = NativeClassChainAccess::ReadMemory;
        struct Value
        {
            std::uintptr_t controllerAddress = 0, viewportAddress = 0, managerAddress = 0;
        };
        NativeStartupProvidersAccess(std::uintptr_t imageBase, ReadMemory guardedRead) noexcept : image(imageBase), read(guardedRead) {}

        // Require positive observations of both typed links and an already
        // cached manager. Native absence can be legitimate; refusal here means
        // this conservative diagnostic cannot certify all three prerequisites.
        // No initializer is invoked and every refusal preserves output.
        bool Read(std::uintptr_t worldAddress, Value& output) const noexcept
        {
            Path first{}, second{};
            if (!Gather(worldAddress, first) || !Fence(first) || !Gather(worldAddress, second) || !Fence(second) || !Equal(first, second) ||
                second.controller != Controller::Present || second.viewport != Viewport::CachedManager)
                return false;
            output = second.value;
            return true;
        }

    private:
        enum class Controller
        {
            NoEntries,
            InvalidWeak,
            IndexOutOfRange,
            StaleSerial,
            RejectedFlags,
            NullObject,
            WrongType,
            Present
        };
        enum class Viewport
        {
            Missing,
            WrongType,
            NeedsInitialization,
            CachedManager
        };
        // The path is fixed-depth; this is a diagnostic evidence budget, not a
        // guessed native array/registry capacity. Every consumed field is fenced.
        static constexpr std::size_t maximumTokens = 64, maximumBytes = 12;
        using Header = std::array<unsigned char, 12>;
        struct Token
        {
            std::uintptr_t                          address = 0;
            std::size_t                             size = 0;
            std::array<unsigned char, maximumBytes> bytes{};
        };
        struct Path
        {
            Value                            value{};
            Controller                       controller = Controller::NoEntries;
            Viewport                         viewport = Viewport::Missing;
            std::array<Token, maximumTokens> tokens{};
            std::size_t                      count = 0;
        };
        struct Type
        {
            Header         candidate{}, base{};
            std::uintptr_t selected = 0;
            bool           selectedRead = false;
        };
        static_assert(sizeof(std::uintptr_t) == 8);
        static_assert(NativeStartupProvidersProfile::worldControllerDataOffset + sizeof(std::uintptr_t) ==
                      NativeStartupProvidersProfile::worldControllerCountOffset);
        static_assert(NativeStartupProvidersProfile::classChainOffset + sizeof(std::uintptr_t) == NativeStartupProvidersProfile::classChainIndexOffset);
        static_assert(NativeStartupProvidersProfile::weakRecordBytes == 8 && NativeStartupProvidersProfile::chunkIndexShift == 16 &&
                      NativeStartupProvidersProfile::chunkEntryBytes == sizeof(std::uintptr_t));
        static bool Add(std::uintptr_t base, std::size_t offset, std::uintptr_t& output) noexcept
        {
            if (!base || offset > (std::numeric_limits<std::uintptr_t>::max)() - base)
                return false;
            output = base + offset;
            return true;
        }
        bool Copy(std::uintptr_t address, void* output, std::size_t size) const noexcept
        {
            return read && address && size && size <= (std::numeric_limits<std::uintptr_t>::max)() - address && read(address, output, size);
        }
        template <typename T>
        bool Record(Path& path, std::uintptr_t address, T& output) const noexcept
        {
            static_assert(sizeof(T) <= maximumBytes);
            if (path.count == maximumTokens)
                return false;
            Token next{};
            next.address = address;
            next.size = sizeof(T);
            if (!Copy(address, next.bytes.data(), next.size))
                return false;
            std::memcpy(&output, next.bytes.data(), sizeof(output));
            path.tokens[path.count++] = next;
            return true;
        }
        template <typename T>
        bool Field(Path& path, std::uintptr_t base, std::size_t offset, T& output) const noexcept
        {
            std::uintptr_t address = 0;
            return Add(base, offset, address) && Record(path, address, output);
        }
        template <typename T, std::size_t N>
        static T Decode(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            if (offset <= bytes.size() && sizeof(T) <= bytes.size() - offset)
                std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        bool CaptureType(Path& path, std::uintptr_t candidate, std::uintptr_t base, Type& output) const noexcept
        {
            namespace P = NativeStartupProvidersProfile;
            Type next{};
            if (!Field(path, candidate, P::classChainOffset, next.candidate) || !Field(path, base, P::classChainOffset, next.base))
                return false;
            const auto candidateIndex = Decode<std::int32_t>(next.candidate, 8), baseIndex = Decode<std::int32_t>(next.base, 8);
            if (candidateIndex < 0 || candidateIndex > NativeClassChainAccess::maximumIndex || baseIndex < 0 ||
                baseIndex > NativeClassChainAccess::maximumIndex)
                return false;
            if (baseIndex <= candidateIndex)
            {
                if (!Field(path, Decode<std::uintptr_t>(next.candidate, 0), static_cast<std::size_t>(baseIndex) * 8, next.selected))
                    return false;
                next.selectedRead = true;
            }
            output = next;
            return true;
        }
        bool TypeMatches(Path& path, std::uintptr_t object, std::uintptr_t cacheRva, bool& output) const noexcept
        {
            std::uintptr_t base = 0, candidate = 0;
            if (!Field(path, image, cacheRva, base) || !base || !Field(path, object, NativeStartupProvidersProfile::objectClassOffset, candidate))
                return false;
            Type before{}, after{};
            if (!CaptureType(path, candidate, base, before) || !NativeClassChainAccess(read).IsChildOf(candidate, base, output) ||
                !CaptureType(path, candidate, base, after))
                return false;
            return before.candidate == after.candidate && before.base == after.base && before.selected == after.selected &&
                   before.selectedRead == after.selectedRead;
        }
        bool ControllerPath(std::uintptr_t world, Path& path) const noexcept
        {
            namespace P = NativeStartupProvidersProfile;
            Header header{};
            if (!Field(path, world, P::worldControllerDataOffset, header))
                return false;
            const auto count = Decode<std::int32_t>(header, 8);
            // Native tests signed <=0 and consumes no first element then.
            // No capacity or extra world/TArray field is inferred here.
            if (count <= 0)
                return true;
            std::array<unsigned char, P::weakRecordBytes> weak{};
            if (!Record(path, Decode<std::uintptr_t>(header, 0), weak))
                return false;
            const auto index = Decode<std::int32_t>(weak, P::weakIndexOffset);
            const auto serial = Decode<std::uint32_t>(weak, P::weakSerialOffset);
            path.controller = Controller::InvalidWeak;
            if (!serial || index < 0)
                return true;
            std::int32_t objectCount = 0;
            if (!Field(path, image, P::objectCountRva, objectCount))
                return false;
            path.controller = Controller::IndexOutOfRange;
            if (index >= objectCount)
                return true;
            std::uintptr_t table = 0, chunk = 0, item = 0;
            const auto     nonnegative = static_cast<std::uint32_t>(index);
            if (!Field(path, image, P::objectChunksRva, table) ||
                !Field(path, table, static_cast<std::size_t>(nonnegative >> P::chunkIndexShift) * P::chunkEntryBytes, chunk) ||
                !Add(chunk, static_cast<std::size_t>(nonnegative & 0xffffu) * P::objectItemBytes, item))
                return false;
            std::uint32_t itemSerial = 0, flags = 0;
            if (!Field(path, item, P::itemSerialOffset, itemSerial))
                return false;
            path.controller = Controller::StaleSerial;
            if (itemSerial != serial)
                return true;
            if (!Field(path, item, P::itemFlagsOffset, flags))
                return false;
            path.controller = Controller::RejectedFlags;
            if (flags & P::itemRejectMask)
                return true;
            std::uintptr_t object = 0;
            if (!Record(path, item, object))
                return false;
            path.controller = Controller::NullObject;
            if (!object)
                return true;
            bool matches = false;
            if (!TypeMatches(path, object, P::playerControllerClassCacheRva, matches))
                return false;
            path.controller = matches ? Controller::Present : Controller::WrongType;
            if (matches)
                path.value.controllerAddress = object;
            return true;
        }
        bool ViewportPath(Path& path) const noexcept
        {
            namespace P = NativeStartupProvidersProfile;
            std::uintptr_t engine = 0, viewport = 0;
            if (!Field(path, image, P::engineRva, engine) || !Field(path, engine, P::engineViewportOffset, viewport))
                return false;
            if (!viewport)
                return true;
            bool matches = false;
            if (!TypeMatches(path, viewport, P::viewportClassCacheRva, matches))
                return false;
            path.viewport = Viewport::WrongType;
            if (!matches)
                return true;
            path.value.viewportAddress = viewport;
            if (!Field(path, viewport, P::viewportManagerOffset, path.value.managerAddress))
                return false;
            // Native invokes a mutating lazy initializer for a null manager.
            // Requiring an existing cache leaves that obligation entirely alone.
            path.viewport = path.value.managerAddress ? Viewport::CachedManager : Viewport::NeedsInitialization;
            return true;
        }
        bool Gather(std::uintptr_t world, Path& path) const noexcept { return ControllerPath(world, path) && ViewportPath(path); }
        bool Fence(const Path& path) const noexcept
        {
            for (std::size_t index = 0; index < path.count; ++index)
            {
                const auto&                             before = path.tokens[index];
                std::array<unsigned char, maximumBytes> after{};
                if (!Copy(before.address, after.data(), before.size) || after != before.bytes)
                    return false;
            }
            return true;
        }
        static bool Equal(const Path& a, const Path& b) noexcept
        {
            if (a.value.controllerAddress != b.value.controllerAddress || a.value.viewportAddress != b.value.viewportAddress ||
                a.value.managerAddress != b.value.managerAddress || a.controller != b.controller || a.viewport != b.viewport || a.count != b.count)
                return false;
            for (std::size_t index = 0; index < a.count; ++index)
            {
                const auto& x = a.tokens[index];
                const auto& y = b.tokens[index];
                if (x.address != y.address || x.size != y.size || x.bytes != y.bytes)
                    return false;
            }
            return true;
        }
        const std::uintptr_t image;
        const ReadMemory     read;
    };
}
