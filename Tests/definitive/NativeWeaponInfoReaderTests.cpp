/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real native weapon reader and SDK getters over owned Windows images
 *****************************************************************************/
#include "../../Client/game_de/NativeWeaponInfoReader.h"
#include "../../Client/game_de/NativeWeaponInfoProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include "../../Client/game_de/CWeaponInfoReadDE.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

namespace WeaponTestHost
{
    std::uintptr_t           image = 0, flipAddress = 0, watchedRow = 0;
    bool                     buildAllowed = true, flipAfterRow = false;
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
        const auto result = ::ReadProcessMemory(process, source, output, bytes, copied);
        if (result && flipAfterRow && image && reinterpret_cast<std::uintptr_t>(source) == watchedRow && bytes == 112)
        {
            flipAfterRow = false;
            DWORD old = 0, ignored = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(flipAddress), 1, PAGE_READWRITE, &old))
                std::abort();
            *reinterpret_cast<unsigned char*>(flipAddress) ^= 1;
            if (!VirtualProtect(reinterpret_cast<void*>(flipAddress), 1, old, &ignored))
                std::abort();
        }
        return result;
    }
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: mutation/pointer SDK method reached read-only fixture\n", stderr);
        std::abort();
    }
}
#define ReadProcessMemory WeaponTestHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#undef ReadProcessMemory
namespace mta::game_de::NativeImageAccess
{
    bool WeaponFixtureExecutable() noexcept
    {
        return WeaponTestHost::buildAllowed && (WeaponTestHost::image || CheckExecutableBuild());
    }
}
#define CheckExecutableBuild WeaponFixtureExecutable
#define GetModuleHandleW     WeaponTestHost::Module
#include "../../Client/game_de/NativeWeaponInfoReader.cpp"
#undef GetModuleHandleW
#undef CheckExecutableBuild
#include "../../Client/game_de/CWeaponInfoReadDE.cpp"
#include "WeaponInfoReadFixture.generated.h"

