/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual first-hash bank lookup over owned image and metadata pages
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeModelHashHistoricalBankMain
#include "NativeModelBankTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelHashReader.h"
#include "../../Client/game_de/NativeModelHashProfile.h"
namespace HashReadHost
{
    std::uintptr_t failAddress = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (reinterpret_cast<std::uintptr_t>(input) == failAddress)
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        return PoolTestHost::Read(process, input, output, bytes, copied);
    }
}
#define ReadProcessMemory HashReadHost::Read
#include "../../Client/game_de/NativeModelHashReader.cpp"
#undef ReadProcessMemory
namespace HashFixture
{
    namespace H = NativeModelHashProfile;
    using Reader = NativeModelHashReader;
    constexpr std::uintptr_t CursorRva = 0x572bcd0, ImageObjectRva = 0x5318000;
    constexpr std::uint32_t  Wanted = 0xfedcba98u, CursorSentinel = 0x12345678u;
    struct Fixture final : BankFixture
    {
        unsigned char* objects = nullptr;
        unsigned char* mapped = nullptr;
        HANDLE         mapping = nullptr;
        explicit Fixture(const std::filesystem::path& path) : BankFixture(path)
        {
            objects = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, nullptr);
            if (mapping)
                mapped = static_cast<unsigned char*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 4096));
            Check(objects && mapped, "create only task-owned private/mapped metadata pages");
            RestoreProfile();
            Reset();
        }
        ~Fixture()
        {
            if (mapped)
                UnmapViewOfFile(mapped);
            if (mapping)
                CloseHandle(mapping);
            if (objects)
                VirtualFree(objects, 0, MEM_RELEASE);
        }
        std::uintptr_t A() const noexcept { return reinterpret_cast<std::uintptr_t>(objects); }
        std::uintptr_t B() const noexcept { return reinterpret_cast<std::uintptr_t>(objects + 4096); }
        static void    Hash(std::uintptr_t at, std::uint32_t value) { std::memcpy(reinterpret_cast<void*>(at + H::HashOffset), &value, sizeof(value)); }
        void           RestoreProfile()
        {
            for (const auto& pin : H::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        }
        void Reset()
        {
            BankFixture::Reset();
            std::vector<std::uintptr_t> absent(NativeModelBankProfile::ModelCount, 0);
            transform.image.Patch(NativeModelBankProfile::BankRva, absent.data(), NativeModelBankProfile::BankBytes, PAGE_READWRITE);
            std::memset(objects, 0xcc, 8192);
            std::memset(mapped, 0xdd, 4096);
            Hash(A(), 0x11223344u);
            Hash(B(), 0x55667788u);
            Hash(reinterpret_cast<std::uintptr_t>(mapped), 0x88776655u);
            Global(CursorRva, CursorSentinel);
            HashReadHost::failAddress = 0;
        }
        bool CursorUntouched() const noexcept
        {
            std::uint32_t value = 0;
            std::memcpy(&value, reinterpret_cast<void*>(transform.image.image + CursorRva), sizeof(value));
            return value == CursorSentinel;
        }
    };
    Reader::Value Sentinel()
    {
        return {true, {0xdeadbeefu, 0xabcd, 0xfeed}, 0x87654321u};
    }
    bool Untouched(const Reader::Value& value)
    {
        const auto expected = Sentinel();
        return value.found == expected.found && value.model.modelIndex == expected.model.modelIndex && value.model.slotAddress == expected.model.slotAddress &&
               value.model.nativeModelInfo == expected.model.nativeModelInfo && value.hash == expected.hash;
    }
    Fixture* changing = nullptr;
    unsigned mutation = 0;
    void     Mutate()
    {
        switch (mutation)
        {
            case 0:
                Fixture::Hash(changing->A(), Wanted ^ 1u);
                break;
            case 1:
                changing->Slot(0, changing->B());
                break;
            case 2:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 3:
                changing->Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
            {
                const auto byte = static_cast<unsigned char>(H::Code[0].bytes[0] ^ 1u);
                changing->transform.image.Patch(H::Code[0].rva, &byte, 1, PAGE_EXECUTE_READ);
                break;
            }
            case 5:
                Fixture::Hash(changing->B(), Wanted);
                break;
        }
    }
}
int main(int argc, char** argv)
{
    namespace F = HashFixture;
    using Reader = F::Reader;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "model hash fixture has an explicit owned workspace directory");
    Reader::Binding refused{123};
    Check(!Reader::Qualify(refused) && refused.image == 123, "real shipping qualification refuses the non-game host without output");
    F::Fixture fixture(argv[1]);
    F::changing = &fixture;
    const Reader::Binding binding{fixture.transform.image.image};
    PoolTestHost::image = binding.image;
    Reader     reader(binding, &PoolTestHost::Owner), unowned(binding, nullptr);
    auto       value = F::Sentinel();
    const auto reset = [&]
    {
        fixture.Reset();
        value = F::Sentinel();
    };
    Check(Reader::Revalidate(binding), "actual bank domain and two exact saved model hash consumers qualify");
    Check(unowned.FindFirstByHash(0, value) == ClockResult::NotInitialized && F::Untouched(value), "image bytes alone do not create a native callback owner");
    reset();
    Check(reader.FindFirstByHash(0, value) == ClockResult::Ok && !value.found && !value.model.modelIndex && !value.model.slotAddress &&
              !value.model.nativeModelInfo && !value.hash && fixture.CursorUntouched(),
          "all20000 null slots form a genuine complete miss, including hashzero");
    for (const auto hash : {std::uint32_t{0}, std::uint32_t{0x7fffffff}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
    {
        reset();
        fixture.Slot(17, fixture.A());
        F::Fixture::Hash(fixture.A(), hash);
        Check(reader.FindFirstByHash(hash, value) == ClockResult::Ok && value.found && value.hash == hash && value.model.modelIndex == 17 &&
                  value.model.nativeModelInfo == fixture.A() && value.model.slotAddress == binding.image + NativeModelBankProfile::BankRva + 17 * 8 &&
                  fixture.CursorUntouched(),
              "full uint32 keys and poisoned vtable/type bytes require only the actual raw model hash");
    }
    reset();
    fixture.Slot(4, fixture.A());
    fixture.Slot(17, fixture.B());
    F::Fixture::Hash(fixture.A(), F::Wanted);
    F::Fixture::Hash(fixture.B(), F::Wanted);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && value.found && value.model.modelIndex == 4,
          "duplicates select the first ascending nonnull match");
    F::Fixture::Hash(fixture.A(), F::Wanted ^ 1u);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && value.found && value.model.modelIndex == 17,
          "a new current lookup does not cache the earlier hash winner");
    reset();
    fixture.Slot(NativeModelBankProfile::ModelCount - 1, fixture.B());
    F::Fixture::Hash(fixture.B(), F::Wanted);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && value.found && value.model.modelIndex == NativeModelBankProfile::ModelCount - 1,
          "the last real model slot remains searchable without a sampled-prefix miss");
    reset();
    fixture.Slot(0, fixture.A());
    F::Fixture::Hash(fixture.A(), F::Wanted);
    fixture.Slot(NativeModelBankProfile::ModelCount - 1, 1);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && value.found && !value.model.modelIndex,
          "an earliest match does not acquire unreadable metadata in the unused tail");
    reset();
    fixture.Slot(17, fixture.A());
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && !value.found && fixture.CursorUntouched(),
          "a complete bank/hash miss preserves the separate name cursor");
    reset();
    fixture.Slot(4, 1);
    fixture.Slot(17, fixture.A());
    F::Fixture::Hash(fixture.A(), F::Wanted);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::ReadFailure && F::Untouched(value),
          "an unreadable nonnull qualifying prefix refuses instead of skipping a possibly earlier match");
    reset();
    fixture.Slot(17, reinterpret_cast<std::uintptr_t>(fixture.mapped));
    F::Fixture::Hash(reinterpret_cast<std::uintptr_t>(fixture.mapped), F::Wanted);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && value.found &&
              value.model.nativeModelInfo == reinterpret_cast<std::uintptr_t>(fixture.mapped),
          "the established metadata policy admits readable mapped pages without allocation rights");
    reset();
    std::array<unsigned char, 12> imageMetadata{};
    std::memcpy(imageMetadata.data() + F::H::HashOffset, &F::Wanted, sizeof(F::Wanted));
    fixture.transform.image.Patch(F::ImageObjectRva, imageMetadata.data(), imageMetadata.size(), PAGE_READONLY);
    fixture.Slot(17, binding.image + F::ImageObjectRva);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && value.found && value.model.nativeModelInfo == binding.image + F::ImageObjectRva,
          "readable image metadata also supplies only its minimal raw hash extent");
    for (const auto protection : {DWORD{PAGE_READONLY}, DWORD{PAGE_NOACCESS}, DWORD{PAGE_EXECUTE_READ}})
    {
        reset();
        fixture.Slot(17, fixture.A());
        F::Fixture::Hash(fixture.A(), F::Wanted);
        DWORD prior = 0, ignored = 0;
        Check(VirtualProtect(fixture.objects, 4096, protection, &prior) != FALSE, "change only task-owned metadata page protection");
        const auto result = reader.FindFirstByHash(F::Wanted, value);
        Check(protection == PAGE_READONLY ? result == ClockResult::Ok && value.found : result == ClockResult::ReadFailure && F::Untouched(value),
              "metadata reads accept readonly data and refuse inaccessible or executable pages");
        Check(VirtualProtect(fixture.objects, 4096, prior, &ignored) != FALSE, "restore only task-owned metadata page protection");
    }
    reset();
    const auto crossPage = fixture.A() + 4096 - 8;
    F::Fixture::Hash(crossPage, F::Wanted);
    fixture.Slot(17, crossPage);
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Ok && value.found, "the minimal hash prefix may cross readable pages of one allocation");
    DWORD prior = 0, ignored = 0;
    Check(VirtualProtect(fixture.objects + 4096, 4096, PAGE_NOACCESS, &prior) != FALSE, "restrict the second owned metadata page only");
    value = F::Sentinel();
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::ReadFailure && F::Untouched(value),
          "every page of the hash field prefix must remain readable");
    Check(VirtualProtect(fixture.objects + 4096, 4096, prior, &ignored) != FALSE, "restore the second owned metadata page");
    reset();
    fixture.Slot(17, fixture.A());
    F::Fixture::Hash(fixture.A(), F::Wanted);
    HashReadHost::failAddress = fixture.A() + F::H::HashOffset;
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::ReadFailure && F::Untouched(value),
          "a real field read failure preserves every caller output field");
    for (F::mutation = 0; F::mutation != 6; ++F::mutation)
    {
        reset();
        if (F::mutation != 1)
            fixture.Slot(3, fixture.B());
        fixture.Slot(17, fixture.A());
        F::Fixture::Hash(fixture.A(), F::Wanted);
        if (F::mutation == 1)
            F::Fixture::Hash(fixture.B(), F::Wanted);
        PoolTestHost::watched = fixture.A() + F::H::HashOffset;
        PoolTestHost::after = &F::Mutate;
        Check(reader.FindFirstByHash(F::Wanted, value) != ClockResult::Ok && F::Untouched(value) && !PoolTestHost::after,
              "hash/earlier-bank/earlier-hash/profile drift or owner/GC loss refuses a hybrid qualifying prefix");
        if (F::mutation == 4)
            fixture.RestoreProfile();
    }
    reset();
    fixture.Slot(3, fixture.B());
    fixture.Slot(NativeModelBankProfile::ModelCount - 1, fixture.A());
    F::mutation = 5;
    PoolTestHost::watched = fixture.A() + F::H::HashOffset;
    PoolTestHost::after = &F::Mutate;
    Check(reader.FindFirstByHash(F::Wanted, value) != ClockResult::Ok && F::Untouched(value), "a complete miss also rechecks all earlier inspected metadata");
    reset();
    PoolTestHost::ownerResult = ClockResult::Expired;
    Check(reader.FindFirstByHash(F::Wanted, value) == ClockResult::Expired && F::Untouched(value),
          "an unavailable existing callback owner never becomes a miss");
    reset();
    fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{8});
    Check(reader.FindFirstByHash(F::Wanted, value) != ClockResult::Ok && F::Untouched(value),
          "native frontend state grants no gameplay model hash observation");
    reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread worker([&] { foreign = reader.FindFirstByHash(F::Wanted, value); });
    worker.join();
    Check(foreign == ClockResult::WrongThread && F::Untouched(value), "actual game-thread identity still applies to a permissive callback seam");
    Check(fixture.CursorUntouched(), "all lookup and refusal paths preserve the independent native name cursor");
    std::puts("Production native model hash lookup fixture completed");
    std::printf("Passed %u native model hash checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
