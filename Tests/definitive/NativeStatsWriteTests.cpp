/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CStats mutation routing without executing native game code
 *****************************************************************************/
// Reuse the real sparse Windows image fixture and actual readonly SDK object.
// Its earlier test main is retained as an ordinary function, never executed.
#define wmain FrozenReadonlyStatFixtureMain
#include "NativeStatsReaderTests.cpp"
#undef wmain
#include "../../Client/game_de/NativeStatsWriteCalls.h"
#include "../../Client/game_de/NativeStatsWriteProfile.h"
#include "../../Client/game_de/CStatsDE.h"
#define GetModuleHandleW  StatsTestHost::Module
#define ReadProcessMemory StatsTestHost::Read
#include "../../Client/game_de/NativeStatsWriteCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW
#include "../../Client/game_de/CStatsDE.cpp"
#include <cmath>
#include <type_traits>

namespace W = NativeStatsWriteCalls;
namespace WP = NativeStatsWriteProfile;
namespace
{
    struct MutationHost;
    MutationHost* active = nullptr;

    template <typename T>
    void Store(std::uintptr_t address, T value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }
    std::uint32_t Bits(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    struct MutationHost final : NativeStatsWriteAccess
    {
        OwnedImage        owned;
        NativeStatsReader reader;
        // The native pointer readers require pointer alignment. A stronger
        // invented alignment adds warning-producing gaps between these exact
        // allocation sizes without testing any additional native contract.
        alignas(std::uintptr_t) std::array<unsigned char, 0xb00> ped{}, otherPed{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x3c8> data{}, otherData{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x20> script{};
        W::Receipt     receipt{};
        unsigned       calls = 0, ownerChecks = 0, ownerFailAt = 0, swapAt = 0;
        unsigned       scriptSwapAt = 0;
        std::uintptr_t calledRva = 0;
        std::uint32_t  calledIndex = 0, calledBits = 0;
        ClockResult    ownerFailure = ClockResult::Expired, callResult = ClockResult::Ok;
        bool           changeValue = false, swapAfter = false, changePinAfter = false, changeImageAfter = false;

        std::uintptr_t Image() const { return owned.image; }
        std::uintptr_t Ped() const { return reinterpret_cast<std::uintptr_t>(ped.data()); }
        std::uintptr_t OtherPed() const { return reinterpret_cast<std::uintptr_t>(otherPed.data()); }
        std::uintptr_t Data() const { return reinterpret_cast<std::uintptr_t>(data.data()); }
        std::uintptr_t Script() const { return reinterpret_cast<std::uintptr_t>(script.data()); }
        template <typename T>
        void Patch(std::uintptr_t rva, T value)
        {
            owned.Patch(rva, &value, sizeof(value));
        }
        void Protection(std::uintptr_t rva, std::size_t size, DWORD flags)
        {
            DWORD old = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(Image() + rva), size, flags, &old) != FALSE, "protect only owned stat-call image pages");
        }
        void SeedPed(std::array<unsigned char, 0xb00>& bytes)
        {
            bytes.fill(0);
            const auto address = reinterpret_cast<std::uintptr_t>(bytes.data());
            Store(address, Image() + WP::PlayerVtableRva);
            Store(address + 0x5f0, Image() + WP::PlayerInfoRva);
            Store(address + 0x76c, 100.0f);
            Store(address + 0x7e0, std::int32_t{0});
            Store(address + 0xa10, static_cast<unsigned char>(0));
        }
        void ResetContext()
        {
            SeedPed(ped);
            SeedPed(otherPed);
            data.fill(0);
            otherData.fill(0);
            script.fill(0);
            const std::array<unsigned char, 7> ordinary = {'f', 'r', 'e', 'e', 'r', 'o', 0};
            std::memcpy(script.data() + 0x10, ordinary.data(), ordinary.size());
            Store(Image() + WP::PlayerRootRva, Ped());
            Store(Image() + WP::PlayerInfoRva, Data());
            Store(Image() + WP::PlayerIndexRva, static_cast<unsigned char>(0));
            Store(Image() + WP::ScriptRootRva, Script());
            Patch(WP::PlayerVtableRva + 0x50, Image() + WP::PlayerControlRva);
        }
        void Create(const std::filesystem::path& directory)
        {
            owned.Create(directory);
            for (const auto& pin : WP::Code)
            {
                owned.Patch(pin.rva, pin.bytes.data(), pin.size);
                Protection(pin.rva, pin.size, PAGE_EXECUTE_READ);
            }
            owned.Patch(WP::MissionGuardRva, WP::MissionGuard.data(), WP::MissionGuard.size());
            Protection(WP::MissionGuardRva, WP::MissionGuard.size(), PAGE_READONLY);
            Protection(WP::PlayerRootRva, 0x1c0, PAGE_READWRITE);
            Protection(WP::PlayerIndexRva, 1, PAGE_READWRITE);
            Protection(WP::ScriptRootRva, sizeof(std::uintptr_t), PAGE_READWRITE);
            ResetContext();
            StatsTestHost::image = Image();
            Check(reader.Bind() == Status::Ok, "actual reader binds the owned native call prerequisites");
        }
        void ResetCall()
        {
            receipt = {};
            calls = ownerChecks = ownerFailAt = swapAt = scriptSwapAt = 0;
            calledRva = 0;
            calledIndex = calledBits = 0;
            ownerFailure = ClockResult::Expired;
            callResult = ClockResult::Ok;
            changeValue = swapAfter = changePinAfter = changeImageAfter = false;
            StatsTestHost::image = Image();
            StatsTestHost::failAt.store(0);
        }
        static ClockResult Owner() noexcept
        {
            auto& host = *active;
            ++host.ownerChecks;
            if (host.swapAt == host.ownerChecks)
                Store(host.Image() + WP::PlayerRootRva, host.OtherPed());
            if (host.scriptSwapAt == host.ownerChecks)
                host.script[0x10] ^= 1;
            return host.ownerFailAt == host.ownerChecks ? host.ownerFailure : ClockResult::Ok;
        }
        static ClockResult Invoke(W::NativeFunction target, std::uint32_t index, float amount, std::uint32_t& exception) noexcept
        {
            auto& host = *active;
            ++host.calls;
            host.calledRva = reinterpret_cast<std::uintptr_t>(target) - host.Image();
            host.calledIndex = index;
            host.calledBits = Bits(amount);
            // The test seam observes the typed function pointer and arguments.
            // It NEVER invokes target or simulates native caps/reaction rules.
            // An independent side effect proves that the wrapper neither forces
            // target equality nor restores a partially entered transaction.
            if (host.changeValue)
                Store(host.Image() + P::FloatBankRva, 413.25f);
            if (host.swapAfter)
                Store(host.Image() + WP::PlayerRootRva, host.OtherPed());
            if (host.changePinAfter)
            {
                const unsigned char changed = static_cast<unsigned char>(WP::Code.front().bytes[0] ^ 1);
                host.owned.Patch(WP::Code.front().rva, &changed, 1);
            }
            if (host.changeImageAfter)
                StatsTestHost::image = 0;
            if (host.callResult == ClockResult::OriginalException)
                exception = EXCEPTION_ACCESS_VIOLATION;
            return host.callResult;
        }
        ClockResult Apply(W::Operation operation, unsigned short index, float amount)
        {
            return W::Apply(reader, Image(), operation, index, amount, &Owner, &receipt, &Invoke);
        }
        ClockResult SetStatValue(unsigned short index, float value) noexcept override
        {
            return W::Apply(reader, Image(), W::Operation::Set, index, value, &Owner, &receipt, &Invoke);
        }
        ClockResult ModifyStat(unsigned short index, float value) noexcept override
        {
            return W::Apply(reader, Image(), W::Operation::Modify, index, value, &Owner, &receipt, &Invoke);
        }
    };

    void Called(MutationHost& host, std::uintptr_t rva, unsigned short index, float amount)
    {
        Check(host.calls == 1 && host.ownerChecks == 3, "transaction enters exactly once between two entry and one completion owner checks");
        Check(host.calledRva == rva && host.receipt.entryRva == rva, "typed target is the independently qualified native entry");
        Check(host.calledIndex == index && host.receipt.promotedIndex == index, "actual SDK ushort is promoted to full native uint32 argument");
        Check(host.calledBits == Bits(amount) && Bits(host.receipt.passedAmount) == Bits(amount), "native XMM1 argument preserves selected float bits");
        Check(host.receipt.entered && host.receipt.returned && !host.receipt.exceptionCode, "completed native-call receipt records entry and return honestly");
    }
    void Refused(MutationHost& host, W::Operation operation, unsigned short index, float amount, ClockResult expected, const char* why)
    {
        host.ResetCall();
        Check(host.Apply(operation, index, amount) == expected, why);
        Check(!host.calls && !host.receipt.entered && !host.receipt.returned, "refused prerequisite never reaches the typed native-call seam");
    }
    void ActualSdkMutations(MutationHost& host)
    {
        CStatsDE object(host.reader, host);
        CStats&  sdk = object;
        for (unsigned short index = 0; index < P::IntegerSdkEnd; ++index)
        {
            if (index >= P::FloatCount && index < P::IntegerFirst)
                continue;
            const float before = sdk.GetStatValue(index);
            host.ResetCall();
            sdk.SetStatValue(index, 65537.75f);
            Called(host, WP::SetRva, index, 65537.75f);
            Check(sdk.GetStatValue(index) == before, "native intentional no-op is accepted without forcing a raw bank store");
            host.ResetCall();
            sdk.ModifyStat(index, 7.125f);
            Called(host, WP::IncrementRva, index, 7.125f);
            host.ResetCall();
            sdk.ModifyStat(index, -7.125f);
            Called(host, WP::DecrementRva, index, 7.125f);
        }
        Check(sdk.GetSkillStatIndex(WEAPONTYPE_PISTOL) == 69, "concrete actual CStats retains the real inherited skill mapping");
        for (float value : {-0.0f, 0.0f})
        {
            host.ResetCall();
            sdk.ModifyStat(120, value);
            Called(host, WP::IncrementRva, 120, value);
        }
        const float intMaximum = std::nextafter(static_cast<float>((std::numeric_limits<std::int32_t>::max)()), 0.0f);
        for (float value : {static_cast<float>((std::numeric_limits<std::int32_t>::min)()), -65537.75f, intMaximum})
        {
            host.ResetCall();
            sdk.SetStatValue(342, value);
            Called(host, WP::SetRva, 342, value);
        }
        host.ResetCall();
        sdk.ModifyStat(342, -intMaximum);
        Called(host, WP::DecrementRva, 342, intMaximum);
        for (float value : {(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)()})
        {
            host.ResetCall();
            sdk.SetStatValue(0, value);
            Called(host, WP::SetRva, 0, value);
        }
        host.ResetCall();
        std::memcpy(host.script.data() + 0x10, WP::MissionGuard.data(), WP::MissionGuard.size());
        sdk.SetStatValue(64, 0.5f);
        Called(host, WP::SetRva, 64, 0.5f);
        host.ResetContext();
        host.ResetCall();
        host.ownerFailAt = 1;
        Refuses([&] { sdk.SetStatValue(0, 1.0f); }, "actual CStats Set surfaces a refused owner instead of pretending success");
        Check(!host.calls, "actual SDK owner refusal makes no native call");
        host.ResetCall();
        host.ownerFailAt = 1;
        Refuses([&] { sdk.ModifyStat(0, 1.0f); }, "actual CStats Modify surfaces a refused owner");
        struct Unavailable final : NativeStatsWriteAccess
        {
        } unavailable;
        CStatsDE absent(host.reader, unavailable);
        Refuses([&] { absent.SetStatValue(0, 1); }, "default unavailable native writer never claims actual SDK mutation");
        Refuses([&] { absent.ModifyStat(0, 1); }, "default unavailable native writer never claims actual SDK delta");
    }
    void Inputs(MutationHost& host)
    {
        for (unsigned index = 0; index <= UINT16_MAX; ++index)
        {
            if (index < P::FloatCount || (index >= P::IntegerFirst && index < P::IntegerSdkEnd))
                continue;
            Refused(host, W::Operation::Set, static_cast<unsigned short>(index), 1, ClockResult::InvalidArgument,
                    "every unexposed ushort stat ID refuses before a native entry");
        }
        for (const auto operation : {W::Operation::Set, W::Operation::Modify})
        {
            for (const float amount :
                 {(std::numeric_limits<float>::quiet_NaN)(), (std::numeric_limits<float>::infinity)(), -(std::numeric_limits<float>::infinity)()})
                Refused(host, operation, 0, amount, ClockResult::InvalidArgument, "nonfinite incoming native transaction values refuse");
            for (const float amount :
                 {static_cast<float>((std::numeric_limits<std::int32_t>::max)()),
                  std::nextafter(static_cast<float>((std::numeric_limits<std::int32_t>::min)()), -(std::numeric_limits<float>::infinity)())})
                Refused(host, operation, 342, amount, ClockResult::InvalidArgument, "native int32 first-conversion overflow refuses without int16 tightening");
        }
        Refused(host, W::Operation::Modify, 120, static_cast<float>((std::numeric_limits<std::int32_t>::min)()), ClockResult::InvalidArgument,
                "negative int32 minimum cannot become a representable positive native decrement input");
        Refused(host, static_cast<W::Operation>(0xffffffff), 0, 1, ClockResult::InvalidArgument, "malformed mutation operation refuses");
        host.ResetCall();
        Check(W::Apply(host.reader, host.Image(), W::Operation::Set, 0, 1, nullptr, &host.receipt, &MutationHost::Invoke) == ClockResult::NotInitialized &&
                  !host.calls && !host.receipt.entered,
              "native entry cannot be invoked without a real owner guard callback");
        Check(W::Apply(host.reader, 0, W::Operation::Set, 0, 1, &MutationHost::Owner, &host.receipt, &MutationHost::Invoke) == ClockResult::BindingChanged &&
                  !host.calls,
              "missing actual image refuses before entry");
    }
    void Prerequisites(MutationHost& host)
    {
        for (const auto& pin : WP::Code)
        {
            const unsigned char changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            host.owned.Patch(pin.rva, &changed, 1);
            Refused(host, W::Operation::Set, 0, 1, ClockResult::BindingChanged, "each entry, reaction, constructor and ABI-caller block is required");
            host.owned.Patch(pin.rva, pin.bytes.data(), 1);
        }
        const unsigned char changed = 'x';
        host.owned.Patch(WP::MissionGuardRva, &changed, 1);
        Refused(host, W::Operation::Modify, 0, 1, ClockResult::BindingChanged, "actual native mission-guard literal is part of supported semantics");
        host.owned.Patch(WP::MissionGuardRva, WP::MissionGuard.data(), 1);
        Store(host.Image() + WP::PlayerRootRva, std::uintptr_t{0});
        Refused(host, W::Operation::Modify, 245, 1, ClockResult::NotInitialized, "native PLAYING9 intent cannot substitute for a loaded local player");
        host.ResetContext();
        Store(host.Image() + WP::PlayerIndexRva, static_cast<unsigned char>(1));
        Refused(host, W::Operation::Set, 0, 1, ClockResult::BindingChanged, "unknown alternate native player route refuses without guessed row counts");
        host.ResetContext();
        Store(host.Ped(), std::uintptr_t{0});
        Refused(host, W::Operation::Set, 0, 1, ClockResult::BindingChanged, "wrong actual native ped class refuses");
        host.ResetContext();
        host.Patch(WP::PlayerVtableRva + 0x50, std::uintptr_t{0});
        Refused(host, W::Operation::Set, 0, 1, ClockResult::BindingChanged, "wrong native player-control vtable identity refuses");
        host.ResetContext();
        Store(host.Ped() + 0x5f0, host.Image() + WP::PlayerInfoRva + 8);
        Refused(host, W::Operation::Modify, 335, 1, ClockResult::BindingChanged, "foreign player-info relationship cannot authorize health mutation");
        host.ResetContext();
        Store(host.Ped() + 0x7e0, std::int32_t{1});
        Refused(host, W::Operation::Set, 0, 1, ClockResult::BindingChanged, "nonlocal native selector refuses");
        host.ResetContext();
        Store(host.Ped() + 0xa10, static_cast<unsigned char>(13));
        Refused(host, W::Operation::Set, 0, 1, ClockResult::BindingChanged, "current weapon slot must fit the proven thirteen native slots");
        for (const float health : {(std::numeric_limits<float>::quiet_NaN)(), (std::numeric_limits<float>::infinity)(), (std::numeric_limits<float>::max)()})
        {
            host.ResetContext();
            Store(host.Ped() + 0x76c, health);
            Refused(host, W::Operation::Modify, 245, 1, ClockResult::BindingChanged, "loaded-player health must fit the actual native first conversion");
        }
        host.ResetContext();
        Store(host.Image() + WP::PlayerInfoRva, std::uintptr_t{0});
        Refused(host, W::Operation::Set, 0, 1, ClockResult::BindingChanged, "missing real native player-data object refuses");
        host.ResetContext();
        Store(host.Image() + WP::ScriptRootRva, std::uintptr_t{0});
        Refused(host, W::Operation::Set, 64, 1, ClockResult::NotInitialized, "special native setter64 cannot dereference a missing current script");
        host.ResetCall();
        Check(host.Apply(W::Operation::Modify, 64, 1) == ClockResult::Ok, "native Modify does not acquire the setter-only current-script precondition");
        host.ResetContext();
        host.Protection(WP::PlayerRootRva, 0x1c0, PAGE_READONLY);
        Refused(host, W::Operation::Set, 0, 1, ClockResult::ReadFailure, "native player-info storage must remain writable image data");
        host.Protection(WP::PlayerRootRva, 0x1c0, PAGE_READWRITE);
        host.Protection(WP::ScriptRootRva, sizeof(std::uintptr_t), PAGE_NOACCESS);
        Refused(host, W::Operation::Set, 64, 1, ClockResult::NotInitialized, "unreadable script global refuses before native unguarded dereference");
        host.Protection(WP::ScriptRootRva, sizeof(std::uintptr_t), PAGE_READWRITE);
        host.ResetContext();
        host.ResetCall();
        StatsTestHost::failAt.store(StatsTestHost::reads.load() + 1);
        Check(host.Apply(W::Operation::Set, 0, 1) == ClockResult::BindingChanged && !host.calls, "failed partial OS provenance copy never enters native code");
        host.ResetCall();
        std::atomic<ClockResult> result{ClockResult::Ok};
        std::thread              worker([&] { result.store(host.Apply(W::Operation::Set, 0, 1)); });
        worker.join();
        Check(result.load() == ClockResult::WrongThread && !host.calls, "actual reader binding rejects a different thread before calling owner or target");
    }
    void TransactionBoundaries(MutationHost& host)
    {
        for (const unsigned stop : {1U, 2U, 3U})
        {
            host.ResetCall();
            host.ownerFailAt = stop;
            Check(host.Apply(W::Operation::Set, 0, 1) == ClockResult::Expired,
                  "generation or native PLAYING9 admission is checked at each transaction boundary");
            Check(host.calls == (stop == 3 ? 1U : 0U) && host.receipt.entered == (stop == 3) && host.receipt.returned == (stop == 3),
                  "receipt distinguishes pre-entry refusal from completed effects with a lost owner");
        }
        host.ResetCall();
        host.swapAt = 1;
        Check(host.Apply(W::Operation::Set, 0, 1) == ClockResult::BindingChanged && !host.calls,
              "changed loaded player between entry context bookends refuses");
        host.ResetContext();
        host.ResetCall();
        host.scriptSwapAt = 1;
        Check(host.Apply(W::Operation::Set, 64, 1) == ClockResult::BindingChanged && !host.calls, "changed actual script name between bookends refuses");
        host.ResetContext();
        host.ResetCall();
        host.swapAfter = true;
        host.changeValue = true;
        Check(host.Apply(W::Operation::Set, 0, 1) == ClockResult::BindingChanged && host.calls == 1 && host.receipt.entered && host.receipt.returned,
              "changed player after native return reports failure without another invocation");
        Check(*reinterpret_cast<const float*>(host.Image() + P::FloatBankRva) == 413.25f, "post-entry failure preserves independent native effects");
        host.ResetContext();
        host.ResetCall();
        host.callResult = ClockResult::OriginalException;
        host.changeValue = true;
        Check(host.Apply(W::Operation::Modify, 0, -1) == ClockResult::OriginalException && host.calls == 1 && host.receipt.entered && !host.receipt.returned &&
                  host.receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION,
              "typed native exception receipt records partial entry without fabricated return");
        Check(*reinterpret_cast<const float*>(host.Image() + P::FloatBankRva) == 413.25f,
              "partially entered exception is neither retried nor snapshot-restored");
        host.ResetCall();
        host.changePinAfter = true;
        Check(host.Apply(W::Operation::Modify, 0, 1) == ClockResult::BindingChanged && host.calls == 1 && host.receipt.returned,
              "post-return native provenance change cannot be accepted as a stable transaction");
        host.owned.Patch(WP::Code.front().rva, WP::Code.front().bytes.data(), 1);
        host.ResetCall();
        host.changeImageAfter = true;
        Check(host.Apply(W::Operation::Set, 0, 1) == ClockResult::BindingChanged && host.calls == 1 && host.receipt.returned,
              "image replacement after invocation is reported without retrying native effects");
        host.ResetCall();
        host.reader.Reset();
        Check(host.Apply(W::Operation::Set, 0, 1) == ClockResult::NotInitialized && !host.calls,
              "retired generation reader cannot be reused for a later native transaction");
        Check(host.reader.Bind() == Status::Ok, "restored supported owner can bind a distinct later generation");
        Store(host.Image() + P::FloatBankRva, FloatValue(0));
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    MutationHost host;
    host.Create(argv[1]);
    active = &host;
    constexpr auto                       bankBytes = P::IntegerBankRva + P::IntegerStorageCount * sizeof(std::uint32_t) - P::FloatBankRva;
    std::array<unsigned char, bankBytes> original{};
    std::memcpy(original.data(), reinterpret_cast<const void*>(host.Image() + P::FloatBankRva), original.size());
    ActualSdkMutations(host);
    host.ResetContext();
    Inputs(host);
    Prerequisites(host);
    TransactionBoundaries(host);
    Check(!std::memcmp(original.data(), reinterpret_cast<const void*>(host.Image() + P::FloatBankRva), original.size()),
          "fixture restores only its deliberately injected owned scalar effects; production never performs a generic stat rollback");
    StatsTestHost::image = 0;
    active = nullptr;
    std::printf("Passed %u native stat mutation routing and actual SDK checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
