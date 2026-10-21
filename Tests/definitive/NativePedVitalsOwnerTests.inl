/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow generic-ped vitals through the same registered-cell owner
 ****************************************************************************/
namespace
{
    struct PedVitalsOwnerContext
    {
        NativePedReferenceToken token;
        NativePedVitalField     field = NativePedVitalField::Health;
        ClockResult             result = ClockResult::NotInitialized;
        std::uint32_t           operation = 0;
        float                   value = 0, output = 123;
    };
    void PedVitalsOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedVitalsOwnerContext*>(opaque);
        switch (context.operation)
        {
            case 0:
                context.result = access.AcquirePedReference(1, context.token);
                break;
            case 1:
                context.result = access.ReadPedVital(context.token, context.field, context.output);
                break;
            case 2:
                context.result = access.SetPedVital(context.token, context.field, context.value);
                break;
            default:
                std::abort();
        }
    }
    void NativePedVitalsOwnedSdk(ClockImageFixture& fixture)
    {
        namespace V = NativePedVitalsProfile;
        namespace R = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace P = NativePlayerInfoProfile;
        struct Saved
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
        };
        std::vector<Saved> saved;
        const auto         admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            Saved  prior{rva, std::vector<unsigned char>(size)};
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), prior.bytes.data(), size, &copied) && copied == size,
                  "save bounded generic vitals and reference proof spans in the actual callback fixture");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "vitals owner keeps exact code, vtable and mutable-data page policies");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : R::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : V::Code)
            admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : R::Slots)
        {
            const auto target = fixture.image + slot.targetRva;
            admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        const std::uintptr_t zero = 0;
        const std::uint32_t  state = 9, marker = 0;
        const auto           thread = GetCurrentThreadId();
        const unsigned char  initialized = 1, playerIndex = 0;
        admit(Ped::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        admit(P::PlayerIndexRva, &playerIndex, sizeof(playerIndex), PAGE_READWRITE);
        std::vector<unsigned char> row(P::RowBytes);
        admit(P::BankRva, row.data(), row.size(), PAGE_READWRITE);
        std::vector<unsigned char> nodes(R::ReferenceCount * R::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < R::ReferenceCount; ++i)
        {
            const auto next = fixture.image + R::ReferencesRva + (i + 1) * R::ReferenceBytes;
            std::memcpy(nodes.data() + i * R::ReferenceBytes, &next, sizeof(next));
        }
        admit(R::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        const auto first = fixture.image + R::ReferencesRva;
        admit(R::FreeHeadRva, &first, sizeof(first), PAGE_READWRITE);
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto vtable = fixture.image + R::PedVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[V::TypeOffset] = 3;
        Check(!fixture.Get<std::uintptr_t>(P::BankRva), "the generic occupied pool row has no current-player root or PlayerInfo binding");
        ClockPedVitalsOwnerTest::flag = reinterpret_cast<std::uintptr_t>(pool.flags);
        const auto store = [&](std::size_t at, std::uint32_t bits) { std::memcpy(pool.objects + at, &bits, sizeof(bits)); };
        const auto bits = [&](std::size_t at)
        {
            std::uint32_t value = 0;
            std::memcpy(&value, pool.objects + at, sizeof(value));
            return value;
        };
        const auto floatBits = [](float value)
        {
            std::uint32_t result = 0;
            std::memcpy(&result, &value, sizeof(result));
            return result;
        };
        const auto fromBits = [](std::uint32_t value)
        {
            float result = 0;
            std::memcpy(&result, &value, sizeof(result));
            return result;
        };
        store(V::HealthOffset, 0x3f800001);
        store(V::MaxHealthOffset, 0x7fc05678);
        store(V::ArmorOffset, 0x40000001);
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = ClockPedVitalsOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        ClockPedVitalsOwnerTest::stores = 0;
        ClockPedVitalsOwnerTest::loseIdentity = false;
        PedVitalsOwnerContext context;
        Check(runtime.access.ReadPedVital(context.token, context.field, context.output) == ClockResult::NotInitialized && context.output == 123 &&
                  runtime.access.SetPedVital(context.token, context.field, 7) == ClockResult::NotInitialized && !ClockPedVitalsOwnerTest::stores,
              "unbound vitals refuse without exposing a raw ped or changing caller output");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "the same callback retires its real reference cell after vitals borrowing stops");
            }
            Check(!service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.playerInfo.IsBound() && !runtime.timingHandler &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "vitals retirement releases real registered storage and player binding before owner acknowledgement");
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedVitalsOwnerHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew one actual callback lease between independent vitals requests");
                fixture.invokeTimer();
            };
            invoke();
            Check(context.result == ClockResult::Ok && runtime.pedReferences.IsRetained(), "vitals use a token issued by real verified registration");
            Check(runtime.access.ReadPedVital(context.token, context.field, context.output) != ClockResult::Ok && context.output == 123 &&
                      runtime.access.SetPedVital(context.token, context.field, 7) != ClockResult::Ok && !ClockPedVitalsOwnerTest::stores,
                  "the registered token supplies no between-callback vitals read or mutation authority");
            context.operation = 1;
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x3f800001 && !runtime.playerInfo.IsBound() &&
                      !fixture.Get<std::uintptr_t>(P::BankRva),
                  "an actual later callback resolves generic-ped vitals with the current-player root absent");
            context.operation = 2;
            context.value = fromBits(0x7fc01234);
            invoke();
            Check(context.result == ClockResult::Ok && bits(V::HealthOffset) == 0x7fc01234 && bits(V::MaxHealthOffset) == 0x7fc05678 &&
                      bits(V::ArmorOffset) == 0x40000001 && ClockPedVitalsOwnerTest::stores == 1,
                  "raw health CAS preserves NaN payload and leaves native max-health and armour neighbours untouched");
            context.operation = 1;
            invoke();
            Check(context.result == ClockResult::Ok && floatBits(context.output) == 0x7fc01234, "the actual token read preserves accepted raw health bits");
            const auto token = context.token;
            ++context.token.version;
            context.output = 123;
            invoke();
            Check(context.result != ClockResult::Ok && context.output == 123 && ClockPedVitalsOwnerTest::stores == 1 && !runtime.stopping.load(),
                  "a stale CPU token refuses within a genuine callback without writes or stopping the unrelated valid owner");
            context.token = token;
            context.operation = 2;
            context.field = NativePedVitalField::Armor;
            context.value = fromBits(0x80000000);
            invoke();
            Check(context.result == ClockResult::Ok && bits(V::ArmorOffset) == 0x80000000 && bits(V::HealthOffset) == 0x7fc01234 &&
                      bits(V::MaxHealthOffset) == 0x7fc05678,
                  "the second real native scalar preserves signed zero and every neighbouring health field");
            retire(service);
            Check(bits(V::HealthOffset) == 0x7fc01234 && bits(V::ArmorOffset) == 0x80000000,
                  "End retires pointer-cell lifetime without rolling back accepted health or armour intent");
        }
        {
            context.operation = 0;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &PedVitalsOwnerHandler, &context);
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "the next generation independently registers its generic-ped token");
            const auto stores = ClockPedVitalsOwnerTest::stores;
            context.operation = 2;
            context.field = NativePedVitalField::Health;
            context.value = fromBits(0xffc07654);
            ClockPedVitalsOwnerTest::loseIdentity = true;
            Check(service.Renew(runtime.generation.load()), "renew before one post-store acknowledgement fault");
            fixture.invokeTimer();
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockPedVitalsOwnerTest::stores == stores + 1 &&
                      bits(V::HealthOffset) == 0xffc07654 && bits(V::MaxHealthOffset) == 0x7fc05678,
                  "real post-store pool-generation loss stops the owner while preserving the accepted raw scalar");
            Check(runtime.access.SetPedVital(context.token, context.field, 9) != ClockResult::Ok && ClockPedVitalsOwnerTest::stores == stores + 1,
                  "an entered acknowledgement failure is never replayed through a producer call");
            // Restore only this owned fixture generation so exact registered-
            // cell cleanup can proceed; accepted vital bits are never replayed.
            pool.flags[0] = 1;
            retire(service);
            Check(bits(V::HealthOffset) == 0xffc07654 && bits(V::ArmorOffset) == 0x80000000,
                  "reference cleanup after failed acknowledgement never restores an accepted native field");
        }
        ClockPedVitalsOwnerTest::loseIdentity = false;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = ClockPedVitalsOwnerTest::image = ClockPedVitalsOwnerTest::flag = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped vitals callback owner fixture completed");
        std::puts("Production native generic ped vitals callback owner fixture completed");
    }
}
