/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual Windows weather operations on owned image memory
 *****************************************************************************/
#include <windows.h>
#include "../../Client/game_de/NativeWeatherOverrideWin32.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

namespace
{
    using namespace mta::game_de;
    using Platform = NativeWeatherOverridePlatform;
    using Exchange = Platform::Exchange;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s (Windows error %lu)\n", why, GetLastError());
            std::exit(EXIT_FAILURE);
        }
    }
    struct OwnedImage
    {
        HANDLE         file = INVALID_HANDLE_VALUE, mapping = nullptr;
        void*          view = nullptr;
        std::uintptr_t base = 0;
        explicit OwnedImage(const std::filesystem::path& directory)
        {
            // A tiny file supplies sparse image-backed sections at real scalar
            // RVAs. Its NOP bytes are never executed or passed to LoadLibrary.
            std::vector<unsigned char> bytes(0x800);
            IMAGE_DOS_HEADER           dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x80;
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 3;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL | IMAGE_FILE_LARGE_ADDRESS_AWARE;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.ImageBase = 0x180000000ull;
            nt.OptionalHeader.SectionAlignment = 0x1000;
            nt.OptionalHeader.FileAlignment = 0x200;
            nt.OptionalHeader.MajorOperatingSystemVersion = 6;
            nt.OptionalHeader.MajorSubsystemVersion = 6;
            nt.OptionalHeader.SizeOfImage = 0x5730000;
            nt.OptionalHeader.SizeOfHeaders = 0x400;
            nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
            nt.OptionalHeader.SizeOfStackReserve = 0x100000;
            nt.OptionalHeader.SizeOfStackCommit = 0x1000;
            nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
            nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            std::memcpy(bytes.data() + 0x80, &nt, sizeof(nt));
            std::array<IMAGE_SECTION_HEADER, 3> sections{};
            std::memcpy(sections[0].Name, ".text", 5);
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x1000;
            sections[0].SizeOfRawData = 0x200;
            sections[0].PointerToRawData = 0x400;
            sections[0].Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            std::memcpy(sections[1].Name, ".gap", 4);
            sections[1].VirtualAddress = 0x2000;
            sections[1].Misc.VirtualSize = 0x52fe000;
            sections[1].Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            std::memcpy(sections[2].Name, ".scalar", 7);
            sections[2].VirtualAddress = 0x5300000;
            sections[2].Misc.VirtualSize = 0x430000;
            sections[2].SizeOfRawData = 0x200;
            sections[2].PointerToRawData = 0x600;
            sections[2].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            std::memcpy(bytes.data() + 0x80 + sizeof(nt), sections.data(), sizeof(sections));
            std::memset(bytes.data() + 0x400, 0x90, 0x200);
            const auto path = directory / L"owned-weather-platform-image.dll";
            file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "create only this run's small owned image file");
            DWORD written = 0;
            Check(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size(),
                  "write complete owned PE image");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            Check(mapping != nullptr, "map owned PE as genuine image pages");
            view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
            Check(view != nullptr, "map owned image without executing an instruction");
            base = reinterpret_cast<std::uintptr_t>(view);
        }
        ~OwnedImage()
        {
            if (view)
                UnmapViewOfFile(view);
            if (mapping)
                CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
    };
}

