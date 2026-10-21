/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real checked pool/handle observations over owned OS image/data pages
 ****************************************************************************/
#include "../../Client/game_de/NativePedPoolReader.h"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

namespace PoolTestHost
{
    std::uintptr_t            image = 0, watched = 0;
    unsigned                  reads = 0, failAt = 0, ownerCalls = 0, revokeAt = 0;
    mta::game_de::ClockResult ownerResult = mta::game_de::ClockResult::Ok;
    void (*after)() = nullptr;
    HMODULE WINAPI Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (++reads == failAt)
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, input, output, bytes, copied);
        if (result && reinterpret_cast<std::uintptr_t>(input) == watched && after)
        {
            const auto action = after;
            after = nullptr;
            action();
        }
        return result;
    }
    mta::game_de::ClockResult Owner() noexcept
    {
        return ++ownerCalls == revokeAt ? mta::game_de::ClockResult::WrongThread : ownerResult;
    }
}
// Only current-main-image selection and observation faults are controlled.
// The actual PE/section/MEM_IMAGE/private-page checks and Resolve implementation
// compile once unchanged; the real executable hash remains a wrong-host refusal.
#define ReadProcessMemory PoolTestHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#define GetModuleHandleW PoolTestHost::Module
#include "../../Client/game_de/NativePedPoolReader.cpp"
#undef GetModuleHandleW
#undef ReadProcessMemory

