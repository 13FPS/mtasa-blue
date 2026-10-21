/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real registered-token metadata read under the existing callback
 ****************************************************************************/
namespace
{
    struct PedMetadataOwnerContext
    {
        NativePedReferenceToken token;
        NativePedMetadata       output{0xbeef, 0xee};
        ClockResult             result = ClockResult::NotInitialized;
        bool                    read = false;
    };
    void PedMetadataOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PedMetadataOwnerContext*>(opaque);
        context.result = context.read ? access.ReadPedMetadata(context.token, context.output) : access.AcquirePedReference(1, context.token);
    }
    void NativePedMetadataOwnedSdk(ClockImageFixture& fixture)
    {
        namespace P = NativePedReferenceProfile;
        namespace Ped = NativePedPoolProfile;
        namespace M = NativePedMetadataProfile;
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
                  "save only bounded pointer-cell proof spans in the existing owned callback image");
            saved.push_back(std::move(prior));
            fixture.Patch(rva, bytes, size);
            DWORD previous = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &previous) != FALSE,
                  "native reference owner preserves real code, relocated-slot and mutable-bank page policies");
        };
        for (const auto& pin : Ped::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : P::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : M::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        for (const auto& slot : P::Slots)
        {
            const auto target = fixture.image + slot.targetRva;
            admit(slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
        const std::uintptr_t zero = 0;
        const std::uint32_t  state = 9, marker = 0;
        const auto           thread = GetCurrentThreadId();
        const unsigned char  initialized = 1;
        admit(Ped::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativePlayerInfoProfile::BankRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &state, sizeof(state), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        std::vector<unsigned char> nodes(P::ReferenceCount * P::ReferenceBytes);
        for (std::size_t i = 0; i + 1 < P::ReferenceCount; ++i)
        {
            const auto next = fixture.image + P::ReferencesRva + (i + 1) * P::ReferenceBytes;
            std::memcpy(nodes.data() + i * P::ReferenceBytes, &next, sizeof(next));
        }
        admit(P::ReferencesRva, nodes.data(), nodes.size(), PAGE_READWRITE);
        const auto first = fixture.image + P::ReferencesRva;
        admit(P::FreeHeadRva, &first, sizeof(first), PAGE_READWRITE);
        // Reuse the actual handle-bank fixture. This service needs no SDK ped,
        // ambient player, matrix or actor graph to register one qualified row.
        ClockTeleportPool pool(fixture, Ped::RootRva, Ped::NativeCapacity, Ped::ObjectStride);
        pool.Publish();
        pool.flags[0] = 1;
        const auto vtable = fixture.image + P::PedVtableRva;
        std::memcpy(pool.objects, &vtable, sizeof(vtable));
        pool.objects[P::TypeOffset] = 0xab;
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = fixture.image;
        ClockPedReferenceOwnerTest::registers = ClockPedReferenceOwnerTest::unregisters = 0;
        ClockPedReferenceOwnerTest::partialUnlink = false;
        ClockPedReferenceOwnerTest::unregisterResult = ClockResult::Ok;
        const std::uint16_t model = 0xffff, neighbour = 0x5678;
        std::memcpy(pool.objects + M::ModelOffset, &model, sizeof(model));
        std::memcpy(pool.objects + M::ModelOffset + sizeof(model), &neighbour, sizeof(neighbour));
        PedMetadataOwnerContext context;
        NativeClockService      service;
        BeginOwned(service, fixture, false, true, true, &PedMetadataOwnerHandler, &context);
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && runtime.pedReferences.IsRetained(),
              "metadata borrows a token published only after actual reference registration");
        Check(runtime.access.ReadPedMetadata(context.token, context.output) != ClockResult::Ok && context.output.modelIndex == 0xbeef &&
                  context.output.entityType == 0xee,
              "a producer with a valid CPU token cannot read native metadata or replace caller output");
        context.read = true;
        const auto invoke = [&]
        {
            Check(service.Renew(runtime.generation.load()), "renew the real lease between metadata callback requests");
            fixture.invokeTimer();
        };
        std::array<unsigned char, Ped::ObjectStride> original{};
        std::memcpy(original.data(), pool.objects, original.size());
        invoke();
        Check(context.result == ClockResult::Ok && context.output.modelIndex == 0xffff && context.output.entityType == 3 && !runtime.playerInfo.IsBound() &&
                  !fixture.Get<std::uintptr_t>(NativePlayerInfoProfile::BankRva),
              "the actual generic-ped callback reads raw model WORD and masked type with no ambient current player");
        Check(!std::memcmp(original.data(), pool.objects, original.size()), "native metadata observation changes no native ped or neighbouring field");
        const auto token = context.token;
        ++context.token.version;
        context.output = {0xbeef, 0xee};
        invoke();
        Check(context.result != ClockResult::Ok && context.output.modelIndex == 0xbeef && context.output.entityType == 0xee && !runtime.stopping.load(),
              "a stale registered token refuses without a fake enum, output change or unrelated owner stop");
        context.token = token;
        const std::uint16_t next = 0x8123;
        std::memcpy(pool.objects + M::ModelOffset, &next, sizeof(next));
        std::memcpy(original.data(), pool.objects, original.size());
        invoke();
        Check(context.result == ClockResult::Ok && context.output.modelIndex == next && context.output.entityType == 3 &&
                  !std::memcmp(original.data(), pool.objects, original.size()),
              "a later callback reads the current raw unsigned model value without caching or native writes");
        const auto unlink = ClockPedReferenceOwnerTest::unregisters;
        Check(!service.End() && runtime.pedReferences.IsRetained() && ClockPedReferenceOwnerTest::unregisters == unlink,
              "producer End retains real native-visible cells until their natural cleanup callback");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.pedReferences.IsRetained() && !runtime.timingHandler &&
                  ClockPedReferenceOwnerTest::unregisters == unlink + 1 && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "metadata shares the actual reference retirement gate and releases no cell prematurely");
        ClockReaderTestHost::image = ClockPedReferenceOwnerTest::image = 0;
        for (auto item = saved.rbegin(); item != saved.rend(); ++item)
            fixture.Patch(item->rva, item->bytes.data(), item->bytes.size());
        std::puts("Production native ped metadata callback owner fixture completed");
    }
}
