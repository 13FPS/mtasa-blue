/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real native state reader over a small task-owned SEC_IMAGE
 *****************************************************************************/
#include "../../Client/game_de/NativeGameStateReader.h"
#include "../../Client/game_de/NativeGameStateProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>
#include <vector>

namespace GameStateTestHost
{
    std::uintptr_t    image = 0;
    std::atomic<bool> failStateRead{false};
    std::atomic<bool> transitionAfterRead{false};
    std::atomic<bool> removeWriteBeforeAtomic{false};
    DWORD             atomicOldProtection = 0;

    HMODULE WINAPI Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }

    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (image && reinterpret_cast<std::uintptr_t>(source) == image + mta::game_de::NativeGameStateProfile::stateRva && bytes == sizeof(std::uint32_t) &&
            failStateRead.exchange(false))
        {
            // Fail after real page admission and poison local staging. A partial
            // OS copy must never publish a made-up SDK state to the caller.
            std::memset(output, 0xa7, bytes);
            if (copied)
                *copied = bytes / 2;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, source, output, bytes, copied);
        if (result && image && reinterpret_cast<std::uintptr_t>(source) == image + mta::game_de::NativeGameStateProfile::stateRva &&
            bytes == sizeof(std::uint32_t) && transitionAfterRead.exchange(false))
        {
            // Change the real owned image between two successful OS reads.
            // A frontend/loading transition must not publish mixed samples.
            constexpr std::uint32_t next = 7;
            SIZE_T                  written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeGameStateProfile::stateRva), &next, sizeof(next),
                                      &written) ||
                written != sizeof(next))
                std::abort();
        }
        return result;
    }

    LONG Atomic(volatile LONG* destination, LONG value, LONG expected)
    {
        if (removeWriteBeforeAtomic.exchange(false) && !VirtualProtect(const_cast<LONG*>(destination), sizeof(LONG), PAGE_READONLY, &atomicOldProtection))
            std::abort();
        // Use the real aligned atomic instruction after changing the owned
        // page. Its access violation must be caught by production's SEH guard.
        return InterlockedCompareExchange(destination, value, expected);
    }
}

#define ReadProcessMemory GameStateTestHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#undef ReadProcessMemory
namespace mta::game_de::NativeImageAccess
{
    bool GameStateFixtureExecutable() noexcept
    {
        // The selected owned image exists only in this test translation unit.
        // With no selection, the shipping executable hash gate stays active.
        return GameStateTestHost::image != 0 || CheckExecutableBuild();
    }
}
#define CheckExecutableBuild GameStateFixtureExecutable
#define GetModuleHandleW     GameStateTestHost::Module
#pragma push_macro("InterlockedCompareExchange")
#undef InterlockedCompareExchange
#define InterlockedCompareExchange GameStateTestHost::Atomic
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#pragma pop_macro("InterlockedCompareExchange")
#include "../../Client/game_de/NativeGameStateReader.cpp"
#undef GetModuleHandleW
#undef CheckExecutableBuild

