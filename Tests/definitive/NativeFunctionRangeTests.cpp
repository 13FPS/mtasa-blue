/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise bounded unwind families without native hook ownership
 *****************************************************************************/
#include "../../Client/game_de/NativeFunctionRange.h"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using mta::game_de::NativeFunctionRange;
    using mta::game_de::NativeFunctionRangeDetail::Entry;
    unsigned checks = 0;

    void Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }

    struct Fixture
    {
        std::array<unsigned char, 4096> bytes{};
        std::array<Entry, 3>            entries{{{0x400, 0x430, 0x300}, {0x500, 0x530, 0x340}, {0x700, 0x740, 0x320}}};
        Fixture()
        {
            IMAGE_DOS_HEADER dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x80;
            Write(0, dos);
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.SizeOfOptionalHeader = static_cast<WORD>(sizeof(IMAGE_OPTIONAL_HEADER64));
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(bytes.size());
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION] = {0x200, static_cast<DWORD>(sizeof(entries))};
            Write(0x80, nt);
            Write(0x200, entries);
            bytes[0x300] = bytes[0x340] = 1;
            bytes[0x320] = 1 | (UNW_FLAG_CHAININFO << 3);
            bytes[0x322] = 3;
            // Three codes occupy six bytes, padded to eight before the chain.
            Write(0x32c, entries[0]);
        }
        template <class Value>
        void Write(std::size_t offset, const Value& value)
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }
        bool Decode(const Entry& entry, NativeFunctionRange& output)
        {
            return mta::game_de::NativeFunctionRangeDetail::Decode(bytes.data(), bytes.size(), entry.begin, entry, output);
        }
        void Refuses(const char* description)
        {
            NativeFunctionRange output{11, 22};
            Check(!Decode(entries[0], output) && output.begin == 11 && output.end == 22, description);
        }
    };

    __declspec(noinline) DWORD ActualUnwindFunction(DWORD seed)
    {
        volatile DWORD values[32]{};
        values[seed % 32] = GetCurrentProcessId();
        return values[seed % 32] + GetCurrentThreadId();
    }
}