namespace
{
    using namespace mta::game_de;
    namespace P = NativePedPoolProfile;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u ped-pool checks: %s (OS %lu)\n", checks, reason, GetLastError());
            std::exit(1);
        }
    }
    struct OwnedImage
    {
        HANDLE         file = INVALID_HANDLE_VALUE, mapping = nullptr;
        std::uintptr_t image = 0;
        explicit OwnedImage(const std::filesystem::path& directory)
        {
            std::vector<unsigned char> bytes(0xc00);
            IMAGE_DOS_HEADER           dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x80;
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 4;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL | IMAGE_FILE_LARGE_ADDRESS_AWARE;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.ImageBase = 0x180000000ull;
            nt.OptionalHeader.SectionAlignment = 0x1000;
            nt.OptionalHeader.FileAlignment = 0x200;
            nt.OptionalHeader.MajorOperatingSystemVersion = nt.OptionalHeader.MajorSubsystemVersion = 6;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>((P::ImageSize + 0xfff) & ~std::uintptr_t{0xfff});
            nt.OptionalHeader.SizeOfHeaders = 0x400;
            nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
            nt.OptionalHeader.SizeOfStackReserve = nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
            nt.OptionalHeader.SizeOfStackCommit = nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            std::memcpy(bytes.data() + 0x80, &nt, sizeof(nt));
            // SEC_IMAGE requires adjacent virtual sections even when most
            // bytes are unused. Keep the real OS page checks in the fixture.
            std::array<IMAGE_SECTION_HEADER, 4> sections{};
            std::memcpy(sections[0].Name, ".head", 5);
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0xfff000;
            sections[0].Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            std::memcpy(sections[1].Name, ".text", 5);
            sections[1].VirtualAddress = 0x1000000;
            sections[1].Misc.VirtualSize = 0x300000;
            sections[1].Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            std::memcpy(sections[2].Name, ".rdata", 6);
            sections[2].VirtualAddress = 0x1300000;
            sections[2].Misc.VirtualSize = 0x442b000;
            sections[2].Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            std::memcpy(sections[3].Name, ".data", 5);
            sections[3].VirtualAddress = 0x572b000;
            sections[3].Misc.VirtualSize = nt.OptionalHeader.SizeOfImage - sections[3].VirtualAddress;
            sections[3].Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            for (std::size_t index = 0; index < sections.size(); ++index)
            {
                sections[index].SizeOfRawData = 0x200;
                sections[index].PointerToRawData = static_cast<DWORD>(0x400 + index * 0x200);
            }
            std::memcpy(bytes.data() + 0x80 + sizeof(nt), sections.data(), sizeof(sections));
            const auto path = directory / L"owned-ped-pool-image.dll";
            file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "create a tiny task-owned sparse PE, never a copy of GTA");
            DWORD written = 0;
            Check(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size(),
                  "write the complete sparse fixture PE");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            Check(mapping != nullptr, "create genuine OS image pages without LoadLibrary or native execution");
            image = reinterpret_cast<std::uintptr_t>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != 0, "map owned image bytes");
            // The native PE's advertised image size is unrounded. The loader
            // requires a rounded reservation for this synthetic image first.
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            Patch(0x80, &nt, sizeof(nt), PAGE_READONLY);
            for (const auto& pin : P::Code)
                Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto method = image + P::Code[1].rva;
            Patch(0x1300010, &method, sizeof(method), PAGE_READONLY);
            Patch(0x1300020, &method, sizeof(method), PAGE_READONLY);
            const std::uintptr_t absent = 0;
            Patch(P::RootRva, &absent, sizeof(absent), PAGE_READWRITE);
        }
        void Patch(std::uintptr_t rva, const void* bytes, std::size_t size, DWORD final)
        {
            DWORD previous = 0, ignored = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &previous) != FALSE, "open only bounded owned fixture bytes");
            std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
            Check(VirtualProtect(reinterpret_cast<void*>(image + rva), size, final, &ignored) != FALSE, "restore real bounded page protection");
        }
        ~OwnedImage()
        {
            if (image)
                UnmapViewOfFile(reinterpret_cast<void*>(image));
            if (mapping)
                CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
    };
    struct Pool
    {
        unsigned char* header = nullptr;
        unsigned char* objects = nullptr;
        unsigned char* flags = nullptr;
        OwnedImage&    image;
        explicit Pool(OwnedImage& image) : image(image)
        {
            header = static_cast<unsigned char*>(VirtualAlloc(nullptr, 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, P::NativeCapacity * P::ObjectStride, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            flags = static_cast<unsigned char*>(VirtualAlloc(nullptr, P::NativeCapacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(header && objects && flags, "three actual private allocations mirror the native separate pool/header/flag banks");
            Reset();
        }
        template <typename T>
        void Put(std::size_t offset, T value)
        {
            std::memcpy(header + offset, &value, sizeof(value));
        }
        void Reset()
        {
            Put(0, reinterpret_cast<std::uintptr_t>(objects));
            Put(8, reinterpret_cast<std::uintptr_t>(flags));
            Put(16, static_cast<std::int32_t>(P::NativeCapacity));
            Put(20, std::int32_t{-1});
            Put(24, static_cast<unsigned char>(1));
            std::memset(flags, 0x80, P::NativeCapacity);
            for (const auto index : {std::size_t{0}, std::size_t{8}, P::NativeCapacity - 1})
            {
                const auto vtable = image.image + 0x1300010;
                std::memcpy(objects + index * P::ObjectStride, &vtable, sizeof(vtable));
            }
            flags[0] = 1;
            flags[8] = 127;
            flags[P::NativeCapacity - 1] = 0;
            const auto root = reinterpret_cast<std::uintptr_t>(header);
            image.Patch(P::RootRva, &root, sizeof(root), PAGE_READWRITE);
            PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
            PoolTestHost::after = nullptr;
            PoolTestHost::watched = 0;
            PoolTestHost::ownerResult = ClockResult::Ok;
        }
        ~Pool()
        {
            VirtualFree(flags, 0, MEM_RELEASE);
            VirtualFree(objects, 0, MEM_RELEASE);
            VirtualFree(header, 0, MEM_RELEASE);
        }
    };
    Pool*    changing = nullptr;
    unsigned mutation = 0;
    void     Change()
    {
        auto& pool = *changing;
        if (mutation == 0)
            pool.Put(20, std::int32_t{2});
        if (mutation == 1)
            pool.flags[0] = 2;
        if (mutation == 2)
            pool.Put(24, static_cast<unsigned char>(0));
        if (mutation == 3)
        {
            const auto other = pool.image.image + 0x1300020;
            std::memcpy(pool.objects, &other, sizeof(other));
        }
        if (mutation == 4)
        {
            const std::uintptr_t absent = 0;
            pool.image.Patch(P::RootRva, &absent, sizeof(absent), PAGE_READWRITE);
        }
    }
    NativePedPoolReader::Value Sentinel()
    {
        NativePedPoolReader::Value value;
        value.handle = 0xdeadbeef;
        value.nativePed = 123;
        return value;
    }
    bool Untouched(const NativePedPoolReader::Value& value)
    {
        return value.handle == 0xdeadbeef && value.nativePed == 123;
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2, "runner supplies only its durable evidence directory");
    NativePedPoolReader::Binding refused{123};
    Check(!NativePedPoolReader::Qualify(refused) && refused.image == 123,
          "real shipping executable admission refuses this ordinary host and preserves binding output");
    OwnedImage image{std::filesystem::path(argv[1])};
    Pool       pool(image);
    PoolTestHost::image = image.image;
    NativePedPoolReader reader({image.image}, PoolTestHost::Owner);
    NativePedPoolReader unowned({image.image}, nullptr);
    auto                noOwner = Sentinel();
    Check(unowned.Resolve(1, noOwner) == ClockResult::NotInitialized && Untouched(noOwner),
          "a qualified image without the existing callback owner cannot publish a native pool value");
    Check(NativePedPoolReader::Revalidate({image.image}), "exact image/page/section/native code qualification is real even in the controlled host");
    NativePedPoolReader::Value value;
    Check(reader.Resolve(1, value) == ClockResult::Ok && value.slot == 0 && value.generation == 1 &&
              value.nativePed == reinterpret_cast<std::uintptr_t>(pool.objects),
          "handle1 means occupied pool slot0 generation1, with no ambient player substitution");
    Check(reader.Recheck(value) == ClockResult::Ok, "immediate same-scope recheck confirms all consumed native identity");
    Check(reader.Resolve((8u << 8) | 127u, value) == ClockResult::Ok && value.slot == 8 &&
              value.nativePed == reinterpret_cast<std::uintptr_t>(pool.objects) + 8 * P::ObjectStride,
          "native high bits select a real derived-ped stride and low7 generation127 remains valid");
    Check(reader.Resolve(static_cast<std::uint32_t>((P::NativeCapacity - 1) << 8), value) == ClockResult::Ok && value.generation == 0,
          "wrapped occupied generation0 is valid; do not invent a nonzero-generation rule");
    for (const auto handle : {0xffffffffu, 0x80000000u, 1000u << 8, 2u, 0x80u})
    {
        value = Sentinel();
        Check(reader.Resolve(handle, value) != ClockResult::Ok && Untouched(value),
              "negative, out-of-bank, stale and crafted free-slot handles refuse without caller output mutation");
    }
    pool.Reset();
    Check(reader.Resolve(1, value) == ClockResult::Ok, "establish exact positive read count for exhaustive observation failure injection");
    const auto readCount = PoolTestHost::reads;
    for (unsigned fail = 1; fail <= readCount; ++fail)
    {
        pool.Reset();
        PoolTestHost::failAt = fail;
        value = Sentinel();
        Check(reader.Resolve(1, value) != ClockResult::Ok && Untouched(value), "each actual image/header/slot/vtable read failure preserves caller output");
    }
    for (unsigned revoke = 1; revoke <= 3; ++revoke)
    {
        pool.Reset();
        PoolTestHost::revokeAt = revoke;
        value = Sentinel();
        Check(reader.Resolve(1, value) == ClockResult::WrongThread && Untouched(value),
              "scope loss at every real owner bookend cannot publish a delayed raw native identity");
    }
    for (mutation = 0; mutation < 5; ++mutation)
    {
        pool.Reset();
        changing = &pool;
        PoolTestHost::watched = image.image + 0x1300010;
        PoolTestHost::after = Change;
        value = Sentinel();
        Check(reader.Resolve(1, value) != ClockResult::Ok && Untouched(value),
              "moving cursor/generation/ownership/vtable/root cannot survive the second full native capture");
    }
    pool.Reset();
    Check(reader.Resolve(1, value) == ClockResult::Ok, "obtain an immediate value before native slot generation advances");
    pool.flags[0] = 2;
    Check(reader.Recheck(value) != ClockResult::Ok && reader.Resolve(2, value) == ClockResult::Ok && value.generation == 2,
          "a later valid incarnation cannot masquerade as the previously observed ped handle");
    pool.Reset();
    DWORD prior = 0, ignored = 0;
    Check(VirtualProtect(pool.flags, P::NativeCapacity, PAGE_NOACCESS, &prior) != FALSE, "revoke only owned native flag-bank protection");
    value = Sentinel();
    Check(reader.Resolve(1, value) == ClockResult::ReadFailure && Untouched(value), "unreadable native bank refuses before a stale pointer can escape");
    Check(VirtualProtect(pool.flags, P::NativeCapacity, prior, &ignored) != FALSE, "restore only the owned bank page");
    const std::uintptr_t absent = 0;
    image.Patch(P::RootRva, &absent, sizeof(absent), PAGE_READWRITE);
    value = Sentinel();
    Check(reader.Resolve(1, value) == ClockResult::NotInitialized && Untouched(value), "absent native ped pool is explicit, not a fabricated local player");
    PoolTestHost::image = 0;
    value = Sentinel();
    Check(reader.Resolve(1, value) == ClockResult::UnsupportedBuild && Untouched(value), "a changed current main-image identity revokes the borrowed binding");
    std::printf("Passed %u native ped-pool reader checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
