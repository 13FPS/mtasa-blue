/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve bounded chained-unwind families before hook quiescence
 *****************************************************************************/
#include "NativeFunctionRange.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace mta::game_de
{
    namespace
    {
        using Entry = NativeFunctionRangeDetail::Entry;
        constexpr std::size_t MaximumEntries = 262144, MaximumChain = 32;

        bool Copy(std::uintptr_t address, void* output, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, bytes, &copied) && copied == bytes;
        }

        struct Image
        {
            std::uintptr_t base;
            std::size_t    bytes;
            bool           Range(std::size_t rva, std::size_t count) const noexcept { return count && rva < bytes && count <= bytes - rva; }
            template <class Value>
            bool Read(std::size_t rva, Value& output) const noexcept
            {
                return Range(rva, sizeof(Value)) && Copy(base + rva, &output, sizeof(Value));
            }
            bool Valid(const Entry& entry) const noexcept
            {
                return entry.begin < entry.end && entry.end <= bytes && !(entry.unwind & 3) && Range(entry.unwind, 4);
            }
        };

        bool Same(const Entry& first, const Entry& second) noexcept
        {
            return first.begin == second.begin && first.end == second.end && first.unwind == second.unwind;
        }

        bool Headers(std::uintptr_t base, std::size_t mappedBytes, IMAGE_NT_HEADERS64& nt) noexcept
        {
            IMAGE_DOS_HEADER dos{};
            if ((mappedBytes && mappedBytes < sizeof(dos)) || !Copy(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                dos.e_lfanew > 1048576)
                return false;
            const auto offset = static_cast<std::size_t>(dos.e_lfanew);
            if ((mappedBytes && (offset >= mappedBytes || sizeof(nt) > mappedBytes - offset)) || offset > (std::numeric_limits<std::uintptr_t>::max)() - base ||
                !Copy(base + offset, &nt, sizeof(nt)))
                return false;
            return nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
                   nt.FileHeader.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER64) && nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
                   nt.OptionalHeader.SizeOfImage >= offset + sizeof(nt) && (!mappedBytes || nt.OptionalHeader.SizeOfImage == mappedBytes) &&
                   nt.FileHeader.SizeOfOptionalHeader <= nt.OptionalHeader.SizeOfImage - offset - offsetof(IMAGE_NT_HEADERS64, OptionalHeader) &&
                   nt.OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXCEPTION;
        }

        bool Root(const Image& image, Entry entry, Entry& output) noexcept
        {
            std::array<Entry, MaximumChain> visited{};
            for (std::size_t depth = 0; depth < visited.size(); ++depth)
            {
                if (!image.Valid(entry))
                    return false;
                for (std::size_t index = 0; index < depth; ++index)
                    if (Same(visited[index], entry))
                        return false;
                visited[depth] = entry;
                std::array<unsigned char, 4> header{};
                if (!image.Read(entry.unwind, header) || (header[0] & 7) != 1)
                    return false;
                const unsigned flags = header[0] >> 3;
                if ((flags & ~7u) || ((flags & 4u) && (flags & 3u)))
                    return false;
                const std::size_t alignedCodes = (static_cast<std::size_t>(header[2]) * 2 + 3) & ~std::size_t{3};
                if (!image.Range(entry.unwind, 4 + alignedCodes))
                    return false;
                const auto tail = static_cast<std::size_t>(entry.unwind) + 4 + alignedCodes;
                if (!(flags & 4u))
                {
                    std::uint32_t handler = 0;
                    if ((flags & 3u) && (!image.Read(tail, handler) || !image.Range(handler, 1)))
                        return false;
                    output = entry;
                    return true;
                }
                if (!image.Read(tail, entry))
                    return false;
            }
            return false;
        }
    }

    bool NativeFunctionRangeDetail::Decode(const void* address, std::size_t mappedBytes, std::uint32_t functionRva, const Entry& containing,
                                           NativeFunctionRange& output) noexcept
    {
        try
        {
            const auto         base = reinterpret_cast<std::uintptr_t>(address);
            IMAGE_NT_HEADERS64 nt{};
            if (!base || !mappedBytes || mappedBytes > (std::numeric_limits<std::uintptr_t>::max)() - base || !Headers(base, mappedBytes, nt))
                return false;
            const Image image{base, mappedBytes};
            const auto  directory = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
            static_assert(sizeof(Entry) == sizeof(RUNTIME_FUNCTION));
            if (!directory.VirtualAddress || directory.VirtualAddress % 4 || directory.Size % sizeof(Entry) ||
                !image.Range(directory.VirtualAddress, directory.Size))
                return false;
            const auto count = directory.Size / sizeof(Entry);
            if (!count || count > MaximumEntries || !image.Valid(containing) || functionRva < containing.begin || functionRva >= containing.end)
                return false;
            std::vector<Entry> entries(count);
            if (!Copy(base + directory.VirtualAddress, entries.data(), directory.Size))
                return false;
            Entry target{};
            if (!Root(image, containing, target))
                return false;
            bool          found = false;
            std::uint32_t low = containing.begin, high = containing.end, previousEnd = 0;
            for (const auto& entry : entries)
            {
                Entry root{};
                if (!image.Valid(entry) || entry.begin < previousEnd || !Root(image, entry, root))
                    return false;
                previousEnd = entry.end;
                found = found || Same(entry, containing);
                // Chained hot/cold fragments share root identity. A bounding
                // interval intentionally includes any gaps: false busy is safer
                // than detaching a trampoline while a peer can still enter it.
                if (root.begin == target.begin && root.unwind == target.unwind)
                {
                    low = (std::min)(low, (std::min)(entry.begin, root.begin));
                    high = (std::max)(high, (std::max)(entry.end, root.end));
                }
            }
            if (!found || low >= high)
                return false;
            output = {base + low, base + high};
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool FindNativeFunctionRange(const void* function, NativeFunctionRange& output) noexcept
    {
        if (!function)
            return false;
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(function),
                                &module))
            return false;
        const auto         base = reinterpret_cast<std::uintptr_t>(module), pc = reinterpret_cast<std::uintptr_t>(function);
        IMAGE_NT_HEADERS64 nt{};
        if (!Headers(base, 0, nt) || pc < base || pc - base >= nt.OptionalHeader.SizeOfImage)
            return false;
        DWORD64          lookupBase = 0;
        const auto*      lookup = RtlLookupFunctionEntry(static_cast<DWORD64>(pc), &lookupBase, nullptr);
        RUNTIME_FUNCTION native{};
        if (!lookup || lookupBase != base || !Copy(reinterpret_cast<std::uintptr_t>(lookup), &native, sizeof(native)))
            return false;
        const auto  directory = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        const auto  lookupAddress = reinterpret_cast<std::uintptr_t>(lookup);
        const Image image{base, nt.OptionalHeader.SizeOfImage};
        if (!image.Range(directory.VirtualAddress, directory.Size) || lookupAddress < base || lookupAddress - base < directory.VirtualAddress ||
            lookupAddress - base - directory.VirtualAddress >= directory.Size || (lookupAddress - base - directory.VirtualAddress) % sizeof(RUNTIME_FUNCTION))
            return false;
        const NativeFunctionRangeDetail::Entry containing{native.BeginAddress, native.EndAddress, native.UnwindData};
        return NativeFunctionRangeDetail::Decode(module, nt.OptionalHeader.SizeOfImage, static_cast<std::uint32_t>(pc - base), containing, output);
    }
}
