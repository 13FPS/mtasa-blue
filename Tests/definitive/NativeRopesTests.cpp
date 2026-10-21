/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual rope SDK/actions over owned memory, never game instructions
 *****************************************************************************/
#define wmain FrozenPlayerFixtureMainForRopes
#include "NativePlayerInfoTests.cpp"
#undef wmain
#include "../../Client/game_de/NativeRopesMemory.h"
#include "../../Client/game_de/NativeRopesProfile.h"
#include "../../Client/game_de/CRopesDE.h"
#include <SharedUtil.Defines.h>
#include <CVector.h>

namespace RopesHost
{
    using namespace mta::game_de;
    using M = NativeRopesMemory;
    unsigned              creates = 0, removes = 0, exchanges = 0;
    std::uintptr_t        entry = 0, identity = 0, row = 0;
    std::uint32_t         duration = 0;
    M::Vector12           position;
    std::int32_t          nativeResult = 1;
    ClockResult           result = ClockResult::Ok;
    std::function<void()> duringCall, beforeCas, afterCas;
    LONG                  Cas32(volatile LONG* address, LONG value, LONG expected)
    {
        ++exchanges;
        if (beforeCas)
            beforeCas();
        const auto observed = _InterlockedCompareExchange(address, value, expected);
        if (afterCas)
            afterCas();
        return observed;
    }
    ClockResult Create(M::CreateFunction function, std::uintptr_t key, const M::Vector12& vector, std::uint32_t timeout, std::int32_t& output,
                       std::uint32_t& exception) noexcept
    {
        ++creates;
        entry = reinterpret_cast<std::uintptr_t>(function);
        identity = key;
        position = vector;
        duration = timeout;
        if (duringCall)
            duringCall();
        output = nativeResult;
        if (result == ClockResult::OriginalException)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
    ClockResult Remove(M::RemoveFunction function, void* object, std::uint32_t& exception) noexcept
    {
        ++removes;
        entry = reinterpret_cast<std::uintptr_t>(function);
        row = reinterpret_cast<std::uintptr_t>(object);
        if (duringCall)
            duringCall();
        if (result == ClockResult::OriginalException)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return result;
    }
    void Reset()
    {
        PlayerHost::Reset();
        creates = removes = exchanges = duration = 0;
        entry = identity = row = 0;
        position = {};
        nativeResult = 1;
        result = ClockResult::Ok;
        duringCall = beforeCas = afterCas = {};
    }
}
#define GetModuleHandleW     PlayerHost::Module
#define CheckExecutableBuild PlayerFixtureExecutable
#define ReadProcessMemory    PlayerHost::Read
#undef InterlockedCompareExchange
#define InterlockedCompareExchange RopesHost::Cas32
#include "../../Client/game_de/NativeRopesMemory.cpp"
#undef InterlockedCompareExchange
#undef ReadProcessMemory
#undef CheckExecutableBuild
#undef GetModuleHandleW
#include "../../Client/game_de/CRopesDE.cpp"

namespace
{
    namespace RP = NativeRopesProfile;
    template <class T>
    T& RopeField(std::uintptr_t address, std::size_t offset)
    {
        return *reinterpret_cast<T*>(address + offset);
    }
    void RopeProtect(std::uintptr_t address, std::size_t bytes, DWORD value)
    {
        DWORD old = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(address), bytes, value, &old) != FALSE, "protect only owned rope fixture pages");
    }
    void SeedRopes(OwnedImage& image)
    {
        for (const auto& pin : RP::Code)
        {
            image.Patch(pin.rva, pin.bytes.data(), pin.size);
            RopeProtect(image.image + pin.rva, pin.size, PAGE_EXECUTE_READ);
        }
        RopeProtect(image.image + RP::TableRva, RP::RowCount * RP::RowBytes, PAGE_READWRITE);
        RopeProtect(image.image + RP::CounterRva, 0x1000, PAGE_READWRITE);
        RopeProtect(image.image + RP::ReferencesRva, RP::ReferenceCount * RP::ReferenceBytes, PAGE_READWRITE);
        const auto destroy = image.image + RP::HookDestructorRva, remove = image.image + RP::HookRemoveRva;
        image.Patch(RP::HookVtableRva, &destroy, sizeof(destroy));
        image.Patch(RP::HookVtableRva + 0x20, &remove, sizeof(remove));
    }
    class RopeAccess final : public NativeRopesAccess
    {
    public:
        NativeRopesMemory&         memory;
        NativeRopesMemory::Receipt receipt;
        explicit RopeAccess(NativeRopesMemory& value) : memory(value) {}
        ClockResult CreateSwatRope(const CVector& position, std::uint32_t duration, std::int32_t& output) noexcept override
        {
            return memory.Create(position, duration, output, PlayerHost::Admit, &receipt, RopesHost::Create);
        }
        ClockResult RemoveEntityRope(std::uintptr_t key) noexcept override { return memory.Remove(key, PlayerHost::Admit, &receipt, RopesHost::Remove); }
    };
    class MissingRopes final : public NativeRopesAccess
    {
    };
    void ExerciseRopes(OwnedImage& image)
    {
        SeedRopes(image);
        PlayerHost::image = image.image;
        NativeRopesMemory memory;
        RopeAccess        access(memory);
        CRopesDE          object(access);
        CRopes&           sdk = object;
        const auto        base = image.image + RP::TableRva;
        auto&             counter = RopeField<std::uint32_t>(image.image, RP::CounterRva);
        const auto        clear = [&]
        {
            RopesHost::Reset();
            counter = 0;
            std::memset(reinterpret_cast<void*>(base), 0, RP::RowCount * RP::RowBytes);
            std::memset(reinterpret_cast<void*>(image.image + RP::ReferencesRva), 0, RP::ReferenceCount * RP::ReferenceBytes);
        };
        clear();
        std::int32_t output = 123;
        CVector      position(1.5f, -2.0f, 33.25f);
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::NotInitialized && output == 123,
              "unbound rope action preserves output and applies no intent");
        Check(memory.Bind() == ClockResult::Ok && memory.IsBound(), "bind exact owned image/rope profile on one thread");
        Check(memory.Qualify(nullptr) == ClockResult::NotInitialized, "static table does not invent a native callback lease");
        for (std::uint32_t duration : {0u, 1u, 4000u, UINT32_MAX})
        {
            clear();
            const auto id = sdk.CreateRopeForSwatPed(position, duration);
            Check(id == 100 && counter == 1 && RopesHost::creates == 1 && RopesHost::entry == image.image + RP::CreateRva && RopesHost::identity == 100 &&
                      RopesHost::duration == duration && RopesHost::position.x == position.fX && RopesHost::position.y == position.fY &&
                      RopesHost::position.z == position.fZ && access.receipt.entered && access.receipt.returned && access.receipt.counterCommitted,
                  "actual SDK passes exact position/duration and returns native generated ID through one action boundary");
        }
        clear();
        Check(object.CreateRopeForSwatPed(position) == 100 && RopesHost::duration == 4000, "existing SDK default duration remains4000");
        clear();
        counter = 255;
        Check(sdk.CreateRopeForSwatPed(position, 7) == 355 && counter == 0, "DE byte-wrap counter semantics remain exact");
        clear();
        for (std::size_t i = 0; i < RP::RowCount; ++i)
        {
            RopeField<unsigned char>(base + i * RP::RowBytes, RP::TypeOffset) = 8;
            RopeField<std::uintptr_t>(base + i * RP::RowBytes, RP::IdentityOffset) = 2000 + i;
        }
        RopesHost::nativeResult = 0;
        Check(sdk.CreateRopeForSwatPed(position, 4000) == 100 && counter == 1, "native0/full table still takes the proved DE nonnegative ID/counter branch");
        for (std::size_t i = 0; i < RP::RowCount; ++i)
            Check(RopeField<std::uintptr_t>(base + i * RP::RowBytes, RP::IdentityOffset) == 2000 + i,
                  "full-table ID result never claims or synthesizes an extra rope row");
        clear();
        RopesHost::nativeResult = -1;
        Check(sdk.CreateRopeForSwatPed(position, 4000) == -1 && counter == 0 && !access.receipt.counterCommitted,
              "proved signed-negative branch preserves counter and returns ID-1");
        clear();
        RopesHost::beforeCas = [&] { counter = 99; };
        output = 123;
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::OwnerConflict && counter == 99 &&
                  output == 123 && access.receipt.entered && RopesHost::creates == 1,
              "foreign counter edit survives an already-entered native action without replay");
        clear();
        RopesHost::afterCas = [&] { counter = 77; };
        output = 123;
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::OwnerConflict && counter == 77 &&
                  output == 123 && access.receipt.counterCommitted,
              "post-commit conflict is honest and never blanket-restores native intent");
        clear();
        RopesHost::result = ClockResult::OriginalException;
        RopesHost::duringCall = [&] { RopeField<unsigned char>(base, RP::TypeOffset) = 8; };
        output = 123;
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::OriginalException &&
                  access.receipt.entered && !access.receipt.returned && access.receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION && output == 123 &&
                  RopeField<unsigned char>(base, RP::TypeOffset) == 8,
              "native exception preserves partial rope effects and exact receipt without retry");
        clear();
        RopesHost::duringCall = [&] { PlayerHost::expiresAt = PlayerHost::ownerChecks + 1; };
        output = 123;
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::Expired && access.receipt.entered &&
                  output == 123 && counter == 0,
              "post-entry owner revocation prevents later counter mutation but does not undo native action");
        clear();
        Check(sdk.CreateRopeForSwatPed(position, 4000) == 100, "measure bounded actual owner admission sequence");
        const auto ownerBoundaries = PlayerHost::ownerChecks;
        Check(ownerBoundaries > 4 && ownerBoundaries < 40, "finite native rope callback bookend count");
        for (unsigned failure = 1; failure <= ownerBoundaries; ++failure)
        {
            clear();
            PlayerHost::expiresAt = failure;
            output = 123;
            Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::Expired && output == 123 &&
                      RopesHost::creates <= 1,
                  "every real callback revocation point refuses without automatic native retry");
            if (!access.receipt.entered)
                Check(counter == 0 && !RopesHost::exchanges, "pre-entry refusal writes no native state");
        }
        clear();
        CVector invalid = position;
        invalid.fX = (std::numeric_limits<float>::infinity)();
        Check(
            memory.Create(invalid, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::InvalidArgument && !RopesHost::creates,
            "nonfinite world coordinate refuses before native arithmetic");
        clear();
        const std::uintptr_t opaque = 0xdeadbeef;
        RopeField<std::uintptr_t>(base, RP::IdentityOffset) = opaque;
        RopeField<unsigned char>(base, RP::TypeOffset) = 8;
        RopesHost::duringCall = [&] { RopeField<unsigned char>(base, RP::TypeOffset) = 0; };
        sdk.RemoveEntityRope(reinterpret_cast<CEntitySAInterface*>(opaque));
        Check(RopesHost::removes == 1 && RopesHost::entry == image.image + RP::RemoveRva && RopesHost::row == base && access.receipt.entered,
              "unreadable opaque SDK key is compared only; exact native row is the actual removal receiver");
        clear();
        sdk.RemoveEntityRope(reinterpret_cast<CEntitySAInterface*>(opaque));
        Check(!RopesHost::removes && !access.receipt.entered, "absent identity preserves classic no-action behavior");
        clear();
        RopeField<std::uintptr_t>(base, RP::IdentityOffset) = opaque;
        sdk.RemoveEntityRope(reinterpret_cast<CEntitySAInterface*>(opaque));
        Check(RopesHost::removes == 1, "classic first equality match includes an inactive native row");

        auto* allocation = VirtualAlloc(nullptr, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(allocation != nullptr, "allocate only owned hook/picked native-shaped objects");
        const auto                                              hook = reinterpret_cast<std::uintptr_t>(allocation), picked = hook + 0x1000;
        alignas(std::uintptr_t) std::array<unsigned char, 0x20> hookPool{}, pickedPool{};
        std::array<unsigned char, 1>                            hookFlags{}, pickedFlags{};
        const auto                                              seedDependencies = [&]
        {
            clear();
            std::memset(allocation, 0, 0x2000);
            hookFlags[0] = pickedFlags[0] = 0;
            RopeField<std::uintptr_t>(base, RP::IdentityOffset) = opaque;
            RopeField<unsigned char>(base, RP::TypeOffset) = 4;
            RopeField<std::uintptr_t>(base, RP::HookOffset) = hook;
            RopeField<std::uintptr_t>(base, RP::PickedUpOffset) = picked;
            RopeField<std::uintptr_t>(hook, 0) = image.image + RP::HookVtableRva;
            RopeField<unsigned char>(hook, 0x6a) = 4;
            RopeField<std::uintptr_t>(picked, 0) = image.image + RP::HookVtableRva;
            RopeField<unsigned char>(picked, 0x6a) = 3;
            const auto first = image.image + RP::ReferencesRva, second = first + RP::ReferenceBytes;
            RopeField<std::uintptr_t>(hook, 0x40) = first;
            RopeField<std::uintptr_t>(picked, 0x40) = second;
            RopeField<std::uintptr_t>(first, 0) = 0;
            RopeField<std::uintptr_t>(first, 8) = base + RP::HookOffset;
            RopeField<std::uintptr_t>(second, 0) = 0;
            RopeField<std::uintptr_t>(second, 8) = base + RP::PickedUpOffset;
            const auto pool = [&](std::array<unsigned char, 0x20>& bytes, std::uintptr_t objects, unsigned char* flags, std::uintptr_t root)
            {
                const auto address = reinterpret_cast<std::uintptr_t>(bytes.data());
                RopeField<std::uintptr_t>(address, 0) = objects;
                RopeField<std::uintptr_t>(address, 8) = reinterpret_cast<std::uintptr_t>(flags);
                RopeField<std::int32_t>(address, 0x10) = 1;
                image.Patch(root, &address, sizeof(address));
            };
            pool(hookPool, hook, hookFlags.data(), 0x572b588);
            pool(pickedPool, picked, pickedFlags.data(), 0x572b5a0);
            RopesHost::duringCall = [&]
            {
                RopeField<unsigned char>(base, RP::TypeOffset) = 0;
                RopeField<std::uintptr_t>(base, RP::HookOffset) = 0;
                RopeField<std::uintptr_t>(base, RP::PickedUpOffset) = 0;
            };
        };
        seedDependencies();
        sdk.RemoveEntityRope(reinterpret_cast<CEntitySAInterface*>(opaque));
        Check(RopesHost::removes == 1 && access.receipt.returned, "real native pool membership and registered row slots admit generic attached-entity removal");
        seedDependencies();
        RopeField<std::uintptr_t>(hook, 0x40) = 0;
        Check(memory.Remove(opaque, PlayerHost::Admit, &access.receipt, RopesHost::Remove) == ClockResult::BindingChanged && !access.receipt.entered,
              "readable object pages alone confer no native lifetime authority");
        seedDependencies();
        RopeField<std::uintptr_t>(image.image + RP::ReferencesRva, 8) = base + RP::HolderOffset;
        Check(memory.Remove(opaque, PlayerHost::Admit, &access.receipt, RopesHost::Remove) == ClockResult::BindingChanged && !RopesHost::removes,
              "reference registration must name the exact consumed row field");
        seedDependencies();
        RopeField<std::uintptr_t>(hook, 0x40) = hook;
        Check(memory.Remove(opaque, PlayerHost::Admit, &access.receipt, RopesHost::Remove) == ClockResult::BindingChanged && !RopesHost::removes,
              "foreign private reference node is not a native fixed-array registration");
        seedDependencies();
        RopeField<std::uintptr_t>(image.image + RP::ReferencesRva, 0) = image.image + RP::ReferencesRva;
        Check(memory.Remove(opaque, PlayerHost::Admit, &access.receipt, RopesHost::Remove) == ClockResult::BindingChanged && !RopesHost::removes,
              "cyclic reference chain refuses within the native3200 node bound");
        seedDependencies();
        hookFlags[0] = 0x80;
        Check(memory.Remove(opaque, PlayerHost::Admit, &access.receipt, RopesHost::Remove) == ClockResult::BindingChanged && !RopesHost::removes,
              "freed native hook allocation cannot be destructed through stale reference metadata");
        seedDependencies();
        RopeField<std::uintptr_t>(base, RP::HookOffset) = 0;
        Check(memory.Remove(opaque, PlayerHost::Admit, &access.receipt, RopesHost::Remove) == ClockResult::BindingChanged && !RopesHost::removes,
              "picked-up object cannot enter native release with missing required hook");
        seedDependencies();
        RopesHost::result = ClockResult::OriginalException;
        RopesHost::duringCall = [&] { RopeField<unsigned char>(base, RP::TypeOffset) = 0; };
        Check(memory.Remove(opaque, PlayerHost::Admit, &access.receipt, RopesHost::Remove) == ClockResult::OriginalException && access.receipt.entered &&
                  !access.receipt.returned && RopeField<unsigned char>(base, RP::TypeOffset) == 0,
              "partial native remove effects survive exception without replay");
        Check(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "release only fixture-owned object allocations");
        clear();
        for (const auto& pin : RP::Code)
        {
            const auto foreign = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            image.Patch(pin.rva, &foreign, 1);
            Check(memory.Qualify(PlayerHost::Admit) == ClockResult::BindingChanged, "every qualified rope/reference/action pin rejects a foreign code edit");
            image.Patch(pin.rva, pin.bytes.data(), pin.size);
        }
        clear();
        ClockResult wrong = ClockResult::Ok;
        std::thread other([&] { wrong = memory.Qualify(PlayerHost::Admit); });
        other.join();
        Check(wrong == ClockResult::WrongThread, "borrowed rope service cannot transfer its binding thread");
        clear();
        PlayerHost::image = 0;
        output = 123;
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::BindingChanged && output == 123 &&
                  !access.receipt.entered,
              "current-module replacement never invokes an old rope entry or publishes output");
        PlayerHost::image = image.image;
        clear();
        PlayerHost::readFault = true;
        output = 123;
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) != ClockResult::Ok && output == 123 &&
                  !access.receipt.entered,
              "partial OS image read preserves output and refuses before native entry");
        clear();
        RopeProtect(base, RP::RowCount * RP::RowBytes, PAGE_READONLY);
        output = 123;
        Check(memory.Create(position, 4000, output, PlayerHost::Admit, &access.receipt, RopesHost::Create) == ClockResult::BindingChanged && output == 123 &&
                  !access.receipt.entered,
              "native rope action requires writable genuine image table pages");
        RopeProtect(base, RP::RowCount * RP::RowBytes, PAGE_READWRITE);
        MissingRopes missing;
        CRopesDE     unavailable(missing);
        Refuses([&] { unavailable.CreateRopeForSwatPed(position); }, "absent owner never publishes a synthetic rope ID");
        Refuses([&] { unavailable.RemoveEntityRope(nullptr); }, "default removal is an explicit refusal rather than a no-op service");
        clear();
        counter = 5;
        memory.Reset();
        Check(counter == 5 && !memory.IsBound(), "retirement releases only binding, preserving persistent native rope intent");
        PlayerHost::buildAllowed = false;
        Check(memory.Bind() == ClockResult::UnsupportedBuild, "new binding refuses unknown executable identity");
        PlayerHost::buildAllowed = true;
    }
}
int wmain(int argc, wchar_t** argv)
{
    Check(argc == 2, "explicit workspace-owned rope fixture directory");
    OwnedImage image;
    image.Create(argv[1]);
    ExerciseRopes(image);
    std::printf("Passed %u native ropes and actual SDK checks (64-bit)\n", checks);
    return 0;
}