int main()
{
    static_assert(sizeof(void*) == 8 && sizeof(Entry) == sizeof(RUNTIME_FUNCTION));
    NativeFunctionRange empty;
    Check(!empty.Contains(0) && !empty.Contains(1), "empty ranges contain no instruction address");
    Fixture             image;
    NativeFunctionRange range{11, 22};
    Check(image.Decode(image.entries[0], range), "root entry resolves its complete chained family");
    const auto base = reinterpret_cast<std::uintptr_t>(image.bytes.data());
    Check(range.begin == base + 0x400 && range.end == base + 0x740, "cold fragment extends the conservative root interval");
    Check(range.Contains(base + 0x400) && range.Contains(base + 0x500) && range.Contains(base + 0x73f), "cached range includes entry, gaps and cold fragment");
    Check(!range.Contains(base + 0x3ff) && !range.Contains(base + 0x740), "range boundaries are half open");
    Check(image.Decode(image.entries[2], range) && range.begin == base + 0x400 && range.end == base + 0x740,
          "a cold lookup resolves the same root despite aligned odd unwind-code count");
    Check(image.Decode(image.entries[1], range) && range.begin == base + 0x500 && range.end == base + 0x530, "an unrelated function has its own root identity");
    Fixture cycle;
    cycle.bytes[0x320] = 1 | (UNW_FLAG_CHAININFO << 3);
    cycle.Write(0x32c, cycle.entries[2]);
    cycle.Refuses("a self-referential chain refuses without changing caller output");
    Fixture excessiveChain;
    excessiveChain.entries[2].unwind = 0x800;
    excessiveChain.Write(0x200, excessiveChain.entries);
    for (unsigned index = 0; index < 32; ++index)
    {
        const auto offset = 0x800 + index * 32;
        excessiveChain.bytes[offset] = 1 | (UNW_FLAG_CHAININFO << 3);
        const Entry next = index == 31 ? excessiveChain.entries[0] : Entry{0x700, 0x740, offset + 32};
        excessiveChain.Write(offset + 4, next);
    }
    excessiveChain.Refuses("an acyclic chain beyond the fixed traversal budget refuses");
    Fixture badVersion;
    badVersion.bytes[0x320] = 2 | (UNW_FLAG_CHAININFO << 3);
    badVersion.Refuses("unsupported unwind versions cannot silently omit a fragment");
    Fixture mixedFlags;
    mixedFlags.bytes[0x320] |= UNW_FLAG_EHANDLER << 3;
    mixedFlags.Refuses("CHAININFO cannot be mixed with handler flags");
    Fixture unknownFlags;
    unknownFlags.bytes[0x320] |= 8 << 3;
    unknownFlags.Refuses("unknown unwind flags refuse the family");
    Fixture outside;
    outside.entries[2].end = 4097;
    outside.Write(0x200, outside.entries);
    outside.Refuses("out-of-image function ranges cannot become detach authority");
    Fixture truncated;
    truncated.entries[2].unwind = 4092;
    truncated.Write(0x200, truncated.entries);
    truncated.bytes[4092] = 1 | (UNW_FLAG_CHAININFO << 3);
    truncated.Refuses("a chain outside the image refuses");
    Fixture unsorted;
    unsorted.entries[1].begin = 0x410;
    unsorted.Write(0x200, unsorted.entries);
    unsorted.Refuses("overlapping table entries are rejected");
    Fixture badHeader;
    badHeader.bytes[0] = 0;
    badHeader.Refuses("invalid PE identity is rejected");
    std::vector<unsigned char> excessiveTable(4 * 1024 * 1024);
    std::memcpy(excessiveTable.data(), image.bytes.data(), image.bytes.size());
    IMAGE_NT_HEADERS64 oversized{};
    std::memcpy(&oversized, excessiveTable.data() + 0x80, sizeof(oversized));
    oversized.OptionalHeader.SizeOfImage = static_cast<DWORD>(excessiveTable.size());
    oversized.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION] = {0x1000, static_cast<DWORD>(262145 * sizeof(Entry))};
    std::memcpy(excessiveTable.data() + 0x80, &oversized, sizeof(oversized));
    range = {11, 22};
    Check(!mta::game_de::NativeFunctionRangeDetail::Decode(excessiveTable.data(), excessiveTable.size(), 0x400, image.entries[0], range) && range.begin == 11 &&
              range.end == 22,
          "a valid image range cannot exceed the fixed pdata entry budget");
    Check(!mta::game_de::NativeFunctionRangeDetail::Decode(reinterpret_cast<const void*>((std::numeric_limits<std::uintptr_t>::max)() - 15), 4096, 0x400,
                                                           image.entries[0], range),
          "image-base overflow is rejected before memory access");
    Check(!mta::game_de::NativeFunctionRangeDetail::Decode(image.bytes.data(), 1, 0x400, image.entries[0], range),
          "a truncated mapped extent cannot expose a DOS header");
    Fixture falseLookup;
    Entry   absent{0x450, 0x460, 0x300};
    range = {11, 22};
    Check(!falseLookup.Decode(absent, range) && range.begin == 11 && range.end == 22, "lookup metadata must match an actual pdata entry");
    range = {11, 22};
    Check(!mta::game_de::FindNativeFunctionRange(nullptr, range) && range.begin == 11 && range.end == 22, "null lookup preserves output");
    Check(!mta::game_de::FindNativeFunctionRange(reinterpret_cast<const void*>(1), range), "unmapped addresses are not module-owned functions");
    Check(!mta::game_de::FindNativeFunctionRange(image.bytes.data(), range), "an owned synthetic image is not a registered native module");
    const auto  function = reinterpret_cast<const void*>(&ActualUnwindFunction);
    DWORD64     lookupBase = 0;
    const auto* entry = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(function), &lookupBase, nullptr);
    Check(entry != nullptr, "the compiled fixture supplies actual AMD64 unwind metadata");
    Check(mta::game_de::FindNativeFunctionRange(function, range), "production lookup accepts the compiled function's module and pdata");
    Check(range.Contains(reinterpret_cast<std::uintptr_t>(function)) && range.begin <= lookupBase + entry->BeginAddress &&
              range.end >= lookupBase + entry->EndAddress,
          "the actual containing unwind entry is inside its cached family");
    std::printf("Passed %u native unwind function-family checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
