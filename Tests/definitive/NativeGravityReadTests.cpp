/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned-image gravity evidence and actual partial CGame SDK dispatch
 *****************************************************************************/
#include "../../Client/game_de/NativeGravityReadAccess.h"
#include "../../Client/game_de/NativeGravityReadProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include "../../Client/game_de/CGameTimingDE.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fixture
{
    using namespace mta::game_de;
    namespace Profile = NativeGravityReadProfile;
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
            throw std::runtime_error("native gravity assertion " + std::to_string(checks));
    }
    [[noreturn]] void Unqualified()
    {
        throw std::runtime_error("unqualified test-only SDK method");
    }
    [[noreturn]] void UnqualifiedNoexcept() noexcept
    {
        std::terminate();
    }
    std::uintptr_t owned = 0, failAddress = 0;
    unsigned       scalarReads = 0;
    bool           changeScalar = false, changeCode = false;
    Status         readerStatus = Status::NotInitialized;
    float          readerValue = 0;
    unsigned       readerCalls = 0, timingCalls = 0;

    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    void Write(std::uintptr_t rva, const void* source, std::size_t bytes, DWORD finalProtection)
    {
        DWORD prior = 0, ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(owned + rva), bytes, PAGE_READWRITE, &prior) != FALSE);
        std::memcpy(reinterpret_cast<void*>(owned + rva), source, bytes);
        Check(VirtualProtect(reinterpret_cast<void*>(owned + rva), bytes, finalProtection, &ignored) != FALSE);
    }
    void Scalar(std::uint32_t value)
    {
        Write(Profile::DownwardAccelerationRva, &value, sizeof(value), PAGE_READONLY);
    }
    bool Read(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool executable) noexcept
    {
        if (image != owned || !destination || !bytes || address < owned || address - owned > Profile::ImageSize ||
            bytes > Profile::ImageSize - (address - owned) || address == failAddress)
            return false;
        MEMORY_BASIC_INFORMATION page{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) != sizeof(page) || page.State != MEM_COMMIT ||
            reinterpret_cast<std::uintptr_t>(page.AllocationBase) != owned || page.Protect & (PAGE_GUARD | PAGE_NOACCESS))
            return false;
        const auto protection = page.Protect & 0xff;
        if (executable && protection != PAGE_EXECUTE_READ)
            return false;
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), destination, bytes, &copied) || copied != bytes)
            return false;
        if (address == owned + Profile::DownwardAccelerationRva && ++scalarReads == 1)
        {
            // The controlled callback owns this modeled image; these edits
            // test overlapping data/code changes without executing its bytes.
            try
            {
                if (changeScalar)
                    Scalar(0x3f000000);
                if (changeCode)
                {
                    const unsigned char byte = Profile::Code[0].bytes[0] ^ 1;
                    Write(Profile::Code[0].rva, &byte, sizeof(byte), PAGE_EXECUTE_READ);
                }
            }
            catch (...)
            {
                return false;
            }
        }
        return true;
    }
    bool ShippingRead(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool executable) noexcept
    {
        return NativeImageAccess::ReadImage(image, address, destination, bytes, false, executable);
    }
    class OwnedImage final
    {
    public:
        OwnedImage()
        {
            owned = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, Profile::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(owned != 0);
            auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(owned);
            dos->e_magic = IMAGE_DOS_SIGNATURE;
            dos->e_lfanew = 0x100;
            auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(owned + 0x100);
            nt->Signature = IMAGE_NT_SIGNATURE;
            nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt->FileHeader.NumberOfSections = 2;
            nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(Profile::ImageSize);
            auto* sections = IMAGE_FIRST_SECTION(nt);
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x3f40000;
            sections[0].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            sections[1].VirtualAddress = 0x4000000;
            sections[1].Misc.VirtualSize = 0xf00000;
            sections[1].Characteristics = IMAGE_SCN_MEM_READ;
            for (const auto& pin : Profile::Code)
                Write(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
            Scalar(Profile::PositiveDefaultBits);
            Write(Profile::OrientedAccelerationRva, &Profile::NegativeDefaultBits, sizeof(Profile::NegativeDefaultBits), PAGE_READONLY);
        }
        ~OwnedImage()
        {
            VirtualFree(reinterpret_cast<void*>(owned), 0, MEM_RELEASE);
            owned = 0;
        }
        OwnedImage(const OwnedImage&) = delete;
        OwnedImage& operator=(const OwnedImage&) = delete;
    };
    class TimingOwner final : public NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override
        {
            ++timingCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override
        {
            ++timingCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult Pause(bool) noexcept override
        {
            ++timingCalls;
            return ClockResult::NotInitialized;
        }
    };
}

// Only the reader dependency is controlled in this SDK fixture. The actual
// CGameTimingDE implementation is linked separately and must dispatch once.
namespace mta::game_de
{
    Status NativeGameTimingReader::Read(NativeGameTimingValues&) const noexcept
    {
        return Status::NotInitialized;
    }
    Status NativeGameTimingReader::ReadFPS(float&) const noexcept
    {
        return Status::NotInitialized;
    }
    Status NativeGameTimingReader::ReadGravity(float& output) const noexcept
    {
        ++fixture::readerCalls;
        if (fixture::readerStatus == Status::Ok)
            std::memcpy(&output, &fixture::readerValue, sizeof(output));
        return fixture::readerStatus;
    }
}
#include "GravityTimingFixture.generated.h"

int main()
{
    using namespace fixture;
    try
    {
        static_assert(std::is_abstract_v<CGameTimingDE> && std::is_base_of_v<CGame, CGameTimingDE>);
        float      output = 9;
        const auto host = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        Check(!NativeGravityReadAccess(host, ShippingRead).Read(output) && output == 9);
        Check(!NativeGravityReadAccess(0, Read).Read(output) && output == 9);
        Check(!NativeGravityReadAccess((std::numeric_limits<std::uintptr_t>::max)() - Profile::ImageSize + 1, Read).Read(output) && output == 9);
        Check(!NativeGravityReadAccess(host, nullptr).Read(output) && output == 9);
        {
            OwnedImage                         image;
            const NativeGravityReadAccess      access(owned, Read);
            const std::array<std::uint32_t, 8> finite = {Profile::PositiveDefaultBits, 0, 0x80000000, 0x3f000000, 0xbf000000, 1, 0x7f7fffff, 0xff7fffff};
            for (const auto value : finite)
            {
                Scalar(value);
                scalarReads = 0;
                output = 9;
                Check(access.Read(output) && Bits(output) == value && scalarReads == 2);
            }
            for (const auto value : {0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f800001u})
            {
                Scalar(value);
                scalarReads = 0;
                output = 9;
                Check(!access.Read(output) && output == 9);
            }
            Scalar(Profile::PositiveDefaultBits);
            failAddress = owned + Profile::DownwardAccelerationRva;
            output = 9;
            Check(!access.Read(output) && output == 9);
            failAddress = 0;
            scalarReads = 0;
            changeScalar = true;
            Check(!access.Read(output) && output == 9);
            changeScalar = false;
            Scalar(Profile::PositiveDefaultBits);
            scalarReads = 0;
            changeCode = true;
            Check(!access.Read(output) && output == 9);
            changeCode = false;
            Write(Profile::Code[0].rva, Profile::Code[0].bytes.data(), Profile::Code[0].size, PAGE_EXECUTE_READ);
            for (const auto& pin : Profile::Code)
            {
                for (std::size_t index = 0; index < pin.size; ++index)
                {
                    const unsigned char byte = pin.bytes[index] ^ 1;
                    Write(pin.rva + index, &byte, sizeof(byte), PAGE_EXECUTE_READ);
                    output = 9;
                    Check(!access.Read(output) && output == 9);
                    Write(pin.rva + index, pin.bytes.data() + index, 1, PAGE_EXECUTE_READ);
                }
                failAddress = owned + pin.rva;
                output = 9;
                Check(!access.Read(output) && output == 9);
                failAddress = 0;
            }
            auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(owned + 0x100);
            nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
            Check(!access.Read(output) && output == 9);
            nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt->OptionalHeader.SizeOfImage--;
            Check(!access.Read(output) && output == 9);
            nt->OptionalHeader.SizeOfImage++;
            auto* sections = IMAGE_FIRST_SECTION(nt);
            sections[1].Characteristics |= IMAGE_SCN_MEM_WRITE;
            Check(!access.Read(output) && output == 9);
            sections[1].Characteristics &= ~IMAGE_SCN_MEM_WRITE;
            sections[1].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            Check(!access.Read(output) && output == 9);
            sections[1].Characteristics &= ~IMAGE_SCN_MEM_EXECUTE;
            sections[0].Characteristics |= IMAGE_SCN_MEM_WRITE;
            Check(!access.Read(output) && output == 9);
            sections[0].Characteristics &= ~IMAGE_SCN_MEM_WRITE;
            scalarReads = 0;
            Check(access.Read(output) && Bits(output) == Profile::PositiveDefaultBits);
        }
        NativeGameTimingReader reader;
        TimingOwner            owner;
        TimingFixture          game(reader, owner);
        for (const auto value : {0x3c03126fu, 0x80000000u, 0xbf000000u, 0x7f7fffffu})
        {
            std::memcpy(&readerValue, &value, sizeof(value));
            readerStatus = Status::Ok;
            readerCalls = 0;
            Check(Bits(game.GetGravity()) == value && readerCalls == 1 && timingCalls == 0);
        }
        for (const auto status : {Status::InvalidArgument, Status::WrongThread, Status::NotInitialized, Status::Unsupported})
        {
            readerStatus = status;
            readerCalls = 0;
            bool refused = false;
            try
            {
                (void)game.GetGravity();
            }
            catch (const std::runtime_error&)
            {
                refused = true;
            }
            Check(refused && readerCalls == 1 && timingCalls == 0);
        }
        std::printf("Passed %u native gravity read checks (64-bit; owned-image-and-sdk)\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
