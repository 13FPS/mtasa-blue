/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual player-info primitives/SDK over owned OS images and objects
 ****************************************************************************/
#include "../../Client/game_de/NativePlayerInfoMemory.h"
#include "../../Client/game_de/NativePlayerInfoProfile.h"
#include "../../Client/game_de/NativePlayerInfoExtraProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include "../../Client/game_de/CPlayerInfoDE.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>
#include <intrin.h>

namespace PlayerHost
{
    std::uintptr_t                                   image = 0;
    bool                                             buildAllowed = true, readFault = false, exchangeFault = false;
    unsigned                                         ownerChecks = 0, expiresAt = 0, exchanges = 0;
    std::function<void()>                            beforeExchange, afterExchange;
    std::function<void(std::uintptr_t, std::size_t)> afterRead;
    HMODULE WINAPI                                   Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID source, LPVOID target, SIZE_T bytes, SIZE_T* copied)
    {
        if (readFault)
        {
            readFault = false;
            std::memset(target, 0xa7, bytes);
            if (copied)
                *copied = bytes / 2;
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, source, target, bytes, copied);
        if (result && afterRead)
            afterRead(reinterpret_cast<std::uintptr_t>(source), bytes);
        return result;
    }
    mta::game_de::ClockResult Admit() noexcept
    {
        return ++ownerChecks == expiresAt ? mta::game_de::ClockResult::Expired : mta::game_de::ClockResult::Ok;
    }
    void Before()
    {
        ++exchanges;
        if (exchangeFault)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        if (beforeExchange)
            beforeExchange();
    }
    LONG64 Cas64(volatile LONG64* address, LONG64 value, LONG64 expected)
    {
        Before();
        const auto observed = _InterlockedCompareExchange64(address, value, expected);
        if (afterExchange)
            afterExchange();
        return observed;
    }
    SHORT Cas16(volatile SHORT* address, SHORT value, SHORT expected)
    {
        Before();
        const auto observed = _InterlockedCompareExchange16(address, value, expected);
        if (afterExchange)
            afterExchange();
        return observed;
    }
    char Cas8(volatile char* address, char value, char expected)
    {
        Before();
        const auto observed = _InterlockedCompareExchange8(address, value, expected);
        if (afterExchange)
            afterExchange();
        return observed;
    }
    void Reset()
    {
        ownerChecks = expiresAt = exchanges = 0;
        readFault = exchangeFault = false;
        beforeExchange = afterExchange = {};
        afterRead = {};
    }
}
#define ReadProcessMemory PlayerHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#undef ReadProcessMemory
namespace mta::game_de::NativeImageAccess
{
    bool PlayerFixtureExecutable() noexcept
    {
        return PlayerHost::buildAllowed && (PlayerHost::image || CheckExecutableBuild());
    }
}
#undef InterlockedCompareExchange64
#undef InterlockedCompareExchange16
#define InterlockedCompareExchange64 PlayerHost::Cas64
#define InterlockedCompareExchange16 PlayerHost::Cas16
#define _InterlockedCompareExchange8 PlayerHost::Cas8
#define CheckExecutableBuild         PlayerFixtureExecutable
#define GetModuleHandleW             PlayerHost::Module
#define ReadProcessMemory            PlayerHost::Read
#include "../../Client/game_de/NativePlayerInfoMemory.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
#undef CheckExecutableBuild
#undef _InterlockedCompareExchange8
#undef InterlockedCompareExchange16
#undef InterlockedCompareExchange64
#include "../../Client/game_de/CPlayerInfoDE.cpp"

