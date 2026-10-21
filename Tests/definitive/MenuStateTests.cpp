/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real native frontend-menu reader over a small task-owned SEC_IMAGE
 *****************************************************************************/
#include "../../Client/game_de/NativeGameStateReader.h"
#include "../../Client/game_de/NativeGameStateProfile.h"
#include "../../Client/game_de/NativeFrontendMenuProfile.h"
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

namespace MenuStateTestHost
{
    // Live evidence identifies this adjacent field as a volatile scalar, not
    // menu state. Its changes must not invalidate a widget-only observation.
    constexpr std::size_t ignoredScalarOffset = 0x350;
    struct Header
    {
        std::uint64_t data;
        std::int32_t  count;
        std::int32_t  capacity;
        std::uint64_t pending;
    };
    static_assert(sizeof(Header) == 24);
    std::uintptr_t             image = 0;
    std::uintptr_t             receiver = 0;
    std::atomic<bool>          failMenuRead{false};
    std::atomic<bool>          transitionAfterRead{false};
    std::atomic<bool>          changeRootAfterRead{false};
    std::atomic<bool>          changeVptrAfterRead{false};
    std::atomic<unsigned>      changeSlotAfterRead{0};
    std::atomic<std::uint32_t> ignoredScalarSequence{0};
    std::atomic<bool>          failOwnerRead{false};

    HMODULE WINAPI Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }

    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (receiver && reinterpret_cast<std::uintptr_t>(source) == receiver && bytes == sizeof(std::uintptr_t) && failOwnerRead.exchange(false))
        {
            std::memset(output, 0xa7, bytes);
            if (copied)
                *copied = bytes / 2;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        if (image && reinterpret_cast<std::uintptr_t>(source) == image + mta::game_de::NativeFrontendMenuProfile::menuHeaderRva && bytes == sizeof(Header) &&
            failMenuRead.exchange(false))
        {
            // Fail after real page admission and poison local staging. A partial
            // OS copy must never publish a made-up SDK menu state to the caller.
            std::memset(output, 0xa7, bytes);
            if (copied)
                *copied = bytes / 2;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, source, output, bytes, copied);
        if (result && image && reinterpret_cast<std::uintptr_t>(source) == image + mta::game_de::NativeFrontendMenuProfile::menuHeaderRva &&
            bytes == sizeof(Header) && transitionAfterRead.exchange(false))
        {
            // Change the real owned image between two successful OS reads.
            // A menu transition must not publish a mixed owner/header snapshot.
            constexpr Header next{0, 0, 0, 1};
            SIZE_T           written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeFrontendMenuProfile::menuHeaderRva), &next,
                                      sizeof(next), &written) ||
                written != sizeof(next))
                std::abort();
        }
        if (result && image && reinterpret_cast<std::uintptr_t>(source) == image + mta::game_de::NativeFrontendMenuProfile::engineRootRva &&
            bytes == sizeof(std::uintptr_t) && changeRootAfterRead.exchange(false))
        {
            // Root bookends must detect retirement even when the copied
            // receiver itself remains readable until this owned test ends.
            constexpr std::uintptr_t next = 0;
            SIZE_T                   written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + mta::game_de::NativeFrontendMenuProfile::engineRootRva), &next,
                                      sizeof(next), &written) ||
                written != sizeof(next))
                std::abort();
        }
        if (result && image && receiver && reinterpret_cast<std::uintptr_t>(source) == receiver && bytes == sizeof(std::uintptr_t) &&
            changeVptrAfterRead.exchange(false))
        {
            const auto next = image + mta::game_de::NativeFrontendMenuProfile::engineVtableRva + sizeof(std::uintptr_t);
            SIZE_T     written = 0;
            if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(receiver), &next, sizeof(next), &written) || written != sizeof(next))
                std::abort();
        }
        if (result && receiver && reinterpret_cast<std::uintptr_t>(source) == receiver + mta::game_de::NativeFrontendMenuProfile::widgetOffset &&
            bytes == sizeof(std::uintptr_t))
        {
            const auto change = changeSlotAfterRead.exchange(0);
            if (change)
            {
                constexpr std::uintptr_t widget = 1;
                const auto               scalar = ignoredScalarSequence.fetch_add(1) + 1;
                const auto               offset = change == 1 ? mta::game_de::NativeFrontendMenuProfile::widgetOffset : ignoredScalarOffset;
                const auto               sourceValue = change == 1 ? static_cast<const void*>(&widget) : static_cast<const void*>(&scalar);
                const auto               size = change == 1 ? sizeof(widget) : sizeof(scalar);
                SIZE_T                   written = 0;
                if (!::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(receiver + offset), sourceValue, size, &written) || written != size)
                    std::abort();
            }
        }
        return result;
    }
}

