/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual stat-bank reader and CStats getters over owned Windows images
 *****************************************************************************/
#include "../../Client/game_de/NativeStatsReader.h"
#include "../../Client/game_de/NativeStatsProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include "../../Client/game_de/CStatsReadDE.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

namespace StatsTestHost
{
    std::uintptr_t           image = 0, watched = 0;
    bool                     buildAllowed = true, flipScalar = false, changeCode = false;
    std::uintptr_t           codeToChange = 0;
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
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, source, output, bytes, copied);
        if (result && reinterpret_cast<std::uintptr_t>(source) == watched && bytes == sizeof(std::uint32_t))
        {
            if (flipScalar)
            {
                flipScalar = false;
                *reinterpret_cast<std::uint32_t*>(watched) ^= 1;
            }
            if (changeCode)
            {
                changeCode = false;
                DWORD old = 0, ignored = 0;
                if (!VirtualProtect(reinterpret_cast<void*>(codeToChange), 1, PAGE_READWRITE, &old))
                    std::abort();
                *reinterpret_cast<unsigned char*>(codeToChange) ^= 1;
                if (!VirtualProtect(reinterpret_cast<void*>(codeToChange), 1, old, &ignored))
                    std::abort();
            }
        }
        return result;
    }
}
#define ReadProcessMemory StatsTestHost::Read
#include "../../Client/game_de/NativeImageAccess.cpp"
#undef ReadProcessMemory
namespace mta::game_de::NativeImageAccess
{
    bool StatsFixtureExecutable() noexcept
    {
        return StatsTestHost::buildAllowed && (StatsTestHost::image || CheckExecutableBuild());
    }
}
#define CheckExecutableBuild StatsFixtureExecutable
#define GetModuleHandleW     StatsTestHost::Module
#include "../../Client/game_de/NativeStatsReader.cpp"
#undef GetModuleHandleW
#undef CheckExecutableBuild
#include "../../Client/game_de/CStatsReadDE.cpp"

