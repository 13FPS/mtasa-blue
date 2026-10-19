/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual checked partition reader over a sparse, owned SEC_IMAGE
 *****************************************************************************/
#include "../../Client/game_de/NativeStreamingFileIdsReader.h"
#include "../../Client/game_de/NativeStreamingFileIdsProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace StreamingTestHost
{
    std::uintptr_t           image = 0;
    std::atomic<std::size_t> reads{0}, failAt{0};
    HMODULE WINAPI           Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (reads.fetch_add(1) + 1 == failAt.load())
        {
            // Deterministically inject an OS copy failure after normal page
            // admission. Poison local staging to prove output is not published.
            std::memset(output, 0xA7, bytes);
            if (copied)
                *copied = bytes / 2;
            SetLastError(ERROR_PARTIAL_COPY);
            return FALSE;
        }
        return ::ReadProcessMemory(process, source, output, bytes, copied);
    }
}

#define ReadProcessMemory StreamingTestHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#undef ReadProcessMemory
namespace mta::game_de::NativeImageAccess
{
    bool StreamingFixtureExecutable() noexcept
    {
        // Only this test translation unit can select its owned image. A null
        // selection still exercises the unchanged shipping executable hash gate.
        return StreamingTestHost::image != 0 || CheckExecutableBuild();
    }
}
#define CheckExecutableBuild StreamingFixtureExecutable
#define GetModuleHandleW     StreamingTestHost::Module
#include "../../Client/game_de/NativeStreamingFileIdsReader.cpp"
#undef GetModuleHandleW
#undef CheckExecutableBuild