using namespace mta::game_de;
namespace P = NativeWeaponInfoProfile;
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
    template <class Operation>
    void Refuses(Operation operation, const char* why)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, why);
            return;
        }
        catch (...)
        {
        }
        Check(false, why);
    }

    NativeWeaponInfoValues Row(std::size_t slot)
    {
        NativeWeaponInfoValues value;
        const auto             number = static_cast<float>(slot);
        value.fireType = static_cast<std::int32_t>(slot % 6);
        value.targetRange = 100.25f + number;
        value.weaponRange = 200.5f + number;
        value.model = slot ? 300 + static_cast<std::int32_t>(slot) : -1;
        value.model2 = -1;
        value.slot = static_cast<std::int32_t>(slot % 13);
        const std::uint32_t flags = UINT32_C(0xa5000000) | static_cast<std::uint32_t>(slot);
        std::memcpy(&value.flags, &flags, sizeof(flags));
        value.animGroup = UINT32_C(0xc0000000) + static_cast<std::uint32_t>(slot);
        value.clipAmmo = static_cast<std::int16_t>(-1234 + static_cast<int>(slot));
        value.damage = static_cast<std::int16_t>(-234 + static_cast<int>(slot));
        value.fireOffset[0] = number + 0.125f;
        value.fireOffset[1] = number + 0.25f;
        value.fireOffset[2] = number + 0.375f;
        value.skill = slot < 47 ? 1 : slot < 58 ? 0 : slot < 69 ? 2 : 3;
        value.requiredStat = 16777217 + static_cast<std::int32_t>(slot);
        value.accuracy = number + 0.625f;
        value.moveSpeed = number + 0.75f;
        value.animLoopStart = number + 1.125f;
        value.animLoopEnd = number + 1.25f;
        value.animFireTime = number + 1.375f;
        value.anim2LoopStart = number + 1.5f;
        value.anim2LoopEnd = number + 1.625f;
        value.anim2FireTime = number + 1.75f;
        value.animBreakout = number + 1.875f;
        value.speed = number + 2.125f;
        value.radius = number + 2.25f;
        value.lifeSpan = number + 2.375f;
        value.spread = number + 2.5f;
        value.aimOffset = static_cast<std::int16_t>(-44 + static_cast<int>(slot));
        value.defaultCombo = static_cast<std::uint8_t>(slot + 4);
        value.combosAvailable = static_cast<std::uint8_t>(slot + 1);
        return value;
    }

    class OwnedImage
    {
    public:
        std::uintptr_t image = 0;
        HANDLE         file = INVALID_HANDLE_VALUE, mapping = nullptr;
        ~OwnedImage()
        {
            WeaponTestHost::image = 0;
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
            const auto                    seed = [&](std::uintptr_t rva, const void* input, std::size_t size, DWORD flags)
            {
                const auto* bytes = static_cast<const unsigned char*>(input);
                for (std::size_t index = 0; index < size; ++index)
                {
                    const auto address = static_cast<std::uint32_t>(rva + index);
                    auto&      page = pages[address & ~std::uint32_t{0xfff}];
                    page.bytes[address & 0xfff] = bytes[index];
                    page.flags = flags;
                }
            };
            constexpr DWORD code = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE;
            constexpr DWORD literal = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA;
            constexpr DWORD bank = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA;
            for (const auto& pin : P::Code)
                seed(pin.rva, pin.bytes.data(), pin.size, code);
            for (const auto& pin : P::Literals)
                seed(pin.rva, pin.bytes.data(), pin.size, literal);
#if defined(MTA_DE_WEAPON_FIXTURE_EXTRA_SEED)
            // Extra owned-image suites may add qualified code/data pages. The
            // ordinary reader fixture keeps its original seed set unchanged.
            MTA_DE_WEAPON_FIXTURE_EXTRA_SEED(seed, code, literal, bank);
#endif
            for (std::size_t slot = 0; slot < P::RowCount; ++slot)
            {
                const auto value = Row(slot);
                seed(P::BankRva + slot * P::RowBytes, &value, sizeof(value), bank);
            }
            std::vector<IMAGE_SECTION_HEADER> sections;
            std::vector<unsigned char>        bytes(0x1000, 0);
            std::uint32_t                     cursor = 0x1000;
            const auto                        section = [&](std::uint32_t rva, std::uint32_t size, DWORD flags, const std::vector<unsigned char>* contents)
            {
                IMAGE_SECTION_HEADER value{};
                const char*          name = contents ? ".owned" : ".gap";
                std::memcpy(value.Name, name, std::strlen(name));
                value.VirtualAddress = rva;
                value.Misc.VirtualSize = size;
                value.Characteristics = flags;
                if (contents)
                {
                    value.PointerToRawData = static_cast<DWORD>(bytes.size());
                    value.SizeOfRawData = static_cast<DWORD>(contents->size());
                    bytes.insert(bytes.end(), contents->begin(), contents->end());
                }
                sections.push_back(value);
            };
            constexpr DWORD gap = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_UNINITIALIZED_DATA;
            for (auto it = pages.begin(); it != pages.end();)
            {
                const auto rva = it->first;
                const auto flags = it->second.flags;
                if (rva > cursor)
                    section(cursor, rva - cursor, gap, nullptr);
                std::vector<unsigned char> contents;
                auto                       end = rva;
                do
                {
                    contents.insert(contents.end(), it->second.bytes.begin(), it->second.bytes.end());
                    end += 4096;
                    ++it;
                } while (it != pages.end() && it->first == end && it->second.flags == flags);
                section(rva, end - rva, flags, &contents);
                cursor = end;
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
            Check(sections.size() <= 96 && 0x100 + sizeof(nt) + sections.size() * sizeof(IMAGE_SECTION_HEADER) <= 4096, "bounded owned sparse PE headers");
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            std::memcpy(bytes.data() + 0x100, &nt, sizeof(nt));
            std::memcpy(bytes.data() + 0x100 + sizeof(nt), sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER));
            const auto path = directory / L"owned-weapon-bank-image.bin";
            Check(!std::filesystem::exists(path), "create only unique owned sparse image");
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save small weapon fixture without copying GTA");
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "open only owned fixture");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            Check(mapping != nullptr, "actual Windows SEC_IMAGE mapping");
            image = reinterpret_cast<std::uintptr_t>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != 0, "map genuine owned OS image pages");
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            Patch(0x100, &nt, sizeof(nt));
        }
    };

    class TestWeapon final : public WeaponTestHost::WeaponFixtureBase
    {
    public:
        TestWeapon(const NativeWeaponInfoReader& reader, std::size_t slot) noexcept : WeaponFixtureBase(reader, slot) {}
    };

    void CheckSdk(CWeaponInfo& sdk, const NativeWeaponInfoValues& value)
    {
        Check(sdk.GetModel() == static_cast<eWeaponModel>(value.model) && sdk.GetModel2() == static_cast<eWeaponModel>(value.model2),
              "SDK preserves both model values including native -1");
        Check(sdk.GetWeaponRange() == value.weaponRange && sdk.GetTargetRange() == value.targetRange, "SDK reads genuine range scalars");
        Check(sdk.GetDamagePerHit() == value.damage && sdk.GetMaximumClipAmmo() == value.clipAmmo, "SDK preserves signed sixteen-bit damage and clip values");
        Check(sdk.GetAccuracy() == value.accuracy && sdk.GetMoveSpeed() == value.moveSpeed, "SDK reads accuracy and movement scalars");
        Check(sdk.GetFiringSpeed() == value.speed && sdk.GetRadius() == value.radius && sdk.GetLifeSpan() == value.lifeSpan && sdk.GetSpread() == value.spread,
              "SDK reads projectile/area scalars");
        Check(sdk.GetSlot() == static_cast<eWeaponSlot>(value.slot) && sdk.GetSkill() == static_cast<eWeaponSkill>(value.skill),
              "SDK reads slot and skill types");
        Check(sdk.GetRequiredStatLevel() == static_cast<float>(value.requiredStat), "SDK preserves integer-to-float required-stat conversion");
        Check(sdk.GetFlags() == value.flags && sdk.IsFlagSet(0x80000000) && !sdk.IsFlagSet(0) && !sdk.IsFlagSet(0x02000000),
              "SDK flags preserve signed bits, high-bit and empty masks");
        Check(sdk.GetAnimGroup() == value.animGroup && sdk.GetFireType() == static_cast<eFireType>(value.fireType),
              "SDK reads animation group and native classified fire type");
        Check(sdk.GetWeaponAnimLoopStart() == value.animLoopStart && sdk.GetWeaponAnimLoopStop() == value.animLoopEnd &&
                  sdk.GetWeaponAnimLoopFireTime() == value.animFireTime,
              "SDK reads first animation times");
        Check(sdk.GetWeaponAnim2LoopStart() == value.anim2LoopStart && sdk.GetWeaponAnim2LoopStop() == value.anim2LoopEnd &&
                  sdk.GetWeaponAnim2LoopFireTime() == value.anim2FireTime,
              "SDK reads second animation times");
        Check(sdk.GetAnimBreakoutTime() == value.animBreakout && sdk.GetWeaponAnimBreakoutTime() == value.animBreakout && sdk.GetWeaponRadius() == value.radius,
              "SDK aliases use the same genuine native field");
        Check(sdk.GetAimOffsetIndex() == value.aimOffset && sdk.GetDefaultCombo() == value.defaultCombo && sdk.GetCombosAvailable() == value.combosAvailable,
              "SDK preserves final short and byte fields");
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    static_assert(std::is_abstract_v<CWeaponInfoReadDE> && !std::is_abstract_v<TestWeapon>);
    NativeWeaponInfoReader reader;
    NativeWeaponInfoValues output = Row(3), canary = output;
    Check(reader.Read(0, output) == Status::NotInitialized && std::memcmp(&output, &canary, sizeof(output)) == 0, "unbound row read retains output");
    Check(reader.Read(P::RowCount, output) == Status::InvalidArgument && std::memcmp(&output, &canary, sizeof(output)) == 0,
          "invalid SDK slot never reads memory");
    Check(reader.Read(static_cast<std::size_t>(-1), output) == Status::InvalidArgument && std::memcmp(&output, &canary, sizeof(output)) == 0,
          "overflowing SDK slot cannot enter address arithmetic");
    Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "shipping executable gate refuses ordinary host");
    OwnedImage fixture;
    fixture.Create(argv[1]);
    WeaponTestHost::image = fixture.image;
    Check(reader.Bind() == Status::Ok && reader.IsBound(), "bind actual production reader over owned PE/pin/page checks");
    for (std::size_t slot = 0; slot < P::RowCount; ++slot)
    {
        const auto expected = Row(slot);
        Check(reader.Read(slot, output) == Status::Ok && std::memcmp(&output, &expected, sizeof(output)) == 0,
              "all eighty native rows use exact independent bank stride");
    }
    TestWeapon first(reader, 0), last(reader, 79), invalid(reader, P::RowCount);
    CheckSdk(first, Row(0));
    CheckSdk(last, Row(79));
    Refuses([&] { invalid.GetWeaponRange(); }, "actual SDK getter explicitly refuses invalid row");
    auto changed = Row(0);
    changed.weaponRange = 543.25f;
    fixture.Patch(P::BankRva, &changed, sizeof(changed));
    Check(first.GetWeaponRange() == changed.weaponRange, "actual SDK getter observes fresh native bytes without cached snapshot");
    const auto originalFirst = Row(0);
    fixture.Patch(P::BankRva, &originalFirst, sizeof(originalFirst));
    bool        foreignRead = false, foreignBind = false, foreignSdk = false;
    std::thread foreign(
        [&]
        {
            auto next = canary;
            foreignRead = reader.Read(0, next) == Status::WrongThread && std::memcmp(&next, &canary, sizeof(next)) == 0;
            foreignBind = reader.Bind() == Status::WrongThread;
            try
            {
                first.GetWeaponRange();
            }
            catch (const std::runtime_error&)
            {
                foreignSdk = true;
            }
        });
    foreign.join();
    Check(foreignRead && foreignBind && foreignSdk && reader.IsBound(), "wrong-thread native and SDK reads/rebind refuse without losing ownership");
    const auto refuseByte = [&](std::uintptr_t rva)
    {
        const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + rva);
        const auto replacement = static_cast<unsigned char>(original ^ 1);
        fixture.Patch(rva, &replacement, 1);
        output = canary;
        Check(reader.Read(0, output) == Status::Unsupported && std::memcmp(&output, &canary, sizeof(output)) == 0,
              "changed native profile refuses and preserves row output");
        fixture.Patch(rva, &original, 1);
    };
    for (const auto& pin : P::Code)
    {
        refuseByte(pin.rva);
        refuseByte(pin.rva + pin.size - 1);
    }
    for (const auto& pin : P::Literals)
        refuseByte(pin.rva);
    const auto before = WeaponTestHost::reads.load();
    Check(reader.Read(0, output) == Status::Ok, "measure actual complete OS-copy sequence");
    const auto copies = WeaponTestHost::reads.load() - before;
    for (std::size_t failure = 1; failure <= copies; ++failure)
    {
        WeaponTestHost::failAt = WeaponTestHost::reads.load() + failure;
        output = canary;
        Check(reader.Read(0, output) == Status::Unsupported && std::memcmp(&output, &canary, sizeof(output)) == 0,
              "each partial OS copy leaves row output untouched");
        WeaponTestHost::failAt = 0;
    }
    WeaponTestHost::watchedRow = fixture.image + P::BankRva;
    const auto flipAndRestore = [&](std::uintptr_t rva)
    {
        const auto original = *reinterpret_cast<const unsigned char*>(fixture.image + rva);
        WeaponTestHost::flipAddress = fixture.image + rva;
        WeaponTestHost::flipAfterRow = true;
        output = canary;
        Check(reader.Read(0, output) == Status::Unsupported && std::memcmp(&output, &canary, sizeof(output)) == 0,
              "row/code change across bookends cannot publish a mixed row");
        fixture.Patch(rva, &original, 1);
    };
    flipAndRestore(P::BankRva + offsetof(NativeWeaponInfoValues, flags));
    flipAndRestore(P::Code.front().rva);
    DWORD      old = 0, ignored = 0;
    const auto denied = reinterpret_cast<void*>(fixture.image + P::BankRva);
    Check(VirtualProtect(denied, 1, PAGE_NOACCESS, &old) != FALSE, "deny only owned bank pages");
    output = canary;
    Check(reader.Read(0, output) == Status::Unsupported && std::memcmp(&output, &canary, sizeof(output)) == 0,
          "unreadable bank refuses before publishing native values");
    Check(VirtualProtect(denied, 1, old, &ignored) != FALSE, "restore owned bank pages");
    Check(VirtualProtect(denied, 1, PAGE_READONLY, &old) != FALSE, "make only owned native bank readonly");
    Check(reader.Read(0, output) == Status::Unsupported, "bank page characteristics must still match its native writable storage");
    Check(VirtualProtect(denied, 1, old, &ignored) != FALSE, "restore owned writable bank");
    const auto& nt = *reinterpret_cast<const IMAGE_NT_HEADERS64*>(fixture.image + 0x100);
    const auto  mutate = [&](std::uintptr_t rva, const auto& originalValue, const auto& replacement)
    {
        fixture.Patch(rva, &replacement, sizeof(replacement));
        output = canary;
        Check(reader.Read(0, output) == Status::Unsupported && std::memcmp(&output, &canary, sizeof(output)) == 0,
              "invalid PE metadata cannot publish native weapon rows");
        fixture.Patch(rva, &originalValue, sizeof(originalValue));
    };
    const auto& dos = *reinterpret_cast<const IMAGE_DOS_HEADER*>(fixture.image);
    const auto  dosMagic = dos.e_magic;
    mutate(offsetof(IMAGE_DOS_HEADER, e_magic), dosMagic, WORD{0});
    const auto dosOffset = dos.e_lfanew;
    mutate(offsetof(IMAGE_DOS_HEADER, e_lfanew), dosOffset, LONG{-1});
    const auto machine = nt.FileHeader.Machine;
    mutate(0x100 + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, Machine), machine, WORD{IMAGE_FILE_MACHINE_I386});
    const auto sectionCount = nt.FileHeader.NumberOfSections;
    mutate(0x100 + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, NumberOfSections), sectionCount, WORD{97});
    const auto imageSize = nt.OptionalHeader.SizeOfImage;
    mutate(0x100 + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, SizeOfImage), imageSize, DWORD{1});
    const auto selected = WeaponTestHost::image;
    WeaponTestHost::image = 0;
    output = canary;
    Check(reader.Read(0, output) == Status::Unsupported && std::memcmp(&output, &canary, sizeof(output)) == 0, "reader cannot follow a replacement main image");
    WeaponTestHost::image = selected;
    WeaponTestHost::buildAllowed = false;
    Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "failed owner-thread rebind clears old binding");
    WeaponTestHost::buildAllowed = true;
    Check(reader.Bind() == Status::Ok && first.GetWeaponRange() == Row(0).weaponRange, "real reader and SDK remain reusable after restoring owned metadata");
    reader.Reset();
    Refuses([&] { first.GetWeaponRange(); }, "actual SDK getter refuses retired read binding");
    Check(!reader.IsBound() && reader.Read(0, output) == Status::NotInitialized, "explicit reset retires borrowed native binding");
    std::printf("Passed %u native weapon-info reader and actual SDK checks (64-bit)\n", checks);
    return 0;
}
