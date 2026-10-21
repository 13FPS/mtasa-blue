/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real checked pool/handle observations over owned OS image/data pages
 ****************************************************************************/
#include "../../Client/game_de/NativePedPoolReader.h"
#include "../../Client/game_de/NativePedTransformReader.h"
#include "../../Client/game_de/NativePedTransformProfile.h"
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
#include "../../Client/game_de/NativePedTransformReader.cpp"
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
            std::fprintf(stderr, "FAIL after %u ped-transform checks: %s (OS %lu)\n", checks, reason, GetLastError());
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
            const auto path = directory / L"owned-ped-transform-image.dll";
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

    struct TransformFixture
    {
        OwnedImage     image;
        Pool           pool;
        unsigned char* matrix = nullptr;
        explicit TransformFixture(const std::filesystem::path& path) : image(path), pool(image)
        {
            namespace T = NativePedTransformProfile;
            for (const auto& pin : T::Code)
                image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto firstMethod = image.image + NativePedPoolProfile::Code[1].rva;
            const auto teleport = image.image + T::NativeTeleportRva;
            for (const auto table : {T::PedVtableRva, T::PlayerVtableRva})
            {
                image.Patch(table, &firstMethod, sizeof(firstMethod), PAGE_READONLY);
                image.Patch(table + T::TeleportSlotOffset, &teleport, sizeof(teleport), PAGE_READONLY);
            }
            image.Patch(T::ScriptVelocityInScaleRva, &T::ScriptVelocityInScaleBits, 4, PAGE_READONLY);
            image.Patch(T::ScriptVelocityOutScaleRva, &T::ScriptVelocityOutScaleBits, 4, PAGE_READONLY);
            matrix = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(matrix != nullptr, "retain one actual private native-style matrix allocation with a distinct owner field");
            Reset(false);
        }
        template <class T>
        void Store(std::size_t offset, const T& value)
        {
            std::memcpy(pool.objects + offset, &value, sizeof(value));
        }
        template <class T>
        void Matrix(std::size_t offset, const T& value)
        {
            std::memcpy(matrix + offset, &value, sizeof(value));
        }
        void Reset(bool linked)
        {
            namespace T = NativePedTransformProfile;
            pool.Reset();
            const auto table = image.image + T::PlayerVtableRva;
            Store(0, table);
            Store(T::InlinePositionOffset, CVector(1.25f, -2.5f, 3.75f));
            Store(T::MatrixPointerOffset, linked ? reinterpret_cast<std::uintptr_t>(matrix) : std::uintptr_t{0});
            Store(T::VelocityOffset, CVector(0.125f, -0.25f, 0.5f));
            Matrix(0, CVector(1, 2, 3));
            Matrix(0x10, CVector(4, 5, 6));
            Matrix(0x20, CVector(7, 8, 9));
            Matrix(0x30, CVector(10, 20, 30));
            Matrix(T::MatrixOwnerOffset, reinterpret_cast<std::uintptr_t>(pool.objects));
        }
        ~TransformFixture()
        {
            if (matrix)
                VirtualFree(matrix, 0, MEM_RELEASE);
        }
    };
    NativePedTransformReader::Value TransformSentinel()
    {
        NativePedTransformReader::Value v;
        v.ped.handle = 0xdeadbeef;
        v.positionAddress = 123;
        v.position = CVector(101, 102, 103);
        v.velocity = CVector(201, 202, 203);
        return v;
    }
    bool TransformUntouched(const NativePedTransformReader::Value& v)
    {
        return v.ped.handle == 0xdeadbeef && v.positionAddress == 123 && v.position.fX == 101 && v.position.fY == 102 && v.position.fZ == 103 &&
               v.velocity.fX == 201 && v.velocity.fY == 202 && v.velocity.fZ == 203;
    }
    TransformFixture* activeTransform = nullptr;
    unsigned          transformMutation = 0;
    void              ChangeTransform()
    {
        namespace T = NativePedTransformProfile;
        auto& f = *activeTransform;
        if (transformMutation == 0)
            f.Store(T::MatrixPointerOffset, std::uintptr_t{0});
        if (transformMutation == 1)
            f.Matrix(T::MatrixOwnerOffset, std::uintptr_t{0});
        if (transformMutation == 2)
            f.Matrix(0x30, CVector(31, 32, 33));
        if (transformMutation == 3)
            f.Store(T::VelocityOffset, CVector(4, 5, 6));
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "owned transform evidence uses an explicit workspace output directory");
    NativePedTransformReader::Binding refused{123};
    Check(!NativePedTransformReader::Qualify(refused) && refused.image == 123, "shipping wrong-executable qualification remains real and output-preserving");
    TransformFixture fixture(argv[1]);
    PoolTestHost::image = fixture.image.image;
    NativePedTransformReader reader({fixture.image.image}, &PoolTestHost::Owner), unowned({fixture.image.image}, nullptr);
    auto                     value = TransformSentinel();
    Check(unowned.Read(1, value) == ClockResult::NotInitialized && TransformUntouched(value),
          "native image and pool identity cannot replace a borrowed callback owner");
    Check(NativePedTransformReader::Revalidate({fixture.image.image}),
          "all exact transform code, relocated method slots, section policies and script scales qualify");
    Check(reader.Read(1, value) == ClockResult::Ok && value.ped.nativePed == reinterpret_cast<std::uintptr_t>(fixture.pool.objects) && !value.matrix &&
              value.positionAddress == value.ped.nativePed + 8 && value.position.fX == 1.25f && value.position.fY == -2.5f && value.position.fZ == 3.75f &&
              value.velocity.fX == 0.125f && value.velocity.fY == -0.25f && value.velocity.fZ == 0.5f,
          "inline placement and raw velocity use the resolved actual ped, without script conversion or ambient actor substitution");
    CVector* borrowed = nullptr;
    Check(reader.BorrowPosition(1, borrowed) == ClockResult::Ok && reinterpret_cast<std::uintptr_t>(borrowed) == value.positionAddress,
          "the immediate SDK-compatible borrow exposes the actual inline CVector address, never a cached vector copy");
    const CVector changed(11, 12, 13);
    std::memcpy(borrowed, &changed, sizeof(changed));
    Check(reader.Recheck(value) != ClockResult::Ok && reader.Read(1, value) == ClockResult::Ok && value.position.fX == 11,
          "native intent observed through the real borrowed position invalidates a previous snapshot without forcing a later transform");
    fixture.Reset(true);
    Check(reader.Read(1, value) == ClockResult::Ok && value.matrix == reinterpret_cast<std::uintptr_t>(fixture.matrix) &&
              value.positionAddress == value.matrix + 0x30 && value.position.fX == 10 && value.position.fY == 20 && value.position.fZ == 30 &&
              value.matrixRows[0].fX == 1 && value.matrixRows[1].fY == 5 && value.matrixRows[2].fZ == 9,
          "a currently owned matrix supplies its padded real rows and position instead of stale inline translation");
    Check(reader.BorrowPosition(1, borrowed) == ClockResult::Ok && borrowed == reinterpret_cast<CVector*>(fixture.matrix + 0x30),
          "matrix mode returns the genuine native position address with no synthetic matrix allocation");
    const auto nativePed = reinterpret_cast<std::uintptr_t>(fixture.pool.objects);
    fixture.Matrix(NativePedTransformProfile::MatrixOwnerOffset, nativePed + NativePedPoolProfile::ObjectStride);
    value = TransformSentinel();
    borrowed = reinterpret_cast<CVector*>(std::uintptr_t{1});
    Check(reader.Read(1, value) == ClockResult::BindingChanged && TransformUntouched(value) && reader.BorrowPosition(1, borrowed) != ClockResult::Ok &&
              borrowed == reinterpret_cast<CVector*>(std::uintptr_t{1}),
          "a matrix already recycled to another owner cannot escape through the old ped selector");
    fixture.Reset(true);
    Check(reader.Read(1, value) == ClockResult::Ok, "seed a valid linked observation before failure scheduling");
    PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
    Check(reader.Read(1, value) == ClockResult::Ok, "measure only actual guarded OS-read and owner boundaries");
    const auto reads = PoolTestHost::reads, owners = PoolTestHost::ownerCalls;
    for (unsigned failure = 1; failure <= reads; ++failure)
    {
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::failAt = failure;
        value = TransformSentinel();
        Check(reader.Read(1, value) != ClockResult::Ok && TransformUntouched(value),
              "every actual image/pool/matrix/vector read failure preserves caller output");
    }
    PoolTestHost::failAt = 0;
    for (unsigned refusal = 1; refusal <= owners; ++refusal)
    {
        PoolTestHost::reads = PoolTestHost::ownerCalls = 0;
        PoolTestHost::revokeAt = refusal;
        value = TransformSentinel();
        Check(reader.Read(1, value) == ClockResult::WrongThread && TransformUntouched(value),
              "each real borrowed owner boundary can revoke before snapshot publication");
    }
    PoolTestHost::revokeAt = 0;
    activeTransform = &fixture;
    for (transformMutation = 0; transformMutation < 4; ++transformMutation)
    {
        fixture.Reset(true);
        PoolTestHost::watched = transformMutation == 3 ? reinterpret_cast<std::uintptr_t>(fixture.pool.objects) + NativePedTransformProfile::VelocityOffset
                                                       : reinterpret_cast<std::uintptr_t>(fixture.matrix) + 0x30;
        PoolTestHost::after = &ChangeTransform;
        value = TransformSentinel();
        Check(reader.Read(1, value) != ClockResult::Ok && TransformUntouched(value),
              "changing matrix selection, matrix ownership, position or velocity cannot create a hybrid snapshot");
    }
    for (mutation = 0; mutation < 5; ++mutation)
    {
        fixture.Reset(true);
        changing = &fixture.pool;
        PoolTestHost::watched = reinterpret_cast<std::uintptr_t>(fixture.pool.header) + 20;
        PoolTestHost::after = &Change;
        value = TransformSentinel();
        Check(reader.Read(1, value) != ClockResult::Ok && TransformUntouched(value),
              "independent pool root/header/slot/vtable movement revokes the corresponding transform");
    }
    fixture.Reset(true);
    Check(reader.Read(1, value) == ClockResult::Ok, "obtain a same-owner matrix observation before slot retirement");
    fixture.pool.flags[0] = 2;
    Check(reader.Recheck(value) != ClockResult::Ok && reader.Read(2, value) == ClockResult::Ok && value.ped.generation == 2,
          "the same ped address under a different issued generation cannot inherit an old transform observation");
    fixture.Reset(true);
    DWORD prior = 0, ignored = 0;
    Check(VirtualProtect(fixture.matrix, 0x1000, PAGE_NOACCESS, &prior) != FALSE, "revoke only owned matrix pages");
    value = TransformSentinel();
    Check(reader.Read(1, value) != ClockResult::Ok && TransformUntouched(value),
          "unreadable matrix storage refuses rather than falling back to stale inline data");
    Check(VirtualProtect(fixture.matrix, 0x1000, prior, &ignored) != FALSE, "restore only owned matrix pages");
    const auto zero = std::uintptr_t{0};
    fixture.image.Patch(NativePedPoolProfile::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
    borrowed = reinterpret_cast<CVector*>(std::uintptr_t{1});
    Check(reader.BorrowPosition(1, borrowed) == ClockResult::NotInitialized && borrowed == reinterpret_cast<CVector*>(std::uintptr_t{1}),
          "absent handle pool never aliases ambient local player or publishes a replacement vector");
    PoolTestHost::image = 0;
    value = TransformSentinel();
    Check(reader.Read(1, value) == ClockResult::UnsupportedBuild && TransformUntouched(value),
          "changed main-image identity invalidates the borrowed transform binding");
    std::printf("Passed %u native ped-transform reader checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