using namespace mta::game_de;
namespace P = NativeStatsProfile;
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
    float FloatValue(std::size_t index)
    {
        return static_cast<float>(index) * 1.25f - 10.5f;
    }
    std::int32_t IntValue(std::size_t index)
    {
        if (index == 0)
            return (std::numeric_limits<std::int32_t>::min)();
        if (index == P::IntegerStorageCount - 2)
            return (std::numeric_limits<std::int32_t>::max)();
        return (index % 2 ? 1 : -1) * (16777217 + static_cast<std::int32_t>(index));
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
            for (std::size_t index = 0; index < P::FloatCount; ++index)
            {
                const auto value = FloatValue(index);
                seed(P::FloatBankRva + index * sizeof(value), &value, sizeof(value), bank);
            }
            for (std::size_t index = 0; index < P::IntegerStorageCount; ++index)
            {
                const auto value = IntValue(index);
                seed(P::IntegerBankRva + index * sizeof(value), &value, sizeof(value), bank);
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
            const auto path = directory / L"owned-stats-image.bin";
            Check(!std::filesystem::exists(path), "create only unique owned sparse image");
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save small stat fixture without copying GTA");
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

    class TestStats final : public CStatsReadDE
    {
    public:
        explicit TestStats(const NativeStatsReader& reader) : CStatsReadDE(reader) {}
        void ModifyStat(unsigned short, float) override
        {
            std::fputs("FAIL: unqualified native stat reaction reached readonly fixture\n", stderr);
            std::abort();
        }
        void SetStatValue(unsigned short, float) override
        {
            std::fputs("FAIL: unqualified native stat mutation reached readonly fixture\n", stderr);
            std::abort();
        }
    };
    bool FloatBits(float value, std::uint32_t bits)
    {
        std::uint32_t observed = 0;
        std::memcpy(&observed, &value, sizeof(observed));
        return observed == bits;
    }
    void ActualSdk(NativeStatsReader& reader, std::uintptr_t image)
    {
        TestStats object(reader);
        CStats&   sdk = object;
        for (unsigned short index = 0; index < P::FloatCount; ++index)
            Check(sdk.GetStatValue(index) == FloatValue(index), "actual CStats getter reads each of eighty-two native floating stats");
        for (unsigned short index = P::IntegerFirst; index < P::IntegerSdkEnd; ++index)
            Check(sdk.GetStatValue(index) == static_cast<float>(IntValue(index - P::IntegerFirst)),
                  "actual CStats getter preserves every signed integer-to-float conversion including endpoints");
        for (unsigned index = 0; index <= UINT16_MAX; ++index)
        {
            if (index < P::FloatCount || (index >= P::IntegerFirst && index < P::IntegerSdkEnd))
                continue;
            float output = 731.25f;
            Check(reader.Read(static_cast<unsigned short>(index), output) == Status::InvalidArgument && output == 731.25f,
                  "entire unsigned-short gap/sentinel domain refuses without publishing aliased storage");
        }
        for (const auto index : {std::uint16_t{82}, std::uint16_t{119}, std::uint16_t{343}, std::uint16_t{65535}})
            Refuses([&] { sdk.GetStatValue(index); }, "actual SDK refuses unqualified/gap statistic IDs");
        constexpr unsigned short expected[] = {69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 75};
        for (int type = 0; type != 256; ++type)
        {
            const auto index = sdk.GetSkillStatIndex(static_cast<eWeaponType>(type));
            const auto wanted = type >= 22 && type <= 32 ? expected[type - 22] : UINT16_MAX;
            Check(index == wanted, "actual SDK skill classifier preserves native weapon domain and shared Tec9/Uzi stat");
        }
        for (const auto type : {(std::numeric_limits<int>::min)(), -1, (std::numeric_limits<int>::max)()})
            Check(sdk.GetSkillStatIndex(static_cast<eWeaponType>(type)) == UINT16_MAX,
                  "malformed weapon types preserve native classifier's ushort failure sentinel");
        const auto  address = image + P::FloatBankRva + 10 * sizeof(float);
        const auto  original = FloatValue(10);
        const float changed = 912.375f;
        std::memcpy(reinterpret_cast<void*>(address), &changed, sizeof(changed));
        Check(sdk.GetStatValue(10) == changed, "SDK stat reads observe fresh native updates instead of a cached table");
        for (const std::uint32_t bits : {UINT32_C(0x80000000), UINT32_C(0x7fc12345), UINT32_C(0x7f800000), UINT32_C(0xff800000)})
        {
            std::memcpy(reinterpret_cast<void*>(address), &bits, sizeof(bits));
            Check(FloatBits(sdk.GetStatValue(10), bits), "native floating stat observations preserve exact special-value bits");
        }
        std::memcpy(reinterpret_cast<void*>(address), &original, sizeof(original));
    }

    void ChangedAndPartial(NativeStatsReader& reader, OwnedImage& owned)
    {
        const auto image = owned.image;
        const auto address = image + P::FloatBankRva + 5 * sizeof(float);
        float      output = 1201.5f;
        StatsTestHost::watched = address;
        StatsTestHost::flipScalar = true;
        Check(reader.Read(5, output) == Status::Unsupported && output == 1201.5f, "changed native scalar between matching-copy bookends refuses publication");
        const auto original = FloatValue(5);
        std::memcpy(reinterpret_cast<void*>(address), &original, sizeof(original));
        StatsTestHost::failAt.store(StatsTestHost::reads.load() + 1);
        Check(reader.Read(5, output) == Status::Unsupported && output == 1201.5f, "failed partial OS qualification copy preserves caller output");
        StatsTestHost::failAt.store(0);
        const auto& pin = P::Code.front();
        StatsTestHost::codeToChange = image + pin.rva;
        StatsTestHost::changeCode = true;
        Check(reader.Read(5, output) == Status::Unsupported && output == 1201.5f,
              "native provenance changing after scalar copy invalidates the completed observation");
        owned.Patch(pin.rva, pin.bytes.data(), 1);
        StatsTestHost::watched = 0;
        Check(reader.Read(5, output) == Status::Ok && output == original, "reader can observe the restored original qualified binding");
        for (const auto& code : P::Code)
        {
            const unsigned char corrupt = static_cast<unsigned char>(code.bytes[0] ^ 1);
            owned.Patch(code.rva, &corrupt, 1);
            output = 1201.5f;
            Check(reader.Read(5, output) == Status::Unsupported && output == 1201.5f, "each loader/getter/classifier/consumer profile block is required");
            unsigned short skill = 4000;
            Check(reader.ReadSkillIndex(WEAPONTYPE_PISTOL, skill) == Status::Unsupported && skill == 4000,
                  "skill policy also refuses changed supported-image semantics");
            owned.Patch(code.rva, code.bytes.data(), 1);
        }
        DWORD old = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(image + P::FloatBankRva), sizeof(float), PAGE_READONLY, &old) != FALSE,
              "protect only owned native stat page");
        output = 1201.5f;
        Check(reader.Read(0, output) == Status::Unsupported && output == 1201.5f, "read-only bank cannot impersonate actual writable native stat storage");
        Check(VirtualProtect(reinterpret_cast<void*>(image + P::FloatBankRva), sizeof(float), old, &ignored) != FALSE, "restore only owned stat page");
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + 0x100);
        auto* sections =
            reinterpret_cast<IMAGE_SECTION_HEADER*>(image + 0x100 + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt->FileHeader.SizeOfOptionalHeader);
        bool checkedBankSection = false;
        for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index)
        {
            if (!NativeImageAccess::SectionContains(sections[index], P::IntegerBankRva, P::IntegerStorageCount * sizeof(std::int32_t),
                                                    IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE))
                continue;
            const auto flags = sections[index].Characteristics;
            const auto changed = flags & ~IMAGE_SCN_MEM_WRITE;
            const auto rva = reinterpret_cast<std::uintptr_t>(&sections[index].Characteristics) - image;
            owned.Patch(rva, &changed, sizeof(changed));
            Check(reader.Read(120, output) == Status::Unsupported, "bank section metadata is required as well as writable pages");
            owned.Patch(rva, &flags, sizeof(flags));
            checkedBankSection = true;
            break;
        }
        Check(checkedBankSection, "metadata refusal actually exercised the genuine owned integer-bank section");
    }

    void Lifetime(NativeStatsReader& reader, std::uintptr_t image)
    {
        float          output = 1199.25f;
        unsigned short skill = 3999;
        Status         read = Status::Ok, bind = Status::Ok, policy = Status::Ok;
        std::thread    worker(
            [&]
            {
                read = reader.Read(0, output);
                policy = reader.ReadSkillIndex(WEAPONTYPE_PISTOL, skill);
                bind = reader.Bind();
            });
        worker.join();
        Check(read == Status::WrongThread && policy == Status::WrongThread && bind == Status::WrongThread,
              "reader and actual policy share their established native owner thread");
        Check(output == 1199.25f && skill == 3999 && reader.IsBound(), "wrong-thread calls preserve outputs and the original binding");
        StatsTestHost::image = 0;
        Check(reader.Read(0, output) == Status::Unsupported && output == 1199.25f, "replacement current module cannot reuse an earlier bank");
        StatsTestHost::image = image;
        reader.Reset();
        Check(
            !reader.IsBound() && reader.Read(0, output) == Status::NotInitialized && reader.ReadSkillIndex(WEAPONTYPE_PISTOL, skill) == Status::NotInitialized,
            "retired stat reader refuses scalar and classifier operations");
        Check(output == 1199.25f && skill == 3999, "retired observations never publish stale data");
        StatsTestHost::buildAllowed = false;
        Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "unsupported executable cannot bind native stat semantics");
        StatsTestHost::buildAllowed = true;
        Check(reader.Bind() == Status::Ok, "supported complete stat image can qualify a later owner generation");
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    NativeStatsReader reader;
    float             untouched = 100.5f;
    Check(reader.Read(0, untouched) == Status::NotInitialized && untouched == 100.5f, "ordinary unbound host cannot claim native statistics");
    Check(reader.Bind() == Status::Unsupported, "ordinary executable is not the supported game image");
    OwnedImage owned;
    owned.Create(argv[1]);
    StatsTestHost::image = owned.image;
    constexpr auto                       bankBytes = P::IntegerBankRva + P::IntegerStorageCount * sizeof(std::uint32_t) - P::FloatBankRva;
    std::array<unsigned char, bankBytes> original{};
    std::memcpy(original.data(), reinterpret_cast<const void*>(owned.image + P::FloatBankRva), original.size());
    Check(reader.Bind() == Status::Ok, "real production reader qualifies both full native banks and all semantic pins");
    ActualSdk(reader, owned.image);
    ChangedAndPartial(reader, owned);
    Lifetime(reader, owned.image);
    Check(!std::memcmp(original.data(), reinterpret_cast<const void*>(owned.image + P::FloatBankRva), original.size()),
          "readonly SDK and refusal tests leave every bank and padding byte intact");
    StatsTestHost::image = 0;
    std::printf("Passed %u native stat-bank and actual SDK checks (64-bit)\n", checks);
    return 0;
}
