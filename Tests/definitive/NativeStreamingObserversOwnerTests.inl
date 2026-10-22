/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit group/archive facts only on the actual retained callback
 ****************************************************************************/
{
    namespace T = NativeStreamingModelGroupsProfile;
    namespace A = NativeModelArchiveProfile;
    for (const auto& pin : T::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    for (const auto& pin : A::Code)
        admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    const auto archiveGroup = fixture.image + A::GroupRva;
    const auto archiveBranch = static_cast<std::uint32_t>(A::BranchRva);
    admit(A::GroupSlotRva, &archiveGroup, sizeof(archiveGroup), PAGE_READONLY);
    // The embedded jump table shares its native text page with the false
    // epilogue; its synthetic page must retain that executable protection.
    admit(A::BranchTableRva + A::BranchIndex * sizeof(archiveBranch), &archiveBranch, sizeof(archiveBranch), PAGE_EXECUTE_READ);
    NativeStreamingModelGroupsReader::Value groups;
    groups.pedCount = 0xffffffff;
    groups.peds.fill(-1);
    groups.vehicles.fill(-1);
    groups.peds[0] = -2;
    groups.peds[1] = 0x10000;
    groups.peds[23] = 0xffff;
    groups.vehicles[0] = 17;
    groups.vehicles[1] = -2;
    groups.vehicles[2] = 18;
    groups.vehicles[3] = -1;
    groups.vehicles[4] = 19;
    admit(T::PedCountRva, &groups.pedCount, sizeof(groups.pedCount), PAGE_READWRITE);
    admit(T::PedSlotsRva, groups.peds.data(), sizeof(groups.peds), PAGE_READWRITE);
    admit(T::VehicleSlotsRva, groups.vehicles.data(), sizeof(groups.vehicles), PAGE_READWRITE);
    NativeStreamingModelGroupsReader::Value preserved;
    preserved.pedCount = 77;
    preserved.peds.fill(123);
    preserved.vehicles.fill(321);
    auto observed = preserved;
    bool allocated = true;
    Check(absent.ReadStreamingModelGroups(observed) == ClockResult::NotInitialized && observed.pedCount == preserved.pedCount &&
              observed.peds == preserved.peds && observed.vehicles == preserved.vehicles &&
              absent.ReadModelArchiveAllocated(3, allocated) == ClockResult::NotInitialized && allocated,
          "older owners cannot manufacture empty groups or absent archives and preserve their previous outputs");
    NativeClockService service;
    BeginOwned(service, fixture, false, true, true, &ModelOwnershipOwnerHandler, &context);
    Check(runtime.access.ReadStreamingModelGroups(observed) != ClockResult::Ok && observed.pedCount == 77 && observed.peds == preserved.peds &&
              observed.vehicles == preserved.vehicles && runtime.access.ReadModelArchiveAllocated(3, allocated) != ClockResult::Ok && allocated,
          "retained owner outside actual callback cannot publish tracker values or archive membership");
    context.operation = ModelOwnershipOperation::ModelGroups;
    context.groups = preserved;
    fixture.invokeTimer();
    Check(context.result == ClockResult::Ok && context.groups.pedCount == 0xffffffff && context.groups.peds == groups.peds &&
              context.groups.vehicles == groups.vehicles && !runtime.stopping.load(),
          "actual callback observes all native24/50 slots and the raw ped counter without clamping or object leases");
    Check(NativeStreamingModelGroupsReader::IsPedTracked(context.groups, 0xffff) && !NativeStreamingModelGroupsReader::IsPedTracked(context.groups, 0) &&
              NativeStreamingModelGroupsReader::VehicleCount(context.groups) == 3 && NativeStreamingModelGroupsReader::IsVehicleTracked(context.groups, 18) &&
              !NativeStreamingModelGroupsReader::IsVehicleTracked(context.groups, 19),
          "current group facts retain real MTA sign and sentinel selection rules");
    const auto badGroup = static_cast<unsigned char>(T::Code[0].bytes[0] ^ 1);
    fixture.Patch(T::Code[0].rva, &badGroup, 1);
    context.groups = preserved;
    Check(service.Renew(runtime.generation.load()), "renew before read-only group profile refusal");
    fixture.invokeTimer();
    Check(context.result != ClockResult::Ok && context.groups.pedCount == preserved.pedCount && context.groups.peds == preserved.peds &&
              context.groups.vehicles == preserved.vehicles && !runtime.stopping.load(),
          "changed group producer code refuses without fabricated empty output or corrupting the callback owner");
    fixture.Patch(T::Code[0].rva, T::Code[0].bytes, 1);
    context.operation = ModelOwnershipOperation::ArchiveAllocated;
    context.index = 3;
    const auto slot = NativeModelBankProfile::BankRva + 3 * sizeof(std::uintptr_t);
    const auto flags = L::ArrayRva + 3 * L::RowStride + L::FlagsOffset;
    // The native named query observes only an opaque nonnull bank word and
    // flag80. An instantiated model or provider graph is not its predicate.
    fixture.Put(slot, std::uintptr_t{1});
    for (const auto nativeFlags : std::array<unsigned char, 4>{0, 0x10, 0x80, 0xff})
    {
        fixture.Put(flags, nativeFlags);
        fixture.Put(L::ArrayRva + 3 * L::RowStride + L::StateOffset, static_cast<unsigned char>(2));
        context.loaded = (nativeFlags & 0x80) != 0;
        Check(service.Renew(runtime.generation.load()), "renew same callback for genuine named archive query");
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && context.loaded == ((nativeFlags & 0x80) == 0) && !runtime.stopping.load(),
              "actual archive owner preserves genuine flag80 result independently of raw loaded state and metadata dereference");
    }
    fixture.Put(slot, std::uintptr_t{0});
    fixture.Put(flags, static_cast<unsigned char>(0));
    context.loaded = true;
    Check(service.Renew(runtime.generation.load()), "renew current absent-bank archive observation");
    fixture.invokeTimer();
    Check(context.result == ClockResult::Ok && !context.loaded && !runtime.stopping.load(),
          "qualified current null bank is a genuine false archive result rather than generic owner refusal");
    context.index = 0xffffffff;
    context.loaded = true;
    Check(service.Renew(runtime.generation.load()), "renew before full-DWORD archive bounds refusal");
    fixture.invokeTimer();
    Check(context.result == ClockResult::InvalidArgument && context.loaded && !runtime.stopping.load(),
          "invalid complete model key refuses and preserves boolean instead of manufacturing absent archive");
    fixture.Put(slot, modelAddress);
    fixture.Put(flags, static_cast<unsigned char>(0));
    fixture.Put(L::ArrayRva + 3 * L::RowStride + L::StateOffset, static_cast<unsigned char>(1));
    context.index = 3;
    const auto badArchive = static_cast<unsigned char>(A::Code[0].bytes[0] ^ 1);
    fixture.Patch(A::Code[0].rva, &badArchive, 1);
    Check(service.Renew(runtime.generation.load()), "renew before read-only archive profile refusal");
    fixture.invokeTimer();
    Check(context.result != ClockResult::Ok && context.loaded && !runtime.stopping.load(),
          "archive profile loss preserves caller output and remains distinct from successful false membership");
    fixture.Patch(A::Code[0].rva, A::Code[0].bytes, 1);
    Check(service.Renew(runtime.generation.load()), "same owner remains usable after clean observer refusals");
    fixture.invokeTimer();
    Check(context.result == ClockResult::Ok && context.loaded && !runtime.stopping.load(), "genuine archive query resumes on the same retained callback");
    retire(service);
    std::puts("Production native streaming groups callback owner fixture completed");
    std::puts("Production native model archive callback owner fixture completed");
}
