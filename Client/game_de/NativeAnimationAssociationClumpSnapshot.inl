/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual held private hierarchy authority for receiver snapshots
 ****************************************************************************/
// Included inside NativeAnimationAssociationStorageAccess. Public operations
// already own the shared native scope; these helpers never acquire it again.
static ClockResult ClumpReadHierarchy(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, NativeAnimationHierarchyValues& output,
                                      std::uintptr_t& sequences) noexcept
{
    namespace A = AnimationAssociationStorageDetail;
    if (!s.data || !s.data->clump || !s.data->clump->acceptedSource || !s.data->clump->privateHierarchy)
        return ClockResult::NotInitialized;
    const auto& d = *s.data;
    const auto& source = *d.clump;
    if (!generation || d.generation != generation || source.receiver.ownerGeneration != generation || !source.receiver.serial ||
        source.hierarchy.ownerGeneration != generation || !source.hierarchy.locator || !source.hold)
        return ClockResult::OwnerConflict;
    const auto& hold = *source.hold;
    if (!hold.IsHeld() || hold.target != &s || hold.image != image || !hold.controller || hold.receiver.ownerGeneration != source.receiver.ownerGeneration ||
        hold.receiver.serial != source.receiver.serial || hold.hierarchy.ownerGeneration != source.hierarchy.ownerGeneration ||
        hold.hierarchy.locator != source.hierarchy.locator)
        return Pin(s, ClockResult::BindingChanged);
    NativeAnimationHierarchyValues values;
    std::uintptr_t                 currentSequences = 0;
    auto                           result = hold.controller->ReadForAssociation(image, hold, owner, values, currentSequences);
    if (result != ClockResult::Ok)
        return result;
    // The actual held issue constrains pointer/count, not mutable safe scalar
    // fields such as hash, duration, compression flag or block metadata.
    if (values.interfaceAddress != source.hierarchy.locator || currentSequences != source.sequences || values.sequenceCount != source.sequenceCount)
        return Pin(s, ClockResult::BindingChanged);
    if (currentSequences)
    {
        std::uint64_t capacity = 0;
        if (currentSequences < 8 || currentSequences % 8 || !A::M::Copy(currentSequences - 8, &capacity, sizeof(capacity)))
            return ClockResult::ReadFailure;
        if (capacity < values.sequenceCount || capacity > ((std::numeric_limits<std::size_t>::max)() - 8) / 80 ||
            !A::M::PrivateWritable(currentSequences - 8, 8 + static_cast<std::size_t>(capacity) * 80) ||
            !A::M::Disjoint(currentSequences - 8, 8 + static_cast<std::size_t>(capacity) * 80, d.receiver, 104))
            return Pin(s, ClockResult::BindingChanged);
    }
    else if (values.sequenceCount)
        return Pin(s, ClockResult::BindingChanged);
    output = values;
    sequences = currentSequences;
    return ClockResult::Ok;
}
static ClockResult ClumpSnapshot(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, AnimationAssociationStorageDetail::Bytes& bytes,
                                 NativeModelReferenceHeap::Value& heap, const AnimationAssociationProgressDetail::Plan* transition = nullptr) noexcept
{
    using namespace AnimationAssociationStorageDetail;
    const auto state = s.GetState();
    if (state == State::PublicationPinned)
        return ClockResult::OwnerConflict;
    if (!s.data || !s.data->knownConstructed || s.data->destructorEntered ||
        (state != State::LiveUnpublished && state != State::FaultDebt && state != State::NodesReleased))
        return ClockResult::NotInitialized;
    const auto& d = *s.data;
    if (!d.clump || !d.clump->acceptedSource || !d.clump->privateHierarchy)
        return ClockResult::NotInitialized;
    if ((d.nodeFreeEntered && !d.nodeFreeReturned) || (d.nodeClearEntered && !d.nodePointerCleared))
        return ClockResult::OwnerConflict;
    auto result = Qualify(image, s, generation, owner, heap);
    if (result != ClockResult::Ok)
        return result;
    NativeAnimationHierarchyValues hierarchy;
    std::uintptr_t                 sequences = 0;
    if ((result = ClumpReadHierarchy(image, s, generation, owner, hierarchy, sequences)) != ClockResult::Ok)
        return result;
    if (!M::Copy(d.receiver, bytes.data(), bytes.size()))
        return ClockResult::ReadFailure;
    if (Field<std::uintptr_t>(bytes, 0) != image + NativeAnimationAssociationProfile::VtableRva || Field<std::uintptr_t>(bytes, 8) ||
        Field<std::uintptr_t>(bytes, 0x10) || Field<std::uint32_t>(bytes, 0x48) ||
        Field<std::uintptr_t>(bytes, 0x20) != (d.nodePointerCleared ? std::uintptr_t{0} : d.nodes) ||
        Field<std::uintptr_t>(bytes, 0x28) != d.clump->hierarchy.locator || Field<std::uint16_t>(bytes, 0x18) != d.source.count ||
        (Field<std::uint16_t>(bytes, 0x46) & 0x4000))
        return Pin(s, ClockResult::BindingChanged);
    if (d.nodeFreeReturned)
    {
        if (transition)
            return ClockResult::NotInitialized;
        // A causal FreeNodes ACK leaves the source pointer/count stale. Keep
        // the real H hold and frame journals, but never inspect freed header
        // or node storage merely because those addresses remain readable.
        for (const auto& frame : d.frameJournal)
            if (!AnimationAssociationProgressDetail::CopyJournal(frame))
                return Pin(s, ClockResult::BindingChanged);
    }
    else
    {
        if (!d.nodeRaw || d.nodeRaw > (std::numeric_limits<std::uintptr_t>::max)() - 64 || ((d.nodeRaw + 64) & ~std::uintptr_t{63}) != d.nodes ||
            !M::PrivateWritable(d.nodeRaw, d.request) || d.nodes < d.nodeRaw + 8 || d.nodes % 64 || d.nodes - d.nodeRaw > d.request ||
            static_cast<std::size_t>(d.source.count) * NativeAnimationAssociationProfile::NodeBytes > d.request - (d.nodes - d.nodeRaw) ||
            !M::Disjoint(d.nodeRaw, d.request, d.receiver, bytes.size()))
            return Pin(s, ClockResult::BindingChanged);
        std::uintptr_t original = 0;
        if (!M::Copy(d.nodes - 8, &original, sizeof(original)))
            return ClockResult::ReadFailure;
        if (original != d.nodeRaw || d.source.map.size() != d.source.count || d.suppressed.size() != d.source.count || d.cache.size() != d.source.count ||
            (d.progressUnknown && !transition) || (transition && transition->afterNodes.size() != d.source.count))
            return Pin(s, ClockResult::BindingChanged);
        for (std::size_t index = 0; index != d.source.count; ++index)
        {
            std::array<unsigned char, 40> node{};
            if (!M::Copy(d.nodes + index * 40, node.data(), node.size()))
                return ClockResult::ReadFailure;
            const auto sequence = d.suppressed[index] ? std::uintptr_t{0} : d.source.map[index];
            if (sequence)
            {
                if (!sequences || sequence < sequences || (sequence - sequences) % 80 || (sequence - sequences) / 80 >= hierarchy.sequenceCount)
                    return Pin(s, ClockResult::BindingChanged);
                std::array<unsigned char, 80> row{};
                if (!M::Copy(sequence, row.data(), row.size()))
                    return ClockResult::ReadFailure;
                if (Field<std::int16_t>(row, 0x44) <= 0)
                    return Pin(s, ClockResult::BindingChanged);
            }
            if (Field<std::uintptr_t>(node, 0x10) != sequence || Field<std::uintptr_t>(node, 0x18) != d.receiver ||
                Field<std::uint32_t>(node, 0x20) != 0xffffffff)
                return Pin(s, ClockResult::BindingChanged);
            if (transition)
            {
                if (node != transition->afterNodes[index])
                    return Pin(s, ClockResult::BindingChanged);
            }
            else
                for (std::size_t word = 0; word != 4; ++word)
                    if ((d.cache[index].defined & (1u << word)) && Field<std::uint32_t>(node, word * 4) != d.cache[index].words[word])
                        return Pin(s, ClockResult::BindingChanged);
        }
        if (transition)
        {
            if (!AnimationAssociationProgressDetail::Verify(*transition, true))
                return Pin(s, ClockResult::BindingChanged);
        }
        else
            for (const auto& frame : d.frameJournal)
                if (!AnimationAssociationProgressDetail::CopyJournal(frame))
                    return Pin(s, ClockResult::BindingChanged);
    }
    NativeAnimationHierarchyValues confirmed;
    std::uintptr_t                 currentSequences = 0;
    if ((result = ClumpReadHierarchy(image, s, generation, owner, confirmed, currentSequences)) != ClockResult::Ok)
        return result;
    if (currentSequences != sequences || confirmed.interfaceAddress != hierarchy.interfaceAddress || confirmed.sequenceCount != hierarchy.sequenceCount)
        return Pin(s, ClockResult::BindingChanged);
    NativeModelReferenceHeap::Value after;
    if ((result = Qualify(image, s, generation, owner, after)) != ClockResult::Ok)
        return result;
    return NativeModelReferenceHeap::Same(heap, after) ? ClockResult::Ok : ClockResult::BindingChanged;
}
static ClockResult ClumpReadLength(std::uintptr_t image, const S& s, std::uint64_t generation, C owner, std::uint32_t& output) noexcept
{
    namespace A = AnimationAssociationStorageDetail;
    A::Bytes                        before{}, middle{}, after{};
    NativeModelReferenceHeap::Value heap, middleHeap, afterHeap;
    auto                            result = ClumpSnapshot(image, s, generation, owner, before, heap);
    if (result != ClockResult::Ok)
        return result;
    NativeAnimationHierarchyValues first, second;
    std::uintptr_t                 sequences = 0, currentSequences = 0;
    if ((result = ClumpReadHierarchy(image, s, generation, owner, first, sequences)) != ClockResult::Ok ||
        (result = ClumpSnapshot(image, s, generation, owner, middle, middleHeap)) != ClockResult::Ok)
        return result;
    if (before != middle || !NativeModelReferenceHeap::Same(heap, middleHeap))
        return ClockResult::BindingChanged;
    if ((result = ClumpReadHierarchy(image, s, generation, owner, second, currentSequences)) != ClockResult::Ok ||
        (result = ClumpSnapshot(image, s, generation, owner, after, afterHeap)) != ClockResult::Ok)
        return result;
    if (before != after || first.totalTimeBits != second.totalTimeBits || sequences != currentSequences || !NativeModelReferenceHeap::Same(heap, afterHeap))
        return ClockResult::BindingChanged;
    if ((result = A::Context::Owner(image, owner)) != ClockResult::Ok)
        return result;
    // Duration remains the original current H field bits, including zero or
    // NaN payloads; a guard failure never fabricates a successful numeric value.
    output = second.totalTimeBits;
    return ClockResult::Ok;
}
static ClockResult ClumpResolveHierarchy(std::uintptr_t image, const S& s, std::uint64_t generation, C owner,
                                         NativeAnimationHierarchyReferenceToken& output) noexcept
{
    namespace A = AnimationAssociationStorageDetail;
    A::Bytes                        first{}, second{};
    NativeModelReferenceHeap::Value heap, after;
    auto                            result = ClumpSnapshot(image, s, generation, owner, first, heap);
    if (result != ClockResult::Ok)
        return result;
    const auto token = s.data->clump->hierarchy;
    if ((result = ClumpSnapshot(image, s, generation, owner, second, after)) != ClockResult::Ok)
        return result;
    if (first != second || !NativeModelReferenceHeap::Same(heap, after) || s.data->clump->hierarchy.ownerGeneration != token.ownerGeneration ||
        s.data->clump->hierarchy.locator != token.locator)
        return ClockResult::BindingChanged;
    if ((result = A::Context::Owner(image, owner)) != ClockResult::Ok)
        return result;
    output = token;
    return ClockResult::Ok;
}