using namespace mta::game_de;
namespace P = NativeStreamingFileIdsProfile;
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
    const NativeStreamingFileIds expected{0, 20000, 25000, 25255, 25511, 25575, 25755, 26230, 26316};
    const NativeStreamingFileIds canary{11, 22, 33, 44, 55, 66, 77, 88, 99};
    bool                         Equal(const NativeStreamingFileIds& a, const NativeStreamingFileIds& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }

    class OwnedImage
    {
    public:
        std::uintptr_t image = 0;
        HANDLE         file = INVALID_HANDLE_VALUE, mapping = nullptr;
        ~OwnedImage()
        {
            StreamingTestHost::image = 0;
            if (image)
                UnmapViewOfFile(reinterpret_cast<void*>(image));
            if (mapping)
                CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
        void Patch(std::uintptr_t rva, const void* bytes, std::size_t size)
        {
            DWORD old = 0, ignored = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &old) != FALSE, "admit only owned fixture bytes for patching");
            std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, old, &ignored) != FALSE, "restore owned fixture page protection");
        }
        void Create(const std::filesystem::path& root)
        {
            const auto path = root / L"owned-streaming-image.bin";
            Check(!std::filesystem::exists(path), "create unique owned image evidence");
            std::vector<unsigned char> bytes(0x9000, 0);
            IMAGE_DOS_HEADER           dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 11;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.ImageBase = UINT64_C(0x180000000);
            nt.OptionalHeader.SectionAlignment = 0x1000;
            nt.OptionalHeader.FileAlignment = 0x200;
            nt.OptionalHeader.SizeOfHeaders = 0x1000;
            nt.OptionalHeader.SizeOfCode = 0x5000;
            nt.OptionalHeader.SizeOfInitializedData = 0x3000;
            nt.OptionalHeader.BaseOfCode = 0x113b000;
            nt.OptionalHeader.SizeOfImage = 0x5ca5000;
            nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
            nt.OptionalHeader.MajorOperatingSystemVersion = nt.OptionalHeader.MajorSubsystemVersion = 6;
            nt.OptionalHeader.SizeOfStackReserve = nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
            nt.OptionalHeader.SizeOfStackCommit = nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            std::memcpy(bytes.data() + dos.e_lfanew, &nt, sizeof(nt));
            std::array<IMAGE_SECTION_HEADER, 11> sections{};
            const auto section = [&](std::size_t index, const char* name, std::uint32_t rva, std::uint32_t size, std::uint32_t raw, DWORD flags)
            {
                auto& value = sections[index];
                std::memcpy(value.Name, name, std::strlen(name));
                value.VirtualAddress = rva;
                value.Misc.VirtualSize = size;
                value.SizeOfRawData = raw ? size : 0;
                value.PointerToRawData = raw;
                value.Characteristics = flags;
            };
            constexpr DWORD gap = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            constexpr DWORD code = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            constexpr DWORD data = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            // Adjacent virtual sections satisfy the Windows loader while only
            // four tiny raw sections carry pins. No game file is copied or run.
            section(0, ".head", 0x1000, 0x113a000, 0, gap);
            section(1, ".code1", 0x113b000, 0x2000, 0x1000, code);
            section(2, ".gap1", 0x113d000, 0x149000, 0, gap);
            section(3, ".code2", 0x1286000, 0x3000, 0x3000, code);
            section(4, ".gap2", 0x1289000, 0x3080000, 0, gap);
            section(5, ".names1", 0x4309000, 0x2000, 0x6000, data);
            section(6, ".gap3", 0x430b000, 0xf000, 0, gap);
            section(7, ".names2", 0x431a000, 0x1000, 0x8000, data);
            section(8, ".gap4", 0x431b000, 0xf7c000, 0, gap);
            section(9, ".rows", 0x5297000, 0x68000, 0, gap | IMAGE_SCN_MEM_WRITE);
            section(10, ".tail", 0x52ff000, 0x9a6000, 0, gap);
            std::memcpy(bytes.data() + dos.e_lfanew + sizeof(nt), sections.data(), sizeof(sections));
            const auto seed = [&](std::uintptr_t rva, const auto& pin)
            {
                for (const auto& value : sections)
                    if (value.PointerToRawData && rva >= value.VirtualAddress && rva + pin.size() <= value.VirtualAddress + value.SizeOfRawData)
                    {
                        std::memcpy(bytes.data() + value.PointerToRawData + rva - value.VirtualAddress, pin.data(), pin.size());
                        return;
                    }
                Check(false, "every native pin must fit in a small owned raw section");
            };
            seed(P::initializeRva, P::initialize);
            seed(P::classifierRva, P::classifier);
            seed(P::selectorRva, P::selector);
            seed(P::requestStrideRva, P::requestStride);
            seed(P::requestDffRva, P::requestDff);
            seed(P::requestWindowsRva, P::requestWindows);
            seed(P::categoryNamesRva, P::categoryNames);
            seed(P::iplNameRva, P::iplName);
            seed(P::filenameFormatsRva, P::filenameFormats);
            seed(P::scriptFormatRva, P::scriptFormat);
            seed(P::animationRecordingFormatsRva, P::animationRecordingFormats);
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save bounded sparse PE fixture");
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "open only owned fixture image");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            Check(mapping != nullptr, "map the sparse fixture as actual Windows SEC_IMAGE");
            image = reinterpret_cast<std::uintptr_t>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != 0, "obtain owned image pages");
            // The native header has an unaligned observed size. Seed only that
            // metadata after the kernel admits a normally rounded image map.
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::imageSize);
            Patch(static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt));
        }
    };
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    NativeStreamingFileIdsReader reader;
    auto                         output = canary;
    Check(!reader.IsBound() && reader.Read(output) == Status::NotInitialized && Equal(output, canary), "unbound read preserves every output field");
    Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "actual shipping hash gate refuses this test executable");
    OwnedImage fixture;
    fixture.Create(argv[1]);
    StreamingTestHost::image = fixture.image;
    Check(reader.Bind() == Status::Ok && reader.IsBound(), "bind only the selected owned image after real PE/pin/page validation");
    Check(reader.Read(output) == Status::Ok && Equal(output, expected), "read actual checked native operands for all nine SDK values");
    std::array<std::uint32_t, 9> actual{}, wanted{};
    std::memcpy(actual.data(), &output, sizeof(output));
    std::memcpy(wanted.data(), &expected, sizeof(expected));
    for (std::size_t index = 0; index < actual.size(); ++index)
        Check(actual[index] == wanted[index], "independent expected partition field");
    Check(output.count == 26312 + 4, "SDK count includes all four list sentinel rows");
    bool        foreignRead = false, foreignBind = false;
    std::thread foreign(
        [&]
        {
            auto value = canary;
            foreignRead = reader.Read(value) == Status::WrongThread && Equal(value, canary);
            foreignBind = reader.Bind() == Status::WrongThread;
        });
    foreign.join();
    Check(foreignRead && foreignBind && reader.IsBound(), "foreign read/rebind cannot borrow or erase caller-thread read ownership");

    const auto refuseByte = [&](std::uintptr_t rva)
    {
        const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + rva);
        const auto changed = static_cast<unsigned char>(original ^ 1);
        fixture.Patch(rva, &changed, 1);
        auto value = canary;
        Check(reader.Read(value) == Status::Unsupported && Equal(value, canary),
              "changed limits, identity, stride/count, names or instructions preserve all outputs");
        fixture.Patch(rva, &original, 1);
        Check(reader.Read(value) == Status::Ok && Equal(value, expected), "restored owned pins allow a fresh successful read without cached defaults");
    };
    for (const auto rva : std::array<std::uintptr_t, 24>{P::classifierRva,
                                                         P::requestStrideRva,
                                                         P::requestDffRva,
                                                         P::requestWindowsRva,
                                                         P::categoryNamesRva,
                                                         P::iplNameRva,
                                                         P::filenameFormatsRva,
                                                         P::scriptFormatRva,
                                                         P::animationRecordingFormatsRva,
                                                         0x1286b5c,
                                                         0x1286b56,
                                                         0x1286b80,
                                                         0x1286bd0,
                                                         0x1286c1f,
                                                         0x1286c6e,
                                                         0x1286c9c,
                                                         0x1286cc4,
                                                         0x113bde3,
                                                         0x113bde0,
                                                         0x113bdec,
                                                         0x113be08,
                                                         0x113be27,
                                                         0x113be46,
                                                         0})
        refuseByte(rva);
    constexpr std::uintptr_t ntRva = 0x100;
    refuseByte(ntRva + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, Machine));
    refuseByte(ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, SizeOfImage));
    refuseByte(ntRva + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, NumberOfSections) + 1);

    for (const auto target : {P::selectorRva, P::categoryNamesRva, P::nativeArrayRva})
    {
        DWORD old = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + target), 1, PAGE_NOACCESS, &old) != FALSE, "withhold an owned code/name/array page");
        auto value = canary;
        Check(reader.Read(value) == Status::Unsupported && Equal(value, canary), "real unavailable image pages cannot publish partial partitions");
        Check(VirtualProtect(reinterpret_cast<void*>(fixture.image + target), 1, old, &ignored) != FALSE, "restore owned page after refusal");
    }
    const auto before = StreamingTestHost::reads.load();
    Check(reader.Read(output) == Status::Ok, "measure complete native-reader sample operations");
    const auto sampleReads = (StreamingTestHost::reads.load() - before) / 2;
    for (const auto delay : {std::size_t{1}, sampleReads + 1})
    {
        StreamingTestHost::failAt.store(StreamingTestHost::reads.load() + delay);
        auto value = canary;
        Check(reader.Read(value) == Status::Unsupported && Equal(value, canary), "injected OS partial-copy failure in either complete sample preserves output");
        StreamingTestHost::failAt.store(0);
    }
    auto* privatePage = VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Check(privatePage != nullptr, "allocate only an owned non-image refusal page");
    StreamingTestHost::image = reinterpret_cast<std::uintptr_t>(privatePage);
    NativeStreamingFileIdsReader privateReader;
    Check(privateReader.Bind() == Status::Unsupported, "MEM_PRIVATE bytes cannot substitute for an owned executable image");
    Check(VirtualFree(privatePage, 0, MEM_RELEASE) != FALSE, "release the owned private refusal page");
    StreamingTestHost::image = fixture.image;
    reader.Reset();
    output = canary;
    Check(!reader.IsBound() && reader.Read(output) == Status::NotInitialized && Equal(output, canary),
          "reset removes read admission without publishing stale limits");
    std::printf("Passed %u native streaming file-ID checks\n", checks);
    std::puts("All nine native partition values and four sentinel rows checked");
    return 0;
}