using namespace mta::game_de;
namespace P = NativeGameStateProfile;
namespace
{
    unsigned       checks = 0;
    constexpr auto canary = SystemState::GS_INIT_INTRO_MPEG;

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL after %u native game-state checks: %s\n", checks, why);
            std::exit(1);
        }
    }

    class OwnedImage
    {
    public:
        std::uintptr_t image = 0;
        HANDLE         file = INVALID_HANDLE_VALUE, mapping = nullptr;

        ~OwnedImage()
        {
            GameStateTestHost::image = 0;
            if (image)
                UnmapViewOfFile(reinterpret_cast<void*>(image));
            if (mapping)
                CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }

        void Patch(std::uintptr_t rva, const void* source, std::size_t size)
        {
            DWORD old = 0, ignored = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &old) != FALSE, "temporarily write only task-owned fixture bytes");
            std::memcpy(reinterpret_cast<void*>(image + rva), source, size);
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, old, &ignored) != FALSE, "restore actual owned image-page protection");
        }

        void State(std::uint32_t value)
        {
            SIZE_T copied = 0;
            Check(::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + P::stateRva), &value, sizeof(value), &copied) &&
                      copied == sizeof(value),
                  "seed only the owned native state DWORD");
        }

        void Create(const std::filesystem::path& directory)
        {
            const auto path = directory / L"owned-game-state-image.bin";
            Check(!std::filesystem::exists(path), "preserve earlier evidence by creating the owned image once");
            struct Page
            {
                std::uint32_t rva;
                DWORD         flags;
            };
            std::vector<Page> pages;
            constexpr DWORD   code = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            constexpr DWORD   data = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            for (const auto& pin : P::pins)
                for (auto page = pin.rva & ~std::uintptr_t{0xfff}; page < pin.rva + pin.size; page += 0x1000)
                    pages.push_back({static_cast<std::uint32_t>(page), pin.code ? code : data});
            pages.push_back({static_cast<std::uint32_t>(P::stateRva & ~std::uintptr_t{0xfff}), data | IMAGE_SCN_MEM_WRITE});
            std::sort(pages.begin(), pages.end(), [](const Page& a, const Page& b) { return a.rva < b.rva; });
            pages.erase(std::unique(pages.begin(), pages.end(), [](const Page& a, const Page& b) { return a.rva == b.rva; }), pages.end());

            std::vector<IMAGE_SECTION_HEADER> sections;
            std::uint32_t                     nextRva = 0x1000, rawOffset = 0x1000;
            const auto                        section = [&](std::uint32_t rva, std::uint32_t size, DWORD flags, bool raw)
            {
                IMAGE_SECTION_HEADER value{};
                std::memcpy(value.Name, raw ? ".pins" : ".gap", raw ? 5 : 4);
                value.VirtualAddress = rva;
                value.Misc.VirtualSize = size;
                value.SizeOfRawData = raw ? size : 0;
                value.PointerToRawData = raw ? rawOffset : 0;
                value.Characteristics = flags;
                sections.push_back(value);
                if (raw)
                    rawOffset += size;
            };
            // Adjacent virtual sections satisfy the actual Windows loader;
            // only the handful of native pin/data pages consume disk bytes.
            for (std::size_t index = 0; index < pages.size();)
            {
                const auto  begin = pages[index];
                std::size_t end = index + 1;
                while (end < pages.size() && pages[end].rva == pages[end - 1].rva + 0x1000 && pages[end].flags == begin.flags)
                    ++end;
                if (nextRva < begin.rva)
                    section(nextRva, begin.rva - nextRva, IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ, false);
                const auto size = pages[end - 1].rva + 0x1000 - begin.rva;
                section(begin.rva, size, begin.flags, true);
                nextRva = begin.rva + size;
                index = end;
            }
            constexpr std::uint32_t mappedSize = 0x5ca5000;
            section(nextRva, mappedSize - nextRva, IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ, false);
            Check(sections.size() < 32, "the compact native profile needs only a bounded sparse section table");
            std::vector<unsigned char> bytes(rawOffset, 0);
            IMAGE_DOS_HEADER           dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = static_cast<WORD>(sections.size());
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.ImageBase = UINT64_C(0x180000000);
            nt.OptionalHeader.SectionAlignment = 0x1000;
            nt.OptionalHeader.FileAlignment = 0x200;
            nt.OptionalHeader.SizeOfHeaders = 0x1000;
            nt.OptionalHeader.SizeOfImage = mappedSize;
            nt.OptionalHeader.BaseOfCode = pages.front().rva;
            for (const auto& value : sections)
            {
                if (value.Characteristics & IMAGE_SCN_CNT_CODE)
                    nt.OptionalHeader.SizeOfCode += value.SizeOfRawData;
                if (value.Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA)
                    nt.OptionalHeader.SizeOfInitializedData += value.SizeOfRawData;
            }
            nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
            nt.OptionalHeader.MajorOperatingSystemVersion = nt.OptionalHeader.MajorSubsystemVersion = 6;
            nt.OptionalHeader.SizeOfStackReserve = nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
            nt.OptionalHeader.SizeOfStackCommit = nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            std::memcpy(bytes.data() + dos.e_lfanew, &nt, sizeof(nt));
            std::memcpy(bytes.data() + dos.e_lfanew + sizeof(nt), sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER));
            for (const auto& pin : P::pins)
            {
                bool seeded = false;
                for (const auto& value : sections)
                    if (value.PointerToRawData && pin.rva >= value.VirtualAddress && pin.rva + pin.size <= value.VirtualAddress + value.SizeOfRawData)
                    {
                        std::memcpy(bytes.data() + value.PointerToRawData + pin.rva - value.VirtualAddress, pin.bytes, pin.size);
                        seeded = true;
                        break;
                    }
                Check(seeded, "every required production pin fits its corresponding owned raw image section");
            }
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save the bounded sparse image without copying a game executable");
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "open only the owned native-state image");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            Check(mapping != nullptr, "create real SEC_IMAGE page identity for production validation");
            image = reinterpret_cast<std::uintptr_t>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != 0, "map the sparse image using the Windows loader");
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::imageSize);
            Patch(static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt));
        }
    };
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    NativeGameStateReader reader;
    auto                  output = canary;
    Check(!reader.IsBound() && reader.Read(output) == Status::NotInitialized && output == canary, "unbound reads preserve the SDK output");
    Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "the unchanged shipping executable hash gate rejects this test host");
    OwnedImage fixture;
    fixture.Create(argv[1]);
    GameStateTestHost::image = fixture.image;
    fixture.State(0);
    Check(reader.Bind() == Status::Ok && reader.IsBound(), "Bind qualifies image and pins without claiming native startup state zero has an SDK meaning");
    constexpr std::array<SystemState::Enum, 4> mapped{SystemState::GS_INIT_FRONTEND, SystemState::GS_FRONTEND, SystemState::GS_INIT_PLAYING_GAME,
                                                      SystemState::GS_PLAYING_GAME};
    for (std::uint32_t state = 0; state != 10; ++state)
    {
        auto          semantic = canary;
        std::uint32_t encoded = 0xfacecafe;
        Check(state < 6 ? !NativeGameStateMemory::Decode(state, semantic) && semantic == canary
                        : NativeGameStateMemory::Decode(state, semantic) && semantic == mapped[state - 6],
              "the shared decoder publishes only independently proved state meanings");
        Check(state < 6 ? !NativeGameStateMemory::Encode(static_cast<SystemState::Enum>(state), encoded) && encoded == 0xfacecafe
                        : NativeGameStateMemory::Encode(mapped[state - 6], encoded) && encoded == state,
              "the explicit SDK inverse preserves output for every unproved early state");
    }
    for (const auto invalid : {10u, 0x80000000u, (std::numeric_limits<std::uint32_t>::max)()})
    {
        auto          semantic = canary;
        std::uint32_t encoded = 0xfacecafe;
        Check(!NativeGameStateMemory::Decode(invalid, semantic) && semantic == canary &&
                  !NativeGameStateMemory::Encode(static_cast<SystemState::Enum>(invalid), encoded) && encoded == 0xfacecafe,
              "out-of-range SDK/native inputs preserve codec outputs");
    }
    // This owned SEC_IMAGE models only DWORD storage, not native startup. The
    // surrounding canaries detect a write widened to adjacent native globals.
    constexpr std::array<unsigned char, 4> before = {0x19, 0x63, 0xa7, 0xe1}, after = {0x21, 0x75, 0xb9, 0xf3};
    fixture.Patch(P::stateRva - before.size(), before.data(), before.size());
    fixture.Patch(P::stateRva + sizeof(std::uint32_t), after.data(), after.size());
    for (std::uint32_t expected = 6; expected != 10; ++expected)
        for (std::uint32_t target = 6; target != 10; ++target)
        {
            fixture.State(expected);
            std::uint32_t observed = 0;
            Check(NativeGameStateMemory::Read(fixture.image, observed) && observed == expected, "the shared raw reader uses the actual qualified native DWORD");
            Check(NativeGameStateMemory::CompareExchange(fixture.image, expected, target) == NativeGameStateMemory::StoreResult::Applied &&
                      NativeGameStateMemory::Read(fixture.image, observed) && observed == target &&
                      std::memcmp(reinterpret_cast<const void*>(fixture.image + P::stateRva - before.size()), before.data(), before.size()) == 0 &&
                      std::memcmp(reinterpret_cast<const void*>(fixture.image + P::stateRva + sizeof(std::uint32_t)), after.data(), after.size()) == 0,
                  "each supported pair atomically replaces exactly four bytes without touching adjacent native storage");
        }
    fixture.State(9);
    Check(NativeGameStateMemory::CompareExchange(fixture.image, 7, 8) == NativeGameStateMemory::StoreResult::ValueChanged,
          "a native advance after the caller's snapshot is preserved instead of overwritten");
    std::uint32_t native = 0;
    Check(NativeGameStateMemory::Read(fixture.image, native) && native == 9, "a rejected comparison leaves the new native state intact");
    for (const auto invalid : {0u, 5u, 10u, (std::numeric_limits<std::uint32_t>::max)()})
    {
        Check(NativeGameStateMemory::CompareExchange(fixture.image, invalid, 8) == NativeGameStateMemory::StoreResult::Unsupported &&
                  NativeGameStateMemory::CompareExchange(fixture.image, 9, invalid) == NativeGameStateMemory::StoreResult::Unsupported &&
                  NativeGameStateMemory::Read(fixture.image, native) && native == 9,
              "invalid expected or desired states cannot trigger a write");
    }
    native = 0xfacecafe;
    Check(!NativeGameStateMemory::Read(0, native) && native == 0xfacecafe &&
              NativeGameStateMemory::CompareExchange(0, 7, 8) == NativeGameStateMemory::StoreResult::Unsupported &&
              NativeGameStateMemory::CompareExchange((std::numeric_limits<std::uintptr_t>::max)(), 7, 8) == NativeGameStateMemory::StoreResult::Unsupported,
          "missing and overflowing images refuse before scalar access and preserve outputs");
    fixture.State(7);
    GameStateTestHost::removeWriteBeforeAtomic.store(true);
    Check(NativeGameStateMemory::CompareExchange(fixture.image, 7, 8) == NativeGameStateMemory::StoreResult::Unsupported &&
              !GameStateTestHost::removeWriteBeforeAtomic.load(),
          "a protection change after admission refuses inside the actual atomic write guard");
    DWORD ignoredAtomic = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + P::stateRva), sizeof(std::uint32_t), GameStateTestHost::atomicOldProtection, &ignoredAtomic) !=
                  FALSE &&
              NativeGameStateMemory::Read(fixture.image, native) && native == 7,
          "the interrupted atomic write leaves the full original DWORD intact");
    for (std::uint32_t state = 0; state != 10; ++state)
    {
        fixture.State(state);
        output = canary;
        const auto result = reader.Read(output);
        Check(state < 6 ? result == Status::Unsupported && output == canary : result == Status::Ok && output == mapped[state - 6],
              "every native state0..9 either translates through proved semantics or refuses with caller output unchanged");
    }
    for (const auto state : {10u, 0x80000000u, (std::numeric_limits<std::uint32_t>::max)()})
    {
        fixture.State(state);
        output = canary;
        Check(reader.Read(output) == Status::Unsupported && output == canary, "out-of-range native state DWORDs cannot be cast into SDK enum values");
    }
    fixture.State(6);
    Check(reader.Read(output) == Status::Ok && output == SystemState::GS_INIT_FRONTEND,
          "the next read observes fresh supported state instead of a cached successful value");
    bool        foreignRead = false, foreignBind = false;
    std::thread foreign(
        [&]
        {
            auto value = canary;
            foreignRead = reader.Read(value) == Status::WrongThread && value == canary;
            foreignBind = reader.Bind() == Status::WrongThread;
        });
    foreign.join();
    Check(foreignRead && foreignBind && reader.IsBound(), "foreign read/rebind cannot borrow or erase the caller's reader ownership");
    for (const auto& pin : P::pins)
    {
        const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + pin.rva);
        const auto changed = static_cast<unsigned char>(original ^ 1);
        fixture.Patch(pin.rva, &changed, 1);
        output = canary;
        Check(reader.Read(output) == Status::Unsupported && output == canary,
              "changing any required dispatch, semantic anchor or resource label refuses publication");
        Check(NativeGameStateMemory::CompareExchange(fixture.image, 6, 8) == NativeGameStateMemory::StoreResult::Unsupported,
              "the writer refuses every changed required native pin before an atomic write");
        fixture.Patch(pin.rva, &original, 1);
        Check(reader.Read(output) == Status::Ok && output == SystemState::GS_INIT_FRONTEND,
              "restored pins permit a new native read without rebinding to cached state");
    }
    for (DWORD protection : {PAGE_NOACCESS, PAGE_READONLY, PAGE_EXECUTE_READWRITE})
    {
        DWORD previous = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + P::stateRva), sizeof(std::uint32_t), protection, &previous) != FALSE,
              "change only the task-owned native state page");
        output = canary;
        Check(reader.Read(output) == Status::Unsupported && output == canary, "native state must remain readable, writable and non-executable");
        Check(NativeGameStateMemory::CompareExchange(fixture.image, 6, 8) == NativeGameStateMemory::StoreResult::Unsupported,
              "the write requires the same readable writable non-executable state page as reads");
        Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + P::stateRva), sizeof(std::uint32_t), previous, &ignored) != FALSE,
              "restore task-owned data-page classification");
    }
    GameStateTestHost::transitionAfterRead.store(true);
    output = canary;
    Check(reader.Read(output) == Status::Unsupported && output == canary && !GameStateTestHost::transitionAfterRead.load(),
          "a real owned state transition between successful reads preserves output rather than claiming a stable SDK state");
    Check(reader.Read(output) == Status::Ok && output == SystemState::GS_FRONTEND, "a later stable native state can be observed after the refused transition");
    GameStateTestHost::failStateRead.store(true);
    output = canary;
    Check(reader.Read(output) == Status::Unsupported && output == canary && !GameStateTestHost::failStateRead.load(),
          "a partial OS read poisons only staging and cannot publish an invented SDK state");
    reader.Reset();
    output = canary;
    Check(!reader.IsBound() && reader.Read(output) == Status::NotInitialized && output == canary,
          "Reset retires ownership rather than returning a prior state");
    fixture.State(9);
    Check(reader.Bind() == Status::Ok && reader.Read(output) == Status::Ok && output == SystemState::GS_PLAYING_GAME,
          "a fresh owner can bind and observe actual supported state after reset");
    reader.Reset();
    std::puts("All ten native inputs checked; only proved states6..9 translated");
    std::printf("Passed %u native game-state checks\n", checks);
    return 0;
}