#define ReadProcessMemory MenuStateTestHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#undef ReadProcessMemory
namespace mta::game_de::NativeImageAccess
{
    bool MenuStateFixtureExecutable() noexcept
    {
        // The selected owned image exists only in this test translation unit.
        // With no selection, the shipping executable hash gate stays active.
        return MenuStateTestHost::image != 0 || CheckExecutableBuild();
    }
}
#define CheckExecutableBuild MenuStateFixtureExecutable
#define GetModuleHandleW     MenuStateTestHost::Module
#define ReadProcessMemory    MenuStateTestHost::Read
#include "../../Client/game_de/NativeGameStateMemory.cpp"
#include "../../Client/game_de/NativeGameStateReader.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
#undef CheckExecutableBuild

using namespace mta::game_de;
namespace P = NativeGameStateProfile;
namespace M = NativeFrontendMenuProfile;
namespace
{
    unsigned checks = 0;
    using Header = MenuStateTestHost::Header;

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL after %u native menu-state checks: %s\n", checks, why);
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
            MenuStateTestHost::image = 0;
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

        void HeaderValue(const Header& value)
        {
            SIZE_T copied = 0;
            Check(::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + M::menuHeaderRva), &value, sizeof(value), &copied) &&
                      copied == sizeof(value),
                  "seed only the owned legacy menu header");
        }

        void Root(std::uintptr_t value)
        {
            SIZE_T copied = 0;
            Check(::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + M::engineRootRva), &value, sizeof(value), &copied) &&
                      copied == sizeof(value),
                  "seed only the owned receiver root");
        }

        void Create(const std::filesystem::path& directory)
        {
            const auto path = directory / L"owned-menu-state-image.bin";
            Check(!std::filesystem::exists(path), "preserve earlier evidence by creating the owned image once");
            struct Page
            {
                std::uint32_t rva;
                DWORD         flags;
            };
            std::vector<Page> pages;
            constexpr DWORD   code = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            constexpr DWORD   data = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            const auto        addPins = [&](const auto& pins)
            {
                for (const auto& pin : pins)
                    for (auto page = pin.rva & ~std::uintptr_t{0xfff}; page < pin.rva + pin.size; page += 0x1000)
                        pages.push_back({static_cast<std::uint32_t>(page), pin.code ? code : data});
            };
            addPins(P::pins);
            addPins(M::pins);
            pages.push_back({static_cast<std::uint32_t>(M::engineVtableRva & ~std::uintptr_t{0xfff}), data});
            for (const auto& target : M::vtableTargets)
            {
                pages.push_back({static_cast<std::uint32_t>(target.slotRva & ~std::uintptr_t{0xfff}), data});
                pages.push_back({static_cast<std::uint32_t>(target.targetRva & ~std::uintptr_t{0xfff}), code});
            }
            for (const auto& target : M::imagePointers)
            {
                pages.push_back({static_cast<std::uint32_t>(target.slotRva & ~std::uintptr_t{0xfff}), data});
                pages.push_back({static_cast<std::uint32_t>(target.targetRva & ~std::uintptr_t{0xfff}), target.code ? code : data});
            }
            pages.push_back({static_cast<std::uint32_t>(P::stateRva & ~std::uintptr_t{0xfff}), data | IMAGE_SCN_MEM_WRITE});
            pages.push_back({static_cast<std::uint32_t>(M::menuHeaderRva & ~std::uintptr_t{0xfff}), data | IMAGE_SCN_MEM_WRITE});
            pages.push_back({static_cast<std::uint32_t>(M::engineRootRva & ~std::uintptr_t{0xfff}), data | IMAGE_SCN_MEM_WRITE});
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
            Check(sections.size() < 64, "the compact native profile needs only a bounded sparse section table");
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
            const auto seedPins = [&](const auto& pins)
            {
                for (const auto& pin : pins)
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
            };
            seedPins(P::pins);
            seedPins(M::pins);
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
            for (const auto& target : M::vtableTargets)
            {
                // Real mapped vtables contain relocated VAs. Seed those after
                // mapping so the fixture cannot pass an RVA-only comparison.
                const auto pointer = image + target.targetRva;
                Patch(target.slotRva, &pointer, sizeof(pointer));
            }
            for (const auto& target : M::imagePointers)
            {
                const auto pointer = image + target.targetRva;
                Patch(target.slotRva, &pointer, sizeof(pointer));
            }
        }
    };

    class OwnedReceiver
    {
    public:
        std::uintptr_t address = 0;

        OwnedReceiver()
        {
            address = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(address != 0, "allocate only a task-owned non-executable private receiver page");
            MenuStateTestHost::receiver = address;
        }

        ~OwnedReceiver()
        {
            MenuStateTestHost::receiver = 0;
            if (address)
                VirtualFree(reinterpret_cast<void*>(address), 0, MEM_RELEASE);
        }

        template <typename T>
        void Field(std::size_t offset, T value)
        {
            SIZE_T copied = 0;
            Check(offset + sizeof(value) <= 0x1000 &&
                      ::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address + offset), &value, sizeof(value), &copied) &&
                      copied == sizeof(value),
                  "seed only the task-owned receiver field");
        }
    };
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    NativeGameStateReader reader;
    bool                  output = true;
    Check(!reader.IsBound() && reader.ReadMenu(output) == Status::NotInitialized && output, "unbound menu reads preserve output");
    Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "the shipping executable hash gate still rejects the test host");
    OwnedImage fixture;
    fixture.Create(argv[1]);
    MenuStateTestHost::image = fixture.image;
    OwnedReceiver receiver;
    receiver.Field(0, fixture.image + M::engineVtableRva);
    receiver.Field(M::widgetOffset, std::uintptr_t{0});
    receiver.Field(MenuStateTestHost::ignoredScalarOffset, std::uint32_t{0});
    fixture.State(9);
    fixture.HeaderValue({0, 0, 0, 0});
    fixture.Root(receiver.address);
    Check(reader.Bind() == Status::Ok && reader.IsBound(), "bind the exact image once while keeping native menu ownership read-only");
    Check(reader.ReadMenu(output) == Status::Ok && !output, "empty typed receiver and legacy header mean no native menu");

    // Opaque nonnull values are deliberately unreadable addresses. A genuine
    // receiver qualifies these slots; no widget or array element is inspected.
    receiver.Field(M::widgetOffset, std::uintptr_t{1});
    Check(reader.ReadMenu(output) == Status::Ok && output, "the genuine UMG widget counts even when the legacy header is empty");
    receiver.Field(M::widgetOffset, std::uintptr_t{0});
    fixture.HeaderValue({0, 0, 0, 1});
    Check(reader.ReadMenu(output) == Status::Ok && output, "a pending legacy menu counts without dereferencing its pointer");
    fixture.HeaderValue({1, 1, 1, 0});
    Check(reader.ReadMenu(output) == Status::Ok && output, "a populated legacy stack counts without dereferencing its data");
    receiver.Field(M::widgetOffset, std::uintptr_t{1});
    Check(reader.ReadMenu(output) == Status::Ok && output, "simultaneous genuine and legacy menus still produce one true predicate");
    receiver.Field(M::widgetOffset, std::uintptr_t{0});
    fixture.HeaderValue({1, (std::numeric_limits<std::int32_t>::max)(), (std::numeric_limits<std::int32_t>::max)(), 0});
    Check(reader.ReadMenu(output) == Status::Ok && output, "signed native array invariants impose no invented count or pointer-alignment ceiling");
    fixture.HeaderValue({0, 0, 0, 0});
    receiver.Field(0x7b0, std::uintptr_t{1});
    Check(reader.ReadMenu(output) == Status::Ok && !output, "an intro-movie widget alone is not a frontend menu");
    receiver.Field(0x7b0, std::uintptr_t{0});

    for (const auto header : {Header{0, -1, 0, 0}, Header{0, 0, -1, 0}, Header{1, 2, 1, 0}, Header{0, 1, 1, 0}})
    {
        fixture.HeaderValue(header);
        output = true;
        Check(reader.ReadMenu(output) == Status::Unsupported && output, "invalid native array storage refuses without replacing caller output");
    }
    fixture.HeaderValue({0, 0, 0, 0});
    bool        foreignRead = false, foreignBind = false;
    std::thread foreign(
        [&]
        {
            bool value = true;
            foreignRead = reader.ReadMenu(value) == Status::WrongThread && value;
            foreignBind = reader.Bind() == Status::WrongThread;
        });
    foreign.join();
    Check(foreignRead && foreignBind && reader.IsBound(), "a foreign thread cannot borrow or retire the reader's owner");

    for (const auto& pin : M::pins)
    {
        const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + pin.rva);
        const auto changed = static_cast<unsigned char>(original ^ 1);
        fixture.Patch(pin.rva, &changed, 1);
        output = true;
        Check(reader.ReadMenu(output) == Status::Unsupported && output, "each independent menu semantic or owner pin is required");
        fixture.Patch(pin.rva, &original, 1);
        Check(reader.ReadMenu(output) == Status::Ok && !output, "restored menu pins permit a fresh uncached native read");
    }
    for (const auto root : {std::uintptr_t{0}, std::uintptr_t{1}, fixture.image + M::menuHeaderRva})
    {
        fixture.Root(root);
        output = true;
        Check(reader.ReadMenu(output) == Status::Unsupported && output,
              "unavailable, unreadable or image-backed roots cannot stand in for a qualified heap receiver");
    }
    fixture.Root(receiver.address);
    receiver.Field(0, fixture.image + M::engineVtableRva + sizeof(std::uintptr_t));
    output = true;
    Check(reader.ReadMenu(output) == Status::Unsupported && output, "readable receiver storage with a different native type is unsupported");
    receiver.Field(0, fixture.image + M::engineVtableRva);
    for (const DWORD protection : {PAGE_NOACCESS, PAGE_READONLY, PAGE_EXECUTE_READWRITE})
    {
        DWORD previous = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(receiver.address), 0x1000, protection, &previous) != FALSE, "change only the owned receiver page");
        output = true;
        Check(reader.ReadMenu(output) == Status::Unsupported && output, "native receiver storage must remain readable, writable and non-executable");
        Check(VirtualProtect(reinterpret_cast<void*>(receiver.address), 0x1000, previous, &ignored) != FALSE, "restore the owned receiver page");
    }
    for (const auto rva : {M::menuHeaderRva, M::engineRootRva})
    {
        DWORD previous = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), 8, PAGE_READONLY, &previous) != FALSE, "change only the owned menu image data page");
        output = true;
        Check(reader.ReadMenu(output) == Status::Unsupported && output, "menu globals must retain their native writable non-executable page classification");
        Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), 8, previous, &ignored) != FALSE, "restore the owned menu image data page");
    }
    const auto target = M::vtableTargets.front();
    Check(fixture.image != UINT64_C(0x140000000), "the owned image uses a different loaded base from the profiled game executable");
    const auto unrelocated = UINT64_C(0x140000000) + target.targetRva;
    fixture.Patch(target.slotRva, &unrelocated, sizeof(unrelocated));
    output = true;
    Check(reader.ReadMenu(output) == Status::Unsupported && output, "an unrelocated native vtable target cannot qualify a rebased image");
    const auto relocated = fixture.image + target.targetRva;
    fixture.Patch(target.slotRva, &relocated, sizeof(relocated));
    Check(reader.ReadMenu(output) == Status::Ok && !output, "actual loaded VA normalization accepts the relocated vtable target");

    MenuStateTestHost::transitionAfterRead.store(true);
    output = false;
    Check(reader.ReadMenu(output) == Status::Unsupported && !output && !MenuStateTestHost::transitionAfterRead.load(),
          "a native header transition between full snapshots preserves caller output");
    Check(reader.ReadMenu(output) == Status::Ok && output, "the later stable pending menu is observed without rebinding");
    fixture.HeaderValue({0, 0, 0, 0});
    MenuStateTestHost::changeSlotAfterRead.store(1);
    output = false;
    Check(reader.ReadMenu(output) == Status::Unsupported && !output && !MenuStateTestHost::changeSlotAfterRead.load(),
          "a genuine widget change between snapshots cannot publish a mixed menu predicate");
    Check(reader.ReadMenu(output) == Status::Ok && output, "the later stable genuine widget is observed without rebinding");
    MenuStateTestHost::changeSlotAfterRead.store(2);
    output = false;
    Check(reader.ReadMenu(output) == Status::Ok && output && !MenuStateTestHost::changeSlotAfterRead.load(),
          "an unrelated adjacent scalar increment cannot reject a stable genuine widget");
    receiver.Field(M::widgetOffset, std::uintptr_t{0});
    MenuStateTestHost::changeSlotAfterRead.store(2);
    output = true;
    Check(reader.ReadMenu(output) == Status::Ok && !output && !MenuStateTestHost::changeSlotAfterRead.load(),
          "an unrelated adjacent scalar increment cannot invent or reject empty menu state");
    MenuStateTestHost::changeRootAfterRead.store(true);
    output = true;
    Check(reader.ReadMenu(output) == Status::Unsupported && output && !MenuStateTestHost::changeRootAfterRead.load(),
          "root bookends reject receiver retirement during a snapshot");
    fixture.Root(receiver.address);
    MenuStateTestHost::changeVptrAfterRead.store(true);
    output = true;
    Check(reader.ReadMenu(output) == Status::Unsupported && output && !MenuStateTestHost::changeVptrAfterRead.load(),
          "typed receiver bookends reject a changed owner vptr");
    receiver.Field(0, fixture.image + M::engineVtableRva);
    MenuStateTestHost::failMenuRead.store(true);
    output = true;
    Check(reader.ReadMenu(output) == Status::Unsupported && output && !MenuStateTestHost::failMenuRead.load(),
          "a partial OS copy cannot publish a fabricated menu predicate");
    MenuStateTestHost::failOwnerRead.store(true);
    output = true;
    Check(reader.ReadMenu(output) == Status::Unsupported && output && !MenuStateTestHost::failOwnerRead.load(),
          "a heap OS-copy failure after page admission preserves caller output");
    reader.Reset();
    output = true;
    Check(!reader.IsBound() && reader.ReadMenu(output) == Status::NotInitialized && output, "Reset retires menu read ownership and cached values");
    Check(reader.Bind() == Status::Ok && reader.ReadMenu(output) == Status::Ok && !output, "a new reader owner can observe the restored empty native menu");
    reader.Reset();
    std::puts("Genuine widget, legacy menu and intro-only distinction checked");
    std::printf("Passed %u native menu-state checks\n", checks);
    return 0;
}
