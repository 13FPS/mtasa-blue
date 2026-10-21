/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual filename reader over an owned sparse Windows SEC_IMAGE
 *****************************************************************************/
#include "../../Client/game_de/NativeDataFileNamesReader.h"
#include "../../Client/game_de/NativeDataFileNamesProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <thread>
#include <vector>

namespace FileNamesTestHost
{
    std::uintptr_t           image = 0, flipAddress = 0;
    bool                     buildAllowed = true, flipAtLastName = false;
    std::atomic<std::size_t> reads{0}, failAt{0};
    HMODULE WINAPI           Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (reads.fetch_add(1) + 1 == failAt.load())
        {
            std::memset(output, 0xa7, bytes);
            if (copied)
                *copied = bytes / 2;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        const auto  result = ::ReadProcessMemory(process, source, output, bytes, copied);
        const auto& last = mta::game_de::NativeDataFileNamesProfile::Names.back();
        if (result && flipAtLastName && image && reinterpret_cast<std::uintptr_t>(source) == image + last.rva)
        {
            flipAtLastName = false;
            DWORD old = 0, ignored = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(flipAddress), 1, PAGE_READWRITE, &old))
                std::abort();
            *reinterpret_cast<unsigned char*>(flipAddress) ^= 1;
            if (!VirtualProtect(reinterpret_cast<void*>(flipAddress), 1, old, &ignored))
                std::abort();
        }
        return result;
    }
}
#define ReadProcessMemory FileNamesTestHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#undef ReadProcessMemory
namespace mta::game_de::NativeImageAccess
{
    bool FileNamesFixtureExecutable() noexcept
    {
        // Only this test translation unit can select an owned sparse image.
        // With no selection the real shipping executable hash gate still runs.
        return FileNamesTestHost::buildAllowed && (FileNamesTestHost::image || CheckExecutableBuild());
    }
}
#define CheckExecutableBuild FileNamesFixtureExecutable
#define GetModuleHandleW     FileNamesTestHost::Module
#include "../../Client/game_de/NativeDataFileNamesReader.cpp"
#undef GetModuleHandleW
#undef CheckExecutableBuild

