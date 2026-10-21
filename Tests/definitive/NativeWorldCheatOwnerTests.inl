/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Same production runtime owner for14 real named cheat reads
 ****************************************************************************/
struct WorldCheatReadContext
{
    MtaGame::CWorldCheatPolicy::Id id = MtaGame::CWorldCheatPolicy::Id::TankMode;
    bool                           output = true;
    ClockResult                    result = ClockResult::NotInitialized;
    unsigned                       calls = 0, refusal = 0;
};

void WorldCheatReadHandler(NativeGameTimingAccess& access, void* opaque)
{
    auto& context = *static_cast<WorldCheatReadContext*>(opaque);
    ++context.calls;
    Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load(),
          "named cheat reads borrow the same actual post-original timer owner");
    const auto generation = timingCallbackGeneration;
    const auto admission = runtime.admissionGeneration.load();
    if (context.refusal == 1)
        ++timingCallbackGeneration;
    if (context.refusal == 2)
        runtime.admissionGeneration.store(admission + 1);
    if (context.refusal == 3)
        ++callbackDepth;
    context.result = access.ReadWorldCheat(context.id, context.output);
    if (context.refusal == 3)
        --callbackDepth;
    runtime.admissionGeneration.store(admission);
    timingCallbackGeneration = generation;
}

