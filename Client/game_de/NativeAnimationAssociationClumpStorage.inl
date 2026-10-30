/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Causal clump operations over the existing actual issued104B owner
 ****************************************************************************/
// Included inside NativeAnimationAssociationStorageAccess. The caller owns the
// actual storage/controller; a stack origin never adopts another native row.
struct ClumpPlanContext
{
    const NativeAnimationAssociationClumpOrigin*         origin = nullptr;
    const NativeAnimationAssociationHierarchyDependency* hold = nullptr;
    C                                                    owner = nullptr;
};
static ClockResult ClumpPlanRecheck(const void* opaque) noexcept
{
    const auto& context = *static_cast<const ClumpPlanContext*>(opaque);
    return context.origin ? context.origin->Recheck() : ClockResult::OwnerConflict;
}
static ClockResult ClumpPlanReadHierarchy(const void* opaque, NativeAnimationHierarchyValues& output, std::uintptr_t& sequences) noexcept
{
    const auto& context = *static_cast<const ClumpPlanContext*>(opaque);
    if (!context.origin)
        return ClockResult::OwnerConflict;
    const auto& origin = *context.origin;
    if (origin.IsPrivateHierarchy())
    {
        if (!context.hold || !context.hold->IsHeld())
            return ClockResult::OwnerConflict;
        return origin.Hierarchies().ReadForAssociation(origin.Image(), *context.hold, context.owner, output, sequences);
    }
    NativeAnimationHierarchyMemory memory({origin.Image()}, context.owner);
    NativeAnimationHierarchyValues next;
    std::uintptr_t                 nextSequences = 0;
    auto                           result = memory.Read(origin.Hierarchy().locator, next);
    if (result == ClockResult::Ok)
        result = memory.ReadSequences(origin.Hierarchy().locator, nextSequences);
    if (result != ClockResult::Ok)
        return result;
    output = next;
    sequences = nextSequences;
    return ClockResult::Ok;
}
static ClockResult ClumpInvokeConstruct(NativeAnimationAssociationCalls::ClumpConstructorFunction function, std::uintptr_t receiver, std::uintptr_t clump,
                                        std::uintptr_t hierarchy, std::uintptr_t& returned, std::uint32_t& exception) noexcept
{
    __try
    {
        returned = function(receiver, clump, hierarchy);
        return ClockResult::Ok;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        exception = GetExceptionCode();
        return ClockResult::OriginalException;
    }
}
static ClockResult ClumpInvokeInitialize(NativeAnimationAssociationCalls::ClumpInitializeFunction function, std::uintptr_t receiver, std::uintptr_t clump,
                                         std::uintptr_t hierarchy, std::uint32_t& exception) noexcept
{
    __try
    {
        function(receiver, clump, hierarchy);
        return ClockResult::Ok;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        exception = GetExceptionCode();
        return ClockResult::OriginalException;
    }
}
static ClockResult ClumpExchangeDefault(std::uintptr_t address, std::size_t width, std::uint64_t expected, std::uint64_t target, std::uint64_t& observed,
                                        bool& accepted, std::uint32_t& exception) noexcept
{
    accepted = false;
    __try
    {
        if (width == 2)
            observed = static_cast<std::uint16_t>(
                InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), static_cast<SHORT>(target), static_cast<SHORT>(expected)));
        else if (width == 4)
            observed = static_cast<std::uint32_t>(
                InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(target), static_cast<LONG>(expected)));
        else if (width == 8)
            observed = static_cast<std::uint64_t>(
                InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address), static_cast<LONG64>(target), static_cast<LONG64>(expected)));
        else
            return ClockResult::InvalidArgument;
        const auto mask = width == 8 ? ~std::uint64_t{0} : (std::uint64_t{1} << (width * 8)) - 1;
        accepted = observed == (expected & mask);
        return ClockResult::Ok;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        exception = GetExceptionCode();
        return ClockResult::OriginalException;
    }
}
static ClockResult ClumpReleaseDependencies(std::uintptr_t image, S& s, std::uint64_t generation, C owner) noexcept
{
    using Dependency = NativeAnimationAssociationHierarchyDependency;
    using Release = NativeAnimationAssociationHierarchyRelease;
    if (!s.data || !generation || s.data->generation != generation)
        return ClockResult::OwnerConflict;
    auto& d = *s.data;
    if (!d.clump)
        return ClockResult::Ok;
    // Node-only release leaves metadata/H live. Only the actual outer release
    // or original deleting-destructor ACK can authorize these CPU-key releases.
    if (!d.clumpRetiredAck && (d.constructorEntered || !d.outer.CanDiscard()))
        return ClockResult::OwnerConflict;
    const auto release = [&](std::unique_ptr<Dependency>& held) noexcept
    {
        if (!held)
            return ClockResult::Ok;
        if (held->CanDiscard())
        {
            held.reset();
            return ClockResult::Ok;
        }
        if (held->target != &s || held->image != image || !held->controller || held->receiver.ownerGeneration != generation)
            return ClockResult::OwnerConflict;
        const Release proof(*held, Release::Reason::ReceiverRetired, held->command);
        const auto    result = held->controller->ReleaseForAssociation(image, *held, proof, owner, nullptr);
        // A lost final owner ACK cannot retract an already known key release.
        if (held->CanDiscard())
            held.reset();
        return result;
    };
    if (d.clump->pending)
    {
        auto result = release(d.clump->pending->newHold);
        if (result != ClockResult::Ok)
            return result;
        if ((result = release(d.clump->pending->oldHold)) != ClockResult::Ok)
            return result;
    }
    const auto result = release(d.clump->hold);
    if (result != ClockResult::Ok)
        return result;
    d.clump.reset();
    return ClockResult::Ok;
}
static ClockResult ApplyClumpCore(std::uintptr_t image, S& s, std::uint64_t generation, NativeAnimationAssociationClumpOperation operation,
                                  const NativeAnimationAssociationClumpOrigin& origin, C owner, std::uintptr_t& output,
                                  NativeAnimationAssociationCalls::ClumpReceipt*           receipt,
                                  const NativeAnimationAssociationCalls::ClumpInvocations* supplied) noexcept
{
    using namespace AnimationAssociationStorageDetail;
    namespace Plan = NativeAnimationClumpInitializationPlan;
    namespace Verify = NativeAnimationAssociationClumpVerification;
    namespace Journal = AnimationAssociationClumpStorageDetail;
    using Operation = NativeAnimationAssociationClumpOperation;
    using Dependency = NativeAnimationAssociationHierarchyDependency;
    using Release = NativeAnimationAssociationHierarchyRelease;
    NativeAnimationAssociationCalls::ClumpReceipt r;
    r.receiverGeneration = generation;
    r.receiverSerial = origin.Receiver().serial;
    r.operation = operation;
    r.clump = origin.Input().clump;
    r.nativePed = origin.Input().ped.nativePed;
    r.hierarchy = origin.Hierarchy().locator;
    r.privateHierarchy = origin.IsPrivateHierarchy();
    const auto finish = [&](ClockResult result) noexcept
    {
        r.result = result;
        r.retained = s.IsRetained();
        if (r.hadEffects && result != ClockResult::Ok)
        {
            r.partialAccepted = r.defaultsAccepted != 0 || r.originalEntered;
            r.enteredUnknown = true;
            s.state.store(State::FaultDebt, std::memory_order_release);
        }
        if (s.data && s.data->clump && s.data->clump->pending)
            s.data->clump->pending->receipt = r;
        if (receipt)
            *receipt = r;
        return result;
    };
    if (receipt)
        *receipt = r;
    if (!generation || origin.Image() != image || origin.Generation() != generation || origin.Receiver().ownerGeneration != generation ||
        !origin.Receiver().serial || !owner)
        return finish(ClockResult::OwnerConflict);
    if (operation != Operation::Construct && operation != Operation::Initialize && operation != Operation::InitializeCustom)
        return finish(ClockResult::InvalidArgument);
    const bool acquireGate = !origin.HasHeldGate();
    if (acquireGate && !Context::TryEnter())
        return finish(ClockResult::Busy);
    struct Gate
    {
        bool acquired;
        ~Gate()
        {
            if (acquired)
                NativeStreamingCompletionContext::Leave();
        }
    } gate{acquireGate};
    auto result = origin.Recheck();
    if (result != ClockResult::Ok)
        return finish(result);
    const auto priorState = s.GetState();
    if (!s.data || s.data->generation != generation || (priorState != State::Unconstructed && priorState != State::NodesReleased) ||
        (operation == Operation::Initialize && priorState != State::NodesReleased))
        return finish(ClockResult::OwnerConflict);
    auto& d = *s.data;
    r.receiver = d.receiver;
    const bool hadSource = d.clump && d.clump->acceptedSource;
    const bool copyReference = d.knownConstructed && (!hadSource || d.clump->copyReference);
    r.copyOwnedReference = copyReference;
    if (d.destructorEntered || d.retirementVerified || (d.clump && d.clump->pending) ||
        (priorState == State::Unconstructed && (d.constructorEntered || d.knownConstructed)) ||
        (priorState == State::NodesReleased &&
         (!d.knownConstructed || !d.nodeFreeEntered || !d.nodeFreeReturned || !d.nodeFreeVerified || (d.nodeClearEntered && !d.nodePointerCleared))) ||
        (copyReference && (operation != Operation::Initialize || origin.IsPrivateHierarchy())))
        return finish(ClockResult::OwnerConflict);
    NativeModelReferenceHeap::Value heap;
    if ((result = Qualify(image, s, generation, owner, heap)) != ClockResult::Ok)
        return finish(result);
    if (!Verify::Qualify(image))
        return finish(ClockResult::BindingChanged);
    Verify::Bytes before{};
    if (priorState == State::NodesReleased)
    {
        if ((result = Snapshot(image, s, generation, owner, before, heap)) != ClockResult::Ok)
            return finish(result);
    }
    else if (!M::Copy(d.receiver, before.data(), before.size()))
        return finish(ClockResult::ReadFailure);
    r.priorNodes = Field<std::uintptr_t>(before, 0x20);
    r.priorNodeCount = Field<std::uint16_t>(before, 0x18);
    r.heap = heap.current;
    // All CPU journals and actual new dependency keys precede the first store
    // or original entry. An allocation failure cannot orphan a native result.
    try
    {
        if (!d.clump)
            d.clump = std::make_unique<Journal::State<Bank>>();
        auto& state = *d.clump;
        if ((state.receiver.serial && (state.receiver.ownerGeneration != generation || state.receiver.serial != origin.Receiver().serial)) ||
            state.nextCommand == (std::numeric_limits<std::uint64_t>::max)())
            return finish(ClockResult::OwnerConflict);
        state.receiver = origin.Receiver();
        state.pending = std::make_unique<Journal::Pending<Bank>>();
        auto& pending = *state.pending;
        r.command = ++state.nextCommand;
        pending.receipt = r;
        pending.before = before;
        pending.targetHierarchy = origin.Hierarchy();
        pending.targetPrivate = origin.IsPrivateHierarchy();
        pending.copyReference = copyReference;
        const auto refuse = [&](ClockResult failure) noexcept
        {
            // Wholly unentered new holds can be cancelled by this exact
            // command; custom prefixes and native entry never use this proof.
            if (!r.hadEffects && pending.newHold && !pending.newHold->CanDiscard())
            {
                const Release proof(*pending.newHold, Release::Reason::UnenteredNewHoldCancellation, r.command);
                const auto    released = pending.newHold->controller
                                             ? pending.newHold->controller->ReleaseForAssociation(image, *pending.newHold, proof, owner, nullptr)
                                             : ClockResult::OwnerConflict;
                if (released != ClockResult::Ok && !pending.newHold->CanDiscard())
                {
                    s.state.store(State::FaultDebt, std::memory_order_release);
                    return finish(failure);
                }
            }
            if (!r.hadEffects)
                state.pending.reset();
            return finish(failure);
        };
        Dependency* targetHold = nullptr;
        if (origin.IsPrivateHierarchy())
        {
            r.dependencyPlanned = true;
            if (state.hold && state.privateHierarchy && state.hierarchy.locator == origin.Hierarchy().locator)
            {
                targetHold = state.hold.get();
                result = origin.Hierarchies().RecheckForAssociation(image, *targetHold, owner, nullptr);
            }
            else
            {
                pending.newHold.reset(new Dependency(s, origin.Receiver(), r.command));
                targetHold = pending.newHold.get();
                result = origin.Hierarchies().HoldForAssociation(image, origin.Hierarchy(), *targetHold, owner, nullptr);
            }
            r.dependencyHeld = targetHold && targetHold->IsHeld();
            if (result != ClockResult::Ok)
                return refuse(result);
        }
        else
        {
            if ((result = CaptureBank(image, owner, origin.Hierarchy().locator, pending.targetBank)) != ClockResult::Ok)
                return refuse(result);
            if (copyReference &&
                (pending.targetBank.identity.index != d.source.bank.identity.index || pending.targetBank.identity.name != d.source.bank.identity.name))
                return refuse(ClockResult::OwnerConflict);
        }
        ClumpPlanContext context{&origin, targetHold, owner};
        Plan::Origin     inputOrigin;
        inputOrigin.image = image;
        inputOrigin.input = origin.Input();
        inputOrigin.input.association = d.receiver;
        inputOrigin.owner = owner;
        inputOrigin.receiverBytes = 104;
        inputOrigin.allowEmptyFrames = true;
        inputOrigin.context = &context;
        inputOrigin.temporaryModel = origin.TemporaryModel();
        inputOrigin.recheck = &ClumpPlanRecheck;
        inputOrigin.readHierarchy = &ClumpPlanReadHierarchy;
        if ((result = Plan::Access::CaptureInHeldScope(inputOrigin, pending.plan)) != ClockResult::Ok)
            return refuse(result);
        r.container = pending.plan.container;
        pending.observation.sequenceMap.resize(pending.plan.frameCount);
        pending.suppressed.assign(pending.plan.frameCount, 0);
        pending.cache.assign(pending.plan.frameCount, AnimationAssociationProgressDetail::Cache{});
        NativeAnimationAssociationCalls::ClumpInvocations invocations;
        if (supplied)
            invocations = *supplied;
        if (!invocations.construct)
            invocations.construct = &ClumpInvokeConstruct;
        if (!invocations.initialize)
            invocations.initialize = &ClumpInvokeInitialize;
        if (!invocations.defaults)
            invocations.defaults = &ClumpExchangeDefault;
        NativeModelReferenceHeap::Value currentHeap;
        if ((result = Qualify(image, s, generation, owner, currentHeap)) != ClockResult::Ok ||
            (result = Plan::Access::RecheckInHeldScope(inputOrigin, pending.plan)) != ClockResult::Ok)
            return refuse(result);
        if (!NativeModelReferenceHeap::Same(heap, currentHeap) || !NativeModelReferenceHeap::Same(heap, pending.plan.heap))
            return refuse(ClockResult::BindingChanged);
        Verify::Bytes currentBefore{};
        if (!M::Copy(d.receiver, currentBefore.data(), currentBefore.size()))
            return refuse(ClockResult::ReadFailure);
        if (currentBefore != before)
            return refuse(ClockResult::BindingChanged);
        if (operation == Operation::InitializeCustom)
        {
            constexpr std::array<std::size_t, 13> offsets{0x30, 0x3c, 0x20, 0x34, 0x38, 0x40, 0x46, 0x48, 0, 0x1a, 0x44, 0x10, 8};
            constexpr std::array<std::size_t, 13> widths{4, 4, 8, 4, 4, 4, 2, 4, 8, 2, 2, 8, 8};
            const std::array<std::uint64_t, 13>   targets{0x3f800000, 0x3f800000, 0, 0, 0, 0, 0, 0, image + NativeAnimationAssociationProfile::VtableRva,
                                                        0xffff,     0xffff,     0, 0};
            for (std::size_t index = 0; index != offsets.size(); ++index)
            {
                auto& store = r.defaults[index];
                store.address = d.receiver + offsets[index];
                store.width = widths[index];
                store.target = targets[index];
                std::memcpy(&store.before, before.data() + offsets[index], widths[index]);
                store.attempted = true;
                ++r.defaultsAttempted;
                r.hadEffects = true;
                d.knownConstructed = false;
                s.state.store(State::ConstructEntered, std::memory_order_release);
                bool accepted = false;
                result = invocations.defaults(store.address, store.width, store.before, store.target, store.observed, accepted, r.exceptionCode);
                store.returned = result == ClockResult::Ok;
                store.accepted = accepted;
                if (accepted)
                    ++r.defaultsAccepted;
                if (result != ClockResult::Ok)
                    return finish(result);
                if (!accepted || store.observed != store.before)
                    return finish(ClockResult::BindingChanged);
                std::uint64_t after = 0;
                if (!M::Copy(store.address, &after, store.width))
                    return finish(ClockResult::ReadFailure);
                if (after != store.target)
                    return finish(ClockResult::BindingChanged);
                store.verified = true;
                ++r.defaultsVerified;
                if (index == 2)
                {
                    pending.nodeClearDefaultAccepted = true;
                    if (d.nodeFreeReturned && d.nodeFreeVerified)
                    {
                        d.nodeClearEntered = d.nodePointerCleared = d.nodeClearVerified = true;
                    }
                }
                if ((result = origin.Recheck()) != ClockResult::Ok || (result = Qualify(image, s, generation, owner, currentHeap)) != ClockResult::Ok)
                    return finish(result);
                if (!NativeModelReferenceHeap::Same(heap, currentHeap))
                    return finish(ClockResult::BindingChanged);
            }
        }
        if ((result = Plan::Access::RecheckInHeldScope(inputOrigin, pending.plan)) != ClockResult::Ok ||
            (result = Qualify(image, s, generation, owner, currentHeap)) != ClockResult::Ok)
            return refuse(result);
        if (!NativeModelReferenceHeap::Same(heap, currentHeap))
            return refuse(ClockResult::BindingChanged);
        // Custom's real ordered stores precede its original Init. Consume the
        // temporary borrow's shared original claim only at this last boundary.
        if ((result = origin.Claim()) != ClockResult::Ok)
            return refuse(result);
        d.constructorEntered = true;
        d.knownConstructed = false;
        s.state.store(State::ConstructEntered, std::memory_order_release);
        r.entryRva = operation == Operation::Construct ? 0xfeb910 : 0xfeba90;
        r.originalEntered = r.hadEffects = true;
        origin.Entered();
        std::uintptr_t nativeReturned = 0;
        if (operation == Operation::Construct)
            result = invocations.construct(reinterpret_cast<NativeAnimationAssociationCalls::ClumpConstructorFunction>(image + r.entryRva), d.receiver,
                                           origin.Input().clump, origin.Hierarchy().locator, nativeReturned, r.exceptionCode);
        else
            result = invocations.initialize(reinterpret_cast<NativeAnimationAssociationCalls::ClumpInitializeFunction>(image + r.entryRva), d.receiver,
                                            origin.Input().clump, origin.Hierarchy().locator, r.exceptionCode);
        r.originalReturned = result == ClockResult::Ok;
        s.state.store(State::FaultDebt, std::memory_order_release);
        // Observe even on an entered failure, before a fallible owner fence.
        // These facts stay in the pending journal; uncertainty never adopts
        // the returned node/header or makes it eligible for guessed cleanup.
        const auto observed = Verify::Observe(image, d.receiver, operation, pending.before, pending.plan, pending.observation);
        if (observed == ClockResult::Ok)
        {
            r.observed = true;
            r.nodes = pending.observation.nodes;
            r.nodeRaw = pending.observation.nodeRaw;
            r.nodeCount = pending.observation.count;
            r.nodeRequestBytes = pending.observation.requestBytes;
        }
        if (result != ClockResult::Ok)
            return finish(result);
        if (operation == Operation::Construct && nativeReturned != d.receiver)
            return finish(ClockResult::BindingChanged);
        if (observed != ClockResult::Ok)
            return finish(observed);
        if ((result = Plan::Access::RecheckInHeldScope(inputOrigin, pending.plan)) != ClockResult::Ok ||
            (result = Qualify(image, s, generation, owner, currentHeap)) != ClockResult::Ok)
            return finish(result);
        if (!NativeModelReferenceHeap::Same(heap, currentHeap))
            return finish(ClockResult::BindingChanged);
        if (!origin.IsPrivateHierarchy())
        {
            Bank bank;
            if ((result = CaptureBank(image, owner, origin.Hierarchy().locator, bank)) != ClockResult::Ok)
                return finish(result);
            if (!SameBank(pending.targetBank, bank) || bank.refs != pending.targetBank.refs)
                return finish(ClockResult::BindingChanged);
        }
        // Only the genuine returned call plus all input/heap/bookends grants
        // this issued receiver its new accepted metadata and native nodes.
        d.nodes = pending.observation.nodes;
        d.nodeRaw = pending.observation.nodeRaw;
        d.request = pending.observation.requestBytes;
        d.source.source = {};
        d.source.reference = {};
        d.source.count = pending.observation.count;
        d.source.map = std::move(pending.observation.sequenceMap);
        d.source.sequenceCapacity = pending.plan.sequenceCapacity;
        d.source.bank = pending.targetBank;
        d.source.bank.hierarchy = origin.Hierarchy().locator;
        d.suppressed = std::move(pending.suppressed);
        d.cache = std::move(pending.cache);
        d.frameJournal.clear();
        d.pendingProgress.reset();
        d.progressUnknown = false;
        d.nodeFreeEntered = d.nodeFreeReturned = d.nodeFreeVerified = false;
        d.nodeClearEntered = d.nodePointerCleared = d.nodeClearVerified = false;
        d.nodeFreeEntryRva = 0;
        d.clumpRetiredAck = false;
        state.hierarchy = origin.Hierarchy();
        state.privateHierarchy = origin.IsPrivateHierarchy();
        state.copyReference = copyReference;
        state.sequences = pending.plan.sequences;
        state.sequenceCount = pending.plan.hierarchy.sequenceCount;
        state.acceptedSource = pending.acceptedSource = true;
        d.knownConstructed = true;
        if (state.hold && state.hold.get() != targetHold)
            pending.oldHold = std::move(state.hold);
        if (pending.newHold)
            state.hold = std::move(pending.newHold);
        if (pending.oldHold)
        {
            const Release proof(*pending.oldHold, Release::Reason::HierarchyReplaced, r.command, state.hierarchy.locator,
                                state.privateHierarchy ? state.hold.get() : nullptr);
            result = pending.oldHold->controller->ReleaseForAssociation(image, *pending.oldHold, proof, owner, nullptr);
            r.oldDependencyReleased = pending.oldHold->CanDiscard();
            if (r.oldDependencyReleased)
                pending.oldHold.reset();
            if (result != ClockResult::Ok)
                return finish(result);
        }
        if ((result = origin.Recheck()) != ClockResult::Ok)
            return finish(result);
        r.verified = true;
        r.enteredUnknown = r.partialAccepted = false;
        state.pending.reset();
        s.state.store(State::LiveUnpublished, std::memory_order_release);
        output = d.receiver;
        return finish(ClockResult::Ok);
    }
    catch (...)
    {
        // The command/holds were installed before effects. Retain their exact
        // entered state instead of discarding an allocation on CPU OOM.
        if (s.data && s.data->clump && s.data->clump->pending && !r.hadEffects)
        {
            auto& pending = *s.data->clump->pending;
            if (pending.newHold && !pending.newHold->CanDiscard())
            {
                const Release proof(*pending.newHold, Release::Reason::UnenteredNewHoldCancellation, r.command);
                const auto    released = pending.newHold->controller
                                             ? pending.newHold->controller->ReleaseForAssociation(image, *pending.newHold, proof, owner, nullptr)
                                             : ClockResult::OwnerConflict;
                if (released == ClockResult::Ok || pending.newHold->CanDiscard())
                    pending.newHold.reset();
                else
                    s.state.store(State::FaultDebt, std::memory_order_release);
            }
            if (!pending.newHold)
                s.data->clump->pending.reset();
        }
        return finish(ClockResult::WriteFailure);
    }
}