using namespace mta::game_de;
namespace P = NativeDataFileNamesProfile;
namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, why);
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
            FileNamesTestHost::image = 0;
            if (image)
                UnmapViewOfFile(reinterpret_cast<void*>(image));
            if (mapping)
                CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
        void Patch(std::uintptr_t rva, const void* input, std::size_t size)
        {
            DWORD old = 0, ignored = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &old) != FALSE, "patch only owned image bytes");
            std::memcpy(reinterpret_cast<void*>(image + rva), input, size);
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, old, &ignored) != FALSE, "restore owned image protection");
        }
        void Create(const std::filesystem::path& directory)
        {
            struct Page
            {
                std::array<unsigned char, 4096> bytes{};
                DWORD                           flags = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA;
            };
            std::map<std::uint32_t, Page> pages;
            const auto                    seed = [&](std::uintptr_t rva, const void* input, std::size_t size, bool code)
            {
                const auto* bytes = static_cast<const unsigned char*>(input);
                for (std::size_t index = 0; index < size; ++index)
                {
                    const auto address = static_cast<std::uint32_t>(rva + index);
                    auto&      page = pages[address & ~std::uint32_t{0xfff}];
                    page.bytes[address & 0xfff] = bytes[index];
                    if (code)
                        page.flags = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE;
                }
            };
            for (const auto& pin : P::Code)
                seed(pin.rva, pin.bytes.data(), pin.size, true);
            for (const auto& name : P::Names)
                seed(name.rva, name.expected, name.size, false);
            for (const auto& literal : P::Literals)
                seed(literal.rva, literal.expected, literal.size, false);
            std::vector<IMAGE_SECTION_HEADER> sections;
            std::vector<unsigned char>        bytes(0x1000, 0);
            std::uint32_t                     cursor = 0x1000;
            const auto                        section = [&](std::uint32_t rva, std::uint32_t size, DWORD flags, const Page* page)
            {
                IMAGE_SECTION_HEADER value{};
                const char*          name = page ? ".owned" : ".gap";
                std::memcpy(value.Name, name, std::strlen(name));
                value.VirtualAddress = rva;
                value.Misc.VirtualSize = size;
                value.Characteristics = flags;
                if (page)
                {
                    value.PointerToRawData = static_cast<DWORD>(bytes.size());
                    value.SizeOfRawData = 4096;
                    bytes.insert(bytes.end(), page->bytes.begin(), page->bytes.end());
                }
                sections.push_back(value);
            };
            constexpr DWORD gap = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_UNINITIALIZED_DATA;
            for (const auto& entry : pages)
            {
                if (entry.first > cursor)
                    section(cursor, entry.first - cursor, gap, nullptr);
                section(entry.first, 4096, entry.second.flags, &entry.second);
                cursor = entry.first + 4096;
            }
            constexpr std::uint32_t roundedImageSize = 0x5ca5000;
            section(cursor, roundedImageSize - cursor, gap, nullptr);
            IMAGE_DOS_HEADER dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = static_cast<WORD>(sections.size());
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.ImageBase = UINT64_C(0x180000000);
            nt.OptionalHeader.SectionAlignment = 4096;
            nt.OptionalHeader.FileAlignment = 512;
            nt.OptionalHeader.SizeOfHeaders = 4096;
            nt.OptionalHeader.SizeOfImage = roundedImageSize;
            nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
            nt.OptionalHeader.MajorOperatingSystemVersion = nt.OptionalHeader.MajorSubsystemVersion = 6;
            nt.OptionalHeader.SizeOfStackReserve = nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
            nt.OptionalHeader.SizeOfStackCommit = nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            Check(sections.size() <= 96 && 0x100 + sizeof(nt) + sections.size() * sizeof(IMAGE_SECTION_HEADER) <= 4096, "bounded sparse PE headers");
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            std::memcpy(bytes.data() + 0x100, &nt, sizeof(nt));
            std::memcpy(bytes.data() + 0x100 + sizeof(nt), sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER));
            const auto path = directory / L"owned-data-file-names-image.bin";
            Check(!std::filesystem::exists(path), "create unique owned sparse image");
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save tiny image without copying GTA");
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "open only owned fixture");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            Check(mapping != nullptr, "actual Windows SEC_IMAGE metadata admission");
            image = reinterpret_cast<std::uintptr_t>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != 0, "map owned image with genuine OS page types");
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            Patch(0x100, &nt, sizeof(nt));
        }
    };
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    NativeDataFileNamesReader reader;
    bool                      output = true;
    Check(reader.Read(output) == Status::NotInitialized && output && !reader.IsBound(), "unbound read preserves output");
    Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "real shipping image hash refuses the ordinary test host");
    OwnedImage fixture;
    fixture.Create(argv[1]);
    FileNamesTestHost::image = fixture.image;
    Check(reader.Bind() == Status::Ok && reader.IsBound(), "bind only owned image with actual PE/pin/page checks");
    output = false;
    Check(reader.Read(output) == Status::Ok && output, "all twelve native loader operands match");
    bool        foreignRead = false, foreignBind = false;
    std::thread foreign(
        [&]
        {
            bool value = true;
            foreignRead = reader.Read(value) == Status::WrongThread && value;
            foreignBind = reader.Bind() == Status::WrongThread;
        });
    foreign.join();
    Check(foreignRead && foreignBind && reader.IsBound(), "wrong-thread read/rebind cannot erase read ownership");
    for (const auto& name : P::Names)
    {
        for (const auto offset : {std::size_t{0}, name.size - 1})
        {
            const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + name.rva + offset);
            const auto changed = static_cast<unsigned char>(original ^ 1);
            fixture.Patch(name.rva + offset, &changed, 1);
            output = true;
            Check(reader.Read(output) == Status::Ok && !output, "changed loader name/terminator yields genuine false, not binding refusal");
            fixture.Patch(name.rva + offset, &original, 1);
            Check(reader.Read(output) == Status::Ok && output, "restored live name is observed without cached result");
        }
    }
    const auto refuseByte = [&](std::uintptr_t rva)
    {
        const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + rva);
        const auto changed = static_cast<unsigned char>(original ^ 1);
        fixture.Patch(rva, &changed, 1);
        for (const auto canary : {false, true})
        {
            output = canary;
            Check(reader.Read(output) == Status::Unsupported && output == canary, "changed native operand relationship refuses and preserves output");
        }
        fixture.Patch(rva, &original, 1);
    };
    for (const auto& pin : P::Code)
        refuseByte(pin.rva);
    for (const auto& literal : P::Literals)
        refuseByte(literal.rva);
    const auto copyCount = FileNamesTestHost::reads.load();
    Check(reader.Read(output) == Status::Ok && output, "measure complete actual OS copy sequence");
    const auto copies = FileNamesTestHost::reads.load() - copyCount;
    for (std::size_t failure = 1; failure <= copies; ++failure)
    {
        FileNamesTestHost::failAt = FileNamesTestHost::reads.load() + failure;
        output = true;
        Check(reader.Read(output) == Status::Unsupported && output, "every partial OS copy keeps caller output intact");
        FileNamesTestHost::failAt = 0;
    }
    const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + P::Names.front().rva);
    FileNamesTestHost::flipAddress = fixture.image + P::Names.front().rva;
    FileNamesTestHost::flipAtLastName = true;
    output = true;
    Check(reader.Read(output) == Status::Unsupported && output, "mismatching full name samples cannot publish a mixed result");
    fixture.Patch(P::Names.front().rva, &original, 1);
    const auto selected = FileNamesTestHost::image;
    FileNamesTestHost::image = 0;
    output = true;
    Check(reader.Read(output) == Status::Unsupported && output, "binding cannot follow a replacement main image");
    FileNamesTestHost::image = selected;
    DWORD      old = 0, ignored = 0;
    const auto denied = reinterpret_cast<void*>(fixture.image + P::Names.front().rva);
    Check(VirtualProtect(denied, 1, PAGE_NOACCESS, &old) != FALSE, "deny only owned name pages");
    output = false;
    Check(reader.Read(output) == Status::Unsupported && !output, "unreadable name page is refusal rather than false comparison");
    Check(VirtualProtect(denied, 1, old, &ignored) != FALSE, "restore owned name pages");
    const auto& nt = *reinterpret_cast<const IMAGE_NT_HEADERS64*>(fixture.image + 0x100);
    const auto  mutate = [&](std::uintptr_t rva, const auto& originalValue, const auto& replacement)
    {
        fixture.Patch(rva, &replacement, sizeof(replacement));
        output = true;
        Check(reader.Read(output) == Status::Unsupported && output, "invalid native PE identity/range retains output");
        fixture.Patch(rva, &originalValue, sizeof(originalValue));
    };
    const auto& dos = *reinterpret_cast<const IMAGE_DOS_HEADER*>(fixture.image);
    const auto  dosMagic = dos.e_magic;
    mutate(offsetof(IMAGE_DOS_HEADER, e_magic), dosMagic, WORD{0});
    const auto dosOffset = dos.e_lfanew;
    mutate(offsetof(IMAGE_DOS_HEADER, e_lfanew), dosOffset, LONG{-1});
    const auto signature = nt.Signature;
    mutate(0x100 + offsetof(IMAGE_NT_HEADERS64, Signature), signature, DWORD{0});
    const auto machine = nt.FileHeader.Machine;
    mutate(0x100 + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, Machine), machine, WORD{IMAGE_FILE_MACHINE_I386});
    const auto size = nt.OptionalHeader.SizeOfImage;
    mutate(0x100 + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, SizeOfImage), size, DWORD{1});
    Check(reader.Read(output) == Status::Ok && output, "all failed reads leave reader reusable after real restoration");
    FileNamesTestHost::buildAllowed = false;
    Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "failed owner-thread rebind clears old binding");
    FileNamesTestHost::buildAllowed = true;
    Check(reader.Bind() == Status::Ok, "fresh qualified binding after reset");
    reader.Reset();
    Check(!reader.IsBound() && reader.Read(output) == Status::NotInitialized, "explicit reset retires borrowed ownership");
    std::printf("Passed %u native data filename checks (64-bit)\n", checks);
    return 0;
}