void NativeWorldCheatRegistryOwner(ClockImageFixture& fixture)
{
    namespace P = NativeCheatProfile;
    using Policy = MtaGame::CWorldCheatPolicy;
    using Id = Policy::Id;
    NativeClockService    service;
    WorldCheatReadContext context;
    bool                  value = true;
    Check(runtime.access.ReadWorldCheat(Id::TankMode, value) == ClockResult::NotInitialized && value,
          "a missing native owner cannot publish a named cheat observation");
    ClockReaderTestHost::image = fixture.image;
    BeginOwned(service, fixture, false, true, true, &WorldCheatReadHandler, &context);
    fixture.AdmitCheatPins();
    for (const auto& member : P::ReadMembers)
    {
        fixture.Patch(member.consumer.rva, member.consumer.bytes.data(), member.consumer.size);
        const std::uint64_t action = member.actionRva ? fixture.image + member.actionRva : 0;
        fixture.Patch(P::DispatchTableRva + member.nativeIndex * sizeof(action), &action, sizeof(action));
        const unsigned char clear = 0;
        fixture.Patch(P::FlagsRva + member.nativeIndex, &clear, 1);
    }
    for (const auto& registered : P::ReadRegistrations)
    {
        const auto&                  registration = registered.native;
        std::array<unsigned char, 7> instruction{0x4c, 0x8d, 0x05};
        const auto                   displacement = static_cast<std::int32_t>(registration.nameRva - registration.instructionRva - instruction.size());
        std::memcpy(instruction.data() + 3, &displacement, sizeof(displacement));
        fixture.Patch(registration.instructionRva, instruction.data(), instruction.size());
        fixture.Patch(registration.nameRva, registration.name, std::strlen(registration.name) + 1);
    }
    const auto originalFlags = fixture.Get<std::array<unsigned char, P::NativeFlagCount>>(P::FlagsRva);
    const auto writesBefore = ClockTestFaults::clockDataWrites.load();
    for (const auto& member : P::ReadMembers)
    {
        context.id = member.id;
        context.output = true;
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && !context.output, "all14 real rows read clear native values through production admission");
        const unsigned char raw = 2;
        fixture.Patch(P::FlagsRva + member.nativeIndex, &raw, 1);
        const auto before = fixture.Get<std::array<unsigned char, P::NativeFlagCount>>(P::FlagsRva);
        context.output = false;
        fixture.invokeTimer();
        Check(context.result == ClockResult::Ok && context.output && fixture.Get<std::array<unsigned char, P::NativeFlagCount>>(P::FlagsRva) == before,
              "raw nonzero values remain true without native writes or neighbor edits");
        const unsigned char clear = 0;
        fixture.Patch(P::FlagsRva + member.nativeIndex, &clear, 1);
    }
    for (const auto& logical : Policy::Members)
        if (!P::FindRead(logical.id))
        {
            context.id = logical.id;
            context.output = true;
            fixture.invokeTimer();
            Check(context.result == ClockResult::UnsupportedBuild && context.output,
                  "each of five known unresolved names refuses explicitly rather than returning successful false");
        }
    context.id = static_cast<Id>(255);
    fixture.invokeTimer();
    Check(context.result == ClockResult::InvalidArgument && context.output, "invalid logical IDs refuse without output publication");
    context.id = Id::TankMode;
    value = false;
    Check(runtime.access.ReadWorldCheat(context.id, value) != ClockResult::Ok && !value,
          "same OS thread outside the real callback cannot borrow named cheat reads");
    bool        foreignRefused = false;
    std::thread foreign(
        [&]
        {
            bool output = true;
            foreignRefused = runtime.access.ReadWorldCheat(Id::TankMode, output) != ClockResult::Ok && output;
        });
    foreign.join();
    Check(foreignRefused, "foreign thread cannot borrow the retained engine owner");
    for (unsigned refusal = 1; refusal <= 3; ++refusal)
    {
        context.refusal = refusal;
        context.output = true;
        fixture.invokeTimer();
        Check(context.result == (refusal == 3 ? ClockResult::WrongThread : ClockResult::OwnerConflict) && context.output,
              "generation admission and nesting refusal preserve the caller output");
    }
    context.refusal = 0;
    const auto* tank = P::FindRead(Id::TankMode);
    Check(tank != nullptr, "test selects a qualified independent readonly member");
    const auto& alias = P::ReadRegistrations[0].native;
    const char  changed = '?';
    fixture.Patch(alias.nameRva, &changed, 1);
    fixture.invokeTimer();
    Check(context.result == ClockResult::BindingChanged && context.output, "foreign selected native alias prevents publication");
    fixture.Patch(alias.nameRva, alias.name, 1);
    const std::uint64_t wrongAction = fixture.image + 0x10708d0;
    fixture.Patch(P::DispatchTableRva + tank->nativeIndex * sizeof(wrongAction), &wrongAction, sizeof(wrongAction));
    fixture.invokeTimer();
    Check(context.result == ClockResult::BindingChanged && context.output, "a redirected native action slot cannot validate the same row");
    const std::uint64_t absent = 0;
    fixture.Patch(P::DispatchTableRva + tank->nativeIndex * sizeof(absent), &absent, sizeof(absent));
    ClockTestFaults::cheatWriteAddress = fixture.image + P::FlagsRva + tank->nativeIndex;
    ClockTestFaults::failNextCheatRead.store(true);
    fixture.invokeTimer();
    Check(context.result == ClockResult::ReadFailure && context.output && !ClockTestFaults::failNextCheatRead.load(),
          "actual OS field-copy failure is not a successful disabled native value");
    ClockTestFaults::cheatAfterRead = [] { ++timingCallbackGeneration; };
    fixture.invokeTimer();
    Check(context.result == ClockResult::OwnerConflict && context.output && !ClockTestFaults::cheatAfterRead,
          "scope revoked during the real field copy cannot publish a stale result");
    ClockTestFaults::cheatWriteAddress = 0;
    Check(ClockTestFaults::clockDataWrites.load() == writesBefore && !runtime.nativeTouched.load() && !runtime.timingCaptured.load() &&
              !runtime.timingDirty.load() && fixture.Get<std::array<unsigned char, P::NativeFlagCount>>(P::FlagsRva) == originalFlags,
          "readonly registry requests own no data writes timing intent or restoration snapshots");

    // The actual existing freeze transaction owns this exact clock branch.
    // The new read descriptor ends before it and never masks foreign bytes.
    fixture.invoke();
    auto state = Execute(service, fixture, ClockOperation::Freeze);
    Check(state.lastCommandResult == ClockResult::Ok && state.patchInstalled, "existing clock owner installs its exact reviewed freeze branch");
    context.id = Id::FasterClock;
    context.output = true;
    fixture.invokeTimer();
    Check(context.result == ClockResult::Ok && !context.output, "fasterclock read accepts the existing owner's exact frozen code identity");
    auto foreignPatch = NativeClockProfile::frozenPatch;
    foreignPatch[1] ^= 1;
    fixture.Patch(NativeClockProfile::patchRva, foreignPatch.data(), foreignPatch.size());
    const auto calls = context.calls;
    fixture.invokeTimer();
    Check(context.calls == calls && runtime.failure.load() == ClockResult::BindingChanged,
          "arbitrary changed clock bytes refuse before entering the named-read handler");
    fixture.Patch(NativeClockProfile::patchRva, NativeClockProfile::frozenPatch.data(), NativeClockProfile::frozenPatch.size());
    FinishOwned(service, fixture);
    Check(fixture.Get<std::array<unsigned char, P::NativeFlagCount>>(P::FlagsRva) == originalFlags,
          "same-owner cleanup never rewinds or rewrites native cheat bytes after readonly requests");
    value = true;
    Check(runtime.access.ReadWorldCheat(Id::TankMode, value) == ClockResult::NotInitialized && value, "retired owner cannot publish cached registry state");
    ClockReaderTestHost::image = 0;
    std::puts("Production native fourteen-row world cheat registry read owner fixture completed");
}