using namespace mta::game_de;
namespace P = NativePlayerInfoProfile;
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
        Check(false, why);
    }
    class OwnedImage
    {
    public:
        std::uintptr_t image = 0;
        HANDLE         file = INVALID_HANDLE_VALUE, mapping = nullptr;
        ~OwnedImage()
        {
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
            constexpr DWORD bank = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA;
            for (const auto& pin : P::Code)
                seed(pin.rva, pin.bytes.data(), pin.size, code);
            for (const auto& pin : NativePlayerInfoExtraProfile::Code)
                seed(pin.rva, pin.bytes.data(), pin.size, code);
            const std::array<unsigned char, P::RowBytes * 2> rows{};
            seed(P::BankRva, rows.data(), rows.size(), bank);
            const unsigned char index = 0;
            seed(P::PlayerIndexRva, &index, sizeof(index), bank);
            const std::array<std::uintptr_t, 11> vtable{};
            seed(P::PlayerVtableRva, vtable.data(), sizeof(vtable), IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
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
            const auto path = directory / L"owned-player-info-image.bin";
            Check(!std::filesystem::exists(path), "create only unique owned sparse image");
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save small player-info fixture without copying GTA");
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

    struct LoadedPlayer
    {
        void*          allocation = nullptr;
        std::uintptr_t address = 0;
        explicit LoadedPlayer(OwnedImage& host)
        {
            allocation = VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            Check(allocation != nullptr, "allocate only an owned local player fixture");
            address = reinterpret_cast<std::uintptr_t>(allocation);
            const auto vtable = host.image + P::PlayerVtableRva;
            const auto control = host.image + P::PlayerControlRva;
            const auto inlineData = host.image + P::BankRva + 8;
            std::memcpy(allocation, &vtable, sizeof(vtable));
            std::memcpy(static_cast<unsigned char*>(allocation) + 0x5f0, &inlineData, sizeof(inlineData));
            host.Patch(P::PlayerVtableRva + 0x50, &control, sizeof(control));
            host.Patch(P::BankRva, &address, sizeof(address));
        }
        ~LoadedPlayer()
        {
            if (allocation)
                VirtualFree(allocation, 0, MEM_RELEASE);
        }
    };
    template <class T>
    T& Field(std::uintptr_t image, std::size_t offset)
    {
        return *reinterpret_cast<T*>(image + P::BankRva + offset);
    }
    class Access final : public NativePlayerInfoAccess
    {
    public:
        NativePlayerInfoMemory& memory;
        explicit Access(NativePlayerInfoMemory& value) : memory(value) {}
        ClockResult ReadPlayerMoney(std::int32_t& value) noexcept override { return memory.ReadMoney(value, PlayerHost::Admit); }
        ClockResult SetPlayerMoney(std::int32_t value, bool instant) noexcept override { return memory.SetMoney(value, instant, PlayerHost::Admit); }
        ClockResult SetPlayerLastTimeEaten(std::int16_t value) noexcept override { return memory.SetLastTimeEaten(value, PlayerHost::Admit); }
        ClockResult SetPlayerDoesNotGetTired(bool value) noexcept override { return memory.SetDoesNotGetTired(value, PlayerHost::Admit); }
        ClockResult ReadPlayerFloat(NativePlayerInfoFloatField field, float& value) noexcept override
        {
            return memory.ReadFloat(field, value, PlayerHost::Admit);
        }
        ClockResult ReadPlayerDword(NativePlayerInfoDwordField field, std::uint32_t& value) noexcept override
        {
            return memory.ReadDword(field, value, PlayerHost::Admit);
        }
        ClockResult ReadPlayerByte(NativePlayerInfoByteField field, std::uint8_t& value) noexcept override
        {
            return memory.ReadByte(field, value, PlayerHost::Admit);
        }
        ClockResult SetPlayerCamDrunkLevel(std::uint8_t value) noexcept override { return memory.SetCamDrunkLevel(value, PlayerHost::Admit); }
    };
    using TestPlayer = CPlayerInfoDE;
    class DefaultAccess final : public NativePlayerInfoAccess
    {
    };
    void Completion(NativePlayerInfoMemory& memory, OwnedImage& host, CPlayerInfo& sdk)
    {
        // Literal native offsets here come from independent producer/consumer
        // evidence, so the fixture can catch a switched SDK/typed field map.
        struct FloatCase
        {
            NativePlayerInfoFloatField field;
            std::size_t                offset;
            float (CPlayerInfo::*get)();
        };
        const std::array<FloatCase, 4> floats{{{NativePlayerInfoFloatField::FpsMoveHeading, 0x70, &CPlayerInfo::GetFPSMoveHeading},
                                               {NativePlayerInfoFloatField::CarTwoWheelDistance, 0x130, &CPlayerInfo::GetCarTwoWheelDist},
                                               {NativePlayerInfoFloatField::BikeRearWheelDistance, 0x13c, &CPlayerInfo::GetBikeRearWheelDist},
                                               {NativePlayerInfoFloatField::BikeFrontWheelDistance, 0x144, &CPlayerInfo::GetBikeFrontWheelDist}}};
        const auto                     bitsOf = [](float value)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        };
        for (const auto& field : floats)
            for (auto bits : std::array<std::uint32_t, 7>{0, 0x80000000, 0x3f800001, 0xbf654321, 0x7f800000, 0xff800000, 0x7fc12345})
            {
                Field<std::uint32_t>(host.image, field.offset) = bits;
                float value = 123;
                PlayerHost::Reset();
                Check(memory.ReadFloat(field.field, value, PlayerHost::Admit) == ClockResult::Ok && bitsOf(value) == bits,
                      "typed float preserves the exact native field bits");
                Check(bitsOf((sdk.*field.get)()) == bits, "actual SDK float getter reaches its independently seeded member");
            }
        struct DwordCase
        {
            NativePlayerInfoDwordField field;
            std::size_t                offset;
            DWORD (CPlayerInfo::*get)();
        };
        const std::array<DwordCase, 4> dwords{{{NativePlayerInfoDwordField::CarTwoWheelCounter, 0x12c, &CPlayerInfo::GetCarTwoWheelCounter},
                                               {NativePlayerInfoDwordField::CarLess3WheelCounter, 0x134, &CPlayerInfo::GetCarLess3WheelCounter},
                                               {NativePlayerInfoDwordField::BikeRearWheelCounter, 0x138, &CPlayerInfo::GetBikeRearWheelCounter},
                                               {NativePlayerInfoDwordField::BikeFrontWheelCounter, 0x140, &CPlayerInfo::GetBikeFrontWheelCounter}}};
        for (const auto& field : dwords)
            for (auto bits : std::array<std::uint32_t, 5>{0, 1, 0x7fffffff, 0x80000000, 0xffffffff})
            {
                Field<std::uint32_t>(host.image, field.offset) = bits;
                std::uint32_t value = 123;
                PlayerHost::Reset();
                Check(memory.ReadDword(field.field, value, PlayerHost::Admit) == ClockResult::Ok && value == bits,
                      "typed wheel counter preserves full unsigned32 domain");
                Check((sdk.*field.get)() == bits, "actual SDK wheel counter getter reaches its distinct native member");
            }
        for (unsigned value = 0; value != 256; ++value)
        {
            PlayerHost::Reset();
            Field<unsigned char>(host.image, 0x5f) = 0xa5;
            Field<unsigned char>(host.image, 0x62) = 0x5a;
            Check(memory.SetCamDrunkLevel(static_cast<std::uint8_t>(value), PlayerHost::Admit) == ClockResult::Ok && PlayerHost::exchanges == 1,
                  "all actual byte drunk levels use one exact aligned16 exchange");
            Check(Field<unsigned char>(host.image, 0x60) == value && Field<unsigned char>(host.image, 0x61) == (value == 0 ? 1 : 0) &&
                      Field<unsigned char>(host.image, 0x5f) == 0xa5 && Field<unsigned char>(host.image, 0x62) == 0x5a,
                  "MTA zero-level fade policy preserves adjacent native data");
            Check(sdk.GetCamDrunkLevel() == value, "actual SDK drunk getter observes live byte");
            sdk.SetCamDrunkLevel(static_cast<byte>(255 - value));
            Check(Field<unsigned char>(host.image, 0x60) == 255 - value && Field<unsigned char>(host.image, 0x61) == (value == 255 ? 1 : 0),
                  "actual SDK drunk setter preserves existing MTA fade semantics");
            Field<unsigned char>(host.image, 0x183) = static_cast<unsigned char>(value);
            Field<unsigned char>(host.image, 0x184) = static_cast<unsigned char>(255 - value);
            Field<unsigned char>(host.image, 0x187) = static_cast<unsigned char>(value);
            Check(sdk.GetMaxHealth() == value && sdk.GetMaxArmor() == 255 - value && sdk.CanDoDriveBy() == (value != 0),
                  "all raw ability bytes are freshly exposed with native nonzero driveby meaning");
        }
        PlayerHost::Reset();
        std::uint32_t dword = 123;
        std::uint8_t  byteValue = 123;
        float         floatValue = 123;
        Check(memory.ReadDword(static_cast<NativePlayerInfoDwordField>(0xffffffff), dword, PlayerHost::Admit) == ClockResult::InvalidArgument && dword == 123,
              "malformed DWORD field refuses without output mutation");
        Check(memory.ReadByte(static_cast<NativePlayerInfoByteField>(0xffffffff), byteValue, PlayerHost::Admit) == ClockResult::InvalidArgument &&
                  byteValue == 123,
              "malformed byte field refuses without output mutation");
        Check(memory.ReadFloat(static_cast<NativePlayerInfoFloatField>(0xffffffff), floatValue, PlayerHost::Admit) == ClockResult::InvalidArgument &&
                  floatValue == 123,
              "malformed float field refuses without output mutation");
        Check(PlayerHost::ownerChecks == 0, "invalid enum values never request native authority");
        Field<std::uint32_t>(host.image, 0x12c) = 100;
        bool changed = false;
        PlayerHost::afterRead = [&](std::uintptr_t address, std::size_t size)
        {
            if (!changed && address == host.image + P::BankRva + 0x12c && size == 4)
            {
                changed = true;
                Field<std::uint32_t>(host.image, 0x12c) = 101;
            }
        };
        Check(memory.ReadDword(NativePlayerInfoDwordField::CarTwoWheelCounter, dword, PlayerHost::Admit) == ClockResult::OwnerConflict && dword == 123,
              "foreign native producer between reads is reported instead of cached");
        PlayerHost::Reset();
        Field<std::uint16_t>(host.image, 0x60) = 0x100;
        PlayerHost::beforeExchange = [&] { Field<std::uint16_t>(host.image, 0x60) = 0x255; };
        Check(memory.SetCamDrunkLevel(42, PlayerHost::Admit) == ClockResult::OwnerConflict && Field<std::uint16_t>(host.image, 0x60) == 0x255,
              "drunk/fade foreign pair survives a stale CAS");
        PlayerHost::Reset();
        PlayerHost::expiresAt = 3;
        Check(memory.SetCamDrunkLevel(42, PlayerHost::Admit) == ClockResult::Expired && Field<std::uint16_t>(host.image, 0x60) == 42,
              "owner loss after drunk CAS reports persistent accepted effect");
        PlayerHost::Reset();
        for (const auto& pin : NativePlayerInfoExtraProfile::Code)
        {
            const auto changedCode = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            host.Patch(pin.rva, &changedCode, 1);
            Check(memory.ReadByte(NativePlayerInfoByteField::MaxArmor, byteValue, PlayerHost::Admit) == ClockResult::BindingChanged && byteValue == 123,
                  "all independently qualified extra field pins participate in admission");
            host.Patch(pin.rva, pin.bytes.data(), 1);
        }
        DefaultAccess defaults;
        CPlayerInfoDE missing(defaults);
        CPlayerInfo&  refused = missing;
        for (const auto& field : floats)
            Refuses([&] { (refused.*field.get)(); }, "missing native float read never returns a fabricated zero");
        for (const auto& field : dwords)
            Refuses([&] { (refused.*field.get)(); }, "missing native counter read never returns a fabricated zero");
        Refuses([&] { refused.GetCamDrunkLevel(); }, "missing native drunkenness read refuses");
        Refuses([&] { refused.SetCamDrunkLevel(0); }, "missing native drunkenness setter refuses");
        Refuses([&] { refused.GetMaxHealth(); }, "missing native max health refuses");
        Refuses([&] { refused.GetMaxArmor(); }, "missing native max armor refuses");
        Refuses([&] { refused.CanDoDriveBy(); }, "missing native driveby ability refuses");
        PlayerHost::Reset();
    }
    void Exercise(OwnedImage& host)
    {
        LoadedPlayer           player(host);
        NativePlayerInfoMemory memory;
        PlayerHost::image = host.image;
        std::int32_t output = 0x12345678;
        Check(memory.ReadMoney(output, PlayerHost::Admit) == ClockResult::NotInitialized && output == 0x12345678, "unbound read preserves caller output");
        Check(memory.Bind() == ClockResult::Ok && memory.IsBound(), "bind exact owned profile once on its thread");
        NativePlayerInfoMemory::Identity identity{123, 456};
        Check(memory.ReadIdentity(identity, PlayerHost::Admit) == ClockResult::Ok && identity.image == host.image && identity.ped == player.address,
              "dependent services borrow the same independently checked player identity");
        identity = {123, 456};
        Check(memory.ReadIdentity(identity, nullptr) == ClockResult::NotInitialized && identity.image == 123 && identity.ped == 456,
              "identity snapshot does not grant a native lease or alter output on refusal");
        auto& money = Field<std::int32_t>(host.image, P::MoneyOffset);
        auto& display = Field<std::int32_t>(host.image, P::DisplayMoneyOffset);
        auto& eaten = Field<std::int16_t>(host.image, P::LastTimeEatenOffset);
        auto& tired = Field<unsigned char>(host.image, P::DoesNotGetTiredOffset);
        for (auto value : std::array<std::int32_t, 5>{INT32_MIN, -1, 0, 1, INT32_MAX})
        {
            PlayerHost::Reset();
            display = -7654321;
            Check(memory.SetMoney(value, false, PlayerHost::Admit) == ClockResult::Ok && money == value && display == -7654321,
                  "noninstant exact signed32 preserves displayed bits");
            Check(memory.ReadMoney(output, PlayerHost::Admit) == ClockResult::Ok && output == value, "actual balance getter never returns smoothed display");
            Check(memory.SetMoney(value, true, PlayerHost::Admit) == ClockResult::Ok && money == value && display == value,
                  "instant one aligned64 exchange publishes both signed32 members");
        }
        std::array<unsigned char, P::RowBytes * 2> snapshot{};
        std::memcpy(snapshot.data(), reinterpret_cast<void*>(host.image + P::BankRva), snapshot.size());
        for (auto value : std::array<std::int16_t, 5>{INT16_MIN, -1, 0, 1, INT16_MAX})
        {
            PlayerHost::Reset();
            Check(memory.SetLastTimeEaten(static_cast<std::int16_t>(value), PlayerHost::Admit) == ClockResult::Ok && eaten == value,
                  "signed16 food intent is not clamped or widened");
        }
        for (unsigned old : {0u, 1u, 2u, 255u})
            for (bool value : {false, true})
            {
                tired = static_cast<unsigned char>(old);
                PlayerHost::Reset();
                Check(memory.SetDoesNotGetTired(value, PlayerHost::Admit) == ClockResult::Ok && tired == (value ? 1 : 0),
                      "native ability old noncanonical bytes do not fabricate bool refusal");
            }
        for (std::size_t i = 0; i < snapshot.size(); ++i)
            if (i != P::LastTimeEatenOffset && i != P::LastTimeEatenOffset + 1 && i != P::DoesNotGetTiredOffset)
                Check(snapshot[i] == Field<unsigned char>(host.image, i), "food/ability writes preserve all other row bytes and neighboring player");
        PlayerHost::Reset();
        money = 100;
        display = 200;
        PlayerHost::beforeExchange = [&] { money = 101; };
        Check(memory.SetMoney(999, true, PlayerHost::Admit) == ClockResult::OwnerConflict && money == 101 && display == 200 && PlayerHost::exchanges == 1,
              "pre-exchange foreign producer survives without retry");
        PlayerHost::Reset();
        money = 100;
        display = 200;
        PlayerHost::beforeExchange = [&] { display = 201; };
        Check(memory.SetMoney(999, false, PlayerHost::Admit) == ClockResult::OwnerConflict && money == 100 && display == 201,
              "noninstant CAS refuses stale display instead of replacing native smoothing");
        PlayerHost::Reset();
        PlayerHost::afterExchange = [&] { money = 123; };
        Check(memory.SetMoney(999, true, PlayerHost::Admit) == ClockResult::OwnerConflict && money == 123 && display == 999,
              "post-exchange foreign producer is preserved with honest partial native effects");
        PlayerHost::Reset();
        money = 100;
        display = 200;
        PlayerHost::expiresAt = 2;
        Check(memory.SetMoney(999, true, PlayerHost::Admit) == ClockResult::Expired && money == 100 && display == 200 && PlayerHost::exchanges == 0,
              "owner loss immediately before CAS prevents mutation");
        PlayerHost::Reset();
        PlayerHost::expiresAt = 3;
        Check(memory.SetMoney(999, true, PlayerHost::Admit) == ClockResult::Expired && money == 999 && display == 999,
              "owner loss after CAS leaves persistent effects and reports failure");
        PlayerHost::Reset();
        PlayerHost::exchangeFault = true;
        eaten = 17;
        Check(memory.SetLastTimeEaten(0, PlayerHost::Admit) == ClockResult::WriteFailure && eaten == 17, "CAS protection race exception refuses safely");
        PlayerHost::Reset();
        PlayerHost::afterExchange = [&] { PlayerHost::readFault = true; };
        Check(memory.SetLastTimeEaten(-9, PlayerHost::Admit) == ClockResult::ReadFailure && eaten == -9,
              "failed post-write copy does not snapshot-restore accepted intent");
        PlayerHost::Reset();
        Check(memory.SetDoesNotGetTired(true, nullptr) == ClockResult::NotInitialized && PlayerHost::exchanges == 0,
              "a bound native row alone does not supply owner authority");
        output = 0x12345678;
        PlayerHost::readFault = true;
        Check(memory.ReadMoney(output, PlayerHost::Admit) != ClockResult::Ok && output == 0x12345678, "partial native read preserves caller output");
        PlayerHost::Reset();
        PlayerHost::expiresAt = 2;
        output = 0x12345678;
        Check(memory.ReadMoney(output, PlayerHost::Admit) == ClockResult::Expired && output == 0x12345678,
              "final read admission failure preserves caller output");
        PlayerHost::Reset();
        const std::uintptr_t zero = 0;
        host.Patch(P::BankRva, &zero, sizeof(zero));
        Check(memory.SetMoney(1, true, PlayerHost::Admit) == ClockResult::NotInitialized && PlayerHost::exchanges == 0,
              "fixed bank address cannot manufacture a loaded player");
        host.Patch(P::BankRva, &player.address, sizeof(player.address));
        unsigned char index = 1;
        host.Patch(P::PlayerIndexRva, &index, 1);
        Check(memory.SetLastTimeEaten(0, PlayerHost::Admit) == ClockResult::BindingChanged, "unknown alternate player-index route refuses");
        index = 0;
        host.Patch(P::PlayerIndexRva, &index, 1);
        auto*      inlineLink = reinterpret_cast<std::uintptr_t*>(player.address + 0x5f0);
        const auto correctLink = *inlineLink;
        *inlineLink += 8;
        Check(memory.SetDoesNotGetTired(true, PlayerHost::Admit) == ClockResult::BindingChanged, "actor inline-data identity mismatch refuses");
        *inlineLink = correctLink;
        auto* selector = reinterpret_cast<std::int32_t*>(player.address + 0x7e0);
        *selector = 1;
        Check(memory.SetMoney(1, false, PlayerHost::Admit) == ClockResult::BindingChanged, "actor's independent player selector must match zero");
        *selector = 0;
        const auto correctControl = host.image + P::PlayerControlRva;
        host.Patch(P::PlayerVtableRva + 0x50, &zero, sizeof(zero));
        Check(memory.SetMoney(1, true, PlayerHost::Admit) == ClockResult::BindingChanged, "exact player control vtable entry is rechecked");
        host.Patch(P::PlayerVtableRva + 0x50, &correctControl, sizeof(correctControl));
        PlayerHost::Reset();
        money = 100;
        display = 200;
        const auto oldHeader = *reinterpret_cast<IMAGE_NT_HEADERS64*>(host.image + 0x100);
        auto       changedHeader = oldHeader;
        changedHeader.FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
        PlayerHost::afterExchange = [&] { host.Patch(0x100, &changedHeader, sizeof(changedHeader)); };
        Check(memory.SetMoney(999, true, PlayerHost::Admit) == ClockResult::BindingChanged && money == 999 && display == 999,
              "post-exchange metadata loss is an honest failure without rollback");
        host.Patch(0x100, &oldHeader, sizeof(oldHeader));
        PlayerHost::Reset();
        DWORD protection = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(host.image + P::BankRva), P::RowBytes, PAGE_READONLY, &protection) != FALSE,
              "protect only owned player bank");
        Check(memory.SetMoney(123, true, PlayerHost::Admit) == ClockResult::BindingChanged && money == 999 && display == 999,
              "read-only bank cannot pass native write admission");
        Check(VirtualProtect(reinterpret_cast<void*>(host.image + P::BankRva), P::RowBytes, protection, &ignored) != FALSE, "restore owned bank protection");
        const auto correctImage = PlayerHost::image;
        PlayerHost::image = 0;
        Check(memory.SetMoney(123, true, PlayerHost::Admit) == ClockResult::BindingChanged, "module replacement refuses stale binding");
        PlayerHost::image = correctImage;
        for (const auto& pin : P::Code)
        {
            auto changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            host.Patch(pin.rva, &changed, 1);
            Check(memory.SetMoney(1, true, PlayerHost::Admit) == ClockResult::BindingChanged, "each actual producer/consumer pin participates in admission");
            host.Patch(pin.rva, pin.bytes.data(), 1);
        }
        ClockResult other = ClockResult::Ok;
        std::thread thread(
            [&]
            {
                std::int32_t value = 123;
                other = memory.ReadMoney(value, PlayerHost::Admit);
                Check(value == 123, "wrong-thread read output preserved");
            });
        thread.join();
        Check(other == ClockResult::WrongThread, "read binding cannot be transferred to another thread");
        PlayerHost::Reset();
        Access       access(memory);
        TestPlayer   object(access);
        CPlayerInfo& sdk = object;
        for (long value : {LONG_MIN, -1L, 0L, 1L, LONG_MAX})
        {
            sdk.SetPlayerMoney(value, true);
            Check(sdk.GetPlayerMoney() == value && money == value && display == value, "actual MTA SDK money methods reach real native primitive");
        }
        sdk.SetLastTimeEaten(-123);
        sdk.SetDoesNotGetTired(true);
        Check(eaten == -123 && tired == 1, "actual startup SDK food and stamina setters reach exact fields");
        DefaultAccess refuses;
        TestPlayer    unsupported(refuses);
        CPlayerInfo&  partial = unsupported;
        Refuses([&] { partial.GetPlayerMoney(); }, "default access never converts missing money to zero");
        Refuses([&] { partial.SetPlayerMoney(123, true); }, "default owner refuses money");
        Refuses([&] { partial.SetLastTimeEaten(0); }, "default owner refuses food");
        Refuses([&] { partial.SetDoesNotGetTired(true); }, "default owner refuses stamina");
        Refuses([&] { partial.GetWanted(); }, "default owner refuses publication of an absent wanted service");
        CWanted* wanted = reinterpret_cast<CWanted*>(std::uintptr_t{123});
        Check(refuses.GetWanted(wanted) == ClockResult::NotInitialized && wanted == reinterpret_cast<CWanted*>(std::uintptr_t{123}),
              "default wanted access preserves caller pointer output");
        Completion(memory, host, sdk);
        money = 271;
        display = 271;
        eaten = 13;
        tired = 1;
        memory.Reset();
        Check(!memory.IsBound() && money == 271 && display == 271 && eaten == 13 && tired == 1,
              "safe retirement resets binding without undoing persistent native intent");
        PlayerHost::buildAllowed = false;
        Check(memory.Bind() == ClockResult::UnsupportedBuild && !memory.IsBound(), "new generation checks supported executable identity");
        PlayerHost::buildAllowed = true;
    }
}
int wmain(int argc, wchar_t** argv)
{
    Check(argc == 2, "explicit workspace-owned fixture output directory");
    OwnedImage host;
    host.Create(argv[1]);
    Exercise(host);
    std::printf("Passed %u native player-info and actual SDK checks (64-bit)\n", checks);
    return 0;
}