int main(int argc, char** argv)
{
    Check(argc == 2, "runner supplies a durable per-run evidence directory");
    NativeWeatherOverrideWin32                platform;
    OwnedImage                                image{std::filesystem::path(argv[1])};
    const NativeWeatherScalarProfile::Binding binding{image.base};
    const auto                                read = platform.Reader();
    Check(read != nullptr, "production platform supplies its actual checked image reader");
    Platform::Bytes stock{}, changed{}, actual{};
    Check(read(image.base + 0x1000, stock.data(), stock.size()), "read owned executable image bytes");
    changed = stock;
    changed[4] = 0x12;
    changed[5] = 0x34;
    std::uint32_t previous = 0xabcdef01;
    Check(!platform.OpenCode({}, image.base + 0x1000, 8, previous) && previous == 0xabcdef01, "wrong image preserves protection output");
    Check(!platform.OpenCode(binding, image.base + 0x1001, 8, previous), "unaligned code window is refused");
    Check(!platform.OpenCode(binding, image.base + 0x1000, 4, previous), "partial operand windows are refused");
    Check(platform.ExchangeCode(binding, image.base + 0x1000, 8, stock, changed) == Exchange::Unavailable,
          "read-execute image cannot be mutated without owned writable protection");
    Check(platform.OpenCode(binding, image.base + 0x1000, 8, previous) && previous == PAGE_EXECUTE_READ, "open exact aligned image window");
    Check(platform.ExchangeCode(binding, image.base + 0x1000, 8, changed, stock) == Exchange::Conflict, "atomic compare refuses stale expected bytes");
    Check(read(image.base + 0x1000, actual.data(), actual.size()) && actual == stock, "failed compare preserves both window and neighbors");
    Check(platform.ExchangeCode(binding, image.base + 0x1000, 8, stock, changed) == Exchange::Exchanged, "actual aligned64 native platform CAS commits");
    Check(read(image.base + 0x1000, actual.data(), actual.size()) && actual == changed, "aligned64 result preserves neighboring bytes");
    Check(platform.Flush(image.base + 0x1000, 8), "flush exact owned image window");
    Check(platform.CloseCode(binding, image.base + 0x1000, 8, previous), "restore exact original code protection");
    Check(platform.ExchangeCode(binding, image.base + 0x1000, 8, changed, stock) == Exchange::Unavailable, "restored RX page denies later code stores");
    Check(platform.OpenCode(binding, image.base + 0x1020, 16, previous), "open aligned128 owned image window");
    Check(read(image.base + 0x1020, stock.data(), stock.size()), "read actual aligned128 expected word");
    changed = stock;
    changed[7] = 0x56;
    changed[8] = 0x78;
    if (platform.CompareExchange128())
    {
        Check(platform.ExchangeCode(binding, image.base + 0x1020, 16, stock, changed) == Exchange::Exchanged,
              "CPU-qualified aligned128 CAS commits as one word");
        Check(read(image.base + 0x1020, actual.data(), actual.size()) && actual == changed, "aligned128 preserves all other actual image bytes");
        Check(platform.ExchangeCode(binding, image.base + 0x1020, 16, stock, changed) == Exchange::Conflict,
              "aligned128 compare preserves foreign current word");
    }
    else
        Check(platform.ExchangeCode(binding, image.base + 0x1020, 16, stock, changed) == Exchange::Unavailable,
              "unsupported CPU cannot execute128-bit exchange");
    Check(platform.CloseCode(binding, image.base + 0x1020, 16, previous), "restore128 window protection");
    for (std::size_t index = 0; index < NativeWeatherScalarProfile::Fields.size(); ++index)
    {
        const auto    field = static_cast<NativeWeatherScalarProfile::Field>(index);
        std::uint32_t value = 0xdeadbeef;
        const auto    replacement = 0x3f800000u + static_cast<std::uint32_t>(index);
        Check(platform.ReadScalar(binding, field, value) && value == 0, "read each genuine image-backed scalar field");
        Check(platform.ExchangeScalar(binding, field, 0, replacement) == Exchange::Exchanged, "exact scalar CAS uses its own qualified field");
        Check(platform.ExchangeScalar(binding, field, 0, 0) == Exchange::Conflict, "scalar CAS preserves an intervening field value");
        Check(platform.ReadScalar(binding, field, value) && value == replacement, "scalar publication preserves exact float bits");
    }
    std::uint32_t value = 0xdeadbeef;
    Check(!platform.ReadScalar(binding, static_cast<NativeWeatherScalarProfile::Field>(255), value) && value == 0xdeadbeef, "invalid field preserves output");
    Check(!platform.ReadScalar({}, NativeWeatherScalarProfile::Field::Rain, value) && value == 0xdeadbeef, "missing binding preserves output");
    Check(platform.ExchangeScalar({}, NativeWeatherScalarProfile::Field::Rain, 0, 1) == Exchange::Unavailable, "missing binding grants no scalar writes");
    const NativeWeatherOverrideCodeView emptyView({}, 1, read);
    Check(!platform.Revalidate(binding, emptyView), "owned generic PE does not impersonate supported game qualification");
    Check(
        !platform.AllocateNear({2, 1}) && !platform.AllocateNear({(std::numeric_limits<std::uintptr_t>::max)(), (std::numeric_limits<std::uintptr_t>::max)()}),
        "invalid and overflowing allocation ranges are refused");
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const NativeWeatherOverrideProfile::Range range{reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress),
                                                    reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress)};
    const auto                                scratch = platform.AllocateNear(range);
    Check(scratch != 0 && scratch >= range.minimum && scratch <= range.maximum && platform.ScratchValid(scratch),
          "allocate actual private RW nonexecutable scratch inside requested range");
    Check(!platform.ScratchValid(scratch + 4), "an interior address cannot own or release the allocation");
    Check(!platform.OpenCode({scratch}, scratch, 8, previous), "private executable-looking storage cannot become native code authority");
    Check(platform.ExchangeCode({scratch}, scratch, 8, stock, changed) == Exchange::Unavailable, "private storage cannot bypass image code requirements");
    Check(!read(scratch, actual.data(), actual.size()), "code reader refuses ordinary private scratch");
    DWORD old = 0, ignored = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(scratch), 4, PAGE_READONLY, &old) != FALSE, "owned fixture can change only its own scratch protection");
    Check(!platform.ScratchValid(scratch) && !platform.ReleaseScratch(scratch), "unqualified scratch protection cannot be silently freed");
    Check(VirtualProtect(reinterpret_cast<void*>(scratch), 4, old, &ignored) != FALSE && platform.ReleaseScratch(scratch),
          "restore owned RW protection before exact release");
    Check(!platform.ScratchValid(scratch) && !platform.ReleaseScratch(scratch), "released allocation cannot be reused as retained scratch identity");
    std::printf("Passed %u actual Windows weather platform checks (64-bit)\n", checks);
    return EXIT_SUCCESS;
}
